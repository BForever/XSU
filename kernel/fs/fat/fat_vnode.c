#include <kern/errno.h>
#include <kern/fcntl.h>
#include <xsu/fs/fat.h>
#include <xsu/utils.h>
#ifdef VFS_DEBUG
#include <driver/vga.h>
#endif

/*
 * vnode operations. 
 */

/*
 * This is called on *each* open().
 */
static int fat_open(struct vnode* v, int openflags)
{
    /*
	 * At this level we do not need to handle O_CREAT, O_EXCL, or O_TRUNC.
	 * We *would* need to handle O_APPEND, but we don't support it.
	 *
	 * Any of O_RDONLY, O_WRONLY, and O_RDWR are valid, so we don't need
	 * to check that either.
	 */

    if (openflags & O_APPEND) {
        return EUNIMP;
    }

    (void)v;

    return 0;
}

/*
 * This is called on *each* open() of a directory.
 * Directories may only be open for read.
 */
static int fat_opendir(struct vnode* v, int openflags)
{
    switch (openflags & O_ACCMODE) {
    case O_RDONLY:
        break;
    case O_WRONLY:
    case O_RDWR:
    default:
        return EISDIR;
    }
    if (openflags & O_APPEND) {
        return EISDIR;
    }

    (void)v;
    return 0;
}

/*
 * Called on the *last* close().
 *
 * This function should attempt to avoid returning errors, as handling
 * them usefully is often not possible.
 */
static int fat_close(struct vnode* v)
{
    // Sync it.
    return VOP_FSYNC(v);
}

/*
 * Called when the vnode refcount (in-memory usage count) hits zero.
 *
 * This function should try to avoid returning errors other than EBUSY.
 */
static int fat_reclaim(struct vnode* v)
{
    (void)v;
    return 0;
}

/*
 * Called for read().
 */
static int fat_read(struct vnode* v, unsigned char* buf, unsigned long count)
{
    struct FILE* fp = v->vn_data;
    int result;

    result = fs_read(fp, buf, count);

    return result;
}

/*
 * Called for write().
 */
static int fat_write(struct vnode* v, const unsigned char* buf, unsigned long count)
{
    struct FILE* fp = v->vn_data;
    int result;

    result = fs_write(fp, buf, count);

    return result;
}

/*
 * Called for ioctl()
 */
static int fat_ioctl(struct vnode* v, int op, userptr_t data)
{
    /*
	 * No ioctls.
	 */

    (void)v;
    (void)op;
    (void)data;

    return EINVAL;
}

static int fat_stat(struct vnode* v, struct stat* statbuf)
{
    (void)v;
    (void)statbuf;
    return 0;
}

/*
 * Return the type of the file (types as per kern/stat.h)
 */
static int fat_gettype_file(struct vnode* v, uint32_t* ret)
{
    (void)v;
    *ret = S_IFREG;
    return 0;
}

/*
 * Check for legal seeks on files. Allow anything non-negative.
 * We could conceivably, here, prohibit seeking past the maximum
 * file size our inode structure can support, but we don't - few
 * people ever bother to check lseek() for failure and having 
 * read() or write() fail is sufficient.
 */
static int fat_tryseek(struct vnode* v, off_t pos)
{
    if (pos < 0) {
        return EINVAL;
    }

    // Allow anything else.
    (void)v;

    return 0;
}

/*
 * Called for fsync(), and also on filesystem unmount, global sync(),
 * and some other cases.
 */
static int fat_fsync(struct vnode* v)
{
    (void)v;
    int result;

    result = fs_fflush();

    return result;
}

/*
 * Called for mmap().
 */
static int sfs_mmap(struct vnode* v /* add stuff as needed */)
{
    (void)v;
    return EUNIMP;
}

/*
 * Called for ftruncate() and from sfs_reclaim.
 */
static int fat_truncate(struct vnode* v, off_t len)
{
    (void)v;
    return 0;
}

static int fat_namefile(struct vnode* vv, struct uio* uio)
{
    (void)vv;
    (void)uio;
    return 0;
}

// Create a subdirectory.
static int fat_mkdir(struct vnode* vn, const char* name, mode_t mode)
{

    (void)mode;
    (void)vn;
    int result;

    result = fs_mkdir(name);
#ifdef VFS_DEBUG
    kernel_printf("FAT_MKDIR: created directory.\n");
#endif

    return 0;
}

static int fat_rmdir(struct vnode* vn, const char* name)
{
    (void)vn;
    return 0;
}

