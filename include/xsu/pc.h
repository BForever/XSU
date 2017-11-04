#ifndef _XSU_PC_H
#define _XSU_PC_H

//#include <xsu/list.h>
#include <exc.h>


typedef struct {
    context context;
    int ASID;
    unsigned int counter;
    char name[32];
    unsigned long start_time;
    enum {
        PROC_STATE_READY = 0,
        PROC_STATE_RUNNING,
        PROC_STATE_WAITING
    }state;
} task_struct;

typedef union {
    task_struct task;
    unsigned char kernel_stack[4096];
} task_union;

#define PROC_DEFAULT_TIMESLOTS 2

void init_pc();
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context);
int pc_peek();
void pc_create(int asid, void (*func)(), unsigned int init_sp, unsigned int init_gp, char* name);
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context);
int pc_kill(int proc);
task_struct* get_curr_pcb();
int print_proc();

#endif  // !_XSU_PC_H