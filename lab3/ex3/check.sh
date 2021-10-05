#!/bin/bash

make clean
make

if command -v valgrind
then
    valgrind --fair-sched=yes ./ex3 < test.in
    valgrind --fair-sched=yes ./ex3 < ../ex2/order_test.in
    valgrind --fair-sched=yes ./ex3 < ../ex2/seq_test.in
    valgrind --fair-sched=yes ./ex3 < ../ex2/par_test.in
    valgrind --fair-sched=yes ./ex3 < ../ex1/seq_test.in
    valgrind --fair-sched=yes ./ex3 < ../ex1/par_test.in
    valgrind --fair-sched=yes ./ex3 < ../ex1/tiny_test.in
fi

make clean