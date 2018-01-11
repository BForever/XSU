#ifndef _XSU_SLUB_H
#define _XSU_SLUB_H

#include <xsu/list.h>
#include <xsu/buddy.h>

#define SIZE_INT 4
#define SLUB_AVAILABLE 0x0
#define SLUB_USED 0xff

/*
 * slub_head makes the allocation accessible from listTailPtr to the end of the page
 * @listTailPtr : points to the head of the rest of the page
 * @allocatedNumber : keeps the numbers of memory segments that has been allocated
 */
struct slub_head {
    void *listTailPtr;
    unsigned int allocatedNumber;
};

/*
 * slub pages is chained in this struct
 * @partial keeps the list of un-totally-allocated pages
 * @full keeps the list of totally-allocated pages
 */
struct kmem_cache_node {
    struct list_head partial;
    struct list_head full;
};

/*
 * current being allocated page unit
 */
struct kmem_cache_cpu {
    void **cpuFreeObjectPtr;  // points to the free-space head addr inside current page
    struct page *page;
};

struct kmem_cache {
    unsigned int size;
    unsigned int objectSize;
    unsigned int offset;
    struct kmem_cache_node node;
    struct kmem_cache_cpu cpu;
    unsigned char name[16];
};

// extern struct kmem_cache kmalloc_caches[PAGE_SHIFT];
extern void init_slub();
extern void *kmalloc(unsigned int size);
extern void kfree(void *obj);

#endif
