#include <driver/vga.h>
#include <xsu/slab.h>
#include <xsu/utils.h>

/*
 * C memory functions. 
 */

void* kernel_memcpy(void* dst, void* src, int len)
{
    char* deststr = dst;
    char* srcstr = src;
    while (len--) {
        *deststr = *srcstr;
        deststr++;
        srcstr++;
    }
    return dst;
}

#pragma GCC push_options
#pragma GCC optimize("O2")
void* kernel_memset(void* dst, int b, int len)
{
#ifdef MEMSET_DEBUG
    kernel_printf("memset:%x,%x,len%x,", (int)dest, b, len);
#endif // ! MEMSET_DEBUG
    char content = b ? -1 : 0;
    char* deststr = dst;
    while (len--) {
        *deststr = content;
        deststr++;
    }
#ifdef MEMSET_DEBUG
    kernel_printf("%x\n", (int)deststr);
#endif // ! MEMSET_DEBUG
    return dst;
}
#pragma GCC pop_options

unsigned int* kernel_memset_word(unsigned int* dst, unsigned int w, int len)
{
    while (len--)
        *dst++ = w;

    return dst;
}

/*
 * C standard function - copy a block of memory, handling overlapping
 * regions correctly.
 */
void* kernel_memmove(void* dst, void* src, size_t len)
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

char* kernel_strchr(const char* s, int ch_arg)
{
    // Avoid sign-extension problems.
    const char ch = ch_arg;

    // Scan from left to right.
    while (*s) {
        // If we hit it, return it.
        if (*s == ch) {
            return (char*)s;
        }
        s++;
    }

    // If we were looking for the 0, return that.
    if (*s == ch) {
        return (char*)s;
    }

    // Didn't find it.
    return NULL;
}

char* kernel_strcpy(char* dest, const char* src)
{
    while ((*dest++ = *src++))
        ;
    return dest;
}

char* kernel_strcat(char* dest, const char* src)
{
    unsigned int offset = kernel_strlen(dest);
    char* result = kmalloc(kernel_strlen(dest) + kernel_strlen(src) + 1);
    kernel_strcpy(result, dest);
    kernel_strcpy(result + offset, src);

    return result;
}

/*
 * Like strdup, but calls kmalloc.
 */
char* kernel_strdup(const char* s)
{
    char* z;
#ifdef FS_DEBUG
    kernel_printf("original string is: %s and the size is %x\n", s, kernel_strlen(s) + 1);
#endif
    z = kmalloc(kernel_strlen(s) + 1);
#ifdef FS_DEBUG
    kernel_printf("malloc address is: %x\n", z);
#endif
    if (z == NULL) {
        return NULL;
    }
    kernel_strcpy(z, s);
    return z;
}

/*
 * Standard C string function: tokenize a string splitting based on a
 * list of separator characters. Reentrant version.
 *
 * The "context" argument should point to a "char *" that is preserved
 * between calls to strtok_r that wish to operate on same string.
 */
char* kernel_strtok_r(char* string, const char* seps, char** context)
{
    char* head; /* start of word */
    char* tail; /* end of word */

    // If we're starting up, initialize context.
    if (string) {
        *context = string;
    }

    // Get potential start of this next word.
    head = *context;
    if (head == NULL) {
        return NULL;
    }

    // Skip any leading separators.
    while (*head && kernel_strchr(seps, *head)) {
        head++;
    }

    // Did we hit the end?
    if (*head == 0) {
        // Nothing left.
        *context = NULL;
        return NULL;
    }

    // Skip over word.
    tail = head;
    while (*tail && !kernel_strchr(seps, *tail)) {
        tail++;
    }

    // Save head for next time in context.
    if (*tail == 0) {
        *context = NULL;
    } else {
        *tail = 0;
        tail++;
        *context = tail;
    }

    // Return current word.
    return head;
}
