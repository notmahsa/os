#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include <stdbool.h>
#include "thread.h"
#include "test_thread.h"

void
thread_stubbb(void (*fn)){
    fn();
    // fn(parg);
    // thread_exit();
    // thread_exit();
}

int setcontext_called = 0;
int
main(int argc, char **argv)
{
//	thread_init();
//	test_basic();
	if (setcontext_called == 0){
	    int err;
	    ucontext_t * new_context = malloc(sizeof(ucontext_t));
        err = getcontext(new_context);
        assert(!err);
        new_context->uc_stack.ss_sp = malloc(1024);
        new_context->uc_stack.ss_size = 1024;
        new_context->uc_stack.ss_flags = 0;
        new_context->uc_link = 0;
        new_context->uc_mcontext.gregs[REG_RIP] = (long long)thread_stubbb;
        new_context->uc_mcontext.gregs[REG_RSI] = (long long)&main;
        // new_context->uc_mcontext.gregs[REG_RDI] = (long long)NULL;
        setcontext_called = 1;
        setcontext(new_context);
    }
    else {
        printf("Got here!\n");
    }

	return 0;
}
