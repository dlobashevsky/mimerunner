#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <obstack.h>

#include "codecs.h"
#include "mimerun.h"

// $Id$


#define MTMP_INIT_BUF	256

//! minimal size of MAIL text
#define MIME_MINIMAL_LEN	8

typedef struct mime_part_item_t
{
  mime_part_t part;
  struct mime_part_item_t **next;
  const uint8_t** ptrs;
  size_t* ssz;
  size_t idx;
} mime_part_item_t;

typedef struct mime_tmp_t
{
  uint32_t gflags;
  const char* label;
  struct obstack chunks;
  struct obstack data;
  struct obstack fields;
//  struct obstack parts;
  mime_part_item_t* root;
//  size_t parts;
  uint8_t* tmp;
  size_t tsz;
  uint8_t* tmp2;
  size_t tsz2;
  
  uint8_t* store[2];		//!< charset/boundary
  size_t ss[2];			//!< allocated sizez
  size_t as[2];			//!< allocated sizez

  int mbox;
  size_t cnt;
} mime_tmp_t;


typedef struct mmap_t
{
  FILE *f;
  size_t sz;
  uint8_t* data;
} mmap_t;

static mmap_t* mmap_init(const char* fn,uint32_t flags)
{
  if(!fn)  return 0;
  struct stat st;
  if(stat(fn,&st) || ! (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)))
  {
    if(flags&MIME_FLAG_VERBOSE)
      fprintf(stderr,"Source file %s not exists or not plain : %s\n",fn,strerror(errno));
    return 0;
  }

  FILE *f=0;
  if(!(f=fopen(fn,"r")))
  {
    if(flags&MIME_FLAG_VERBOSE)
      fprintf(stderr,"Can not open file %s : %s\n",fn,strerror(errno));
    return 0;
  }

  void* ptr=mmap(0,st.st_size,PROT_READ,MAP_PRIVATE,fileno(f),0);

  if(ptr==(void*)-1)
  {
    if(flags&MIME_FLAG_VERBOSE)
      fprintf(stderr,"Can not read file %s : %s\n",fn,strerror(errno));
    fclose(f);
    return 0;
  }

  mmap_t* rv=md_new(rv);
  rv->f=f;
  rv->data=ptr;
  rv->sz=st.st_size;
  return rv;
}

static void mmap_free(mmap_t* m)
{
  if(!m) return;
  munmap(m->data,m->sz);
  fclose(m->f);
  md_free(m);
}

static void add_chunk(mime_t* mime,mime_tmp_t* mtmp,const uint8_t* data,size_t sz)
{
  mime_chunk_t chunk={off:mime->dsz,sz:sz};
  obstack_grow(&mtmp->chunks,&chunk,sizeof(chunk));
  obstack_grow(&mtmp->data,data,sz);
  mime->csz++;
  mime->dsz+=sz;
}


static void add_chunk_decode(mime_t* mime,mime_tmp_t* mtmp,const uint8_t* data,size_t sz,int32_t enc,int32_t cs)
{
  if(!sz || (enc<0 && cs<0))
  {
    add_chunk(mime,mtmp,data,sz);
    return;
  }
  
  uint8_t* coded=0;
  ssize_t csz=codecs_from(enc,cs>=0 ? mtmp->store[1] : 0,data,sz,&coded);

  if(csz>=0 && coded)
    add_chunk(mime,mtmp,coded,csz);
  else
    add_chunk(mime,mtmp,data,sz);
  md_free(coded);
}


static time_t mime_time(const char* format,const uint8_t* data,size_t sz)
{
  struct tm tm;
  if(!strptime(data,format,&tm))  return -1;
  return timegm(&tm);
}


static ssize_t narrow_left(const uint8_t* data,ssize_t sz)
{
  size_t u=0;
  while(u<sz && memchr("\n\r\t ",data[u],4)) u++;
  return u;
}

static ssize_t narrow_right(const uint8_t* data,ssize_t sz)
{
  while(sz>0 && memchr("\n\r\t ",data[sz-1],4))  sz--;
  return sz;
}

