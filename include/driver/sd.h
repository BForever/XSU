#ifndef _DRIVER_SD_H
#define _DRIVER_SD_H

#define SECSIZE 512

unsigned int sd_read_block(unsigned char* buf, unsigned long addr, unsigned long count);
unsigned int sd_write_block(unsigned char* buf, unsigned long addr, unsigned long count);

#endif // ! _DRIVER_SD_H