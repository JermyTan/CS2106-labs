#include "restaurant.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_TABLE_SIZES 5
#define NOT_ASSIGNED -1

// You can declare global variables here
typedef struct
{
    int id;
    int size;
    int num_free_seats;
    int assigned_queue_nums[NUM_TABLE_SIZES];
} table;

typedef struct GROUP
{
    int num_people;
    int queue_num;
    struct GROUP *next;
} group;

typedef struct
{
    group *head;
    group *tail;
} list;

void enqueue(list *queue, group *new_group)
{
    if (!queue->head)
    {
        queue->head = new_group;
        queue->tail = new_group;
        return;
    }

    queue->tail->next = new_group;
    queue->tail = new_group;
}

group *dequeue(list *queue, int queue_num)
{
    if (!queue->head)
    {
        return NULL;
    }

    group *removed_group = NULL;

    if (queue->head->queue_num == queue_num)
    {
        removed_group = queue->head;

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
        group *previous_group = queue->head;
        group *current_group = previous_group->next;

        while (current_group)
        {
            if (current_group->queue_num == queue_num)
            {
                removed_group = current_group;
                previous_group->next = current_group->next;
                break;
            }

            previous_group = current_group;
            current_group = current_group->next;
        }
    }

    removed_group->next = NULL;
    return removed_group;
}

group *get_group(list *queue, int queue_num)
{
    group *current_group = queue->head;

    while (current_group)
    {
        if (current_group->queue_num == queue_num)
        {
            break;
        }
        current_group = current_group->next;
    }

    return current_group;
}

group *get_first_group(list *queue, int table_size)
{
    group *current_group = queue->head;

    while (current_group)
    {
        if (current_group->num_people <= table_size)
        {
            break;
        }
        current_group = current_group->next;
    }

    return current_group;
}

static int total_num_tables;
static table **tables;
static list *queue;
static pthread_mutex_t process_table_lock;
static pthread_cond_t process_table_cond;
static int next_queue_num;

static int check_syscall(int value, const char *error_msg)
{
    if (value != 0)
    {
        perror(error_msg);
    }
    return value;
}

static void update_table_assigned_queue_num(table *existing_table, int old_queue_num, int new_queue_num)
{
    for (int i = 0; i < NUM_TABLE_SIZES; i++)
    {
        if (existing_table->assigned_queue_nums[i] == old_queue_num)
        {
            existing_table->assigned_queue_nums[i] = new_queue_num;
            break;
        }
    }
}

static int assign_table(group *waiting_group, int queue_num)
{
    if (queue_num == NOT_ASSIGNED)
    {
        int minimum_free_seats = NUM_TABLE_SIZES + 1;
        table *assigned_table = NULL;

        // try to assign totally empty and unassigned tables first
        // then try assign partially empty tables if possible.
        for (int i = 0; i < total_num_tables; i++)
        {
            if (tables[i]->size != tables[i]->num_free_seats || tables[i]->num_free_seats < waiting_group->num_people)
            {
                if (waiting_group->num_people <= tables[i]->num_free_seats && tables[i]->num_free_seats < minimum_free_seats)
                {
                    minimum_free_seats = tables[i]->num_free_seats;
                    assigned_table = tables[i];
                }

                continue;
            }

            assigned_table = tables[i];
            break;
        }

        // try to assign tables
        if (assigned_table)
        {
            update_table_assigned_queue_num(assigned_table, NOT_ASSIGNED, waiting_group->queue_num);
            assigned_table->num_free_seats -= waiting_group->num_people;
            return assigned_table->id;
        }
    }
    else
    {
        for (int i = 0; i < total_num_tables; i++)
        {
            if (tables[i]->size < waiting_group->num_people)
            {
                continue;
            }

            for (int j = 0; j < NUM_TABLE_SIZES; j++)
            {
                if (tables[i]->assigned_queue_nums[j] == queue_num)
                {
                    return tables[i]->id;
                }
            }
        }
    }

    return NOT_ASSIGNED;
}