static void field_symb(int c,size_t off,mime_tmp_t* mtmp)
{
  if(off>=mtmp->tsz)
    mtmp->tmp=md_realloc(mtmp->tmp,mtmp->tsz+=MTMP_INIT_BUF);
  mtmp->tmp[off]=c;
}



//!< fill charset and boundary
static void field_content(mime_t* fill,mime_part_t* head,mime_tmp_t* mtmp,const uint8_t* data,size_t sz);

static void field_finalize(size_t sz,mime_t* fill,mime_part_t* head,mime_tmp_t* mtmp,mime_field_t* pair,uint8_t n)
{
  const struct mime_hash_t* h=0;
  switch(pair->code)
  {
    case MIME_FIELD_CONTENT__TRANSFER__ENCODING:
      if(!n)
      {
        h=mime_enc(mtmp->tmp,sz);
        if(h) head->enc=h->idx;
      }
      break;
    case MIME_FIELD_CONTENT__TYPE:
      if(!n)
      {
        h=mime_get(mtmp->tmp,sz);
        if(h) head->type=h->idx;
        const uint8_t* p=memchr(mtmp->tmp,'/',sz);
        if(!p) break;
        h=mime_get0(mtmp->tmp,p-mtmp->tmp);
        if(h) head->type0=h->idx;
      }
      else
        field_content(fill,head,mtmp,mtmp->tmp,sz);
      break;
  }

  if(fill->flags&MIME_FLAG_FIELDS)
  {
    uint8_t* res;
    ssize_t r=codecs_word(mtmp->tmp,sz,&res);
    if(r<0)
      head->err|=MIME_ERROR_FCONV;
    else
    {
      add_chunk(fill,mtmp,res,r);
      md_free(res);
    }
  }
  else
    add_chunk(fill,mtmp,mtmp->tmp,sz);

  pair->vsz++;
}

static void field_parse(const uint8_t* data,size_t sz,mime_t* fill,mime_part_t* head,mime_tmp_t* mtmp)
{
  uint8_t* d=memchr(data,':',sz);
  if(!d)  return;
  
// check field name in hash
  mime_field_t pair={-1,0,0,0};
  pair.name=fill->csz;

  {
    size_t z=narrow_right(data,d-data);
    size_t b=narrow_left(data,z);z=z-b;
    const uint8_t* u=data+b;

    add_chunk(fill,mtmp,u,z);
    pair.voff=fill->csz;

    const struct mime_hash_t* h=mime_field(u,z);

    if(h)
      pair.code=h->idx;

  }

  sz-=++d-data;
  data=d;
  sz=narrow_right(data,sz);
  size_t u=narrow_left(data,sz);sz-=u;data+=u;
// todo: check empty field, sz==0 on such data here
  int state=0;
  uint8_t n=0;
  
  size_t l=0;

  for(size_t i=0;i<sz;i++)
    switch(data[i])
    {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if(!state) state=1;
        continue;
      case ';':
        state=2;
	field_finalize(l,fill,head,mtmp,&pair,n);
	l=0;
	n=1;
        continue;
      default:
        if(state==1)
          field_symb(' ',l++,mtmp);
        state=0;
        field_symb(data[i],l++,mtmp);
        continue;
    }

  field_finalize(l,fill,head,mtmp,&pair,n);

  obstack_grow(&mtmp->fields,&pair,sizeof(pair));
  head->fsz++;
  fill->fsz++;
}


#include "mimeinc.inc"


void mime_free(mime_t* m)
{
  if(!m)  return;

  md_free(m->chunks);
  md_free(m->fields);
  md_free(m->data);
  md_free(m->parts);
  md_free(m);
}


void mime_part_item_free(mime_part_item_t* m)
{
  if(!m)  return;

  for(ssize_t i=0;i<m->part.psz;i++)
    mime_part_item_free(m->next[i]);

  md_free(m->next);
  md_free(m->ptrs);
  md_free(m->ssz);
  md_free(m);
}

