"""Exercise the actual firmware queue/review functions with injected NVS failures."""
from pathlib import Path
import tempfile,subprocess
source=Path('firmware/main/pet_cloud.c').read_text()
code=source[source.index('#define CAPACITY'):source.index('typedef struct {char bytes')]
prefix=r'''
#include "pet_services.h"
#include "pet_ndef.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef int SemaphoreHandle_t;
typedef int TaskHandle_t;
static int notifications;
#define xTaskNotifyGive(a) (++notifications)
typedef int nvs_handle_t;
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)(a))
#define xSemaphoreGive(a) ((void)(a))
#define ESP_OK 0
static int fail_write,fail_commit;static unsigned char durable[4096],staged[4096];static size_t staged_size;
static int nvs_set_blob(int h,const char *key,const void *data,size_t size){(void)h;(void)key;if(fail_write)return 1;assert(size<sizeof(staged));memcpy(staged,data,size);staged_size=size;return 0;}
static int nvs_commit(int h){(void)h;if(fail_commit)return 1;memcpy(durable,staged,staged_size);return 0;}
static void esp_fill_random(void *ptr,size_t n){static unsigned k=0;memset(ptr,++k,n);}
'''
main=r'''
static void reset(void){memset(&saved,0,sizeof(saved));memset(&view,0,sizeof(view));saved.version=1;mutex=1;fail_write=fail_commit=0;strcpy(config.base,"https://pet.example");memset(config.key,'a',64);config.key[64]=0;strcpy(saved.encounter,"11111111111111111111111111111111");view.introduction_ready=true;}
static void present(void){pet_cloud_arm();pet_cloud_card("https://pet.example/c/22222222222222222222222222222222");assert(view.card_stage==2);}
int main(void){
 reset();worker_task=1;view.introduction_ready=false;view.form_opened=true;pet_cloud_start();assert(request_start&&view.hello_starting&&!view.form_opened&&notifications==1);
 pet_cloud_cancel();assert(request_cancel&&!request_start);pet_cloud_start();assert(request_cancel&&request_start&&view.hello_starting&&!view.form_opened);
 request_start=request_cancel=false;
 reset();pet_cloud_card("https://pet.example/c/22222222222222222222222222222222");assert(view.card_stage==0);
 pet_cloud_card_error("Unreadable card");assert(!view.card_status[0]);
 pet_cloud_arm();pet_cloud_card_error("Unreadable card");assert(view.card_stage==1&&!strcmp(view.card_status,"Unreadable card")&&saved.count==0);
 present();pet_cloud_card_error("Other card");assert(view.card_stage==2&&!strcmp(view.card_id,"22222222222222222222222222222222"));
 pet_cloud_arm();pet_cloud_card("https://pet.example.evil/c/22222222222222222222222222222222");assert(view.card_stage==1);pet_cloud_card("https://pet.example/c/22222222222222222222222222222222?x");assert(view.card_stage==1);
 present();assert(!pet_cloud_accept());fail_write=1;pet_cloud_save();assert(saved.count==0&&view.card_stage==2);fail_write=0;fail_commit=1;pet_cloud_save();assert(saved.count==0&&view.card_stage==2);fail_commit=0;pet_cloud_save();assert(saved.count==1&&view.card_stage==3);saved_t recovered;memcpy(&recovered,durable,sizeof(recovered));assert(recovered.count==1&&!strcmp(recovered.pending[0].encounter,saved.encounter));assert(recovered.pending[0].event[0]);
 pet_cloud_save();assert(saved.count==1);present();pet_cloud_save();assert(saved.count==1);view.card_stage=3;assert(pet_cloud_accept());assert(!saved.encounter[0]&&saved.count==1);memcpy(&recovered,durable,sizeof(recovered));assert(!recovered.encounter[0]&&recovered.count==1);
 reset();saved.count=CAPACITY;present();pet_cloud_save();assert(saved.count==CAPACITY&&view.card_stage==2);
 reset();assert(!pet_cloud_configure("https://other.example","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));saved.encounter[0]=0;assert(!pet_cloud_configure("http://other.example","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));assert(!pet_cloud_configure("https://other.example/path","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));assert(pet_cloud_configure("https://other.example","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
 reset();present();pet_cloud_save();saved.pending[0].blocked=1;pet_cloud_arm();assert(saved.count==0&&view.card_stage==1);
 puts("PASS: exact card origin, explicit review/save, NVS failures, durable queue, duplicate guards, capacity, configuration guards and failed-card recovery");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'outbox.c';p.write_text(prefix+code+main);exe=Path(d)/'outbox'
 subprocess.run(['cc','-Wall','-Wno-unused-variable','-Wno-unused-function','-fsanitize=address,undefined','-Ifirmware/main',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