/*
 * Create a file. If EXCL is set, insist that the filename not already
 * exist; otherwise, if it already exists, just open it.
 */
static int fat_creat(struct vnode* v, const char* name, bool excl, mode_t mode, struct vnode** ret)
{
    (void)v;
    int result;

    result = fs_create(name);

    return result;
}

/*
 * Make a hard link to a file.
 * The VFS layer should prevent this being called unless both
 * vnodes are ours.
 */
static int fat_link(struct vnode* dir, const char* name, struct vnode* file)
{
    (void)dir;

    return 0;
}

/*
 * Delete a file.
 */
static int fat_remove(struct vnode* dir, const char* name)
{
    (void)dir;
    int result;

    result = fs_rm(name);

    return result;
}

/*
 * Rename a file.
 *
 * Since we don't support subdirectories, assumes that the two
 * directories passed are the same.
 */
static int fat_rename(struct vnode* d1, const char* n1, struct vnode* d2, const char* n2)
{
    (void)d1;
    (void)d2;
    int result;

    result = fs_mv(n1, n2);

    return result;
}

/*
 * lookparent returns the last path component as a string and the
 * directory it's in as a vnode.
 *
 * Since we don't support subdirectories, this is very easy - 
 * return the root dir and copy the path.
 */

int fat_parsepath(char* path, char* subpath)
{
    int i = 1;
    while (1) {
        if (path[i] == '/' || path[i] == '\0')
            return i;
        subpath[i] = path[i];
        i++;
    }
}

/*
 * Lookup gets a vnode for a pathname.
 *
 * Since we don't support subdirectories, it's easy - just look up the
 * name.
 */
static int fat_lookup(struct vnode* v, char* path, struct vnode** ret)
{
    (void)v;
    return 0;
}

static int fat_lookparent(struct vnode* v, char* path, struct vnode** ret, char* buf, size_t buflen)
{
    (void)v;
    (void)buf;
    return 0;
}

static int fat_notdir(void)
{
    return ENOTDIR;
}

static int fat_isdir(void)
{
    return EISDIR;
}

static int fat_unimp(void)
{
    return EUNIMP;
}

/*
 * Casting through void * prevents warnings.
 * All of the vnode ops return int, and it's ok to cast functions that
 * take args to functions that take no args.
 */

#define ISDIR ((void*)fat_isdir)
#define NOTDIR ((void*)fat_notdir)
#define UNIMP ((void*)fat_unimp)

/*
 * Function table for sfs files.
 */
static const struct vnode_ops fat_fileops = {
    VOP_MAGIC, /* mark this a valid vnode ops table */

    fat_open,
    fat_close,
    fat_reclaim,

    fat_read,
    NOTDIR, /* readlink */
    NOTDIR, /* getdirentry */
    fat_write,
    fat_ioctl,
    fat_stat,
    fat_gettype,
    fat_tryseek,
    fat_fsync,
    fat_mmap,
    fat_truncate,
    NOTDIR, /* namefile */

    NOTDIR, /* creat */
    NOTDIR, /* symlink */
    NOTDIR, /* mkdir */
    NOTDIR, /* link */
    NOTDIR, /* remove */
    NOTDIR, /* rmdir */
    NOTDIR, /* rename */

    NOTDIR, /* lookup */
    NOTDIR, /* lookparent */
};

/*
 * Function table for the sfs directory.
 */
static const struct vnode_ops fat_dirops = {
    VOP_MAGIC, /* mark this a valid vnode ops table */

    fat_opendir,
    fat_close,
    fat_reclaim,

    ISDIR, /* read */
    ISDIR, /* readlink */
    UNIMP,
    ISDIR, /* write */
    fat_ioctl,
    fat_stat,
    fat_gettype,
    UNIMP, /* tryseek */
    fat_fsync,
    ISDIR, /* mmap */
    ISDIR, /* truncate */
    fat_namefile,
    fat_creat,
    UNIMP, /* symlink */
    fat_mkdir, /* mkdir */
    fat_link,
    fat_remove,
    fat_rmdir, /* rmdir */
    fat_rename,
    fat_lookup,
    fat_lookparent,
};

/*
 * Get vnode for the root of the filesystem.
 */
struct vnode* fat_getroot(struct fs* fs)
{
    struct fat_fs* fat = fs->fs_data;
    struct vnode* vn;
    int result;

    result = VOP_INIT(vn, &fat_dirops, fs, NULL);
    if (result) {
        log(LOG_FAIL, "fat: getroot: Cannot load root vnode\n");
    }

    return vn;
}