int mime_populate(const uint8_t* data,size_t sz,mime_t* fill,mime_part_item_t* head,mime_tmp_t* mtmp)
{
  head->part.type=head->part.type0=head->part.enc=head->part.charset=head->part.charset_text=head->part.boundary=head->part.text=head->part.epilog=-1;
  head->part.foff=fill->fsz;
  head->part.poff=fill->psz;

  int crlf=0;

  ssize_t u=mimeinc_header(data,sz,fill,&head->part,mtmp);

  if(u<=0)
  {
    uint8_t* r1=memmem(data,sz,"\n\n",2);
    uint8_t* r2=memmem(data,sz,"\r\n\r\n",4);
    switch(((!!r1)<<2)| !!r2)
    {
      case 0:
        return 1;
      case 1:
      z2:
        crlf=1;
        u=r2-data+4;
        break;
      case 3:
        if(r2<r1)  goto z2;
      case 2:
        u=r1-data+2;
        break;
    }
  }
  else
    if(u>=2 && data[u-2]=='\r')  crlf=1;

  if(u>=sz)  return 1;
  sz-=u;
  data+=u;
  head->part.psz=0;

  fill->psz++;
  uint8_t* b1=0;
  uint8_t* b2=0;

  if(head->part.type0!=MIME_CT0_MULTIPART)
  {
place_rest:
    md_free(b1);
    md_free(b2);
    head->part.text=fill->csz;
    add_chunk_decode(fill,mtmp,data,sz,head->part.enc,head->part.charset);
//    add_chunk(fill,mtmp,data,sz);
    return 0;
  }

  if(head->part.boundary<0)
  {
    if(mtmp->gflags&MIME_FLAG_VERBOSE)
      fprintf(stderr,"%s%sboundary info not found\n",mtmp->label ?: "",mtmp->label ? ": " : "");
    goto place_rest;
  }

  b1=md_malloc(mtmp->ss[0]+3+crlf);
  b2=md_malloc(mtmp->ss[0]+6+2*crlf);
  memcpy(b1+2,mtmp->store[0],mtmp->ss[0]);
  memcpy(b2+3+crlf,mtmp->store[0],mtmp->ss[0]);
  b1[0]=b2[1+crlf]=b1[1]=b2[2+crlf]=b2[mtmp->ss[0]+3+crlf]=b2[mtmp->ss[0]+4+crlf]='-';
  b1[mtmp->ss[0]+2+crlf]=b2[mtmp->ss[0]+5+2*crlf]=b2[crlf]='\n';
  if(crlf)
    b2[mtmp->ss[0]+5+crlf]=b2[0]=b1[mtmp->ss[0]+2]='\r';

  const uint8_t* p=memmem(data,sz,b1,mtmp->ss[0]+3+crlf);
  if(!p)
  {
    p=memmem(data,sz,b2,mtmp->ss[0]+6+2*crlf);
    if(!p)
    {
      if(mtmp->gflags&MIME_FLAG_VERBOSE)
        fprintf(stderr,"%s%sboundary not found\n",mtmp->label ?: "",mtmp->label ? ": " : "");
      goto place_rest;
    }

    head->part.epilog=fill->csz;
// todo: decode text
    add_chunk_decode(fill,mtmp,p+mtmp->ss[0]+6+2*crlf,data+sz-(p+mtmp->ss[0]+6+2*crlf),head->part.enc,head->part.charset);
//    add_chunk(fill,mtmp,p+mtmp->ss[0]+6,data+sz-(p+mtmp->ss[0]+6));
    sz=p-data;
    goto place_rest;
  }

  head->part.text=fill->csz;
// todo: decode text
  add_chunk_decode(fill,mtmp,data,p-data,head->part.enc,head->part.charset);
//  add_chunk(fill,mtmp,data,p-data);
//  p+=mtmp->ss[0]+3;

  if(p>=data+sz)
  {
    if(mtmp->gflags&MIME_FLAG_VERBOSE)
      fprintf(stderr,"%s%smultipart structure broken\n",mtmp->label ?: "",mtmp->label ? ": " : "");
    goto place_rest;
  }


  sz-=(p-data);
  data=p;

// search epilog
// todo
  p=memmem(data,sz,b2,mtmp->ss[0]+6+2*crlf);
  if(p && p+mtmp->ss[0]+6+2*crlf<=data+sz)
  {
    head->part.epilog=fill->csz;
// todo: decode text
    add_chunk_decode(fill,mtmp,p+mtmp->ss[0]+6+2*crlf,data+sz-(p+mtmp->ss[0]+6+2*crlf),head->part.enc,head->part.charset);
//    add_chunk(fill,mtmp,p+mtmp->ss[0]+6,data+sz-(p+mtmp->ss[0]+6));
    sz=p-data;
  }

  head->part.psz=0;

  for(p=data;p && p<=data+sz;p=memmem(p,sz-(p-data),b1,mtmp->ss[0]+3+crlf))
  {
    head->part.psz++;
    p+=mtmp->ss[0]+3+crlf;
  }

  head->next=md_pcalloc(head->part.psz);
  head->ptrs=md_pcalloc(head->part.psz);
  head->ssz=md_tcalloc(size_t,head->part.psz);

  size_t i=0;

  for(p=data+mtmp->ss[0]+3+crlf;p<data+sz;i++)
  {
    head->ptrs[i]=p;
    p=memmem(p,sz-(p-data),b1,mtmp->ss[0]+3+crlf);
    if(!p)
    {
      head->ssz[i]=data-head->ptrs[i]+sz;
      i++;
      break;
    }
    head->ssz[i]=p-head->ptrs[i];
    p+=mtmp->ss[0]+3+crlf;
  }

  head->part.psz=i;
  head->part.poff=mtmp->cnt;

  for(i=0;i<head->part.psz;i++)
  {
    head->next[i]=md_tcalloc(mime_part_item_t,1);
    head->next[i]->idx=mtmp->cnt++;
  }

  md_free(b1);
  md_free(b2);

  for(i=0;i<head->part.psz;i++)
    mime_populate(head->ptrs[i],head->ssz[i],fill,head->next[i],mtmp);

  return 0;
}

