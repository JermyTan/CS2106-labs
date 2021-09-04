/**
 * CS2106 AY21/22 Semester 1 - Lab 2
 *
 * This file contains function definitions. Your implementation should go in
 * this file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "myshell.h"

#define INFO 0
#define WAIT 1
#define TERMINATE 2
#define RUNNING 0
#define EXITED 1
#define TERMINATING 2

typedef struct
{
    pid_t pid;
    int state_id;
    int status;
} process;

const int MAX_PROGRAMS = 100;
const char *PROCESS_STATE[3] = {"Running", "Exited", "Terminating"};
const char *SHELL_COMMANDS[4] = {"info", "wait", "terminate", NULL};

int num_child_processes;
process **child_processes;

void my_init(void)
{
    // Initialize what you need here
    num_child_processes = 0;
    child_processes = (process **)malloc(MAX_PROGRAMS * sizeof(process *));
}

int get_shell_command_id(char *command)
{
    int i = 0;
    while (SHELL_COMMANDS[i])
    {
        if (strcmp(command, SHELL_COMMANDS[i]) == 0)
        {
            return i;
        }
        i++;
    }

    // represents program command
    return -1;
}

void exec_info()
{
    process *child_process;
    for (int i = 0; i < num_child_processes; i++)
    {
        child_process = child_processes[i];

        if (child_process->state_id == EXITED)
        {
            printf("[%d] %s %d\n",
                   child_process->pid,
                   PROCESS_STATE[child_process->state_id],
                   WEXITSTATUS(child_process->status));
        }
        else
        {
            printf("[%d] %s\n",
                   child_process->pid,
                   PROCESS_STATE[child_process->state_id]);
        }
    }
}

void exec_program(char *program, char **args)
{
    if (access(program, F_OK) != 0)
    {
        printf("%s not found\n", program);
        return;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        execv(program, args);
        exit(1);
    }

    process *new_process = (process *)malloc(sizeof(process));
    new_process->pid = pid;
    new_process->state_id = RUNNING;

    child_processes[num_child_processes++] = new_process;

    wait(&(new_process->status));

    new_process->state_id = EXITED;
}

void my_process_command(size_t num_tokens, char **tokens)
{
    if (num_tokens <= 1)
    {
        return;
    }

    char *command = *tokens;

    switch (get_shell_command_id(command))
    {
    case INFO:
        exec_info();
        break;
    case WAIT:
        printf("wait\n");
        break;
    case TERMINATE:
        printf("terminate\n");
        break;
    default:
        exec_program(command, tokens);
    }
}

void my_quit(void)
{
    // Clean up function, called after "quit" is entered as a user command
    while (num_child_processes)
    {
        free(child_processes[num_child_processes--]);
    }
    free(child_processes);
    printf("Goodbye!\n");
}
