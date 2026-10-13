"""Ensure only supported, CRC-valid, writable tag profiles reach page writes."""
from pathlib import Path
import subprocess
import tempfile
s=Path('firmware/main/pet_rfid.c').read_text()
a=s.index('static void crc_a(');b=s.index('static esp_err_t exchange(',a)
x=s.index('static bool ntag21x(');y=s.index('static void test_write(',x)
prefix=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_LOG_INFO 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOG_BUFFER_HEX_LEVEL(...) ((void)0)
static uint8_t ver[10],header[16];static bool bad_crc;
'''
mocks=r'''
static int transceive(const uint8_t *tx,size_t n,uint8_t bits,uint8_t *rx,size_t *rn,uint8_t *rb){
    (void)tx;(void)n;(void)bits;crc_a(ver,8,ver+8);memcpy(rx,ver,10);if(bad_crc)rx[9]^=1;*rn=10;*rb=0;return 0;
}
static int read_data(uint8_t addr,uint8_t *out){assert(addr==0);memcpy(out,header,16);return 0;}
'''
main=r'''
int main(void){
    uint8_t v[8]={0,0x53,4,2,1,0,0x0f,3};memcpy(ver,v,8);
    header[12]=0xe1;header[13]=0x10;header[14]=0x12;header[15]=0;
    assert(ntag21x());ver[1]=4;assert(ntag21x());
    ver[1]=0x55;assert(!ntag21x());ver[1]=0x53;
    header[11]=1;assert(!ntag21x());header[11]=0;
    header[15]=15;assert(!ntag21x());header[15]=0;
    header[14]=2;assert(!ntag21x());header[14]=0x12;
    bad_crc=true;assert(!ntag21x());
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'profile.c';exe=Path(d)/'profile';c.write_text(prefix+s[a:b]+mocks+s[x:y]+main)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Supported vendor, CRC, lock, access, and capacity guards passed')
