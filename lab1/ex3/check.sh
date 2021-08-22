#!/bin/bash

make clean
make
./ex3 sample.in | diff sample.out -
./ex3 small_test.in | diff small_test.out -
./ex3 big_test.in | diff big_test.out -

if command -v valgrind
then
    valgrind ./ex3 sample.in
    valgrind ./ex3 small_test.in
    valgrind ./ex3 big_test.in
fi
