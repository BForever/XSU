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

#ifndef _XSU_FS_VFS_H
#define _XSU_FS_VFS_H

/*
 * Misc
 *
 *    init_vfs      - Call during system initialization to allocate 
 *                    structures.
 *
 *    vfs_setbootfs - Set the filesystem that paths beginning with a
 *                    slash are sent to. If not set, these paths fail
 *                    with ENOENT. The argument should be the device
 *                    name or volume name for the filesystem (such as
 *                    "lhd0:") but need not have the trailing colon.
 *
 *    vfs_clearbootfs - Clear the bootfs filesystem. This should be
 *                    done during shutdown so that the filesystem in
 *                    question can be unmounted.
 *
 *    vfs_adddev    - Add a device to the VFS named device list. If
 *                    MOUNTABLE is zero, the device will be accessible
 *                    as "DEVNAME:". If the mountable flag is set, the
 *                    device will be accessible as "DEVNAMEraw:" and
 *                    mountable under the name "DEVNAME". Thus, the
 *                    console, added with MOUNTABLE not set, would be
 *                    accessed by pathname as "con:", and lhd0, added
 *                    with mountable set, would be accessed by
 *                    pathname as "lhd0raw:" and mounted by passing
 *                    "lhd0" to vfs_mount.
 *
 *    vfs_addfs     - Add a hardwired filesystem to the VFS named device
 *                    list. It will be accessible as "devname:". This is
 *                    intended for filesystem-devices like emufs, and
 *                    gizmos like Linux procfs or BSD kernfs, not for
 *                    mounting filesystems on disk devices.
 *
 *    vfs_mount     - Attempt to mount a filesystem on a device. The
 *                    device named by DEVNAME will be looked up and 
 *                    passed, along with DATA, to the supplied function
 *                    MOUNTFUNC, which should create a struct fs and
 *                    return it in RESULT.
 *
 *    vfs_unmount   - Unmount the filesystem presently mounted on the
 *                    specified device.
 *
 *    vfs_unmountall - Unmount all mounted filesystems.
 */

void init_vfs(void);
int vfs_adddev(const char* devname, struct device* dev, int mountable);

#endif
