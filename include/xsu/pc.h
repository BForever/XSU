#ifndef _XSU_PC_H
#define _XSU_PC_H

#include <arch.h>
#include <xsu/list.h>
#include <xsu/lock.h>



// Exception level:set when exc in kernel mode--for status register
#define EXL = 1 
// Pagetable base address, also mapped by TLB
#define PAGETABLE_BASE 0xC0000000

// Schedule info
// Instructions per ms
#define CPUSPEED 100000   
// Unit:ms
#define ONESHEDTIME 10
// Cycles per schedule
#define ONESHEDINS CPUSPEED*ONESHEDTIME 

// Default time slots
#define PROC_DEFAULT_TIMESLOTS 1
// The start address of user code segment
#define USER_CODE 0
// The top of the user stack segment
#define USER_STACK 0x80000000
// The size of the user stack
#define USER_STACK_SIZE 8192//8k User stack size

// Process state
#define PROC_STATE_READY 0
#define PROC_STATE_RUNNING 2
#define PROC_STATE_WAITING 4
#define PROC_STATE_CREATING 8
#define PROC_STATE_SLEEPING 16

// The number of implemented previleges
#define PROC_LEVELS 3
// The higest level in multi ready lists
#define PROC_HIGEST_LEVEL PROC_LEVELS - 1

// Utils for asid allocation
#define ASIDMAP(asid) ((asidmap[asid/32]>>(asid%32))&1)



// Lists for schedule
// All tasks' lists
extern struct list_head shed_list;
// Multilevel ready lists
extern struct list_head ready_list[PROC_LEVELS];
// Sleep lists
extern struct list_head sleep_list;

// One Entry's structure in TLB
typedef struct {
    //31-6:PFN 5-3:Cache coherence attr 2:Dirty 1:Valid 0:Global//2
    unsigned int entrylo0;
    //same as lo0//3
    unsigned int entrylo1;
    //31-13:VPN2 7-0:ASID//10
    unsigned int entryhi;
    //28-13:PageMask//5
    unsigned int pagemask;
} TLBEntry;

// Virtual memory area node, linked by list
typedef struct{
    // The start of the virtual address area 
    unsigned int va_start;
    // The corresponding physical address 
    unsigned int pa;
    // The end of the virtual address area 
    unsigned int va_end;
    // List node for linking
    struct list_head vma;
} vma_node;

// 64 bits time type
typedef struct{
    unsigned int lo;
    unsigned int hi;
} time_u64;

// Task struct, represents one task/process/thread
typedef struct {
    // Register storage, including sp and gp
    context context;
    unsigned int kernel_stack;
    unsigned int user_stack;
    
    // Info
    unsigned int ASID;//pid, as well as asid, for tlb to use
    char name[32];
    time_u64 start_time;
    int kernelflag;//kernal thread flag

    // Memory management: for user
    TLBEntry **pagecontent;
    struct list_head vma;
    struct list_head *vma_heap_tail;

    // Schedule
    unsigned int level;//0,1,2: 0 is lowest
    unsigned int state;
    struct list_head shed;
    struct list_head ready;
    struct list_head sleep;
    time_u64 sleeptime;
    struct list_head wait;
    struct list_head son;
    int counter;//used for cpu time chips counting
    
    // List as head
    struct list_head sons;
    struct list_head be_waited_list;
} task_struct;

// The current running task
extern task_struct* current;

// Task struct with it's stack
typedef union {
    task_struct task;
    unsigned char kernel_stack[4096];
} task_union;

// Semphore, for single CPU system
struct semaphore {
    char name[30];
    struct list_head wait_list;
    int count;
};

// Init,create,kill
void init_pc();
int pc_create(void (*func)(), char* name);
int pc_create_child(void (*func)(), char* name);
task_struct* create_kthread(char* name, int level ,int asfather);
task_struct* create_process (char* name,unsigned int phy_code,unsigned int length,unsigned int level);
int pc_kill(int asid);
void __kill(task_struct* task);
task_struct* pc_find(int asid);
void printtask(task_struct* task);
void printalltask();
int print_proc();
// Print all tasks that are in the ready list
void printreadylist();

// Asid management
void clearasid(unsigned int asid);
int getemptyasid();
void clearasidmap();
int setasidmap(unsigned int asid);
unsigned int getasid();
void setasid(unsigned int asid);

// Schedule
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context);
void copy_context(context* src, context* dest);
void __reset_counter();
void __pc_schedule(unsigned int status, unsigned int cause, context* pt_context);
void __request_schedule();
void flushsleeplist();

// TLB
void TLB_init();
void TLBMod_exc(unsigned int status, unsigned int cause, context* pt_context);
void TLBL_exc(unsigned int status, unsigned int cause, context* pt_context);
void TLBS_exc(unsigned int status, unsigned int cause, context* pt_context);
void TLBrefill();
TLBEntry* getTLBentry(unsigned int badvpn2,TLBEntry **pagecontent);
void printTLBEntry(TLBEntry *tlb);
void printTLB();
void insertTLB(TLBEntry *entry);
unsigned int testTLB(unsigned int va);

// User process
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
void clearpage(void *pagestart);
// Print vma list of task
void printvmalist(task_struct* task);
// Given asid and physical address, get corresponding virtual address
unsigned int getuseraddr(int asid, unsigned int pa);

// Test functions
void fk1();
void test_sleep1sandprint();
void test_sleep5s();
void test_forkandkill();
void test_forkandwait();
void test_fatherandchild();
void fu1();
int pc_test();



// Process control's syscall definition
void pc_init_syscall();
// Request memory, address will be in a0
void syscall_malloc(unsigned int status, unsigned int cause, context* pt_context);
// Free the space started at a0
void syscall_free(unsigned int status, unsigned int cause, context* pt_context);
// exit: caller be killed
void syscall_exit(unsigned int status, unsigned int cause, context* pt_context);
// kill: kill process whose asid = a0
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context);
// print all tasks' infomation
void syscall_printtasks(unsigned int status, unsigned int cause, context* pt_context);
// Process release cpu
void __syscall_schedule(unsigned int status, unsigned int cause, context* pt_context);
// fork: a0 = 0 means caller,a0>0 means new task
void syscall_fork(unsigned int status, unsigned int cause, context* pt_context);
// sleep:process will sleep a0 ms
void syscall_sleep(unsigned int status, unsigned int cause, context* pt_context);
// wait:blocked entil task a0 ends 
void syscall_wait(unsigned int status, unsigned int cause, context* pt_context);

// Call a syscall with code in v0 and parameter a0
int call_syscall_a0(int code,int a0);

// Debug utils
int sw(int bit);

// Time utils
void pc_time_get(time_u64* dst);
int pc_time_cmp(time_u64 *large, time_u64 *small);
void pc_time_add(time_u64 *dst,unsigned int src);

#endif  // !_ZJUNIX_PC_H