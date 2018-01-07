#ifndef _ZJUNIX_PC_H
#define _ZJUNIX_PC_H

#include <arch.h>
#include <zjunix/list.h>
#include <zjunix/lock.h>

//register storage in task struct
typedef struct {
    unsigned int epc;
    unsigned int at;
    unsigned int v0, v1;
    unsigned int a0, a1, a2, a3;
    unsigned int t0, t1, t2, t3, t4, t5, t6, t7;
    unsigned int s0, s1, s2, s3, s4, s5, s6, s7;
    unsigned int t8, t9;
    unsigned int hi, lo;
    unsigned int gp;
    unsigned int sp;
    unsigned int fp;
    unsigned int ra;
} context;

//exception level:set when exc in kernel mode--for status register
#define EXL = 1 
//pagetable base address, also mapped by TLB
#define PAGETABLE_BASE 0xC0000000

//schedule info
#define CPUSPEED 100000   //instructions per ms
#define ONESHEDTIME 10   //ms
#define ONESHEDINS CPUSPEED*ONESHEDTIME //cycles per schedule

//default time slots
#define PROC_DEFAULT_TIMESLOTS 1
//the start address of user code segment
#define USER_CODE 0
//the top of the user stack segment
#define USER_STACK 0x80000000
//the size of the user stack
#define USER_STACK_SIZE 8192//8k User stack size

//process state
#define PROC_STATE_READY 0
#define PROC_STATE_RUNNING 2
#define PROC_STATE_WAITING 4
#define PROC_STATE_CREATING 8
#define PROC_STATE_SLEEPING 16

//the number of implemented previleges
#define PROC_LEVELS 3

//utils for asid allocation
#define ASIDMAP(asid) ((asidmap[asid/32]>>(asid%32))&1)



//lists for schedule
extern struct list_head shed_list;
extern struct list_head ready_list[PROC_LEVELS];
extern struct list_head wait_list;

//one Entry's structure in TLB
typedef struct {
    unsigned int entrylo0;//31-6:PFN 5-3:Cache coherence attr 2:Dirty 1:Valid 0:Global//2
    unsigned int entrylo1;//same as lo0//3
    unsigned int entryhi;//31-13:VPN2 7-0:ASID//10
    unsigned int pagemask;//28-13:PageMask//5
} TLBEntry;

//virtual memory area node, linked by list
typedef struct{
    unsigned int va_start;
    unsigned int pa;
    unsigned int va_end;
    struct list_head vma;
} vma_node;

//task struct, represents one task/process/thread
typedef struct {
    //register storage, including sp and gp
    context context;
    unsigned int kernel_stack;
    unsigned int user_stack;
    
    //info
    unsigned int ASID;//pid, as well as asid, for tlb to use
    char name[32];
    unsigned long start_time;
    int kernelflag;//kernal thread flag

    //memory management: for user
    TLBEntry **pagecontent;
    struct list_head vma;
    struct list_head *vma_heap_tail;

    //schedule
    unsigned int level;//0,1,2: 0 is lowest
    unsigned int state;
    struct list_head shed;
    struct list_head ready;
    struct list_head sleep;
    int sleeptime;
    struct list_head wait;
    struct list_head son;
    
    unsigned int counter;//used for cpu time chips counting
    //list as head
    struct list_head sons;
    struct list_head be_waited_list;
} task_struct;

//the current running task
extern task_struct* current;

//task struct with it's stack
typedef union {
    task_struct task;
    unsigned char kernel_stack[4096];
} task_union;

//semphore, for single CPU system
struct semaphore {
    char name[30];
    struct list_head wait_list;
    int count;
};

//init,create,kill
void init_pc();
void pc_create(void (*func)(), char* name);
task_struct* create_kthread(char* name,int level);
task_struct* create_process (char* name,unsigned int phy_code,unsigned int length,unsigned int level);
int pc_kill(int asid);
void pc_deletelist(task_struct *task);
void pc_deletetask(task_struct *task);
task_struct* pc_find(int asid);
void printalltask();
int print_proc();

int __fork_kthread(task_struct* src);

//asid management
void clearasid(unsigned int asid);
int getemptyasid();
void clearasidmap();
int setasidmap(unsigned int asid);
unsigned int getasid();
void setasid(unsigned int asid);

//schedule
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context);
void copy_context(context* src, context* dest);
task_struct* __getnexttask();
void __reset_counter();
void __pc_schedule(unsigned int status, unsigned int cause, context* pt_context);
void __request_schedule();
void flushsleeplist();

//TLB
void TLB_init();
void TLBMod_exc(unsigned int status, unsigned int cause, context* pt_context);
void TLBL_exc(unsigned int status, unsigned int cause, context* pt_context);
void TLBS_exc(unsigned int status, unsigned int cause, context* pt_context);
void TLBrefill();
TLBEntry* getTLBentry(unsigned int badvpn2,TLBEntry **pagecontent);
void printTLBEntry(TLBEntry *tlb);
void printTLB();
void insertTLB(TLBEntry *entry,int asid);
unsigned int testTLB(unsigned int va);

//user process
void add_code_vma(struct list_head *vmahead,unsigned int pa,unsigned int length);//pa must be the start of a frame
void add_stack_vma(struct list_head *vmahead,unsigned int pa,unsigned int length);
int add_vma(struct list_head *vma_prev,unsigned int pa,unsigned int length);
vma_node* findvma(unsigned int va);
int addrvalid(unsigned int badaddr);
int do_mapping(unsigned int asid, vma_node* vma,TLBEntry **pagecontent);
int do_unmapping(vma_node* vma,TLBEntry **pagecontent);
void map_all(task_struct* task);
void unmap_all(task_struct* task);//except code and stack
void free_heap(task_struct *task);

//syscall code allocation
#define SYSCALL_MALLOC 1
#define SYSCALL_FREE   2
#define SYSCALL_EXIT   3
#define SYSCALL_KILL   5
#define SYSCALL_PRINTTASKS 6
#define SYSCALL_SCHEDULE 7
#define SYSCALL_FORK 8
#define SYSCALL_SLEEP 9

//pc's syscall define
void pc_init_syscall();
//request memory, address will be in a0
void syscall_malloc(unsigned int status, unsigned int cause, context* pt_context);
//free the space started at a0
void syscall_free(unsigned int status, unsigned int cause, context* pt_context);
//exit: caller be killed
void syscall_exit(unsigned int status, unsigned int cause, context* pt_context);
//kill: kill process whose asid = a0
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context);
//print all tasks' infomation
void syscall_printtasks(unsigned int status, unsigned int cause, context* pt_context);
//process release cpu
void __syscall_schedule(unsigned int status, unsigned int cause, context* pt_context);
//fork: a0 = 0 means caller,a0>0 means new task
void syscall_fork(unsigned int status, unsigned int cause, context* pt_context);
//sleep:process will sleep a0 ms
void syscall_sleep(unsigned int status, unsigned int cause, context* pt_context);

int call_syscall_a0(int code,int a0);

//debug utils
int sw(int bit);

#endif  // !_ZJUNIX_PC_H