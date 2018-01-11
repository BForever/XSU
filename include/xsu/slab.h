#ifndef _XSU_SLAB_H
#define _XSU_SLAB_H

#include <xsu/buddy.h>
#include <xsu/list.h>

#define SIZE_INT 4
#define SLAB_AVAILABLE 0x0
#define SLAB_USED 0xff

/*
 * slab_head makes the allocation accessible from listTailPtr to the end of the page
 * @listTailPtr : points to tail of the page's slab element list
 * @allocatedNumber : keeps the numbers of memory segments that has been allocated
 */
struct slab_head {
    void* listTailPtr;
    unsigned int allocatedNumber;
};

/*
 * this is the slab control block, record the information and control the slub' allocation
 * each slub contains a kmem_cache_node
 * @partial keeps the list of un-totally-allocated slub element
 * @full keeps the list of totally-allocated slub element
 */
struct kmem_cache_node {
    struct list_head partial;
    struct list_head full;
};

/*
 * current being allocated page unit
 * @cpuFreeObjectPtr: points to the free-space head addr inside current page
 * @page: points to the current page(which is being allocated!)
 */
struct kmem_cache_cpu {
    // attention, it is a two-level pointer
    void** cpuFreeObjectPtr; 
    struct page* page;
};
/*
 * this is the slab information block, record the information of every single slub system
 * each slub contains a kmem_cache
 * @size: input parameters, such as 8, 16, 32....., but it doesn't means the really allocated size
 * @objectSize: the real allocated size, contains the memory(allocated to user) and the pointers(user cannot access)
 * @offset: pointer's offset in slub element
 * @name: this slub element's name
 */

struct kmem_cache {
    unsigned int size;
    unsigned int objectSize;
    unsigned int offset;
    struct kmem_cache_node node;
    struct kmem_cache_cpu cpu;
    unsigned char name[16];
};

// extern struct kmem_cache kmalloc_caches[PAGE_SHIFT];
extern void init_slab();
extern void* kmalloc(unsigned int size);
extern void kfree(void* obj);
extern void kmemtop();
#endif
