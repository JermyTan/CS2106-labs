#!/bin/bash

make clean
make

if command -v valgrind
then
    valgrind --fair-sched=yes ./ex5 < seq_test.in
    valgrind --fair-sched=yes ./ex5 < load_test.in
    valgrind --fair-sched=yes ./ex5 < ../ex4/par_test.in
fi
