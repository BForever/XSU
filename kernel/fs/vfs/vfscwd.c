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
 * VFS operations involving the current directory.
 */
#include <xsu/current.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/types.h>

extern struct current curpath;

/*
 * Get current directory as a vnode.
 * 
 * We do not synchronize curthread->t_cwd, because it belongs exclusively
 * to its own thread; no other threads should access it.
 */
int vfs_getcurdir(struct vnode** ret)
{
    int rv = 0;
    VOP_INCREF(curpath.t_cwd);
    *ret = curpath.t_cwd;

    return rv;
}

/*
 * Set current directory as a vnode.
 * The passed vnode must in fact be a directory.
 */
int vfs_setcurdir(struct vnode* dir)
{
    struct vnode* old;
    mode_t vtype;
    int result;

    result = VOP_GETTYPE(dir, &vtype);
    if (result) {
        return result;
    }
    if (vtype != S_IFDIR) {
        return ENOTDIR;
    }

    VOP_INCREF(dir);

    old = curpath.t_cwd;
    curpath.t_cwd = dir;

    if (old != NULL) {
        VOP_DECREF(old);
    }

    return 0;
}

/*
 * Set current directory, as a pathname. Use vfs_lookup to translate
 * it to a vnode.
 */
int vfs_chdir(char* path)
{
    struct vnode* vn;
    int result;

    result = vfs_lookup(path, &vn);
    if (result) {
        return result;
    }
    result = vfs_setcurdir(vn);
    VOP_DECREF(vn);
    return result;
}
