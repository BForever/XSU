#include "pc.h"
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>

// ASID bitmap, used for asid allocation
unsigned int asidmap[8];

// Schedule lists:
// All tasks' lists
struct list_head shed_list;
// Multilevel ready lists
struct list_head ready_list[PROC_LEVELS];
// Sleep lists
struct list_head sleep_list;
// The current running task
task_struct* current;

// Function prototypes.
// Kill all sons of father thread
static void pc_killallsons(task_struct* father);
// Fork the src thread, return new thread's asid
static int __fork_kthread(task_struct* src);
// Release all threads waiting for this task
static void pc_releasewaiting(task_struct* task);
// Delete the corresponding entry in every lists
static void pc_deletelist(task_struct* task);
// Delete the task struct and free the space
static void pc_deletetask(task_struct* task);
// Initalize all the list_head in task
static void pc_init_tasklists(task_struct* task);

// Kill all children of father thread
static void pc_killallchildren(task_struct* father)
{
    struct list_head *pos, *n;
    task_struct* child;
    list_for_each_safe(pos, n, &father->children)
    {
        child = list_entry(pos, task_struct, child);
        __kill(child);
    }
}

// Release all threads waiting for this task
static void pc_releasewaiting(task_struct* task)
{
    struct list_head *pos, *n;
    task_struct* waiter;
    list_for_each_safe(pos, n, &task->be_waited_list)
    {
        waiter = list_entry(pos, task_struct, wait);
        waiter->state = PROC_STATE_READY;
        waiter->level = PROC_LEVELS - 1;
        list_add_tail(&waiter->ready,&ready_list[waiter->level]);
    }
}

// Kill a task
void __kill(task_struct* task)
{
    // First find all tasks waiting for it's exit and release them
    pc_releasewaiting(task);
    // Kill all it's son threads
    pc_killallchildren(task);  
    // Delete the corresponding entry in every lists
    pc_deletelist(task);
    // Delete the task struct and free the space
    pc_deletetask(task);
}

// Get whether the corresponding switch is on
int sw(int bit)
{
    // Shift the bits
    if (((*GPIO_SWITCH) >> bit) & 1) {
        return 1;
    }
    return 0;
}

// Create a kernel thread using a function and name
int pc_create(void (*func)(), char* name)
{
    // Create the kernel thread task struct and set the level
    task_struct* task = create_kthread(name, PROC_LEVELS / 2, 0);
    // Locate the starting address
    task->context.epc = (unsigned int)func;
    // Add it to the ready list
    list_add_tail(&task->ready, &ready_list[task->level]);
    // Set the state
    task->state = PROC_STATE_READY;
    // Return asid for potential use
    return task->ASID;
}

// Create a kernel thread using a function and name, and take it as a child thread
int pc_create_child(void (*func)(), char* name)
{
    // Create the kernel thread task struct and set the level, take it as a child thread
    task_struct* task = create_kthread(name, PROC_LEVELS / 2, 1);
    // Locate the starting address
    task->context.epc = (unsigned int)func;
    // Add it to the ready list
    list_add_tail(&task->ready, &ready_list[task->level]);
    // Set the state
    task->state = PROC_STATE_READY;
    // Return asid for potential use
    return task->ASID;
}

