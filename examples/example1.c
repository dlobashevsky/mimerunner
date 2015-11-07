#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include "mimerun.h"


// $Id$

/*
	This example print all header fields with numerical code, non-recursive version
*/

int main(int ac,char** av)
{
  ac--;
  av++;
  if(!ac)  return -1;

  for(size_t n=0;n<ac;n++)
  {
    mime_t* mime=mime_init_file(av[n],MIME_FLAG_DECODE|MIME_FLAG_FIELDS|MIME_FLAG_SINGLE);
    if(!mime) continue;

    ssize_t fields=mime_getall_count_fields(mime);
    for(ssize_t i=0;i<fields;i++)
    {
      const mime_field_t* mf=mime_getall_field(mime,i);
      if(!mf)  continue;
      printf("%d\t",mime_get_field_code(mime,mf));
      mime_string_t s=mime_get_field_text(mime,mf);
      if(s.size>=0)
        printf("'%.*s'",(int)s.size,s.data);

      ssize_t vals=mime_get_count_values(mime,mf);
      for(ssize_t v=0;v<vals;v++)
      {
        s=mime_get_value(mime,mf,v);
        if(s.size>=0)
          printf("\t[%.*s]",(int)s.size,s.data);
      }
      printf("\n");
    }

    mime_free(mime);
  }

  return 0;
}
