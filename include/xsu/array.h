#ifndef _XSU_ARRAY_H
#define _XSU_ARRAY_H

#include <assert.h>

struct array {
    void** v;
    unsigned num, max;
};

/*
 * Base array type (resizeable array of void pointers) and operations.
 *
 * create - allocate an array.
 * destroy - destroy an allocated array.
 * init - initialize an array in space externally allocated.
 * cleanup - clean up an array in space externally allocated.
 * num - return number of elements in array.
 * get - return element no. INDEX.
 * set - set element no. INDEX to VAL.
 * setsize - change size to NUM elements; may fail and return error.
 * add - append VAL to end of array; return its index in INDEX_RET if
 *       INDEX_RET isn't null; may fail and return error.
 * remove - excise entry INDEX and slide following entries down to
 *       close the resulting gap.
 *
 * Note that expanding an array with setsize doesn't initialize the new
 * elements. (Usually the caller is about to store into them anyway.)
 */

struct array* array_create(void);
void array_destroy(struct array*);
void array_init(struct array*);
void array_cleanup(struct array*);
unsigned array_num(const struct array*);
void* array_get(const struct array*, unsigned index);
void array_set(const struct array*, unsigned index, void* val);
int array_setsize(struct array*, unsigned num);
int array_add(struct array*, void* val, unsigned* index_ret);
void array_remove(struct array*, unsigned index);

#endif
