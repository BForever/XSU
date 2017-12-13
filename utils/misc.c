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

#include <kern/errmsg.h>
#include <xsu/log.h>
#include <xsu/slab.h>
#include <xsu/types.h>
#include <xsu/utils.h>

/*
 * Standard C function to return a string for a given errno.
 * Kernel version; panics if it hits an unknown error.
 */
const char* strerror(int errcode)
{
    if (errcode >= 0 && errcode < sys_nerr) {
        return sys_errlist[errcode];
    }
    log(LOG_FAIL, "Invalid error code %d\n", errcode);
    return NULL;
}

int pow(int x, int z)
{
    int ret = 1;
    if (z < 0)
        return -1;
    while (z--) {
        ret *= x;
    }
    return ret;
}

#pragma GCC push_options
#pragma GCC optimize("O0")

void kernel_cache(unsigned int block_index)
{
    block_index = block_index | 0x80000000;
    asm volatile(
        "li $t0, 233\n\t"
        "mtc0 $t0, $8\n\t"
        "move $t0, %0\n\t"
        "cache 0, 0($t0)\n\t"
        "nop\n\t"
        "cache 1, 0($t0)\n\t"
        : "=r"(block_index));
}

#pragma GCC pop_options

void kernel_serial_puts(char* str)
{
    while (*str)
        *((unsigned int*)0xbfc09018) = *str++;
}

void kernel_serial_putc(char c)
{
    *((unsigned int*)0xbfc09018) = c;
}

unsigned int is_bound(unsigned int val, unsigned int bound)
{
    return !(val & (bound - 1));
}
