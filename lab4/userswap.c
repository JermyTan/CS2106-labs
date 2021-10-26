#include "userswap.h"
#include <stdio.h>
#include <stdlib.h>
// #include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096
#define DEFAULT_LORM 2106 * PAGE_SIZE

typedef struct MEMINFO
{
    void *address;
    size_t size;
    struct MEMINFO *next;
} meminfo;

typedef struct
{
    meminfo *head;
    meminfo *tail;
} list;

static void enqueue(list *queue, meminfo *new_meminfo)
{
    if (!queue->head)
    {
        queue->head = new_meminfo;
        queue->tail = new_meminfo;
        return;
    }

    queue->tail->next = new_meminfo;
    queue->tail = new_meminfo;
}

static meminfo *dequeue(list *queue, void *address)
{
    if (!queue->head)
    {
        return NULL;
    }

    meminfo *removed_meminfo = NULL;

    if (queue->head->address == address)
    {
        removed_meminfo = queue->head;

        if (queue->head == queue->tail)
        {
            queue->head = NULL;
            queue->tail = NULL;
        }
        else
        {
            queue->head = queue->head->next;
        }
    }
    else
    {
        meminfo *previous_meminfo = queue->head;
        meminfo *current_memoinfo = previous_meminfo->next;

        while (current_memoinfo)
        {
            if (current_memoinfo->address == address)
            {
                removed_meminfo = current_memoinfo;
                previous_meminfo->next = current_memoinfo->next;
                break;
            }

            previous_meminfo = current_memoinfo;
            current_memoinfo = current_memoinfo->next;
        }
    }

    removed_meminfo->next = NULL;
    return removed_meminfo;
}

meminfo *get_meminfo(list *queue, void *address)
{
    meminfo *current_memoinfo = queue->head;

    while (current_memoinfo)
    {
        if (current_memoinfo->address == address)
        {
            break;
        }
        current_memoinfo = current_memoinfo->next;
    }

    return current_memoinfo;
}

static int is_controlled_mem_region(list *queue, void *address)
{
    meminfo *current_memoinfo = queue->head;

    while (current_memoinfo)
    {
        if (current_memoinfo->address <= address && address < current_memoinfo->address + current_memoinfo->size)
        {
            return 1;
        }
        current_memoinfo = current_memoinfo->next;
    }

    return 0;
}

static size_t lorm = DEFAULT_LORM;
static size_t total_allocated_mem_size = 0;
static list meminfo_queue = {NULL, NULL};

static int check_syscall(int value, const char *error_msg)
{
    if (value < 0)
    {
        perror(error_msg);
    }
    return value;
}

static size_t round_up_mem_size(size_t size)
{
    return size % PAGE_SIZE ? (size / PAGE_SIZE + 1) * PAGE_SIZE : size;
}

static void page_fault_handler(void *address)
{
    check_syscall(mprotect(address, PAGE_SIZE, PROT_READ), "page_fault_handler: mprotect error");
}

static void teardown_sig_handler(int signum)
{
    struct sigaction sa;
    sa.sa_flags = NULL;
    check_syscall(sigemptyset(&sa.sa_mask), "teardown_sig_handler: sigemptyset error");
    sa.sa_handler = SIG_DFL;

    check_syscall(sigaction(signum, &sa, NULL), "teardown_sig_handler: sigaction error");
}

static void sigsegv_handler(int signum, siginfo_t *si, void *unused)
{
    if (signum != SIGSEGV || !is_controlled_mem_region(&meminfo_queue, si->si_addr))
    {
        teardown_sig_handler(signum);
        check_syscall(raise(signum), "sigsegv_handler: raise error");
        return;
    }

    page_fault_handler(si->si_addr);
}

static void setup_sigsegv_handler()
{
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    check_syscall(sigemptyset(&sa.sa_mask), "setup_sigsegv_handler: sigemptyset error");
    sa.sa_sigaction = sigsegv_handler;

    check_syscall(sigaction(SIGSEGV, &sa, NULL), "setup_sigsegv_handler: sigaction error");
}

void userswap_set_size(size_t size)
{
    lorm = size;
}

void *userswap_alloc(size_t size)
{
    setup_sigsegv_handler();

    size_t rounded_size = round_up_mem_size(size);
    void *address = mmap(NULL, rounded_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (address == MAP_FAILED)
    {
        check_syscall(-1, "userswap_alloc: mmap error");
    }

    meminfo *new_meminfo = (meminfo *)malloc(sizeof(meminfo));
    new_meminfo->address = address;
    new_meminfo->size = rounded_size;
    new_meminfo->next = NULL;

    enqueue(&meminfo_queue, new_meminfo);

    total_allocated_mem_size += rounded_size;

    return address;
}

void userswap_free(void *mem)
{
    meminfo *removed_meminfo = dequeue(&meminfo_queue, mem);

    check_syscall(munmap(removed_meminfo->address, removed_meminfo->size), "userswap_free: munmap error");

    total_allocated_mem_size -= removed_meminfo->size;

    free(removed_meminfo);
}

void *userswap_map(int fd, size_t size)
{
    setup_sigsegv_handler();

    return NULL;
}
