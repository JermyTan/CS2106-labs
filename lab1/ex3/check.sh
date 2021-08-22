#!/bin/bash

make clean
make
./ex3 sample.in | diff sample.out -
./ex3 small_test.in | diff small_test.out -
./ex3 big_test.in | diff big_test.out -