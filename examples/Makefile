CC=gcc

CFLAGS+= -std=gnu11 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -fPIC -Wall -Wno-parentheses -Wno-switch -Wno-pointer-sign -Wno-trampolines \
 -Wno-unused-result -g -I../src

LIBS=-L../src -lmimerunner

SRC= $(wildcard *.c)
BINS= $(SRC:.c=.run)


all: $(BINS)

%.run: %.c
	gcc $(CFLAGS) $< $(LIBS) -o $@

clean:
	rm -f $(BINS) *.img
