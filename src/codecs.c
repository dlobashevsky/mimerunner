#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iconv.h>
#include <obstack.h>

#include "mimerun.h"
#include "codecs.h"
#include "mimerun/mime.cs.h"
#include "mimerun/mime.enc.h"

// $Id$


#define CODECS_SUFFIX		"//TRANSLIT"
#define CS_EXPAND		10

#define CS_DEFAULT_BUF		32


static const uint8_t base64[256]=
{
  [0 ... 255] = 255,
  ['+'] = 62,
  ['/'] = 63,
  ['0'] = 52,53,54,55,56,57,58,59,60,61,
  ['A'] = 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
  ['a'] = 26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
  ['='] = 254,
  ['\n'] = 253,
  ['\r'] = 253,
  ['\t'] = 253,
  [' '] = 253
};

//"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";

static const uint8_t tohex[256]=
{
  [0 ... 255] = 255,
  ['0'] = 0,
  ['1'] = 1,
  ['2'] = 2,
  ['3'] = 3,
  ['4'] = 4,
  ['5'] = 5,
  ['6'] = 6,
  ['7'] = 7,
  ['8'] = 8,
  ['9'] = 9,

  ['a'] = 10,
  ['b'] = 11,
  ['c'] = 12,
  ['d'] = 13,
  ['e'] = 14,
  ['f'] = 15,

  ['A'] = 10,
  ['B'] = 11,
  ['C'] = 12,
  ['D'] = 13,
  ['E'] = 14,
  ['F'] = 15
};

ssize_t codecs_from_base64(const uint8_t* data,size_t sz,uint8_t** ret,uint32_t* err)
{
  if(!data || !ret)  return -1;
  if(!sz)  return 0;
  if(err)  *err=0;

  struct obstack mem;
  obstack_init(&mem);

  uint8_t c=0;
  uint32_t acc=0;
  uint8_t* a=(void*)&acc;
 
  size_t i;
  for(i=0;i<sz;i++)
  {
    uint8_t d=base64[data[i]];
    switch(d)
    {
      case 255:
        if(err) err[0]++;
        obstack_free(&mem,0);
        return -1;
      case 254:
        goto pad;
      case 253:
        continue;
      default:
        acc<<=6;
        acc|=d;
        c++;
        c%=4;
        if(!c)  
        {
//          obstack_grow(&mem,(void*)&acc,3);
          obstack_1grow(&mem,a[2]);
          obstack_1grow(&mem,a[1]);
          obstack_1grow(&mem,a[0]);
          acc=0;
        }
    }
  }

pad:

  switch(c)
  {
    case 1: acc<<=6;
    case 2: acc<<=6;
    case 3: acc<<=6;
  }

  c=0;
  if(i<sz && data[i]=='=')
  {
    c++;
    if(i+1<sz && data[i+1]=='=')  c++;
  }

  switch(c)
  {
    case 1:
      obstack_1grow(&mem,a[2]);
      obstack_1grow(&mem,a[1]);
      break;
    case 2:
      obstack_1grow(&mem,a[2]);
  }

  ssize_t rv=obstack_object_size(&mem);
  *ret=md_malloc(rv ?: 1);
  void* t=obstack_finish(&mem);
  memcpy(*ret,t,rv);
  obstack_free(&mem,0);
  return rv;
}

ssize_t codecs_from_qp(const uint8_t* data,size_t sz,uint8_t** ret,uint32_t* err)
{
  if(!data || !ret)  return -1;
  if(!sz)  return 0;
  if(err)  *err=0;

  struct obstack mem;
  obstack_init(&mem);

  size_t i;
  for(i=0;i<sz;i++)
    switch(data[i])
    {
      case '=': 
      {
        if(++i>=sz)
        {
leave:
          obstack_free(&mem,0);
          if(err) *err=1;
          return -1;
        }

        switch(data[i])
        {
          case '\r':
            if(i+1>=sz || data[i+1]!='\n')  goto leave;
            i++;
          case '\n':
            continue;

          case '0' ... '9':
          case 'a' ... 'f':
          case 'A' ... 'F':
            if(i+1<sz)
            {
              obstack_1grow(&mem,(tohex[data[i]]<<4)|tohex[data[i+1]]);
              i++;
              continue;
            }

          default:
            goto leave;
        }
      }
      default:
        obstack_1grow(&mem,data[i]);
        continue;
    }


  ssize_t rv=obstack_object_size(&mem);
  *ret=md_malloc(rv ?: 1);
  void* t=obstack_finish(&mem);
  memcpy(*ret,t,rv);
  obstack_free(&mem,0);
  return rv;
}

