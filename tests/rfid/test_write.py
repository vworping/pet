"""Fault-inject the actual write-test orchestration without hardware."""
from pathlib import Path
import subprocess
import tempfile
s=Path('firmware/main/pet_rfid.c').read_text()
a=s.index('static void test_write(');b=s.index('static esp_err_t write_user_page(',a)
prefix=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
typedef struct {uint8_t uid[10],len,sak,atqa[2];} card_t;
#define ESP_OK 0
#define ESP_LOG_INFO 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOG_BUFFER_HEX_LEVEL(...) ((void)0)
static bool supported=true;static int reads,writes,fail_read,fail_write;
static uint8_t memory[16],original[16];
static bool classic(const card_t *c){return c->sak==8;}
static bool ntag21x(void){return supported;}
static int authenticate(const card_t *c){(void)c;return 0;}
static int read_data(uint8_t addr,uint8_t *out){assert(addr==4 || addr==8);reads++;if(reads==fail_read)return 1;memcpy(out,memory,16);return 0;}
static int write_data(bool cl,const uint8_t *in){writes++;memcpy(memory,in,cl?16:4);return writes==fail_write?1:0;}
static void reset(void){reads=writes=fail_read=fail_write=0;supported=true;for(int i=0;i<16;i++)memory[i]=original[i]=i;}
'''
main=r'''
int main(void){
    card_t c={.sak=0};
    reset();test_write(&c,"TEST");assert(writes==2 && reads==3);assert(!memcmp(memory,original,16));
    reset();fail_read=1;test_write(&c,"TEST");assert(writes==0);
    reset();supported=false;test_write(&c,"TEST");assert(writes==0);
    reset();fail_write=1;test_write(&c,"TEST");assert(writes==2);assert(!memcmp(memory,original,16));
    reset();fail_read=2;test_write(&c,"TEST");assert(writes==2);assert(!memcmp(memory,original,16));
    reset();c.sak=8;test_write(&c,"TEST");assert(writes==2);assert(!memcmp(memory,original,16));
    reset();c.sak=0x20;test_write(&c,"TEST");assert(writes==0);
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'write.c';exe=Path(d)/'write';c.write_text(prefix+s[a:b]+main)
    subprocess.run(['cc','-Wall','-Werror','-Wno-unused-variable','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Write refusal, read-back, restoration after lost ACK/read, and unsupported-tag tests passed')
