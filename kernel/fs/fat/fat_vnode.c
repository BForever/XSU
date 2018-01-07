#include <kern/errno.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/fcntl.h>
#include <xsu/log.h>
#include <xsu/slab.h>
#include <xsu/stat.h>
#include <xsu/utils.h>

#ifdef VFS_DEBUG
#include <driver/vga.h>
#endif

/*
 * vnode operations. 
 */

/*
 * Look for a name in a directory and hand back a vnode for the
 * file, if there is one.
 */
static int fat_lookonce(struct vnode* v, const char* name, struct vnode** ret, int* slot)
{
    (void)v;
    return 0;
}

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
    FILE* fp = v->vn_data;
    int result;

    result = fs_read(fp, buf, count);

    return result;
}

/*
 * Called for write().
 */
static int fat_write(struct vnode* v, const unsigned char* buf, unsigned long count)
{
    FILE* fp = v->vn_data;
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

static int fat_gettype_dir(struct vnode* v, uint32_t* ret)
{
    (void)v;
    *ret = S_IFDIR;
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
static int fat_mmap(struct vnode* v /* add stuff as needed */)
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

    char* root = "/";
    kernel_strcat(root, name);
    unsigned char* filename = kernel_strdup(root);
#ifdef VFS_DEBUG
    kernel_printf("name: %s\n", name);
    kernel_printf("root appended: %s\n", root);
    kernel_printf("duplicated file name: %s\n", filename);
#endif
    result = fs_mkdir(filename);
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

    char* root = "/";
    kernel_strcat(root, name);
    unsigned char* filename = kernel_strdup(root);
#ifdef VFS_DEBUG
    kernel_printf("name: %s\n", name);
    kernel_printf("root appended: %s\n", root);
    kernel_printf("duplicated file name: %s\n", filename);
#endif
    result = fs_create(filename);

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
    char* root = "/";
    kernel_strcat(root, name);
    unsigned char* filename = kernel_strdup(root);
#ifdef VFS_DEBUG
    kernel_printf("name: %s\n", name);
    kernel_printf("root appended: %s\n", root);
    kernel_printf("duplicated file name: %s\n", filename);
#endif
    result = fs_rm(filename);
#ifdef VFS_DEBUG
    kernel_printf("\nremove complete.\n");
#endif

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

    char* root1 = "/";
    char* root2 = "/";
    kernel_strcat(root1, n1);
    kernel_strcat(root2, n2);
    unsigned char* src = kernel_strdup(root1);
    unsigned char* dst = kernel_strdup(root2);
#ifdef VFS_DEBUG
    kernel_printf("name 1: %s\n", n1);
    kernel_printf("name 2: %s\n", n2);
    kernel_printf("root1 appended: %s\n", root1);
    kernel_printf("root2 appended: %s\n", root2);
    kernel_printf("src file name: %s\n", src);
    kernel_printf("dst file name: %s\n", dst);
#endif
    result = fs_mv(src, dst);

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
    struct vnode* final;
    int result, root_fl = 0;
    char subpath[20];
    char buf_path[256];
    char* temp_path = buf_path;

    // To suport subdirectories we need to look up every subdirectory in a path.
    kernel_strcpy(temp_path, path);
    char* check = kernel_strchr(path, '/');
    if (check == NULL) {
        result = fat_lookonce(v, path, &final, NULL);
        if (result) {
            return result;
        }
        VOP_INCREF(v);
        *ret = final;
        return 0;
    }
    do {
        int pos = fat_parsepath(temp_path, subpath);
        temp_path = temp_path + pos;
#ifdef VFS_DEBUG
        kernel_printf("\npath: %s\n", path);
        kernel_printf("\nsubpath: %s\n", subpath);
#endif
        root_fl = 0;
        result = fat_lookonce(v, subpath, &final, NULL);
        if (result) {
            return result;
        }
        VOP_DECREF(v);
        v = final;
        assert(final != NULL, "encounter a null vnode.");
        char* test = kernel_strchr(temp_path, '/');
        if (test == NULL) {
            break;
        }
    } while (temp_path != '\0');

    *ret = final;
    return 0;
}

static int fat_lookparent(struct vnode* v, char* path, struct vnode** ret, char* buf, size_t buflen)
{
#ifdef VFS_DEBUG
    kernel_printf("path: %s\n", path);
#endif

    if (kernel_strlen(path) + 1 > buflen) {
        return ENAMETOOLONG;
    }

    char* check = kernel_strchr(path, '/');
    if (check == NULL) {
        VOP_INCREF(v);
        *ret = v;
        kernel_strcpy(buf, path);
        return 0;
    }

    *check = '\0';
    check++;
    kernel_strcpy(buf, check);
    return fat_lookup(v, path, ret);
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
    fat_gettype_file,
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
    fat_gettype_dir,
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

    vn = kmalloc(sizeof(struct vnode));
    result = VOP_INIT(vn, &fat_dirops, fs, NULL);
    if (result) {
        log(LOG_FAIL, "fat: getroot: Cannot load root vnode\n");
    }

    return vn;
}

struct vnode* fat_getroot_file(struct fs* fs)
{
    struct fat_fs* fat = fs->fs_data;
    struct vnode* vn;
    int result;

    vn = kmalloc(sizeof(struct vnode));
    result = VOP_INIT(vn, &fat_fileops, fs, NULL);
    if (result) {
        log(LOG_FAIL, "fat: getroot_file: Cannot load root vnode\n");
    }

    return vn;
}