#include "pc.h"
#include <driver/vga.h>
#include <intr.h>
#include <exc.h>
#include <xsu/syscall.h>
#include <xsu/utils.h>
#include <xsu/slab.h>
void TLB_init()
{
    unsigned int i;
    unsigned int vpn2 = 0x81000000;
    unsigned int PTEBase = PAGETABLE_BASE;
    for(i=0;i<32;i++)
    {
        asm volatile(
            "mtc0    $zero, $2\n\t"//clear valid bit
            "mtc0    $zero, $3\n\t"
            "mtc0    $zero, $5\n\t"//clear pagemask
            "mtc0    %0,    $10\n\t"//set vpn2 to different value
            "mtc0    %1,    $0\n\t"//set index
            "nop\n\t"//# CP0 hazard
            "nop\n\t"//# CP0 hazard
            "tlbwi"
            :
            :"r"(vpn2), "r"(i)
        );
    }
    asm volatile("mtc0  %0,$4"::"r"(PTEBase));    
}

//print one TLB Entry
void printTLBEntry(TLBEntry *tlb)
{
    kernel_printf("entryhi:%x entrylo0:%x entrylo1:%x pagemask:%x\n",tlb->entryhi,tlb->entrylo0,tlb->entrylo1,tlb->pagemask);
}

unsigned int testTLB(unsigned int va)
{
    int asid = getasid();
    unsigned int entryhi,entrylo0,entrylo1,pagemask;
    entryhi = (va&0xFFFFFE000) | current->ASID;
    kernel_printf("the testing entryhi is %x\n",entryhi);
    asm volatile(
        "mtc0   %2,$10\n\t"
        "nop\n\t"
        "nop\n\t"
        "tlbp\n\t"
        "tlbr\n\t"
        "nop\n\t"
        "nop\n\t"
        "mfc0   %0,$2\n\t"
        "mfc0   %1,$3\n\t"
        "mfc0   %2,$10\n\t"
        "mfc0   %3,$5\n\t"
        :"=r"(entrylo0),"=r"(entrylo1),"=r"(entryhi),"=r"(pagemask)
    );
    
    unsigned int result = (va&0x1000)?entrylo1:entrylo0;
    kernel_printf("the result pfn is %x\n",result);
    result &= ~(0xFFF);
    result |= (va&0xFFF);
    setasid(asid);
    return result;
}
//print all noninital TLB Entry
void printTLB()
{
    int asid = getasid();
    int i;
    kernel_printf("TLB modified list:\n");
    for(i=0;i<32;i++)
    {
        unsigned int entryhi,entrylo0,entrylo1,pagemask;
        asm volatile(
            "mtc0   %4,$0\n\t"//set index
            "nop\n\t"//cp0 harzard
            "nop\n\t"//cp0 harzard
            "tlbr\n\t"
            "nop\n\t"//cp0 harzard
            "nop\n\t"//cp0 harzard
            "mfc0   %0,$2\n\t"
            "nop\n\t"
            "mfc0   %1,$3\n\t"
            "nop\n\t"
            "mfc0   %2,$10\n\t"
            "nop\n\t"
            "mfc0   %3,$5\n\t"
            "nop\n\t"
            :"=r"(entrylo0),"=r"(entrylo1),"=r"(entryhi),"=r"(pagemask)
            :"r"(i)
        );
        //whether initial Entry
        if(entryhi!=0x81000000)
        {
            kernel_printf("index %d:",i);
            kernel_printf("entryhi:%x entrylo0:%x entrylo1:%x pagemask:%x\n",entryhi,entrylo0,entrylo1,pagemask);
        }   
    }
    setasid(asid);
    
}


