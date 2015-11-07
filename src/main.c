#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

#include "mimerun.h"

// $Id$


typedef enum out_t
{
  OUT_DIRECTORY=0,
  OUT_JSON,
  OUT_BSON,
  OUT_XML
} out_t;

static const char* usage=
  "mimerun\n$Id$\n\n"
  "options:\n"
  "\t-o filename\t\toutput file or directory name\n"
//  "\t-f filename\t\tfilter file (not yet implemented\n"
//  "\t-b\t\toutput as bson\n"
//  "\t-j\t\toutput as json (all texts in base64)\n"
  "\t-d\t\toutput as directory dump (default)\n"
//  "\t-x\t\toutput as XML (not implemented)\n"
  "\t-v\t\tversion\n"
  "\t-h\t\tthis help\n"
  "\t-s\t\tsilent\n";


int main(int ac,char** av)
{
  ac--;
  av++;

  if(!ac)
  {
    fprintf(stderr,usage);
    return 0;
  }

  size_t idx=0;
  uint32_t flags=MIME_FLAG_VERBOSE;
//  int mbox=0;
//  int text=0;
  
  out_t outtype=OUT_DIRECTORY;
  char* outfile=0;
//  char* filterfile=0;

  char* source[ac];

  for(size_t n=0;n<ac;n++)
    if(av[n][0]=='-')
      switch(av[n][1])
      {
        case 'o':
          if(++n>=ac)
          {
            fprintf(stderr,"output file name empty, exiting");
            return EINVAL;
          }
          outfile=av[n];
          continue;
#if 0
        case 'f':
          if(++n>=ac)
          {
            fprintf(stderr,"filter file name empty, exiting");
            return EINVAL;
          }
          filterfile=av[n];
          continue;

        case 'm':
          mbox=1;
          continue;

        case 't':
          text=1;
          continue;
#endif
        case 'j':
          outtype=OUT_JSON;
          continue;
        case 'b':
          outtype=OUT_BSON;
          continue;

        case 'd':
          outtype=OUT_DIRECTORY;
          continue;

        case 'x':
          fprintf(stderr,"sorry, not yet implemented\n");
          continue;

        case 's':
          flags=0;
          continue;

        case 'v':
          fprintf(stderr,"$Id$\n");
          continue;

        case 'h':
          fprintf(stderr,usage);
          continue;

        default:
          fprintf(stderr,"Unknown switch [-%s]",av[n]);
          fprintf(stderr,usage);
          return EINVAL;
      }
    else
      source[idx++]=av[n];

  if(!outfile)
  {
    fprintf(stderr,"output file name empty, exiting");
    return EINVAL;
  }

  if(!idx)
  {
    fprintf(stderr,"no source file found");
    return 0;
  }

  FILE *fout=0;
  switch(outtype)
  {
    case OUT_BSON:
    case OUT_JSON:
    case OUT_XML:
      fout=fopen(outfile,"w");
      if(!fout)
      {
        perror("Output file creation error");
        return errno;
      }
      break;
    case OUT_DIRECTORY:
      {
        struct stat st;
        if(!stat(outfile,&st))
        {
          if(!S_ISDIR(st.st_mode))
          {
            fprintf(stderr,"Target directory exists and not a directory");
            return EEXIST;
          }
          break;
        }
        if(mkdir(outfile,0666))
        {
          perror("Output directory creation error");
          return errno;
        }
      }
      break;
    default:
      fprintf(stderr,"Something unusual here, leaving");
      return EPERM;
  }

  size_t count=0;

  for(size_t n=0;n<idx;n++)
  {
    mime_it_t* it=mime_it_init_file(source[n],flags|MIME_FLAG_DECODE|MIME_FLAG_FIELDS);
    if(!it)
    {
      if(flags&MIME_FLAG_VERBOSE)
        fprintf(stderr,"cant parse file %s\n",source[n]);
      continue;
    }
    mime_t* m=0;
    while(m=mime_it_next(it))
    {
      if((flags&MIME_FLAG_VERBOSE) && mime_it_err(it))
        fprintf(stderr,"error in file %s: %s\n",source[n],mime_err_str(mime_it_err(it)));

      switch(outtype)
      {
        case OUT_BSON:
          
          break;
        case OUT_XML:
          break;
        case OUT_DIRECTORY:
          {
            char *dir=md_asprintf("%s/mail_%09zu",outfile,count++);
            if(mime_dump(m,dir) && (flags&MIME_FLAG_VERBOSE))
              fprintf(stderr,"%s: dump error\n",source[n]);
            md_free(dir);
          }
          break;
      }
      mime_free(m);
    }
    mime_it_free(it);
    it=0;
  }

  switch(outtype)
  {
    case OUT_JSON:
    case OUT_BSON:
    case OUT_XML:
      fclose(fout);
      break;
    case OUT_DIRECTORY:
      break;
  }

  return 0;
}

