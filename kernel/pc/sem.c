#include "pc.h"
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>

//Semaphore
struct semaphore* create_semaphore(char* name, int count)
{
    struct semaphore* new = kmalloc(sizeof(struct semaphore));
    kernel_memcpy(new->name, name, 29);
    new->name[29] = 0;
    new->count = count;
    INIT_LIST_HEAD(&new->wait_list);
}
void release_semaphore(struct semaphore* sem)
{
    // Kill all possible waiting processes
    struct list_head *pos, *n;
    task_struct* waiter;
    list_for_each_safe(pos, n, &sem->wait_list)
    {
        waiter = list_entry(pos, task_struct, wait);
        __kill(waiter);
    }
    // Free struct
    kfree((void*)sem);
}
void sem_wait(struct semaphore* sem)
{
    int old = disable_interrupts();
    //decrese the count
    sem->count--;
    //if not available
    if (sem->count <= -1) {
        //start waiting
        current->state = PROC_STATE_WAITING;
        //go to wait list
        list_add_tail(&current->wait, &sem->wait_list);
        //run next process
        enable_interrupts(old);
        __request_schedule();
    } else {
        enable_interrupts(old);
    }
    
    
}
void sem_signal(struct semaphore* sem)
{
    int old = disable_interrupts();
    //increase the count
    sem->count++;
    //if there's processes waiting
    if (!list_empty(&sem->wait_list)) {
        //get the first process(FIFO)
        task_struct* task = list_entry(sem->wait_list.next, task_struct, wait);
        //awake it
        list_del(sem->wait_list.next);
        //become the next process in ready list
        list_add(&task->ready, &ready_list[task->level]);
    }
    enable_interrupts(old);
}