int mime_populate_parts(mime_t* m,mime_part_item_t* p)
{
  memcpy(m->parts+p->idx,&p->part,sizeof(p->part));
  for(size_t i=0;i<p->part.psz;i++)
    mime_populate_parts(m,p->next[i]);
  return 0;
}

mime_t* mime_init(const uint8_t* data,size_t sz,const char* label,uint32_t flags)
{
  if(!data || !sz) return 0;

// simple sanity check
  ssize_t ehead=0,bbody=0;
  {
    uint8_t* r1=memmem(data,sz,"\n\n",2);
    uint8_t* r2=memmem(data,sz,"\r\n\r\n",4);
    switch(((!!r1)<<2)| !!r2)
    {
      case 0:
        return 0;
      case 1:
      z2:
        ehead=r2-data;
        bbody=ehead+4;
        if(bbody>=sz)  return 0;
        break;
      case 3:
        if(r2<r1)  goto z2;
      case 2:
        ehead=r1-data;
        bbody=ehead+2;
        if(bbody>=sz)  return 0;
        break;
    }
  }

  mime_tmp_t mtmp;
  memset(&mtmp,0,sizeof(mtmp));
  obstack_init(&mtmp.chunks);
  obstack_init(&mtmp.data);
  obstack_init(&mtmp.fields);
//  obstack_init(&mtmp.parts);
  mtmp.root=md_new(mtmp.root);
  mtmp.tsz=mtmp.tsz2=MTMP_INIT_BUF;
  mtmp.tmp=md_malloc(mtmp.tsz);
  mtmp.tmp2=md_malloc(mtmp.tsz2);
  mtmp.cnt=1;
  mtmp.gflags=flags;
  mtmp.label=label;
  
  mime_t* rv=md_new(rv);
  rv->flags=flags;
// parse
  ssize_t off=mimeinc_optheader(data,sz,rv,&mtmp);
  if(off<0)  goto err;

  if(sz<=off)  goto err;
  sz-=off;
  data+=off;

  if(mime_populate(data,sz,rv,mtmp.root,&mtmp)<0)  goto err;

  void *t;

  rv->chunks=md_tcalloc(mime_chunk_t,rv->csz);
  t=obstack_finish(&mtmp.chunks);
  memcpy(rv->chunks,t,sizeof(mime_chunk_t)*rv->csz);
  obstack_free(&mtmp.chunks,0);

  rv->data=md_calloc(rv->dsz);
  t=obstack_finish(&mtmp.data);
  memcpy(rv->data,t,rv->dsz);
  obstack_free(&mtmp.data,0);
  
  rv->fields=md_tcalloc(mime_field_t,rv->fsz);
  t=obstack_finish(&mtmp.fields);
  memcpy(rv->fields,t,sizeof(mime_field_t)*rv->fsz);
  obstack_free(&mtmp.fields,0);

  rv->parts=md_tcalloc(mime_part_t,mtmp.cnt);
  rv->psz=mtmp.cnt;

  mime_populate_parts(rv,mtmp.root);

  md_free(mtmp.tmp);
  md_free(mtmp.tmp2);

  for(size_t i=0;i<sizeof(mtmp.store)/sizeof(*mtmp.store);i++)
    md_free(mtmp.store[i]);

  mime_part_item_free(mtmp.root);
  return rv;

err:
  md_free(rv);
  obstack_free(&mtmp.chunks,0);
  obstack_free(&mtmp.data,0);
  obstack_free(&mtmp.fields,0);

  for(size_t i=0;i<sizeof(mtmp.store)/sizeof(*mtmp.store);i++)
    md_free(mtmp.store[i]);
  mime_part_item_free(mtmp.root);
  md_free(mtmp.tmp);
  md_free(mtmp.tmp2);
  return 0;
}

