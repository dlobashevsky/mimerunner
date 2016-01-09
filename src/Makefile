# $Id: Makefile 72 2015-06-21 13:13:49Z dima $

NAME=mimerunner

CC=gcc

CFLAGS+= -std=gnu11 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -fPIC -Wall -Wno-parentheses -Wno-switch -Wno-pointer-sign -Wno-trampolines \
 -Wno-unused-result -O3 -march=native -mtune=native -funroll-loops

MIME_SRC=../dicts/mime.types
MIME_FIELDS_SRC=../dicts/mime.fields
MIME_ENC_SRC=../dicts/mime.encodings
MIME_CS_SRC=../dicts/mime.charsets

GP=mimerun/mime.types.gp mimerun/mime.types.0.gp mimerun/mime.fields.gp mimerun/mime.enc.gp mimerun/mime.cs.gp
GPC=$(GP:.gp=.c)
GENH=mimerun/mime.types.h mimerun/mime.fields.h mimerun/mime.enc.h mimerun/mime.cs.h

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


mimerun/mime.types.gp mimerun/mime.types.0.gp: $(MIME_SRC)
	../scripts/mime.sh $< mimerun/mime.types

mimerun/mime.fields.gp: $(MIME_FIELDS_SRC)
	../scripts/fields.sh $< mimerun/mime.fields

mimerun/mime.cs.gp: $(MIME_CS_SRC)
	../scripts/cs.sh $< mimerun/mime.cs

mimerun/mime.enc.gp: $(MIME_ENC_SRC)
	../scripts/enc.sh $< mimerun/mime.enc


mimerun/mime.types.c: mimerun/mime.types.gp
	gperf -t -L ANSI-C -G -C -l -N mime_get -K name -I --ignore-case $^ > $@

mimerun/mime.types.0.c: mimerun/mime.types.0.gp
	gperf -t -L ANSI-C -G -C -l -N mime_get0 -K name -I --ignore-case $^ > $@

mimerun/mime.fields.c: mimerun/mime.fields.gp
	gperf -t -L ANSI-C -G -C -l -N mime_field -K name -I --ignore-case $^ > $@

mimerun/mime.cs.c: mimerun/mime.cs.gp
	gperf -t -L ANSI-C -G -C -l -N mime_cs -K name -I --ignore-case $^ > $@

mimerun/mime.enc.c: mimerun/mime.enc.gp
	gperf -t -L ANSI-C -G -C -l -N mime_enc -K name -I --ignore-case $^ > $@

install: $(PROG) $(LIBNAME) $(NAME).h
	test -d $(DEST) && install -Ds $(PROG) $(DEST)/bin/$(PROG) && \
  install -D -m 0644 $(LIBNAME) $(DEST)/lib/$(LIBNAME) && \
  install -D -m 0644 $(NAME).h $(DEST)/include/$(NAME).h && \
  install -D -m 0644 mimerun/mime.cs.h  $(DEST)/include/mimerun/mime.cs.h && \
  install -D -m 0644 mimerun/mime.enc.h  $(DEST)/include/mimerun/mime.enc.h && \
  install -D -m 0644 mimerun/mime.fields.h  $(DEST)/include/mimerun/mime.fields.h && \
  install -D -m 0644 mimerun/mime.types.h  $(DEST)/include/mimerun/mime.types.h && \
  install -D -m 0644 ../doc/$(NAME).1 $(DEST)/share/man/man1/$(NAME).1


dist clean:
	rm -fR $(OBJS) $(DFILES) $(LIBNAME) $(PROG) *.tmp *.tmp~ docs ../docs *.inc $(REC) mimerun/*

ifeq (,$(findstring $(MAKECMDGOALS),dist clean depend doc docs))
-include $(DFILES)
endif

