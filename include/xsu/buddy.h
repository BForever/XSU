#ifndef _XSU_BUDDY_H
#define _XSU_BUDDY_H

#include <xsu/list.h>
#include <xsu/lock.h>

#define _PAGE_RESERVED (1 << 31)
#define _PAGE_ALLOCED (1 << 30)
#define _PAGE_SLAB (1 << 29)

/*
 * struct buddy page is one info-set for the buddy group of pages
 */
struct page {
    /* if the page is used by slab system,
     * then slabFreeSpacePtr represents the base-addr of free space
     * in this way, slab can get the next allocated block's address
     */
    void** slabFreeSpacePtr; 
    // the declaration of the usage of this page _PAGE_RESERVED, _PAGE_ALLOCED, _PAGE_SLAB
    unsigned int flag; 
    // this value hasn't been used    
    struct list_head list; // double-way list
    void* pageCacheBlock; // default 0x(-1)
    /* the order level of the page
     * when the page is allocated or in free element's first page, the orderlevel is the current buddy's level
     * but if the page is part of a larger buddy's element(such as the second page), the orderlevel is -1 
     */
    unsigned int pageOrderLevel; 


    //unsigned int reference; 
};

#define PAGE_SHIFT 12
/*
 * order means the size of the set of pages, e.g. order = 1 -> 2^1
 * pages(consequent) are free In current system, we allow the max order to be
 * 4(2^4 consequent free pages)
 */
#define MAX_BUDDY_ORDER 4

struct FreeList {
    unsigned int freeNumer;
    struct list_head freeHead;
};

struct buddy_sys {
    unsigned int buddyStartPageNumber;
    unsigned int buddyEndPageNumber;
    struct page* startPagePtr;
    struct lock_t buddyLock;
    struct FreeList freeList[MAX_BUDDY_ORDER + 1];
};
// two page is in the same level, in other words in the same group
//#define _is_same_bpgroup(firstPage, secondPage) (((*(firstPage)).pageOrderLevel == (*(secondPage)).pageOrderLevel))
// two page is in the same level, in other words in the same group 
#define _is_same_pageOrderLevel(page, levelValue) ((*(page)).pageOrderLevel == (levelValue))
// set page's level value
#define set_pageOrderLevel(page, levelValue) ((*(page)).pageOrderLevel = (levelValue))
// set flag's value
#define set_flag(page, flagValue) ((*(page)).flag = (flagValue))
// clean flag's value
#define clean_flag(page) ((*(page)).flag = 0)
// judge whether they have the same flag value
#define has_flag(page, flagValue) ((*(page)).flag & flagValue)
// set reference value
#define set_ref(page, referenceValue) ((*(page)).reference = (referenceValue))
// increase reference value
#define inc_ref(page, incValue) ((*(page)).reference += (incValue))
// decrease reference value
#define dec_ref(page, decValue) ((*(page)).reference -= (decValue))

extern struct page* pages;
extern struct buddy_sys buddy;

extern void __free_pages(struct page* page, unsigned int order);
extern struct page* __alloc_pages(unsigned int order);

extern void free_pages(void* addr, unsigned int order);

extern void* alloc_pages(unsigned int order);

extern void init_buddy();

extern void buddy_info();

extern void get_buddy_allocation_state();

#endif