mime_t* mime_init_file(const char* fn,uint32_t flags)
{
  if(!fn)  return 0;
  mmap_t* mm=mmap_init(fn,flags);
  if(!mm)  return 0;
  mime_t* rv=mime_init(mm->data,mm->sz,fn,flags);
  mmap_free(mm);
  return rv;
}

int mime_ddump(const mime_t* m,const char* dir)
{
  if(!m || !dir || !*dir)  return -1;
  if(mkdir(dir,0777))  return -1;
  
  char *bf=md_asprintf("%s/chunks",dir);
  mkdir(bf,0777);
  md_free(bf);

  for(size_t i=0;i<m->csz;i++)
  {
    bf=md_asprintf("%s/chunks/chunk%08zu.data",dir,i);
    FILE *f=fopen(bf,"w");
    fwrite(m->data+m->chunks[i].off,1,m->chunks[i].sz,f);
    fclose(f);
    md_free(bf);
  }

  bf=md_asprintf("%s/list.txt",dir);
  FILE *f=fopen(bf,"w");

  if(m->head.addr>=0)
    fprintf(f,"mbox:\t%d,%d,%zd\n",m->head.addr,m->head.time,m->head.date);

  fprintf(f,"flags:\t%u\n",m->flags);
  fprintf(f,"csz:\t%zd\n",m->csz);
  fprintf(f,"fsz:\t%zd\n",m->fsz);
  fprintf(f,"dsz:\t%zd\n",m->dsz);
  fprintf(f,"psz:\t%zd\n\n---------------------- parts\n",m->psz);

  for(size_t i=0;i<m->psz;i++)
    fprintf(f,"%zd\ttype=%d, type0=%d, enc=%d, cs=%d, cstext=%d, div=%d, text=%d, epilog=%d, foff=%d, fsz=%d, poff=%d, psz=%d, err=%u\n",i,
               m->parts[i].type,
               m->parts[i].type0,
               m->parts[i].enc,
               m->parts[i].charset,
               m->parts[i].charset_text,
               m->parts[i].boundary,
               m->parts[i].text,
               m->parts[i].epilog,
               m->parts[i].foff,
               m->parts[i].fsz,
               m->parts[i].poff,
               m->parts[i].psz,
               m->parts[i].err
               );

  fprintf(f,"\n---------------------- fields\n");

  for(size_t i=0;i<m->fsz;i++)
    fprintf(f,"%zd\tcode=%d, name=%d, voff=%d, vsz=%d\n",i,
               m->fields[i].code,
               m->fields[i].name,
               m->fields[i].voff,
               m->fields[i].vsz
               );

  md_free(bf);
  fclose(f);
  return 0;
}

