#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

/*
 0  R  running or runnable (on run queue)
 1  D  uninterruptible sleep (usually IO)
 2  S  interruptible sleep (waiting for an event to complete)
 3  Z  defunct/zombie, terminated but not reaped by its parent
 4  T  stopped, either by a job control signal or because
               it is being traced
*/
#define STACKSIZE 4096

struct ready_queue {
    Tid id;
    struct ready_queue * next;
};

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
	Tid id;
	struct wait_queue * next;
};

/* This is the thread control block */
struct thread {
	ucontext_t context;
	Tid id;
	unsigned short int state;
	void * parg = NULL;
	void (*fn) (void *);

};

bool threads_exist[THREAD_MAX_THREADS] = { false };
struct thread * threads_pointer_list[THREAD_MAX_THREADS] = { NULL };
struct thread * running = NULL;
struct ready_queue * ready_head = NULL;


void
thread_init(void)
{
	int err;
	struct thread * first = malloc(sizeof(struct thread));
	ucontext_t context = { 0 };

	err = getcontext(&context);
    assert(!err);

    first->context = context;
    first->state = 0;
    first->id = 0;

    threads_pointer_list[first->id] = first;
    threads_exist[first->id] = true;
    running = first;
}

Tid
thread_id()
{
	if (running){
	    return running->id;
	}
	return THREAD_INVALID;
}

Tid
thread_stub(void (*fn) (void *), void *parg){
    fn(parg);
    // thread_wakeup();
    return thread_id();
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    int err;
    struct thread * new_thread = malloc(sizeof(struct thread));
    void * new_stack = malloc(STACKSIZE);

    if  (!new_stack || !new_thread){
        return THREAD_NOMEMORY;
    }

    ucontext_t new_context = { 0 };

    err = getcontext(new_context);
    assert(!err);

    new_context.uc_stack.ss_sp = new_stack;
    new_context.uc_stack.ss_size = STACKSIZE;
    new_context.uc_stack.ss_flags = 0;

    if (sigemptyset(&new_context->uc_sigmask) < 0)
        return THREAD_FAILED;

    new_context.uc_mcontext.gregs[REG_RIP] = thread_stub;
    new_context.uc_mcontext.gregs[REG_RSI] = fn;
    new_context.uc_mcontext.gregs[REG_RDI] = parg;

    printf("Context is %p\n", new_context);

    short unsigned int new_id = -1;
    for (int i = 1; i < THREAD_MAX_THREADS; i++){
        if (!threads_exist[i]){
            new_id = i;
            break;
        }
    }

    if (new_id == -1){
        return THREAD_NOMORE;
    }

    new_thread->context = context;
    new_thread->state = 1;
    new_thread->id = new_id;

    threads_pointer_list[new_thread->id] = new_thread;
    threads_exist[new_thread->id] = true;
    thread_wait(new_thread->id);

	return new_thread->id;
}

Tid
thread_yield(Tid want_tid)
{
	TBD();
	return THREAD_FAILED;
}

void
thread_exit()
{
	TBD();
}

Tid
thread_kill(Tid tid)
{
	TBD();
	return THREAD_FAILED;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

int
main(int argc, char **argv)
{
    thread_init();
    thread_create((void *)printf, "HI THERE\n");
}