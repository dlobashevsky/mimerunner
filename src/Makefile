# $Id: Makefile 72 2015-06-21 13:13:49Z dima $

NAME=mimerun

CC=gcc

CFLAGS+= -std=gnu11 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -fPIC -Wall -Wno-parentheses -Wno-switch -Wno-pointer-sign -Wno-trampolines \
 -Wno-unused-result -O3 -march=native -mtune=native -funroll-loops

MIME_SRC=../dicts/mime.types
MIME_FIELDS_SRC=../dicts/mime.fields
MIME_ENC_SRC=../dicts/mime.encodings
MIME_CS_SRC=../dicts/mime.charsets

GP=hash/mime.types.gp hash/mime.types.0.gp hash/mime.fields.gp hash/mime.enc.gp hash/mime.cs.gp
GPC=$(GP:.gp=.c)
GENH=hash/mime.types.h hash/mime.fields.h hash/mime.enc.h hash/mime.cs.h

SRC= $(sort $(wildcard *.c) $(GPC))
OBJS= $(SRC:.c=.o)

LOBJS= $(filter-out main.o,$(OBJS))

DFILES= $(SRC:.c=.d)
HFILES= $(wildcard *.h)

LIBNAME=lib$(NAME).a
PROG=$(NAME)

GOALS= $(LIBNAME) $(PROG)

.PHONY: all lib clean dist install

all:	$(DFILES) $(GOALS)

lib: $(LIBNAME)

doc docs:
	cd ..
	rm -Rf docs
	doxygen


$(DFILES): $(SRC) $(HFILES)


$(LIBNAME): $(LOBJS)
	ar rcsu $@ $^

$(PROG): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@
	strip $@

mimeinc.inc:	mimeinc.re2c
	re2c -g --case-insensitive $< > $@

%.d:	%.c
	$(CC) -MM -MG $(CFLAGS) $< > $@


hash/mime.types.gp hash/mime.types.0.gp: $(MIME_SRC)
	../scripts/mime.sh $< hash/mime.types

hash/mime.fields.gp: $(MIME_FIELDS_SRC)
	../scripts/fields.sh $< hash/mime.fields

hash/mime.cs.gp: $(MIME_CS_SRC)
	../scripts/cs.sh $< hash/mime.cs

hash/mime.enc.gp: $(MIME_ENC_SRC)
	../scripts/enc.sh $< hash/mime.enc


hash/mime.types.c: hash/mime.types.gp
	gperf -t -L ANSI-C -G -C -l -N mime_get -K name -I --ignore-case $^ > $@

hash/mime.types.0.c: hash/mime.types.0.gp
	gperf -t -L ANSI-C -G -C -l -N mime_get0 -K name -I --ignore-case $^ > $@

hash/mime.fields.c: hash/mime.fields.gp
	gperf -t -L ANSI-C -G -C -l -N mime_field -K name -I --ignore-case $^ > $@

hash/mime.cs.c: hash/mime.cs.gp
	gperf -t -L ANSI-C -G -C -l -N mime_cs -K name -I --ignore-case $^ > $@

hash/mime.enc.c: hash/mime.enc.gp
	gperf -t -L ANSI-C -G -C -l -N mime_enc -K name -I --ignore-case $^ > $@

install: $(PROG) $(LIBNAME) $(NAME).h
	test -d $(DEST) && install -Ds $(PROG) $(DEST)/bin/$(PROG) && \
  install -D -m 0644 $(LIBNAME) $(DEST)/lib/$(LIBNAME) && \
  install -D -m 0644 $(NAME).h $(DEST)/include/$(NAME).h && \
  install -D -m 0644 ../doc/$(NAME).1 $(DEST)/share/man/man1/$(NAME).1

dist clean:
	rm -fR $(OBJS) $(DFILES) $(LIBNAME) $(PROG) *.tmp *.tmp~ docs ../docs *.inc $(REC) hash/*

ifeq (,$(findstring $(MAKECMDGOALS),dist clean depend doc docs))
-include $(DFILES)
endif

