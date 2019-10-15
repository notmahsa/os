#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

/* This is the wait queue structure */
struct wait_queue {
    Tid id;
    struct wait_queue *next;
};

/* This is the thread control block */
struct thread {
    Tid id; // Thread ID
    ucontext_t context; // Thread Context
    void * sp; // Stack pointer
    int status; // 0 for dead thread, 1 for alive
    struct wait_queue *wait;
};

// pointer to the next thread to run
struct wait_queue* q_start;
// Array to hold all our threads
struct thread threads[THREAD_MAX_THREADS];
// Current active thread
int active = 0;

void thread_stub(void (*thread_main) (void *), void *arg)
{
    //Tid ret;
    interrupts_on();
    thread_main(arg);  // call thread_main function with arg
    thread_exit();

    // we should only get here if we are the last thread
    // assert(ret == THREAD_NONE);

    for(int i = 0; i < THREAD_MAX_THREADS; i++)
    {
        if(threads[i].status == 1)
        {
            free(threads[i].sp);
        }
    }
    // All threads are done, so process should exit
    exit(0);

}

void
thread_init(void)
{
    int enabled = interrupts_off();
    //Initialize all threads in the data structure
    for(int i = 0; i < THREAD_MAX_THREADS; i++)
    {
        threads[i].id = i;
        threads[i].status = 0;
	threads[i].sp = NULL;
	threads[i].wait = wait_queue_create();
    }

    //Setup current thread in the first position of threads
    active = 0;
    threads[active].status = 1;
    getcontext(&threads[active].context);
    q_start = NULL;
    interrupts_set(enabled);
}

Tid
thread_id()
{
    return active;
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    int enabled = interrupts_off();

    for(int i = 0; i < THREAD_MAX_THREADS; i++)
    {
        //Create a thread at this index
        if(threads[i].status == 0)
        {
            threads[i].status = 1;
            // create the stack
            threads[i].sp= malloc(THREAD_MIN_STACK);
            if(threads[i].sp == NULL)
	    {
		interrupts_set(enabled);
                return THREAD_NOMEMORY;
	    }

            // receive a valid context
            int ret = getcontext(&threads[i].context);
            assert(!ret);

            threads[i].context.uc_stack.ss_size = THREAD_MIN_STACK;
            // fill up the context's fields
            // set up the stack pointer and align it
            threads[i].context.uc_mcontext.gregs[REG_RSP] = (unsigned long) threads[i].sp + THREAD_MIN_STACK - ((unsigned long)threads[i].sp % (unsigned long)16) -8;
            // set up the return address(stub function)
            threads[i].context.uc_mcontext.gregs[REG_RIP] = (unsigned long) &thread_stub;
            // set up the function name
            threads[i].context.uc_mcontext.gregs[REG_RDI] = (unsigned long) fn;
            // set up the function argument
            threads[i].context.uc_mcontext.gregs[REG_RSI] = (unsigned long) parg;
	    // create the wait q with a dummy starting block
	    threads[i].wait = wait_queue_create();

            if(q_start == NULL)
            {
                q_start = malloc(sizeof(struct wait_queue));
                q_start->id = i;
                q_start->next = NULL;
            }
            else
            {
                struct wait_queue *push;
                for(push = q_start; push != NULL; push = push->next)
                {
                    if(push->next == NULL)
                    {
                        push->next = malloc(sizeof(struct wait_queue));
                        push->next->id = i;
                        push->next->next = NULL;
                        break;
                    }
                }
            }
	    interrupts_set(enabled);
            return i;
        }
    }
    interrupts_set(enabled);
    return THREAD_NOMORE;
}

