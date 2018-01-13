#include <driver/vga.h>
#include <xsu/bootmm.h>
#include <xsu/buddy.h>
#include <xsu/list.h>
#include <xsu/lock.h>
#include <xsu/utils.h>

unsigned int kernel_startPhysicalFrameNumber, kernel_endPhysicalFrameNumber;

struct page* pages; // the global variable to reserve all the pages
//???where the pointer is reserved
struct buddy_sys buddy;

/* FUNC@:add buddy element to the list
 * insert into in a decending or increasing order accoring to the freelist's level
 * if the level is 0, which represent the small memory, then, it will insert in a increasing order
 * else if the level is 1, which represent the large memory, then, it will insert in a decreasing order
 * INPUT:
 *  @para new: the pointer represent newly inserted list_node
 *  @para head: the list's head pointer
 *  @para level: the buddy list's level, decide the order
 * RETURN:
 *
 */
void buddy_list_add(struct list_head* new, struct list_head* head, int level)
{
    //FreeList's head, without value!
    struct page* newPage;
    //use the define to get the page
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
            // search the list to find the best place to insert into a list
            unsigned int currentPageNumber = tmp - pages;
            if (currentPageNumber >= newPageNumber) {
            // find the best place to insert
                flag = 1;
                list_add(new, tempList->prev);
                break;
            }
            tempList = tempList->next;
            tmp = container_of(tempList, struct page, list);
        }
        if (!flag) {
            // flag is represent whether find the best place,if find it,
            // then insert into that place.
            // if not find, then insert into the head/end of the list, 
            // according to the pagenumber 
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
            // the same as before
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
/* FUNC@:get buddy system's allocation state
 * according to the allocations state, compute the memory use state.
 * INPUT:
 * RETURN:
 */
void get_buddy_allocation_state()
{
    // totalMemory is the max physical memory, a fix number
    unsigned int totalMemory = buddy.buddyEndPageNumber - buddy.buddyStartPageNumber;
    totalMemory <<= 2;// 4K every page
    // compute the residual memory
    unsigned int residual = 0;
    unsigned int index = 0;
    unsigned int singleElementSize = 4;
    for (index = 0; index <= MAX_BUDDY_ORDER; ++index) {
        // search the freelist to compute the residual memory
        residual += singleElementSize * (buddy.freeList[index].freeNumer);
        singleElementSize <<= 1;
    }
    kernel_printf("MemRegions: %d total, %dK residual.\n", totalMemory, residual);
}
/* FUNC@:get buddy system's allocation info
 * more specifically than the get_buddy_allocation_state function
 * INPUT:
 * RETURN:
 */
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
/* FUNC@: this function is to init all memory with page struct
 * INPUT:
 * @para startPhysicalFrameNumber: the literal meaning
 * @para endPhysicalFrameNumber: the literal meaning
 * RETURN:
 */
void init_pages(unsigned int startPhysicalFrameNumber, unsigned int endPhysicalFrameNumber)
{
    unsigned int i;
    for (i = startPhysicalFrameNumber; i < endPhysicalFrameNumber; i++) {
        clean_flag(pages + i);
        set_flag(pages + i, _PAGE_ALLOCED);
        (pages + i)->reference = 1;
        (pages + i)->pageCacheBlock = (void*)(-1);
        (pages + i)->pageOrderLevel = (-1);// initial state
        (pages + i)->slabFreeSpacePtr = 0; // initially, the free space is the whole page
        INIT_LIST_HEAD(&(pages[i].list));
    }
}
/* FUNC@: this function is to judge whether in the same buddy
 * Usually, it will judge two condition.
 * 1. whether the buddy page is in the same order level
 * 2. whether the buddy page has been allocated
 * the third condition is about the address match.
 * INPUT:
 * @para judgePage: the page which need to be judged
 * @para pageLevel: the literal meaning
 * @para pageIndex:
 * RETURN:
 *  return value:
 * 0 represent the false, in different buddy
 * 1 represent the true, in the same buddy
 */
int judge_in_same_buddy(struct page* judgePage, int pageLevel, int pageIndex){
    int flag = 1; // flag represent the judged result.
    unsigned int buddyGroupIndex;
    struct page* buddyGroupPage;
    buddyGroupIndex = pageIndex ^ (1 << pageLevel);
    buddyGroupPage = judgePage + (buddyGroupIndex - pageIndex);
    // kernel_printf("group%x %x\n", (pageIndex), buddyGroupIndex);
    if (!_is_same_pageOrderLevel(buddyGroupPage, pageLevel)) {
        // condition1: judge the order level
        // kernel_printf("%x %x\n", buddyGroupPage->pageOrderLevel, pageOrderLevel);
        flag = 0;
        return flag;
    }
    // original bug!!
    if (buddyGroupPage->flag == _PAGE_ALLOCED) {
        // condition2: judge the allocation state.
        // another memory has been allocated.
        flag = 0;
        kernel_printf("the buddy memory has been allocated\n");
        return flag;
    }
    return flag;    
}
/* FUNC@: inti the buddy systemn
 * this function will initialize every buddy page.
 * the "init_pages" function will be called.
 * and the "_free_pages" function will be called.
 * INPUT:
 * RETURN:
 */
void init_buddy()
{
    //base_page's size
    unsigned int basePageSize = sizeof(struct page); 
    unsigned char* bp_base;
    unsigned int i;

    // this function is to allocate enough spaces for Page_Frame_Space
    // all the memory is included, the kernel space is also allocated memory space
    bp_base = bootmm_alloc_pages(basePageSize * bmm.maxPhysicalFrameNumber, _MM_KERNEL, 1 << PAGE_SHIFT);

    if (!bp_base) {
        // the remaining memory must be large enough to allocate the whole group
        // of buddy page struct
        kernel_printf("\nERROR : bootmm_alloc_pages failed!\nInit buddy system failed!\n");
        while (1)
            ;
    }

    // the user's space is start from the 0x80000000
    pages = (struct page*)((unsigned int)bp_base | 0x80000000);

    init_pages(0, bmm.maxPhysicalFrameNumber); //from pages[0] to pages[n]
    //initialization
    kernel_startPhysicalFrameNumber = 0;
    kernel_endPhysicalFrameNumber = 0;
    for (i = 0; i < bmm.countInfos; ++i) {
    // get the kernel's endFramePtr by searching all the list
        if (bmm.info[i].endFramePtr > kernel_endPhysicalFrameNumber)
            kernel_endPhysicalFrameNumber = bmm.info[i].endFramePtr;
    }
    // transform to the page number
    kernel_endPhysicalFrameNumber >>= PAGE_SHIFT;

    // page allign
    buddy.buddyStartPageNumber = (kernel_endPhysicalFrameNumber + (1 << MAX_BUDDY_ORDER) - 1) & ~((1 << MAX_BUDDY_ORDER) - 1); // the pages that bootmm using cannot be merged into buddy_sys
    buddy.buddyEndPageNumber = bmm.maxPhysicalFrameNumber & ~((1 << MAX_BUDDY_ORDER) - 1); // remain 2 pages for I/O
    //both the start and end pfn is the page_frame_number

    // init FreeLists of all pageOrderLevels
    for (i = 0; i < MAX_BUDDY_ORDER + 1; i++) {
        buddy.freeList[i].freeNumer = 0;
        INIT_LIST_HEAD(&(buddy.freeList[i].freeHead));
    }
    buddy.startPagePtr = pages + buddy.buddyStartPageNumber;
    init_lock(&(buddy.lock));

    // free all the page to transform the memory control to the buddy system
    for (i = buddy.buddyStartPageNumber; i < buddy.buddyEndPageNumber; ++i) {
        __free_pages(pages + i, 0);
    }
}
/* FUNC@: This function is to free the pages
 * it will call the judge_in_same_buddy function.
 * INPUT:
 *  pbpage: the first page's pointer which will be freed
 *  pageOrderLevel: the free page's order level
 * RETURN:
 */
void __free_pages(struct page* pbpage, unsigned int pageOrderLevel)
{
    //pageIndex -> the current page
    //buddyGroupIndex -> the buddy group that current page is in
    
    unsigned int pageIndex;
    unsigned int buddyGroupIndex;
    unsigned int combinedIndex, tmp;
    struct page* buddyGroupPage;
    unsigned int test = 0;
    //debug code:
    //kernel_printf("buddy_free.\n");
    // dec_ref(pbpage, 1);
    // if(pbpage->reference)
    //	return;
    if (!(has_flag(pbpage, _PAGE_ALLOCED) || has_flag(pbpage, _PAGE_SLAB))) {
        //judge the page, whether it will be freed,if allocated or allocated by slub, then free it.
        //original bug!!, original didn't free.
        kernel_printf("kfree_again. \n");
        return;
    } else if (has_flag(pbpage, _PAGE_SLAB)) {
#ifdef BUDDY_DEBUG 
        // judge whether free the slab page   
        kernel_printf("kfree slab page. \n");
        test = 1;
        kernel_printf("page number is: %x", pbpage);
#endif
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
