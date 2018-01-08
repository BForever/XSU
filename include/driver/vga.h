#ifndef _DRIVER_VGA_H
#define _DRIVER_VGA_H

#include <xsu/utils.h>

extern int cursor_row;
extern int cursor_col;
extern int cursor_freq;

#define VGA_RED 0x00f
#define VGA_GREEN 0x0f0
#define VGA_BLUE 0xf00
#define VGA_CYAN 0xff0
#define VGA_YELLOW 0x0ff
#define VGA_MAGENTA 0xf0f
#define VGA_BLACK 0x000
#define VGA_WHITE 0xfff

struct screen_color {
    int front;
    int background;
};

void init_vga();
void kernel_set_cursor();
void kernel_clear_screen(int scope);
void kernel_scroll_screen();
void kernel_putchar_at(int ch, int fc, int bg, int row, int col);
int kernel_putchar(int ch, int fc, int bg);
int kernel_puts(const char* s, int fc, int bg);
int kernel_putint(int x, int fc, int bg);
unsigned int kernel_putuint(unsigned int x, int fc, int bg);
unsigned int kernel_putint_octal(unsigned int x, int fc, int bg);
unsigned int kernel_putint_hex(unsigned int x, int fc, int bg, bool capital);
int kernel_vprintf(const char* format, va_list ap);
int kernel_printf(const char* format, ...);

#endif