#define MIME_CHUNK_APPEND(x)	obstack_grow(res,m->data+m->chunks[x].off,m->chunks[x].sz)


#define MIME_DUMP_PRINT(x)	(int)m->chunks[x].sz,m->data+m->chunks[x].off

#define MIME_DUMP_FILE(x,n)				\
do { 							\
    bf=md_asprintf("%s/" n,dir);			\
    FILE *f=fopen(bf,"w");				\
    fwrite(m->data+m->chunks[x].off,1,m->chunks[x].sz,f); \
    fclose(f);						\
    md_free(bf);					\
} while(0)


static int mime_dump_recurse(const mime_t* m,const mime_part_t* mp,const char* dir)
{
  char* bf=md_asprintf("%s/header",dir);
  FILE *f=fopen(bf,"w");

  for(size_t i=0;i<mp->fsz;i++)
  {
    mime_field_t* mf=m->fields+mp->foff+i;
    fprintf(f,"%zd\t%.*s:\n",i,MIME_DUMP_PRINT(mf->name));
    for(size_t j=0;j<mf->vsz;j++)
      fprintf(f,"\t%.*s\n",MIME_DUMP_PRINT(mf->voff+j));

    fprintf(f,"\n\n");
  }
  fclose(f);
  md_free(bf);
  if(mp->text>=0)
    MIME_DUMP_FILE(mp->text,"text");
  if(mp->epilog>=0)
    MIME_DUMP_FILE(mp->epilog,"epilog");
 
  for(size_t i=0;i<mp->psz;i++)
  {
    bf=md_asprintf("%s/part_%08zu",dir,i);
    mkdir(bf,0777);
    mime_dump_recurse(m,m->parts+mp->poff+i,bf);
    md_free(bf);
  }
  return 0;
}

int mime_dump(const mime_t* m,const char* dir)
{
  if(!m || !dir)  return -1;

  if(mkdir(dir,0777))  return -1;


  if(m->head.addr>=0)
  {
    FILE *f;
    char *bf=md_asprintf("%s/mbox.header",dir);
    f=fopen(bf,"w");
    fprintf(f,"from:\t%.*s\n",MIME_DUMP_PRINT(m->head.addr));
    fprintf(f,"date:\t%.*s\n",MIME_DUMP_PRINT(m->head.time));
    fprintf(f,"unixtime:\t%lu\n",m->head.date);
    fclose(f);
    md_free(bf);
  }

  mime_dump_recurse(m,m->parts,dir);
  return 0;
}


static ssize_t mime_plain_type0(const mime_t* m,const char* div,uint8_t** ret,int32_t type)
{
  if(!m || !ret)  return -1;

  *ret=0;
  struct obstack rs;
  obstack_init(&rs);
  struct obstack* res= &rs;

  size_t dsz=0;
  if(div) dsz=strlen(div);

  for(size_t i=0;i<m->psz;i++)
    if(m->parts[i].type0==type)
    {
      if(m->parts[i].text>=0)
      {
        MIME_CHUNK_APPEND(m->parts[i].text);
        if(div)
          obstack_grow(&rs,div,dsz);
      }
      if(m->parts[i].epilog>=0)
      {
        MIME_CHUNK_APPEND(m->parts[i].epilog);
        if(div)
          obstack_grow(&rs,div,dsz);
      }
    }

  ssize_t rv=obstack_object_size(&rs);
  *ret=md_malloc(rv);
  void* t=obstack_finish(&rs);
  memcpy(*ret,t,rv);
  obstack_free(&rs,0);
  return rv;
}


