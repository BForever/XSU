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

// void set_pageOrderLevel(struct page* bp, unsigned int pageOrderLevel)
//{
//	bp->pageOrderLevel = pageOrderLevel;
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
    unsigned int totalMemory = buddy.buddyEndPageNumber - buddy.buddyStartPageNumber;
    totalMemory <<= 2;
    unsigned int residual = 0;
    unsigned int index = 0;
    unsigned int singleElementSize = 4;
    for (index = 0; index <= MAX_BUDDY_ORDER; ++index) {
        residual += singleElementSize * (buddy.freeList[index].freeNumer);
        singleElementSize <<= 1;
    }
    kernel_printf("MemRegions: %d total, %dK residual.\n", totalMemory, residual);
}

void buddy_info()
{
    unsigned int index;
    kernel_printf("Buddy-system :\n");
    kernel_printf("\tstart page-frame number : %x\n", buddy.buddyStartPageNumber);
    kernel_printf("\tend page-frame number : %x\n", buddy.buddyEndPageNumber);
    for (index = 0; index <= MAX_BUDDY_ORDER; ++index) {
        kernel_printf("\t(%x)# : %x frees\n", index, buddy.freeList[index].freeNumer);
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
        (pages + i)->pageCacheBlock = (void*)(-1);
        (pages + i)->pageOrderLevel = (-1);
        (pages + i)->slabFreeSpacePtr = 0; // initially, the free space is the whole page
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
    if (!_is_same_pageOrderLevel(buddyGroupPage, pageLevel)) {
        // kernel_printf("%x %x\n", buddyGroupPage->pageOrderLevel, pageOrderLevel);
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
    buddy.buddyStartPageNumber = (kernel_end_pfn + (1 << MAX_BUDDY_ORDER) - 1) & ~((1 << MAX_BUDDY_ORDER) - 1); // the pages that bootmm using cannot be merged into buddy_sys
    buddy.buddyEndPageNumber = bmm.maxPhysicalFrameNumber & ~((1 << MAX_BUDDY_ORDER) - 1); // remain 2 pages for I/O
    //both the start and end pfn is the page_frame_number

    // init FreeLists of all pageOrderLevels
    for (i = 0; i < MAX_BUDDY_ORDER + 1; i++) {
        buddy.freeList[i].freeNumer = 0;
        INIT_LIST_HEAD(&(buddy.freeList[i].freeHead));
    }
    buddy.startPagePtr = pages + buddy.buddyStartPageNumber;
    init_lock(&(buddy.lock));

    for (i = buddy.buddyStartPageNumber; i < buddy.buddyEndPageNumber; ++i) {
        __free_pages(pages + i, 0);
    }
}
/* This function is to free the pages
 * @pbpage: the first page's pointer which will be freed 
 * @bplevle: the order of the page block(represent size).
 */
void __free_pages(struct page* pbpage, unsigned int pageOrderLevel)
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
    // if(pbpage->pageOrderLevel != -1){
    //     kernel_printf("kfree_again.\n");
    //     return;
    // }
    //kernel_printf("free_pageOrderLevel is: %x", pbpage->pageOrderLevel);
    lockup(&buddy.lock);
    set_flag(pbpage, _PAGE_RESERVED);
    pageIndex = pbpage - buddy.startPagePtr;
    // complier do the sizeof(struct) operation, and now pageIndex is the index
    while (pageOrderLevel < MAX_BUDDY_ORDER) {
        if(!judge_in_same_buddy(pbpage, pageOrderLevel, pageIndex))
            break;
        buddyGroupIndex = pageIndex ^ (1 << pageOrderLevel);
        buddyGroupPage = pbpage + (buddyGroupIndex - pageIndex);
        // kernel_printf("group%x %x\n", (pageIndex), buddyGroupIndex);
        // if (!_is_same_pageOrderLevel(buddyGroupPage, pageOrderLevel)) {
        //     // kernel_printf("%x %x\n", buddyGroupPage->pageOrderLevel, pageOrderLevel);

        //     break;
        // }
        // // original bug!!
        // if (buddyGroupPage->flag == _PAGE_ALLOCED) {
        //     // another memory has been allocated.
        //     kernel_printf("the buddy memory has been allocated\n");
        //     break;
        // }
        list_del_init(&buddyGroupPage->list);
        --buddy.freeList[pageOrderLevel].freeNumer;
        set_pageOrderLevel(buddyGroupPage, -1);
        combinedIndex = buddyGroupIndex & pageIndex;
        pbpage += (combinedIndex - pageIndex);
        pageIndex = combinedIndex;
        ++pageOrderLevel;
    }
    // if(test){
    //     kernel_printf("slab page free nearly finished!\n");
    // }
    set_pageOrderLevel(pbpage, pageOrderLevel);
    //    list_add(&(pbpage->list), &(buddy.FreeList[pageOrderLevel].freeHead));
    //kernel_printf("final free_pageOrderLevel is: %x", pageOrderLevel)
    buddy_list_add(&(pbpage->list), &(buddy.freeList[pageOrderLevel].freeHead), pageOrderLevel);
    // if(test){
    //     kernel_printf("slab page free nearly finished!\n");
    // }
    ++buddy.freeList[pageOrderLevel].freeNumer;
    // if(test){
    //     kernel_printf("slab page free finished!\n");
    // }
    // kernel_printf("v%x__addto__%x\n", &(pbpage->list),
    // &(buddy.FreeList[pageOrderLevel].freeHead));
    unlock(&buddy.buddyLock);
}

struct page* __alloc_pages(unsigned int pageOrderLevel)
{
    unsigned int current_order, size;
    struct page *page, *buddyPage;
    struct FreeList* free;

    lockup(&buddy.buddyLock);

    for (current_order = pageOrderLevel; current_order <= MAX_BUDDY_ORDER; ++current_order) {
        free = buddy.freeList + current_order;
        if (!list_empty(&(free->freeHead)))
            goto found;
    }

    unlock(&buddy.buddyLock);
    return 0;

found:
    if (current_order != pageOrderLevel) {
        // alloc from the samll part
        page = container_of(free->freeHead.prev, struct page, list);

    } else {
        page = container_of(free->freeHead.next, struct page, list);
    }
    list_del_init(&(page->list));
    set_pageOrderLevel(page, pageOrderLevel);
    set_flag(page, _PAGE_ALLOCED);
    // set_ref(page, 1);
    --(free->freeNumer);

    size = 1 << current_order;
    while (current_order > pageOrderLevel) {
        --free;
        --current_order;
        size >>= 1;
        buddyPage = page + size;
        //        list_add(&(buddyPage->list), &(free->freeHead));
        buddy_list_add(&(buddyPage->list), &(free->freeHead), current_order);
        ++(free->freeNumer);
        set_pageOrderLevel(buddyPage, current_order);
        set_flag(buddyPage, _PAGE_RESERVED);
    }

    unlock(&buddy.buddyLock);
    return page;
}

void* alloc_pages(unsigned int pageOrderLevel)
{
    struct page* page = __alloc_pages(pageOrderLevel);

    if (!page)
        return 0;

    return (void*)((page - pages) << PAGE_SHIFT);
}

void free_pages(void* addr, unsigned int pageOrderLevel)
{
    //kernel_printf("kfree: %x, size = %x \n", addr, pageOrderLevel);
    __free_pages(pages + ((unsigned int)addr >> PAGE_SHIFT), pageOrderLevel);
}
