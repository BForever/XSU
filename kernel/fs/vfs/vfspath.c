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

    char* name;
    FILE* file;

    name = kmalloc(kernel_strlen(path) + 1);
    result = vfs_lookparent(path, &vn, name, sizeof(name));
    if (result) {
        return result;
    }

    result = fs_open(file, name);
    if (result) {
        return result;
    }

    result = vfs_getroot("sd", &vn, true);
    if (result) {
        return result;
    }

    vn->vn_data = file;
    VOP_INCREF(vn);
    *ret = vn;
    kfree(name);

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
    VOP_DECREF(vn);
}

/* Does most of the work for remove(). */
int vfs_remove(char* path)
{
    struct vnode* dir;
    char* name;
    int result;

    name = kmalloc(kernel_strlen(path) + 1);
    result = vfs_lookparent(path, &dir, name, sizeof(name));
#ifdef VFS_DEBUG
    kernel_printf("result: %d\n", result);
    kernel_printf("path: %s\n", path);
    kernel_printf("name: %s\n", name);
#endif
    if (result) {
        return result;
    }

    result = vfs_getroot("sd", &dir, false);
    if (result) {
        return result;
    }

    result = VOP_REMOVE(dir, name);
    VOP_DECREF(dir);
    kfree(name);

    return result;
}

/* Does most of the work for rename(). */
int vfs_rename(char* oldpath, char* newpath)
{
    struct vnode* olddir;
    char* oldname;
    struct vnode* newdir;
    char* newname;
    int result;

    oldname = kmalloc(kernel_strlen(oldpath) + 1);
    newname = kmalloc(kernel_strlen(newpath) + 1);
    result = vfs_lookparent(oldpath, &olddir, oldname, sizeof(oldname));
    if (result) {
        return result;
    }
    result = vfs_lookparent(newpath, &newdir, newname, sizeof(newname));
    if (result) {
        VOP_DECREF(olddir);
        return result;
    }

    result = vfs_getroot("sd", &olddir, false);
    if (result) {
        return result;
    }
    result = vfs_getroot("sd", &newdir, false);
    if (result) {
        return result;
    }

    if (olddir->vn_fs == NULL || newdir->vn_fs == NULL || olddir->vn_fs != newdir->vn_fs) {
        VOP_DECREF(newdir);
        VOP_DECREF(olddir);
        return EXDEV;
    }

    result = VOP_RENAME(olddir, oldname, newdir, newname);

    VOP_DECREF(newdir);
    VOP_DECREF(olddir);
    kfree(oldname);
    kfree(newname);

    return result;
}

int vfs_cp(char* oldpath, char* newpath)
{
    struct vnode* olddir;
    struct vnode* newdir;
    char* oldname;
    char* newname;
    int result;

    oldname = kmalloc(kernel_strlen(oldpath) + 1);
    newname = kmalloc(kernel_strlen(newpath) + 1);
    result = vfs_lookparent(oldpath, &olddir, oldname, sizeof(oldname));
    if (result) {
        return result;
    }
    result = vfs_lookparent(newpath, &newdir, newname, sizeof(newname));
    if (result) {
        return result;
    }

    result = fs_cp(oldname, newname);
    if (result) {
        return result;
    }

    kfree(oldname);
    kfree(newname);
    return result;
}

int vfs_create(char* path)
{
    struct vnode* vn;
    char* name;
    int result;

    name = kmalloc(kernel_strlen(path) + 1);
    result = vfs_lookparent(path, &vn, name, sizeof(name));
    if (result) {
        return result;
    }

    result = fs_create(name);
    if (result) {
        return result;
    }
    kfree(name);
    return result;
}

int vfs_cat(char* path)
{
    struct vnode* vn;
    char* name;
    int result;

    name = kmalloc(kernel_strlen(path) + 1);
    result = vfs_lookparent(path, &vn, name, sizeof(name));
    if (result) {
        return result;
    }

    result = fs_cat(name);
    if (result) {
        return result;
    }
    kfree(name);
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
    char* name;
    int result;

    name = kmalloc(kernel_strlen(path) + 1);
    result = vfs_lookparent(path, &parent, name, sizeof(name));
    if (result) {
        return result;
    }

    result = vfs_getroot("sd", &parent, false);
    if (result) {
        return result;
    }

    result = VOP_MKDIR(parent, name, mode);

    VOP_DECREF(parent);
    kfree(name);
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
