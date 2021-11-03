#include "userswap.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

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
#define NOT_ASSIGNED -1

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
    bool is_resident;
    bool is_dirty;
    int secondary_storage_fd;
    off_t secondary_storage_location;
} page_table_entry;

typedef struct
{
    unsigned short int num_filled_entries;
    void *entries[NUM_PAGE_TABLE_ENTRIES];
} page_table;

typedef struct
{
    size_t offset;
    void *context;
} page_action_context;

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

static bool is_controlled_mem_region(list *queue, void *address)
{
    node *current_node = queue->head;

    while (current_node)
    {
        if (((mem_region *)current_node->data)->address <= address && address < ((mem_region *)current_node->data)->address + ((mem_region *)current_node->data)->size)
        {
            return true;
        }
        current_node = current_node->next;
    }

    return false;
}

static const int LEVELS[] = {LEVEL_ONE, LEVEL_TWO, LEVEL_THREE, LEVEL_FOUR};
static size_t lorm = DEFAULT_LORM;
static size_t total_resident_mem_size = 0;
static bool has_setup_sigsegv_handler = false;
static list mem_region_queue = {NULL, NULL};
static list page_table_entry_queue = {NULL, NULL};
static list available_swap_file_location_queue = {NULL, NULL};
static page_table page_table_directory = {0, {NULL}};
static int swap_file_fd = NOT_ASSIGNED;
static size_t swap_file_size = 0;

/* helper function prototypes */
static size_t round_up_mem_size(size_t size);
static void setup_sigsegv_handler(void);
static void sigsegv_handler(int signum, siginfo_t *si, void *unused);
static void teardown_sig_handler(int signum);
static void page_fault_handler(void *address);
static void compute_page_table_level_indices(void *address, int page_table_level_indices[]);
static page_table *get_page_table(int page_table_level_indices[]);
static void evict_page(void *address);
static void swap_from_file(page_table_entry *entry);
static void swap_to_file(page_table_entry *entry);
static void apply_page_action(void *address, size_t size, void *(*action)(void *address, page_action_context *action_context), void *context);
static void store_mem_region(void *address, size_t size);
static page_table_entry *insert_default_page_table_entry(void *address, page_action_context *action_context);
static page_table_entry *insert_mapped_file_page_table_entry(void *address, page_action_context *action_context);
static void *clean_up_page(void *address, page_action_context *action_context);
static void free_swap_file_location(page_table_entry *entry);
static void clean_up_page_tables(int level, page_table *table, int page_table_level_indices[]);

static int check_syscall(int value, const char *error_msg)
{
    if (value < 0)
    {
        perror(error_msg);
    }
    return value;
}

// static void display_library_statistics(void)
// {
//     printf("LORM: %zu\n", lorm);

//     printf("Total resident mem: %zu\n", total_resident_mem_size);

//     size_t num_mem_regions = 0;
//     node *current_node = mem_region_queue.head;
//     while (current_node)
//     {
//         num_mem_regions++;
//         current_node = current_node->next;
//     }
//     printf("Num controlled mem regions: %zu\n", num_mem_regions);

//     size_t num_page_table_entries = 0;
//     current_node = page_table_entry_queue.head;
//     while (current_node)
//     {
//         num_page_table_entries++;
//         current_node = current_node->next;
//     }
//     printf("Num page table entries: %zu\n", num_page_table_entries);

//     size_t num_available_swap_file_locations = 0;
//     current_node = available_swap_file_location_queue.head;
//     while (current_node)
//     {
//         num_available_swap_file_locations++;
//         current_node = current_node->next;
//     }
//     printf("Num available swap file locations: %zu\n", num_available_swap_file_locations);

//     printf("Page table directory size: %d\n", page_table_directory.num_filled_entries);

//     printf("Swap file fd: %d\n", swap_file_fd);

//     printf("Swap file size: %zu\n", swap_file_size);
// }

void userswap_set_size(size_t size)
{
    lorm = round_up_mem_size(size);

    while (total_resident_mem_size > lorm)
    {
        evict_page(NULL);
    }
}

