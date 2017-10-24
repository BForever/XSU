#ifndef _XSU_UTILS_H
#define _XSU_UTILS_H

#define container_of(ptr, type, member) ((type*)((char*)ptr - (char*)&(((type*)0)->member)))

void* kernel_memcpy(void* dest, void* src, int len);
void* kernel_memset(void* dest, int b, int len);
unsigned int* kernel_memset_word(unsigned int* dest, unsigned int w, int len);
int kernel_strcmp(const char* dest, const char* src);
char* kernel_strcpy(char* dest, const char* src);
int pow(int x, int z);
void kernel_cache(unsigned int block_index);
void kernel_serial_puts(char* str);
void kernel_serial_putc(char c);
unsigned int is_bound(unsigned int val, unsigned int bound);

/* Something about the varargs.
 * Operating System Design and Implementation, P63 - 64.
 */
typedef unsigned char* va_list;
// Align the arguments according to the size of unsigned int.
// Due to the "default argument promotion" in C, all the argument passed will be promoted to the type of unsigned int
// (since our operating system does not support float point numbers).
#define _INTSIZEOF(n) ((sizeof(n) + sizeof(unsigned int) - 1) & ~(sizeof(unsigned int) - 1))
// Get the address of the first variable argument.
#define va_start(ap, v) (ap = (va_list)&v + _INTSIZEOF(v))
// Get the address of next variable argument and the value current pointer point to.
#define va_arg(ap, t) (*(t*)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)))
// Free the pointer.
#define va_end(ap) (ap = (va_list)0)

#endif
