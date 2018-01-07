#ifndef _ARCH_H
#define _ARCH_H

// machine params
#define MACHINE_MMSIZE 128 * 1024 * 1024 // 128MB
#define MACHINE_SDSIZE 16 * 1024 * 1024 * 2 // 32M Sectors
#define CHAR_VRAM_SIZE 128 * 32 * 4 // 128*32*4
#define PAGE_TABLE_SIZE 256 * 1024 // 4MB
#define GRAPHIC_VRAM_SIZE 1024 * 512 * 4 // 1024*512*4 b-g-r

// Virtual Memory
#define BIOS_ENTRY 0xbfc00000
#define KERNEL_STACK_BOTTOM 0x81000000
#define KERNEL_CODE_ENTRY 0x80001000
#define KERNEL_ENTRY 0x80000000
#define USER_ENTRY 0x00000000

extern unsigned int* const CHAR_VRAM;
extern unsigned int* const GRAPHIC_VRAM;
extern unsigned int* const GPIO_SWITCH; // siwtch read-only
extern unsigned int* const GPIO_BUTTON; // button read-only
extern unsigned int* const GPIO_SEG; // Seg R/W
extern unsigned int* const GPIO_LED; // LED R/W
extern unsigned int* const GPIO_PS2_DATA; // PS2 data register, R/W
extern unsigned int* const GPIO_PS2_CTRL; // PS2 control register, R/W
extern unsigned int* const GPIO_UART_DATA; // UART data register, R/W
extern unsigned int* const GPIO_UART_CTRL; // UART control register, R/W
extern unsigned int* const GPIO_CURSOR; // Cursor 8-bit frequency 8-bit row 8-bit col
extern unsigned int* const VGA_MODE; // enable graphic mode

//register storage in task struct
typedef struct {
    unsigned int epc;
    unsigned int at;
    unsigned int v0, v1;
    unsigned int a0, a1, a2, a3;
    unsigned int t0, t1, t2, t3, t4, t5, t6, t7;
    unsigned int s0, s1, s2, s3, s4, s5, s6, s7;
    unsigned int t8, t9;
    unsigned int hi, lo;
    unsigned int gp;
    unsigned int sp;
    unsigned int fp;
    unsigned int ra;
} context;


// kernel sp
extern volatile unsigned int kernel_sp;

// PS/2 control register:
// [ 5: 0]: RX buffer load(R)
// [13: 0]: TX buffer load(R)
// [18:16]: Error code(R)
//    [31]: Interrupt enable(RW)

// UART control register:
// [ 7: 0]: RX buffer load(R)
// [15: 8]: TX buffer load(R)
// [18:16]: baud rate(RW)
//    [31]: Interrupt enable(RW)

#endif
