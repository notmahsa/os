#include "common.h"

int
main(int argc, char **argv)
{
	for (int i = 0; i < argc; ++i)
    {
        printf("\n%s", argv[i]);
    }
	return 0;
}
