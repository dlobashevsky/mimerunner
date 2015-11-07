# $Id: Makefile 7 2015-04-05 07:40:16Z dima $


DEST ?=/usr/local

all:
	make -C src

clean:
	make -C src clean
	make -C examples clean

install:
	DEST=$(DEST) make -C src install