void *userswap_alloc(size_t size)
{
    setup_sigsegv_handler();

    size_t rounded_size = round_up_mem_size(size);
    void *address = mmap(NULL, rounded_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, NOT_ASSIGNED, 0);

    if (address == MAP_FAILED)
    {
        check_syscall(NOT_ASSIGNED, "userswap_alloc: mmap error");
    }

    apply_page_action(address, rounded_size, (void *(*)(void *, page_action_context *))insert_default_page_table_entry, NULL);

    store_mem_region(address, rounded_size);

    return address;
}

void userswap_free(void *mem)
{
    mem_region *removed_mem_region = (mem_region *)dequeue(&mem_region_queue, mem);

    apply_page_action(removed_mem_region->address, removed_mem_region->size, clean_up_page, NULL);

    check_syscall(munmap(removed_mem_region->address, removed_mem_region->size), "userswap_free: munmap error");

    free(removed_mem_region);

    // reset in-memory swap file state when there is no controlled mem regions
    if (!mem_region_queue.head)
    {
        while (available_swap_file_location_queue.head)
        {
            free(dequeue(&available_swap_file_location_queue, NULL));
            swap_file_size -= PAGE_SIZE;
        }
    }

    // display_library_statistics();
}

void *userswap_map(int fd, size_t size)
{
    setup_sigsegv_handler();

    size_t rounded_size = round_up_mem_size(size);
    void *address = mmap(NULL, rounded_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, NOT_ASSIGNED, 0);

    if (address == MAP_FAILED)
    {
        check_syscall(NOT_ASSIGNED, "userswap_map: mmap error");
    }

    check_syscall(pread(fd, address, rounded_size, 0), "userswap_map: pread error");
    check_syscall(pwrite(fd, address, rounded_size, 0), "userswap_map: pwrite error");
    check_syscall(mprotect(address, rounded_size, PROT_NONE), "userswap_map: mprotect PROT_NONE error");

    apply_page_action(address, rounded_size, (void *(*)(void *, page_action_context *))insert_mapped_file_page_table_entry, &fd);

    store_mem_region(address, rounded_size);

    return address;
}

static size_t round_up_mem_size(size_t size)
{
    return size % PAGE_SIZE ? (size / PAGE_SIZE + 1) * PAGE_SIZE : size;
}

static void setup_sigsegv_handler(void)
{
    if (has_setup_sigsegv_handler)
    {
        return;
    }

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    check_syscall(sigemptyset(&sa.sa_mask), "setup_sigsegv_handler: sigemptyset error");
    sa.sa_sigaction = sigsegv_handler;

    check_syscall(sigaction(SIGSEGV, &sa, NULL), "setup_sigsegv_handler: sigaction error");

    has_setup_sigsegv_handler = true;
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

static void teardown_sig_handler(int signum)
{
    struct sigaction sa;
    sa.sa_flags = 0;
    check_syscall(sigemptyset(&sa.sa_mask), "teardown_sig_handler: sigemptyset error");
    sa.sa_handler = SIG_DFL;

    check_syscall(sigaction(signum, &sa, NULL), "teardown_sig_handler: sigaction error");
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

        if (entry->secondary_storage_location != NOT_ASSIGNED)
        {
            swap_from_file(entry);
        }

        check_syscall(mprotect(entry->address, PAGE_SIZE, PROT_READ), "page_fault_handler: mprotect PROT_READ error");

        enqueue(&page_table_entry_queue, entry);

        entry->is_resident = true;
        total_resident_mem_size += PAGE_SIZE;
    }
    else
    {
        check_syscall(mprotect(entry->address, PAGE_SIZE, PROT_READ | PROT_WRITE), "page_fault_handler: mprotect PROT_READ | PROT_WRITE error");
        entry->is_dirty = true;
    }
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
            table->num_filled_entries++;
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

    if (evicted_entry->is_dirty)
    {
        swap_to_file(evicted_entry);
    }

    check_syscall(madvise(evicted_entry->address, PAGE_SIZE, MADV_DONTNEED), "evict_page: madvise MADV_DONTNEED error");
    check_syscall(mprotect(evicted_entry->address, PAGE_SIZE, PROT_NONE), "evict_page: mprotect PROT_NONE error");

    evicted_entry->is_resident = false;
    evicted_entry->is_dirty = false;
    total_resident_mem_size -= PAGE_SIZE;
}

