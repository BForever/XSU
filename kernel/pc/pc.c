#include "pc.h"

#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>

int count;
unsigned int asidmap[8];

struct list_head shed_list;
struct list_head ready_list[PROC_LEVELS];
struct list_head wait_list;
struct list_head sleep_list;
task_struct* current;

// Function prototypes.
static void __kill(task_struct* task);
static void pc_killallsons(task_struct* father);
static int __fork_kthread(task_struct* src);
static void pc_releasewaiting(task_struct* task);
static void pc_deletelist(task_struct* task);
static void pc_deletetask(task_struct* task);

static void pc_killallsons(task_struct* father)
{
    struct list_head *pos, *n;
    task_struct* son;
    list_for_each_safe(pos, n, &father->sons)
    {
        son = list_entry(pos, task_struct, son);
        __kill(son);
    }
}
static void pc_releasewaiting(task_struct* task)
{
    struct list_head *pos, *n;
    task_struct* waiter;
    list_for_each_safe(pos, n, &task->be_waited_list)
    {
        waiter = list_entry(pos, task_struct, wait);
        __kill(waiter);
    }
}
static void __kill(task_struct* task)
{
    pc_releasewaiting(task);
    pc_killallsons(task);
    pc_deletelist(task);
    pc_deletetask(task);
}

int sw(int bit)
{
    if (((*GPIO_SWITCH) >> bit) & 1) {
        return 1;
    }
    return 0;
}

void pc_create(void (*func)(), char* name)
{
    task_struct* task = create_kthread(name, PROC_LEVELS / 2, 1);
    task->context.epc = (unsigned int)func;
    list_add_tail(&task->ready, &ready_list[task->level]);
    task->state = PROC_STATE_READY;
}
void pc_create_son(void (*func)(), char* name)
{
    task_struct* task = create_kthread(name, PROC_LEVELS / 2, 1);
    task->context.epc = (unsigned int)func;
    list_add_tail(&task->ready, &ready_list[task->level]);
    task->state = PROC_STATE_READY;
}

static void pc_deletelist(task_struct* task)
{
    disable_interrupts();
    list_del_init(&task->shed);
    switch (task->state) {
    case PROC_STATE_RUNNING:
        break;
    case PROC_STATE_READY:
        list_del_init(&task->ready);
        break;
    case PROC_STATE_WAITING:
        list_del_init(&task->wait);
        break;
    case PROC_STATE_SLEEPING:
        list_del_init(&task->sleep);
        break;
    default:
        break;
    }
    enable_interrupts();
}
//delete task struct and all it's content space
static void pc_deletetask(task_struct* task)
{
    clearasid(task->ASID);
    if (!task->kernelflag) {
        //free user address space and heap
        unmap_all(task);
        //free pagecontent
        kfree(task->pagecontent);
    }
    kfree(task); //delete union
}
int pc_kill(int asid)
{
    asid &= 0xFF;
    task_struct* task;
    int killsyscall = SYSCALL_KILL;
    if (asid != 0 && pc_find(asid)) {
        asm volatile(
            "add $v0,$0,%0\n\t"
            "add $a0,$0,%1\n\t"
            "syscall"
            :
            : "r"(killsyscall), "r"(asid));
        return 0;
    } else if (asid == 0)
        return 1;
    else
        return 2;
}
task_struct* pc_find(int asid)
{
    struct list_head* pos;
    list_for_each(pos, &shed_list)
    {
        if (list_entry(pos, task_struct, shed)->ASID == asid) {
            return list_entry(pos, task_struct, shed);
        }
    }
    return (task_struct*)0;
}

int print_proc()
{
    struct list_head* pos;
    kernel_puts("PID name\n", 0xfff, 0);
    list_for_each(pos, &shed_list)
    {
        kernel_printf(" %x  %s\n", list_entry(pos, task_struct, shed)->ASID, list_entry(pos, task_struct, shed)->name);
    }
    return 0;
}

