#include "pthreadP.h"
#include "mypthreadtypes.h"

int
pthread_cond_destroy (cond)
        pthread_cond_t *cond;
{
    my_pthread_cond_t * mycond = (my_pthread_cond_t*) cond;
    int pshared = (mycond->__data.__mutex == (void *) ~0l)
                  ? LLL_SHARED : LLL_PRIVATE;

    /* Make sure we are alone.  */
    lll_lock (mycond->__data.__lock, pshared);

    if (mycond->__data.__total_seq > mycond->__data.__wakeup_seq)
    {
        /* If there are still some waiters which have not been
       woken up, this is an application bug.  */
        lll_unlock (mycond->__data.__lock, pshared);
        return EBUSY;
    }

    /* Tell pthread_cond_*wait that this condvar is being destroyed.  */
    mycond->__data.__total_seq = -1ULL;

    /* If there are waiters which have been already signalled or
       broadcasted, but still are using the pthread_cond_t structure,
       pthread_cond_destroy needs to wait for them.  */
    unsigned int nwaiters = mycond->__data.__nwaiters;

    if (nwaiters >= (1 << COND_NWAITERS_SHIFT))
    {
        /* Wake everybody on the associated mutex in case there are
           threads that have been requeued to it.
           Without this, pthread_cond_destroy could block potentially
           for a long time or forever, as it would depend on other
           thread's using the mutex.
           When all threads waiting on the mutex are woken up, pthread_cond_wait
           only waits for threads to acquire and release the internal
           condvar lock.  */
        if (mycond->__data.__mutex != NULL
            && mycond->__data.__mutex != (void *) ~0l)
        {
            pthread_mutex_t *mut = (pthread_mutex_t *) mycond->__data.__mutex;
            lll_futex_wake (&mut->__data.__lock, INT_MAX,
                            PTHREAD_MUTEX_PSHARED (mut));
        }

        do
        {
            lll_unlock (mycond->__data.__lock, pshared);

            lll_futex_wait (&mycond->__data.__nwaiters, nwaiters, pshared);

            lll_lock (mycond->__data.__lock, pshared);

            nwaiters = mycond->__data.__nwaiters;
        }
        while (nwaiters >= (1 << COND_NWAITERS_SHIFT));
    }

    return 0;
}