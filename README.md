# mimerunner
MIMErunner is a simple Linux application and library designed for fast unpacking and decoding of email messages and mailboxes.
MIMErunner decode all parts of messages and convert them to utf-8.
Initially MIMErunner was written as part of gigascale spam detection and email mining project.
This code probable not portable and was designed for x86_64 architectures with heavy usage of gcc-specific features.
Also MIMErunner is not a stream filter and work with static files on disk or RAM filesystems.

Only MBOXO format is fully supported, but other formats should work fine in most real cases.

#Installation
#Dependencies
For compile you need following packages:
* re2c
* gperf
* bash
* make
* glibc+iconv

#Building

make

make install

#Running
./mimerun -s -o outfolder your_large_gmail_exported_mbox

#Using as library
#Basic types

mime_t - immutable recursive container of all decoded parts and fields related to single message.
All parts have getters, described in mimerun.h

mime_it_t - iterator for MBOX format, return mime_t for every message.

Examples of usage may be found in 'examples/' folder.

#License
Standard BSD 3-clause

#TODO
BSON output.
JSON output (with base64 encoded data).
Filter expression support or embedded interpreter/JIT.
Doxygen documentation.

#Known bugs
Mailboxes exported from gmail have invalid time format, so integer timestamp may be invalid while textual timestamp extracted correctly.
UUencoded parts/fields not supported
