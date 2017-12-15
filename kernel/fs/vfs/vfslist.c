/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * VFS operations that involve the list of VFS (named) devices
 * (the "dev" in "dev:path" syntax).
 */

#define VFSINLINE

#include <kern/errno.h>
#include <xsu/device.h>
#include <xsu/fs/fs.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/slab.h>
#include <xsu/synch.h>
#include <xsu/utils.h>
#ifdef VFS_DEBUG
#include <driver/vga.h>
#endif

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

DECLARRAY(knowndev);
DEFARRAY(knowndev);

static struct knowndevarray* knowndevs;
static struct lock* knowndevs_lock;

// The big lock for all FS ops. Remove for filesystem assignment.
static struct lock* vfs_biglock;
static unsigned vfs_biglock_depth;

/*
 * Setup function. 
 */
void vfs_bootstrap(void)
{
    knowndevs = knowndevarray_create();
    if (knowndevs == NULL) {
        log(LOG_FAIL, "VFS: Could not create knowndevs array.");
    }

    vfs_biglock = lock_create("vfs_biglock");
    if (vfs_biglock == NULL) {
        log(LOG_FAIL, "VFS: Could not create vfs big lock.");
    }
    vfs_biglock_depth = 0;

    devnull_create();
#ifdef VFS_DEBUG
    kernel_printf("A null device has been created.");
#endif
}

/*
 * Operations on vfs_biglock. We make it recursive to avoid having to
 * think about where we do and don't already hold it. This is an
 * undesirable hack that's frequently necessary when a lock covers too
 * much material. Your solution scheme for FS and VFS locking should
 * not require recursive locks.
 */
void vfs_biglock_acquire(void)
{
    if (!lock_do_i_hold(vfs_biglock)) {
        lock_acquire(vfs_biglock);
    } else if (vfs_biglock_depth == 0) {
        /*
		 * Supposedly we hold it, but the depth is 0. This may
		 * mean: (1) the count is messed up, or (2)
		 * lock_do_i_hold is lying. Since OS/161 ships out of
		 * the box with unimplemented locks (students
		 * implement them) that always return true, assume
		 * situation (2). In this case acquire the lock
		 * anyway.
		 *
		 * Once you have working locks, this won't be the
		 * case, and if you get here it should be situation
		 * (1), in which case the count is messed up and one
		 * can panic.
		 */
        lock_acquire(vfs_biglock);
    }
    vfs_biglock_depth++;
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
    kernel_strcat(s, "raw");
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

    assert(lock_do_i_hold(knowndevs_lock), "The device lock was held by other threads");

    num = knowndevarray_num(knowndevs);
    for (i = 0; i < num; i++) {
        kd = knowndevarray_get(knowndevs, i);

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

    //	vfs_biglock_acquire();

    knowndevs_lock = lock_create("knowndev");
    if (knowndevs_lock == NULL) {
        log(LOG_FAIL, "vfs: Could not create knowndev lock\n");
    }

    lock_acquire(knowndevs_lock);
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
    kernel_printf("device's raw name: %s\n", kd->kd_rawname);
#endif

    if (fs != NULL) {
        volname = FSOP_GETVOLNAME(fs);
    }

    if (bad_names(name, rawname, volname)) {
        vfs_biglock_release();
        return EEXIST;
    }

    result = knowndevarray_add(knowndevs, kd, &index);

    if (result == 0 && dev != NULL) {
        /* use index+1 as the device number, so 0 is reserved */
        dev->d_devnumber = index + 1;
    }

    lock_release(knowndevs_lock);
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

    lock_release(knowndevs_lock);
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
