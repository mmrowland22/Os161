/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(*sem));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(*lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

        // add stuff here as needed
	//like semaphore create
	lock->lockWchan = wchan_create(lock->lk_name);
	if(lock->lockWchan == NULL)
	{
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	//initliazie spinlock
	spinlock_initialize(&lock->lockProtect);

	//initialize flag as locked and owner as null until we have more info
	lock->flagLocked = 1;
	lock->owner = NULL;

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed

	//like semaphore
	spinlock_cleanup(&lock->lockProtect);
	wchan_destroy(lock->lockWchan);

        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
// Write this
        //(void)lock;  suppress warning until code gets written

	KASSERT(lock != NULL);

	//Like in semaphore, may not block interrupt handler
	KASSERT(curthread->t_in_interrupt == false);

	//check to make sure owner is not trying to obtain lock it already owns
	if(CURCPU_EXISTS())
	{
		KASSERT(lock->owner != curthread);
	}

	//acquire spinlock for lock before accessing the member fields
	spinlock_acquire(&lock->lockProtect);

	//while loop when woken up check again to see if lock is taken away. If not, set lock
	while(lock->flagLocked == 0)
	{
		wchan_lock(lock->lockWchan);
		spinlock_release(&lock->lockProtect);
		//put thread to sleep if lock is not available
		wchan_sleep(lock->lockWchan);
		//set lock when exiting
		spinlock_acquire(&lock->lockProtect);
	}

	KASSERT(lock->flagLocked > 0);

	lock->flagLocked = 0;

	//set owner to thread
	if(CURCPU_EXISTS())
	{
		lock->owner = curthread;
	}
	else
	{
		lock->owner = NULL;
	}

	//release lock
	spinlock_release(&lock->lockProtect);
}

void
lock_release(struct lock *lock)
{
        // Write this

        //(void)lock;   suppress warning until code gets written

//follow format of already implemented semaphore

	KASSERT(lock != NULL);

	spinlock_acquire(&lock->lockProtect);

	//make sure owner is the actual owner
	if(CURCPU_EXISTS())
	{
		KASSERT(lock->owner = curthread);
	}

	lock->flagLocked++;
	KASSERT(lock->flagLocked > 0);
	wchan_wakeone(lock->lockWchan, &lock->lockProtect);

	spinlock_release(&lock->lockProtect);


}

bool
lock_do_i_hold(struct lock *lock)
{
        // Write this

        //(void)lock;   suppress warning until code gets written

        //return true;  dummy until code gets written

	if(CURCPU_EXISTS())
	{
		return true;
	}

	return(lock->owner==curthread);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(*cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }

        // add stuff here as needed

	cv->cv_name = wchan_create(cv->cv_name);
	if(cv->cvWchan == NULL)
	{
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cvLock);


        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed

	//like semaphore
	spinlock_cleanup(&cv->cvLock);
	wchan_destroy(cv->cvWchan);

        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        // Write this
        //(void)cv;     suppress warning until code gets written
        //(void)lock;   suppress warning until code gets written

	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	//acquire spinlock for CV first to ensure atomicity for the following steps
	spinlock_acquire(&cv->cvLock);

	//release lock
	lock_release(lock);

	//put thread to sleep in cv's wchan
	wchan_sleep(cv->cvWchan);

	//release spinlock
	spinlock_release(&cv->cvLock);

	//acquire lock before completing the function
	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Write this
	//(void)cv;     suppress warning until code gets written
	//(void)lock;   suppress warning until code gets written

	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	//acquire spinlock
	spinlock_acquire(&cv->cvLock);

	wchan_wakeone(cv->cvWchan, &cv->cvLock);

	//release spinlock
	spinlock_release(&cv->cvLock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	//(void)cv;    suppress warning until code gets written
	//(void)lock;   suppress warning until code gets written
	KASSERT(cv != NULL);
        KASSERT(lock != NULL);

        //acquire spinlock
        spinlock_acquire(&cv->cvLock);

        wchan_wakeall(cv->cvWchan, &cv->cvLock);

        //release spinlock
	spinlock_release(&cv->cvLock);

}