ssize_t codecs_to_utf8(const uint8_t* data,size_t sz,const char* from,uint8_t** ret,uint32_t* err)
{
  if(!data || !ret)  return -1;
  *ret=0;
  if(err)  *err=0;
  if(!sz)  return 0;
  
  iconv_t ic=iconv_open("utf-8" CODECS_SUFFIX,from);
  if(ic==(iconv_t) -1)
    return -1;


  size_t osz=sz*CS_EXPAND;
  char* obuf=md_malloc(osz);
  char* pobuf=obuf;
  size_t isz=sz;
  char* ibuf=(void*)data;
  
  iconv(ic,0,0,&pobuf,&osz);
  ssize_t rv=iconv(ic,&ibuf,&isz,&pobuf,&osz);
  
  if(rv<0)
  {
    md_free(obuf);
    *ret=0;
  }
  else
  {
    *ret=obuf;
  }
  iconv_close(ic);
  return rv<0 ? -1 : pobuf-obuf;
}

ssize_t codecs_from(int32_t enc,const char* cs,const uint8_t* data,size_t sz,uint8_t** ret)
{
  if(!data || !sz || !ret)
    return -1;

  uint8_t* tdata=0;
  ssize_t tsz;
  *ret=0;

  switch(enc)
  {
    case MIME_ENC_BASE64:
	tsz=codecs_from_base64(data,sz,&tdata,0);
	break;

    case MIME_ENC_QUOTED__PRINTABLE:
    case MIME_ENC_QUOT__PRINTED:
	tsz=codecs_from_qp(data,sz,&tdata,0);
	break;

    case MIME_ENC_X__UUE:
    case MIME_ENC_X__UUENCODE:
// TODO:
    case MIME_ENC_7BIT:
    case MIME_ENC_8BIT:
    case MIME_ENC_BINARY:
    case MIME_ENC_PLAIN:
    default:
def:
      tsz=sz;
      tdata=md_malloc(tsz);
      memcpy(tdata,data,sz);
  }
  
  if(tsz<0)  goto def;
  if(!cs)
  {
leave:
    *ret=tdata;
    return tsz;
  }

  uint8_t* rdata;
  ssize_t nsz=codecs_to_utf8(tdata,tsz,cs,&rdata,0);
  if(nsz<0)  goto leave;

  *ret=rdata;
  md_free(tdata);
  return nsz;
}

// example: =?iso-8859-1?B?QWxsIHNvZnR3YXJlIC0gZHV0eS1mcmVlIHByaWNlcw==?=
// TODO: rewrite by regexp (re2c)

ssize_t codecs_word(const uint8_t* data,size_t sz,uint8_t** ret)
{
  if(!data || !sz || !ret)  return -1;
  *ret=0;

  size_t osz=sz*CS_EXPAND;
  uint8_t* obuf=md_malloc(osz);
  uint8_t* tbuf=0;
  size_t z=0;

  const uint8_t* p=data;
  size_t acs=CS_DEFAULT_BUF;
  char* cs=md_malloc(acs);

  uint8_t enc=0;
  int32_t encf;
  
  while(p && p<data+sz)
  {
    const uint8_t* q=memmem(p,data+sz-p,"=?",2);
    if(!q)  break;
    memcpy(obuf+z,p,q-p);
    z+=q-p;
//    const uint8_t* l=memmem(q,data+sz-q,"?=",2);
    const uint8_t* n=q;
    const uint8_t* l=0;
    
    if(data+sz<=q+2)  goto next;
    l=memchr(q+2,'?',data+sz-q-2);
    if(!l) goto next;
    if(data+sz<=l+3)  goto next;
//  TODO  =?something?b??=
//    l=memmem(q,data+sz-q,"?=",2);
    l=memmem(l+3,data+sz-l-3,"?=",2);
/*
we can get something as =?utf8?q?=c0=99=0d?=
*/
    p=q;
    if(!l || l<6+q)
    {
next:
      if(!l)
      {
        memcpy(obuf+z,p,data+sz-p);
        z+=data+sz-p;
        break;
      }
      p=l+2;
      memcpy(obuf+z,n,2+l-n);
      z+=2+l-n;
      continue;
    }

    q=memchr(p+2,'?',l-p-2);
    if(!q || q+2>=data+sz || q[2]!='?')  goto next;

    enc=q[1];
    encf=-1;

    if(enc == 'b' || enc == 'B')  encf=MIME_ENC_BASE64;
    if(enc == 'q' || enc == 'Q')  encf=MIME_ENC_QUOTED__PRINTABLE;

    if(encf == -1)  goto next;

    while(q-p-1>=acs)
    {
      acs*=2;
      md_free(cs);
      cs=md_malloc(acs);
    }

    memcpy(cs,p+2,q-p-2);
    cs[q-p-2]=0;
    if(l<=q+3)  goto next;
    ssize_t u=codecs_from(encf,cs,q+3,l-q-3,&tbuf);
    if(u<0)  goto next;

    memcpy(obuf+z,tbuf,u);
    md_free(tbuf);
    z+=u;
    p=l+2;
  }
  
  if(p<data+sz)
  {
    memcpy(obuf+z,p,data+sz-p);
    z+=data+sz-p;
  }
  md_free(cs);
  *ret=obuf;
  return z;
}