static ssize_t mime_plain_type(const mime_t* m,const char* div,uint8_t** ret,int32_t type)
{
  if(!m || !ret)  return -1;

  *ret=0;
  struct obstack rs;
  obstack_init(&rs);
  struct obstack* res= &rs;

  size_t dsz=0;
  if(div) dsz=strlen(div);

  for(size_t i=0;i<m->psz;i++)
    if(m->parts[i].type==type)
    {
      if(m->parts[i].text>=0)
      {
        MIME_CHUNK_APPEND(m->parts[i].text);
        if(div)
          obstack_grow(&rs,div,dsz);
      }
      if(m->parts[i].epilog>=0)
      {
        MIME_CHUNK_APPEND(m->parts[i].epilog);
        if(div)
          obstack_grow(&rs,div,dsz);
      }
    }

  ssize_t rv=obstack_object_size(&rs);
  *ret=md_malloc(rv);
  void* t=obstack_finish(&rs);
  memcpy(*ret,t,rv);
  obstack_free(&rs,0);
  return rv;
}

ssize_t mime_html(const mime_t* m,const char* div,uint8_t** res)
{
  return mime_plain_type(m,div,res,MIME_CT_TEXT_HTML);
}

ssize_t mime_text(const mime_t* m,const char* div,uint8_t** res)
{
  return mime_plain_type(m,div,res,MIME_CT_TEXT_PLAIN);
}

ssize_t mime_text_all(const mime_t* m,const char* div,uint8_t** res)
{
  return mime_plain_type0(m,div,res,MIME_CT0_TEXT);
}

/*
TODO
ssize_t mime_bson(const mime_t* m,uint8_t** ret)
{
  if(!m || !ret)  return -1;
  size_t rsz=0;

}
*/

/**************************** getters *********************************/

char* mime_err_str(uint32_t err)
{
  char *ret=strdup("");

  if(!err)  return ret;

  static const char* errs[]=
  {
    "Boundary not set",
    "Boundary not found",
    "Unknown mime type",
    "No slash in mime type",
    "Encoding unknown",
    "Charset unknown",
    "Malformed field",
    "Conversion error in field",
    "Conversion error in body"
  };


  size_t c=1;
  for(size_t i=0;i<sizeof(errs)/sizeof(*errs);i++,c<<=1)
    if(err&c)
    {
      char *t=md_asprintf("%s, %s",errs[i],ret);
      md_free(ret);
      ret=t;
    }

  ret[strlen(ret)-2]=0;

  return ret;
}


static mime_string_t mime_string_err={-1,0};

static inline mime_string_t mime_chunk_tostring(const mime_t* m,int32_t z)
{
  if(!m || z<0 || z>=m->csz)  return mime_string_err;
  return (mime_string_t){m->chunks[z].sz,m->data+m->chunks[z].off};
}


mime_string_t mime_get_mbox_address(const mime_t* m)
{
  return mime_chunk_tostring(m,m->head.addr);
}

mime_string_t mime_get_mbox_date(const mime_t* m)
{
  return mime_chunk_tostring(m,m->head.time);
}

const mime_part_t* mime_get_root(const mime_t* m)
{
  return m ? m->parts : 0;
}


mime_string_t mime_get_text(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return mime_string_err;
  return mime_chunk_tostring(m,p->text);
}


mime_string_t mime_get_epilog(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return mime_string_err;
  return mime_chunk_tostring(m,p->text);
}


ssize_t mime_get_count_parts(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return -1;
  return p->psz;
}

const mime_part_t* mime_get_part(const mime_t* m,const mime_part_t* p,size_t index)
{
  if(!m || !p || index>=p->psz)  return 0;
  return m->parts+p->poff+index;
}


int32_t mime_get_error(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return -1;
  return p->err;
}

mime_string_t mime_get_boundary(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return mime_string_err;
  return mime_chunk_tostring(m,p->boundary);
}


