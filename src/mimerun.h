// $Id$

//! \file 
//! mime parsing

#include "hash/mime.cs.h"
#include "hash/mime.enc.h"
#include "hash/mime.fields.h"
#include "hash/mime.types.h"

/* memory manager */

#define md_abort	do {  fprintf(stderr,"ERROR %s <%s:%d>\n",__func__,__FILE__,__LINE__); \
			perror("error code:");abort(); } while(0)

#define md_malloc(x)	({ void* tmp__=malloc(x); if(!tmp__) md_abort; tmp__; })
#define md_free		free
#define md_calloc(x)	({ void* tmp__=calloc(x,1); if(!tmp__) md_abort; tmp__; })
#define md_strdup(x)	({ void* tmp__=x ? strdup(x) : 0; if(!tmp__) md_abort; tmp__; })
#define md_realloc(x,y)	({ void* tmp__=realloc(x,y); if(!tmp__) md_abort; tmp__; })

#define md_asprintf(x,...) ({ char *bf__=0; if(asprintf(&bf__, x, ##__VA_ARGS__)<0) md_abort; bf__; })

#define md_tmalloc(x,y)		((x*)md_malloc(sizeof(x)*(y)))
#define md_tcalloc(x,y)		((x*)md_calloc(sizeof(x)*(y)))
#define md_pmalloc(x)		((void*)(md_malloc((x)*sizeof(void*))))
#define md_pcalloc(x)		((void*)(md_calloc((x)*sizeof(void*))))
#define md_new(x)		((typeof(x))md_calloc(sizeof(x[0])))
#define md_anew(x,y)		((typeof(x))md_calloc(sizeof(x[0])*(y)))

#define obstack_chunk_alloc 	malloc
#define obstack_chunk_free 	free

/***************************************/

/*TODO: add different flags for decoding and charset conversion */

//! print errors
#define MIME_FLAG_VERBOSE	1
//! decode all encoded data and convert all texts to utf8
#define MIME_FLAG_DECODE	2
//! convert all fields to binary/utf
#define MIME_FLAG_FIELDS	4
//! not detect mbox container
#define MIME_FLAG_SINGLE	8


//! boundary not set in multipart
#define MIME_ERROR_NOBOUNDARY	1
//! boundary set but not found (actually not error)
#define MIME_ERROR_BOUNDARY	2
//! unknown mime type
#define MIME_ERROR_TYPE_UNKNOWN	4
//! no slash in mime type
#define MIME_ERROR_TYPE_INVALID	8
//! unknown encoding
#define MIME_ERROR_ENC_INVALID	16
//! unknown charset
#define MIME_ERROR_CS_INVALID	32
//! filed without ':' ot no /n/n divider
#define MIME_ERROR_PARSE	64
//! conversion error in field
#define MIME_ERROR_FCONV	128
//! conversion error in body
#define MIME_ERROR_CONV		256


//! value, returned by getters
typedef struct mime_string_t
{
  ssize_t size;			//!< if <0 then error, 0 is valid value
  const uint8_t* data;
} mime_string_t;


//! iterator
typedef struct mime_it_t
{
  uint32_t flags;		//!< MIME_FLAG_*
  int64_t off;			//!< offset for next mail in mbox format or -1 if finished
  const uint8_t* data;
  size_t sz;
//  FILE *f;
  uint64_t lines;
  uint32_t err;
  const char* label;
  struct mmap_t* mm;
} mime_it_t;


//! generic chunk, referred on common storage
typedef struct mime_chunk_t
{
  uint32_t off;			//!< index in top structure::mem
  uint32_t sz;
} mime_chunk_t;


//! optional mailbox head 
typedef struct mime_opthead_t
{
  int32_t addr;			//!< -1 if opthead not exusts or index in top structure::chunks
  int32_t time;
  time_t date;
} mime_opthead_t;


//! pair key/value/comments in head part
typedef struct mime_field_t
{
  int32_t code;			//!< MIME_FIELD_* or -1
  uint32_t name;		//!< key offset in ::chunks
  uint32_t voff;		//!< value offset in ::chunks
  uint32_t vsz;			//!< count of ::chunks
} mime_field_t;


