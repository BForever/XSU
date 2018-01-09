#include <driver/vga.h>
#include <xsu/bootmm.h>
#include <xsu/buddy.h>
#include <xsu/list.h>
#include <xsu/lock.h>
#include <xsu/utils.h>

unsigned int kernel_start_pfn, kernel_end_pfn;

struct page* pages; // the global variable to reserve all the pages
//???where the pointer is reserved
struct buddy_sys buddy;

// void set_bplevel(struct page* bp, unsigned int bplevel)
//{
//	bp->bplevel = bplevel;
//}
void buddy_list_add(struct list_head* new, struct list_head* head, int level)
{
    //FreeList's head, without value!
    struct page* newPage;
    newPage = container_of(new, struct page, list);
    unsigned int newPageNumber = newPage - pages;
    if (head->prev == head) {
        list_add(new, head);
        return;
    }
    if (!level) {
        // the lowest level---->1 page size(4KB)
        // the relative-small memory, which we deemed it a up order based on the address
        int flag = 0;
        struct list_head* tempList = head->next;
        struct list_head* endList = head->prev;
        struct page* tmp;
        struct page* end;
        tmp = container_of(head->next, struct page, list);
        end = container_of(head->prev, struct page, list);
        while (tmp != end) {
            unsigned int currentPageNumber = tmp - pages;
            if (currentPageNumber >= newPageNumber) {
                flag = 1;
                list_add(new, tempList->prev);
                break;
            }
            tempList = tempList->next;
            tmp = container_of(tempList, struct page, list);
        }
        if (!flag) {
            unsigned int currentPageNumber = end - pages;
            if (currentPageNumber >= newPageNumber) {
                flag = 1;
                list_add(new, endList->prev);
            } else {
                flag = 1;
                list_add(new, endList);
            }
        }

    } else if (level > 0) {
        // the relative-large memory, which we deemed it a descending order based on the address
        int flag = 0;
        struct list_head* tempList = head->next;
        struct list_head* endList = head->prev;
        struct page* tmp;
        struct page* end;
        tmp = container_of(head->next, struct page, list);
        end = container_of(head->prev, struct page, list);
        while (tmp != end) {
            unsigned int currentPageNumber = tmp - pages;
            if (currentPageNumber <= newPageNumber) {
                flag = 1;
                list_add(new, tempList->prev);
                break;
            }
            tempList = tempList->next;
            tmp = container_of(tempList, struct page, list);
        }
        if (!flag) {
            unsigned int currentPageNumber = end - pages;
            if (currentPageNumber <= newPageNumber) {
                flag = 1;
                list_add(new, endList->prev);
            } else {
                flag = 1;
                list_add(new, endList);
            }
        }
    }
}

void get_buddy_allocation_state()
{
    unsigned int totalMemory = buddy.buddy_end_pfn - buddy.buddy_start_pfn;
    totalMemory <<= 2;
    unsigned int residual = 0;
    unsigned int index = 0;
    unsigned int singleElementSize = 4;
    for (index = 0; index <= MAX_BUDDY_ORDER; ++index) {
        residual += singleElementSize * (buddy.freeList[index].nr_free);
        singleElementSize <<= 1;
    }
    kernel_printf("MemRegions: %d total, %dK residual.\n", totalMemory, residual);
}

void buddy_info()
{
    unsigned int index;
    kernel_printf("Buddy-system :\n");
    kernel_printf("\tstart page-frame number : %x\n", buddy.buddy_start_pfn);
    kernel_printf("\tend page-frame number : %x\n", buddy.buddy_end_pfn);
    for (index = 0; index <= MAX_BUDDY_ORDER; ++index) {
        kernel_printf("\t(%x)# : %x frees\n", index, buddy.freeList[index].nr_free);
    }
}

