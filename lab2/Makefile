CC=gcc
CFLAGS=-g -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE

.PHONY: clean

all: myshell
bonus: myshell_bonus
myshell: myshell.o driver.o
myshell_bonus: myshell_bonus.o driver.o
clean:
	rm myshell.o myshell_bonus.o driver.o myshell myshell_bonus
