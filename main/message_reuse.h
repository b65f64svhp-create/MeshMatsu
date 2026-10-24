#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
// Copies only whole UTF-8 codepoints. Returns bytes copied; leaves a terminator.
static inline size_t mesh_message_reuse(char *out,size_t capacity,const char *text){
 if(!out||!capacity)return 0;
 size_t at=0;
 while(text&&text[at]){
  unsigned char c=(unsigned char)text[at];size_t n=c<0x80?1:(c>=0xc2&&c<=0xdf)?2:(c>=0xe0&&c<=0xef)?3:(c>=0xf0&&c<=0xf4)?4:0;
  if(!n||n>=capacity-at)break;
  bool valid=true;for(size_t i=1;i<n;i++)if(!text[at+i]||((unsigned char)text[at+i]&0xc0)!=0x80){valid=false;break;}
  if(!valid)break;
  unsigned char second=(unsigned char)text[at+1];
  if((n==3&&((c==0xe0&&second<0xa0)||(c==0xed&&second>=0xa0)))||(n==4&&((c==0xf0&&second<0x90)||(c==0xf4&&second>0x8f))))break;
  memcpy(out+at,text+at,n);at+=n;
 }
 out[at]=0;return at;
}
