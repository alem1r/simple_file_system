DINCLUDE=../include
DSRC=../src

CC=gcc
CFLAGS= -Wall -g -std=gnu99 -Wstrict-prototypes -I$(DINCLUDE)

SRC=$(wildcard *.c)
BINS=$(SRC:.c=)
OBJ=$(wildcard $(DSRC)/*.o)



all: $(BINS)

%: %.c 
	$(CC) -o $@ $< $(OBJ) $(CFLAGS)

run:
	./run_shell.sh

clean:
	rm -rf $(BINS) *.img vgcore*
