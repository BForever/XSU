#include "menu.h"
#include "ls.h"
#include <assert.h>
#include <driver/ps2.h>
#include <driver/sd.h>
#include <driver/vga.h>
#include <kern/errno.h>
#include <xsu/bootmm.h>
#include <xsu/buddy.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/pc.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/time.h>
#include <xsu/utils.h>

char pwd[256];
char buf[64];
int buf_index;
char sd_buffer[8192];

#define MAXMENUARGS 16

static void abs_to_rel(char* oldpath, char* newpath)
{
    // absolute directory.
    char device[5];
    kernel_memcpy(device, oldpath, 4);
    device[4] = 0;
    if (!kernel_strcmp(device, "sd:/")) {
        kernel_memcpy(newpath, oldpath, kernel_strlen(oldpath) + 1);
#ifdef FS_DEBUG
        kernel_printf("new path: %s\n", newpath);
#endif
        return;
    }

    // relative directory.
    char* tmp = kernel_strdup(pwd);

    if (tmp[kernel_strlen(tmp) - 1] != '/') {
        kernel_strcat(tmp, "/");
    }

    kernel_strcat(tmp, oldpath);
    kernel_memcpy(newpath, tmp, kernel_strlen(tmp) + 1);

#ifdef FS_DEBUG
    kernel_printf("new path: %s\n", newpath);
#endif

    return;
}

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

static int cmd_time(int argc, char** argv)
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
    int i;
    for (i = 0; i < 512; i++)
        sd_buffer[i] = i;
    sd_write_block(sd_buffer, 7, 1);
    kernel_puts("sdwi\n", 0xfff, 0);
    return 0;
}

static int cmd_sdr(int argc, char** argv)
{
    int i;
    sd_read_block(sd_buffer, 7, 1);
    for (i = 0; i < 512; i++) {
        kernel_printf("%d ", sd_buffer[i]);
    }
    kernel_putchar('\n', 0xfff, 0);
    return 0;
}

static int cmd_sdwz(int argc, char** argv)
{
    int i;
    for (i = 0; i < 512; i++) {
        sd_buffer[i] = 0;
    }
    sd_write_block(sd_buffer, 7, 1);
    kernel_puts("sdwz\n", 0xfff, 0);
    return 0;
}

/*
 * Command for memory moudle. 
 */
static int cmd_mminfo(int argc, char** argv)
{
    bootmap_info();
    buddy_info();
    return 0;
}

static int cmd_mmtest(int argc, char** argv)
{
    void* address = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 1KB\n", address);
    kfree(address);
    kernel_printf("kfree succeed\n");
    return 0;
}

static int cmd_slubtest(int argc, char** argv)
{
    unsigned int size_kmem_cache[PAGE_SHIFT] = { 96, 192, 8, 16, 32, 64, 128, 256, 512, 1024 };
    unsigned int i;
    for (i = 0; i < 10; i++) {
        void* address = kmalloc(size_kmem_cache[4]);
        kernel_printf("kmalloc : %x, size = 1KB\n", address);
    }
    return 0;
}

static int cmd_buddytest(int argc, char** argv)
{
    void* address = kmalloc(4096 * 2 * 2 * 2 * 2);
    kernel_printf("kmalloc : %x, size = 1KB\n", address);
    address = kmalloc(4096 * 2 * 2 * 2);
    kernel_printf("kmalloc : %x, size = 1KB\n", address);
    address = kmalloc(4096 * 2 * 2);
    kernel_printf("kmalloc : %x, size = 1KB\n", address);
    address = kmalloc(4096 * 2);
    kernel_printf("kmalloc : %x, size = 1KB\n", address);
    return 0;
}

static int cmd_buddy(int argc, char** argv)
{
    void* address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    address = kmalloc(2 * 4096);
    kernel_printf("kmalloc : %x, size = 8KB\n", address);
    address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    kfree(address);
    kfree(address);
    address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    address = kmalloc(4096);
    kernel_printf("kmalloc : %x, size = 4KB\n", address);
    return 0;
}

static int cmd_slub(int argc, char** argv)
{
    void* address1 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address1);
    void* address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    void* address3 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address3);
    kfree(address2);
    kfree(address1);
    kfree(address2);
    kfree(address1);
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    return 0;
}

static int cmd_slub2(int argc, char** argv)
{
    void* address1 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address1);
    void* address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    void* address3 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address3);
    kfree(address1);
    kfree(address2);
    kfree(address3);
    kernel_printf("free a page\n");
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    address2 = kmalloc(1024);
    kernel_printf("kmalloc : %x, size = 4KB\n", address2);
    return 0;
}

/*
 * Command for mounting a filesystem.
 */
static int cmd_pwd(int argc, char** argv)
{
    kernel_printf("%s\n", pwd);
    return 0;
}

