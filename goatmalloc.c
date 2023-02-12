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
//implicit, so every section is mapped, not just the free areas
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

void *walloc(size_t size) {
    if (arena_start == NULL) {
        puts("Error: Unitialized. Setting status code");
        statusno = ERR_UNINITIALIZED;
        return NULL;
    }
    puts("Allocating memory:");
    printf("...looking for free chunk of >= %ld bytes\n", size);

    node_t *curNode, *prev;
    void *result;
//    size_t totalSize = size + NODE_SIZE;
    prev = NULL;

    for (curNode = freeList; curNode != NULL; prev = curNode, curNode = curNode->fwd) {
        if (curNode->size >= size && curNode->is_free) {
            printf("...found free chunk of %ld bytes with header at %p\n", curNode->size, curNode);
            printf("...free chunk->fwd currently points to %p\n", curNode->fwd);
            printf("...free chunk->bwd currently points to %p\n", curNode->bwd);
            short split = 0;
            //block can accomodate us, so make node header and return header + 1 as void*.
            //edge case ignored: if the resulting space is less than 32 bytes no header will be created. should call sbrk
            puts("...checking if splitting is required");
            //if too big and we have space for node_size +1, we split
            // must have at least one byte, otherwise useless. arenasize - (total)size >= nodesize+1
            int leftOver = 0;
            if (curNode->size > size && (leftOver = curNode->size - size) >= NODE_SIZE + 1) {
                split = 1;
                node_t *tempNextNode = curNode->fwd;
                curNode->fwd = (void *) (curNode + 1) + size;
                //populate this new node
                //taking free memory away from the current free block, along with the header size
                curNode->fwd->size = curNode->size - size - NODE_SIZE;
                curNode->fwd->is_free = 1;
                curNode->fwd->fwd = tempNextNode;
                curNode->fwd->bwd = curNode;
            }

            if (split) {
                puts("...splitting free chunk");
            } else {
                puts("...splitting not required");
            }
            printf("...updating chunk header at %p\n", curNode);
            puts("...being careful with my pointer arthimetic and void pointer casting");
            result = curNode + 1;
            if (leftOver > NODE_SIZE) { //if you have room for another block, add it, if not dont waste space
                curNode->size = size;
            }
            curNode->is_free = 0;
            curNode->bwd = prev;
            printf("Allocation starts at %p\n", result);
            break;//if we dont break then cur node will = NULL, and prev will be
        }
    }
    if (curNode == NULL) {
        puts("...no such free chunk exists");
        puts("...setting error code");
        statusno = ERR_OUT_OF_MEMORY;
        return NULL;
    }
    return result;
}

void wfree(void *ptr) {
    //seek to metadata
    puts("Freeing allocated memory:");
    printf("...supplied pointer %p:\n", ptr);
    puts("...being careful with my pointer arthimetic and void pointer casting");
    node_t *metadata = ptr - NODE_SIZE;
    printf("accessing chunk header at %p", metadata);
    printf("chunk size of %lu", metadata->size);
    metadata->is_free = 1;
    puts("...checking if coalescing is needed");
    int coalesce = 0;
    node_t *prevNode = metadata->bwd;
    node_t *nextNode = metadata->fwd;
    if (prevNode != NULL && prevNode->is_free) {
        coalesce = 1;
        prevNode->size += metadata->size + NODE_SIZE;
        prevNode->fwd = nextNode;
    }
    if (nextNode != NULL && nextNode->is_free) {
        coalesce = 1;
        prevNode->size += metadata->size + NODE_SIZE;
        prevNode->bwd = prevNode;
    }
    if (coalesce) {
        puts("...coalescing needed.");
    } else {
        puts("...coalescing not needed.");
    }
}

//int main() {
//    init(PAGE_SIZE);
//    void *buff1 = walloc(64);
//
//    int size = PAGE_SIZE - 64 - (sizeof(node_t) * 2) - 10;
//    void *buff2 = walloc(size);
//    node_t *header2 = (node_t *) (buff2 - sizeof(node_t));
//
//    printf("buff 1 size: %zu\n", header2->size);
//    printf("buff 1 next: %p\n", header2->fwd);
//    printf("buff 1 prev: %p\n", header2->bwd);
//    printf("buff 1 is free: %d\n", header2->is_free);
//}