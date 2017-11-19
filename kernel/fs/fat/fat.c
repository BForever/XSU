#include "fat.h"
#include "utils.h"
#include <driver/vga.h>
#include <xsu/log.h>

#ifdef FS_DEBUG
#include "debug.h"
#include <intr.h>
#endif // !FS_DEBUG

// FAT buffer clock head.
u32 fat_clock_head = 0;
BUF_512 fat_buf[FAT_BUF_NUM];

u8 filename11[13];
u8 new_alloc_empty[PAGE_SIZE];

#define DIR_DATA_BUF_NUM 4
BUF_512 dir_data_buf[DIR_DATA_BUF_NUM];
u32 dir_data_clock_head = 0;

struct fs_info fat_info;

u32 init_fat_info()
{
    u8 meta_buf[512];

    // Init buffers.
    kernel_memset(meta_buf, 0, sizeof(meta_buf));
    kernel_memset(&fat_info, 0, sizeof(struct fs_info));

    // Get MBR sector.
    if (read_block(meta_buf, 0, 1) == 1) {
        goto init_fat_info_error;
    }
    log(LOG_OK, "Get MBR Sector info");
    // MBR partition 1 entry starts from +446, and LBA starts from +8.
    fat_info.base_addr = get_u32(meta_buf + 446 + 8);

    // Get FAT BPB.
    if (read_block(fat_info.BPB.data, fat_info.base_addr, 1) == 1) {
        goto init_fat_info_error;
    }
    log(LOG_OK, "Get FAT BPB");
#ifdef FS_DEBUG
    dump_bpb_info(&(fat_info.BPB.attr));
#endif

    // Sector size (MBR[11]) must be SECTOR_SIZE bytes.
    if (fat_info.BPB.attr.sector_size != SECTOR_SIZE) {
        log(LOG_FAIL, "FAT sector size must be %d bytes, but get %d bytes.", SECTOR_SIZE, fat_info.BPB.attr.sector_size);
        goto init_fat_info_error;
    }

    // Determine FAT type.
    // For FAT32, max root dir entries must be 0.
    if (fat_info.BPB.attr.max_root_dir_entries != 0) {
        goto init_fat_info_error;
    }
    // For FAT32, sectors per fat at BPB[0x16] is 0.
    if (fat_info.BPB.attr.sectors_per_fat != 0) {
        goto init_fat_info_error;
    }
    // For FAT32, total sectors at BPB[0x16] is 0.
    if (fat_info.BPB.attr.num_of_small_sectors != 0) {
        goto init_fat_info_error;
    }
    // If not FAT32, goto error state.
    u32 total_sectors = fat_info.BPB.attr.num_of_sectors;
    u32 reserved_sectors = fat_info.BPB.attr.reserved_sectors;
    u32 sectors_per_fat = fat_info.BPB.attr.num_of_sectors_per_fat;
    u32 total_data_sectors = total_sectors - reserved_sectors - sectors_per_fat * 2;
    u8 sectors_per_cluster = fat_info.BPB.attr.sectors_per_cluster;
    fat_info.total_data_clusters = total_data_sectors / sectors_per_cluster;
    if (fat_info.total_data_clusters < 65525) {
        goto init_fat_info_error;
    }

    // Get root dir sector.
    fat_info.first_data_sector = reserved_sectors + sectors_per_fat * 2;
    log(LOG_OK, "Partition type determined: FAT32");

    // Keep FSInfo in buffer.
    read_block(fat_info.fat_fs_info, 1 + fat_info.base_addr, 1);
    log(LOG_OK, "Get FSInfo sector");

#ifdef FS_DEBUG
    dump_fat_info(&(fat_info));
#endif

    // Init success.
    return 0;
init_fat_info_error:
    return 1;
}

void init_fat_buf()
{
    for (int i = 0; i < FAT_BUF_NUM; i++) {
        fat_buf[i].cur = 0xffffffff;
        fat_buf[i].state = 0;
    }
}

void init_dir_buf()
{
    for (int i = 0; i < DIR_DATA_BUF_NUM; i++) {
        dir_data_buf[i].cur = 0xffffffff;
        dir_data_buf[i].state = 0;
    }
}

// File system initialize.
u32 init_fs()
{
    u32 success = init_fat_info();
    if (success != 0) {
        goto fs_init_error;
    }
    init_fat_buf();
    init_dir_buf();
    return 0;
fs_init_error:
    log(LOG_FAIL, "File system inits fail.");
    return 1;
}

// Write current FAT sector.
u32 write_fat_sector(u32 index)
{
    if ((fat_buf[index].cur != 0xffffffff) && (((fat_buf[index].state) & 0x02) != 0)) {
        // Write FAT and FAT copy.
        if (write_block(fat_buf[index].buf, fat_buf[index].cur, 1) == 1)
            goto write_fat_sector_error;
        if (write_block(fat_buf[index].buf, fat_info.BPB.attr.num_of_sectors_per_fat + fat_buf[index].cur, 1) == 1)
            goto write_fat_sector_error;
        fat_buf[index].state &= 0x01;
    }
    return 0;
write_fat_sector_error:
    return 1;
}