int getemptyasid()
{
    unsigned int i, j;
    for (i = 0; i < 8; i++) {
        if (asidmap[i] == 0xFFFFFFFF)
            continue;
        else {
            for (j = 0; j < 32; j++) {
                if (!((asidmap[i] >> j) & 1)) {
                    setasidmap(i * 32 + j);
                    return i * 32 + j;
                }
            }
        }
    }
    return -1;
}
unsigned int getasid()
{
    unsigned int asid;
    asm volatile("mfc0 %0,$10"
                 : "=r"(asid));
    return asid & 0xFF;
}
void setasid(unsigned int asid)
{
    unsigned int tmp;
    asm volatile("mfc0 %0,$10"
                 : "=r"(tmp));
    tmp &= ~0xFF;
    tmp |= (asid & 0xFF);
    asm volatile("mtc0 %0,$10" ::"r"(tmp));
}
void clearasid(unsigned int asid)
{
    asidmap[asid / 32] &= ~(1 << (asid % 32));
}
int setasidmap(unsigned int asid)
{
    asidmap[asid / 32] |= 1 << (asid % 32);
}
void clearasidmap()
{
    int i;
    for (i = 0; i < 8; i++) {
        asidmap[i] = 0;
    }
}
void init_pc()
{
    int i;
    count = 0;
    clearasidmap();
    INIT_LIST_HEAD(&shed_list);
    for (i = 0; i < PROC_LEVELS; i++) {
        INIT_LIST_HEAD(&ready_list[i]);
    }
    INIT_LIST_HEAD(&wait_list);
    INIT_LIST_HEAD(&sleep_list);
    TLB_init();

    current = 0;
    current = create_kthread("init", 0, 0);
    current->state = PROC_STATE_RUNNING;

    //register_syscall(10, pc_kill_syscall);
    register_interrupt_handler(7, pc_schedule);
    //register three TLB exception code
    pc_init_syscall();
    register_exception_handler(1, TLBMod_exc);
    register_exception_handler(2, TLBL_exc);
    register_exception_handler(3, TLBS_exc);
    //set timer and reset counter: cp0$11:timer value cp0$9:counter
    unsigned int shedins = ONESHEDINS;
    //printalltask();
    asm volatile(
        "move $v0, %0\n\t"
        "mtc0 $v0, $11\n\t"
        "mtc0 $zero, $9"
        :
        : "r"(shedins));
}

task_struct* create_kthread(char* name, int level, int asfather)
{
    //get space
    task_union* utask = kmalloc(sizeof(task_union));
    if (!utask)
        return (task_struct*)0;
    task_struct* task = &utask->task;
    //set context
    task->context.sp = (unsigned int)utask + 4096; //sp
    asm volatile("la %0, _gp"
                 : "=r"(task->context.gp)); //gp

    //init lists
    INIT_LIST_HEAD(&task->be_waited_list);
    INIT_LIST_HEAD(&task->sons);
    if (current && asfather) {
        list_add_tail(&task->son, &current->sons);
    }

    //set info
    task->ASID = getemptyasid(); //asid
    if (task->ASID == (unsigned int)-1) {
        kfree(utask);
        return (task_struct*)0;
    }
    task->kernelflag = 1; //whether kernal thread
    kernel_strcpy(task->name, name); //name
    //task->start_time = get_time();//start time

    //set schedule info
    task->counter = PROC_DEFAULT_TIMESLOTS; //time chips
    list_add_tail(&task->shed, &shed_list); //add to shed list
    task->state = PROC_STATE_CREATING; //set state
    task->level = level;

    return task;
}

void pc_init_syscall()
{
    register_syscall(SYSCALL_MALLOC, syscall_malloc);
    register_syscall(SYSCALL_FREE, syscall_free);
    register_syscall(SYSCALL_EXIT, syscall_exit);
    register_syscall(SYSCALL_KILL, pc_kill_syscall);
    register_syscall(SYSCALL_PRINTTASKS, syscall_printtasks);
    register_syscall(SYSCALL_SCHEDULE, __syscall_schedule);
    register_syscall(SYSCALL_FORK, syscall_fork);
    register_syscall(SYSCALL_SLEEP, syscall_sleep);
}

void syscall_wait(unsigned int status, unsigned int cause, context* pt_context)
{
    int asid = pt_context->a0;
    task_struct* task = pc_find(asid);
    if (task) {
        list_add_tail(&current->wait, &task->be_waited_list);
        current->state = PROC_STATE_WAITING;
        __pc_schedule(status, cause, pt_context);
    }
}