//! mime part
typedef struct mime_part_t
{
  int32_t type;
  int32_t type0;
  int32_t enc;
  int32_t charset;
  int32_t charset_text;
  int32_t boundary;		//!< offset in ::chunks
  int32_t text;			//!< offset in ::chunks (for multipart only before divider taken)
  int32_t epilog;		//!< offset of epilog in ::chunks (for multipart only)

  uint32_t err;

  uint32_t fsz;			//1< count of fields (with comments)
  uint32_t foff;		//!< offset in ::fields

  uint32_t psz;			//!< childs of multipart
  int32_t poff;			//!< offset for multipart or -1
} mime_part_t;


typedef struct mime_t
{
  uint32_t flags;
  mime_opthead_t head;
//  mime_part_t mime;

  mime_chunk_t* chunks;
  size_t csz;

  mime_field_t* fields;
  size_t fsz;

  mime_part_t* parts;
  size_t psz;

  uint8_t* data;
  size_t dsz;
} mime_t;


//! iterator start
mime_it_t* mime_it_init_file(const char* fn,uint32_t flags);
//! iterator start
mime_it_t* mime_it_init(const uint8_t* data,size_t sz,const char* label,uint32_t flags);
//! iterator
mime_t* mime_it_next(mime_it_t*);
//! return error from iterator
uint32_t mime_it_err(mime_it_t*);
//! dtr
void mime_it_free(mime_it_t*);

//! ctr from memory
mime_t* mime_init(const uint8_t* data,size_t sz,const char* label,uint32_t flags);
//! ctr from single file
mime_t* mime_init_file(const char* fn,uint32_t flags);

//! dtr
void mime_free(mime_t*);

//! dump to filesystem
int mime_dump(const mime_t*,const char* dir);

//! debug dump
int mime_ddump(const mime_t*,const char* dir);

// export to BSON
//ssize_t mime_bson(const mime_t*,uint8_t** ret);

/************  getters section ****************/

//! return string with descriptions, must be freed by caller
char* mime_err_str(uint32_t err);


//! return MBOX address part
mime_string_t mime_get_mbox_address(const mime_t*);
//! return MBOX date part
mime_string_t mime_get_mbox_date(const mime_t*);

//! return message root
const mime_part_t* mime_get_root(const mime_t*);

//! return count of parts
ssize_t mime_get_count_parts(const mime_t*,const mime_part_t*);
//! return indexed part
const mime_part_t* mime_get_part(const mime_t*,const mime_part_t*,size_t index);
//! return text of MIME data
mime_string_t mime_get_text(const mime_t*,const mime_part_t*);
//! return epilog of multipart MIME data
mime_string_t mime_get_epilog(const mime_t*,const mime_part_t*);

//! return parsing error code
int32_t mime_get_error(const mime_t*,const mime_part_t*);

//! return count of fields
ssize_t mime_get_count_fields(const mime_t*,const mime_part_t*);
//! return field structure
const mime_field_t* mime_get_field(const mime_t*,const mime_part_t*,size_t idx);

//! return field code or -1 if field unknown
int32_t mime_get_field_code(const mime_t*,const mime_field_t*);
//! return field
mime_string_t mime_get_field_text(const mime_t*,const mime_field_t*);

//! return count of values
ssize_t mime_get_count_values(const mime_t*,const mime_field_t*);
//! return value for field
mime_string_t mime_get_value(const mime_t*,const mime_field_t*,size_t index);

//! return boundary divider
mime_string_t mime_get_boundary(const mime_t*,const mime_part_t*);
//! return charset as text
mime_string_t mime_get_charset_text(const mime_t*,const mime_part_t*);
//! return charset code
int32_t mime_get_charset(const mime_t*,const mime_part_t*);
//! return encoding code
int32_t mime_get_encoding(const mime_t*,const mime_part_t*);
//! return mime type code
int32_t mime_get_type(const mime_t*,const mime_part_t*);
//! return mime type major component code
int32_t mime_get_type0(const mime_t*,const mime_part_t*);

/**********************  non-structured getters *************************************/

//! return count of all parts in message
ssize_t mime_getall_count_parts(const mime_t*);
//! return indexed part
const mime_part_t* mime_getall_part(const mime_t*,size_t index);
//! return count of all fields in message
ssize_t mime_getall_count_fields(const mime_t*);
//! return field structure
const mime_field_t* mime_getall_field(const mime_t*,size_t idx);

