#include <assert.h>
#include <xsu/lock.h>
#include <xsu/slab.h>
#include <xsu/synch.h>
#include <xsu/utils.h>
#include <xsu/wchan.h>

////////////////////////////////////////////////////////////
// Lock.
////////////////////////////////////////////////////////////
// TODO: How to record the current thread?

struct lock* lock_create(const char* name)
{
    struct lock* lock;

    lock = kmalloc(sizeof(struct lock));
    if (lock == NULL) {
        return NULL;
    }

    lock->lk_name = kernel_strdup(name);
    if (lock->lk_name == NULL) {
        kfree(lock);
        return NULL;
    }

    lock->lk_wchan = wchan_create(lock->lk_name);
    if (lock->lk_wchan == NULL) {
        kfree(lock->lk_name);
        kfree(lock);
        return NULL;
    }

    lock->lk_owner = NULL;
    assert(lockup(&lock->lk_lock), "lock failed");
    // add stuff here as needed

    return lock;
}

void lock_destroy(struct lock* lock)
{
    assert(lock != NULL, "lock is null");

    // add stuff here as needed
    cleanup_lock(&lock->lk_lock);
    if (lock->lk_wchan != NULL)
        wchan_destroy(lock->lk_wchan);
    kfree(lock->lk_name);
    kfree(lock);
}

void lock_acquire(struct lock* lock)
{
    // Write this
    assert(lock != NULL, "lock is null");

    assert(curthread->t_in_interrupt == false, "Current thread is handling an interrupt");
    assert(lockup(&lock->lk_lock) == 1, "lock failed");
    assert(!(lock_do_i_hold(lock)), "Current thread has already hold the lock");

    while (lock->lk_owner != NULL) {
        wchan_lock(lock->lk_wchan);
        assert(unlock(&lock->lk_lock) == 1, "unlock failed");
        // wchan_sleep(lock->lk_wchan);
        assert(lockup(&lock->lk_lock) == 1, "lock failed");
    }

    assert(lock->lk_owner == NULL, "Someone else has already hold the lock");
    lock->lk_owner = curthread;
    assert(unlock(&lock->lk_lock), "unlock failed");
}

void lock_release(struct lock* lock)
{
    // Write this
    assert(lock != NULL, "lock is null");
    assert(lockup(&lock->lk_lock), "lock failed");
    lock->lk_owner = NULL;
    wchan_wakeone(lock->lk_wchan);
    assert(unlock(&lock->lk_lock), "unlock failed");
}

bool lock_do_i_hold(struct lock* lock)
{
    // Write this
    if (lock->lk_owner == curthread)
        return true;
    return false;
}
