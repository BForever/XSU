/*
 * VFS operations that involve the list of VFS (named) devices
 * (the "dev" in "dev:path" syntax).
 */

#include <driver/vga.h>
#include <kern/errno.h>
#include <xsu/array.h>
#include <xsu/device.h>
#include <xsu/fs/fs.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/log.h>
#include <xsu/slab.h>
#include <xsu/synch.h>
#include <xsu/utils.h>

/*
 * Structure for a single named device.
 * 
 * kd_name    - Name of device (eg, "lhd0"). Should always be set to
 *              a valid string.
 *
 * kd_rawname - Name of raw device (eg, "lhd0raw"). Is non-NULL if and
 *              only if this device can have a filesystem mounted on
 *              it.
 *
 * kd_device  - Device object this name refers to. May be NULL if kd_fs
 *              is hardwired.
 *
 * kd_fs      - Filesystem object mounted on, or associated with, this
 *              device. NULL if there is no filesystem. 
 *
 * A filesystem can be associated with a device without having been
 * mounted if the device was created that way. In this case,
 * kd_rawname is NULL (prohibiting mount/unmount), and, as there is
 * then no way to access kd_device, it will be NULL as well. This is
 * intended for devices that are inherently filesystems, like emu0.
 *
 * Referencing kd_name, or the filesystem volume name, on a device
 * with a filesystem mounted returns the root of the filesystem.
 * Referencing kd_name on a mountable device with no filesystem
 * returns ENXIO. Referencing kd_name on a device that is not
 * mountable and has no filesystem, or kd_rawname on a mountable
 * device, returns the device itself.
 */

struct knowndev {
    char* kd_name;
    char* kd_rawname;
    struct device* kd_device;
    struct vnode* kd_vnode;
    struct fs* kd_fs;
};

static struct array* knowndevs;

/*
 * Setup function. 
 */
void vfs_bootstrap(void)
{
    knowndevs = array_create();
    if (knowndevs == NULL) {
        log(LOG_FAIL, "VFS: Could not create knowndevs array.");
    }

    // This device is used to establish FAT32, which actually is the SD card.
    devnull_create();
#ifdef VFS_DEBUG
    kernel_printf("A null device has been created.\n");
#endif
}

/*
 * Assemble the name for a raw device from the name for the regular device.
 */
static char* mk_raw_name(const char* name)
{
    char* s = kmalloc(kernel_strlen(name) + 3 + 1);
    if (!s) {
        return NULL;
    }
    kernel_strcpy(s, name);
    s = kernel_strcat(s, "raw");
    return s;
}

/*
 * Check if the two strings passed in are the same, if they're both
 * not NULL (the latter part being significant).
 */
static inline int same_string(const char* a, const char* b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    return !kernel_strcmp(a, b);
}

/*
 * Check if the first string passed is the same as any of the three others,
 * if they're not NULL.
 */
static inline int same_string3(const char* a, const char* b, const char* c, const char* d)
{
    return same_string(a, b) || same_string(a, c) || same_string(a, d);
}

/*
 * Check if any of the three names passed in already exists as a device
 * name.
 */

/*
 * Check if any of the three names passed in already exists as a device
 * name.
 */
static int bad_names(const char* n1, const char* n2, const char* n3)
{
    const char* volname;
    unsigned i, num;
    struct knowndev* kd;

    num = array_num(knowndevs);
    for (i = 0; i < num; i++) {
        kd = array_get(knowndevs, i);

        if (kd->kd_fs) {
            volname = FSOP_GETVOLNAME(kd->kd_fs);
            if (same_string3(volname, n1, n2, n3)) {
                return 1;
            }
        }

        if (same_string3(kd->kd_rawname, n1, n2, n3) || same_string3(kd->kd_name, n1, n2, n3)) {
            return 1;
        }
    }

    return 0;
}

/*
 * Add a new device to the VFS layer's device table.
 *
 * If "mountable" is set, the device will be treated as one that expects
 * to have a filesystem mounted on it, and a raw device will be created
 * for direct access.
 */
