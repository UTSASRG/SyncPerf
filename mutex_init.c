#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <lowlevellock.h>
#include <pthread.h>
//#include <kernel-features.h>
#include <atomic.h>
#include "pthreadP.h"
#include <sysdep.h>

#include<mutex_manager.h>

static const struct pthread_mutexattr default_mutexattr = {
	/* Default is a normal mutex, not shared between processes.  */
	.mutexkind = PTHREAD_MUTEX_NORMAL
};

int pthread_mutex_init (pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr)
{
	const struct pthread_mutexattr *imutexattr;
	assert (sizeof (pthread_mutex_t) <= __SIZEOF_PTHREAD_MUTEX_T);
	imutexattr = ((const struct pthread_mutexattr *) mutexattr
		?: &default_mutexattr);
#ifdef MY_DEBUG
	printf("In my pthread mutex init\n");
#endif
	/* Sanity checks.  */
	switch (__builtin_expect (imutexattr->mutexkind
		& PTHREAD_MUTEXATTR_PROTOCOL_MASK,
		PTHREAD_PRIO_NONE
		<< PTHREAD_MUTEXATTR_PROTOCOL_SHIFT))
	{
	case PTHREAD_PRIO_NONE << PTHREAD_MUTEXATTR_PROTOCOL_SHIFT:
		break;
	case PTHREAD_PRIO_INHERIT << PTHREAD_MUTEXATTR_PROTOCOL_SHIFT:
		//if (__glibc_unlikely (prio_inherit_missing ()))
		//     return ENOTSUP;
		break;
	default:
		/* XXX: For now we don't support robust priority protected mutexes.  */
		if (imutexattr->mutexkind & PTHREAD_MUTEXATTR_FLAG_ROBUST)
			return ENOTSUP;
		break;
	}
	/* Clear the whole variable.  */
	memset (mutex, '\0', __SIZEOF_PTHREAD_MUTEX_T);

	/* Copy the values from the attribute.  */
	mutex->__data.__kind = imutexattr->mutexkind & ~PTHREAD_MUTEXATTR_FLAG_BITS;

	if ((imutexattr->mutexkind & PTHREAD_MUTEXATTR_FLAG_ROBUST) != 0)
	{
#ifndef __ASSUME_SET_ROBUST_LIST
		// if ((imutexattr->mutexkind & PTHREAD_MUTEXATTR_FLAG_PSHARED) != 0
		//         && __set_robust_list_avail < 0)
		//     return ENOTSUP;
#endif
		mutex->__data.__kind |= PTHREAD_MUTEX_ROBUST_NORMAL_NP;
	}
	switch (imutexattr->mutexkind & PTHREAD_MUTEXATTR_PROTOCOL_MASK)
	{
	case PTHREAD_PRIO_INHERIT << PTHREAD_MUTEXATTR_PROTOCOL_SHIFT:
		mutex->__data.__kind |= PTHREAD_MUTEX_PRIO_INHERIT_NP;
		break;
	case PTHREAD_PRIO_PROTECT << PTHREAD_MUTEXATTR_PROTOCOL_SHIFT:
		mutex->__data.__kind |= PTHREAD_MUTEX_PRIO_PROTECT_NP;
		int ceiling = (imutexattr->mutexkind
			& PTHREAD_MUTEXATTR_PRIO_CEILING_MASK)
			>> PTHREAD_MUTEXATTR_PRIO_CEILING_SHIFT;
		if (! ceiling)
		{	  /* See __init_sched_fifo_prio.  */
			//if (atomic_load_relaxed (&__sched_fifo_min_prio) == -1)
			//    __init_sched_fifo_prio ();
			//if (ceiling < atomic_load_relaxed (&__sched_fifo_min_prio))
			//    ceiling = atomic_load_relaxed (&__sched_fifo_min_prio);
		}
		mutex->__data.__lock = ceiling << PTHREAD_MUTEX_PRIO_CEILING_SHIFT;
		break;
	default:
		break;
	}

	/* The kernel when waking robust mutexes on exit never uses
	* FUTEX_PRIVATE_FLAG FUTEX_WAKE.  */
	if ((imutexattr->mutexkind & (PTHREAD_MUTEXATTR_FLAG_PSHARED
		| PTHREAD_MUTEXATTR_FLAG_ROBUST)) != 0)
		mutex->__data.__kind |= PTHREAD_MUTEX_PSHARED_BIT;

	/* Default values: mutex not used yet.  */
	// mutex->__count = 0;	already done by memset
	// // mutex->__owner = 0;	already done by memset
	// // mutex->__nusers = 0;	already done by memset
	// // mutex->__spins = 0;	already done by memset
	// // mutex->__next = NULL;	already done by memset

	// LIBC_PROBE (mutex_init, 1, mutex);

    //mejbah added 
#ifndef ORIGINAL
    *(my_mutex_t**)mutex = create_mutex(mutex);
#endif
                
	return 0;

}