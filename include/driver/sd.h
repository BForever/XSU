#ifndef _DRIVER_SD_H
#define _DRIVER_SD_H

#include <xsu/types.h>

#define SECSIZE 512

u32 sd_read_block(unsigned char* buf, unsigned long addr, unsigned long count);
u32 sd_write_block(unsigned char* buf, unsigned long addr, unsigned long count);

#endif // ! _DRIVER_SD_H