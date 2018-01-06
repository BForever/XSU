#include <kern/errno.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/slab.h>


/*
 * Sync routine. This is what gets invoked if you do FS_SYNC on the
 * sfs filesystem structure.
 */
static int fat_sync(struct fs* fs)
{
    struct fat_fs* fat;
    unsigned i, num;
    int result;

    fat = fs->fs_data;

    // Go over the array of loaded vnodes, syncing as we go.
    num = array_num(fat->fat_vnodes);
    for (i = 0; i < num; i++) {
        struct vnode* v = array_get(fat->fat_vnodes, i);
        VOP_FSYNC(v);
    }

    return 0;
}

/*
 * Routine to retrieve the volume name. Filesystems can be referred
 * to by their volume name followed by a colon as well as the name
 * of the device they're mounted on.
 */
static const char* fat_getvolname(struct fs* fs)
{
    struct fat_fs* fat = fs->fs_data;
    const char* ret = "sd";

    return ret;
}

/*
 * Unmount code.
 *
 * VFS calls FS_SYNC on the filesystem prior to unmounting it.
 */
static int fat_unmount(struct fs* fs)
{
    struct fat_fs* fat = fs->fs_data;

    // Do we have any files open? If so, can't unmount.
    if (array_num(fat->fat_vnodes) > 0) {
        return EBUSY;
    }

    // Once we start nuking stuff we can't fail.
    array_destroy(fat->fat_vnodes);

    // The vfs layer takes care of the device for us.
    (void)fat->fat_device;

    // Destroy the fs object.
    kfree(fat);

    return 0;
}

/*
 * Mount routine.
 *
 * The way mount works is that you call vfs_mount and pass it a
 * filesystem-specific mount routine. Said routine takes a device and
 * hands back a pointer to an abstract filesystem. You can also pass
 * a void pointer through.
 *
 * This organization makes cleanup on error easier. Hint: it may also
 * be easier to synchronize correctly; it is important not to get two
 * filesystem with the same name mounted at once, or two filesystems
 * mounted on the same device at once.
 */
static int fat_domount(void* options, struct device* dev, struct fs** ret)
{
    int result;
    struct fat_fs* fat;
    int i;

    // We don't pass any options through mount.
    (void)options;

    /*
	 * We can't mount on devices with the wrong sector size.
	 *
	 * (Note: for all intents and purposes here, "sector" and
	 * "block" are interchangeable terms. Technically a filesystem
	 * block may be composed of several hardware sectors, but we
	 * don't do that in sfs.)
	 */
    if (dev->d_blocksize != SECTOR_SIZE) {
        return ENXIO;
    }

    // Allocate object.
    fat = kmalloc(sizeof(struct fat_fs));
    if (fat == NULL) {
        return ENOMEM;
    }

    // Allocate array.
    fat->fat_vnodes = array_create();
    if (fat->fat_vnodes == NULL) {
        kfree(fat);
        return ENOMEM;
    }

    // Set the device so we can use fat_rblock().
    fat->fat_device = dev;

    // Set up abstract fs calls.
    fat->fat_absfs.fs_sync = fat_sync;
    fat->fat_absfs.fs_getvolname = fat_getvolname;
    fat->fat_absfs.fs_getroot = fat_getroot;
    fat->fat_absfs.fs_unmount = fat_unmount;
    fat->fat_absfs.fs_data = fat;

    // Hand back the abstract fs.
    *ret = &fat->fat_absfs;

    return 0;
}

/*
 * Actual function called from high-level code to mount an sfs.
 */

int fat_mount(const char* device)
{
    return vfs_mount(device, NULL, fat_domount);
}