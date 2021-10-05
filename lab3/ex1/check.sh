#!/bin/bash

make clean
make

if command -v valgrind
then
    valgrind --fair-sched=yes ./ex1 < seq_test.in
    valgrind --fair-sched=yes ./ex1 < par_test.in
    valgrind --fair-sched=yes ./ex1 < tiny_test.in
fi

make clean