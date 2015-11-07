#!/bin/bash

# $Id$

SRC=$1
DST=$2

#SRC=../../data/mime.types
#DST=mime.types

tmp=`mktemp`

echo -e "struct mime_hash_t\n{\n  const char* name;\n  int idx;\n};\n\n%%" >$DST.gp
cat $SRC | grep -v '^[[:blank:]]' | grep -v ^$ | sed -e 's/[[:blank:]].*$//' | tr [A-Z] [a-z] | grep '/' | sort -u >$tmp
cat $tmp | sed -e 's/^\(.*\)$/"\1"/' | nl -ba -v0 -s, -w1 | sed -e 's/^\([0-9][0-9]*\),\(.*\)$/\2,\1/' >>$DST.gp

#gperf -t -L ANSI-C -G -C -l -N mime_get -K name -I --ignore-case $DST.gp > $DST.c
count=`cat $tmp | wc -l`

echo -e "struct mime_hash_t\n{\n  const char* name;\n  int idx;\n};\n\n" >$DST.h
echo -e "const struct mime_hash_t *mime_get(register const char *str, register unsigned int len);\n\n" >>$DST.h

cat $tmp | tr [a-z] [A-Z] | sed -e 's/-/__/g' -e 's/+/___/g' -e 's/\./____/g' -e 's/\//_/g' | nl -ba -v0 -s, -w1 | \
 sed -e 's/^\([0-9][0-9]*\),\(.*\)$/#define MIME_CT_\2\t\1/' >>$DST.h

echo -e "\n#define MIME_CTS\t" $count >>$DST.h

echo -e "struct mime_hash_t\n{\n  const char* name;\n  int idx;\n};\n\n%%" >$DST.0.gp
cat $tmp | sed -e 's/\/.*$//'| sort -u | sed -e 's/^\(.*\)$/"\1"/' | nl -ba -v0 -s, -w1 | sed -e 's/^\([0-9][0-9]*\),\(.*\)$/\2,\1/' >>$DST.0.gp

#gperf -t -L ANSI-C -G -C -l -N mime_get0 -K name -I --ignore-case $DST.0.gp > $DST.0.c

echo -e "\n\nconst struct mime_hash_t *mime_get0(register const char *str, register unsigned int len);\n\n" >>$DST.h

cat $tmp | sed -e 's/\/.*$//'| sort -u | tr [a-z] [A-Z] | sed -e 's/-/__/g' -e 's/+/___/g' -e 's/\./____/g' | nl -ba -v0 -s, -w1 | \
 sed -e 's/^\([0-9][0-9]*\),\(.*\)$/#define MIME_CT0_\2\t\1/' >>$DST.h

count0=`cat $tmp | sed -e 's/\/.*$//'| sort -u | wc -l`
echo -e "\n#define MIME_CT0S\t" $count0 >>$DST.h

rm -f $tmp