mime_string_t mime_get_charset_text(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return mime_string_err;
  return mime_chunk_tostring(m,p->charset_text);
}

int32_t mime_get_charset(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return -1;
  return p->charset;
}

int32_t mime_get_encoding(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return -1;
  return p->enc;
}

int32_t mime_get_type(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return -1;
  return p->type;
}

int32_t mime_get_type0(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return -1;
  return p->type0;
}

ssize_t mime_get_count_fields(const mime_t* m,const mime_part_t* p)
{
  if(!m || !p)  return -1;
  return p->fsz;
}


const mime_field_t* mime_get_field(const mime_t* m,const mime_part_t* p,size_t idx)
{
  if(!m || !p || idx>=p->fsz)  return 0;
  return m->fields+p->foff+idx;
}

int32_t mime_get_field_code(const mime_t* m,const mime_field_t* f)
{
  if(!m || !f)  return -1;
  return f->code;
}

mime_string_t mime_get_field_text(const mime_t* m,const mime_field_t* f)
{
  if(!m || !f)  return mime_string_err;
  return mime_chunk_tostring(m,f->name);
}

ssize_t mime_get_count_values(const mime_t* m,const mime_field_t* f)
{
  if(!m || !f)  return -1;
  return f->vsz;
}

mime_string_t mime_get_value(const mime_t* m,const mime_field_t* f,size_t index)
{
  if(!m || !f || index>=f->vsz)  return mime_string_err;
  return mime_chunk_tostring(m,f->voff+index);
}


ssize_t mime_getall_count_parts(const mime_t* m)
{
  return m ? m->psz : -1;
}

const mime_part_t* mime_getall_part(const mime_t* m,size_t idx)
{
  return (m && idx<m->psz) ? m->parts+idx : 0;
}


ssize_t mime_getall_count_fields(const mime_t* m)
{
  return m ? m->fsz : -1;
}


const mime_field_t* mime_getall_field(const mime_t* m,size_t idx)
{
  return (m && idx<m->fsz) ? m->fields+idx : 0;
}


/**************************** iterator *********************************/


mime_it_t* mime_it_init_file(const char* fn,uint32_t flags)
{
  if(!fn)  return 0;
  mmap_t* mm=mmap_init(fn,flags);
  if(!mm)  return 0;
  mime_it_t* rv=mime_it_init(mm->data,mm->sz,fn,flags);
  rv->mm=mm;
  return rv;
}

void mime_it_free(mime_it_t* it)
{
  mmap_free(it->mm);
  md_free(it);
}

uint32_t mime_it_err(mime_it_t* it)
{
  return it ? it->err : (uint32_t)-1;
}


mime_t* mime_it_next(mime_it_t* it)
{
  if(!it || it->off<0 || it->off+MIME_MINIMAL_LEN>=it->sz)  return 0;
  it->err=0;

  mime_t* ret=0;
  
  if(it->flags&MIME_FLAG_SINGLE)
  {
l:
    ret=mime_init(it->data+it->off,it->sz-it->off,it->label,it->flags);
    it->off=-1;
    return ret;
  }

  const uint8_t* p=memmem(it->data+it->off,it->sz-it->off,"\nFrom ",6);
  if(!p++ || p<=it->data+it->off+MIME_MINIMAL_LEN || p+MIME_MINIMAL_LEN>=it->data+it->sz)  goto l;

  size_t sz=p-it->data-it->off;
  size_t n=it->off;
  it->off+=sz;

  return mime_init(it->data+n,sz,it->label,it->flags);
}


mime_it_t* mime_it_init(const uint8_t* data,size_t sz,const char* label,uint32_t flags)
{
  if(!data || sz<=MIME_MINIMAL_LEN)  return 0;

  mime_it_t* ret=md_new(ret);
  ret->flags=flags;
  ret->data=data;
  ret->sz=sz;
  ret->label=label;

  if(!(ret->flags&MIME_FLAG_SINGLE) && mimeinc_optheader_detect(data,sz)<=0)
    ret->flags|=MIME_FLAG_SINGLE;

  return ret;
}

