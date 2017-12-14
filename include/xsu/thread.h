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

#ifndef _XSU_THREAD_H
#define _XSU_THREAD_H

#include <xsu/threadlist.h>
#include <xsu/types.h>

/* States a thread can be in. */
typedef enum {
    S_RUN, /* running */
    S_READY, /* ready to run */
    S_SLEEP, /* sleeping */
    S_ZOMBIE, /* zombie; exited but not yet deleted */
} threadstate_t;

/* Thread structure. */
struct thread {
    /*
	 * These go up front so they're easy to get to even if the
	 * debugger is messed up.
	 */
    char* t_name; /* Name of this thread */
    const char* t_wchan_name; /* Name of wait channel, if sleeping */
    threadstate_t t_state; /* State this thread is in */

    /*
	 * Thread subsystem internal fields.
	 */
    struct threadlistnode t_listnode; /* Link for run/sleep/zombie lists */
    char* t_stack; /* Kernel-level stack */
    // struct switchframe* t_context; /* Saved register context (on stack) */

    /*
	 * Interrupt state fields.
	 *
	 * t_in_interrupt is true if current execution is in an
	 * interrupt handler, which means the thread's normal context
	 * of execution is stopped somewhere in the middle of doing
	 * something else. This makes assorted operations unsafe.
	 *
	 * See notes in spinlock.c regarding t_curspl and t_iplhigh_count.
	 *
	 * Exercise for the student: why is this material per-thread
	 * rather than per-cpu or global?
	 */
    bool t_in_interrupt; /* Are we in an interrupt? */
    int t_curspl; /* Current spl*() state */
    int t_iplhigh_count; /* # of times IPL has been raised */

    /*
	 * Public fields
	 */
    int priority;
    /* VM */

    /* VFS */
    struct vnode* t_cwd; /* current working directory */

    /* add more here as needed */
    // struct File* fdesc[MAX_FDESC]; //file descriptor (make look to make an array of 20 FDs)
    //remember to deallocate any FD's for this process
    pid_t pid; //pid for the process need to set it with the process descriptor table
    pid_t ppid; //the parent PID;
};

#endif
