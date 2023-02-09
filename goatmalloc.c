#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "goatmalloc.h"

#define PAGE_SIZE getpagesize()

void *_arena_start;
//the number of PAGE_SIZE pages allocated
long numPages;
//the total size of the allocation block. Will be a multiple of PAGE_SIZE
long arenaSize;

int init(size_t size) {
    //size_t is unsigned, so check msb
    puts("Initializing arena:");
    printf("...requested size %d bytes\n", size);

    if (size >> 63 != 0) {
        return ERR_BAD_ARGUMENTS;
    }
    if (size > MAX_ARENA_SIZE){
        printf("...error: requested size larger than MAX_ARENA_SIZE (2147483647)")
    }
    numPages = (long) ((size / (PAGE_SIZE + 1)) + 1);
    arenaSize = numPages * PAGE_SIZE;

    int fd=open("/dev/zero",O_RDWR);
    _arena_start = mmap(NULL, arenaSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    printf("...pagesize is %d bytes\n", PAGE_SIZE);
    printf("...adjusting size with page boundaries\n");
    //this is an if statement if need to remap^^^
    printf("...adjusted size is %d bytes\n", arenaSize);
    printf("...mapping arena with mmap()\n");
    printf("...arena starts at %p\n", _arena_start);
    //void pointer arithmetic is non-standard, casting to char so that sizeof(ptr) returns 1 byte
    printf("...arena ends at %p\n",(char *) _arena_start + arenaSize);

    puts("...initializing header for initial free chunk");
    printf("...header size is %d bytes\n",32);
}

int destroy(){
    puts("Destroying Arena:");
    munmap(_arena_start,arenaSize);
    puts("...unmapping arena with munmap()");
}

int main() {
    init(1);
}