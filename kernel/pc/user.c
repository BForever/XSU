#include "pc.h"

#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>

task_struct* create_process(char* name, unsigned int phy_code, unsigned int length, unsigned int level)
{
    task_struct* task = create_kthread(name, level, 0);

    //set context
    task->context.sp = USER_STACK; //sp
    asm volatile("la %0, _gp"
                 : "=r"(task->context.gp)); //gp
    task->context.epc = 0; //start address
    task->kernel_stack = (unsigned int)task + 4096;

    task->kernelflag = 0; //whether kernal thread

    //get user address space
    task->pagecontent = kmalloc(4096);
    kernel_printf("malloc pagecontent:%x\n", task->pagecontent);

    clearpage((void*)task->pagecontent);

    // kernel_printf("add_code_vma(%x,%x,%d);\n",&task->vma,phy_code,length);
    INIT_LIST_HEAD(&task->vma);
    add_code_vma(&task->vma, phy_code, length); //map code
    // kernel_printf("add_vma(%x,%x,%d);\n",task->vma.next,((unsigned int)task + 4096),4096);

    task->user_stack = (unsigned int)kmalloc(4096);
    add_stack_vma(&task->vma, task->user_stack, 4096); //map stack
    task->vma_heap_tail = task->vma.next; //prepare for insertion of heap space
    // printvmalist(task);
    map_all(task);
    // printTLB();
    // TLBEntry* tlb = task->pagecontent[0];
    // printTLBEntry(tlb);
    // insertTLB(task->pagecontent[0]);
    // printTLB();
    // int i;
    // kernel_printf("code of user space:\n");
    // for (i=0;i<9;i++)
    // {
    //     kernel_printf("%x\n",*((unsigned int*)(i*4)));
    // }
    unsigned badaddr = 0xc0000000;
    unsigned content;
    content = badaddr - PAGETABLE_BASE;
    content >>= 12;
    if (task->pagecontent[content]) {
        TLBEntry tmp;
        tmp.entryhi = ((badaddr >> 13) << 13) | task->ASID;
        tmp.pagemask = 0;
        if (content & 1) {
            kernel_printf("if(content&1)\n");
            tmp.entrylo0 = 0x81000000;
            tmp.entrylo1 = ((unsigned)((unsigned)task->pagecontent[content] & 0x7FFFFFFF) >> 12) << 6 | (0x3 << 3) | (0x1 << 2) | (0x1 << 1);
        } else {
            kernel_printf("else(content&1==0)\n");
            tmp.entrylo0 = ((unsigned)((unsigned)task->pagecontent[content] & 0x7FFFFFFF) >> 12) << 6 | (0x3 << 3) | (0x1 << 2) | (0x1 << 1);
            tmp.entrylo1 = 0x81000000;
        }
        kernel_printf("try insertTLB(&tmp);\n");
        insertTLB(&tmp);
        kernel_printf("refill tlb:");
        printTLBEntry(&tmp);
        kernel_printf("pgd's first tlb:");
        printTLBEntry((TLBEntry*)(((tmp.entrylo0 >> 6) << 12) | 0x80000000));
    }
    //to run
    list_add_tail(&task->ready, &ready_list[task->level]);
    task->state = PROC_STATE_READY;

    return task;
}

void clearpage(void* pagestart)
{
    kernel_memset_word(pagestart, (unsigned int)0, 1024);
}

