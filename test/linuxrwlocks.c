#include <stdio.h>

#include "libthreads.h"
#include "librace.h"
#include "stdatomic.h"


#define RW_LOCK_BIAS            0x00100000
#define WRITE_LOCK_CMP          RW_LOCK_BIAS

/** Example implementation of linux rw lock along with 2 thread test
 *  driver... */


typedef union {
	atomic_int lock;
} rwlock_t;

static inline int read_can_lock(rwlock_t *lock)
{
	return atomic_load_explicit(&lock->lock, memory_order_relaxed) > 0;
}

static inline int write_can_lock(rwlock_t *lock)
{
	return atomic_load_explicit(&lock->lock, memory_order_relaxed) == RW_LOCK_BIAS;
}

static inline void read_lock(rwlock_t *rw)
{
	int currentvalue=atomic_fetch_sub_explicit(&rw->lock, 1, memory_order_acquire);
	while (currentvalue<0) {
		atomic_fetch_add_explicit(&rw->lock, 1, memory_order_relaxed);
		do {
			currentvalue=atomic_load_explicit(&rw->lock, memory_order_relaxed);
		}	while(currentvalue<=0);
		currentvalue=atomic_fetch_sub_explicit(&rw->lock, 1, memory_order_acquire);
	}
}

static inline void write_lock(rwlock_t *rw)
{
	int currentvalue=atomic_fetch_sub_explicit(&rw->lock, RW_LOCK_BIAS, memory_order_acquire);
	while (currentvalue!=0) {
		atomic_fetch_add_explicit(&rw->lock, RW_LOCK_BIAS, memory_order_relaxed);
		do {
			currentvalue=atomic_load_explicit(&rw->lock, memory_order_relaxed);
		}	while(currentvalue!=RW_LOCK_BIAS);
		currentvalue=atomic_fetch_sub_explicit(&rw->lock, RW_LOCK_BIAS, memory_order_acquire);
	}
}

static inline int read_trylock(rwlock_t *rw)
{
	int currentvalue=atomic_fetch_sub_explicit(&rw->lock, 1, memory_order_acquire);
	if (currentvalue>=0)
		return 1;
	
	atomic_fetch_add_explicit(&rw->lock, 1, memory_order_relaxed);
	return 0;
}

static inline int write_trylock(rwlock_t *rw)
{
	int currentvalue=atomic_fetch_sub_explicit(&rw->lock, RW_LOCK_BIAS, memory_order_acquire);
	if (currentvalue>=0)
		return 1;
	
	atomic_fetch_add_explicit(&rw->lock, RW_LOCK_BIAS, memory_order_relaxed);
	return 0;
}

static inline void read_unlock(rwlock_t *rw)
{
	atomic_fetch_add_explicit(&rw->lock, 1, memory_order_release);
}

static inline void write_unlock(rwlock_t *rw)
{
	atomic_fetch_add_explicit(&rw->lock, RW_LOCK_BIAS, memory_order_release);
}

rwlock_t mylock;
int shareddata;

static void a(void *obj)
{
	int i;
	for(i=0;i<2;i++) {
		if ((i%2)==0) {
			read_lock(&mylock);
			load_32(&shareddata);
			read_unlock(&mylock);
		} else {
			write_lock(&mylock);
			store_32(&shareddata,(unsigned int)i);
			write_unlock(&mylock);
		}
	}
}

void user_main()
{
	thrd_t t1, t2;
	atomic_init(&mylock.lock, RW_LOCK_BIAS);

	printf("Thread %d: creating 2 threads\n", thrd_current());
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&a, NULL);
	
	thrd_join(t1);
	thrd_join(t2);
	printf("Thread %d is finished\n", thrd_current());
}
