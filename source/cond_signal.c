#include "pthreadP.h"
#include "mypthreadtypes.h"

int
pthread_cond_signal (cond)
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
/* Yes.  Mark one of them as woken.  */
++mycond->__data.__wakeup_seq;
++mycond->__data.__futex;

/* Wake one.  */
if (! __builtin_expect (lll_futex_wake_unlock (&mycond->__data.__futex, 1,
                                               1, &mycond->__data.__lock,
                                               pshared), 0))
return 0;

lll_futex_wake (&mycond->__data.__futex, 1, pshared);
}

/* We are done.  */
lll_unlock (mycond->__data.__lock, pshared);

return 0;
}