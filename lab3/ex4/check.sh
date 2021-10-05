#!/bin/bash

make clean
make

if command -v valgrind
then
    valgrind --fair-sched=yes ./ex4 < seq_test.in
    valgrind --fair-sched=yes ./ex4 < par_test.in
fi

make clean