void add_code_vma(struct list_head* vmahead, unsigned int pa, unsigned int length) //pa must be the start of a frame
{
    if (pa & 0xFFF != 0) {
        kernel_printf("invalid code physical address!\n");
        while (1)
            ; //invalid code address;
    }
    vma_node* new = kmalloc(sizeof(vma_node));
    new->va_start = 0;
    new->va_end = length - 1;
    new->pa = pa;

    list_add(&new->vma, vmahead);
}
void add_stack_vma(struct list_head* vmahead, unsigned int pa, unsigned int length)
{
    if (pa & 0xFFF != 0) {
        kernel_printf("invalid stack physical address!\n");
        while (1)
            ; //invalid code address;
    }
    vma_node* new = kmalloc(sizeof(vma_node));
    new->va_start = 0x80000000 - length;
    new->va_end = new->va_start + length - 1;
    new->pa = pa;

    list_add_tail(&new->vma, vmahead);
}
int add_vma(struct list_head* vma_prev, unsigned int pa, unsigned int length)
{
    vma_node* prev = list_entry(vma_prev, vma_node, vma);
    if (prev->va_end >= USER_STACK - USER_STACK_SIZE) {
        return 0; //beyond user space;
    }
    vma_node* new = kmalloc(sizeof(vma_node));
    new->va_start = (prev->va_end & ~(0xFFF)) + 0x1000;
    new->va_start += pa & 0xFFF;
    new->va_end = new->va_start + length - 1;
    new->pa = pa;

    __list_add(&new->vma, vma_prev, vma_prev->next);
    return 1;
}
vma_node* findvma(unsigned int va)
{
    struct list_head* pos;
    list_for_each(pos, &current->vma)
    {
        vma_node* cur = list_entry(pos, vma_node, vma);
        if (cur->va_start <= va && cur->va_end >= va) {
            return cur;
        }
    }
    return 0;
}
void map_all(task_struct* task)
{
    struct list_head* pos;
    vma_node* vma;
    list_for_each(pos, &task->vma)
    {
        vma = list_entry(pos, vma_node, vma);
        kernel_printf("mapping:%x-%x->%x-%x\n", vma->va_start, vma->va_end, vma->pa, vma->pa + vma->va_end - vma->va_start);
        do_mapping(task->ASID, vma, task->pagecontent);
    }
}
void free_heap(task_struct* task)
{
    struct list_head *pos, *tmp;
    pos = task->vma.next->next;
    while (pos != task->vma.prev) {
        tmp = pos->next;
        list_del(pos); //disconnect from vma list
        kfree(list_entry(pos, vma_node, vma)); //free vma_node struction
        pos = tmp;
    }
}
void unmap_all(task_struct* task) //except code and stack
{
    TLB_init();
    struct list_head* pos;
    vma_node* curr;
    list_for_each(pos, &task->vma) //release all allocated space
    {
        curr = list_entry(pos, vma_node, vma);
        kfree((void*)curr->pa);
    }
    free_heap(task);
    int i;
    for (i = 0; i < 1024; i++) //release page table
    {
        if (task->pagecontent[i]) {
            kfree(task->pagecontent[i]);
        }
    }
}
int do_mapping(unsigned int asid, vma_node* vma, TLBEntry** pagecontent)
{
    TLBEntry* pgd;
    unsigned int va = vma->va_start;
    unsigned int pa = vma->pa;

    asid &= 0xFF;
    va &= ~(0xFFF);
    pa &= 0x7FFFF000;

    unsigned int vastart = va >> 12;
    unsigned int vaend = vma->va_end >> 12;

    unsigned int pagenum = vaend - vastart + 1;
    unsigned int startvpn2 = va >> 13;
    unsigned int startpfn2 = pa >> 13;
    int i;
    // kernel_printf("vastart:%x,pagenum:%x\n",vastart,pagenum);
    if (vastart & 1) {
        pgd = pagecontent[startvpn2 >> 8];
        if (pgd == 0) {
            pgd = kmalloc(4096);
            pagecontent[startvpn2 >> 8] = pgd;
            kernel_printf("malloc pgd:%x\n", pgd);
            clearpage(pgd);
        }

        pgd[startvpn2 & 255].entryhi = (va & (~0x1FFF)) | asid;
        pgd[startvpn2 & 255].entrylo1 = ((pa >> 12) << 6) | (0x3 << 3) | (0x1 << 1); //using cache, D = 0, valid
        pgd[startvpn2 & 255].pagemask = 0;
        pa += 0x1000;
        va += 0x1000;
        pagenum--;
    }
    for (i = 0; i < pagenum; i++) {
        unsigned int pfn = (pa >> 12) + i;
        unsigned int vpn = (va >> 12) + i;
        unsigned int pfn2 = pfn >> 1;
        unsigned int vpn2 = vpn >> 1;

        // kernel_printf("pagecontent[0]:%x [1]:%x\n",pagecontent[0],pagecontent[1]);
        pgd = pagecontent[vpn2 >> 8];
        if (pgd == 0) {
            pgd = kmalloc(4096);
            pagecontent[vpn2 >> 8] = pgd;
            kernel_printf("malloc pgd:%x\n", pgd);
            clearpage(pgd);
        }
        kernel_printf("pgd=%x ", pgd);
        // kernel_printf("vpn2=%x\n",vpn2);
        while (sw(1))
            ;
        if (i % 2 == 0) {
            pgd[vpn2 & 255].entryhi = (va & (~0x1FFF)) | asid;
            pgd[vpn2 & 255].pagemask = 0;
            pgd[vpn2 & 255].entrylo0 = (pfn << 6) | (0x3 << 3) | (0x1 << 2) | (0x1 << 1); //using cache, D = 1, valid
            // kernel_printf("pgd[vpn2&255].entrylo0 =%x\n",pgd[vpn2&255].entrylo0);
            // kernel_printf("pgd=%x pagecontent[0]=%x,pagecontent=%x\n",pgd,pagecontent[0],pagecontent);
        } else {
            pgd[vpn2 & 255].entrylo1 = (pfn << 6) | (0x3 << 3) | (0x1 << 2) | (0x1 << 1);
        }
    }
}
int do_unmapping(vma_node* vma, TLBEntry** pagecontent)
{
    unsigned int vpn_start = vma->va_start >> 12;
    unsigned int vpn_end = vma->va_end >> 12;
    unsigned int pagenum = vpn_end - vpn_start + 1;
    int vpn;
    for (vpn = vpn_start; vpn <= vpn_end; vpn++) {
        TLBEntry* pgd = pagecontent[vpn >> 9];
        int vpn2 = vpn >> 1;
        if (vpn % 2 == 0) {
            pgd[vpn2].entrylo0 = 0; //invalid
        } else {
            pgd[vpn2].entrylo1 = 0; //invalid
        }
    }
}

int addrvalid(unsigned int badaddr)
{
    if (current->kernelflag)
        return 1;
    struct list_head* pos;
    //TO DO: decide whether current vpn2 is usable for current process
    list_for_each(pos, &current->vma)
    {
        vma_node* cur = list_entry(pos, vma_node, vma);
        if (cur->va_start <= badaddr && cur->va_end >= badaddr) {
            return 1;
        }
    }
    return 0;
}