static int vfs_doadd(const char* dname, int mountable, struct device* dev, struct fs* fs)
{
    char *name = NULL, *rawname = NULL;
    struct knowndev* kd = NULL;
    struct vnode* vnode = NULL;
    const char* volname = NULL;
    unsigned index;
    int result;

    name = kernel_strdup(dname);
    if (name == NULL) {
        goto nomem;
    }
    if (mountable) {
        rawname = mk_raw_name(name);
        if (rawname == NULL) {
            goto nomem;
        }
    }

    vnode = dev_create_vnode(dev);
    if (vnode == NULL) {
        goto nomem;
    }

    kd = kmalloc(sizeof(struct knowndev));
    if (kd == NULL) {
        goto nomem;
    }

    kd->kd_name = name;
    kd->kd_rawname = rawname;
    kd->kd_device = dev;
    kd->kd_vnode = vnode;
    kd->kd_fs = fs;

#ifdef VFS_DEBUG
    kernel_printf("device's name: %s\n", kd->kd_name);
    if (kd->kd_rawname != NULL) {
        kernel_printf("device's raw name: %s\n", kd->kd_rawname);
    } else {
        kernel_printf("this device is not mountable.\n");
    }
#endif

    if (fs != NULL) {
        volname = FSOP_GETVOLNAME(fs);
    }

    if (bad_names(name, rawname, volname)) {
        return EEXIST;
    }

    result = array_add(knowndevs, kd, &index);

    if (result == 0 && dev != NULL) {
        /* use index+1 as the device number, so 0 is reserved */
        dev->d_devnumber = index + 1;
    }
    return result;

nomem:
    if (name) {
        kfree(name);
    }
    if (rawname) {
        kfree(rawname);
    }
    if (vnode) {
        kfree(vnode);
    }
    if (kd) {
        kfree(kd);
    }

    return ENOMEM;
}

/*
 * Add a new device, by name. See above for the description of
 * mountable.
 */
int vfs_adddev(const char* devname, struct device* dev, int mountable)
{
    return vfs_doadd(devname, mountable, dev, NULL);
}

/*
 * Add a filesystem that does not have an underlying device.
 * This is used for emufs, but might also be used for network
 * filesystems and the like.
 */
int vfs_addfs(const char* devname, struct fs* fs)
{
    return vfs_doadd(devname, 0, NULL, fs);
}

/*
 * Given a device name (lhd0, emu0, somevolname, null, etc.), hand
 * back an appropriate vnode.
 */
int vfs_getroot(const char* devname, struct vnode** result, bool file)
{
    struct knowndev* kd;
    unsigned i, num;

    num = array_num(knowndevs);
    for (i = 0; i < num; i++) {
        kd = array_get(knowndevs, i);

/*
		 * If this device has a mounted filesystem, and
		 * DEVNAME names either the filesystem or the device,
		 * return the root of the filesystem.
		 *
		 * If it has no mounted filesystem, it's mountable,
		 * and DEVNAME names the device, return ENXIO.
		 */
#ifdef VFS_DEBUG
        kernel_printf("device's name: %s\n", kd->kd_name);
#endif

        if (kd->kd_fs != NULL) {
#ifdef VFS_DEBUG
            kernel_printf("this device has a file system.\n");
#endif
            const char* volname;
            volname = FSOP_GETVOLNAME(kd->kd_fs);
#ifdef VFS_DEBUG
            kernel_printf("volume name: %s\n", volname);
#endif
            if (!kernel_strcmp(kd->kd_name, devname) || (volname != NULL && !kernel_strcmp(volname, devname))) {
                if (file) {
                    *result = FSOP_GETROOT_FILE(kd->kd_fs);
                } else {
                    *result = FSOP_GETROOT(kd->kd_fs);
                }
                return 0;
            }
        } else {
            if (kd->kd_rawname != NULL && !kernel_strcmp(kd->kd_name, devname)) {
                return ENXIO;
            }
        }
        kernel_printf("this device does not have a file system.\n");

        /*
		 * If DEVNAME names the device, and we get here, it
		 * must have no fs and not be mountable. In this case,
		 * we return the device itself.
		 */
        if (!kernel_strcmp(kd->kd_name, devname)) {
            assert(kd->kd_fs == NULL, "this device has a file system.");
            assert(kd->kd_rawname == NULL, "this device has a raw name.");
            assert(kd->kd_device != NULL, "this device is null");
            VOP_INCREF(kd->kd_vnode);
            *result = kd->kd_vnode;
            return 0;
        }

        /*
		 * If the device has a rawname and DEVNAME names that,
		 * return the device itself.
		 */
        if (kd->kd_rawname != NULL && !kernel_strcmp(kd->kd_rawname, devname)) {
            assert(kd->kd_device != NULL, "this device is null");
            VOP_INCREF(kd->kd_vnode);
            *result = kd->kd_vnode;
            return 0;
        }

        /*
		 * If none of the above tests matched, we didn't name
		 * any of the names of this device, so go on to the
		 * next one. 
		 */
    }

    /*
	 * If we got here, the device specified by devname doesn't exist.
	 */
    return ENODEV;
}

