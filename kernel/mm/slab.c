#include <arch.h>
#include <driver/vga.h>
#include <xsu/slab.h>
#include <xsu/utils.h>


#define KMEM_ADDR(PAGE, BASE) ((((PAGE) - (BASE)) << PAGE_SHIFT) | 0x80000000)

/*
 * one list of PAGE_SHIFT(now it's 12) possbile memory size
 * 96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, (2 undefined)
 * in current stage, set (2 undefined) to be (4, 2048)
 * 
 */
struct kmem_cache kmalloc_caches[PAGE_SHIFT];

static unsigned int size_kmem_cache[PAGE_SHIFT] = {96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, 1536, 2048};

unsigned int judge_slab_free(struct kmem_cache *cache, void *object){
    //kernel_printf("judeg slab free condition, address is: %x", object);
    struct page* objectPage = pages + ((unsigned int)object >> PAGE_SHIFT);
    object = (void *)((unsigned int)object | KERNEL_ENTRY);
    void **freeSpacePtr = objectPage->slabFreeSpacePtr;
    unsigned int flag = 0;//not found
    if(objectPage->flag != _PAGE_SLAB)
        return 1;
    *freeSpacePtr = (void *)((unsigned int)(*freeSpacePtr) | KERNEL_ENTRY);
    while(!(is_bound((unsigned int )(*(freeSpacePtr)), 1 << PAGE_SHIFT)))
    {
        //kernel_printf("judge_slab_free: freeSpacePtr's value is: %x\n", *(freeSpacePtr));
        if(*(freeSpacePtr) == object)
        {    
            flag = 1;
            break;
        }
        freeSpacePtr = (void **)((unsigned int)*(freeSpacePtr) + cache->offset);
    }
    if(flag != 0)
        return 1;//represent in the free space, which has been freed.
    else
        return 0;
}
// init the struct kmem_cache_cpu
void init_kmem_cpu(struct kmem_cache_cpu *kcpu) {
    kcpu->page = 0;
    kcpu->cpuFreeObjectPtr = 0;
}

// init the struct kmem_cache_node
void init_kmem_node(struct kmem_cache_node *knode) {
    INIT_LIST_HEAD(&(knode->full));
    INIT_LIST_HEAD(&(knode->partial));
}

void init_each_slab(struct kmem_cache *cache, unsigned int size) {
    cache->objectSize = size;
    cache->objectSize += (SIZE_INT - 1);
    cache->objectSize &= ~(SIZE_INT - 1);
    cache->size = cache->objectSize + sizeof(void *);  // add one char as mark(available)
    cache->offset = size;
    init_kmem_cpu(&(cache->cpu));
    init_kmem_node(&(cache->node));
}

void init_slab() {
    unsigned int i;

    for (i = 0; i < PAGE_SHIFT; i++) {
        init_each_slab(&(kmalloc_caches[i]), size_kmem_cache[i]);
    }
#ifdef SLAB_DEBUG
    kernel_printf("Setup Slub ok :\n");
    kernel_printf("\tcurrent slab cache size list:\n\t");
    for (i = 0; i < PAGE_SHIFT; i++) {
        kernel_printf("%x %x ", kmalloc_caches[i].objectSize, (unsigned int)(&(kmalloc_caches[i])));
    }
    kernel_printf("\n");
#endif  // ! SLAB_DEBUG
}

// ATTENTION: sl_objs is the reuse of pageOrderLevel
// ATTENTION: slabFreeSpacePtr must be set right to add itself to reach the end of the page
// 		e.g. if size = 96 then 4096 / 96 = .. .. 64 then slabFreeSpacePtr starts at
// 64
void format_slab_page(struct kmem_cache *cache, struct page *page) {
    unsigned char *movingPtr = (unsigned char *)KMEM_ADDR(page, pages);  // physical addr
    struct slab_head *slabHeadInPage = (struct slab_head *)movingPtr;
    unsigned int *tempPtr;
    unsigned int remaining = (1 << PAGE_SHIFT);
    unsigned int startAddress;
    movingPtr += (unsigned int)sizeof(struct slab_head);
    //kernel_printf("slab_head's size is: %x\n", (unsigned int)sizeof(struct slab_head));

    remaining -=  sizeof(struct slab_head);
    startAddress = (unsigned int)movingPtr;
    set_flag(page, _PAGE_SLAB);
    do {
        tempPtr = (unsigned int *)(movingPtr + cache->offset);
        movingPtr += cache->size;
        *tempPtr = (unsigned int)movingPtr;
        remaining -= cache->size;
    } while (remaining >= cache->size);

    *tempPtr = (unsigned int)movingPtr & ~((1 << PAGE_SHIFT) - 1);
    slabHeadInPage->listTailPtr = tempPtr;
    slabHeadInPage->allocatedNumber = 0;

    cache->cpu.page = page;
    cache->cpu.cpuFreeObjectPtr = (void **)(&startAddress);
    page->pageCacheBlock = (void *)cache;
    page->slabFreeSpacePtr = (cache->cpu.cpuFreeObjectPtr);
}

