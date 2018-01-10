/*
 * High-level VFS operations on pathnames.
 */

#include <assert.h>
#include <driver/vga.h>
#include <kern/errno.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/fcntl.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/slab.h>
#include <xsu/time.h>

#define NAME_MAX 255

/* Does most of the work for open(). */
int vfs_open(char* path, int openflags, mode_t mode, struct vnode** ret)
{
    int how;
    int result;
    int canwrite;
    struct vnode* vn = NULL;

    how = openflags & O_ACCMODE;

    switch (how) {
    case O_RDONLY:
        canwrite = 0;
        break;
    case O_WRONLY:
    case O_RDWR:
        canwrite = 1;
        break;
    default:
        return EINVAL;
    }

    char name[256];
    FILE* file;

    kernel_memcpy(name, path + 3, kernel_strlen(path) - 2);

    result = fs_open(file, name);
    if (result) {
        return result;
    }

    result = vfs_getroot("sd", &vn, true);
    if (result) {
        return result;
    }

    vn->vn_data = file;
    *ret = vn;

    return result;
}

/* Does most of the work for close(). */
void vfs_close(struct vnode* vn)
{
    /*
	 * VOP_DECOPEN and VOP_DECREF don't return errors.
	 *
	 * We assume that the file system makes every reasonable
	 * effort to not fail. If it does fail - such as on a hard I/O
	 * error or something - vnode.c prints a warning. The reason
	 * we don't report errors up to or above this level is that
	 *    (1) most application software does not check for close
	 *        failing, and more importantly
	 *    (2) we're often called from places like process exit
	 *        where reporting the error is impossible and
	 *        meaningful recovery is entirely impractical.
	 */

    FILE* file = vn->vn_data;
    fs_close(file);
    vn->vn_data = 0;
}

/* Does most of the work for remove(). */
int vfs_remove(char* path)
{
    struct vnode* dir;
    char name[256];
    int result;

    kernel_memcpy(name, path + 3, kernel_strlen(path) - 2);

    result = vfs_getroot("sd", &dir, false);
    if (result) {
        return result;
    }

    result = VOP_REMOVE(dir, name);

    return result;
}

/* Does most of the work for rename(). */
int vfs_rename(char* oldpath, char* newpath)
{
    struct vnode* olddir;
    char oldname[256];
    struct vnode* newdir;
    char newname[256];
    int result;

    kernel_memcpy(oldname, oldpath + 3, kernel_strlen(oldpath) - 2);
    kernel_memcpy(newname, newpath + 3, kernel_strlen(newpath) - 2);

    result = vfs_getroot("sd", &olddir, false);
    if (result) {
        return result;
    }
    result = vfs_getroot("sd", &newdir, false);
    if (result) {
        return result;
    }

    if (olddir->vn_fs == NULL || newdir->vn_fs == NULL || olddir->vn_fs != newdir->vn_fs) {
        return EXDEV;
    }

    result = VOP_RENAME(olddir, oldname, newdir, newname);

    return result;
}

int vfs_cp(char* oldpath, char* newpath)
{
    struct vnode* olddir;
    struct vnode* newdir;
    char oldname[256];
    char newname[256];
    int result;

    kernel_memcpy(oldname, oldpath + 3, kernel_strlen(oldpath) - 2);
    kernel_memcpy(newname, newpath + 3, kernel_strlen(newpath) - 2);

    result = fs_cp(oldname, newname);

    return result;
}

int vfs_create(char* path)
{
    struct vnode* vn;
    char name[256];
    int result;

    kernel_memcpy(name, path + 3, kernel_strlen(path) - 2);

    result = fs_create(name);

    return result;
}

int vfs_touch(char* path)
{
    struct vnode* vn;
    char name[256];
    int result;
    FILE file;

    kernel_memcpy(name, path + 3, kernel_strlen(path) - 2);

    result = fs_open(&file, name);
    if (result) {
        // The file does not exist, create it.
        result = fs_create(name);
    } else {
        // The file exists, change the modification time.
        char time_buf[10];
        get_time(time_buf, sizeof(time_buf));
        int hours = (time_buf[0] - '0') * 10 + (time_buf[1] - '0');
        int minutes = (time_buf[3] - '0') * 10 + (time_buf[4] - '0');
        int seconds = 0;
        uint16_t time = (hours << 11) | (minutes << 5) | seconds;
        file.entry.attr.time = time;
        // Close file.
        result = fs_close(&file);
    }

    return result;
}

int vfs_cat(char* path)
{
    struct vnode* vn;
    char name[256];
    int result;

    kernel_memcpy(name, path + 3, kernel_strlen(path) - 2);
    result = fs_cat(name);

    return result;
}

/* Does most of the work for link(). */
int vfs_link(char* oldpath, char* newpath)
{
    return 0;
}

/*
 * Does most of the work for symlink().
 *
 * Note, however, if you're implementing symlinks, that various
 * other parts of the VFS layer are missing crucial elements of
 * support for symlinks.
 */
int vfs_symlink(const char* contents, char* path)
{
    return 0;
}

/*
 * Does most of the work for readlink().
 *
 * Note, however, if you're implementing symlinks, that various
 * other parts of the VFS layer are missing crucial elements of
 * support for symlinks.
 */
int vfs_readlink(char* path, struct uio* uio)
{
    return 0;
}

/*
 * Does most of the work for mkdir.
 */
int vfs_mkdir(char* path, mode_t mode)
{
    struct vnode* parent;
    char name[256];
    int result;

    kernel_memcpy(name, path + 3, kernel_strlen(path) - 2);

    result = vfs_getroot("sd", &parent, false);
    if (result) {
        return result;
    }

    result = VOP_MKDIR(parent, name, mode);

    kernel_printf("VFS_MKDIR: completed execting vfs_mkdir \n");
    return result;
}

/*
 * Does most of the work for rmdir.
 */
int vfs_rmdir(char* path)
{
    return 0;
}
