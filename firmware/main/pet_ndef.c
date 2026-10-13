#include "pet_ndef.h"
#include <stddef.h>
#include <string.h>

bool pet_ndef_image(const uint8_t original[PET_NDEF_BYTES], const char *url,
                    uint8_t out[PET_NDEF_BYTES])
{
    /* This profile reserves dynamic lock bytes at absolute byte 160 (page 40),
     * outside the 144-byte user area. Refuse other TLV layouts. */
    const uint8_t control[5] = {0x01, 0x03, 0xa0, 0x0c, 0x34};
    if (memcmp(original, control, sizeof(control)) || original[5] != 0x03)
        return false;
    size_t suffix_len = 0;
    const char *suffix = NULL;
    uint8_t prefix = 0;
    if (url) {
        if (!strncmp(url, "https://", 8)) { suffix = url + 8; prefix = 4; }
        else if (!strncmp(url, "http://", 7)) { suffix = url + 7; prefix = 3; }
        else return false;
        suffix_len = strlen(suffix);
        if (!suffix_len || suffix_len > PET_NDEF_BYTES - 13) return false;
        for (const unsigned char *p = (const unsigned char *)suffix; *p; p++)
            if (*p <= 0x20 || *p >= 0x7f) return false;
    }
    memset(out, 0, PET_NDEF_BYTES);
    memcpy(out, control, sizeof(control));
    out[5] = 0x03; /* NDEF Message TLV */
    if (!url) { out[6] = 0; out[7] = 0xfe; return true; }
    out[6] = (uint8_t)(5 + suffix_len);
    out[7] = 0xd1; /* MB | ME | SR | TNF well-known */
    out[8] = 1; /* type length */
    out[9] = (uint8_t)(1 + suffix_len);
    out[10] = 'U';
    out[11] = prefix;
    memcpy(out + 12, suffix, suffix_len);
    out[12 + suffix_len] = 0xfe;
    return true;
}

bool pet_ndef_url(const uint8_t *data, unsigned length, char *url, unsigned capacity)
{
    if (!data || !url || !capacity) return false;
    url[0]=0;
    for (unsigned pos=0;pos<length;) {
        unsigned type=data[pos++];
        if (type==0) continue;
        if (type==0xfe || pos>=length) return false;
        unsigned size=data[pos++];
        if (size==0xff || size>length-pos) return false;
        if (type==3) {
            if(size<5 || data[pos]!=0xd1 || data[pos+1]!=1 || data[pos+2]<2 ||
               data[pos+2]+4u!=size || data[pos+3]!='U') return false;
            unsigned prefix=data[pos+4],count=data[pos+2]-1;
            const char *head=prefix==4?"https://":prefix==2?"https://www.":prefix==0?"":NULL;
            if(!head) return false;
            unsigned headlen=strlen(head);
            if(headlen+count>=capacity)return false;
            for(unsigned i=0;i<count;i++)if(data[pos+5+i]<=32 || data[pos+5+i]>=127)return false;
            memcpy(url,head,headlen);memcpy(url+headlen,data+pos+5,count);url[headlen+count]=0;
            if(strncmp(url,"https://",8)){url[0]=0;return false;}return true;
        }
        if(type!=1 && type!=2)return false;
        pos+=size;
    }
    return false;
}
