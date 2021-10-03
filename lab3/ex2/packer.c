#include "packer.h"
#include <stdio.h>
#include <semaphore.h>

#define NUM_COLORS 3
#define GROUP_SIZE 2

// You can declare global variables here
static sem_t color_locks[NUM_COLORS];
static sem_t group_color_ball_locks[NUM_COLORS];
static int ball_counts[NUM_COLORS];
static int ball_ids[NUM_COLORS][GROUP_SIZE];

static int check_syscall(int value, const char *error_msg)
{
    if (value != 0)
    {
        perror(error_msg);
    }
    return value;
}

void packer_init(void)
{
    // Write initialization code here (called once at the start of the program).
    for (int i = 0; i < NUM_COLORS; i++)
    {
        ball_counts[i] = 0;
        check_syscall(sem_init(&color_locks[i], 0, 1), "packer_init: sem_init color_locks error");
        check_syscall(sem_init(&group_color_ball_locks[i], 0, 0), "packer_init: sem_init group_color_ball_locks error");
    }
}

void packer_destroy(void)
{
    // Write deinitialization code here (called once at the end of the program).
    for (int i = 0; i < NUM_COLORS; i++)
    {
        check_syscall(sem_destroy(&color_locks[i]), "packer_destroy: sem_destroy color_locks error");
        check_syscall(sem_destroy(&group_color_ball_locks[i]), "packer_destroy: sem_destroy group_color_ball_locks error");
    }
}

int pack_ball(int colour, int id)
{
    int other_id;
    colour--;

    check_syscall(sem_wait(&color_locks[colour]), "pack_ball: sem_wait color_locks error");

    ball_ids[colour][ball_counts[colour]] = id;
    ball_counts[colour]++;

    if (ball_counts[colour] == GROUP_SIZE)
    {
        check_syscall(sem_post(&group_color_ball_locks[colour]), "pack_ball: sem_post group_color_ball_locks error");
    }
    else
    {
        check_syscall(sem_post(&color_locks[colour]), "pack_ball: sem_post color_locks error");
    }

    check_syscall(sem_wait(&group_color_ball_locks[colour]), "pack_ball: sem_wait group_color_ball_locks error");

    for (int i = 0; i < GROUP_SIZE; i++)
    {
        if (ball_ids[colour][i] != id)
        {
            other_id = ball_ids[colour][i];
            break;
        }
    }
    ball_counts[colour]--;

    if (ball_counts[colour] == 0)
    {
        check_syscall(sem_post(&color_locks[colour]), "pack_ball: sem_post color_locks error");
    }
    else
    {
        check_syscall(sem_post(&group_color_ball_locks[colour]), "pack_ball: sem_post group_color_ball_locks error");
    }

    return other_id;
}