void *slab_alloc(struct kmem_cache *cache) {
    struct slab_head *slabHeadInPage;
    void *object = 0;
    struct page *newpage;

    if (cache->cpu.cpuFreeObjectPtr){
        object = *(cache->cpu.cpuFreeObjectPtr);
    }
    //kernel_printf("slab_alloc's object is: %x\n", object);
slalloc_check:
    // 1st: check if the cpuFreeObjectPtr is in the boundary situation
    if (is_bound((unsigned int)object, (1 << PAGE_SHIFT))) {
        // 2nd: the page is full
        if (cache->cpu.page) {
            list_add_tail(&(cache->cpu.page->list), &(cache->node.full));
        }

        if (list_empty(&(cache->node.partial))) {
            // call the buddy system to allocate one more page to be slab-cache
            newpage = __alloc_pages(0);  // get pageOrderLevel = 0 page === one page
            if (!newpage) {
                // allocate failed, memory in system is used up
                kernel_printf("ERROR: slab request one page in cache failed\n");
                while (1)
                    ;
            }
#ifdef SLAB_DEBUG
            kernel_printf("\tnew page, index: %x \n", newpage - pages);
#endif  // ! SLAB_DEBUG

        // using standard format to shape the new-allocated page,
        // set the new page to be cpu.page
            format_slab_page(cache, newpage);
            object = *(cache->cpu.cpuFreeObjectPtr);
            // as it's newly allocated no check may be need
            goto slalloc_check;
        }
        // get the header of the cpu.page(struct page)

        // This define is used to get the start pointer of a struct, if one of the member variable has been knowing, 
        // the input variable is:
        // tempPtr: the member variable's pointer;  type: the struct's type;   member: the member variable's name
        // the main computing eqution is "struct's address + member variable's relative address = tempPtr"
        // return the pointer of the struct
        //#define container_of(tempPtr, type, member) ((type*)((char*)tempPtr - (char*)&(((type*)0)->member)))
        cache->cpu.page = container_of(cache->node.partial.next, struct page, list);
        list_del(cache->node.partial.next);
        //kernel_printf("cpu.cpuFreeObjectPtr is: %x\n", cache->cpu.page->slabFreeSpacePtr);
        cache->cpu.cpuFreeObjectPtr = cache->cpu.page->slabFreeSpacePtr;
        object = (void *)(*(cache->cpu.cpuFreeObjectPtr));
        //kernel_printf("cpu.page->slabFreeSpacePtr is: %x\n", object);
        goto slalloc_check;
    }
slalloc_normal:
    cache->cpu.cpuFreeObjectPtr = (void **)((unsigned char *)object + cache->offset);
    cache->cpu.page->slabFreeSpacePtr = (cache->cpu.cpuFreeObjectPtr);
    slabHeadInPage = (struct slab_head *)KMEM_ADDR(cache->cpu.page, pages);
    ++(slabHeadInPage->allocatedNumber);
slalloc_end:
    // slab may be full after this allocation
    if (is_bound((unsigned int )(*(cache->cpu.page->slabFreeSpacePtr)), 1 << PAGE_SHIFT)) {
        kernel_printf("slab is full\n");
        list_add_tail(&(cache->cpu.page->list), &(cache->node.full));
        init_kmem_cpu(&(cache->cpu));
    }
    return object;
}

