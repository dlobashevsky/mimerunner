#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mimerun.h"


// $Id$

/*
  Extract all images, recursive version
  For effective extraction sequetial version (as in examples 1 and 3) preferrable, in this code recursion used only as example
*/

static size_t recurse(const mime_t* mime,const mime_part_t* part,size_t count);

int main(int ac,char** av)
{
  ac--;
  av++;
  if(!ac)  return -1;

  size_t cnt=0;

  for(size_t n=0;n<ac;n++)
  {
    mime_t* mime=mime_init_file(av[n],MIME_FLAG_DECODE|MIME_FLAG_FIELDS|MIME_FLAG_SINGLE);
    if(!mime) continue;

    const mime_part_t* root=mime_get_root(mime);
    if(root)
      cnt=recurse(mime,root,cnt);
    mime_free(mime);
  }

  return 0;
}

static size_t recurse(const mime_t* mime,const mime_part_t* part,size_t count)
{
  int32_t type=mime_get_type0(mime,part);

  mime_string_t s;
  ssize_t pc;
  switch(type)
  {
    case MIME_CT0_IMAGE:
      s=mime_get_text(mime,part);
      if(s.size>=0)
      {
        char *name=md_asprintf("%zd.img",count);
        FILE *f=fopen(name,"wb");
        md_free(name);
        if(f)
        {
          fwrite(s.data,s.size,1,f);
          fclose(f);
        }
      }
      ++count;
      break;

    case MIME_CT0_MULTIPART:
        pc=mime_get_count_parts(mime,part);
        if(pc<=0) break;
        for(size_t i=0;i<pc;i++)
          count=recurse(mime,mime_get_part(mime,part,i),count);
  }
  return count;
}
