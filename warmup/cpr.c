#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	}

	char buffer[1024];
    int files[2];
    size_t count;

    files[0] = open(argv[1], O_RDONLY);
    if (files[0] == -1) /* Check if file opened */
        syserror(open, argv[1]);
        return -1;
    files[1] = open(argv[2], O_WRONLY | O_CREAT | S_IRUSR | S_IWUSR);
    if (files[1] == -1) /* Check if file opened (permissions problems ...) */
    {
        close(files[0]);
        return -1;
    }

    while ((count = read(files[0], buffer, sizeof(buffer))) != 0)
        write(files[1], buffer, count);
	return 0;
}
