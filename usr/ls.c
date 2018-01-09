#include <driver/vga.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/slab.h>

void handle_path(char* path, char* newpath)
{
    struct vnode* vn;
    char* name;
    int result;

    name = kmalloc(kernel_strlen(path) + 1);
    result = vfs_lookparent(path, &vn, name, sizeof(name));
    if (result) {
        return;
    }
    kernel_memcpy(newpath, name, kernel_strlen(name) + 1);
    kfree(name);
}

int ls(char* path, char* options)
{
    char* newpath;
    newpath = kmalloc(kernel_strlen(path) + 1);
    handle_path(path, newpath);
    assert(kernel_strlen(newpath) != 0, "path handle error.");
#ifdef VFS_DEBUG
    kernel_printf("ls path: %s\n", newpath);
#endif
    FS_FAT_DIR dir;
    struct dir_entry_attr entry;
    char name[32];
    unsigned int r;

    if (fs_open_dir(&dir, newpath)) {
        kernel_printf("open dir(%s) failed : No such directory!\n", path);
        return 1;
    }

readdir:
    r = fs_read_dir(&dir, (unsigned char*)&entry);
    if (1 != r) {
        if (-1 == r) {
            kernel_printf("\n");
        } else {
            get_filename((unsigned char*)&entry, name);
            if (!kernel_strcmp(options, "-a")) {
                // Include directory entries whose names begin with a dot (.).
                if (entry.attr == 0x10) // sub dir
                    kernel_puts(name, VGA_GREEN, VGA_BLACK);
                else
                    kernel_printf("%s", name);
                kernel_printf("\t");
            } else if (!kernel_strcmp(options, "-l")) {
                // List in long format.
                // 4 * 1024 * 1024 * 1024 = 4264967396B = 4GB
            } else if (!kernel_strcmp(options, "-al")) {
            } else {
                if (name[0] != '.') {
                    if (entry.attr == 0x10) // sub dir
                        kernel_puts(name, VGA_GREEN, VGA_BLACK);
                    else
                        kernel_printf("%s", name);
                    kernel_printf("\t");
                }
            }
            goto readdir;
        }
    } else {
        kfree(newpath);
        return 1;
    }
    kfree(newpath);
    return 0;
}
