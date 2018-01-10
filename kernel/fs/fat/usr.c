#include "fat.h"
#include "utils.h"
#include <driver/vga.h>
#include <xsu/log.h>
#include <xsu/slab.h>

uint8_t mk_dir_buf[32];

// Remove directory entry.
uint32_t fs_rm(uint8_t* filename)
{
    uint32_t clus;
    uint32_t next_clus;
    FILE mk_dir;

    if (fs_open(&mk_dir, filename) == 1)
        goto fs_rm_err;

    // Mark 0xE5.
    mk_dir.entry.data[0] = 0xE5;

    // Release all allocated block.
    clus = get_start_cluster(&mk_dir);

    while (clus != 0 && clus <= fat_info.total_data_clusters + 1) {
        if (get_fat_entry_value(clus, &next_clus) == 1)
            goto fs_rm_err;

        if (fs_modify_fat(clus, 0) == 1)
            goto fs_rm_err;

        clus = next_clus;
    }

    if (fs_close(&mk_dir) == 1)
        goto fs_rm_err;

    return 0;
fs_rm_err:
    return 1;
}

uint32_t fs_mv(uint8_t* src, uint8_t* dest)
{
    FILE oldfile, newfile;

    // Open src.
    if (fs_open(&oldfile, src) == 1) {
        goto fs_mv_err;
    }

    // Read src.
    uint32_t file_size = get_entry_filesize(oldfile.entry.data);
    uint8_t* buf = (uint8_t*)kmalloc(file_size + 1);
    fs_read(&oldfile, buf, file_size);
    buf[file_size] = 0;

#ifdef FS_DEBUG
    kernel_printf("fs_mv:\n");
    kernel_printf("file size: %d\n", file_size);
    kernel_printf("%s\n", buf);
#endif

    // Close src.
    if (fs_close(&oldfile) == 1) {
        goto fs_mv_err;
    }

    // Create dst.
    if (fs_create(dest) == 1) {
        goto fs_mv_err;
    }

    // Open dst.
    if (fs_open(&newfile, dest) == 1) {
        goto fs_mv_err;
    }

    // Write dst.
    if (fs_write(&newfile, buf, file_size) == 1) {
        goto fs_mv_err;
    }

    // Close dst.
    if (fs_close(&newfile) == 1) {
        goto fs_mv_err;
    }

    // Delete src.
    if (fs_rm(src) == 1) {
        goto fs_mv_err;
    }

    kfree(buf);
    return 0;
fs_mv_err:
    return 1;
}

uint32_t fs_cp(unsigned char* src, unsigned char* dest)
{
    FILE oldfile, newfile;

    // Open src.
    if (fs_open(&oldfile, src) == 1) {
        goto fs_cp_err;
    }

    // Read src.
    uint32_t file_size = get_entry_filesize(oldfile.entry.data);
    uint8_t* buf = (uint8_t*)kmalloc(file_size + 1);
    fs_read(&oldfile, buf, file_size);
    buf[file_size] = 0;

#ifdef FS_DEBUG
    kernel_printf("fs_cp:\n");
    kernel_printf("file size: %d\n", file_size);
    kernel_printf("%s\n", buf);
#endif

    // Close src.
    if (fs_close(&oldfile) == 1) {
        goto fs_cp_err;
    }

    // Create dst.
    if (fs_create(dest) == 1) {
        goto fs_cp_err;
    }

    // Open dst.
    if (fs_open(&newfile, dest) == 1) {
        goto fs_cp_err;
    }

    // Write dst.
    if (fs_write(&newfile, buf, file_size) == 1) {
        goto fs_cp_err;
    }

    // Close dst.
    if (fs_close(&newfile) == 1) {
        goto fs_cp_err;
    }

    kfree(buf);
    return 0;
fs_cp_err:
    return 1;
}

// mkdir, create a new file and write . and ..
uint32_t fs_mkdir(uint8_t* filename)
{
    uint32_t i;
    FILE mk_dir;
    FILE file_create;

    // create this directory.
    if (fs_create_with_attr(filename, 0x10) == 1)
        goto fs_mkdir_err;

    // write . in this directory.
    char dot[256];
    kernel_memcpy(dot, filename, kernel_strlen(filename) + 1);
    kernel_strcat(dot, "/..");
    if (fs_create_with_attr(dot, 0x10) == 1)
        goto fs_mkdir_err;

    // write .. in this directory.
    char dotdot[256];
    kernel_memcpy(dotdot, filename, kernel_strlen(filename) + 1);
    kernel_strcat(dotdot, "/...");
    if (fs_create_with_attr(dotdot, 0x10) == 1)
        goto fs_mkdir_err;

    // if (fs_open(&mk_dir, filename) == 1)
    //     goto fs_mkdir_err;

    // mk_dir_buf[0] = '.';
    // for (i = 1; i < 11; i++)
    //     mk_dir_buf[i] = 0;

    // // FIXME: 0x30 (here) or 0x10 (ls)
    // // https://www.wikiwand.com/en/Design_of_the_FAT_file_system
    // mk_dir_buf[11] = 0x10;
    // for (i = 12; i < 32; i++)
    //     mk_dir_buf[i] = 0;

    // if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
    //     goto fs_mkdir_err;

    // mk_dir_buf[0] = '.';
    // mk_dir_buf[1] = '.';

    // for (i = 2; i < 11; i++)
    //     mk_dir_buf[i] = 0x20;

    // mk_dir_buf[11] = 0x10;
    // for (i = 12; i < 32; i++)
    //     mk_dir_buf[i] = 0;

    // mk_dir_buf[14] = mk_dir.entry.data[14];
    // mk_dir_buf[15] = mk_dir.entry.data[15];
    // mk_dir_buf[16] = mk_dir.entry.data[16];
    // mk_dir_buf[17] = mk_dir.entry.data[17];
    // mk_dir_buf[18] = mk_dir.entry.data[18];
    // mk_dir_buf[19] = mk_dir.entry.data[19];
    // mk_dir_buf[22] = mk_dir.entry.data[22];
    // mk_dir_buf[23] = mk_dir.entry.data[23];
    // mk_dir_buf[24] = mk_dir.entry.data[24];
    // mk_dir_buf[25] = mk_dir.entry.data[25];

    // set_uint16_t(mk_dir_buf + 20, (file_create.dir_entry_pos >> 16) & 0xFFFF);
    // set_uint16_t(mk_dir_buf + 26, file_create.dir_entry_pos & 0xFFFF);

    // if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
    //     goto fs_mkdir_err;

    // for (i = 28; i < 32; i++)
    //     mk_dir.entry.data[i] = 0;

    // if (fs_close(&mk_dir) == 1)
    //     goto fs_mkdir_err;

    return 0;
fs_mkdir_err:
    return 1;
}

uint32_t fs_cat(uint8_t* path)
{
    uint8_t filename[12];
    FILE cat_file;

    /* Open */
    if (0 != fs_open(&cat_file, path)) {
        log(LOG_FAIL, "File %s open failed", path);
        return 1;
    }

    /* Read */
    uint32_t file_size = get_entry_filesize(cat_file.entry.data);
    uint8_t* buf = (uint8_t*)kmalloc(file_size + 1);
    fs_read(&cat_file, buf, file_size);
    buf[file_size] = 0;
    kernel_printf("%s\n", buf);
    fs_close(&cat_file);
    kfree(buf);
    return 0;
}