// Delete task from it's linked lists
static void pc_deletelist(task_struct* task)
{
    // First disable the interrupts
    disable_interrupts();
    // Delete it from shed list
    list_del_init(&task->shed);
    // Delete it from it's corresponding list
    switch (task->state) {
    // If it's running, it is not in any lists
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

// Delete task struct and all it's content space
static void pc_deletetask(task_struct* task)
{
    // Release task's asid
    clearasid(task->ASID);
    if (!task->kernelflag) {
        // Free user address space and heap
        unmap_all(task);
        // Free pagecontent
        kfree(task->pagecontent);
    }
    kfree(task); // Delete union
}

// Kill a task by it's asid
int pc_kill(int asid)
{
    asid &= 0xFF;
    task_struct* task;
    if (asid != 0 && pc_find(asid)) {
        call_syscall_a0(SYSCALL_KILL,asid);
        return 0;
    } else if (asid == 0)
        return 1;
    else
        return 2;
}

// Find the corresponding task using asid
task_struct* pc_find(int asid)
{
    struct list_head* pos;
    // Search all tasks in the schedule lists
    list_for_each(pos, &shed_list)
    {
        // If found
        if (list_entry(pos, task_struct, shed)->ASID == asid) {
            return list_entry(pos, task_struct, shed);
        }
    }
    // Not found, return 0
    return (task_struct*)0;
}

// Print process info: old interface in powershell  of ZJUNIX
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

// Get an asdi from asid bitmap, if asid all allocated, return -1 
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

// Get current asid stored in CP0 register 10, select 0
unsigned int getasid()
{
    unsigned int asid;
    asm volatile("mfc0 %0,$10"
                 : "=r"(asid));
    return asid & 0xFF;
}

// Set current asid stored in CP0 register 10, select 0
void setasid(unsigned int asid)
{
    unsigned int tmp;
    asm volatile("mfc0 %0,$10"
                 : "=r"(tmp));
    tmp &= ~0xFF;
    tmp |= (asid & 0xFF);
    asm volatile("mtc0 %0,$10" ::"r"(tmp));
}

// Clear asid in the asid bitmap
void clearasid(unsigned int asid)
{
    asidmap[asid / 32] &= ~(1 << (asid % 32));
}

// Set asdi in the asid bitmap
int setasidmap(unsigned int asid)
{
    asidmap[asid / 32] |= 1 << (asid % 32);
}

// Clear whole asid map
void clearasidmap()
{
    int i;
    for (i = 0; i < 8; i++) {
        asidmap[i] = 0;
    }
}

// Initalize process control
void init_pc()
{
    int i;
    // Initialize whole asid map
    clearasidmap();
    // Initalize schedule list
    INIT_LIST_HEAD(&shed_list);
    // Initalize all ready list
    for (i = 0; i < PROC_LEVELS; i++) {
        INIT_LIST_HEAD(&ready_list[i]);
    }
    // Initalize sleep list
    INIT_LIST_HEAD(&sleep_list);

    // Initalize TLB, including initialize pagetable base address
    TLB_init();

    // Set initalization as thread init
    current = 0;
    current = create_kthread("init", 0, 0);
    current->state = PROC_STATE_RUNNING;

    // Register_syscall(10, pc_kill_syscall);
    register_interrupt_handler(7, pc_schedule);
    
    // Initialize syscall provided by process control
    pc_init_syscall();

    // Register three TLB exception code
    register_exception_handler(1, TLBMod_exc);
    register_exception_handler(2, TLBL_exc);
    register_exception_handler(3, TLBS_exc);

    // Set timer and reset counter: cp0$11:timer value cp0$9:counter
    unsigned int shedins = ONESHEDINS;
    
    //printalltask();
    
    // Set timer
    asm volatile(
        "move $v0, %0\n\t"
        "mtc0 $v0, $11\n\t"
        "mtc0 $zero, $9"
        :
        : "r"(shedins));
}

task_struct* create_kthread(char* name, int level, int asfather)
{
    // Get space
    task_union* utask = kmalloc(sizeof(task_union));
    if (!utask)
        return (task_struct*)0;
    task_struct* task = &utask->task;
    // Set context
    task->context.sp = (unsigned int)utask + 4096; //sp
    asm volatile("la %0, _gp"
                 : "=r"(task->context.gp)); //gp

    // Init lists
    pc_init_tasklists(task);
    if (current && asfather) {
        list_add_tail(&task->child, &current->children); 
    }

    // Set info
    task->ASID = getemptyasid(); //asid
    if (task->ASID == (unsigned int)-1) {
        kfree(utask);
        return (task_struct*)0;
    }
    // Get start time
    pc_time_get(&task->start_time);
    // Whether kernel thread
    task->kernelflag = 1; 

    // Set task name
    if (kernel_strlen(name) < sizeof(task->name))
        kernel_strcpy(task->name, name); //name
    else {
        kernel_printf("too large name!\n");
    }
    //task->start_time = get_time();//start time

    // Set schedule info
    // Set time chips
    task->counter = PROC_DEFAULT_TIMESLOTS; 
    // Add to shed list
    list_add_tail(&task->shed, &shed_list); 
    // Set state
    task->state = PROC_STATE_CREATING; 
    // Set level
    task->level = level;

    return task;
}

// Init all syscall provided by process control
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
    register_syscall(SYSCALL_WAIT, syscall_wait);
}

// wait:blocked entil task a0 ends 
void syscall_wait(unsigned int status, unsigned int cause, context* pt_context)
{
    // Get target asid in a0
    int asid = pt_context->a0;
    // Find the task
    task_struct* task = pc_find(asid);
    // If task found
    if (task) {
        // Move it to target task wait list
        list_add_tail(&current->wait, &task->be_waited_list);
        // Set state to waiting
        current->state = PROC_STATE_WAITING;
        // Run next ready task
        __pc_schedule(status, cause, pt_context);
    }
}

// Process release cpu
void __syscall_schedule(unsigned int status, unsigned int cause, context* pt_context)
{
    __pc_schedule(status, cause, pt_context);
}

// fork: a0 = 0 means caller,a0>0 means new task
void syscall_fork(unsigned int status, unsigned int cause, context* pt_context)
{
    // Store the context
    copy_context(pt_context, &current->context);
    // Only implemented kernel thread fork
    if (current->kernelflag) {
        // Fork and get new task's asid
        unsigned int newid = __fork_kthread(current);
        // Return different value for distinguish
        if (newid != -1) {
            pt_context->a0 = newid;
        } else {
            pt_context->a0 = -1;
        }
    } else {
        kernel_printf("User fork not implemented!");
        syscall_exit(status, cause, pt_context);
    }
}

// print all tasks' infomation
void syscall_printtasks(unsigned int status, unsigned int cause, context* pt_context)
{
    printalltask();
}

// Request memory, address will be in a0
void syscall_malloc(unsigned int status, unsigned int cause, context* pt_context)
{
    //a0: return address
    //a1: size
    unsigned int addr = (unsigned int)kmalloc(pt_context->a0);
    // Add vma
    add_vma(current->vma_heap_tail, addr, pt_context->a0);
    // Set new vma heap pointer
    current->vma_heap_tail = current->vma_heap_tail->next;
    // Return user space address
    pt_context->a0 = list_entry(current->vma_heap_tail, vma_node, vma)->va_start;
}

// Free the space started at a0
void syscall_free(unsigned int status, unsigned int cause, context* pt_context)
{
    //a0: user space address
    vma_node* vma = findvma(pt_context->a0);
    // If try to free code segment
    if (&vma->vma == current->vma.next) 
    {
        kernel_printf("Process %d try to free code space!!!\n", current->ASID);
        while (1)
            ; //try to free code space
    }
    // vma found
    if (vma) {
        kfree((void*)vma->pa);
    }
    // Remove tlb mapping from task's pagetable
    do_unmapping(vma, current->pagecontent);
    // Delete vma
    list_del(&vma->vma);
    // Free the vma struct
    kfree(vma);
}

// exit: caller be killed
void syscall_exit(unsigned int status, unsigned int cause, context* pt_context)
{
    // Prompt
    kernel_printf("process %s exited with return value:%d.\n", current->name, pt_context->a0);
    // a0: return value
    int tmp = pt_context->a0;
    // Use kill to finish exit
    pt_context->a0 = current->ASID;
    pc_kill_syscall(status, cause, pt_context);
    pt_context->a0 = tmp;
}

// kill: kill process whose asid = a0
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context)
{
    // a0:asid of task to be killed
    task_struct* task = pc_find(pt_context->a0);
    
    // If found
    if (task) {
        // Kill it
        __kill(task);
        // If bekilled task is running
        if (task == current) {
            // Reset current task
            current = (task_struct*)0;
            // Run next ready task
            __pc_schedule(status, cause, pt_context);
        }
    // Task not found
    } else {
        kernel_printf("Kill syscall didn't find the task to be killed.\n");
    }
}

