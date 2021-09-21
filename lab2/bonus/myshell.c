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
#define FG 3
#define RUNNING 0
#define EXITED 1
#define TERMINATING 2
#define STOPPED 3
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

static const char *PROCESS_STATE[] = {"Running", "Exited", "Terminating", "Stopped"};
static const char *SHELL_COMMANDS[] = {"info", "wait", "terminate", "fg", NULL};

static int num_child_processes;
// history of all the child processes the shell has executed
static process *child_processes[MAX_PROCESSES];
// -1 if shell is not waiting else contains pid of process shell is waiting
static pid_t waiting_pid;
// 0 if no signal was caught else 1
static int has_caught_signal;

/* helper function prototypes */
// returns id corresponding to given shell commands, -1 is returned for user program command
static int get_shell_command_id(char *command);
// returns 1 if should run program in background else 0
static int check_should_run_in_background(char **args, size_t *num_args);
static void check_redirection_files(char **args, size_t *num_args, char **input_file, char **output_file, char **error_file);
static process *get_child_process(pid_t pid);
static void refresh_process_state(process *child_process, int options);
static void signal_handler(int);
static void exec_info();
static void exec_wait(pid_t pid);
static void exec_terminate(pid_t pid);
static void exec_fg(pid_t pid);
// returns the exit status of the executed program
// 0 is returned if the executed program runs in background
static int exec_program(char *program, char **args, int should_run_in_background, char *input_file, char *output_file, char *error_file);
// returns 0 if the command is executed without errors else -1
static int exec_command(char **args, size_t num_args, int is_chaining_commands);

static int check_syscall(int value, const char *error_msg)
{
    if (value == -1)
    {
        perror(error_msg);
    }
    return value;
}

void my_init(void)
{
    // Initialize what you need here
    num_child_processes = 0;
    waiting_pid = -1;
    has_caught_signal = 0;

    // setup signal intercepters
    signal(SIGTSTP, signal_handler);
    signal(SIGINT, signal_handler);
}

void my_process_command(size_t num_tokens, char **tokens)
{
    size_t start = 0;
    int is_chaining_commands = 0; // 1 if && exists else 0

    // if ^Z or ^C is sent to a program, the other programs in the chain will continue to execute
    for (size_t end = 0; end < num_tokens; end++)
    {
        if (tokens[end] && strcmp(tokens[end], AND_OPERATOR) == 0)
        {
            tokens[end] = NULL;
            is_chaining_commands = 1;
        }

        if (tokens[end] == NULL)
        {
            if (exec_command(tokens + start, end - start, is_chaining_commands) != 0)
            {
                break;
            }

            start = end + 1;
            is_chaining_commands = 0;
        }
    }
}

