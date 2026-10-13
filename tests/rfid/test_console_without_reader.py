"""Run the real worker's setup/recovery loop with a missing reader and USB input."""
from pathlib import Path
import subprocess,tempfile
source=Path('firmware/main/pet_rfid.c').read_text()
worker=source[source.index('static void worker(void *arg)'):source.index('        card_t c;e=select_card(&c);')]
prefix=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static int polls,inits,errors,clears,selected,delays;
static bool recover,clear_requested;
static int64_t now,pending_until;
static char pending_uid[21];
static jmp_buf done;
static int init_reader(void){inits++;return recover&&inits>1?0:1;}
static int64_t esp_timer_get_time(void){return now;}
typedef struct {int tx_buffer_size,rx_buffer_size;} usb_serial_jtag_driver_config_t;
static int usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *c){return 0;}
static void console_poll(void){polls++;pending_uid[0]='A';pending_until=now+30000000;}
static void pet_cloud_card_error(const char *message){errors++;}
static bool pet_cloud_clear_requested(void){return clear_requested;}
static void pet_cloud_clear_result(bool ok){assert(!ok);clears++;clear_requested=false;}
static void vTaskDelay(int ms){assert(ms==250);now+=250000;if(++delays==24)longjmp(done,1);}
'''
suffix=r'''
 selected++;vTaskDelay(pdMS_TO_TICKS(250));
 }
}
int main(void){
 clear_requested=true;if(!setjmp(done))worker(0);
 assert(polls==24&&inits==2&&errors==24&&clears==1&&!pending_uid[0]&&!selected);
 polls=inits=errors=clears=selected=delays=0;now=0;recover=true;
 if(!setjmp(done))worker(0);
 assert(polls==24&&inits==2&&selected>0);
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'console.c';exe=Path(d)/'console';c.write_text(prefix+worker+suffix)
    subprocess.run(['cc','-Wall','-Wno-unused-variable','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: USB setup survives missing NFC reader, writes refused, clear fails safely, reader recovery resumes polling')
