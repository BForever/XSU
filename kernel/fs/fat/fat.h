#ifndef _FAT_H
#define _FAT_H

#include <xsu/fs/fat.h>
#include <xsu/type.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT) // 4k
#define FAT_BUF_NUM 2

extern BUF_512 fat_buf[FAT_BUF_NUM];
extern struct fs_info fat_info;

u32 fs_create_with_attr(u8* filename, u8 attr);
u32 read_fat_sector(u32 ThisFATSecNum);
u32 write_fat_sector(u32 index);

u32 init_fat_info();
void init_fat_buf();
void init_dir_buf();
u32 fs_next_slash(u8* f);
u32 fs_cmp_filename(const u8* f1, const u8* f2);

#endif
