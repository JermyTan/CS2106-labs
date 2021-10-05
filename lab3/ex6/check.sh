#!/bin/bash

make clean
make

if command -v valgrind
then
    valgrind --fair-sched=yes ./ex6 < share_test.in
    valgrind --fair-sched=yes ./ex6 < ../ex5/seq_test.in
    valgrind --fair-sched=yes ./ex6 < ../ex5/load_test.in
    valgrind --fair-sched=yes ./ex6 < ../ex4/par_test.in
fi

make clean