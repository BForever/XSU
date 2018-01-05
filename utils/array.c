#include <kern/errno.h>
#include <xsu/array.h>
#include <xsu/slab.h>
#include <xsu/utils.h>


/*
 * Base array operations.
 */
struct array* array_create(void)
{
    struct array* a;

    a = kmalloc(sizeof(*a));
    if (a != NULL) {
        array_init(a);
    }
    return a;
}

void array_destroy(struct array* a)
{
    array_cleanup(a);
    kfree(a);
}

void array_init(struct array* a)
{
    a->num = a->max = 0;
    a->v = NULL;
}

void array_cleanup(struct array* a)
{
    /*
	 * Require array to be empty - helps avoid memory leaks since
	 * we don't/can't free anything any contents may be pointing
	 * to.
	 */
    assert(a->num == 0, "Assertion failed on array cleanup");
    kfree(a->v);
#ifdef ARRAYS_CHECKED
    a->v = NULL;
#endif
}

unsigned array_num(const struct array* a)
{
    return a->num;
}

void* array_get(const struct array* a, unsigned index)
{
    assert(index < a->num, "Assertion failed on array get");
    return a->v[index];
}

void array_set(const struct array* a, unsigned index, void* val)
{
    (index < a->num, "Assertion failed on array set");
    a->v[index] = val;
}

int array_setsize(struct array* a, unsigned num)
{
    void** newptr;
    unsigned newmax;

    if (num > a->max) {
        /* Don't touch A until the allocation succeeds. */
        newmax = a->max;
        while (num > newmax) {
            newmax = newmax ? newmax * 2 : 4;
        }

        /*
		 * We don't have krealloc, and it wouldn't be
		 * worthwhile to implement just for this. So just
		 * allocate a new block and copy. (Exercise: what
		 * about this and/or kmalloc makes it not worthwhile?)
		 */

        newptr = kmalloc(newmax * sizeof(*a->v));
        if (newptr == NULL) {
            return ENOMEM;
        }
        kernel_memcpy(newptr, a->v, a->num * sizeof(*a->v));
        kfree(a->v);
        a->v = newptr;
        a->max = newmax;
    }
    a->num = num;

    return 0;
}

int array_add(struct array* a, void* val, unsigned* index_ret)
{
    unsigned index;
    int ret;

    index = a->num;
    ret = array_setsize(a, index + 1);
    if (ret) {
        return ret;
    }
    a->v[index] = val;
    if (index_ret != NULL) {
        *index_ret = index;
    }
    return 0;
}

void array_remove(struct array* a, unsigned index)
{
    unsigned num_to_move;

    assert(a->num <= a->max, "Assertion failed on array remove");
    assert(index < a->num, "Assertion failed on array remove");

    num_to_move = a->num - (index + 1);
    kernel_memmove(a->v + index, a->v + index + 1, num_to_move * sizeof(void*));
    a->num--;
}
