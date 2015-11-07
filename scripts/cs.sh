#!/bin/bash
# $Id$

SRC=$1
DST=$2

#SRC=../../data/mime.charsets
#DST=mime.fields

tmp=`mktemp`

echo -e "struct mime_hash_t\n{\n  const char* name;\n  int idx;\n};\n\n%%" >$DST.gp
cat $SRC | grep -v '^[[:blank:]]' | grep -v ^$ | sed -e 's/[[:blank:]].*$//' | tr [A-Z] [a-z] | sort -u >$tmp
cat $tmp | sed -e 's/^\(.*\)$/"\1"/' | nl -ba -v0 -s, -w1 | sed -e 's/^\([0-9][0-9]*\),\(.*\)$/\2,\1/' >>$DST.gp

#gperf -t -L ANSI-C -G -C -l -N mime_cs -K name -I --ignore-case $DST.gp > $DST.c

count=`cat $tmp | wc -l`

echo -e "const struct mime_hash_t *mime_cs(register const char *str, register unsigned int len);\n\n" >$DST.h

cat $tmp | tr [a-z] [A-Z] | sed -e 's/-/__/g' -e 's/\./___/g' -e 's/:/____/g' -e 's/\//_____/g' -e 's/(/______/g' -e 's/)/_______/g' | nl -ba -v0 -s, -w1 | \
 sed -e 's/^\([0-9][0-9]*\),\(.*\)$/#define MIME_CS_\2\t\1/' >>$DST.h

echo -e "\n#define MIME_CS_COUNT\t" $count >>$DST.h

rm -f $tmp
