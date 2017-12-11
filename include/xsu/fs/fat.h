#ifndef _XSU_FS_FAT_H
#define _XSU_FS_FAT_H

#include <xsu/fs/fscache.h>
#include <xsu/types.h>

/* 4k data buffer number in each file struct */
#define LOCAL_DATA_BUF_NUM 4

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 4096

struct __attribute__((__packed__)) dir_entry_attr {
    uint8_t name[8]; /* Name */
    uint8_t ext[3]; /* Extension */
    uint8_t attr; /* attribute bits */
    uint8_t lcase; /* Case for base and extension */
    uint8_t ctime_cs; /* Creation time, centiseconds (0-199) */
    uint16_t ctime; /* Creation time */
    uint16_t cdate; /* Creation date */
    uint16_t adate; /* Last access date */
    uint16_t starthi; /* Start cluster (Hight 16 bits) */
    uint16_t time; /* Last modify time */
    uint16_t date; /* Last modify date */
    uint16_t startlow; /* Start cluster (Low 16 bits) */
    uint32_t size; /* file size (in bytes) */
};

union dir_entry {
    uint8_t data[32];
    struct dir_entry_attr attr;
};

/* file struct */
typedef struct fat_file {
    unsigned char path[256];
    /* Current file pointer */
    unsigned long loc;
    /* Current directory entry position */
    unsigned long dir_entry_pos;
    unsigned long dir_entry_sector;
    /* current directory entry */
    union dir_entry entry;
    /* Buffer clock head */
    unsigned long clock_head;
    /* For normal FAT32, cluster size is 4k */
    BUF_4K data_buf[LOCAL_DATA_BUF_NUM];
} FILE;

typedef struct fs_fat_dir {
    unsigned long cur_sector;
    unsigned long loc;
    unsigned long sec;
} FS_FAT_DIR;

struct __attribute__((__packed__)) BPB_attr {
    // 0x00 ~ 0x0f
    uint8_t jump_code[3];
    uint8_t oem_name[8];
    uint16_t sector_size;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    // 0x10 ~ 0x1f
    uint8_t number_of_copies_of_fat;
    uint16_t max_root_dir_entries;
    uint16_t num_of_small_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_of_heads;
    uint32_t num_of_hidden_sectors;
    // 0x20 ~ 0x2f
    uint32_t num_of_sectors;
    uint32_t num_of_sectors_per_fat;
    uint16_t flags;
    uint16_t version;
    uint32_t cluster_number_of_root_dir;
    // 0x30 ~ 0x3f
    uint16_t sector_number_of_fs_info;
    uint16_t sector_number_of_backup_boot;
    uint8_t reserved_data[12];
    // 0x40 ~ 0x51
    uint8_t logical_drive_number;
    uint8_t unused;
    uint8_t extended_signature;
    uint32_t serial_number;
    uint8_t volume_name[11];
    // 0x52 ~ 0x1fe
    uint8_t fat_name[8];
    uint8_t exec_code[420];
    uint8_t boot_record_signature[2];
};

union BPB_info {
    uint8_t data[512];
    struct BPB_attr attr;
};

struct fs_info {
    uint32_t base_addr;
    uint32_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t total_data_clusters;
    uint32_t total_data_sectors;
    uint32_t first_data_sector;
    union BPB_info BPB;
    uint8_t fat_fs_info[SECTOR_SIZE];
};

unsigned long fs_find(FILE* file);
unsigned long init_fs();
unsigned long fs_open(FILE* file, unsigned char* filename);
unsigned long fs_close(FILE* file);
unsigned long fs_read(FILE* file, unsigned char* buf, unsigned long count);
unsigned long fs_write(FILE* file, const unsigned char* buf, unsigned long count);
unsigned long fs_fflush();
void fs_lseek(FILE* file, unsigned long new_loc);
unsigned long fs_create(unsigned char* filename);
unsigned long fs_mkdir(unsigned char* filename);
unsigned long fs_rm(unsigned char* filename);
unsigned long fs_mv(unsigned char* src, unsigned char* dest);
unsigned long fs_open_dir(FS_FAT_DIR* dir, unsigned char* filename);
unsigned long fs_read_dir(FS_FAT_DIR* dir, unsigned char* buf);
unsigned long fs_cat(unsigned char* path);
void get_filename(unsigned char* entry, unsigned char* buf);
uint32_t read_block(uint8_t* buf, uint32_t addr, uint32_t count);
uint32_t write_block(uint8_t* buf, uint32_t addr, uint32_t count);
uint32_t get_entry_filesize(uint8_t* entry);
uint32_t get_entry_attr(uint8_t* entry);

#endif