static int cmd_cd(int argc, char** argv)
{
    char dir[64];
    char tmp[256];
    unsigned int index;
    FS_FAT_DIR f_dir;

    // default (return to root directory).
    if (argc == 1) {
        kernel_memset(pwd, 0, kernel_strlen(pwd) + 1);
        kernel_memcpy(pwd, "sd:/", 5);
        return 0;
    }

    // . (stay in current directory).
    if (!kernel_strcmp(argv[1], ".")) {
        return 0;
    }

    // .. (return to parent directory).
    if (!kernel_strcmp(argv[1], "..")) {
        if (!kernel_strcmp(pwd, "sd:/")) {
            return 0;
        } else {
            index = kernel_strlen(pwd) - 1;
            while (pwd[index] != '/') {
                pwd[index--] = 0;
            }
            if (index) {
                pwd[index + 1] = 0;
            }
            return 0;
        }
    }

    // absolute directory.
    char* device;
    device = kmalloc(260);
    kernel_memcpy(device, argv[1], 4);
    device[4] = 0;
#ifdef FS_DEBUG
    kernel_printf("device: %s\n", device);
#endif
    if (!kernel_strcmp(device, "sd:/")) {
        if (argv[1][kernel_strlen(argv[1]) - 1] == '/') {
            argv[1][kernel_strlen(argv[1]) - 1] = 0;
        }
        kernel_memcpy(tmp, argv[1] + 3, kernel_strlen(argv[1]) + 1);
#ifdef FS_DEBUG
        kernel_printf("tmp path: %s\n", tmp);
#endif

    check_dir:
        if (fs_open_dir(&f_dir, tmp)) {
            kernel_printf("No such file or directory: %s\n", tmp);
            kfree(device);
            return 1;
        }
        kernel_memset(device, 0, kernel_strlen(device) + 1);
        kernel_strcat(device, "sd:");
        kernel_strcat(device, tmp);
        kernel_memcpy(pwd, device, kernel_strlen(device) + 1);
        kfree(device);
        return 0;
    }

    // relative directory.
    kernel_memset(tmp, 0, kernel_strlen(tmp) + 1);
    kernel_memcpy(tmp, pwd + 3, kernel_strlen(pwd) - 2);
    if (tmp[kernel_strlen(tmp) - 1] != '/') {
        kernel_strcat(tmp, "/");
    }
    kernel_strcat(tmp, argv[1]);
#ifdef FS_DEBUG
    kernel_printf("cd directory: %s\n", tmp);
#endif

    goto check_dir;
}

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
    if (argc == 1) {
        char* tmp;
        tmp = kernel_strdup(pwd);
        return ls(tmp);
    }

    char path[256];
    abs_to_rel(argv[1], path);

    return ls(path);
}

static int cmd_mkdir(int argc, char** argv)
{
    if (argc != 2) {
        kernel_printf("Usage: make directories\n");
        return EINVAL;
    }

    char path[256];
    abs_to_rel(argv[1], path);

    return vfs_mkdir(path, 0);
}

static int cmd_create(int argc, char** argv)
{
    if (argc != 2) {
        kernel_printf("Usage: change file access and mpdification times");
        return EINVAL;
    }

    char path[256];
    abs_to_rel(argv[1], path);

    return vfs_create(path);
}

static int cmd_remove(int argc, char** argv)
{
    if (argc != 2) {
        kernel_printf("Usage: remove files\n");
        return EINVAL;
    }

    char path[256];
    abs_to_rel(argv[1], path);

    return vfs_remove(path);
}

static int cmd_move(int argc, char** argv)
{
    if (argc != 3) {
        kernel_printf("Usage: move files\n");
        return EINVAL;
    }

    char path1[256], path2[256];

    abs_to_rel(argv[1], path1);
    abs_to_rel(argv[2], path2);

    return vfs_rename(path1, path2);
}

static int cmd_copy(int argc, char** argv)
{
    if (argc != 3) {
        kernel_printf("Usage: copy files\n");
        return EINVAL;
    }

    char path1[256], path2[256];

    abs_to_rel(argv[1], path1);
    abs_to_rel(argv[2], path2);

    return vfs_cp(path1, path2);
}

static int cmd_cat(int argc, char** argv)
{
    if (argc != 2) {
        kernel_printf("Usage: concatenate and print files\n");
        return EINVAL;
    }

    char path[256];

    abs_to_rel(argv[1], path);

    return vfs_cat(path);
}

static int cmd_pctest_sleep(int argc, char** argv)
{
    pc_create(test_sleep1sandprint, "test_sleep1sandprint");
    return 0;
}

static int cmd_pctest_fork(int argc, char** argv)
{
    pc_create(test_forkandkill, "test_forkandkill");
    return 0;
}

