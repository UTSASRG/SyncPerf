#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "mutex_manager.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include "pthreadP.h"
#include <lowlevellock.h>
#include "xdefines.h"



#ifndef lll_trylock_elision
#define lll_trylock_elision(a,t) lll_trylock(a)
#endif

#ifndef FORCE_ELISION
#define FORCE_ELISION(m, s)
#endif

int
pthread_mutex_trylock (pthread_mutex_t *mutex) {
#ifdef GET_STATISTICS
  __atomic_fetch_add(&totalLocks, 1, __ATOMIC_RELAXED);
#endif
#ifndef ORIGINAL
	//int tid = getThreadIndex();
    int tid = current->index;
  if( !is_my_mutex(mutex) )
  {
		mutex_t *new_mutex = create_mutex(mutex);
		setSyncEntry(mutex, new_mutex);
  }
  mutex_t *mutex_data = (mutex_t *)get_mutex(mutex);  
  mutex = &mutex_data->mutex;
		
	//mutex_meta_t *curr_meta = NULL;
	

//#if 1
//	long stack[MAX_CALL_STACK_DEPTH + 1];
//	back_trace(stack, MAX_CALL_STACK_DEPTH);
//	curr_meta = get_mutex_meta(tmp, stack);
//#else
//	unsigned int ebp;
//	asm volatile("movl %%ebp,%0\n"
//                 : "=r"(ebp));
//	curr_meta = get_call_site_mutex(tmp, ebp);
//#endif

#ifdef WITH_TRYLOCK
	//add_access_count(curr_meta, idx);
	inc_access_count(mutex_data->entry_index, tid);
	unsigned int esp;
  asm volatile("movl %%esp,%0\n"
                 : "=r"(esp));
  unsigned int esp_offset =  getThreadStackTop() - esp;
  assert(esp_offset > 0);
  add_new_context(mutex_data,  (long)__builtin_return_address(0), esp_offset);
	//trylock_first_timestamp(curr_meta,idx); // get timestamp for first try only by a thread
#endif //WITH_TRYLOCK
#endif //ORIGINAL

  int result =  do_mutex_trylock(mutex);
#ifndef ORIGINAL
#ifdef WITH_TRYLOCK
	if(result == EBUSY ) { //failed as  mutex is already locked
		//printf("\n....trylock failed....\n\n");
		//inc_trylock_fail_count(curr_meta, idx);
		inc_trylock_fail_count(mutex_data->entry_index, tid);
	}
  else {
		//add_trylock_fail_time(curr_meta,idx);
	}
#endif //WITH_TRYLOCK
#endif //ORIGINAL
	return result;

}