void __syscall_schedule(unsigned int status, unsigned int cause, context* pt_context)
{
    __pc_schedule(status, cause, pt_context);
}
void syscall_fork(unsigned int status, unsigned int cause, context* pt_context)
{
    copy_context(pt_context, &current->context);
    if (current->kernelflag) {
        unsigned int newid = __fork_kthread(current);
        if (newid != -1) {
            pt_context->a0 = newid;
        } else {
            pt_context->a0 = -1;
        }
    } else {
        kernel_printf("user fork not implemented!");
        syscall_exit(status, cause, pt_context);
    }
}
void syscall_printtasks(unsigned int status, unsigned int cause, context* pt_context)
{
    printalltask();
}
void syscall_malloc(unsigned int status, unsigned int cause, context* pt_context)
{
    //a0: return address
    //a1: size
    unsigned int addr = (unsigned int)kmalloc(pt_context->a0);
    add_vma(current->vma_heap_tail, addr, pt_context->a0);
    current->vma_heap_tail = current->vma_heap_tail->next;
    pt_context->a0 = list_entry(current->vma_heap_tail, vma_node, vma)->va_start;
}
void syscall_free(unsigned int status, unsigned int cause, context* pt_context)
{
    //a0: user space address
    vma_node* vma = findvma(pt_context->a0);
    if (&vma->vma == current->vma.next) //code segment
    {
        kernel_printf("Process %d try to free code space!!!\n", current->ASID);
        while (1)
            ; //try to free code space
    }
    if (vma) {
        kfree((void*)vma->pa);
    }
    do_unmapping(vma, current->pagecontent);
    list_del(&vma->vma);
    kfree(vma);
}
void syscall_exit(unsigned int status, unsigned int cause, context* pt_context)
{
    kernel_printf("process %s exited with return value:%d.\n", current->name, pt_context->a0);
    //a0: return value
    int tmp = pt_context->a0;
    pt_context->a0 = current->ASID;
    pc_kill_syscall(status, cause, pt_context);
    pt_context->a0 = tmp;
}
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context)
{
    //a0:asid of task to be killed
    task_struct* task = pc_find(pt_context->a0);
    if (task) {
        __kill(task);
        if (task == current) {
            __pc_schedule(status, cause, pt_context);
        }
    }
}

void flushsleeplist()
{
    task_struct* task;
    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &sleep_list)
    {
        task = list_entry(pos, task_struct, sleep);
        task->sleeptime -= ONESHEDTIME;
        if (task->sleeptime <= 0) {
            list_del_init(&task->sleep);
            task->state = PROC_STATE_READY;
            list_add(&task->ready, &ready_list[task->level]);
        }
    }
}
void syscall_sleep(unsigned int status, unsigned int cause, context* pt_context)
{
    //a0:sleep time unit:ms
    current->sleeptime = pt_context->a0;
    list_add_tail(&current->sleep, &sleep_list);
    current->state = PROC_STATE_SLEEPING;
    __pc_schedule(status, cause, pt_context);
}

void __request_schedule()
{
    int code = SYSCALL_SCHEDULE;
    asm volatile(
        "move $v0,%0\n\t"
        "syscall\n\t"
        "nop" ::"r"(code));
}

