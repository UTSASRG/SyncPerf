#include "pthreadP.h"
#include "mypthreadtypes.h"

int
pthread_cond_init (cond, cond_attr)
        pthread_cond_t *cond;
        const pthread_condattr_t *cond_attr;
{
    struct pthread_condattr *icond_attr = (struct pthread_condattr *) cond_attr;

    my_pthread_cond_t * mycond = (my_pthread_cond_t*) cond;

    mycond->__data.__lock = LLL_LOCK_INITIALIZER;
    mycond->__data.__futex = 0;
    mycond->__data.__nwaiters = (icond_attr != NULL
                               ? ((icond_attr->value >> 1)
                                  & ((1 << COND_NWAITERS_SHIFT) - 1))
                               : CLOCK_REALTIME);
    mycond->__data.__total_seq = 0;
    mycond->__data.__wakeup_seq = 0;
    mycond->__data.__woken_seq = 0;
    mycond->__data.__mutex = (icond_attr == NULL || (icond_attr->value & 1) == 0
                            ? NULL : (void *) ~0l);
    mycond->__data.__broadcast_seq = 0;

    return 0;
}