Tid
thread_yield(Tid want_tid)
{
    int enabled = interrupts_off();

    if(want_tid == THREAD_SELF)
    {
	interrupts_set(enabled);
        return active;
    }
    else if(active == want_tid)
    {
	interrupts_set(enabled);
        return active;
    }
    else if(want_tid == THREAD_ANY)
    {
        if(q_start == NULL)
        {
	    interrupts_set(enabled);
            return THREAD_NONE;
        }

        int yield_id = q_start->id;
        int setcontext_called = 0;
        // Save the context of the caller
        getcontext(&threads[active].context);

        if(setcontext_called == 1)
        {
	    interrupts_set(enabled);
            return yield_id;
        }
        setcontext_called = 1;

        // Add the caller thread to the end of the ready q
        struct wait_queue *push;
        for(push = q_start; push != NULL; push = push->next)
        {
            if(push->next == NULL)
            {
                push->next = wait_queue_create();
                push->next->id = active;
                break;
            }
        }

        // POP the first item in the ready q to run
        Tid next_id = q_start->id;
        struct wait_queue *temp = q_start->next;
        free(q_start);
        q_start = temp;
        active = next_id;
        setcontext(&threads[next_id].context);
    }
    else
    {
        if(want_tid > THREAD_MAX_THREADS - 1 || want_tid < 0 || threads[want_tid].status == 0)
        {
	    interrupts_set(enabled);
            return THREAD_INVALID;
        }

        int setcontext_called = 0;
        // Save the context of the caller
        getcontext(&threads[active].context);

        if(setcontext_called == 1)
        {
	    interrupts_set(enabled);
            return want_tid;
        }
        setcontext_called = 1;

        // Add the caller thread to the end of the ready q
        struct wait_queue *push;
        for(push = q_start; push != NULL; push = push->next)
        {
            if(push->next == NULL)
            {
                push->next = wait_queue_create();
                push->next->id = active;
                break;
            }
        }

        // Find the thread id in the ready q, POP it and then run it
        // if thread is in the head and we have to modify head
        if(q_start != NULL && q_start->id == want_tid)
        {
            Tid next_id = q_start->id;
            struct wait_queue *temp = q_start->next;
            free(q_start);
            q_start = temp;
            active = next_id;
            setcontext(&threads[next_id].context);
        }
        // otherwise we don't have to modify head
        else
        {
            struct wait_queue *pop, *previous;
            previous = q_start;
            for(pop = q_start; pop != NULL;pop = pop->next)
            {
                if(pop->id == want_tid)
                {
                    previous->next = pop->next;
                    free(pop);
                    active = want_tid;
                    setcontext(&threads[want_tid].context);
                }
                previous = pop;
            }
        }
    }
    interrupts_set(enabled);
    // we should never get here
    return THREAD_FAILED;
}

void
thread_exit()
{
    int enabled = interrupts_off();

    thread_wakeup(threads[active].wait, 1);
    //Check if this is the last thread
    if(q_start == NULL)
    {
        interrupts_set(enabled);
        return;
    }

    //Declare the current thread dead and free the stack memory
    threads[active].status = 0;
    free(threads[active].sp);
    free(threads[active].wait);

    // POP the first item in the ready q to run
    Tid next_id = q_start->id;
    struct wait_queue *temp = q_start->next;
    free(q_start);
    q_start = temp;
    active = next_id;
    setcontext(&threads[next_id].context);

    interrupts_set(enabled);
    return;
}

