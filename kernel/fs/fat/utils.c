#include "utils.h"
#include "fat.h"
#include <driver/sd.h>

// Read/Write block for FAT (start from first block of partition 1).
u32 read_block(u8* buf, u32 addr, u32 count)
{
    return sd_read_block(buf, addr, count);
}

u32 write_block(u8* buf, u32 addr, u32 count)
{
    return sd_write_block(buf, addr, count);
}

// Char to u16/u32 (little endian).
u16 get_u16(u8* ch)
{
    return (*ch) + ((*(ch + 1)) << 8);
}

u32 get_u32(u8* ch)
{
    return (*ch) + ((*(ch + 1)) << 8) + ((*(ch + 2)) << 16) + ((*(ch + 3)) << 24);
}

// u16/u32 to char.
void set_u16(u8* ch, u16 num)
{
    *ch = (u8)(num & 0xff);
    *(ch + 1) = (u8)((num >> 8) & 0xff);
}

void set_u32(u8* ch, u32 num)
{
    *ch = (u8)(num & 0xff);
    *(ch + 1) = (u8)((num >> 8) & 0xff);
    *(ch + 2) = (u8)((num >> 16) & 0xff);
    *(ch + 3) = (u8)((num >> 24) & 0xff);
}

// Work around.
// Return the bits of num.
// FIXME: Bug?
u32 fs_wa(u32 num)
{
    u32 i;
    for (i = 0; num > 1; num >>= 1, i++)
        ;
    return i;
}

u32 get_entry_filesize(u8* entry)
{
    return get_u32(entry + 24);
}

u32 get_entry_attr(u8* entry)
{
    return entry[11];
}

// DIR_FIRST_CLUSTER_HI/LO to cluster.
u32 get_start_cluster(const FILE* file)
{
    return (file->entry.attr.starthi << 16) + (file->entry.attr.startlow);
}

// Get FAT entry value for a cluster.
u32 get_fat_entry_value(u32 clus, u32* clusEntryVal)
{
    u32 thisFATSecNum;
    u32 thisFATEntOffset;
    u32 index;

    cluster_to_fat_entry(clus, &thisFATSecNum, &thisFATEntOffset);
    index = read_fat_sector(thisFATSecNum);
    if (index == 0xffffffff) {
        goto get_fat_entry_value_error;
    }
    *clusEntryVal = get_u32(fat_buf[index].buf + thisFATEntOffset) & 0xffffffff;
    return 0;

get_fat_entry_value_error:
    return 1;
}

// Modify FAT for a cluster.
u32 fs_modify_fat(u32 clus, u32 clusEntryVal)
{
    u32 thisFATSecNum;
    u32 thisFATEntOffset;
    u32 fat32_val;
    u32 index;

    cluster_to_fat_entry(clus, &thisFATSecNum, &thisFATEntOffset);
    index = read_fat_sector(thisFATSecNum);
    if (index == 0xffffffff) {
        goto fs_modify_fat_error;
    }
    // Set the reference and dirty bit to 1.
    fat_buf[index].state = 3;

    clusEntryVal &= 0x0fffffff;
    fat32_val = (get_u32(fat_buf[index].buf + thisFATEntOffset) & 0xf0000000) | clusEntryVal;
    set_u32(fat_buf[index].buf + thisFATEntOffset, fat32_val);
    return 0;

fs_modify_fat_error:
    return 1;
}

// Determine FAT entry for cluster.
void cluster_to_fat_entry(u32 clus, u32* thisFATSecNum, u32* thisFATEntOffset)
{
    u32 FATOffset = clus << 2;
    *thisFATSecNum = fat_info.BPB.attr.reserved_sectors + (FATOffset >> 9) + fat_info.base_addr;
    *thisFATEntOffset = FATOffset & 511;
}

// Data cluster num <=> sector num.
u32 fs_data_cluster_to_sector(u32 clus)
{
    return ((clus - 2) << fs_wa(fat_info.BPB.attr.sectors_per_cluster)) + fat_info.first_data_sector;
}

u32 fs_sector_to_data_cluster(u32 sec)
{
    return ((sec - fat_info.first_data_sector) >> fs_wa(fat_info.BPB.attr.sectors_per_cluster)) + 2;
}
