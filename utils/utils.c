#include <driver/vga.h>
#include <xsu/utils.h>

/*
 * C memory functions. 
 */

void* kernel_memcpy(void* dest, void* src, int len)
{
    char* deststr = dest;
    char* srcstr = src;
    while (len--) {
        *deststr = *srcstr;
        deststr++;
        srcstr++;
    }
    return dest;
}

#pragma GCC push_options
#pragma GCC optimize("O2")
void* kernel_memset(void* dest, int b, int len)
{
#ifdef MEMSET_DEBUG
    kernel_printf("memset:%x,%x,len%x,", (int)dest, b, len);
#endif // ! MEMSET_DEBUG
    char content = b ? -1 : 0;
    char* deststr = dest;
    while (len--) {
        *deststr = content;
        deststr++;
    }
#ifdef MEMSET_DEBUG
    kernel_printf("%x\n", (int)deststr);
#endif // ! MEMSET_DEBUG
    return dest;
}
#pragma GCC pop_options

unsigned int* kernel_memset_word(unsigned int* dest, unsigned int w, int len)
{
    while (len--)
        *dest++ = w;

    return dest;
}

/*
 * C standard function - copy a block of memory, handling overlapping
 * regions correctly.
 */
void* kernel_memmove(void* dst, const void* src, size_t len)
{
    size_t i;

    /*
	 * If the buffers don't overlap, it doesn't matter what direction
	 * we copy in. If they do, it does, so just assume they always do.
	 * We don't concern ourselves with the possibility that the region
	 * to copy might roll over across the top of memory, because it's
	 * not going to happen.
	 *
	 * If the destination is above the source, we have to copy
	 * back to front to avoid overwriting the data we want to
	 * copy.
	 *
	 *      dest:       dddddddd
	 *      src:    ssssssss   ^
	 *              |   ^  |___|
         *              |___|
	 *
	 * If the destination is below the source, we have to copy
	 * front to back.
	 *
	 *      dest:   dddddddd
	 *      src:    ^   ssssssss
	 *              |___|  ^   |
         *                     |___|
	 */

    if ((uintptr_t)dst < (uintptr_t)src) {
        /*
		 * As author/maintainer of libc, take advantage of the
		 * fact that we know memcpy copies forwards.
		 */
        return kernel_memcpy(dst, src, len);
    }

    /*
	 * Copy by words in the common case. Look in memcpy.c for more
	 * information.
	 */

    if ((uintptr_t)dst % sizeof(long) == 0 && (uintptr_t)src % sizeof(long) == 0 && len % sizeof(long) == 0) {

        long* d = dst;
        const long* s = src;

        /*
		 * The reason we copy index i-1 and test i>0 is that
		 * i is unsigned -- so testing i>=0 doesn't work.
		 */

        for (i = len / sizeof(long); i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    } else {
        char* d = dst;
        const char* s = src;

        for (i = len; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dst;
}

/*
 * Standard (well, semi-standard) C string function - zero a block of
 * memory.
 */
void bzero(void* vblock, size_t len)
{
    char* block = vblock;
    size_t i;

    /*
	 * For performance, optimize the common case where the pointer
	 * and the length are word-aligned, and write word-at-a-time
	 * instead of byte-at-a-time. Otherwise, write bytes.
	 *
	 * The alignment logic here should be portable. We rely on the
	 * compiler to be reasonably intelligent about optimizing the
	 * divides and modula out. Fortunately, it is.
	 */

    if ((uintptr_t)block % sizeof(long) == 0 && len % sizeof(long) == 0) {
        long* lb = (long*)block;
        for (i = 0; i < len / sizeof(long); i++) {
            lb[i] = 0;
        }
    } else {
        for (i = 0; i < len; i++) {
            block[i] = 0;
        }
    }
}

/*
 * C string functions.
 *
 * kernel_strdup is like strdup, but calls kmalloc instead of malloc.
 * If out of memory, it returns NULL.
 */

int kernel_strcmp(const char* dest, const char* src)
{
    while ((*dest == *src) && (*dest != 0)) {
        dest++;
        src++;
    }
    return *dest - *src;
}

unsigned int kernel_strlen(const char* str)
{
    unsigned int len = 0;
    while (str[len])
        ++len;
    return len;
}

char* kernel_strcpy(char* dest, const char* src)
{
    while ((*dest++ = *src++))
        ;
    return dest;
}

char* kernel_strcat(chat* dest, const char* src)
{
    unsigned int offset = kernel_strlen(src);

    kernel_strcpy(dest + offset, src);
    return dest;
}

/*
 * Like strdup, but calls kmalloc.
 */
char* kernel_strdup(const char* s)
{
    char* z;

    z = kmalloc(strlen(s) + 1);
    if (z == NULL) {
        return NULL;
    }
    kernel_strcpy(z, s);
    return z;
}