//////////////////////////////////////////////////
/*
 * Look for a mountable device named DEVNAME.
 * Should already hold knowndevs_lock.
 */
static int findmount(const char* devname, struct knowndev** result)
{
    struct knowndev* dev;
    unsigned i, num;
    bool found = false;

    num = array_num(knowndevs);
    for (i = 0; !found && i < num; i++) {
        dev = array_get(knowndevs, i);
        if (dev->kd_rawname == NULL) {
            // not mountable/unmountable.
            continue;
        }

        if (!kernel_strcmp(devname, dev->kd_name)) {
            *result = dev;
            found = true;
        }
    }

    return found ? 0 : ENODEV;
}

/*
 * Mount a filesystem. Once we've found the device, call MOUNTFUNC to
 * set up the filesystem and hand back a struct fs.
 *
 * The DATA argument is passed through unchanged to MOUNTFUNC.
 */
int vfs_mount(const char* devname, void* data, int (*mountfunc)(void* data, struct device*, struct fs** ret))
{
    const char* volname;
    struct knowndev* kd;
    struct fs* fs;
    int result;

    result = findmount(devname, &kd);
    if (result) {
        return result;
    }

    if (kd->kd_fs != NULL) {
        return EBUSY;
    }
    assert(kd->kd_rawname != NULL, "this device is unmountable.");
    assert(kd->kd_device != NULL, "this device does not exist.");

    result = mountfunc(data, kd->kd_device, &fs);
    if (result) {
        return result;
    }

    assert(fs != NULL, "file system does not exist.");

    kd->kd_fs = fs;

    volname = FSOP_GETVOLNAME(fs);
    kernel_printf("vfs: mounted %s: on %s\n", volname ? volname : kd->kd_name, kd->kd_name);

    return 0;
}

/*
 * Unmount a filesystem/device by name.
 * First calls FSOP_SYNC on the filesystem; then calls FSOP_UNMOUNT.
 */
int vfs_unmount(const char* devname)
{
    struct knowndev* kd;
    int result;

    result = findmount(devname, &kd);
    if (result) {
        goto fail;
    }

    if (kd->kd_fs == NULL) {
        result = EINVAL;
        goto fail;
    }
    assert(kd->kd_rawname != NULL, "this device is unmountable.");
    assert(kd->kd_device != NULL, "this device does not exist");

    result = FSOP_SYNC(kd->kd_fs);
    if (result) {
        goto fail;
    }

    result = FSOP_UNMOUNT(kd->kd_fs);
    if (result) {
        goto fail;
    }

    kernel_printf("vfs: Unmounted %s:\n", kd->kd_name);

    // Now drop the filesystem.
    kd->kd_fs = NULL;

    assert(result == 0, "unmount failed");

fail:
    return result;
}

/*
 * Global unmount function.
 */
int vfs_unmountall(void)
{
    struct knowndev* dev;
    unsigned i, num;
    int result;

    num = array_num(knowndevs);
    for (i = 0; i < num; i++) {
        dev = array_get(knowndevs, i);
        if (dev->kd_rawname == NULL) {
            // not mountable/unmountable.
            continue;
        }
        if (dev->kd_fs == NULL) {
            // Not mounted.
            continue;
        }

        kernel_printf("vfs: Unmounting %s:\n", dev->kd_name);

        result = FSOP_SYNC(dev->kd_fs);
        if (result) {
            kernel_printf("vfs: Warning: sync failed for %s: %s, trying "
                          "again\n",
                dev->kd_name, strerror(result));

            result = FSOP_SYNC(dev->kd_fs);
            if (result) {
                kernel_printf("vfs: Warning: sync failed second time"
                              " for %s: %s, giving up...\n",
                    dev->kd_name, strerror(result));
                continue;
            }
        }

        result = FSOP_UNMOUNT(dev->kd_fs);
        if (result == EBUSY) {
            kernel_printf("vfs: Cannot unmount %s: (busy)\n",
                dev->kd_name);
            continue;
        }
        if (result) {
            kernel_printf("vfs: Warning: unmount failed for %s:"
                          " %s, already synced, dropping...\n",
                dev->kd_name, strerror(result));
            continue;
        }

        // Now drop the filesystem.
        dev->kd_fs = NULL;
    }

    return 0;
}
