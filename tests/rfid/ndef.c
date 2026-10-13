#include "pet_ndef.h"
#include <assert.h>
#include <string.h>
int main(void) {
    uint8_t original[144]={1,3,0xa0,0x0c,0x34,3,0,0xfe}, out[144], erased[144];
    assert(pet_ndef_image(original,"https://mindofaxd.com",out));
    const uint8_t expected[]={3,18,0xd1,1,14,'U',4,'m','i','n','d','o','f','a','x','d','.','c','o','m',0xfe};
    assert(!memcmp(out+5,expected,sizeof(expected)));
    assert(!memcmp(out,original,5));
    char parsed[150];assert(pet_ndef_url(out,sizeof(out),parsed,sizeof(parsed)));assert(!strcmp(parsed,"https://mindofaxd.com"));
    assert(!pet_ndef_url(out,15,parsed,sizeof(parsed)));assert(!pet_ndef_url(out,sizeof(out),parsed,5));
    uint8_t bad[144];memcpy(bad,out,144);bad[9]=140;assert(!pet_ndef_url(bad,sizeof(bad),parsed,sizeof(parsed)));
    memcpy(bad,out,144);bad[11]=3;assert(!pet_ndef_url(bad,sizeof(bad),parsed,sizeof(parsed)));
    assert(pet_ndef_image(out,NULL,erased));
    assert(erased[5]==3 && erased[6]==0 && erased[7]==0xfe);
    for(unsigned i=8;i<144;i++)assert(erased[i]==0);
    assert(!pet_ndef_image(original,"javascript:alert(1)",out));
    assert(!pet_ndef_image(original,"https://",out));
    assert(!pet_ndef_image(original,"https://bad\nurl",out));
    char url[150];memcpy(url,"https://",8);memset(url+8,'a',131);url[139]=0;
    assert(pet_ndef_image(original,url,out));assert(out[143]==0xfe);
    url[139]='a';url[140]=0;assert(!pet_ndef_image(original,url,out));
    original[0]=0;assert(!pet_ndef_image(original,"https://example.com",out));
}
