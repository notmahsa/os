#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <ftw.h>

/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

int
copy_file(char* location, char* destination){
    char buf[4096];
    int infile;
    int outfile;
    size_t ret;

    infile = open(location, O_RDONLY);
    if (infile == -1) /* Check if file opened */
        return -1;

    outfile = open(destination, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (outfile == -1) /* Check if file opened (permissions problems ...) */
    {
        close(infile);
        return -1;
    }

    while ((ret = read(infile, buf, sizeof(buf))) != 0)
        write(outfile, buf, ret);

    close(infile);
    close(outfile);
    return 0;
}

//void
//list_dir(const char *location, int indent)
//{
//    DIR *dir;
//    struct dirent *entry;
//
//    if (!(dir = opendir(location)))
//        return;
//
//    while ((entry = readdir(dir)) != NULL) {
//        if (entry->d_type == DT_DIR) {
//            char path[1024];
//            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
//                continue;
//            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
//            printf("%*s[%s]\n", indent, "", entry->d_name);
//            listdir(path, indent + 2);
//        } else {
//            printf("%*s- %s\n", indent, "", entry->d_name);
//        }
//    }
//    closedir(dir);
//}

struct stat * get_buf(char * location)
{
    struct stat * buf;
    buf = (struct stat *)malloc(sizeof(struct stat));
    int stat_out;
    stat_out = stat(location, buf);
    if (stat_out != 0){
        free(buf);
        syserror(stat, location);
    }
    return buf;
}

bool is_file(struct stat * buf){
    bool verdict;
    switch (buf->st_mode & S_IFMT) {
        case S_IFDIR:  verdict = false; break;
        case S_IFREG:  verdict = true; break;
    }
    return verdict;
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	}
    struct stat * buf;
    buf = get_buf(argv[1]);
    printf("%d", is_file(buf));

//    if (copy_file(argv[1], argv[2]) != 0){
//        syserror(open, argv[1]);
//    }
    free(buf);


}
