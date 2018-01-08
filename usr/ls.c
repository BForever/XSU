#include <driver/vga.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/slab.h>

char* cut_front_blank(char* str)
{
    char* s = str;
    unsigned int index = 0;

    while (*s == ' ') {
        ++s;
        ++index;
    }

    if (!index)
        return str;

    while (*s) {
        *(s - index) = *s;
        ++s;
    }

    --s;
    *s = 0;

    return str;
}

unsigned int each_param(char* para, char* word, unsigned int off, char ch)
{
    int index = 0;

    while (para[off] && para[off] != ch) {
        word[index] = para[off];
        ++index;
        ++off;
    }

    word[index] = 0;

    return off;
}

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

int ls(char* para)
{
    char* newpath;
    newpath = kmalloc(kernel_strlen(para) + 1);
    handle_path(para, newpath);
    assert(kernel_strlen(newpath) != 0, "path handle error.");
#ifdef VFS_DEBUG
    kernel_printf("ls path: %s\n", newpath);
#endif
    char pwd[128];
    struct dir_entry_attr entry;
    char name[32];
    char* p = newpath;
    unsigned int next;
    unsigned int r;
    unsigned int p_len;
    FS_FAT_DIR dir;

    p = cut_front_blank(p);
    p_len = kernel_strlen(p);
    next = each_param(p, pwd, 0, ' ');

    if (fs_open_dir(&dir, pwd)) {
        kernel_printf("open dir(%s) failed : No such directory!\n", pwd);
        return 1;
    }

readdir:
    r = fs_read_dir(&dir, (unsigned char*)&entry);
    if (1 != r) {
        if (-1 == r) {
            kernel_printf("\n");
        } else {
            get_filename((unsigned char*)&entry, name);
            if (entry.attr == 0x10) // sub dir
                kernel_puts(name, VGA_GREEN, VGA_BLACK);
            else
                kernel_printf("%s", name);
            kernel_printf("\t");
            goto readdir;
        }
    } else {
        kfree(newpath);
        return 1;
    }
    kfree(newpath);
    return 0;
}
