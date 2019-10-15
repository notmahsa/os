#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

/*
 0  R  running (or runnable, on run queue)
 1  S  ready (interruptible sleep, waiting for an event to complete)
 2  D  blocked (uninterruptible sleep, usually IO)
 3  Z  exited (defunct/zombie, terminated but not reaped by its parent)
 4  T  stopped (either by a job control signal or because it is being traced)
*/

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
	ucontext_t * context;
	Tid id;
	unsigned short int state;
	void * parg;
	void (*fn) (void *);
};

bool threads_exist[THREAD_MAX_THREADS] = { false };
struct thread * threads_pointer_list[THREAD_MAX_THREADS] = { NULL };
struct thread * running = NULL;
struct ready_queue * ready_head = NULL;

void
thread_init(void)
{
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());

	int err;
	struct thread * first_thread = (struct thread *)malloc(sizeof(struct thread));
	ucontext_t * context = (ucontext_t *)malloc(sizeof(ucontext_t));

    first_thread->context = context;
    first_thread->state = 0;
    first_thread->id = 0;

    threads_pointer_list[first_thread->id] = first_thread;
    threads_exist[first_thread->id] = true;
    running = first_thread;
    err = getcontext(first_thread->context);
    assert(!err);
    ready_head = NULL;

    interrupts_set(enabled);
}

Tid
thread_id()
{
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());

	if (running){
	    Tid ret = running->id;
	    interrupts_set(enabled);
	    return ret;
	}

	interrupts_set(enabled);
	return THREAD_INVALID;
}

void
thread_stub(void (*fn) (void *), void *parg){
    interrupts_on();
    (*fn)(parg);
    thread_exit();

    exit(0);
}

void
thread_append_to_ready_queue(Tid id){
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
    if (ready_head == NULL){
        struct ready_queue * new_ready_node = malloc(sizeof(struct ready_queue));
        new_ready_node->id = id;
        new_ready_node->next = NULL;
        ready_head = new_ready_node;
        interrupts_set(enabled);
        return;
    }
    struct ready_queue * push = ready_head;
    for(push = ready_head; push != NULL; push = push->next)
    {
        if(push->next == NULL)
        {
            struct ready_queue * wq;
            wq = malloc(sizeof(struct ready_queue));
            assert(wq);
            wq->id = id;
            wq->next = NULL;
            push->next = wq;
            push->next->id = id;
            break;
        }
    }
    interrupts_set(enabled);
}

void
thread_pop_from_ready_queue(Tid id){
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
    if (!ready_head){
        interrupts_set(enabled);
        return;
    }
    if(ready_head != NULL && ready_head->id == id){
        struct ready_queue * temp = ready_head->next;
        free(ready_head);
        ready_head = temp;
        interrupts_set(enabled);
        return;
    }
    struct ready_queue * pop, * previous;
    previous = ready_head;
    for(pop = ready_head; pop != NULL; pop = pop->next)
    {
        if(pop->id == id)
        {
            previous->next = pop->next;
            free(pop);
        }
        previous = pop;
    }
    interrupts_set(enabled);
}

Tid
thread_implicit_exit(Tid tid)
{
    /*
       Only kills the thread. This does not destroy the thread.
       Thread will enter zombie state.
    */
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
    if (threads_exist[tid] == false){
        interrupts_set(enabled);
	    return THREAD_NONE;
	}

	struct thread * thread_to_be_killed = threads_pointer_list[tid];
    thread_to_be_killed->state = 3;

    bool already_in_ready_queue = false;
    struct ready_queue * pop;
    for(pop = ready_head; pop != NULL; pop = pop->next)
    {
        if(pop->id == tid){
            already_in_ready_queue = true;
            break;
        }
    }

    if (already_in_ready_queue){
        interrupts_set(enabled);
        return tid;
    }

    thread_append_to_ready_queue(thread_to_be_killed->id);
    interrupts_set(enabled);
    return tid;
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
    int err;
    struct thread * new_thread = malloc(sizeof(struct thread));
    void * new_stack = malloc(THREAD_MIN_STACK);
    ucontext_t * new_context = malloc(sizeof(ucontext_t));

    if  (!new_stack || !new_thread || !new_context){
        free(new_stack);
        free(new_thread);
        free(new_context);
        interrupts_set(enabled);
        return THREAD_NOMEMORY;
    }

    short int new_id = -1;
    for (int i = 1; i < THREAD_MAX_THREADS; i++){
        if (threads_exist[i] == false){
            new_id = i;
            break;
        }
    }

    if (new_id == -1){
        free(new_stack);
        free(new_thread);
        free(new_context);
        interrupts_set(enabled);
        return THREAD_NOMORE;
    }

    err = getcontext(new_context);
    assert(!err);

    new_context->uc_stack.ss_sp = new_stack;
    new_context->uc_stack.ss_size = THREAD_MIN_STACK;
    new_context->uc_stack.ss_flags = 0;
    new_context->uc_link = 0;

    unsigned long subtraction_factor = (unsigned long)new_stack % (unsigned long)16;

    new_context->uc_mcontext.gregs[REG_RSP] = (long long)(new_stack + THREAD_MIN_STACK - subtraction_factor - 8);
    new_context->uc_mcontext.gregs[REG_RIP] = (long long)thread_stub;
    new_context->uc_mcontext.gregs[REG_RDI] = (long long)fn;
    new_context->uc_mcontext.gregs[REG_RSI] = (long long)parg;
    new_context->uc_mcontext.gregs[REG_RBP] = (long long)new_stack;

    new_thread->context = new_context;
    new_thread->state = 1;
    new_thread->id = new_id;

    threads_pointer_list[new_thread->id] = new_thread;
    threads_exist[new_thread->id] = true;
    thread_append_to_ready_queue(new_thread->id);

    Tid ret = new_thread->id;
    interrupts_set(enabled);
	return ret;
}