void slab_free(struct kmem_cache *cache, void *object) {
    if(judge_slab_free(cache, object)){
        kernel_printf("slab_free_failed, because it has been freed before\n");
        return;
    }
    struct page *opage = pages + ((unsigned int)object >> PAGE_SHIFT);
    object = (void *)((unsigned int)object | KERNEL_ENTRY);
    //kernel_printf("slab_free is : %x\n", object);
    unsigned int *tempPtr;
    struct slab_head *slabHeadInPage = (struct slab_head *)KMEM_ADDR(opage, pages);
    //kernel_printf("slab_head is: %x\n", slabHeadInPage);
    //kernel_printf("slab_free_1\n");
    //kernel_printf("slab_object is: %x\n", object);
    if (!(slabHeadInPage->allocatedNumber)) {
        kernel_printf("ERROR : slab_free error!\n");
        // die();
        while (1)
            ;
    }

    unsigned int listTailValue = *(unsigned int *)(slabHeadInPage->listTailPtr);
    tempPtr = (unsigned int *)((unsigned char *)object + cache->offset);
    //kernel_printf("tempPtr is: %x\n", tempPtr);
    //kernel_printf("listTailPtr is: %x\n", slabHeadInPage->listTailPtr);
    // if(tempPtr != slabHeadInPage->listTailPtr){
    // *tempPtr = *((unsigned int *)(slabHeadInPage->listTailPtr));
    //kernel_printf("*tempPtr is: %x\n", *tempPtr);
    *((unsigned int *)(slabHeadInPage->listTailPtr)) = (unsigned int)object;
    *tempPtr = listTailValue;
    slabHeadInPage->listTailPtr = tempPtr;
    // }else{
        // return;
    //     opage->slabFreeSpacePtr = (void**) &object;
    //     // kernel_printf("")
    //     kernel_printf("opage->slabFreeSpacePtr is: %x ", *(opage->slabFreeSpacePtr));
    //     if(cache->cpu.page == opage)
    //         cache->cpu.cpuFreeObjectPtr = opage->slabFreeSpacePtr;
    }
    --(slabHeadInPage->allocatedNumber);
    //kernel_printf("slab_free_2\n");
    if (list_empty(&(opage->list))){
        return;
    }
    kernel_printf("slab_free successful, the address is %x\n", object);
        
    if (!(slabHeadInPage->allocatedNumber)) {
        list_del_init(&(opage->list));
        //kernel_printf("free the page, since all of those page has been free\n");
        __free_pages(opage, 0);
        if(cache->cpu.page == opage){
            //kernel_printf("initialize the cache_cpu\n");
            init_kmem_cpu(&(cache->cpu));
        }
        //kernel_printf("slab free finished! \n");
        return;
    }
    //kernel_printf("slab_free_3\n");
    list_del_init(&(opage->list));
    list_add_tail(&(opage->list), &(cache->node.partial));
}

// find the best-fit slab system for (size)
unsigned int get_slab(unsigned int size) {
    unsigned int i;
    unsigned int slubSize = (1 << (PAGE_SHIFT - 1));  // half page
    unsigned int fitIndex = PAGE_SHIFT;             // record the best fit num & index

    for (i = 0; i < PAGE_SHIFT; i++) {
        if ((kmalloc_caches[i].objectSize >= size) && (kmalloc_caches[i].objectSize < slubSize)) {
            slubSize = kmalloc_caches[i].objectSize;
            fitIndex = i;
        }
    }
    return fitIndex;
}

void *kmalloc(unsigned int size) {
    struct kmem_cache *cache;
    unsigned int fitIndex;
    unsigned int pageOrderLevel;
    if (!size)
        return 0;

    // if the size larger than the max size of slab system, then call buddy to
    // solve this
    if (size > kmalloc_caches[PAGE_SHIFT - 1].objectSize) {
        size += (1 << PAGE_SHIFT) - 1;
        size &= ~((1 << PAGE_SHIFT) - 1);    
        pageOrderLevel = 0;
        size = size >> (PAGE_SHIFT);
        size -=1;
        while(size > 0){
            size >>= 1;
            pageOrderLevel++;
        }
        //kernel_printf("pageOrderLevel is: %x\n", pageOrderLevel);
        return (void *)(KERNEL_ENTRY | (unsigned int)alloc_pages(pageOrderLevel));
    }

    fitIndex = get_slab(size);
    if (fitIndex >= PAGE_SHIFT) {
        kernel_printf("ERROR: No available slab\n");
        while (1)
            ;
    }
    return (void *)(KERNEL_ENTRY | (unsigned int)slab_alloc(&(kmalloc_caches[fitIndex])));
}

void kfree(void *freePtr) {
    struct page *page;
    //kernel_printf("kfree\n");

    freePtr = (void *)((unsigned int)freePtr & (~KERNEL_ENTRY));
    page = pages + ((unsigned int)freePtr >> PAGE_SHIFT);
    if (!(page->flag == _PAGE_SLAB))
        return free_pages((void *)((unsigned int)freePtr & ~((1 << PAGE_SHIFT) - 1)), page->pageOrderLevel);
    //kernel_printf("slab_free\n");
    // return slab_free(page->pageCacheBlock, (void *)((unsigned int)freePtr | KERNEL_ENTRY));
    return slab_free(page->pageCacheBlock, freePtr);
       
}

void kmemtop(){
    get_buddy_allocation_state();
}