// this function is to init all memory with page struct
void init_pages(unsigned int start_pfn, unsigned int end_pfn)
{
    unsigned int i;
    for (i = start_pfn; i < end_pfn; i++) {
        clean_flag(pages + i);
        set_flag(pages + i, _PAGE_ALLOCED);
        (pages + i)->reference = 1;
        (pages + i)->virtual = (void*)(-1);
        (pages + i)->bplevel = (-1);
        (pages + i)->slabp = 0; // initially, the free space is the whole page
        INIT_LIST_HEAD(&(pages[i].list));
    }
}
// return value:
// 0 represent the false, in different buddy
// 1 represent the true, in the same buddy
int judge_in_same_buddy(struct page* judgePage, int pageLevel, int pageIndex){
    int flag = 1;
    unsigned int buddyGroupIndex;
    struct page* buddyGroupPage;
    buddyGroupIndex = pageIndex ^ (1 << pageLevel);
    buddyGroupPage = judgePage + (buddyGroupIndex - pageIndex);
    // kernel_printf("group%x %x\n", (pageIndex), buddyGroupIndex);
    if (!_is_same_bplevel(buddyGroupPage, pageLevel)) {
        // kernel_printf("%x %x\n", buddyGroupPage->bplevel, bplevel);
        flag = 0;
        return flag;
    }
    // original bug!!
    if (buddyGroupPage->flag == _PAGE_ALLOCED) {
        // another memory has been allocated.
        flag = 0;
        kernel_printf("the buddy memory has been allocated\n");
        return flag;
    }
    return flag;    
}

void init_buddy()
{
    unsigned int bpsize = sizeof(struct page); //base_page's size
    unsigned char* bp_base;
    unsigned int i;

    // this function is to allocate enough spaces for Page_Frame_Space
    // all the memory is included, the kernel space is also allocated memory space
    bp_base = bootmm_alloc_pages(bpsize * bmm.maxPhysicalFrameNumber, _MM_KERNEL, 1 << PAGE_SHIFT);

    if (!bp_base) {
        // the remaining memory must be large enough to allocate the whole group
        // of buddy page struct
        kernel_printf("\nERROR : bootmm_alloc_pages failed!\nInit buddy system failed!\n");
        while (1)
            ;
    }

    // ?????, this code is used for?
    pages = (struct page*)((unsigned int)bp_base | 0x80000000);

    init_pages(0, bmm.maxPhysicalFrameNumber); //from pages[0] to pages[n]

    kernel_start_pfn = 0;
    kernel_end_pfn = 0;
    for (i = 0; i < bmm.countInfos; ++i) {
        if (bmm.info[i].endFramePtr > kernel_end_pfn)
            kernel_end_pfn = bmm.info[i].endFramePtr;
    }
    kernel_end_pfn >>= PAGE_SHIFT;

    //?????, why the MAX_BUDDY_ORDER
    buddy.buddy_start_pfn = (kernel_end_pfn + (1 << MAX_BUDDY_ORDER) - 1) & ~((1 << MAX_BUDDY_ORDER) - 1); // the pages that bootmm using cannot be merged into buddy_sys
    buddy.buddy_end_pfn = bmm.maxPhysicalFrameNumber & ~((1 << MAX_BUDDY_ORDER) - 1); // remain 2 pages for I/O
    //both the start and end pfn is the page_frame_number

    // init FreeLists of all bplevels
    for (i = 0; i < MAX_BUDDY_ORDER + 1; i++) {
        buddy.freeList[i].nr_free = 0;
        INIT_LIST_HEAD(&(buddy.freeList[i].free_head));
    }
    buddy.start_page = pages + buddy.buddy_start_pfn;
    init_lock(&(buddy.lock));

    for (i = buddy.buddy_start_pfn; i < buddy.buddy_end_pfn; ++i) {
        __free_pages(pages + i, 0);
    }
}
/* This function is to free the pages
 * @pbpage: the first page's pointer which will be freed 
 * @bplevle: the order of the page block(represent size).
 */
