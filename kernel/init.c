#include "../usr/ps.h"
#include <arch.h>
#include <driver/ps2.h>
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <page.h>
#include <version.h>
#include <xsu/bootmm.h>
#include <xsu/buddy.h>
#include <xsu/current.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/log.h>
#include <xsu/pc.h>
#include <xsu/slab.h>
#include <xsu/syscall.h>
#include <xsu/time.h>

static const char copyright[] = "Copyright (c) 2017, 2018\n   Spicy Chicken of Zhejiang University. All rights reserved.\n";
static const int buildversion = 1;
static const char buildconfig[] = "MIPS32";

struct vnode startup = {
    .vn_refcount = 0, /* Reference count */
    .vn_opencount = 0,
    .vn_fs = 0, /* Filesystem vnode belongs to */
    .vn_data = 0, /* Filesystem-specific data */
    .vn_ops = 0, /* Functions on this vnode */
    .vn_rwlock = 0,
    .vn_createlock = 0,
    .vn_countlock = 0
};

struct current curpath = {
    .t_cwd = &startup
};

void machine_info()
{
    int row;
    int col;
    kernel_printf("\n%s base system version %s\n", OS_NAME, BASE_VERSION);
    kernel_printf("%s\n", copyright);
    row = cursor_row;
    col = cursor_col;
    cursor_row = 29;
    kernel_printf("system version %s (%s #%d)", GROUP_VERSION, buildconfig, buildversion);
    cursor_row = row;
    cursor_col = col;
    kernel_set_cursor();
}

void init_kernel()
{
    kernel_clear_screen(31);
    // Exception
    init_exception();
    // Page table
    init_pgtable();
    // Drivers
    init_vga();
    init_ps2();
    init_time();
    // Memory management
    log(LOG_START, "Memory Modules.");
    init_bootmm();
    log(LOG_OK, "Bootmem.");
    init_buddy();
    log(LOG_OK, "Buddy.");
    init_slab();
    log(LOG_OK, "Slab.");
    log(LOG_END, "Memory Modules.");
    // Virtual file system
    log(LOG_START, "Virtual File System.");
    vfs_bootstrap();
    log(LOG_OK, "VFS startup.");
    // Default bootfs -- but ignore failure, in case emu0 doesn't exist.
    vfs_setbootfs("emu0");
    log(LOG_OK, "Boot file system created.");
    log(LOG_END, "Virtual File System.");
    // // File system
    // log(LOG_START, "File System.");
    // init_fs();
    // log(LOG_END, "File System.");
    // System call
    log(LOG_START, "System Calls.");
    init_syscall();
    log(LOG_END, "System Calls.");
    // Process control
    log(LOG_START, "Process Control Module.");
    init_pc();
    log(LOG_END, "Process Control Module.");
    // Interrupts
    log(LOG_START, "Enable Interrupts.");
    init_interrupts();
    log(LOG_END, "Enable Interrupts.");
    // Init finished
    machine_info();
    *GPIO_SEG = 0x11223344;
    // Enter shell
    ps();
}
