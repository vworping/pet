#include "pet_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 assert(pet_portal_peer_allowed(0xc0a80401,0xc0a80402));
 assert(!pet_portal_peer_allowed(0xc0a80110,0xc0a80402));
 assert(!pet_portal_peer_allowed(0xc0a80401,0xc0a80102));
 assert(!pet_portal_peer_allowed(0xc0a80401,0xc0a804ff));
 assert(pet_text_valid("Zoë García",40));assert(pet_text_valid("François, Łukasz & D’Arcy",40));
 assert(pet_text_valid("Café — déjà vu…",80));assert(!pet_text_valid(" ",40));assert(!pet_text_valid("bad\ntext",80));
 assert(!pet_text_valid("\xc0\xaf",40));assert(!pet_text_valid("\xed\xa0\x80",40));assert(!pet_text_valid("\xe2",40));
 assert(!pet_text_valid("🙂",40));assert(!pet_text_valid("abc",2));
 uint8_t q[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,3,'w','w','w',4,'t','e','s','t',0,0,1,0,1};
 uint8_t r[528];size_t n=pet_dns_reply(q,sizeof(q),r,sizeof(r));assert(n==sizeof(q)+16&&r[7]==1&&r[n-4]==192&&r[n-1]==1);
 for(size_t i=0;i<sizeof(q);i++)assert(!pet_dns_reply(q,i,r,sizeof(r)));
 assert(!pet_dns_reply(q,sizeof(q),r,10));q[23]=28;n=pet_dns_reply(q,sizeof(q),r,sizeof(r));assert(n==sizeof(q)&&r[7]==0);
 q[12]=0xc0;assert(!pet_dns_reply(q,sizeof(q),r,sizeof(r)));
 uint8_t noise[512];unsigned state=47;
 for(int t=0;t<10000;t++){for(size_t i=0;i<sizeof(noise);i++){state=state*1664525u+1013904223u;noise[i]=(uint8_t)(state>>24);}pet_dns_reply(noise,(unsigned)t%513,r,sizeof(r));}
 puts("PASS: AP-only peers, bounded DNS, malformed datagrams, supported Unicode and malformed UTF-8");
}
