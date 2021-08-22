#!/bin/bash

make clean
make
./ex2 < sample.in | diff sample.out -
./ex2 < small_test.in | diff small_test.out -
./ex2 < big_test.in | diff big_test.out -