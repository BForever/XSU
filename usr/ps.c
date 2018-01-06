#include "ps.h"
#include "../usr/ls.h"
#include <driver/ps2.h>
#include <driver/sd.h>
#include <driver/vga.h>
#include <kern/errno.h>
#include <xsu/device.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/log.h>
#include <xsu/time.h>
#include <xsu/utils.h>

char ps_buffer[64];
int ps_buffer_index;

void test_syscall4()
{
    asm volatile(
        "li $a0, 0x00ff\n\t"
        "li $v0, 4\n\t"
        "syscall\n\t"
        "nop\n\t");
}

/*
 * Command for mounting a filesystem.
 */

// Table of mountable filesystem types.
static const struct {
    const char* name;
    int (*func)(const char* device);
} mounttable[] = {
#ifdef FAT32
    { "FAT32", fat_mount },
#endif
    { NULL, NULL }
};

static int cmd_mount(int nargs, char** args)
{
    char* fstype;
    char* device;
    int i;

    if (nargs != 3) {
        kernel_printf("Usage: mount fstype device:\n");
        return EINVAL;
    }

    fstype = args[1];
    device = args[2];

    // Allow (but do not require) colon after device name.
    if (device[kernel_strlen(device) - 1] == ':') {
        device[kernel_strlen(device) - 1] = 0;
    }

    for (i = 0; mounttable[i].name; i++) {
        if (!kernel_strcmp(mounttable[i].name, fstype)) {
            return mounttable[i].func(device);
        }
    }
    kernel_printf("Unknown filesystem type %s\n", fstype);
    return EINVAL;
}

static int cmd_unmount(int nargs, char** args)
{
    char* device;

    if (nargs != 2) {
        kernel_printf("Usage: unmount device:\n");
        return EINVAL;
    }

    device = args[1];

    // Allow (but do not require) colon after device name.
    if (device[kernel_strlen(device) - 1] == ':') {
        device[kernel_strlen(device) - 1] = 0;
    }

    return vfs_unmount(device);
}

/*
 * Command to set the "boot fs". 
 *
 * The boot filesystem is the one that pathnames like /bin/sh with
 * leading slashes refer to.
 *
 * The default bootfs is "emu0".
 */
static int cmd_bootfs(int nargs, char** args)
{
    char* device;

    if (nargs != 2) {
        kernel_printf("Usage: bootfs device\n");
        return EINVAL;
    }

    device = args[1];

    // Allow (but do not require) colon after device name.
    if (device[kernel_strlen(device) - 1] == ':') {
        device[kernel_strlen(device) - 1] = 0;
    }

    return vfs_setbootfs(device);
}

// void test_proc()
// {
//     unsigned int timestamp;
//     unsigned int currTime;
//     unsigned int data;
//     asm volatile("mfc0 %0, $9, 6\n\t"
//                  : "=r"(timestamp));
//     data = timestamp & 0xff;
//     while (1) {
//         asm volatile("mfc0 %0, $9, 6\n\t"
//                      : "=r"(currTime));
//         if (currTime - timestamp > 100000000) {
//             timestamp += 100000000;
//             *((unsigned int*)0xbfc09018) = data;
//         }
//     }
// }

// int proc_demo_create()
// {
//     int asid = pc_peek();
//     if (asid < 0) {
//         kernel_puts("Failed to allocate pid.\n", 0xfff, 0);
//         return 1;
//     }
//     unsigned int init_gp;
//     asm volatile("la %0, _gp\n\t"
//                  : "=r"(init_gp));
//     pc_create(asid, test_proc, (unsigned int)kmalloc(4096), init_gp, "test");
//     return 0;
// }

