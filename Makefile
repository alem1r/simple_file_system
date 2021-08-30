DINCLUDE=./include
DSRC=./src

CC=gcc
CFLAGS=-Wall -g -std=gnu99 -Wstrict-prototypes -I$(DINCLUDE)

HEADERS=$(wildcard $(DINCLUDE)/*)
SRC=$(wildcard $(DSRC)/*.c)
OBJ=$(patsubst %.c,%.o,$(wildcard $(DSRC)/*.c))

.PHONY: clean all

all: $(OBJ) sh test 

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)



run_shell:
	make -s -C ./shell run



sh:
	make -C ./shell

clean:
	make -C ./tests clean
	make -C ./shell clean
	rm -rf $(DSRC)/*.o