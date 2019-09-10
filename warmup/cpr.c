#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <libgen.h>

/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

struct stat *
get_stat(const char * location)
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

int
copy_file(char * location, const char * destination){
    char buf[4096];
    int infile;
    int outfile;
    size_t ret;
    char * full_file;
    full_file = (char*)destination;

    infile = open(location, O_RDONLY);
    if (infile == -1)
        return -1;

    outfile = open(destination, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (outfile == -1)
    {
        char filename[1024];
        snprintf(filename, sizeof(filename), "%s/%s", destination, basename(location));
        outfile = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        full_file = filename;

        if (outfile == -1) {
            close(infile);
            return -1;
        }
    }

    while ((ret = read(infile, buf, sizeof(buf))) != 0)
        write(outfile, buf, ret);

    close(infile);
    close(outfile);
    int out;
    mode_t temp_mode;
    temp_mode = get_stat(location)->st_mode;
    printf("(%3o)\n", temp_mode & 0777);
    if ((out = chmod(full_file, get_stat(location)->st_mode) != 0)){
        syserror(chmod, full_file);
    }
    return 0;
}

char *
make_dir(const char *destination, const char *foldername, const mode_t mode){
    char * buf = malloc(sizeof(char) * 16384);
    snprintf(buf, sizeof(char) * 16384, "%s/%s", destination, foldername);
    //printf("Creating folder %s, dest=%s, foldername=%s\n", buf, destination, foldername);
    if (mkdir(buf, mode) != 0){
//        if (errno == EEXIST){
//            //printf("Folder %s already exists", buf);
//            return buf;
//        }
        syserror(mkdir, buf);
    }
    return buf;
}

void
make_path(const char *destination, const mode_t mode){
    if (mkdir(destination, mode) != 0){
//        if (errno == EEXIST){
//            //printf("Folder %s already exists", buf);
//            return buf;
//        }
        syserror(mkdir, destination);
    }
}

void
copy_dir(const char *location, const char *destination, int indent)
{
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(location))){
        syserror(opendir, location);
    }

    while ((entry = readdir(dir)) != NULL) {
        char buf[16384];
        snprintf(buf, sizeof(char) * 16384, "%s/%s", location, entry->d_name);
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            //printf("%*s[%s]\n", indent, "", entry->d_name);

            char * created_dir;
            struct stat * loc_stat = get_stat(location);
            created_dir = make_dir(destination, entry->d_name, S_IRUSR | S_IWUSR | S_IXUSR);
            free(loc_stat);

            copy_dir(buf, created_dir, indent + 2);
            int out;
            if ((out = chmod(buf, loc_stat->st_mode) != 0)){
                syserror(chmod, buf);
            }

        } else {
            //printf("%*s- %s  ---  %s\n", indent, "", entry->d_name, location);
            copy_file(buf, destination);
        }
    }
    closedir(dir);
}

bool
is_file(struct stat * buf){
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
    buf = get_stat(argv[1]);
    if (is_file(buf)){
        if (copy_file(argv[1], argv[2]) != 0){
            free(buf);
            syserror(open, argv[1]);
        }
    }
    else {
        make_path(argv[2], S_IRUSR | S_IWUSR | S_IXUSR);
        copy_dir(argv[1], argv[2], 8);
        chmod(argv[2], buf->st_mode);
    }
    free(buf);
}
