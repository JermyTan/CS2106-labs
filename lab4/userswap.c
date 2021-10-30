#include "userswap.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096
#define NUM_PAGE_TABLE_ENTRIES 512
#define DEFAULT_LORM 2106 * PAGE_SIZE
#define NUM_OFFSET_BITS 12
#define NUM_PAGE_TABLE_INDEX_BITS 9
#define PAGE_TABLE_INDEX_MASK NUM_PAGE_TABLE_ENTRIES - 1
#define LEVEL_ONE 3
#define LEVEL_TWO 2
#define LEVEL_THREE 1
#define LEVEL_FOUR 0

typedef struct
{
    void *address;
} node_data;

typedef struct NODE
{
    void *data;
    struct NODE *next;
} node;

typedef struct
{
    node *head;
    node *tail;
} list;

typedef struct MEM_REGION
{
    void *address;
    size_t size;
} mem_region;
typedef struct
{
    void *address;
    int is_resident;
    int is_dirty;
} page_table_entry;

typedef struct
{
    int num_filled_entries;
    void *entries[NUM_PAGE_TABLE_ENTRIES];
} page_table;

static void enqueue(list *queue, void *data)
{
    node *new_node = (node *)malloc(sizeof(node));
    new_node->data = data;
    new_node->next = NULL;

    if (!queue->head)
    {
        queue->head = new_node;
        queue->tail = new_node;
        return;
    }

    queue->tail->next = new_node;
    queue->tail = new_node;
}

static void *dequeue(list *queue, void *address)
{
    if (!queue->head)
    {
        return NULL;
    }

    node *removed_node = NULL;

    if (!address || ((node_data *)queue->head->data)->address == address)
    {
        removed_node = queue->head;

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
        node *previous_node = queue->head;
        node *current_node = previous_node->next;

        while (current_node)
        {
            if (((node_data *)current_node->data)->address == address)
            {
                removed_node = current_node;
                previous_node->next = current_node->next;
                break;
            }

            previous_node = current_node;
            current_node = current_node->next;
        }
    }

    if (!removed_node)
    {
        return NULL;
    }

    void *data = removed_node->data;
    free(removed_node);

    return data;
}

static int is_controlled_mem_region(list *queue, void *address)
{
    node *current_node = queue->head;

    while (current_node)
    {
        if (((mem_region *)current_node->data)->address <= address && address < ((mem_region *)current_node->data)->address + ((mem_region *)current_node->data)->size)
        {
            return 1;
        }
        current_node = current_node->next;
    }

    return 0;
}

static const int LEVELS[] = {LEVEL_ONE, LEVEL_TWO, LEVEL_THREE, LEVEL_FOUR};
static size_t lorm = DEFAULT_LORM;
static size_t total_resident_mem_size = 0;
static list mem_region_queue = {NULL, NULL};
static list page_table_entry_queue = {NULL, NULL};
static page_table page_table_directory = {0, {NULL}};

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

static void compute_page_table_level_indices(void *address, int page_table_level_indices[])
{
    size_t address_value = (size_t)address;
    address_value >>= NUM_OFFSET_BITS;

    for (int i = 0; i < 4; i++)
    {
        page_table_level_indices[LEVELS[i]] = address_value & (PAGE_TABLE_INDEX_MASK);
        address_value >>= NUM_PAGE_TABLE_INDEX_BITS;
    }
}

static page_table *get_page_table(int page_table_level_indices[])
{
    page_table *table = &page_table_directory;

    for (int i = 0; i < 3; i++)
    {
        if (!table->entries[page_table_level_indices[i]])
        {
            page_table *new_table = (page_table *)malloc(sizeof(page_table));
            new_table->num_filled_entries = 0;
            for (int i = 0; i < NUM_PAGE_TABLE_ENTRIES; i++)
            {
                new_table->entries[i] = NULL;
            }

            table->entries[page_table_level_indices[i]] = new_table;
            table->num_filled_entries += 1;
        }

        table = table->entries[page_table_level_indices[i]];
    }

    return table;
}

static void evict_page(void *address)
{
    page_table_entry *evicted_entry = (page_table_entry *)dequeue(&page_table_entry_queue, address);

    if (!evicted_entry)
    {
        return;
    }

    check_syscall(mprotect(evicted_entry->address, PAGE_SIZE, PROT_NONE), "evict_page: mprotect PROT_NONE error");

    evicted_entry->is_resident = 0;
    evicted_entry->is_dirty = 0;
    total_resident_mem_size -= PAGE_SIZE;
}