static void swap_to_file(page_table_entry *entry)
{
    if (entry->secondary_storage_fd == NOT_ASSIGNED)
    {
        if (swap_file_fd == NOT_ASSIGNED)
        {
            char swap_file_name[30];
            check_syscall(sprintf(swap_file_name, "%d.swap", getpid()), "swap_to_file: sprintf error");

            swap_file_fd = check_syscall(open(swap_file_name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO), "swap_to_file: open error");
        }

        entry->secondary_storage_fd = swap_file_fd;
    }

    if (entry->secondary_storage_location == NOT_ASSIGNED)
    {
        off_t *swap_file_location_ptr = (off_t *)dequeue(&available_swap_file_location_queue, NULL);

        if (swap_file_location_ptr)
        {
            entry->secondary_storage_location = *swap_file_location_ptr;
            free(swap_file_location_ptr);
        }
        else
        {
            entry->secondary_storage_location = swap_file_size;
            swap_file_size += PAGE_SIZE;
        }
    }

    check_syscall(pwrite(entry->secondary_storage_fd, entry->address, PAGE_SIZE, entry->secondary_storage_location), "swap_to_file: pwrite error");
}

static void swap_from_file(page_table_entry *entry)
{
    check_syscall(mprotect(entry->address, PAGE_SIZE, PROT_WRITE), "swap_from_file: mprotect PROT_WRITE error");
    check_syscall(pread(entry->secondary_storage_fd, entry->address, PAGE_SIZE, entry->secondary_storage_location), "swap_from_file: pread error");
}

static void apply_page_action(void *address, size_t size, void *(*action)(void *address, page_action_context *action_context), void *context)
{
    size_t num_pages = size / PAGE_SIZE;
    page_action_context action_context = {0, context};

    while (num_pages--)
    {
        action(address, &action_context);
        address += PAGE_SIZE;
        action_context.offset += PAGE_SIZE;
    }
}

static void store_mem_region(void *address, size_t size)
{
    mem_region *new_mem_region = (mem_region *)malloc(sizeof(mem_region));
    new_mem_region->address = address;
    new_mem_region->size = size;

    enqueue(&mem_region_queue, new_mem_region);
}

static page_table_entry *insert_default_page_table_entry(void *address, page_action_context *unused)
{
    int page_table_level_indices[4];
    compute_page_table_level_indices(address, page_table_level_indices);

    page_table *table = get_page_table(page_table_level_indices);

    page_table_entry *new_entry = (page_table_entry *)malloc(sizeof(page_table_entry));
    new_entry->address = address;
    new_entry->is_resident = false;
    new_entry->is_dirty = false;
    new_entry->secondary_storage_fd = NOT_ASSIGNED;
    new_entry->secondary_storage_location = NOT_ASSIGNED;

    table->entries[page_table_level_indices[LEVEL_ONE]] = new_entry;
    table->num_filled_entries++;

    return new_entry;
}

static page_table_entry *insert_mapped_file_page_table_entry(void *address, page_action_context *action_context)
{
    page_table_entry *new_entry = insert_default_page_table_entry(address, NULL);

    new_entry->secondary_storage_location = action_context->offset;
    new_entry->secondary_storage_fd = *(int *)action_context->context;

    return new_entry;
}

static void *clean_up_page(void *address, page_action_context *unused)
{
    int page_table_level_indices[4];
    compute_page_table_level_indices(address, page_table_level_indices);

    page_table *table = get_page_table(page_table_level_indices);

    page_table_entry *entry = table->entries[page_table_level_indices[LEVEL_ONE]];

    bool is_using_backing_file = entry->secondary_storage_fd != NOT_ASSIGNED && entry->secondary_storage_fd != swap_file_fd;

    if (!is_using_backing_file)
    {
        free_swap_file_location(entry);
    }

    entry->is_dirty = entry->is_dirty && is_using_backing_file;

    evict_page(entry->address);

    clean_up_page_tables(LEVEL_FOUR, &page_table_directory, page_table_level_indices);

    return NULL;
}

static void free_swap_file_location(page_table_entry *entry)
{
    if (entry->secondary_storage_location == NOT_ASSIGNED)
    {
        return;
    }

    off_t *swap_file_location_ptr = (off_t *)malloc(sizeof(off_t));
    *swap_file_location_ptr = entry->secondary_storage_location;

    enqueue(&available_swap_file_location_queue, swap_file_location_ptr);

    entry->secondary_storage_location = NOT_ASSIGNED;
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
        table->num_filled_entries--;
    }
}