void restaurant_init(int num_tables[5])
{
    // initialize queue
    next_queue_num = 0;
    queue = (list *)malloc(sizeof(list));
    queue->head = NULL;
    queue->tail = NULL;

    // initialize table and queue counts
    total_num_tables = 0;
    for (int i = 0; i < NUM_TABLE_SIZES; i++)
    {
        total_num_tables += num_tables[i];
    }

    tables = (table **)malloc(sizeof(table *) * total_num_tables);

    int table_id = 0;
    for (int i = 0; i < NUM_TABLE_SIZES; i++)
    {
        for (int j = 0; j < num_tables[i]; j++)
        {
            table *new_table = (table *)malloc(sizeof(table));

            new_table->id = table_id;
            new_table->size = i + 1;
            new_table->num_free_seats = new_table->size;

            for (int k = 0; k < NUM_TABLE_SIZES; k++)
            {
                new_table->assigned_queue_nums[k] = NOT_ASSIGNED;
            }

            tables[table_id++] = new_table;
        }
    }

    // initialize mutex lock and conditional variable
    check_syscall(pthread_mutex_init(&process_table_lock, NULL), "restaurant_init: pthread_mutex_init process_table_lock error");
    check_syscall(pthread_cond_init(&process_table_cond, NULL), "restaurant_init: pthread_cond_init process_table_cond error");
}

void restaurant_destroy(void)
{
    free(queue);

    while (total_num_tables)
    {
        free(tables[--total_num_tables]);
    }

    free(tables);

    check_syscall(pthread_mutex_destroy(&process_table_lock), "restaurant_destroy: pthread_mutex_destroy process_table_lock error");
    check_syscall(pthread_cond_destroy(&process_table_cond), "restaurant_destroy: pthread_cond_destroy process_table_cond error");
}

int request_for_table(group_state *state, int num_people)
{
    check_syscall(pthread_mutex_lock(&process_table_lock), "request_for_table: pthread_mutex_lock process_table_lock error");

    group *new_group = (group *)malloc(sizeof(group));
    new_group->num_people = num_people;
    new_group->queue_num = next_queue_num++;
    new_group->next = NULL;

    on_enqueue();

    state->queue_num = new_group->queue_num;
    state->num_people = new_group->num_people;
    state->table_id = assign_table(new_group, NOT_ASSIGNED);

    if (state->table_id == NOT_ASSIGNED)
    {
        enqueue(queue, new_group);

        while (get_group(queue, new_group->queue_num))
        {
            check_syscall(pthread_cond_wait(&process_table_cond, &process_table_lock), "request_for_table: pthread_cond_wait (process_table_cond, process_table_lock) error");
        }

        state->table_id = assign_table(new_group, new_group->queue_num);
    }

    check_syscall(pthread_mutex_unlock(&process_table_lock), "request_for_table: pthread_mutex_unlock process_table_lock error");

    free(new_group);

    return state->table_id;
}

void leave_table(group_state *state)
{
    check_syscall(pthread_mutex_lock(&process_table_lock), "leave_table: pthread_mutex_lock process_table_lock error");

    table *assigned_table = tables[state->table_id];
    assigned_table->num_free_seats += state->num_people;
    update_table_assigned_queue_num(assigned_table, state->queue_num, NOT_ASSIGNED);

    group *waiting_group;

    // for ex6, we can match fit as many groups within a table where total group sizes <= table size
    while (assigned_table->num_free_seats > 0 && (waiting_group = get_first_group(queue, assigned_table->num_free_seats)))
    {
        update_table_assigned_queue_num(assigned_table, NOT_ASSIGNED, waiting_group->queue_num);
        assigned_table->num_free_seats -= waiting_group->num_people;
        dequeue(queue, waiting_group->queue_num);
        // signals request_for_table to process waiting groups
        check_syscall(pthread_cond_broadcast(&process_table_cond), "leave_table: pthread_cond_broadcast process_table_cond error");
    }

    check_syscall(pthread_mutex_unlock(&process_table_lock), "leave_table: pthread_mutex_unlock process_table_lock error");
}
