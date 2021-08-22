/*************************************
* Lab 1 Exercise 3
* Name: Tan Kai Qun, Jeremy
* Student No: A0136134N
* Lab Group: 18
*************************************/

#include <stdio.h>
#include <stdlib.h>

#include "function_pointers.h"
#include "node.h"

// The runner is empty now! Modify it to fulfill the requirements of the
// exercise. You can use ex2.c as a template

// DO NOT initialize the func_list array in this file. All initialization
// logic for func_list should go into function_pointers.c.

// Macros
#define SUM_LIST 0
#define INSERT_AT 1
#define DELETE_AT 2
#define ROTATE_LIST 3
#define REVERSE_LIST 4
#define RESET_LIST 5
#define MAP 6

void run_instruction(list *lst, int instr, FILE *fp);

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Error: expecting 1 argument, %d found\n", argc - 1);
        exit(1);
    }

    // We read in the file name provided as argument
    char *fname = argv[1];

    // Update the array of function pointers
    // DO NOT REMOVE THIS CALL
    // (You may leave the function empty if you do not need it)
    update_functions();

    // Rest of code logic here
    FILE *fp = fopen(fname, "r");
    list *lst = (list *)malloc(sizeof(list));
    lst->head = NULL;

    int instr;
    while (fscanf(fp, "%d", &instr) == 1)
    {
        run_instruction(lst, instr, fp);
    }

    fclose(fp);
    reset_list(lst);
    free(lst);
}

void run_instruction(list *lst, int instr, FILE *fp)
{
    int index, data, offset;
    switch (instr)
    {
    case SUM_LIST:
        printf("%ld\n", sum_list(lst));
        break;
    case INSERT_AT:
        fscanf(fp, "%d %d", &index, &data);
        insert_node_at(lst, index, data);
        break;
    case DELETE_AT:
        fscanf(fp, "%d", &index);
        delete_node_at(lst, index);
        break;
    case ROTATE_LIST:
        fscanf(fp, "%d", &offset);
        rotate_list(lst, offset);
        break;
    case REVERSE_LIST:
        reverse_list(lst);
        break;
    case RESET_LIST:
        reset_list(lst);
        break;
    case MAP:
        fscanf(fp, "%d", &index);
        map(lst, func_list[index]);
    }
}