//ocurrs when process want to write on a page which TLBentry's Dirty bit is zero
//used for copy on write
void TLBMod_exc(unsigned int status, unsigned int cause, context* context)
{
    unsigned int badaddr;
    unsigned int badvpn2;
    asm volatile(
        "mfc0   %0, $8\n\t"
        :"=r"(badaddr)
    );
    //first lookup the process's write privilege on this page
    //if it's able to write, copy to a new physical page and set Dirty
    kernel_printf("TLBMod_exc at %x",badaddr);
    //if not, invalid write and kill the process
}
//ocurrs for two situations:
//1:invalid TLBentry
//2:no match TLBentry :which means refill failed, status[EXL]=1
void TLBL_exc(unsigned int status, unsigned int cause, context* context)
{
    unsigned int badaddr;
    unsigned int content;
    asm volatile(
        "mfc0   %0, $8\n\t"
        :"=r"(badaddr)
    );
    unsigned int seg = GPIO_SEG;
    asm volatile(
        "sw   $sp, 0(%0)\n\t"
        ::"r"(seg)
    );
    kernel_printf("TLBL_exc at %x,EPC = %x,ASID = %d,tasid:%d\n",badaddr,context->epc,getasid(),current->ASID);
    while(sw(2));
    //while(1);
    printTLB();
    if(badaddr>=0xc0000000)
    {
        // kernel_printf("content = badaddr - PAGETABLE_BASE;\n");
        content = badaddr - PAGETABLE_BASE;
        content >>= 12;
        // kernel_printf("if(current->pagecontent[content]--content=%x)\n",content);
        if(current->pagecontent[content])
        {
            TLBEntry tmp;
            tmp.entryhi = ((badaddr>>13)<<13) | current->ASID;
            tmp.pagemask = 0;
            if(content&1)
            {
                // kernel_printf("if(content&1)\n");
                tmp.entrylo0 = 0x81000000;
                tmp.entrylo1 = ((unsigned)((unsigned)current->pagecontent[content]&0x7FFFFFFF)>>12)<<6 | (0x3<<3) | (0x1<<2) | (0x1<<1);
            }
            else
            {
                // kernel_printf("else(content&1==0)\n");
                tmp.entrylo0 = ((unsigned)((unsigned)current->pagecontent[content]&0x7FFFFFFF)>>12)<<6 | (0x3<<3) | (0x1<<2) | (0x1<<1);
                tmp.entrylo1 = 0x81000000;
            }
            // kernel_printf("try insertTLB(&tmp,current->ASID);&tmp=%x\n",&tmp);
            
            insertTLB(&tmp,current->ASID);
            kernel_printf("refill tlb:");
            printTLBEntry(&tmp);
            // kernel_printf("pgd's first tlb:");
            printTLBEntry((TLBEntry*)(((tmp.entrylo0>>6)<<12)|0x80000000));
            
        }
        else
        {
            kernel_printf("%s","Memory access error! Process try to write via illegal address.\n");
            while(1);//
        }
    }
    else//invalid entry
    {
        kernel_printf("TLB:detect invalid entry,do nothing.\n");
        
        // if(addrvalid(badaddr))
        // {
        //     kernel_printf("TLB:address valid, refill.\n");
        //     TLBrefill();
        // }
        // else
        // {
        //     kernel_printf("%s","Memory access error! Process try to read via illegal address.\n");
        //     while(1);//
        // }
    } 
    while(sw(3));
    kernel_printf("TLBL_exc finished\n");
    printTLB();
}
void TLBS_exc(unsigned int status, unsigned int cause, context* context)
{
    unsigned int badaddr;
    unsigned int content;
    asm volatile(
        "mfc0   %0, $8\n\t"
        :"=r"(badaddr)
    );
    kernel_printf("TLBS_exc at %x,EPC = %x,ASID = %d,tasid:%d\n",badaddr,context->epc,getasid(),current->ASID);
    while(sw(2));
    //while(1);
    if(badaddr>=0xc0000000)
    {
        content = badaddr - PAGETABLE_BASE;
        content >>= 12;
        if(current->pagecontent[content])
        {
            TLBEntry tmp;
            tmp.entryhi = ((badaddr>>13)<<13) | current->ASID;
            tmp.pagemask = 0;
            if(content&1)
            {
                kernel_printf("if(content&1)\n");
                tmp.entrylo0 = 0x81000000;
                tmp.entrylo1 = ((unsigned)((unsigned)current->pagecontent[content]&0x7FFFFFFF)>>12)<<6 | (0x3<<3) | (0x1<<2) | (0x1<<1);
            }
            else
            {
                kernel_printf("else(content&1==0)\n");
                tmp.entrylo0 = ((unsigned)((unsigned)current->pagecontent[content]&0x7FFFFFFF)>>12)<<6 | (0x3<<3) | (0x1<<2) | (0x1<<1);
                tmp.entrylo1 = 0x81000000;
            }
            kernel_printf("try insertTLB(&tmp,current->ASID);\n");
            insertTLB(&tmp,current->ASID);
            kernel_printf("refill tlb:");
            printTLBEntry(&tmp);
            kernel_printf("pgd's first tlb:");
            printTLBEntry((TLBEntry*)(((tmp.entrylo0>>6)<<12)|0x80000000));
            
        }
        else
        {
            kernel_printf("%s","Memory access error! Process try to write via illegal address.\n");
            while(1);//
        }
    }
    else//invalid entry
    {
        kernel_printf("TLB:detect invalid entry, do nothing.");
        
        // if(addrvalid(badaddr))
        // {
        //     TLBrefill();
        // }
        // else
        // {
        //     kernel_printf("%s","Memory access error! Process try to read via illegal address.\n");
        //     while(1);//
        // }
    } 
    kernel_printf("TLBS_exc finished\n");
    while(sw(3));
    printTLB();
}
//TO DO: when schedule exception return to user process, set user mode
//TO DO: TLB initialize set all valid 0, and set all vpn2 different and asid 0
//       incase there are same matches which lead to error mapping
void TLBrefill()
{
    unsigned int t1,t2;
    asm volatile(
        "mfc0    %0, $4\n\t"
        "lw		 %1, 0(%0)\n\t"
        "mtc0    %1, $2\n\t"
        "lw      %1, 4(%0)\n\t"
        "mtc0    %1, $3\n\t"
        "lw      %1, 8(%0)\n\t"
        "mtc0    %1, $10\n\t"
        "lw      %1, 12(%0)\n\t"
        "mtc0    %1, $5\n\t"
        "nop\n\t"//# CP0 hazard
        "nop\n\t"//# CP0 hazard
        "tlbwr"
        :"=r"(t1),"=r"(t2)
    );
    kernel_printf("context:%x\n",t1);
    kernel_printf("refill tlb:");
    printTLBEntry(t1);
    printTLB();
}

void insertTLB(TLBEntry *entry,int asid)
{
    kernel_printf("insertTLB()started--entry=%x\n",entry);
    asm volatile(
        "addi    $sp, $sp, -8\n\t"
        "sw      $k0, 0($sp)\n\t"
        "sw      $k1, 4($sp)\n\t"
        "move    $k0, %0\n\t"
        "lw		 $k1, 0($k0)\n\t"
        "mtc0    $k1, $2\n\t"
        "lw      $k1, 4($k0)\n\t"
        "mtc0    $k1, $3\n\t"
        "lw      $k1, 8($k0)\n\t"
        "mtc0    $k1, $10\n\t"
        "lw      $k1, 12($k0)\n\t"
        "mtc0    $k1, $5\n\t"
        "lw      $k0, 0($sp)\n\t"//# CP0 hazard
        "lw      $k1, 4($sp)\n\t"//# CP0 hazard
        "addi    $sp, $sp, 8\n\t"
        "tlbwr"
        :
        :"r"(entry)
    );
    // kernel_printf("inside insertTLB()--try setasid(%d)\n",asid);
    setasid(asid);
    // kernel_printf("insertTLB() returning\n",asid);
    // kernel_printf("your asid now is: %d\n",getasid());
}