#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "goatmalloc.h"

#define PAGE_SIZE getpagesize()
#define NODE_SIZE sizeof(node_t)

void *arena_start = NULL;
//the number of PAGE_SIZE pages allocated
long numPages;
//the total size of the allocation block. Will be a multiple of PAGE_SIZE
long arenaSize;
int statusno;
node_t *freeList;

int init(size_t size) {
    puts("Initializing arena:");
    printf("...requested size %lu bytes\n", size);

    if (size >> 63 != 0) {
        return ERR_BAD_ARGUMENTS;
    }
    if (size > MAX_ARENA_SIZE) {
        printf("...error: requested size larger than MAX_ARENA_SIZE (2147483647)");
    }

    numPages = (long) ((size / (PAGE_SIZE + 1)) + 1);
    arenaSize = numPages * PAGE_SIZE;

    printf("...pagesize is %d bytes\n", PAGE_SIZE);
    if (size % PAGE_SIZE != 0) {
        printf("...adjusting size with page boundaries\n");
    }

    printf("...adjusted size is %ld bytes\n", arenaSize);
    printf("...mapping arena with mmap()\n");

    int fd = open("/dev/zero", O_RDWR);
    arena_start = mmap(NULL, arenaSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    if (arena_start == MAP_FAILED) {
        perror("map failed");
        return ERR_SYSCALL_FAILED;
    }

    printf("...arena starts at %p\n", arena_start);
    printf("...arena ends at %p\n", arena_start + arenaSize);

    puts("...initializing header for initial free chunk");

    freeList = arena_start;
    freeList->size = arenaSize - NODE_SIZE;
    freeList->is_free = 1;
    freeList->bwd = NULL;
    freeList->fwd = NULL;

    printf("...header size is %ld bytes\n", NODE_SIZE);
    return arenaSize;
}

int destroy() {
    if (arena_start == NULL) {
        puts("...error: cannot destroy unintialized arena. Setting error status");
        return ERR_UNINITIALIZED;
    }
    puts("Destroying Arena:");
    puts("...unmapping arena with munmap()");
    if (munmap(arena_start, arenaSize) == -1) {
        perror("munmap failed");
        return ERR_SYSCALL_FAILED;
    }
    arena_start = NULL;
    arenaSize = 0;
    return 0;
}

void* walloc(size_t size) {
    if (arena_start == NULL) {
        puts("Error: Unitialized. Setting status code");
        statusno = ERR_UNINITIALIZED;
        return NULL;
    }
    puts("Allocating memory:");
    printf("...looking for free chunk of >= %ld bytes\n", size);
    node_t *curNode;
    //will have to keep track of prev and size of new cur node
    for (curNode = freeList; curNode != NULL; curNode = curNode->fwd) {
        if (freeList->size < size) {
            puts("...no such free chunk exists");
            puts("...setting error code");
            statusno = ERR_OUT_OF_MEMORY;
            return NULL;
        }
    }
    return NULL;
}