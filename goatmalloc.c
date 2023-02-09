#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "goatmalloc.h"

void *_arena_start;

int init(size_t size) {
    //size_t is unsigned, so check msb
    if (size >> 63 != 0) {
        return ERR_BAD_ARGUMENTS;
    }
    int pageSize = getpagesize();
    long numPages = (long) ((size / (pageSize + 1)) + 1);
    long arenaSize = numPages * pageSize;
    int fd=open("/dev/zero",O_RDWR);
    _arena_start = mmap(NULL, arenaSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    puts("Initializing arena:");
    printf("...requested size %d bytes\n", size);
    printf("...pagesize is %d bytes\n", pageSize);
    printf("...adjusting size with page boundaries\n");
    printf("...adjusted size is %d bytes\n", arenaSize);
    printf("...mapping arena with mmap()\n");
    printf("...arena starts at %p\n", _arena_start);
    printf("...arena ends at %p\n",_arena_start[arenaSize-1]);
}

int main() {
    init(-1);
}