#ifndef _DRIVER_PS2
#define _DRIVER_PS2

#include <intr.h>

void init_ps2();
void ps2_handler(unsigned int status, unsigned int cause, context* context);
int kernel_getkey();
int kernel_getchar();

#endif