#include <xsu/pc.h>
#include <xsu/syscall.h>
#include "top.h"

static unsigned int getprocessnum();
static unsigned int getrunningnum();
static unsigned int getsleepingnum();

void top() {
    int i;
    char* process = "processes: ";
    char* total = " total,";
    char* running = " running, ";
    char* sleeping = " sleeping";

    char* processnum = "   ";
    char* runningnum = "   ";
    char* sleepingnum = "   ";
    while(1)
    {
        itoa(getprocessnum(),processnum,3);
        itoa(getrunningnum(),runningnum,3);
        itoa(getsleepingnum(),sleepingnum,3);

        for (i = 0; i < 11; i++)
            kernel_putchar_at(process[i], 0xfff, 0, 29, 0 + i);
        for (i = 0; i < 3; i++)
            kernel_putchar_at(processnum[i], 0xfff, 0, 29, 11 + i);
        for (i = 0; i < 7; i++)
            kernel_putchar_at(total[i], 0xfff, 0, 29, 14 + i);
        for (i = 0; i < 3; i++)
            kernel_putchar_at(runningnum[i], 0xfff, 0, 29, 17 + i);
        for (i = 0; i < 10; i++)
            kernel_putchar_at(running[i], 0xfff, 0, 29, 27 + i);
        for (i = 0; i < 3; i++)
            kernel_putchar_at(sleepingnum[i], 0xfff, 0, 29, 30 + i);
        for (i = 0; i < 9; i++)
            kernel_putchar_at(sleeping[i], 0xfff, 0, 29, 39 + i);
        
        call_syscall_a0(SYSCALL_SLEEP,1000);
    }
    
    // asm volatile("mtc0 $zero, $9");
}

void itoa(unsigned int num,char* dst,unsigned int maxlength)
{
    int i;
    for (i = maxlength - 1; i >= 0; i--)
    {
        dst[i] = (num % 10) + '0';
        num /= 10; 
    }
}

static unsigned int getprocessnum()
{
    struct list_head *pos;
    unsigned int count = 0;
    list_for_each(pos,&shed_list)
    {
        count++;
    }
    return count;
}
static unsigned int getrunningnum()
{
    struct list_head *pos;
    int i;
    unsigned int count = 0;
    for(i = 0; i < PROC_LEVELS; i++)
    {
        list_for_each(pos,&ready_list[i])
        {
            count++;
        }
    }
    // Consider current running task, which is top
    return count+1; 
}
static unsigned int getsleepingnum()
{
    struct list_head *pos;
    unsigned int count = 0;
    list_for_each(pos,&sleep_list)
    {
        count++;
    }
    return count;
}