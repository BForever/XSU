#include "pc.h"

#include <intr.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>
#include "../../usr/ps.h"

//task_struct pcb[8];
task_union pcb[8];//Because there's no memory management at this time
int curr_proc;

static void copy_context(context* src, context* dest) {
    dest->epc = src->epc;
    dest->at = src->at;
    dest->v0 = src->v0;
    dest->v1 = src->v1;
    dest->a0 = src->a0;
    dest->a1 = src->a1;
    dest->a2 = src->a2;
    dest->a3 = src->a3;
    dest->t0 = src->t0;
    dest->t1 = src->t1;
    dest->t2 = src->t2;
    dest->t3 = src->t3;
    dest->t4 = src->t4;
    dest->t5 = src->t5;
    dest->t6 = src->t6;
    dest->t7 = src->t7;
    dest->s0 = src->s0;
    dest->s1 = src->s1;
    dest->s2 = src->s2;
    dest->s3 = src->s3;
    dest->s4 = src->s4;
    dest->s5 = src->s5;
    dest->s6 = src->s6;
    dest->s7 = src->s7;
    dest->t8 = src->t8;
    dest->t9 = src->t9;
    dest->hi = src->hi;
    dest->lo = src->lo;
    dest->gp = src->gp;
    dest->sp = src->sp;
    dest->fp = src->fp;
    dest->ra = src->ra;
}
//start up the schedule and run the init process
void init_pc() {
    int i;
    int init_gp;
    //initalize asid
    for (i = 1; i < 8; i++)
        pcb[i].task.ASID = -1;
    //set init process
    pcb[0].task.ASID = 0;
    pcb[0].task.counter = PROC_DEFAULT_TIMESLOTS;
    kernel_strcpy(pcb[0].task.name, "init");
    
    asm volatile("la %0, _gp":"=r"(init_gp));//load gp

    start:
    pc_create_union(1,ps,init_gp,"ps");//create process powershell
    pcb[0].task.state = PROC_STATE_WAITING;//hung up itself

    register_syscall(10, pc_kill_syscall);
    register_interrupt_handler(7, pc_schedule);
    
    //set timer and reset counter: cp0$11:timer value cp0$9:counter
    asm volatile(
        "li $v0, 500000\n\t"
        "mtc0 $v0, $11\n\t"
        "mtc0 $zero, $9");
        
    while (1){//if waked up, execute itself again
        if (pcb[0].task.state == PROC_STATE_RUNNING){
            goto start;
        }
    }

    
}


void pc_schedule(unsigned int status, unsigned int cause, context* pt_context) {
    //check whether the process' timeslots are used out
    if (pcb[curr_proc].task.counter)
    {
        pcb[curr_proc].task.counter--;
        goto timenotusedout;
    } 
    else //restore counter and choose the next process
    {
        pcb[curr_proc].task.counter = PROC_DEFAULT_TIMESLOTS;
        pcb[curr_proc].task.state = PROC_STATE_READY;
    }
    // Save context
    copy_context(pt_context, &(pcb[curr_proc].task.context));
    int i;
    for (i = 0; i < 8; i++) {
        curr_proc = (curr_proc + 1) & 7;
        if (pcb[curr_proc].task.ASID >= 0 && pcb[curr_proc].task.state == PROC_STATE_READY){
            pcb[curr_proc].task.state = PROC_STATE_RUNNING; 
            break;
        }
            
    }
    if (i == 8){ 
        

    }
    // Load context
    copy_context(&(pcb[curr_proc].task.context), pt_context);

    timenotusedout:
    //reset counter
    asm volatile("mtc0 $zero, $9\n\t");
}

int pc_peek() {
    int i = 0;
    for (i = 0; i < 8; i++)
        if (pcb[i].task.ASID < 0)
            break;
    if (i == 8)
        return -1;
    return i;
}

void pc_create_union(int asid, void (*func)(), unsigned int init_gp, char* name) {
    pcb[asid].task.context.epc = (unsigned int)func;
    pcb[asid].task.context.sp = (void*)(pcb[asid].kernel_stack + 4096);
    pcb[asid].task.context.gp = init_gp;
    kernel_strcpy(pcb[asid].task.name, name);
    pcb[asid].task.ASID = asid;
    pcb[asid].task.counter = PROC_DEFAULT_TIMESLOTS; 
}

void pc_create(int asid, void (*func)(), unsigned int init_sp, unsigned int init_gp, char* name) {
    pcb[asid].task.context.epc = (unsigned int)func;
    pcb[asid].task.context.sp = init_sp;
    pcb[asid].task.context.gp = init_gp;
    kernel_strcpy(pcb[asid].task.name, name);
    pcb[asid].task.ASID = asid;
    pcb[asid].task.counter = PROC_DEFAULT_TIMESLOTS;
}

void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context) {
    if (curr_proc != 0) {
        pcb[curr_proc].task.ASID = -1;
        pc_schedule(status, cause, pt_context);
    }
}

int pc_kill(int proc) {
    proc &= 7;
    if (proc != 0 && pcb[proc].task.ASID >= 0) {
        pcb[proc].task.ASID = -1;
        return 0;
    } else if (proc == 0)
        return 1;
    else
        return 2;
}

task_struct* get_curr_pcb() {
    return &pcb[curr_proc].task;
}

int print_proc() {
    int i;
    kernel_puts("PID name\n", 0xfff, 0);
    for (i = 0; i < 8; i++) {
        if (pcb[i].task.ASID >= 0)
            kernel_printf(" %x  %s\n", pcb[i].task.ASID, pcb[i].task.name);
    }
    return 0;
}