int
do_mutex_trylock (pthread_mutex_t *mutex)
{
	int oldval;
	pid_t id = THREAD_GETMEM (THREAD_SELF, tid);

	switch (__builtin_expect (PTHREAD_MUTEX_TYPE_ELISION (mutex),
		PTHREAD_MUTEX_TIMED_NP))
	{
		/* Recursive mutex.  */
	case PTHREAD_MUTEX_RECURSIVE_NP|PTHREAD_MUTEX_ELISION_NP:
	case PTHREAD_MUTEX_RECURSIVE_NP:
		/* Check whether we already hold the mutex.  */
		if (mutex->__data.__owner == id)
		{
			/* Just bump the counter.  */
			if (__glibc_unlikely (mutex->__data.__count + 1 == 0))
				/* Overflow of the counter.  */
				return EAGAIN;

			++mutex->__data.__count;
			return 0;
		}

		if (lll_trylock (mutex->__data.__lock) == 0)
		{
			/* Record the ownership.  */
			mutex->__data.__owner = id;
			mutex->__data.__count = 1;
			++mutex->__data.__nusers;
			return 0;
		}
		break;
	case PTHREAD_MUTEX_TIMED_ELISION_NP:
		assert(PTHREAD_MUTEX_TYPE_ELISION (mutex) != PTHREAD_MUTEX_TIMED_ELISION_NP);
#if 1 //mejbah

		elision: __attribute__((unused))
			 if (lll_trylock_elision (mutex->__data.__lock,
				 mutex->__data.__elision) != 0)
				 break;
		 /* Don't record the ownership.  */
		 return 0;
#endif

	case PTHREAD_MUTEX_TIMED_NP:
		FORCE_ELISION (mutex, goto elision);
		/*FALL THROUGH*/
	case PTHREAD_MUTEX_ADAPTIVE_NP:
	case PTHREAD_MUTEX_ERRORCHECK_NP:
		if (lll_trylock (mutex->__data.__lock) != 0)
			break;

		/* Record the ownership.  */
		mutex->__data.__owner = id;
		++mutex->__data.__nusers;

		return 0;

	case PTHREAD_MUTEX_ROBUST_RECURSIVE_NP:
	case PTHREAD_MUTEX_ROBUST_ERRORCHECK_NP:
	case PTHREAD_MUTEX_ROBUST_NORMAL_NP:
	case PTHREAD_MUTEX_ROBUST_ADAPTIVE_NP:
#if 1 //mejbah
		THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending,
			&mutex->__data.__list.__next);

		oldval = mutex->__data.__lock;
		do
		{
again:
			if ((oldval & FUTEX_OWNER_DIED) != 0)
			{
				/* The previous owner died.  Try locking the mutex.  */
				int newval = id | (oldval & FUTEX_WAITERS);

				newval
					= atomic_compare_and_exchange_val_acq (&mutex->__data.__lock,
					newval, oldval);

				if (newval != oldval)
				{
					oldval = newval;
					goto again;
				}

				/* We got the mutex.  */
				mutex->__data.__count = 1;
				/* But it is inconsistent unless marked otherwise.  */
				mutex->__data.__owner = PTHREAD_MUTEX_INCONSISTENT;

				ENQUEUE_MUTEX (mutex);
				THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);

				/* Note that we deliberately exist here.  If we fall
				through to the end of the function __nusers would be
				incremented which is not correct because the old
				owner has to be discounted.  */
				return EOWNERDEAD;
			}

			/* Check whether we already hold the mutex.  */
			if (__glibc_unlikely ((oldval & FUTEX_TID_MASK) == id))
			{
				int kind = PTHREAD_MUTEX_TYPE (mutex);
				if (kind == PTHREAD_MUTEX_ROBUST_ERRORCHECK_NP)
				{
					THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending,
						NULL);
					return EDEADLK;
				}

				if (kind == PTHREAD_MUTEX_ROBUST_RECURSIVE_NP)
				{
					THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending,
						NULL);

					/* Just bump the counter.  */
					if (__glibc_unlikely (mutex->__data.__count + 1 == 0))
						/* Overflow of the counter.  */
						return EAGAIN;

					++mutex->__data.__count;

					return 0;
				}
			}

			oldval = atomic_compare_and_exchange_val_acq (&mutex->__data.__lock,
				id, 0);
			if (oldval != 0 && (oldval & FUTEX_OWNER_DIED) == 0)
			{
				THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);

				return EBUSY;
			}

			if (__builtin_expect (mutex->__data.__owner
				== PTHREAD_MUTEX_NOTRECOVERABLE, 0))
			{
				/* This mutex is now not recoverable.  */
				mutex->__data.__count = 0;
				if (oldval == id)
					lll_unlock (mutex->__data.__lock,
					PTHREAD_ROBUST_MUTEX_PSHARED (mutex));
				THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);
				return ENOTRECOVERABLE;
			}
		}
		while ((oldval & FUTEX_OWNER_DIED) != 0);

		ENQUEUE_MUTEX (mutex);
		THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);

		mutex->__data.__owner = id;
		++mutex->__data.__nusers;
		mutex->__data.__count = 1;

		return 0;

		/* The PI support requires the Linux futex system call.  If that's not
		available, pthread_mutex_init should never have allowed the type to
		be set.  So it will get the default case for an invalid type.  */
