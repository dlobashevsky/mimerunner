#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mimerun.h"


// $Id$

/*
  extract all text parts and join into one text, non-recursive version
*/

#define DIVIDER1		"\f"
#define DIVIDER2		"\f\f"

int main(int ac,char** av)
{
  ac--;
  av++;
  if(!ac)  return -1;

  for(size_t n=0;n<ac;n++)
  {
    mime_t* mime=mime_init_file(av[n],MIME_FLAG_DECODE|MIME_FLAG_FIELDS|MIME_FLAG_SINGLE);
    if(!mime) continue;

    ssize_t parts=mime_getall_count_parts(mime);
    for(ssize_t i=0;i<parts;i++)
    {
      const mime_part_t* part=mime_getall_part(mime,i);
      if(!part)  continue;
      
      mime_string_t s;
      int32_t type=mime_get_type0(mime,part);
      switch(type)
      {
        case MIME_CT0_MESSAGE:
        case MIME_CT0_TEXT:
          s=mime_get_text(mime,part);
          if(s.size>=0)
            printf("%.*s%s",(int)s.size,s.data,DIVIDER1);
          continue;

        case MIME_CT0_MULTIPART:
          s=mime_get_text(mime,part);
          if(s.size>=0)
            printf("%.*s%s",(int)s.size,s.data,DIVIDER1);
          s=mime_get_epilog(mime,part);
          if(s.size>=0)
            printf("%.*s%s",(int)s.size,s.data,DIVIDER1);
          continue;

        default:
          continue;
      }
    }

    printf("%s",DIVIDER2);
    mime_free(mime);
  }

  return 0;
}
