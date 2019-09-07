#include "common.h"

int
main(int argc, char **argv)
{
    if (argc < 2)
    {
        return 0;
    }

	for (int i = 1; i < argc; ++i)
    {
        printf("%s\n", argv[i]);
    }
	return 0;
}
