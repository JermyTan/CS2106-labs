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
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "myshell.h"

#define INFO 0
#define WAIT 1
#define TERMINATE 2
#define RUNNING 0
#define EXITED 1
#define TERMINATING 2
#define BACKGROUND_TASK_FLAG "&"

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

// returns 1 if should run program in background else 0
int check_should_run_in_background(char **args, size_t num_args)
{
    if (num_args <= 2 || strcmp(args[num_args - 2], BACKGROUND_TASK_FLAG) != 0)
    {
        return 0;
    }

    // remove "&" from args
    args[num_args - 2] = NULL;
    return 1;
}

process *get_child_process(pid_t pid)
{
    for (int i = 0; i < num_child_processes; i++)
    {
        if (child_processes[i]->pid == pid)
        {
            return child_processes[i];
        }
    }

    return NULL;
}

int is_terminated_status(int status)
{
    return WIFEXITED(status) || WIFSIGNALED(status);
}

void exec_info()
{
    process *child_process;
    for (int i = 0; i < num_child_processes; i++)
    {
        child_process = child_processes[i];

        // refresh status for terminating / background running tasks
        if (
            child_process->state_id != EXITED &&
            waitpid(child_process->pid, &(child_process->status), WNOHANG) == child_process->pid &&
            is_terminated_status(child_process->status))
        {
            child_process->state_id = EXITED;
        }

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

void exec_wait(pid_t pid)
{
    process *child_process = get_child_process(pid);

    // if child process with given pid doesn't exists or child process has already exited
    if (
        !child_process ||
        child_process->state_id == EXITED ||
        waitpid(pid, &(child_process->status), 0) != pid ||
        !is_terminated_status(child_process->status))
    {
        return;
    }

    child_process->state_id = EXITED;
}

void exec_terminate(pid_t pid)
{
    process *child_process = get_child_process(pid);

    // if child process with given pid doesn't exists or child process has already exited
    if (!child_process || child_process->state_id == EXITED || kill(pid, SIGTERM) != 0)
    {
        return;
    }

    child_process->state_id = TERMINATING;
}

void exec_program(char *program, char **args, int should_run_in_background)
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

    if (should_run_in_background)
    {
        printf("Child[%d] in background\n", new_process->pid);
    }
    else if (wait(&(new_process->status)) == new_process->pid)
    {
        new_process->state_id = EXITED;
    }
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
        exec_wait(atoi(tokens[1]));
        break;
    case TERMINATE:
        exec_terminate(atoi(tokens[1]));
        break;
    default:
        exec_program(command, tokens, check_should_run_in_background(tokens, num_tokens));
    }
}

void my_quit(void)
{
    // Clean up function, called after "quit" is entered as a user command
    while (num_child_processes)
    {
        free(child_processes[--num_child_processes]);
    }
    free(child_processes);
    printf("Goodbye!\n");
}