// sleep:process will sleep a0 ms
void syscall_sleep(unsigned int status, unsigned int cause, context* pt_context)
{
    // a0:sleep time unit:ms
    // kernel_printf("%s:start sleep for %d ms\n",current->name,pt_context->a0);
    // Get current time
    pc_time_get(&current->sleeptime);
    // Calc the wake time
    pc_time_add(&current->sleeptime,pt_context->a0*CPUSPEED);
    // Add it to the sleep list
    list_add_tail(&current->sleep, &sleep_list);
    // Set state to sleep
    current->state = PROC_STATE_SLEEPING;
    // Run next ready task
    __pc_schedule(status, cause, pt_context);
}

// Process release cpu
void __request_schedule()
{
    call_syscall_a0(SYSCALL_SCHEDULE,0);
}

// USED FOR TESTING
// Manully insert TLB to test
void fk1()
{
    int i, j;
    unsigned int asid = current->ASID;
    kernel_printf("kernel thread 1\n");

    TLBEntry* TLB0;
    TLB0 = kmalloc(sizeof(TLBEntry));
    unsigned int phy = (unsigned int)kmalloc(4096);
    kernel_printf("phy = %x\n", phy);
    TLB0->entryhi = (0) | asid;
    TLB0->pagemask = 0;
    TLB0->entrylo0 = ((phy & 0x7FFFF000) >> 6) | (0x3 << 3) | (0x1 << 2) | (0x1 << 1); //using cache, D = 1, valid
    TLB0->entrylo1 = 0;

    insertTLB(TLB0);
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

// Test: every one second, print ps using sleep syscall
void test_sleep1sandprint()
{
    kernel_printf("\nTask %d:Test_sleep 1s and print tasks started.\n",current->ASID);
    while (1) {
        call_syscall_a0(SYSCALL_SLEEP, 1000);
        call_syscall_a0(SYSCALL_PRINTTASKS,0);
    }
}

// Test: sleep 5 seconds
void test_sleep5s()
{
    kernel_printf("\nTask %d:Test_sleep 5s started.\n",current->ASID);
    call_syscall_a0(SYSCALL_PRINTTASKS, 0);
    call_syscall_a0(SYSCALL_SLEEP, 5000);
    kernel_printf("Task %d:Awaked, try exit.\n",current->ASID);
    call_syscall_a0(SYSCALL_EXIT, 0);
}
void test_forkandkill()
{
    kernel_printf("\nTask %d:Test_fork and kill started.\n",current->ASID);
    int id;
    id = call_syscall_a0(SYSCALL_FORK, 0);
    if (id) {
        kernel_printf("Task %d:Fork new task id = %d\n",current->ASID, id);
        call_syscall_a0(SYSCALL_EXIT, 0);
    } else {
        kernel_printf("Task %d:Be forked,get id = %d\n",current->ASID, id);
        call_syscall_a0(SYSCALL_EXIT, 0);
    }
}
void test_forkandwait()
{
    kernel_printf("\nTask %d:Test_fork and wait started.\n",current->ASID);
    int id;
    id = call_syscall_a0(SYSCALL_FORK, 0);
    if (id) {
        kernel_printf("Task %d:Fork new task id = %d, start waiting it\n" ,current->ASID,id);
        call_syscall_a0(SYSCALL_WAIT, id);
        kernel_printf("Task %d:Wait ended, exit.\n",current->ASID);
        call_syscall_a0(SYSCALL_EXIT, 0);
    } else {
        kernel_printf("Task %d:Be forked,get id = %d, sleep 3s...\n",current->ASID, id);
        call_syscall_a0(SYSCALL_SLEEP, 3000);
        kernel_printf("Task %d:Be fork task wakes, exit.\n",current->ASID);
        call_syscall_a0(SYSCALL_EXIT,0);
    }
}

static void test_child()
{
    while(1);
}

static void test_father()
{
    int child[3];

    child[0] = pc_create_child(test_child,"child0");
    child[1] = pc_create_child(test_child,"child1");
    child[2] = pc_create_child(test_child,"child2");

    kernel_printf("Task %d:3 children created.\n",current->ASID);
    call_syscall_a0(SYSCALL_PRINTTASKS,0);
    while(1);
}

void test_fatherandchild()
{
    int father = pc_create(test_father,"father");
    kernel_printf("\nFather thread created.\n");
    return;
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
// For test purposes
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
//print the infomation of single task
void printtask(task_struct* task)
{
    kernel_printf("%s\t%d\t", task->name, task->ASID);
    // switch(task->state)
    // {
    //     case PROC_STATE_CREATING:kernel_printf("creating");break;
    //     case PROC_STATE_READY:kernel_printf("ready   ");break;
    //     case PROC_STATE_RUNNING:kernel_printf("running ");break;
    //     case PROC_STATE_SLEEPING:kernel_printf("sleeping");break;
    //     case PROC_STATE_WAITING:kernel_printf("waiting ");break;
    //     default:kernel_printf("unknown ");break;
    // }
    if(task->state == PROC_STATE_CREATING){
        kernel_printf("creating");
    }else if(task->state == PROC_STATE_READY){
        kernel_printf("ready");
    }
    else if(task->state == PROC_STATE_RUNNING){
        kernel_printf("running");
    }else if(task->state == PROC_STATE_SLEEPING){
        kernel_printf("sleeping");
    }else if(task->state == PROC_STATE_WAITING){
        kernel_printf("waiting");
    }else{
        kernel_printf("unknown");
    }
    kernel_printf("\t%d\n", task->counter);
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
// Given asid and physical address, get corresponding virtual address
unsigned int getuseraddr(int asid, unsigned int pa)
{
    task_struct* task;
    task = pc_find(asid);
    struct list_head* pos;
    vma_node* node;
    // Search task's vma list to compare
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
    new = (task_struct*)kmalloc(sizeof(task_union));
    kernel_memcpy(new, src, sizeof(task_union));
    // Assign new asid
    new->ASID = (unsigned int)getemptyasid();
    if ((int)new->ASID == -1) {
        kfree(new);
        return -1;
    }
    // Relocate stack pointer
    new->context.sp = (unsigned int)src->context.sp - (unsigned int)src + (unsigned int)new;
    // Return value
    new->context.a0 = 0;
    // Reset state
    new->state = PROC_STATE_READY;
    list_add_tail(&new->shed, &shed_list);
    list_add_tail(&new->ready, &ready_list[new->level]);
    pc_init_tasklists(new);

    new->counter = PROC_DEFAULT_TIMESLOTS;

    return new->ASID;
}
static void pc_init_tasklists(task_struct* task)
{
    // Init lists
    INIT_LIST_HEAD(&task->be_waited_list);
    INIT_LIST_HEAD(&task->child);
}
// Call a syscall with code in v0 and parameter a0
int call_syscall_a0(int code, int a0)
{
    // Restore the result in a0
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