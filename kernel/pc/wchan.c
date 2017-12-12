#include <xsu/slab.h>
#include <xsu/wchan.h>

/*
 * Wait channel functions
 */

/*
 * Create a wait channel. NAME is a symbolic string name for it.
 * This is what's displayed by ps -alx in Unix.
 *
 * NAME should generally be a string constant. If it isn't, alternate
 * arrangements should be made to free it after the wait channel is
 * destroyed.
 */
struct wchan* wchan_create(const char* name)
{
    struct wchan* wc;

    wc = kmalloc(sizeof(*wc));
    if (wc == NULL) {
        return NULL;
    }
    init_lock(&wc->wc_lock);
    threadlist_init(&wc->wc_threads);
    wc->wc_name = name;
    return wc;
}

/*
 * Destroy a wait channel. Must be empty and unlocked.
 * (The corresponding cleanup functions require this.)
 */
void wchan_destroy(struct wchan* wc)
{
    cleanup_lock(&wc->wc_lock);
    threadlist_cleanup(&wc->wc_threads);
    kfree(wc);
}

/*
 * Lock and unlock a wait channel, respectively.
 */
void wchan_lock(struct wchan* wc)
{
    assert(lockup(&wc->wc_lock) == 1, "lock failed");
}

void wchan_unlock(struct wchan* wc)
{
    assert(unlock(&wc->wc_lock) == 1, "unlock failed");
}

/*
 * Yield the cpu to another process, and go to sleep, on the specified
 * wait channel WC. Calling wakeup on the channel will make the thread
 * runnable again. The channel must be locked, and will be *unlocked*
 * upon return.
 */
void wchan_sleep(struct wchan* wc)
{
    /* may not sleep in an interrupt handler */
    assert(!curthread->t_in_interrupt, "Current thread is handling an interrupt");

    thread_switch(S_SLEEP, wc);
}

/*
 * Wake up one thread sleeping on a wait channel.
 */
void wchan_wakeone(struct wchan* wc)
{
    struct thread* target;

    /* Lock the channel and grab a thread from it */
    assert(lockup(&wc->wc_lock) == 1, "lock failed");
    target = threadlist_remhead(&wc->wc_threads);
    /*
	 * Nobody else can wake up this thread now, so we don't need
	 * to hang onto the lock.
	 */
    assert(unlock(&wc->wc_lock) == 1, "unlock failed");

    if (target == NULL) {
        /* Nobody was sleeping. */
        return;
    }

    thread_make_runnable(target, false);
}

/*
 * Wake up all threads sleeping on a wait channel.
 */
void wchan_wakeall(struct wchan* wc)
{
    struct thread* target;
    struct threadlist list;

    threadlist_init(&list);

    /*
	 * Lock the channel and grab all the threads, moving them to a
	 * private list.
	 */
    assert(lockup(&wc->wc_lock) == 1, "lock failed");
    while ((target = threadlist_remhead(&wc->wc_threads)) != NULL) {
        threadlist_addtail(&list, target);
    }
    /*
	 * Nobody else can wake up these threads now, so we don't need
	 * to hang onto the lock.
	 */
    assert(unlock(&wc->wc_lock) == 1, "unlock failed");

    /*
	 * We could conceivably sort by cpu first to cause fewer lock
	 * ops and fewer IPIs, but for now at least don't bother. Just
	 * make each thread runnable.
	 */
    while ((target = threadlist_remhead(&list)) != NULL) {
        thread_make_runnable(target, false);
    }

    threadlist_cleanup(&list);
}

/*
 * Return nonzero if there are no threads sleeping on the channel.
 * This is meant to be used only for diagnostic purposes.
 */
bool wchan_isempty(struct wchan* wc)
{
    bool ret;

    assert(lockup(&wc->wc_lock) == 1, "lock failed");
    ret = threadlist_isempty(&wc->wc_threads);
    assert(unlock(&wc->wc_lock) == 1, "unlock failed");

    return ret;
}
