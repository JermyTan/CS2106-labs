/*************************************
* Lab 1 Exercise 3
* Name: Tan Kai Qun, Jeremy
* Student No: A0136134N
* Lab Group: 18
*************************************/

#include "functions.h"

#define ADD_ONE 0
#define ADD_TWO 1
#define MULTIPLY_FIVE 2
#define SQUARE 3
#define CUBE 4

// Initialize the func_list array here!
int (*func_list[5])(int);

// You can also use this function to help you with
// the initialization. This will be called in ex3.c.
void update_functions()
{
    func_list[ADD_ONE] = add_one;
    func_list[ADD_TWO] = add_two;
    func_list[MULTIPLY_FIVE] = multiply_five;
    func_list[SQUARE] = square;
    func_list[CUBE] = cube;
}
