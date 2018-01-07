#include "menu.h"
#include "ls.h"
#include <assert.h>
#include <driver/ps2.h>
#include <driver/sd.h>
#include <driver/vga.h>
#include <kern/errno.h>
#include <xsu/time.h>
#include <xsu/utils.h>
#include <xsu/vfs.h>

char buf[64];
int buf_index;
char sd_buffer[8192];

#define MAXMENUARGS 16

/*
 * Command for basic operations. 
 */
static int cmd_clear(int argc, char** argv)
{
    kernel_clear_screen(31);
    return 0;
}

static int cmd_echo(int argc, char** argv)
{
    kernel_printf("%s\n", argv[1]);
    return 0;
}

static int cmd_ime(int argc, char** argv)
{
    char buf[10];
    get_time(buf, sizeof(buf));
    kernel_printf("%s\n", buf);
    return 0;
}

/*
 * Command for hardware test. 
 */

static int cmd_syscall4(int argc, char** argv)
{
    asm volatile(
        "li $a0, 0x00ff\n\t"
        "li $v0, 4\n\t"
        "syscall\n\t"
        "nop\n\t");
    return 0;
}

static int cmd_sdwi(int argc, char** argv)
{
    for (i = 0; i < 512; i++)
        sd_buffer[i] = i;
    sd_write_block(sd_buffer, 7, 1);
    kernel_puts("sdwi\n", 0xfff, 0);
    return 0;
}

static int cmd_sdr(int argc, char** argv)
{
    sd_read_block(sd_buffer, 7, 1);
    for (i = 0; i < 512; i++) {
        kernel_printf("%d ", sd_buffer[i]);
    }
    kernel_putchar('\n', 0xfff, 0);
}

static int cmd_sdwz(int argc, char** argv)
{
    for (i = 0; i < 512; i++) {
        sd_buffer[i] = 0;
    }
    sd_write_block(sd_buffer, 7, 1);
    kernel_puts("sdwz\n", 0xfff, 0);
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

static int cmd_mount(int argc, char** argv)
{
    char* fstype;
    char* device;
    int i;

    if (argc != 3) {
        kernel_printf("Usage: mount fstype device:\n");
        return EINVAL;
    }

    fstype = argv[1];
    device = argv[2];

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

static int cmd_unmount(int argc, char** argv)
{
    char* device;

    if (argc != 2) {
        kernel_printf("Usage: unmount device:\n");
        return EINVAL;
    }

    device = argv[1];

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
static int cmd_bootfs(int argc, char** argv)
{
    char* device;

    if (argc != 2) {
        kernel_printf("Usage: bootfs device\n");
        return EINVAL;
    }

    device = argv[1];

    // Allow (but do not require) colon after device name.
    if (device[kernel_strlen(device) - 1] == ':') {
        device[kernel_strlen(device) - 1] = 0;
    }

    return vfs_setbootfs(device);
}

static int cmd_ls(int argc, char** argv)
{
    return ls(argv[1]);
}

static cmd_mkdir(int argc, char** argv)
{
    return vfs_mkdir(argv[1], 0);
}

static cmd_create(int argc, char** argv)
{
    return fs_create(argv[1]);
}

static cmd_remove(int argc, char** argv)
{
    return vfs_remove(argv[1]);
}

static cmd_cat(int argc, char** argv)
{
    return fs_cat(argv[1]);
}

/*
 * Command table. 
 */
static struct {
    const char* name;
    int (*func)(int nargs, char** args);
} cmdtable[] = {
/* menus */
#if 0
    { "?", cmd_mainmenu },
    { "h", cmd_mainmenu },
    { "help", cmd_mainmenu },
    { "?o", cmd_opsmenu },
    { "?t", cmd_testmenu },
#endif
    /* operations */
    { "clear", cmd_clear },
    { "echo", cmd_echo },
    { "time", cmd_time },
#if 0
    { "cd", cmd_chdir },
    { "pwd", cmd_pwd },
#endif
    /* hardware */
    { "syscall4", cmd_syscall4 },
    { "sdwi", cmd_sdwi },
    { "sdr", cmd_sdr },
    { "sdwz", cmd_sdwz },
    /* file system */
    { "mount", cmd_mount },
    { "unmount", cmd_unmount },
    { "bootfs", cmd_bootfs },
    { "ls", cmd_ls },
    { "mkdir", cmd_mkdir },
    { "create", cmd_create },
    { "rm", cmd_remove },
    { "cat", cmd_cat },
    { NULL, NULL }
};

/*
 * Process a single command.
 */
static int cmd_dispatch(char* cmd)
{
    char* args[MAXMENUARGS];
    int nargs = 0;
    char* word;
    char* context;
    int i, result;

    for (word = strtok_r(cmd, " \t", &context);
         word != NULL;
         word = strtok_r(NULL, " \t", &context)) {

        if (nargs >= MAXMENUARGS) {
            kernel_printf("Command line has too many words\n");
            return E2BIG;
        }
        args[nargs++] = word;
    }

    if (nargs == 0) {
        return 0;
    }

    for (i = 0; cmdtable[i].name; i++) {
        if (*cmdtable[i].name && !kernel_strcmp(args[0], cmdtable[i].name)) {
            assert(cmdtable[i].func != NULL, "this command has not been implemented.");

            result = cmdtable[i].func(nargs, args);
            return result;
        }
    }

    kernel_printf("%s: Command not found\n", args[0]);
    return EINVAL;
}

/*
 * Evaluate a command line that may contain multiple semicolon-delimited
 * commands.
 */
static void menu_execute()
{
    char* command;
    char* context;
    int result;

    for (command = strtok_r(buf, ";", &context);
         command != NULL;
         command = strtok_r(NULL, ";", &context)) {

        result = cmd_dispatch(command);
        if (result) {
            kernel_printf("Menu command failed: %s\n", strerror(result));
        }
    }
}

void menu()
{
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
                menu_execute();
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