static int cmd_pctest_kill(int argc, char** argv)
{
    pc_create(test_sleep5sandkillasid2, "test_sleep5sandkillasid2");
    return 0;
}
static int cmd_kill(int argc, char** argv)
{
    int i;
    if (argc >= 2) {
        for (i = 1; i < argc; i++) {
            call_syscall_a0(SYSCALL_KILL, argv[i][0] - '0');
        }
    }
}
static int cmd_ps(int argc, char** argv)
{
    call_syscall_a0(SYSCALL_PRINTTASKS, 0);
}
static int cmd_exit(int argc, char** argv)
{
    kernel_printf("exit\n");
    call_syscall_a0(SYSCALL_EXIT, 0);
}
static int cmd_syscall(int argc, char** argv)
{
    if (argc == 2) {
        call_syscall_a0(argv[1][0] - '0', 0);
    } else if (argc == 3) {
        call_syscall_a0(argv[1][0] - '0', argv[2][0] - '0');
    } else {
        kernel_printf("Usage: syscall v0 a0\n");
    }
}

/*
 * Command table. 
 */
static struct {
    const char* name;
    int (*func)(int argc, char** argv);
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
    /* hardware */
    { "syscall4", cmd_syscall4 },
    { "sdwi", cmd_sdwi },
    { "sdr", cmd_sdr },
    { "sdwz", cmd_sdwz },
    /* memory module */
    { "mminfo", cmd_mminfo },
    { "mmtest", cmd_mmtest },
    { "slubtest", cmd_slubtest },
    { "buddytest", cmd_buddytest },
    { "buddy", cmd_buddy },
    { "slub", cmd_slub },
    { "slub2", cmd_slub2 },
    /* file system */
    { "pwd", cmd_pwd },
    { "mount", cmd_mount },
    { "unmount", cmd_unmount },
    { "bootfs", cmd_bootfs },
    { "ls", cmd_ls },
    { "mkdir", cmd_mkdir },
    { "touch", cmd_create },
    { "rm", cmd_remove },
    { "mv", cmd_move },
    { "cp", cmd_copy },
    { "cd", cmd_cd },
    { "cat", cmd_cat },
    /* process control */
    { "ps", cmd_ps },
    { "kill", cmd_kill },
    { "exit", cmd_exit },
    { "syscall", cmd_syscall },
    { "pctest_sleep", cmd_pctest_sleep },
    { "pctest_fork", cmd_pctest_fork },
    { "pctest_kill", cmd_pctest_kill },
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

    for (word = kernel_strtok_r(cmd, " \t", &context);
         word != NULL;
         word = kernel_strtok_r(NULL, " \t", &context)) {

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
static int menu_execute()
{
    char* command;
    char* context;
    int result;

    kernel_putchar('\n', VGA_BLACK, VGA_BLACK);
    for (command = kernel_strtok_r(buf, ";", &context);
         command != NULL;
         command = kernel_strtok_r(NULL, ";", &context)) {

        result = cmd_dispatch(command);
        if (result) {
            kernel_printf("Menu command failed: %s\n", strerror(result));
        }
    }
    return result;
}

void menu()
{
    kernel_printf("Press any key to enter shell.\n");
    kernel_getchar();
    char c;
    buf_index = 0;
    buf[0] = 0;
    kernel_clear_screen(31);
    // Welcome prompt.
    char time_buf[10];
    get_time(time_buf, sizeof(time_buf));
    kernel_printf("Last login: %s on ttys001\n", time_buf);
    // Command prompt.
    kernel_memcpy(pwd, "sd:/", 5);
    kernel_puts("-> ", VGA_GREEN, VGA_BLACK);
    kernel_puts(pwd, VGA_CYAN, VGA_BLACK);
    kernel_puts(" $ ", VGA_YELLOW, VGA_BLACK);
    // Command result.
    int result = 0;

    while (1) {
        c = kernel_getchar();
        if (c == '\n') {
            buf[buf_index] = 0;
            result = menu_execute();
            if (result) {
                kernel_puts("-> ", VGA_RED, VGA_BLACK);
            } else {
                kernel_puts("-> ", VGA_GREEN, VGA_BLACK);
            }
            kernel_puts(pwd, VGA_CYAN, VGA_BLACK);
            kernel_puts(" $ ", VGA_YELLOW, VGA_BLACK);
            buf_index = 0;
            kernel_memset(buf, 0, 64);
        } else if (c == 0x08) {
            if (buf_index) {
                buf_index--;
                kernel_putchar_at(' ', VGA_WHITE, VGA_BLACK, cursor_row, cursor_col - 1);
                cursor_col--;
                kernel_set_cursor();
            }
        } else {
            if (buf_index < 63) {
                buf[buf_index++] = c;
                kernel_putchar(c, VGA_WHITE, VGA_BLACK);
            }
        }
    }
}
