#ifndef _XSU_UTILS_H
#define _XSU_UTILS_H

#include <xsu/types.h>

#define container_of(ptr, type, member) ((type*)((char*)ptr - (char*)&(((type*)0)->member)))

/*
 * C memory functions. 
 */
void* kernel_memcpy(void* dst, void* src, int len);
void* kernel_memset(void* dst, int b, int len);
void* kernel_memmove(void* dst, void* src, size_t len);
unsigned int* kernel_memset_word(unsigned int* dst, unsigned int w, int len);
void bzero(void* vblock, size_t len);

/*
 * C string functions.
 *
 * kernel_strdup is like strdup, but calls kmalloc instead of malloc.
 * If out of memory, it returns NULL.
 */
int kernel_strcmp(const char* dest, const char* src);
unsigned int kernel_strlen(const char* str);
char* kernel_strchr(const char* s, int ch_arg);
char* kernel_strcpy(char* dest, const char* src);
char* kernel_strcat(char* dest, const char* src);
char* kernel_strdup(const char* str);
char* kernel_strtok_r(char* string, const char* seps, char** context);

/*
 * Miscellaneous functions.
 */
void kernel_cache(unsigned int block_index);
void kernel_serial_puts(char* str);
void kernel_serial_putc(char c);

int pow(int x, int z);
unsigned int is_bound(unsigned int val, unsigned int bound);
const char* strerror(int errcode);

typedef unsigned char* va_list;
#define _INTSIZEOF(n) ((sizeof(n) + sizeof(unsigned int) - 1) & ~(sizeof(unsigned int) - 1))
#define va_start(ap, v) (ap = (va_list)&v + _INTSIZEOF(v))
#define va_arg(ap, t) (*(t*)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)))
#define va_end(ap) (ap = (va_list)0)

#endif
