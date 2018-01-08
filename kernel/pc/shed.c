#include "pc.h"

#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>

void copy_context(context* src, context* dest)
{
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
task_struct* __getnexttask()
{
    flushsleeplist(); //update sleep list
    task_struct* next = (task_struct*)0;
    int i;
    for (i = PROC_LEVELS - 1; i >= 0; i--) {
        if (!list_empty(&ready_list[i])) {
            next = list_entry(ready_list[i].next, task_struct, ready);
            break;
        }
    }
    if (!next) {
        kernel_printf("No process running\n");
        while (1)
            ; //panic
    }
}
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context)
{
    //printalltask();
    //printreadylist();
    current->counter--;
    // Check whether the process' timeslots are used out
    if (current->counter) {
        current->counter--;
        __reset_counter();
    } else // Restore counter and choose the next process
    {
        current->counter = PROC_DEFAULT_TIMESLOTS;
        current->state = PROC_STATE_READY;
        list_add_tail(&current->ready, &ready_list[current->level]);

        __pc_schedule(status, cause, pt_context);
    }
}
void __pc_schedule(unsigned int status, unsigned int cause, context* pt_context) //find next task and load context
{
    copy_context(pt_context, &current->context);
    // Get next ready task
    current = __getnexttask();
    list_del_init(&current->ready);
    if (current) {
        current->state = PROC_STATE_RUNNING;
    }

    // Load context
    copy_context(&current->context, pt_context);

    __reset_counter();
}
void __reset_counter()
{
    setasid(current->ASID);
    kernel_sp = current->kernel_stack;

    // Reset counter
    asm volatile("mtc0 $zero, $9\n\t");
}