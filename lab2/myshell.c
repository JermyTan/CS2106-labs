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
#include <fcntl.h>
#include <sys/stat.h>
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
#define AND_OPERATOR "&&"
#define INPUT_REDIR_OPERATOR "<"
#define OUTPUT_REDIR_OPERATOR ">"
#define ERROR_REDIR_OPERATOR "2>"
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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
    for (int i = 0; SHELL_COMMANDS[i]; i++)
    {
        if (strcmp(command, SHELL_COMMANDS[i]) == 0)
        {
            return i;
        }
    }

    // represents program command
    return -1;
}

// returns 1 if should run program in background else 0
int check_should_run_in_background(char **args, size_t *num_args)
{
    if (*num_args <= 1 || strcmp(args[*num_args - 1], BACKGROUND_TASK_FLAG) != 0)
    {
        return 0;
    }

    // remove "&" from args
    args[--(*num_args)] = NULL;
    return 1;
}

void check_redirection_files(char **args, size_t *num_args, char **input_file, char **output_file, char **error_file)
{
    size_t original_num_args = *num_args, i = 0;

    while (i < original_num_args)
    {
        if (strcmp(args[i], INPUT_REDIR_OPERATOR) == 0 && *input_file == NULL)
        {
            args[i] = NULL;
            *num_args = MIN(i, *num_args);
            *input_file = args[++i];
        }
        else if (strcmp(args[i], OUTPUT_REDIR_OPERATOR) == 0 && *output_file == NULL)
        {
            args[i] = NULL;
            *num_args = MIN(i, *num_args);
            *output_file = args[++i];
        }
        else if (strcmp(args[i], ERROR_REDIR_OPERATOR) == 0 && *error_file == NULL)
        {
            args[i] = NULL;
            *num_args = MIN(i, *num_args);
            *error_file = args[++i];
        }

        i++;
    }
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
    if (!child_process ||
        child_process->state_id == EXITED ||
        kill(pid, SIGTERM) != 0)
    {
        return;
    }

    child_process->state_id = TERMINATING;
}

// returns the exit status of the executed program
// 0 is returned if the executed program runs in background
int exec_program(char *program, char **args, int should_run_in_background, char *input_file, char *output_file, char *error_file)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        int in_fd = input_file ? open(input_file, O_RDONLY) : STDIN_FILENO;
        int out_fd = output_file ? open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777) : STDOUT_FILENO;
        int err_fd = error_file ? open(error_file, O_WRONLY | O_CREAT | O_TRUNC, 0777) : STDERR_FILENO;

        if (in_fd != STDIN_FILENO)
        {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != STDOUT_FILENO)
        {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        if (err_fd != STDERR_FILENO)
        {
            dup2(err_fd, STDERR_FILENO);
            close(err_fd);
        }

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
    else if (wait(&(new_process->status)) == new_process->pid && is_terminated_status(new_process->status))
    {
        new_process->state_id = EXITED;
    }

    return new_process->state_id == EXITED ? WEXITSTATUS(new_process->status) : 0;
}

// returns 0 if the command is executed without errors else 1
int exec_command(char **args, size_t num_args)
{
    if (num_args <= 0)
    {
        return 1;
    }

    char *command = *args;

    switch (get_shell_command_id(command))
    {
    case INFO:
        exec_info();
        break;
    case WAIT:
        if (num_args < 2)
        {
            printf("wait: Missing arguments\n");
            return 1;
        }
        exec_wait(atoi(args[1]));
        break;
    case TERMINATE:
        if (num_args < 2)
        {
            printf("terminate: Missing arguments\n");
            return 1;
        }
        exec_terminate(atoi(args[1]));
        break;
    default:
        if (access(command, F_OK) != 0)
        {
            printf("%s not found\n", command);
            return 1;
        }

        int should_run_in_background = check_should_run_in_background(args, &(num_args));

        char *input_file = NULL, *output_file = NULL, *error_file = NULL;
        check_redirection_files(args, &(num_args), &(input_file), &(output_file), &(error_file));

        if (input_file && access(input_file, F_OK) != 0)
        {
            printf("%s does not exist\n", input_file);
            return 1;
        }

        if (exec_program(
                command,
                args,
                should_run_in_background,
                input_file,
                output_file,
                error_file) != 0)
        {
            printf("%s failed\n", command);
            return 1;
        }
    }

    return 0;
}

void my_process_command(size_t num_tokens, char **tokens)
{
    size_t start = 0;
    for (size_t end = 0; end < num_tokens; end++)
    {
        if (tokens[end] && strcmp(tokens[end], AND_OPERATOR) == 0)
        {
            tokens[end] = NULL;
        }

        if (tokens[end] == NULL)
        {
            if (exec_command(tokens + start, end - start) != 0)
            {
                break;
            }

            start = end + 1;
        }
    }
}

void my_quit(void)
{
    // Clean up function, called after "quit" is entered as a user command

    process *child_process;
    while (num_child_processes)
    {
        child_process = child_processes[--num_child_processes];

        if (child_process->state_id != EXITED)
        {
            kill(child_process->pid, SIGTERM);
            waitpid(child_process->pid, &(child_process->status), 0);
        }

        free(child_process);
    }
    free(child_processes);
    printf("Goodbye!\n");
}
