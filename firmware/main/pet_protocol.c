#include "pet_protocol.h"
#include "pet_glyphs.h"
#include <string.h>
bool pet_portal_peer_allowed(uint32_t local,uint32_t peer){
 return local==0xc0a80401u&&(peer&0xffffff00u)==0xc0a80400u&&(peer&255u)>=2&&(peer&255u)<=254;
}
size_t pet_dns_reply(const uint8_t *q,size_t n,uint8_t *r,size_t cap){
 if(n<17||n>512||(q[2]&0xf8)||q[4]||q[5]!=1)return 0;
 size_t p=12;
 while(p<n&&q[p]){unsigned len=q[p++];if(len>63||p+len>=n)return 0;p+=len;}
 if(p>=n||p+5>n)return 0;
 p++;
 unsigned type=((unsigned)q[p]<<8)|q[p+1],cls=((unsigned)q[p+2]<<8)|q[p+3];p+=4;
 if(cls!=1||p>cap)return 0;
 bool answer=type==1;if(answer&&p+16>cap)return 0;
 memcpy(r,q,p);r[2]=0x80|(q[2]&1);r[3]=0x80;memset(r+6,0,6);r[7]=answer?1:0;
 if(answer){const uint8_t a[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};memcpy(r+p,a,sizeof(a));p+=sizeof(a);}
 return p;
}
bool pet_text_valid(const char *text,size_t max_characters){
 if(!text)return false;
 const unsigned char *p=(const unsigned char*)text;size_t count=0;bool visible=false;
 while(*p){uint32_t c;unsigned remaining;
  if(*p<0x80){c=*p++;remaining=0;}
  else if(*p>=0xc2&&*p<=0xdf){c=*p++&31;remaining=1;}
  else if(*p>=0xe0&&*p<=0xef){c=*p++&15;remaining=2;}
  else return false;
  unsigned bytes=remaining;
  while(remaining--){if((*p&0xc0)!=0x80)return false;c=(c<<6)|(*p++&63);}
  if((bytes==1&&c<0x80)||(bytes==2&&c<0x800)||(c>=0xd800&&c<=0xdfff))return false;
  bool supported=false;for(size_t i=0;i<sizeof(pet_supported_glyphs)/sizeof(*pet_supported_glyphs);i++){if(pet_supported_glyphs[i]==c){supported=true;break;}}
  if(!supported||++count>max_characters)return false;
  if(c!=32&&c!=160)visible=true;
 }
 return visible;
}