void my_quit(void)
{
    // Clean up function, called after "quit" is entered as a user command

    // set signal handlers back to default
    signal(SIGTSTP, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    process *child_process;
    while (num_child_processes)
    {
        child_process = child_processes[--num_child_processes];

        if (child_process->state_id != EXITED)
        {
            check_syscall(kill(child_process->pid, SIGTERM), "my_quit: kill SIGTERM error");

            if (child_process->state_id == STOPPED)
            {
                check_syscall(killpg(child_process->pid, SIGCONT), "my_quit: killpg SIGCONT error");
            }

            refresh_process_state(child_process, 0);
        }

        free(child_process);
    }
    printf("Goodbye!\n");
}

static int get_shell_command_id(char *command)
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

static int check_should_run_in_background(char **args, size_t *num_args)
{
    if (*num_args <= 1 || strcmp(args[*num_args - 1], BACKGROUND_TASK_FLAG) != 0)
    {
        return 0;
    }

    // remove "&" from args
    args[--(*num_args)] = NULL;
    return 1;
}

static void check_redirection_files(char **args, size_t *num_args, char **input_file, char **output_file, char **error_file)
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

static process *get_child_process(pid_t pid)
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

static void refresh_process_state(process *child_process, int options)
{
    if (!child_process || child_process->state_id == EXITED)
    {
        return;
    }

    waiting_pid = child_process->pid;

    if (check_syscall(waitpid(waiting_pid, &(child_process->status), options), "refresh_process_state: waitpid error") == waiting_pid)
    {
        if (WIFSTOPPED(child_process->status))
        {
            child_process->state_id = STOPPED;
        }

        if (child_process->state_id != TERMINATING && WIFCONTINUED(child_process->status))
        {
            child_process->state_id = RUNNING;
        }

        if (WIFEXITED(child_process->status) || WIFSIGNALED(child_process->status))
        {
            child_process->state_id = EXITED;
        }

        if (has_caught_signal && WIFSIGNALED(child_process->status))
        {
            printf("[%d] interrupted\n", waiting_pid);
        }

        if (has_caught_signal && WIFSTOPPED(child_process->status))
        {
            printf("[%d] stopped\n", waiting_pid);
        }
    }

    // reset state
    waiting_pid = -1;
    has_caught_signal = 0;
}

static void signal_handler(int signum)
{
    // do nth if shell is not waiting
    if (waiting_pid < 0)
    {
        return;
    }

    process *child_process = get_child_process(waiting_pid);

    if (!child_process || child_process->state_id == EXITED || check_syscall(killpg(waiting_pid, signum), "signal_handler: killpg error") != 0)
    {
        return;
    }

    has_caught_signal = 1;
}

static void exec_info()
{
    process *child_process;
    for (int i = 0; i < num_child_processes; i++)
    {
        child_process = child_processes[i];

        refresh_process_state(child_process, WNOHANG | WCONTINUED | WUNTRACED);

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

static void exec_wait(pid_t pid)
{
    process *child_process = get_child_process(pid);

    // does not wait on stopped process
    if (!child_process || child_process->state_id == STOPPED)
    {
        return;
    }

    refresh_process_state(child_process, WUNTRACED);
}

static void exec_terminate(pid_t pid)
{
    process *child_process = get_child_process(pid);

    if (!child_process ||
        child_process->state_id == EXITED ||
        check_syscall(kill(pid, SIGTERM), "exec_terminate: kill SIGTERM error") != 0 ||
        (child_process->state_id == STOPPED && check_syscall(killpg(pid, SIGCONT), "exec_terminate: killpg SIGCONT error") != 0))
    {
        return;
    }

    child_process->state_id = TERMINATING;
}

static void exec_fg(pid_t pid)
{
    process *child_process = get_child_process(pid);

    if (!child_process ||
        child_process->state_id != STOPPED ||
        check_syscall(killpg(pid, SIGCONT), "exec_fg: killpg SIGCONT error") != 0)
    {
        return;
    }

    refresh_process_state(child_process, WCONTINUED);
    refresh_process_state(child_process, WUNTRACED);
}

static int exec_program(char *program, char **args, int should_run_in_background, char *input_file, char *output_file, char *error_file)
{
    pid_t pid = check_syscall(fork(), "exec_program: fork error");

    if (pid == 0)
    {
        if (input_file)
        {
            int in_fd = check_syscall(open(input_file, O_RDONLY), "exec_program: open input_file error");
            check_syscall(dup2(in_fd, STDIN_FILENO), "exec_program: dup2 in_fd error");
            check_syscall(close(in_fd), "exec_program: close error");
        }
        if (output_file)
        {
            int out_fd = check_syscall(open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO), "exec_program: open output_file error");
            check_syscall(dup2(out_fd, STDOUT_FILENO), "exec_program: dup2 out_fd error");
            check_syscall(close(out_fd), "exec_program: close error");
        }
        if (error_file)
        {
            int err_fd = check_syscall(open(error_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO), "exec_program: open error_file error");
            check_syscall(dup2(err_fd, STDERR_FILENO), "exec_program: dup2 err_fd error");
            check_syscall(close(err_fd), "exec_program: close error");
        }

        setpgid(0, 0);

        check_syscall(execv(program, args), "exec_program: execv error");
    }

    process *new_process = (process *)malloc(sizeof(process));
    new_process->pid = pid;
    new_process->state_id = RUNNING;

    child_processes[num_child_processes++] = new_process;

    if (should_run_in_background)
    {
        printf("Child[%d] in background\n", new_process->pid);
    }
    else
    {
        refresh_process_state(new_process, WUNTRACED);
    }

    return new_process->state_id == EXITED ? WEXITSTATUS(new_process->status) : 0;
}

static int exec_command(char **args, size_t num_args, int is_chaining_commands)
{
    if (num_args <= 0)
    {
        return -1;
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
            printf("wait: Missing argument(s)\n");
            return -1;
        }
        exec_wait(atoi(args[1]));
        break;
    case TERMINATE:
        if (num_args < 2)
        {
            printf("terminate: Missing argument(s)\n");
            return -1;
        }
        exec_terminate(atoi(args[1]));
        break;
    case FG:
        if (num_args < 2)
        {
            printf("fg: Missing argument(s)\n");
            return -1;
        }
        exec_fg(atoi(args[1]));
        break;
    default:
        if (access(command, F_OK) != 0)
        {
            printf("%s not found\n", command);
            return -1;
        }

        int should_run_in_background = check_should_run_in_background(args, &(num_args));

        char *input_file = NULL, *output_file = NULL, *error_file = NULL;
        check_redirection_files(args, &(num_args), &(input_file), &(output_file), &(error_file));

        if (input_file && access(input_file, F_OK) != 0)
        {
            printf("%s does not exist\n", input_file);
            return -1;
        }

        if (exec_program(
                command,
                args,
                should_run_in_background,
                input_file,
                output_file,
                error_file) != 0)
        {
            // does not print if only running single command/program
            if (is_chaining_commands)
            {
                printf("%s failed\n", command);
            }

            return -1;
        }
    }

    return 0;
}