//USED FOR TESTING
#define WAITTIME 20000
void fk1()
{
    int i, j;
    unsigned int asid = current->ASID;
    kernel_printf("kernal thread 1\n");

    TLBEntry* TLB0;
    TLB0 = kmalloc(sizeof(TLBEntry));
    unsigned int phy = (unsigned int)kmalloc(4096);
    kernel_printf("phy = %x\n", phy);
    TLB0->entryhi = (0) | asid;
    TLB0->pagemask = 0;
    TLB0->entrylo0 = ((phy & 0x7FFFF000) >> 6) | (0x3 << 3) | (0x1 << 2) | (0x1 << 1); //using cache, D = 1, valid
    TLB0->entrylo1 = 0;

    insertTLB(TLB0, asid);
    printTLB();
    // for(i=0;i<3;i++)
    // {
    //     kernel_printf("physical output: %d\n",*((unsigned int*)(phy+i*4)));
    //     kernel_printf("write phy addr of va %d...\n",i*4);
    //     *((unsigned int*)(phy+i*4)) = i;
    //     kernel_printf("physical output: %d\n",*((unsigned int*)(phy+i*4)));
    // }
    kernel_printf("--------------\n");
    for (i = 0; i < 3; i++) {
        kernel_printf("virtual read: %d\n", *((unsigned int*)(i * 4)));
        kernel_printf("virtual write address %d...\n", i * 4);
        *((unsigned int*)(i * 4)) = i;
        kernel_printf("virtual output: %d\n", *((unsigned int*)(i * 4)));
    }
    kernel_printf("test done, exit...\n");
    kfree(TLB0);
    kfree((void*)phy);
    TLB_init();
}
void test_sleep1sandprint()
{
    kernel_printf("test_sleep 1s and print 2 started.\n");
    while (1) {
        asm volatile(
            "addi $v0,$0,9\n\t"
            "addi $a0,$0,1000\n\t"
            "syscall\n\t"
            "nop\n\t"
            "addi $v0,$0,6\n\t"
            "syscall\n\t"
            "nop");
    }
}
void test_sleep5sandkillasid2()
{
    kernel_printf("test_sleep5s and kill asid 2 started.\n");
    call_syscall_a0(SYSCALL_PRINTTASKS, 0);
    call_syscall_a0(SYSCALL_SLEEP, 5000);
    kernel_printf("awaked, try exit.\n");
    call_syscall_a0(SYSCALL_EXIT, 0);
}
void test_forkandkill()
{
    kernel_printf("test_fork and kill started.\n");
    int id;
    id = call_syscall_a0(SYSCALL_FORK, 0);
    if (id) {
        kernel_printf("fork new task id:%d\n", id);
        call_syscall_a0(SYSCALL_EXIT, 0);
    } else {
        kernel_printf("be forked,get id = %d\n", id);
        call_syscall_a0(SYSCALL_EXIT, 0);
    }
}
void fu1()
{
    asm volatile(
        "addi $v0,$0,6\n\t"
        "syscall\n\t"
        "nop\n\t"
        "add $a0,$0,$0\n\t"
        "addi $v0,$0,3\n\t"
        "syscall\n\t"
        "nop");
}
//for test purposes
int pc_test()
{
    // int i;
    // unsigned *u1;
    // u1 = kmalloc(8192);
    // kernel_memcpy(u1,((unsigned int)fu1)+12,8192);

    // for(i = 0;i<7;i++)
    // {
    //     kernel_printf("%x ",*(unsigned*)(u1+i));
    // }

    // if(sw(0))
    //     fk1();
    // else
    //     create_process("u1",(unsigned int)u1,8192,PROC_LEVELS/2);
    //pc_create(test_sleep1sandprint,"test_sleep1sandprint");
    //pc_create(test_sleep5sandkillasid2,"test_sleep5sandkillasid2");
    call_syscall_a0(SYSCALL_PRINTTASKS, 0);
    pc_create(test_forkandkill, "test_forkandkill");
    //pc_create(fk2,"k2");
    return 0;
}
//print the infomation of single task
void printtask(task_struct* task)
{
    kernel_printf("%s\t%d\t\t%d\t\t%d\n", task->name, task->ASID, task->state, task->counter);
}
//print all the tasks that are registered in system
void printalltask()
{
    int cnt = 0;
    struct list_head* pos;
    task_struct* task;
    //count the number
    list_for_each(pos, &shed_list)
    {
        cnt++;
    }
    //prompt
    kernel_printf("NAME\tASID\tSTATE\tCOUNTER\n");
    list_for_each(pos, &shed_list)
    {
        task = list_entry(pos, task_struct, shed);
        printtask(task);
    }
}
//print all tasks that are in the ready list
void printreadylist()
{
    struct list_head* pos;
    task_struct* task;
    int i;
    //ordered by previlege,from low to high
    for (i = 0; i < PROC_LEVELS; i++) {
        if (!list_empty(&ready_list[i])) {
            kernel_printf("Ready list level %d:\n", i);
            list_for_each(pos, &ready_list[i])
            {
                task = list_entry(pos, task_struct, ready);
                kernel_printf("name: %s,asid: %d,kernel: %d\n", task->name, task->ASID, task->kernelflag);
            }
        }
    }
}
//print vma list of task
void printvmalist(task_struct* task)
{
    struct list_head* pos;
    vma_node* vma;
    int i = 0;
    kernel_printf("vmainfo of process %s:\n", task->name);
    list_for_each(pos, &task->vma)
    {
        vma = list_entry(pos, vma_node, vma);
        kernel_printf("%d:%x-%x->%x-%x\n", i, vma->va_start, vma->va_end, vma->pa, vma->pa + vma->va_end - vma->va_start);
        i++;
    }
}
//given asid and physical address, get corresponding virtual address
unsigned int getuseraddr(int asid, unsigned int pa)
{
    task_struct* task;
    task = pc_find(asid);
    struct list_head* pos;
    vma_node* node;
    //search task's vma list to compare
    list_for_each(pos, &task->vma)
    {
        node = list_entry(pos, vma_node, vma);
        if (pa >= node->pa && pa - node->pa <= (node->va_end - node->va_start)) {
            return node->va_start + pa - node->pa;
        }
    }
    return 0;
}
static int __fork_kthread(task_struct* src)
{
    task_struct* new;
    new = kmalloc(sizeof(task_union));
    kernel_memcpy(new, src, sizeof(task_union));
    //assign new asid
    new->ASID = (unsigned int)getemptyasid();
    if ((int)new->ASID == -1) {
        kfree(new);
        return -1;
    }
    //relocate stack pointer
    new->context.sp = (unsigned int)src->context.sp - (unsigned int)src + (unsigned int)new;
    //return value
    new->context.a0 = 0;
    //reset state
    new->state = PROC_STATE_READY;
    list_add_tail(&new->shed, &shed_list);
    list_add_tail(&new->ready, &ready_list[new->level]);
    new->counter = PROC_DEFAULT_TIMESLOTS;

    return new->ASID;
}

int call_syscall_a0(int code, int a0)
{
    int result;
    asm volatile(
        "move $v0,%1\n\t"
        "move $a0,%2\n\t"
        "syscall\n\t"
        "move %0,$a0"
        : "=r"(result)
        : "r"(code), "r"(a0));
    return result;
}