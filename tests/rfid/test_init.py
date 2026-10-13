"""Exercise actual reader initialization against a reset-register model."""
from pathlib import Path
import subprocess
import tempfile
s=Path('firmware/main/pet_rfid.c').read_text()
a=s.index('static esp_err_t init_reader(');b=s.index('static esp_err_t select_card(',a)
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_TIMEOUT 1
#define ESP_ERR_INVALID_RESPONSE 2
#define CMD 1
#define TAG "test"
#define TRY(x) do { int e_=(x); if(e_)return e_; }while(0)
#define ESP_LOGI(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static uint8_t regs[64];static int fail;
static esp_err_t wr(uint8_t r,uint8_t v){if(fail)return 9;if(r==1 && v==15){memset(regs,0,64);regs[0x14]=0x80;regs[0x37]=0x92;}else regs[r]=v;return 0;}
static esp_err_t rd(uint8_t r,uint8_t *v){if(fail)return 9;*v=regs[r];return 0;}
static int64_t esp_timer_get_time(void){static int64_t t;return t+=1000;}
static void vTaskDelay(int t){(void)t;}
'''
main=r'''
int main(void){assert(init_reader()==0);assert(regs[0x14]==0x83);assert(regs[0x15]==0x40);assert(regs[0x26]==0x70);fail=1;assert(init_reader()==9);}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'init.c';exe=Path(d)/'init';c.write_text(prefix+s[a:b]+main)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Antenna polarity preservation and I2C failure propagation passed')