Tid
thread_yield(Tid want_tid)
{
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
    if (running->state == 3){
        interrupts_set(enabled);
        thread_exit();
    }

    if (!running){
        interrupts_set(enabled);
        return THREAD_FAILED;
    }

    if (want_tid == THREAD_SELF || want_tid == running->id){
        Tid ret = running->id;
        interrupts_set(enabled);
        return ret;
    }

    if (want_tid != THREAD_ANY && (want_tid < 0 || want_tid >= THREAD_MAX_THREADS || threads_exist[want_tid] == 0)){
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    if (ready_head == NULL){
        interrupts_set(enabled);
        return THREAD_NONE;
    }

    if (want_tid == THREAD_ANY){
        int err;
        Tid yield_tid = ready_head->id;
        int setcontext_called = 0;
        err = getcontext(running->context);
        assert(!err);

        if (setcontext_called == 1){
            interrupts_set(enabled);
            return yield_tid;
        }

        setcontext_called = 1;
        running->state = 1;
        thread_append_to_ready_queue(running->id);

        struct ready_queue * temp_head = ready_head->next;
        struct thread * next_thread_to_run;
        next_thread_to_run = threads_pointer_list[ready_head->id];
        if (ready_head->next == NULL){
            thread_implicit_exit(running->id);
        }
        free(ready_head);
        ready_head = temp_head;
        next_thread_to_run->state = 0;
        running = next_thread_to_run;
        setcontext(running->context);
    }
    else{
        int err;
        int setcontext_called = 0;
        struct thread * next_thread_to_run;
        err = getcontext(running->context);
        assert(!err);

        if (setcontext_called == 1){
            interrupts_set(enabled);
            return want_tid;
        }

        setcontext_called = 1;
        thread_append_to_ready_queue(running->id);
        thread_pop_from_ready_queue(want_tid);
        next_thread_to_run = threads_pointer_list[want_tid];
        next_thread_to_run->state = 0;
        running = next_thread_to_run;
        setcontext(running->context);
    }

    interrupts_set(enabled);
	return THREAD_FAILED;
}

void
thread_exit()
{
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
    running->state = 4;
    threads_exist[running->id] = 0;
    threads_pointer_list[running->id] = NULL;
    free(running->context->uc_stack.ss_sp);
    free(running->context);
    free(running);
    running = NULL;

    if (ready_head){
        struct thread * next_thread_to_run;
        struct ready_queue * temp_head = ready_head->next;
        next_thread_to_run = threads_pointer_list[ready_head->id];
        free(ready_head);
        ready_head = temp_head;
        next_thread_to_run->state = 0;
        running = next_thread_to_run;
        setcontext(running->context);
    }
    interrupts_set(enabled);
    exit(0);
}

Tid
thread_kill(Tid tid)
{
    /*
       Destroys the thread and frees all associated memory.
    */
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
    if (threads_exist[tid] == false || running->id == tid){
        interrupts_set(enabled);
	    return THREAD_INVALID;
	}

	struct thread * thread_to_be_killed = threads_pointer_list[tid];
	thread_pop_from_ready_queue(thread_to_be_killed->id);

    thread_to_be_killed->state = 4;
    threads_exist[thread_to_be_killed->id] = false;
    threads_pointer_list[thread_to_be_killed->id] = NULL;
    free(thread_to_be_killed->context->uc_stack.ss_sp);
    free(thread_to_be_killed->context);
    free(thread_to_be_killed);

    interrupts_set(enabled);
    return tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/
/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	wq->id = -1;
	wq->next = NULL;

    interrupts_set(enabled);
	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());
	struct wait_queue * next;
	struct wait_queue * current = wq;

    while (current != NULL){
       next = current->next;
       free(current);
       current = next;
    }

	interrupts_set(enabled);
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
	Tid id;
    struct wait_queue * wait;
};

struct lock *
lock_create()
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());

    struct lock * l;
    l = malloc(sizeof(struct lock));
    assert(l);

    l->id = -1;
    l->wait = wait_queue_create();

    interrupts_set(enabled);
    return l;
}

void
lock_destroy(struct lock *lock)
{
	int enabled = interrupts_off();
	assert(!interrupts_enabled());

    assert(lock != NULL);
    if(lock->id < 0){
        wait_queue_destroy(lock->wait);
        free(lock);
    }

    interrupts_set(enabled);
}

void
lock_acquire(struct lock *lock)
{
	int enabled = interrupts_off();
	assert(!interrupts_enabled());

    assert(lock != NULL);
    while(lock->id >= 0){
        thread_sleep(lock->wait);
    }

    lock->id = running->id;
    interrupts_set(enabled);
}

void
lock_release(struct lock *lock)
{
	int enabled = interrupts_off();
	assert(!interrupts_enabled());

    assert(lock != NULL);
    if(lock->id == running->id){
        thread_wakeup(lock->wait, 1);
        lock->id = -1;
    }
    
    interrupts_set(enabled);
}

struct cv {
	struct wait_queue * wait;
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
