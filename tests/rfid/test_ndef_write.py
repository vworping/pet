"""Exercise actual NDEF page-write ordering and failure handling."""
from pathlib import Path
import subprocess
import tempfile
s=Path('firmware/main/pet_rfid.c').read_text()
a=s.index('static esp_err_t write_user_page(');b=s.index('static bool valid_uid(',a)
prefix=r'''
#include "pet_ndef.h"
#include <assert.h>
#include <string.h>
#include <stddef.h>
typedef int esp_err_t;
typedef struct {uint8_t sak;} card_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NOT_SUPPORTED 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_INVALID_RESPONSE 4
#define ESP_LOG_INFO 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOG_BUFFER_HEX_LEVEL(...) ((void)0)
#define TRY(x) do {int e_=(x);if(e_)return e_;}while(0)
static uint8_t memory[192];static unsigned writes,fail_at;
static bool ntag21x(void){return true;}
static int read_data(uint8_t page,uint8_t *out){assert(page<=40);memcpy(out,memory+page*4,16);return 0;}
static int ack_command(uint8_t *tx,size_t n){
    assert(n==6 && tx[0]==0xa2 && tx[1]>=4 && tx[1]<=39);
    if(++writes==fail_at)return 9;
    if(writes==1){assert(tx[1]==5 && tx[4]==0 && tx[5]==0xfe);}
    memcpy(memory+tx[1]*4,tx+2,4);return 0;
}
static void reset(void){
    memset(memory,0,sizeof(memory));writes=fail_at=0;
    memory[12]=0xe1;memory[13]=0x10;memory[14]=0x12;
    const uint8_t prefix[]={1,3,0xa0,0x0c,0x34,3,0,0xfe};memcpy(memory+16,prefix,8);
}
'''
main=r'''
int main(void){
    card_t c={0};uint8_t expected[144],before[144];reset();memcpy(before,memory+16,144);
    assert(pet_ndef_image(before,"https://mindofaxd.com",expected));
    assert(write_ndef(&c,"UID","https://mindofaxd.com")==0);
    assert(!memcmp(memory+16,expected,144));
    writes=0;assert(write_ndef(&c,"UID",NULL)==0);
    assert(memory[22]==0 && memory[23]==0xfe);
    for(unsigned i=24;i<160;i++)assert(memory[i]==0);
    reset();fail_at=2;assert(write_ndef(&c,"UID","https://mindofaxd.com")==9);assert(memory[22]==0);
    reset();memory[11]=1;assert(write_ndef(&c,"UID","https://mindofaxd.com")!=0);assert(!writes);
    reset();memory[160]=1;assert(write_ndef(&c,"UID","https://mindofaxd.com")!=0);assert(!writes);
    reset();assert(write_user_page(3,before)!=0);assert(write_user_page(40,before)!=0);assert(!writes);
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'ndef-write.c';exe=Path(d)/'ndef-write';c.write_text(prefix+s[a:b]+main)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined','-Ifirmware/main',str(c),'firmware/main/pet_ndef.c','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('NDEF write/erase, commit ordering, interrupted write, lock and page-boundary tests passed')
