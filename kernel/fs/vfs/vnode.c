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
 * Basic vnode support functions.
 */
#include <assert.h>
#include <driver/vga.h>
#include <kern/errno.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/log.h>
#include <xsu/synch.h>
#include <xsu/types.h>
#include <xsu/utils.h>

/*
 * Initialize an abstract vnode.
 * Invoked by VOP_INIT.
 */
int vnode_init(struct vnode* vn, const struct vnode_ops* ops, struct fs* fs, void* fsdata)
{
    assert(vn != NULL, "vnode is null");
    assert(ops != NULL, "vnode operation is null");

    vn->vn_ops = ops;
    vn->vn_refcount = 1;
    vn->vn_opencount = 0;
    vn->vn_fs = fs;
    vn->vn_data = fsdata;

    vn->vn_rwlock = lock_create("vnode-rwlock");
    if (vn->vn_rwlock == NULL) {
        return ENOMEM;
    }
    vn->vn_createlock = lock_create("vnode-createlock");
    if (vn->vn_createlock == NULL) {
        return ENOMEM;
    }
    vn->vn_countlock = lock_create("vnode-countlock");
    if (vn->vn_countlock == NULL) {
        return ENOMEM;
    }

    return 0;
#ifdef VFS_DEBUG
    kernel_printf("DEBUG VFS: ref-%d\topen-%d\n", vn->vn_refcount, vn->vn_opencount);
#endif
}

/*
 * Destroy an abstract vnode.
 * Invoked by VOP_CLEANUP.
 */
void vnode_cleanup(struct vnode* vn)
{
    assert(vn->vn_refcount == 1, "vnode's reference count is not equal to 1");
    assert(vn->vn_opencount == 0, "vnode's open count is not equal to 0");
    assert(vn->vn_countlock != NULL, "vnode is locked!");

    //	lock_release(vn->vn_countlock);
    lock_destroy(vn->vn_countlock);
    vn->vn_ops = NULL;
    vn->vn_refcount = 0;
    vn->vn_opencount = 0;
    vn->vn_fs = NULL;
    vn->vn_countlock = NULL;
    vn->vn_data = NULL;
}

/*
 * Increment refcount.
 * Called by VOP_INCREF.
 */
void vnode_incref(struct vnode* vn)
{
    assert(vn != NULL, "vnode is null");
    lock_acquire(vn->vn_countlock);
    vn->vn_refcount++;
    lock_release(vn->vn_countlock);
}

/*
 * Decrement refcount.
 * Called by VOP_DECREF.
 * Calls VOP_RECLAIM if the refcount hits zero.
 */
void vnode_decref(struct vnode* vn)
{
    int result, do_it = 0;

    assert(vn != NULL, "vnode is null");

    lock_acquire(vn->vn_countlock);

    assert(vn->vn_refcount > 0, "vnode's reference count is zero");
    if (vn->vn_refcount > 1) {
        vn->vn_refcount--;
    } else {
        do_it = 1;
    }
    lock_release(vn->vn_countlock);
    if (do_it) {
        result = VOP_RECLAIM(vn);
        if (result != 0 && result != EBUSY) {
            // XXX: lame.
            kernel_printf("vfs: Warning: VOP_RECLAIM: %s\n", strerror(result));
        }
    }
}

/*
 * Increment the open count.
 * Called by VOP_INCOPEN.
 */
void vnode_incopen(struct vnode* vn)
{
    assert(vn != NULL, "vnode is null");

    lock_acquire(vn->vn_countlock);
    vn->vn_opencount++;
    lock_release(vn->vn_countlock);
}

/*
 * Decrement the open count.
 * Called by VOP_DECOPEN.
 */
void vnode_decopen(struct vnode* vn)
{
    int result;

    assert(vn != NULL, "vnode is null");

    lock_acquire(vn->vn_countlock);

    assert(vn->vn_opencount > 0, "vnode's reference count is zero");
    vn->vn_opencount--;

    if (vn->vn_opencount > 0) {
        lock_release(vn->vn_countlock);
        return;
    }
    lock_release(vn->vn_countlock);
    result = VOP_CLOSE(vn);
    if (result) {
        // XXX: also lame.
        // The FS should do what it can to make sure this code
        // doesn't get reached...
        kernel_printf("vfs: Warning: VOP_CLOSE: %s\n", strerror(result));
    }
}

/*
 * Check for various things being valid.
 * Called before all VOP_* calls.
 */
void vnode_check(struct vnode* v, const char* opstr)
{

    if (v == NULL) {
        log(LOG_FAIL, "vnode_check: vop_%s: null vnode\n", opstr);
    }
    if (v == (void*)0xdeadbeef) {
        log(LOG_FAIL, "vnode_check: vop_%s: deadbeef vnode\n", opstr);
    }

    if (v->vn_ops == NULL) {
        log(LOG_FAIL, "vnode_check: vop_%s: null ops pointer\n", opstr);
    }
    if (v->vn_ops == (void*)0xdeadbeef) {
        log(LOG_FAIL, "vnode_check: vop_%s: deadbeef ops pointer\n", opstr);
    }

    if (v->vn_ops->vop_magic != VOP_MAGIC) {
        log(LOG_FAIL, "vnode_check: vop_%s: ops with bad magic number %lx\n", opstr, v->vn_ops->vop_magic);
    }

    // Device vnodes have null fs pointers.
    //if (v->vn_fs == NULL) {
    //	panic("vnode_check: vop_%s: null fs pointer\n", opstr);
    //}
    if (v->vn_fs == (void*)0xdeadbeef) {
        log(LOG_FAIL, "vnode_check: vop_%s: deadbeef fs pointer\n", opstr);
    }

    lock_acquire(v->vn_countlock);

    if (v->vn_refcount < 0) {
        log(LOG_FAIL, "vnode_check: vop_%s: negative refcount %d\n", opstr, v->vn_refcount);
    } else if (v->vn_refcount == 0 && strcmp(opstr, "reclaim")) {
        log(LOG_FAIL, "vnode_check: vop_%s: zero refcount\n", opstr);
    } else if (v->vn_refcount > 0x100000) {
        kernel_printf("vnode_check: vop_%s: warning: large refcount %d\n", opstr, v->vn_refcount);
    }

    if (v->vn_opencount < 0) {
        log(LOG_FAIL, "vnode_check: vop_%s: negative opencount %d\n", opstr, v->vn_opencount);
    } else if (v->vn_opencount > 0x100000) {
        kernel_printf("vnode_check: vop_%s: warning: large opencount %d\n", opstr, v->vn_opencount);
    }

    lock_release(v->vn_countlock);
}
