#include "pthreadP.h"
#include "mypthreadtypes.h"

int
pthread_cond_broadcast (cond)
pthread_cond_t *cond;
{
    my_pthread_cond_t * mycond = (my_pthread_cond_t*) cond;
int pshared = (mycond->__data.__mutex == (void *) ~0l)
              ? LLL_SHARED : LLL_PRIVATE;
/* Make sure we are alone.  */
lll_lock (mycond->__data.__lock, pshared);

/* Are there any waiters to be woken?  */
if (mycond->__data.__total_seq > mycond->__data.__wakeup_seq)
{
/* Yes.  Mark them all as woken.  */
mycond->__data.__wakeup_seq = mycond->__data.__total_seq;
mycond->__data.__woken_seq = mycond->__data.__total_seq;
mycond->__data.__futex = (unsigned int) mycond->__data.__total_seq * 2;
int futex_val = mycond->__data.__futex;
/* Signal that a broadcast happened.  */
++mycond->__data.__broadcast_seq;

/* We are done.  */
lll_unlock (mycond->__data.__lock, pshared);

/* Do not use requeue for pshared condvars.  */
if (mycond->__data.__mutex == (void *) ~0l)
goto wake_all;

/* Wake everybody.  */
pthread_mutex_t *mut = (pthread_mutex_t *) mycond->__data.__mutex;

/* XXX: Kernel so far doesn't support requeue to PI futex.  */
/* XXX: Kernel so far can only requeue to the same type of futex,
in this case private (we don't requeue for pshared condvars).  */
if (__builtin_expect (mut->__data.__kind
        & (PTHREAD_MUTEX_PRIO_INHERIT_NP
| PTHREAD_MUTEX_PSHARED_BIT), 0))
goto wake_all;

/* lll_futex_requeue returns 0 for success and non-zero
for errors.  */
if (__builtin_expect (lll_futex_requeue (&mycond->__data.__futex, 1,
                                         INT_MAX, &mut->__data.__lock,
                                         futex_val, LLL_PRIVATE), 0))
{
/* The requeue functionality is not available.  */
wake_all:
lll_futex_wake (&mycond->__data.__futex, INT_MAX, pshared);
}

/* That's all.  */
return 0;
}

/* We are done.  */
lll_unlock (mycond->__data.__lock, pshared);

return 0;
}