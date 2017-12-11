#include "utils.h"
#include "fat.h"
#include <driver/sd.h>

// Read/Write block for FAT (starts from first block of partition 1).
uint32_t read_block(uint8_t* buf, uint32_t addr, uint32_t count)
{
    return sd_read_block(buf, addr, count);
}

uint32_t write_block(uint8_t* buf, uint32_t addr, uint32_t count)
{
    return sd_write_block(buf, addr, count);
}

// char to uint16_t/uint32_t.
// Little endian.
uint16_t get_uint16_t(uint8_t* ch)
{
    return (*ch) + ((*(ch + 1)) << 8);
}

uint32_t get_uint32_t(uint8_t* ch)
{
    return (*ch) + ((*(ch + 1)) << 8) + ((*(ch + 2)) << 16) + ((*(ch + 3)) << 24);
}

// uint16_t/uint32_t to char.
void set_uint16_t(uint8_t* ch, uint16_t num)
{
    *ch = (uint8_t)(num & 0xFF);
    *(ch + 1) = (uint8_t)((num >> 8) & 0xFF);
}

void set_uint32_t(uint8_t* ch, uint32_t num)
{
    *ch = (uint8_t)(num & 0xFF);
    *(ch + 1) = (uint8_t)((num >> 8) & 0xFF);
    *(ch + 2) = (uint8_t)((num >> 16) & 0xFF);
    *(ch + 3) = (uint8_t)((num >> 24) & 0xFF);
}

// work around.
uint32_t fs_wa(uint32_t num)
{
    // Return the exponent of 2.
    uint32_t i;
    for (i = 0; num > 1; num >>= 1, i++)
        ;
    return i;
}

uint32_t get_entry_filesize(uint8_t* entry)
{
    return get_uint32_t(entry + 28);
}

uint32_t get_entry_attr(uint8_t* entry)
{
    return entry[11];
}

// DIR_FstClusHI/LO to cluster.
uint32_t get_start_cluster(const FILE* file)
{
    return (file->entry.attr.starthi << 16) + (file->entry.attr.startlow);
}

// Get fat entry value for a cluster.
uint32_t get_fat_entry_value(uint32_t clus, uint32_t* ClusEntryVal)
{
    uint32_t ThisFATSecNum;
    uint32_t ThisFATEntOffset;
    uint32_t index;

    cluster_to_fat_entry(clus, &ThisFATSecNum, &ThisFATEntOffset);

    index = read_fat_sector(ThisFATSecNum);
    if (index == 0xffffffff)
        goto get_fat_entry_value_err;

    *ClusEntryVal = get_uint32_t(fat_buf[index].buf + ThisFATEntOffset) & 0x0FFFFFFF;

    return 0;
get_fat_entry_value_err:
    return 1;
}

// Modify fat for a cluster.
uint32_t fs_modify_fat(uint32_t clus, uint32_t ClusEntryVal)
{
    uint32_t ThisFATSecNum;
    uint32_t ThisFATEntOffset;
    uint32_t fat32_val;
    uint32_t index;

    cluster_to_fat_entry(clus, &ThisFATSecNum, &ThisFATEntOffset);

    index = read_fat_sector(ThisFATSecNum);
    if (index == 0xffffffff)
        goto fs_modify_fat_err;

    fat_buf[index].state = 3;

    ClusEntryVal &= 0x0FFFFFFF;
    fat32_val = (get_uint32_t(fat_buf[index].buf + ThisFATEntOffset) & 0xF0000000) | ClusEntryVal;
    set_uint32_t(fat_buf[index].buf + ThisFATEntOffset, fat32_val);

    return 0;
fs_modify_fat_err:
    return 1;
}

// Determine FAT entry for cluster.
void cluster_to_fat_entry(uint32_t clus, uint32_t* ThisFATSecNum, uint32_t* ThisFATEntOffset)
{
    uint32_t FATOffset = clus << 2;
    *ThisFATSecNum = fat_info.BPB.attr.reserved_sectors + (FATOffset >> 9) + fat_info.base_addr;
    *ThisFATEntOffset = FATOffset & 511;
}

// data cluster num <=> sector num
uint32_t fs_dataclus2sec(uint32_t clus)
{
    return ((clus - 2) << fs_wa(fat_info.BPB.attr.sectors_per_cluster)) + fat_info.first_data_sector;
}

uint32_t fs_sec2dataclus(uint32_t sec)
{
    return ((sec - fat_info.first_data_sector) >> fs_wa(fat_info.BPB.attr.sectors_per_cluster)) + 2;
}