void ps()
{
    // Test for mount.
    log(LOG_START, "Test mount command.");
    char* args[] = { "mount", "FAT32", "sd" };
    cmd_mount(3, args);
    log(LOG_END, "Test mount command.");

    // Test for remove.
    log(LOG_START, "Test `vfs_remove`.");
    vfs_remove("sd:/text.txt");
    log(LOG_END, "Testg `vfs_remove`.");

    kernel_printf("Press any key to enter shell.\n");
    kernel_getchar();
    char c;
    ps_buffer_index = 0;
    ps_buffer[0] = 0;
    kernel_clear_screen(31);
    kernel_puts("PowerShell\n", 0xfff, 0);
    kernel_puts("PS>", 0xfff, 0);
    while (1) {
        c = kernel_getchar();
        if (c == '\n') {
            ps_buffer[ps_buffer_index] = 0;
            if (kernel_strcmp(ps_buffer, "exit") == 0) {
                ps_buffer_index = 0;
                ps_buffer[0] = 0;
                kernel_printf("\nPowerShell exit.\n");
            } else
                parse_cmd();
            ps_buffer_index = 0;
            kernel_puts("PS>", 0xfff, 0);
        } else if (c == 0x08) {
            if (ps_buffer_index) {
                ps_buffer_index--;
                kernel_putchar_at(' ', 0xfff, 0, cursor_row, cursor_col - 1);
                cursor_col--;
                kernel_set_cursor();
            }
        } else {
            if (ps_buffer_index < 63) {
                ps_buffer[ps_buffer_index++] = c;
                kernel_putchar(c, 0xfff, 0);
            }
        }
    }
}

void parse_cmd()
{
    unsigned int result = 0;
    char dir[32];
    char c;
    kernel_putchar('\n', 0, 0);
    char sd_buffer[8192];
    int i = 0;
    char* param;
    for (i = 0; i < 63; i++) {
        if (ps_buffer[i] == ' ') {
            ps_buffer[i] = 0;
            break;
        }
    }
    if (i == 63)
        param = ps_buffer;
    else
        param = ps_buffer + i + 1;
    if (ps_buffer[0] == 0) {
        return;
    } else if (kernel_strcmp(ps_buffer, "clear") == 0) {
        kernel_clear_screen(31);
    } else if (kernel_strcmp(ps_buffer, "echo") == 0) {
        kernel_printf("%s\n", param);
    } else if (kernel_strcmp(ps_buffer, "gettime") == 0) {
        char buf[10];
        get_time(buf, sizeof(buf));
        kernel_printf("%s\n", buf);
    } else if (kernel_strcmp(ps_buffer, "syscall4") == 0) {
        test_syscall4();
    } else if (kernel_strcmp(ps_buffer, "sdwi") == 0) {
        for (i = 0; i < 512; i++)
            sd_buffer[i] = i;
        sd_write_block(sd_buffer, 7, 1);
        kernel_puts("sdwi\n", 0xfff, 0);
    } else if (kernel_strcmp(ps_buffer, "sdr") == 0) {
        sd_read_block(sd_buffer, 7, 1);
        for (i = 0; i < 512; i++) {
            kernel_printf("%d ", sd_buffer[i]);
        }
        kernel_putchar('\n', 0xfff, 0);
    } else if (kernel_strcmp(ps_buffer, "sdwz") == 0) {
        for (i = 0; i < 512; i++) {
            sd_buffer[i] = 0;
        }
        sd_write_block(sd_buffer, 7, 1);
        kernel_puts("sdwz\n", 0xfff, 0);
    } else if (kernel_strcmp(ps_buffer, "ls") == 0) {
        result = ls(param);
        kernel_printf("ls return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "mkdir") == 0) {
        result = fs_mkdir(param);
        kernel_printf("mkdir return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "create") == 0) {
        result = fs_create(param);
        kernel_printf("create return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "rm") == 0) {
        result = fs_rm(param);
        kernel_printf("rm return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "cat") == 0) {
        result = fs_cat(param);
        kernel_printf("cat return with %d\n", result);
    } else {
        kernel_puts(ps_buffer, 0xfff, 0);
        kernel_puts(": command not found\n", 0xfff, 0);
    }
}
