#include <assert.h>
#include <driver/vga.h>

// void assert(int statement, char* message) {
//     if (statement != 1) {
//         kernel_printf("[ASSERT ERROR]: %s\n", message);
//         while (1)
//             ;
//     }
// }

void badassert(const char* expr, const char* msg)
{
    kernel_printf("Assertion failed: %s, %s\n", expr, msg);
    while (1)
        ;
}
