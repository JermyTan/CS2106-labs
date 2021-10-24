#include "userswap.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096
#define DEFAULT_LORM 2106 * PAGE_SIZE

static size_t lorm = DEFAULT_LORM;

static int check_syscall(int value, const char *error_msg)
{
    if (value < 0)
    {
        perror(error_msg);
    }
    return value;
}

size_t round_up_mem_size(size_t size)
{
    return size % PAGE_SIZE ? (size / PAGE_SIZE + 1) * PAGE_SIZE : size;
}

void userswap_set_size(size_t size)
{
    lorm = size;
}

void *userswap_alloc(size_t size)
{
    size_t rounded_size = round_up_mem_size(size);

    return check_syscall(mmap(NULL, rounded_size, PROT_NONE, MAP_ANONYMOUS), "userswap_alloc: mmap error");
}

void userswap_free(void *mem)
{
}

void *userswap_map(int fd, size_t size)
{
    return NULL;
}
