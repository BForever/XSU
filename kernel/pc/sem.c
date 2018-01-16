#include "pc.h"
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>

// Semaphore
// Create a semaphore
struct semaphore* create_semaphore(char* name, int count)
{
    // Malloc space
    struct semaphore* new = kmalloc(sizeof(struct semaphore));
    // Set name
    kernel_memcpy(new->name, name, 29);
    // Cut the name
    new->name[29] = 0;
    // Initialize count
    new->count = count;
    // Initalize wait list
    INIT_LIST_HEAD(&new->wait_list);
}

// Release semaphore
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

// Wait fo resource
void sem_wait(struct semaphore* sem)
{
    // Disable interrupt
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
    } 
    else 
    {
        // Enable interrupt
        enable_interrupts(old);
    }
}

// Resource released
void sem_signal(struct semaphore* sem)
{
    // Disable interrupt
    int old = disable_interrupts();
    // Increase the count
    sem->count++;
    // If there's processes waiting
    if (!list_empty(&sem->wait_list)) {
        // Get the first process(FIFO)
        task_struct* task = list_entry(sem->wait_list.next, task_struct, wait);
        // Awake it
        list_del(sem->wait_list.next);
        // Become the next process in ready list
        list_add(&task->ready, &ready_list[task->level]);
    }
    // Enable interrupt
    enable_interrupts(old);
}