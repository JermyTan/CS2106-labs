/*************************************
* Lab 1 Exercise 3
* Name:
* Student No:
* Lab Group:
*************************************/

#include "node.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Copy in your implementation of the functions from ex2.
// There is one extra function called map which you have to fill up too.
// Feel free to add any new functions as you deem fit.

// Inserts a new node with data value at index (counting from head
// starting at 0).
// Note: index is guaranteed to be valid.
void insert_node_at(list *lst, int index, int data) {
}

// Deletes node at index (counting from head starting from 0).
// Note: index is guarenteed to be valid.
void delete_node_at(list *lst, int index) {
}

// Rotates list by the given offset.
// Note: offset is guarenteed to be non-negative.
void rotate_list(list *lst, int offset) {
}

// Reverses the list, with the original "tail" node
// becoming the new head node.
void reverse_list(list *lst) {
}

// Resets list to an empty state (no nodes) and frees
// any allocated memory in the process
void reset_list(list *lst) {
}

// Traverses list and applies func on data values of
// all elements in the list.
void map(list *lst, int (*func)(int)) {
}

// Traverses list and returns the sum of the data values
// of every node in the list.
long sum_list(list *lst) {
}
