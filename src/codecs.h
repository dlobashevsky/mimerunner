// $Id$

//! \file 
//! minimal decoders

/***************  base primitives ***********************/

//! base64
ssize_t codecs_from_base64(const uint8_t* data,size_t sz,uint8_t** ret,uint32_t* err);
//! quoted-printable
ssize_t codecs_from_qp(const uint8_t* data,size_t sz,uint8_t** ret,uint32_t* err);
//! uuencoded /TODO!
ssize_t codecs_from_uu(const uint8_t* data,size_t sz,uint8_t** ret,uint32_t* err);

//! convert charset
ssize_t codecs_to_utf8(const uint8_t* data,size_t sz,const char* from,uint8_t** ret,uint32_t* err);


//! direct supply of codec type and charset
ssize_t codecs_from(int32_t enc,const char* cs,const uint8_t* data,size_t sz,uint8_t** ret);

//! parse forms as =?koi8-r?q?something?=
ssize_t codecs_word(const uint8_t* data,size_t sz,uint8_t** ret);