static void page_fault_handler(void *address)
{
    int page_table_level_indices[4];
    compute_page_table_level_indices(address, page_table_level_indices);

    page_table *table = get_page_table(page_table_level_indices);

    page_table_entry *entry = table->entries[page_table_level_indices[LEVEL_ONE]];

    if (!entry->is_resident)
    {
        while (total_resident_mem_size >= lorm)
        {
            evict_page(NULL);
        }

        check_syscall(mprotect(address, PAGE_SIZE, PROT_READ), "page_fault_handler: mprotect PROT_READ error");

        enqueue(&page_table_entry_queue, entry);

        entry->is_resident = 1;
        total_resident_mem_size += PAGE_SIZE;
    }
    else
    {
        check_syscall(mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE), "page_fault_handler: mprotect PROT_READ | PROT_WRITE error");
        entry->is_dirty = 1;
    }
}

static void teardown_sig_handler(int signum)
{
    struct sigaction sa;
    sa.sa_flags = 0;
    check_syscall(sigemptyset(&sa.sa_mask), "teardown_sig_handler: sigemptyset error");
    sa.sa_handler = SIG_DFL;

    check_syscall(sigaction(signum, &sa, NULL), "teardown_sig_handler: sigaction error");
}

static void sigsegv_handler(int signum, siginfo_t *si, void *unused)
{
    if (signum != SIGSEGV || !is_controlled_mem_region(&mem_region_queue, si->si_addr))
    {
        teardown_sig_handler(signum);
        check_syscall(raise(signum), "sigsegv_handler: raise error");
        return;
    }

    page_fault_handler(si->si_addr);
}

static void setup_sigsegv_handler(void)
{
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    check_syscall(sigemptyset(&sa.sa_mask), "setup_sigsegv_handler: sigemptyset error");
    sa.sa_sigaction = sigsegv_handler;

    check_syscall(sigaction(SIGSEGV, &sa, NULL), "setup_sigsegv_handler: sigaction error");
}

static void insert_page_table_entry(void *address)
{
    int page_table_level_indices[4];
    compute_page_table_level_indices(address, page_table_level_indices);

    page_table *table = get_page_table(page_table_level_indices);

    page_table_entry *new_entry = (page_table_entry *)malloc(sizeof(page_table_entry));
    new_entry->address = address;
    new_entry->is_resident = 0;
    new_entry->is_dirty = 0;

    table->entries[page_table_level_indices[LEVEL_ONE]] = new_entry;
    table->num_filled_entries += 1;
}

static void clean_up_page_tables(int level, page_table *table, int page_table_level_indices[])
{
    int index = page_table_level_indices[level];

    if (level < LEVEL_ONE)
    {
        clean_up_page_tables(level + 1, table->entries[index], page_table_level_indices);
    }

    if (level == LEVEL_ONE || ((page_table *)table->entries[index])->num_filled_entries == 0)
    {
        free(table->entries[index]);
        table->entries[index] = NULL;
        table->num_filled_entries -= 1;
    }
}

static void remove_page_table_entry(void *address)
{
    int page_table_level_indices[4];
    compute_page_table_level_indices(address, page_table_level_indices);

    clean_up_page_tables(LEVEL_FOUR, &page_table_directory, page_table_level_indices);
}

static void apply_page_action(void *address, size_t size, void (*action)(void *))
{
    size_t num_pages = size / PAGE_SIZE;

    while (num_pages--)
    {
        action(address);
        address += PAGE_SIZE;
    }
}

static void store_mem_region(void *address, size_t size)
{
    mem_region *new_mem_region = (mem_region *)malloc(sizeof(mem_region));
    new_mem_region->address = address;
    new_mem_region->size = size;

    enqueue(&mem_region_queue, new_mem_region);
}

void userswap_set_size(size_t size)
{
    lorm = size;

    while (total_resident_mem_size > lorm)
    {
        evict_page(NULL);
    }
}

void *userswap_alloc(size_t size)
{
    setup_sigsegv_handler();

    size = round_up_mem_size(size);
    void *address = mmap(NULL, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (address == MAP_FAILED)
    {
        check_syscall(-1, "userswap_alloc: mmap error");
    }

    apply_page_action(address, size, insert_page_table_entry);

    store_mem_region(address, size);

    return address;
}

void userswap_free(void *mem)
{
    mem_region *removed_mem_region = (mem_region *)dequeue(&mem_region_queue, mem);

    apply_page_action(removed_mem_region->address, removed_mem_region->size, evict_page);

    check_syscall(munmap(removed_mem_region->address, removed_mem_region->size), "userswap_free: munmap error");

    apply_page_action(removed_mem_region->address, removed_mem_region->size, remove_page_table_entry);

    free(removed_mem_region);
}

void *userswap_map(int fd, size_t size)
{
    setup_sigsegv_handler();

    return NULL;
}
