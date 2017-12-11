#ifndef _FAT_UTILS_H
#define _FAT_UTILS_H

#include <xsu/fs/fat.h>
#include <xsu/types.h>

// Manipulate data through pointers.
uint16_t get_uint16_t(uint8_t* ch);
uint32_t get_uint32_t(uint8_t* ch);
void set_uint16_t(uint8_t* ch, uint16_t num);
void set_uint32_t(uint8_t* ch, uint32_t num);
uint32_t fs_wa(uint32_t num);

// Get file entry info.
uint32_t get_start_cluster(const FILE* file);
uint32_t get_fat_entry_value(uint32_t clus, uint32_t* ClusEntryVal);
uint32_t fs_modify_fat(uint32_t clus, uint32_t ClusEntryVal);

void cluster_to_fat_entry(uint32_t clus, uint32_t* ThisFATSecNum, uint32_t* ThisFATEntOffset);
uint32_t fs_dataclus2sec(uint32_t clus);
uint32_t fs_sec2dataclus(uint32_t sec);

#endif
