#ifndef _FAT_H
#define _FAT_H

#include <xsu/fs/fat.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)

#define FAT_BUF_NUM 2
extern BUF_512 fat_buf[FAT_BUF_NUM];

extern struct fs_info fat_info;

uint32_t fs_create_with_attr(uint8_t* filename, uint8_t attr);
uint32_t read_fat_sector(uint32_t ThisFATSecNum);

#endif
