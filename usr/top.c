#include "top.h"
#include <driver/ps2.h>
#include <driver/vga.h>
#include <xsu/pc.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>

static unsigned int getprocessnum();
static unsigned int getrunningnum();
static unsigned int getsleepingnum();

void top()
{
    while (1) {


        char c = kernel_getchar();
        kernel_clear_screen(31);

        if (c == 'q') {
            return;
        }

        // Process info.
        kernel_printf("Processes: %d total, %d running, %d sleeping.\n", getprocessnum(), getrunningnum(), getsleepingnum());

        // Mem info.
        kmemtop();

        // Every process
        struct list_head* pos;
        task_struct* task;
        kernel_printf("NAME\tASID\tSTATE\tCOUNTER\n");
        list_for_each(pos, &shed_list)
        {
            task = list_entry(pos, task_struct, shed);
            printtask(task);
        }

        //call_syscall_a0(SYSCALL_SLEEP, 1000);
    }

    // asm volatile("mtc0 $zero, $9");
}

static unsigned int getprocessnum()
{
    struct list_head* pos;
    unsigned int count = 0;
    list_for_each(pos, &shed_list)
    {
        count++;
    }
    return count;
}

static unsigned int getrunningnum()
{
    struct list_head* pos;
    int i;
    unsigned int count = 0;
    for (i = 0; i < PROC_LEVELS; i++) {
        list_for_each(pos, &ready_list[i])
        {
            count++;
        }
    }
    // Consider current running task, which is top
    return count + 1;
}

static unsigned int getsleepingnum()
{
    struct list_head* pos;
    unsigned int count = 0;
    list_for_each(pos, &sleep_list)
    {
        count++;
    }
    return count;
}