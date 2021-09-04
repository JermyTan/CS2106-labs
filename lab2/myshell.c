/**
 * CS2106 AY21/22 Semester 1 - Lab 2
 *
 * This file contains function definitions. Your implementation should go in
 * this file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "myshell.h"

typedef struct
{
    pid_t pid;
    bool has_exited;
    int exit_status;
} process;

const int MAX_PROGRAMS = 100;
int num_child_processes;
process *child_processes;

void my_init(void)
{
    // Initialize what you need here
    num_child_processes = 0;
    child_processes = (process *)malloc(MAX_PROGRAMS * sizeof(process));
}

void my_process_command(size_t num_tokens, char **tokens)
{
    if (num_tokens <= 1)
    {
        return;
    }

    char *command = *tokens;

    printf("%s\n", command);

    // if (fork() == 0)
    // {
    //     if (access(*tokens))
    //         printf("%d\n", execv(*tokens, tokens));
    //     exit(1);
    // }

    // wait(NULL);
}

void my_quit(void)
{
    // Clean up function, called after "quit" is entered as a user command
    num_child_processes = 0;
    free(child_processes);
    printf("Goodbye!\n");
}
