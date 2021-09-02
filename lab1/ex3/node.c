/*************************************
* Lab 1 Exercise 3
* Name: Tan Kai Qun, Jeremy
* Student No: A0136134N
* Lab Group: 18
*************************************/

#include "node.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Copy in your implementation of the functions from ex2.
// There is one extra function called map which you have to fill up too.
// Feel free to add any new functions as you deem fit.
int get_list_length(list *lst)
{
    if (!lst->head)
    {
        return 0;
    }

    int length = 0;
    node *current_node = lst->head;

    do
    {
        length++;
        current_node = current_node->next;
    } while (current_node != lst->head);

    return length;
}

node *get_node_at(list *lst, int index)
{
    int current_index = 0;
    node *current_node = lst->head;

    while (current_node && current_index < index)
    {
        current_index++;
        current_node = current_node->next;
    }

    return current_node;
}

// Inserts a new node with data value at index (counting from head
// starting at 0).
// Note: index is guaranteed to be valid.
void insert_node_at(list *lst, int index, int data)
{
    node *new_node = (node *)malloc(sizeof(node));
    new_node->data = data;

    // case 1: node is inserted at the head
    // need to update head and tail pointer
    if (index == 0)
    {
        int length = get_list_length(lst);
        node *tail_node = get_node_at(lst, length - 1);

        // old head becomes new head -> next if old head exists
        // else new head -> next = new head
        new_node->next = lst->head ? lst->head : new_node;
        lst->head = new_node;

        // update tail_node -> next to new head
        if (tail_node)
        {
            tail_node->next = lst->head;
        }

        return;
    }

    // case 2: node is inserted elsewhere
    node *previous_node = get_node_at(lst, index - 1);
    new_node->next = previous_node->next;
    previous_node->next = new_node;
}

// Deletes node at index (counting from head starting from 0).
// Note: index is guarenteed to be valid.
void delete_node_at(list *lst, int index)
{
    // case 1: node is removed from the head
    if (index == 0)
    {
        node *old_head = lst->head;
        int length = get_list_length(lst);

        if (length == 1)
        {
            lst->head = NULL;
        }
        else
        {
            node *tail_node = get_node_at(lst, length - 1);
            lst->head = old_head->next;
            tail_node->next = lst->head;
        }

        // clean up
        free(old_head);

        return;
    }

    // case 2: node is removed elsewhere
    node *previous_node = get_node_at(lst, index - 1);
    node *to_remove_node = previous_node->next;
    previous_node->next = to_remove_node->next;

    // clean up
    free(to_remove_node);
}

// Rotates list by the given offset.
// Note: offset is guarenteed to be non-negative.
void rotate_list(list *lst, int offset)
{
    int length = get_list_length(lst);

    if (length <= 1)
    {
        return;
    }

    offset = offset % length;
    node *current_node = lst->head;

    while (offset--)
    {
        current_node = current_node->next;
    }

    lst->head = current_node;
}

// Reverses the list, with the original "tail" node
// becoming the new head node.
void reverse_list(list *lst)
{
    int length = get_list_length(lst);

    if (length <= 1)
    {
        return;
    }

    node *current_node = lst->head;
    node *previous_node = NULL;
    node *temp_node;

    while (length--)
    {
        temp_node = current_node->next;
        current_node->next = previous_node;
        previous_node = current_node;
        current_node = temp_node;
    }

    // link last node (original head) -> next to new head
    lst->head->next = previous_node;
    lst->head = previous_node;
}

// Resets list to an empty state (no nodes) and frees
// any allocated memory in the process
void reset_list(list *lst)
{
    // approach 1 O(n^2): reuse delete_node_at
    // int length = get_list_length(lst);
    // while (length--)
    // {
    //     delete_node_at(lst, length);
    // }
    // return;

    // approach 2 O(n): single pass
    if (!lst->head)
    {
        return;
    }

    node *current_node = lst->head->next;
    while (current_node != lst->head)
    {
        node *next_node = current_node->next;
        free(current_node);
        current_node = next_node;
    }

    free(lst->head);
    lst->head = NULL;
}

// Traverses list and applies func on data values of
// all elements in the list.
void map(list *lst, int (*func)(int))
{
    if (!lst->head)
    {
        return;
    }

    node *current_node = lst->head;
    do
    {
        current_node->data = func(current_node->data);
        current_node = current_node->next;
    } while (current_node != lst->head);
}

// Traverses list and returns the sum of the data values
// of every node in the list.
long sum_list(list *lst)
{
    if (!lst->head)
    {
        return 0;
    }

    long sum = 0;
    node *current_node = lst->head;
    do
    {
        sum += current_node->data;
        current_node = current_node->next;
    } while (current_node != lst->head);

    return sum;
}