#endif //mejbah
#ifdef __NR_futex
	case PTHREAD_MUTEX_PI_RECURSIVE_NP:
	case PTHREAD_MUTEX_PI_ERRORCHECK_NP:
	case PTHREAD_MUTEX_PI_NORMAL_NP:
	case PTHREAD_MUTEX_PI_ADAPTIVE_NP:
	case PTHREAD_MUTEX_PI_ROBUST_RECURSIVE_NP:
	case PTHREAD_MUTEX_PI_ROBUST_ERRORCHECK_NP:
	case PTHREAD_MUTEX_PI_ROBUST_NORMAL_NP:
	case PTHREAD_MUTEX_PI_ROBUST_ADAPTIVE_NP:
   #if 1
		{
			int kind = mutex->__data.__kind & PTHREAD_MUTEX_KIND_MASK_NP;
			int robust = mutex->__data.__kind & PTHREAD_MUTEX_ROBUST_NORMAL_NP;

			if (robust)
				/* Note: robust PI futexes are signaled by setting bit 0.  */
				THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending,
				(void *) (((uintptr_t) &mutex->__data.__list.__next)
				| 1));

			oldval = mutex->__data.__lock;

			/* Check whether we already hold the mutex.  */
			if (__glibc_unlikely ((oldval & FUTEX_TID_MASK) == id))
			{
				if (kind == PTHREAD_MUTEX_ERRORCHECK_NP)
				{
					THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);
					return EDEADLK;
				}

				if (kind == PTHREAD_MUTEX_RECURSIVE_NP)
				{
					THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);

					/* Just bump the counter.  */
					if (__glibc_unlikely (mutex->__data.__count + 1 == 0))
						/* Overflow of the counter.  */
						return EAGAIN;

					++mutex->__data.__count;

					return 0;
				}
			}

			oldval
				= atomic_compare_and_exchange_val_acq (&mutex->__data.__lock,
				id, 0);

			if (oldval != 0)
			{
				if ((oldval & FUTEX_OWNER_DIED) == 0)
				{
					THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);

					return EBUSY;
				}

				assert (robust);

				/* The mutex owner died.  The kernel will now take care of
				everything.  */
				int private = (robust
					? PTHREAD_ROBUST_MUTEX_PSHARED (mutex)
					: PTHREAD_MUTEX_PSHARED (mutex));
				INTERNAL_SYSCALL_DECL (__err);
				int e = INTERNAL_SYSCALL (futex, __err, 4, &mutex->__data.__lock,
					__lll_private_flag (FUTEX_TRYLOCK_PI,
					private), 0, 0);

				if (INTERNAL_SYSCALL_ERROR_P (e, __err)
					&& INTERNAL_SYSCALL_ERRNO (e, __err) == EWOULDBLOCK)
				{
					THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);

					return EBUSY;
				}

				oldval = mutex->__data.__lock;
			}

			if (__glibc_unlikely (oldval & FUTEX_OWNER_DIED))
			{
				atomic_and (&mutex->__data.__lock, ~FUTEX_OWNER_DIED);

				/* We got the mutex.  */
				mutex->__data.__count = 1;
				/* But it is inconsistent unless marked otherwise.  */
				mutex->__data.__owner = PTHREAD_MUTEX_INCONSISTENT;

				ENQUEUE_MUTEX (mutex);
				THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);

				/* Note that we deliberately exit here.  If we fall
				through to the end of the function __nusers would be
				incremented which is not correct because the old owner
				has to be discounted.  */
				return EOWNERDEAD;
			}

			if (robust
				&& __builtin_expect (mutex->__data.__owner
				== PTHREAD_MUTEX_NOTRECOVERABLE, 0))
			{
				/* This mutex is now not recoverable.  */
				mutex->__data.__count = 0;

				INTERNAL_SYSCALL_DECL (__err);
				INTERNAL_SYSCALL (futex, __err, 4, &mutex->__data.__lock,
					__lll_private_flag (FUTEX_UNLOCK_PI,
					PTHREAD_ROBUST_MUTEX_PSHARED (mutex)),
					0, 0);

				THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);
				return ENOTRECOVERABLE;
			}

			if (robust)
			{
				ENQUEUE_MUTEX_PI (mutex);
				THREAD_SETMEM (THREAD_SELF, robust_head.list_op_pending, NULL);
			}

			mutex->__data.__owner = id;
			++mutex->__data.__nusers;
			mutex->__data.__count = 1;

			return 0;
		}
  #endif //mejbah
#endif  /* __NR_futex.  */

	case PTHREAD_MUTEX_PP_RECURSIVE_NP:
	case PTHREAD_MUTEX_PP_ERRORCHECK_NP:
	case PTHREAD_MUTEX_PP_NORMAL_NP:
	case PTHREAD_MUTEX_PP_ADAPTIVE_NP:
		{
			int kind = mutex->__data.__kind & PTHREAD_MUTEX_KIND_MASK_NP;

			oldval = mutex->__data.__lock;

			/* Check whether we already hold the mutex.  */
			if (mutex->__data.__owner == id)
			{
				if (kind == PTHREAD_MUTEX_ERRORCHECK_NP)
					return EDEADLK;

				if (kind == PTHREAD_MUTEX_RECURSIVE_NP)
				{
					/* Just bump the counter.  */
					if (__glibc_unlikely (mutex->__data.__count + 1 == 0))
						/* Overflow of the counter.  */
						return EAGAIN;

					++mutex->__data.__count;

					return 0;
				}
			}

			int oldprio = -1, ceilval;
			do
			{
				int ceiling = (oldval & PTHREAD_MUTEX_PRIO_CEILING_MASK)
					>> PTHREAD_MUTEX_PRIO_CEILING_SHIFT;

				if (__pthread_current_priority () > ceiling)
				{
					if (oldprio != -1)
						__pthread_tpp_change_priority (oldprio, -1);
					return EINVAL;
				}

				int retval = __pthread_tpp_change_priority (oldprio, ceiling);
				if (retval)
					return retval;

				ceilval = ceiling << PTHREAD_MUTEX_PRIO_CEILING_SHIFT;
				oldprio = ceiling;

				oldval
					= atomic_compare_and_exchange_val_acq (&mutex->__data.__lock,
					ceilval | 1, ceilval);

				if (oldval == ceilval)
					break;
			}
			while ((oldval & PTHREAD_MUTEX_PRIO_CEILING_MASK) != ceilval);

			if (oldval != ceilval)
			{
				__pthread_tpp_change_priority (oldprio, -1);
				break;
			}

			assert (mutex->__data.__owner == 0);
			/* Record the ownership.  */
			mutex->__data.__owner = id;
			++mutex->__data.__nusers;
			mutex->__data.__count = 1;

			return 0;
		}
		break;

	default:
		/* Correct code cannot set any other type.  */
		return EINVAL;
	}

	return EBUSY;
}
