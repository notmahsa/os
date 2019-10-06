#include "thread.h"
#include "test_thread.h"

int setcontext_called = 0;
int
main(int argc, char **argv)
{
//	thread_init();
//	test_basic();
	if (setcontext_called == 0){
	ucontext_t * new_context = malloc(sizeof(ucontext_t));
        err = getcontext(new_context);
        assert(!err);
        new_context->uc_stack.ss_sp = new_stack;
        new_context->uc_stack.ss_size = THREAD_MIN_STACK;
        new_context->uc_stack.ss_flags = 0;
        new_context->uc_link = 0;
        new_context->uc_mcontext.gregs[REG_RIP] = (long long)main;
        new_context->uc_mcontext.gregs[REG_RSI] = 0;
        new_context->uc_mcontext.gregs[REG_RDI] = NULL;
        setcontext_called = 1;
        setcontext(new_context);
    }
    else {
        printf("Got here!\n");
    }

	return 0;
}