// Read FAT sector.
u32 read_fat_sector(u32 ThisFATSecNum)
{
    u32 index;
    // Try to find in buffer.
    for (index = 0; (index < FAT_BUF_NUM) && (fat_buf[index].cur != ThisFATSecNum); index++)
        ;

    // If not in buffer, find victim and replace, otherwise set the reference bit.
    if (index == FAT_BUF_NUM) {
        index = fs_victim_512(fat_buf, &fat_clock_head, FAT_BUF_NUM);
        if (write_fat_sector(index) == 1) {
            goto read_fat_sector_error;
        }
        if (read_block(fat_buf[index].buf, ThisFATSecNum, 1) == 1) {
            goto read_fat_sector_error;
        }

        fat_buf[index].cur = ThisFATSecNum;
        fat_buf[index].state = 1;
    } else {
        fat_buf[index].state |= 0x01;
    }

    return index;
read_fat_sector_error:
    return 0xffffffff;
}

// Path conversion.
u32 fs_next_slash(u8* f)
{
    u32 i, j, k;
    u8 chr11[13];

    for (i = 0; (*(f + i) != '\0') && (*(f + i) != '/'); i++)
        ;
    for (j = 0; j < 12; j++) {
        chr11[j] = '\0';
        filename11[j] = 0x20;
    }
    for (j = 0; j < 12 && j < i; j++) {
        chr11[j] = *(f + j);
        if (chr11[j] >= 'a' && chr11[j] <= 'z') {
            // To uppercase.
            chr11[j] = (u8)(chr11[j] - 'a' + 'A');
        }
    }
    chr11[12] = '\0';

    for (j = 0; (chr11[j] != '\0') && (j < 12); j++) {
        if (chr11[j] == '.') {
            break;
        }
        filename11[j] = chr11[j];
    }
    if (chr11[j] == '.') {
        j++;
        for (k = 8; (chr11[j] != '\0') && (j < 12) && (k < 11); j++, k++) {
            filename11[k] = chr11[j];
        }
    }
    filename11[11] = '\0';

    return i;
}

// strcmp
u32 fs_cmp_filename(const u8* f1, const u8* f2)
{
    for (u32 i = 0; i < 11; i++) {
        if (f1[i] != f2[i]) {
            return 1;
        }
    }
    return 0;
}

// Find a path, only absolute path starting with '/' is accepted.
u32 fs_find(FILE* file)
{
    u8* f = file->path;
    u32 next_slash;
    u32 i, k;
    u32 next_clus;
    u32 index;
    u32 sec;

    if (*(f++) != '/') {
        goto fs_find_error;
    }
    index = fs_read_512(dir_data_buf, fs_data_cluster_to_sector(2), &dir_data_clock_head, DIR_DATA_BUF_NUM);
    // Open root directory.
    if (index == 0xffffffff) {
        goto fs_find_error;
    }

    // Find directory entry.
    while (1) {
        file->dir_entry_pos = 0xffffffff;
        next_slash = fs_next_slash(f);

        while (1) {
            for (sec = 1; sec < fat_info.BPB.attr.sectors_per_cluster; sec++) {
                // Find directory entry in current cluster.
                for (i = 0; i < 512; i += 32) {
                    if (*(dir_data_buf[index].buf + i) == 0) {
                        goto after_fs_find;
                    }
                    // Ignore long path.
                    if ((fs_cmp_filename(dir_data_buf[index].buf + i, filename11) == 0) && ((*(dir_data_buf[index].buf + i + 11) & 0x08) == 0)) {
                        file->dir_entry_pos = i;
                        // Refer to the issue in fs_close().
                        file->dir_entry_sector = dir_data_buf[index].cur - fat_info.base_addr;

                        for (k = 0; k < 32; k++) {
                            file->entry_data[k] = *(dir_data_buf[index].buf + i + k);
                        }

                        goto after_fs_find;
                    }
                }

                // Next sector is in current cluster.
                if (sec < fat_info.BPB.attr.sectors_per_cluster) {
                    index = fs_read_512(dir_data_buf, dir_data_buf[index].cur + 1, &dir_data_clock_head, DIR_DATA_BUF_NUM);
                    if (index == 0xffffffff) {
                        goto fs_find_error;
                    }
                } else {
                    // Read next cluster of current directory.
                    if (get_fat_entry_value(dir_data_buf[index].cur - fat_info.BPB.attr.sectors_per_cluster + 1, &next_clus) == 1) {
                        goto fs_find_error;
                    }
                    if (next_clus <= fat_info.total_data_clusters + 1) {
                        index = fs_read_512(dir_data_buf, fs_data_cluster_to_sector(next_clus), &dir_data_clock_head, DIR_DATA_BUF_NUM);
                        if (index == 0xffffffff) {
                            goto fs_find_error;
                        }
                    } else {
                        goto after_fs_find;
                    }
                }
            }
        }
    after_fs_find:
        // If not found.
        if (file->dir_entry_pos == 0xffffffff) {
            goto fs_find_ok;
        }
        // If path parsing completes.
        if (f[next_slash] == '\0') {
            goto fs_find_ok;
        }
        // If not a sub-directory.
        if ((file->entry.data[11] & 0x10) == 0) {
            goto fs_find_error;
        }
        f += next_slash + 1;

        // Open sub-directory, high word(+20), low word(+26).
        next_clus = get_start_cluster(file);
        if (next_clus <= fat_info.total_data_clusters + 1) {
            index = fs_read_512(dir_data_buf, fs_data_cluster_to_sector(next_clus), &dir_data_clock_head, DIR_DATA_BUF_NUM);
            if (index == 0xffffffff) {
                goto fs_find_error;
            }
        } else {
            goto fs_find_error;
        }
    }
fs_find_ok:
    return 0;
fs_find_error:
    return 1;
}