void __free_pages(struct page* pbpage, unsigned int bplevel)
{
    /* pageIndex -> the current page
     * buddyGroupIndex -> the buddy group that current page is in
     */
    unsigned int pageIndex;
    unsigned int buddyGroupIndex;
    unsigned int combinedIndex, tmp;
    struct page* buddyGroupPage;
    unsigned int test = 0;
    //kernel_printf("buddy_free.\n");
    // dec_ref(pbpage, 1);
    // if(pbpage->reference)
    //	return;
    if (!(has_flag(pbpage, _PAGE_ALLOCED) || has_flag(pbpage, _PAGE_SLAB))) {
        //if(!(has_flag(pbpage, _PAGE_ALLOCED))){
        //original bug!!
        kernel_printf("kfree_again. \n");
        return;
    } else if (has_flag(pbpage, _PAGE_SLAB)) {
        // kernel_printf("kfree slab page. \n");
        // test = 1;
        // kernel_printf("page number is: %x", pbpage);
    }
    // if(pbpage->bplevel != -1){
    //     kernel_printf("kfree_again.\n");
    //     return;
    // }
    //kernel_printf("free_bplevel is: %x", pbpage->bplevel);
    lockup(&buddy.lock);
    set_flag(pbpage, _PAGE_RESERVED);
    pageIndex = pbpage - buddy.start_page;
    // complier do the sizeof(struct) operation, and now pageIndex is the index
    while (bplevel < MAX_BUDDY_ORDER) {
        if(!judge_in_same_buddy(pbpage, bplevel, pageIndex))
            break;
        buddyGroupIndex = pageIndex ^ (1 << bplevel);
        buddyGroupPage = pbpage + (buddyGroupIndex - pageIndex);
        // kernel_printf("group%x %x\n", (pageIndex), buddyGroupIndex);
        // if (!_is_same_bplevel(buddyGroupPage, bplevel)) {
        //     // kernel_printf("%x %x\n", buddyGroupPage->bplevel, bplevel);

        //     break;
        // }
        // // original bug!!
        // if (buddyGroupPage->flag == _PAGE_ALLOCED) {
        //     // another memory has been allocated.
        //     kernel_printf("the buddy memory has been allocated\n");
        //     break;
        // }
        list_del_init(&buddyGroupPage->list);
        --buddy.freeList[bplevel].nr_free;
        set_bplevel(buddyGroupPage, -1);
        combinedIndex = buddyGroupIndex & pageIndex;
        pbpage += (combinedIndex - pageIndex);
        pageIndex = combinedIndex;
        ++bplevel;
    }
    // if(test){
    //     kernel_printf("slab page free nearly finished!\n");
    // }
    set_bplevel(pbpage, bplevel);
    //    list_add(&(pbpage->list), &(buddy.FreeList[bplevel].free_head));
    //kernel_printf("final free_bplevel is: %x", bplevel)
    buddy_list_add(&(pbpage->list), &(buddy.freeList[bplevel].free_head), bplevel);
    // if(test){
    //     kernel_printf("slab page free nearly finished!\n");
    // }
    ++buddy.freeList[bplevel].nr_free;
    // if(test){
    //     kernel_printf("slab page free finished!\n");
    // }
    // kernel_printf("v%x__addto__%x\n", &(pbpage->list),
    // &(buddy.FreeList[bplevel].free_head));
    unlock(&buddy.buddyLock);
}

struct page* __alloc_pages(unsigned int bplevel)
{
    unsigned int current_order, size;
    struct page *page, *buddy_page;
    struct FreeList* free;

    lockup(&buddy.buddyLock);

    for (current_order = bplevel; current_order <= MAX_BUDDY_ORDER; ++current_order) {
        free = buddy.freeList + current_order;
        if (!list_empty(&(free->free_head)))
            goto found;
    }

    unlock(&buddy.buddyLock);
    return 0;

found:
    if (current_order != bplevel) {
        // alloc from the samll part
        page = container_of(free->free_head.prev, struct page, list);

    } else {
        page = container_of(free->free_head.next, struct page, list);
    }
    list_del_init(&(page->list));
    set_bplevel(page, bplevel);
    set_flag(page, _PAGE_ALLOCED);
    // set_ref(page, 1);
    --(free->nr_free);

    size = 1 << current_order;
    while (current_order > bplevel) {
        --free;
        --current_order;
        size >>= 1;
        buddy_page = page + size;
        //        list_add(&(buddy_page->list), &(free->free_head));
        buddy_list_add(&(buddy_page->list), &(free->free_head), current_order);
        ++(free->nr_free);
        set_bplevel(buddy_page, current_order);
        set_flag(buddy_page, _PAGE_RESERVED);
    }

    unlock(&buddy.buddyLock);
    return page;
}

void* alloc_pages(unsigned int bplevel)
{
    struct page* page = __alloc_pages(bplevel);

    if (!page)
        return 0;

    return (void*)((page - pages) << PAGE_SHIFT);
}

void free_pages(void* addr, unsigned int bplevel)
{
    //kernel_printf("kfree: %x, size = %x \n", addr, bplevel);
    __free_pages(pages + ((unsigned int)addr >> PAGE_SHIFT), bplevel);
}