Tid
thread_kill(Tid tid)
{
    int enabled = interrupts_off();
    if(tid == active && q_start == NULL)
    {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    if(tid > THREAD_MAX_THREADS - 1 || tid < 0 || threads[tid].status == 0)
    {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    thread_wakeup(threads[tid].wait, 1);
    threads[tid].status = 0;
    free(threads[tid].sp);
    free(threads[tid].wait);

    if(q_start != NULL && q_start->id == tid)
    {
        struct wait_queue *temp = q_start->next;
        free(q_start);
        q_start = temp;
    }
    // otherwise we don't have to modify head
    else
    {
        struct wait_queue *pop, *previous;
        previous = q_start;
        for(pop = q_start; pop != NULL;pop = pop->next)
        {
            if(pop->id == tid)
            {
                previous->next = pop->next;
                free(pop);
            }
            previous = pop;
        }
    }

    struct wait_queue *kill = threads[active].wait, *temp;
    while(kill->next != NULL)
    {
        if(kill->next->id == tid)
        {
            temp = kill->next;
            kill->next = kill->next->next;
            free(temp);
        }
    }
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
    struct wait_queue *current = wq;

    while(current != NULL)
    {
        current = wq->next;
        free (wq);
        wq = current;
    }
    interrupts_set(enabled);
}

Tid
thread_sleep(struct wait_queue *queue)
{
    int enabled = interrupts_off();

    if(queue == NULL)
    {
	interrupts_set(enabled);
	return THREAD_INVALID;
    }
    else if(q_start == NULL)
    {
	interrupts_set(enabled);
	return THREAD_NONE;
    }
    else
    {
        int yield_id = q_start->id;
        int setcontext_called = 0;
        // Save the context of the caller
        getcontext(&threads[active].context);

        if(setcontext_called == 1)
        {
	    interrupts_set(enabled);
            return yield_id;
        }
        setcontext_called = 1;

	// Add the current thread to the wait q
	struct wait_queue *last = queue;
	while(last->next != NULL)
	{
            last = last->next;
	}
	last->next = wait_queue_create();
	last->next->id = active;

	// POP the first item in the ready q to run
        Tid next_id = q_start->id;
        struct wait_queue *temp = q_start->next;
        free(q_start);
        q_start = temp;
        active = next_id;
        setcontext(&threads[next_id].context);
    }
    interrupts_set(enabled);
    return THREAD_INVALID;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
    int enabled = interrupts_off();
    // if the queue is invalid or there were no threads in the queue
    if (queue == NULL || queue->next == NULL)
    {
        interrupts_set(enabled);
	return 0;
    }

    int counter = 0;
    if(all == 0)  // wake up just the first thread in the queue
    {
        // pop it from the wait_queue
        struct wait_queue *pop = queue->next;
        queue->next = pop->next;
        Tid id = pop->id;
        free(pop);

        if(q_start == NULL)
        {
            q_start = wait_queue_create();
            q_start->id = id;
            counter++;
            interrupts_set(enabled);
            return counter;
        }
        else
        {
            //and push it into the ready_queue
            struct wait_queue *push = q_start;

            for(push = q_start; push != NULL; push = push->next)
            {
                if(push->next == NULL)
                {
                    push->next = wait_queue_create();
                    push->next->id = id;
                    break;
                }
            }
            counter++;
            interrupts_set(enabled);
            return counter;
        }
    }
    else if (all == 1) // wake up all the threads in the queue
        // pop them from the wait_queue and push them into the ready_queue
    {
        struct wait_queue *queue_iter = queue;

        while(queue_iter->next != NULL)
        {
            counter++;
            queue_iter = queue_iter->next;
        }
        if(q_start == NULL)
        {
            q_start = queue->next;
            queue->next = NULL;
        }
        else
        {
            queue_iter = q_start;
            while(queue_iter->next != NULL)
            {
                queue_iter = queue_iter->next;
            }
            queue_iter->next = queue->next;
            queue->next = NULL;
        }
        interrupts_set(enabled);
        return counter;
    }
    // we should never get here
    interrupts_set(enabled);
    return 0;

}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid)
{
    int enabled = interrupts_off();

    // Check if the waiting thread is valid
    if(tid > THREAD_MAX_THREADS - 1 || tid < 0 || threads[tid].status == 0 || tid == active)
    {
	interrupts_set(enabled);
        return THREAD_INVALID;
    }

    thread_sleep(threads[tid].wait);

    interrupts_set(enabled);
    return tid;
}

struct lock {
    Tid id;
    struct wait_queue *wait;
};

struct lock *
lock_create()
{
    int enabled = interrupts_off();
    struct lock *lock;
    lock = malloc(sizeof(struct lock));
    assert(lock);
    lock->id = -1;
    lock->wait = wait_queue_create();
    interrupts_set(enabled);
    return lock;
}

void
lock_destroy(struct lock *lock)
{
    int enabled = interrupts_off();
	assert(lock != NULL);
        if(lock->id < 0)
        {
            wait_queue_destroy(lock->wait);
            free(lock);
        }
    interrupts_set(enabled);
}

void
lock_acquire(struct lock *lock)
{
    int enabled = interrupts_off();
    assert(lock != NULL);

    while(lock->id >= 0)
    {
        thread_sleep(lock->wait);
    }
    lock->id = active;
    interrupts_set(enabled);
}

void
lock_release(struct lock *lock)
{
    int enabled = interrupts_off();
    assert(lock != NULL);

    if(lock->id == active)
    {
        thread_wakeup(lock->wait, 1);
        lock->id = -1;
    }
    interrupts_set(enabled);
}

struct cv {
    struct wait_queue *wait;
};

struct cv *
cv_create()
{
    int enabled = interrupts_off();
    struct cv *cv;

    cv = malloc(sizeof(struct cv));
    assert(cv);
    cv->wait = wait_queue_create();

    interrupts_set(enabled);
    return cv;
}

void
cv_destroy(struct cv *cv)
{
    int enabled = interrupts_off();
    assert(cv != NULL);

    if(cv->wait->next == NULL)
    {
        wait_queue_destroy(cv->wait);
        free(cv);
    }
    interrupts_set(enabled);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_off();
    assert(cv != NULL);
    assert(lock != NULL);

    if(lock->id == active)
    {
        lock_release(lock);
        thread_sleep(cv->wait);
        lock_acquire(lock);
    }
    interrupts_set(enabled);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_off();
    assert(cv != NULL);
    assert(lock != NULL);

    if(lock->id == active)
    {
        thread_wakeup(cv->wait, 0);
    }
    interrupts_set(enabled);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_off();
    assert(cv != NULL);
    assert(lock != NULL);

    if(lock->id == active)
    {
        thread_wakeup(cv->wait, 1);
    }
    interrupts_set(enabled);
}
