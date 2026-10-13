"""Run the actual cloud loop one iteration at a time with deterministic I/O."""
from pathlib import Path
import os
import subprocess
import tempfile

source = Path('firmware/main/pet_cloud.c').read_text()
state = source[source.index('#define CAPACITY'):source.index('typedef struct {char bytes')]
worker = source[source.index('static cJSON *object'):source.index('void pet_cloud_init')]
cjson = Path(os.environ.get('PET_IDF_PATH', '/private/tmp/pet-v4-sdk')) / 'components/json/cJSON'
prefix = r'''
#include "pet_services.h"
#include "pet_ndef.h"
#include "pet_protocol.h"
#include "cJSON.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static time_t clock_time=1700000000;
static time_t test_time(time_t *out){if(out)*out=clock_time;return clock_time;}
#define time test_time
#define ESP_LOGI(...) ((void)0)
static unsigned sntp_starts,sntp_restarts,sntp_servers;
static void esp_sntp_init(void){sntp_starts++;}
static void esp_sntp_restart(void){sntp_restarts++;}
static void esp_sntp_setservername(int index,const char *name){assert(index==0&&name[0]);sntp_servers++;}
typedef int SemaphoreHandle_t;
typedef int TaskHandle_t;
typedef int nvs_handle_t;
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)(a))
#define xSemaphoreGive(a) ((void)(a))
#define xTaskNotifyGive(a) ((void)(a))
#define ESP_OK 0
#define pdTRUE 1
#define pdMS_TO_TICKS(a) (a)
static jmp_buf paused;
static int nvs_set_blob(int h,const char *key,const void *data,size_t n){return 0;}
static int nvs_commit(int h){return 0;}
static void esp_fill_random(void *data,size_t n){memset(data,0xab,n);}
static int64_t esp_timer_get_time(void){return 1000000;}
static void ulTaskNotifyTake(int clear,int delay){assert(delay==250);longjmp(paused,1);}
static uint32_t synced_count;
void pet_services_set_connections(uint32_t count){synced_count=count;}
void pet_services_snapshot(pet_status_t *out){memset(out,0,sizeof(*out));out->wifi_connected=true;}
'''
http = r'''
static unsigned creates,polls,closes,assigns;
static bool cancel_during_create;
static const char *stats_reply="{\"connections\":0}";
static int stats_status=200;
static cJSON *http(const char *path,cJSON *body,int *status){
 *status=200;
 if(!strcmp(path,"/api/device/stats")){*status=stats_status;return cJSON_Parse(stats_reply);}
 if(!strcmp(path,"/api/device/encounters")){
  creates++;assert(body);if(cancel_during_create){cancel_during_create=false;pet_cloud_cancel();}
  cJSON *r=cJSON_CreateObject();char url[128];snprintf(url,sizeof(url),"%s/h/%s",config.base,saved.encounter);cJSON_AddStringToObject(r,"url",url);return r;
 }
 if(strstr(path,"/close")){closes++;return cJSON_Parse("{\"ok\":true}");}
 if(!strcmp(path,"/api/device/assign")){assigns++;return cJSON_Parse("{\"ok\":true}");}
 polls++;assert(!body);return cJSON_Parse("{\"opened\":true}");
}
'''
main = r'''
static void run(void){if(!setjmp(paused))worker(NULL);}
static void reset(void){
 memset(&view,0,sizeof(view));memset(&saved,0,sizeof(saved));memset(&config,0,sizeof(config));
 strcpy(config.base,"https://pet.example");saved.version=2;mutex=worker_task=1;
 request_start=request_cancel=needs_create=false;poll_at=sync_at=count_at=0;cancel_id[0]=0;creates=polls=closes=assigns=0;
 strcpy(saved.encounter,"11111111111111111111111111111111");saved.count=1;
 strcpy(saved.pending[0].event,"22222222222222222222222222222222");
}
int main(void){
 clock_time=0;assert(!prepare_clock(false,1000000)&&!sntp_starts);
 assert(!prepare_clock(true,1000000)&&sntp_starts==1&&sntp_servers==1);
 assert(!prepare_clock(true,2000000)&&!sntp_restarts);
 assert(!prepare_clock(true,9000000)&&sntp_restarts==1&&sntp_servers==2);
 assert(!prepare_clock(false,10000000));assert(!prepare_clock(true,11000000)&&sntp_restarts==2);
 clock_time=1700000000;assert(prepare_clock(true,19000000)&&sntp_servers==3);
 reset();saved.encounter[0]=0;clock_time=0;pet_cloud_start();run();assert(!creates&&view.hello_starting&&view.hello_stage==PET_HELLO_CLOCK);
 clock_time=1700000000;poll_at=99999999;run();assert(creates==1&&view.hello_active&&view.hello_stage==PET_HELLO_READY);
 reset();pet_cloud_cancel();pet_cloud_start();run();
 assert(creates==1&&!closes&&!assigns&&view.hello_active&&!view.hello_starting);
 assert(strcmp(saved.encounter,"11111111111111111111111111111111")&&cancel_id[0]);
 poll_at=0;run();assert(polls==1&&view.form_opened&&!assigns&&!closes);
 view.introduction_ready=true;run();assert(closes==1&&assigns==1&&!saved.count&&!cancel_id[0]);
 reset();saved.encounter[0]=0;pet_cloud_start();cancel_during_create=true;run();
 assert(creates==1&&!view.hello_active&&!view.hello_starting&&!saved.encounter[0]);
 reset();saved.encounter[0]=0;saved.count=0;synced_count=14;run();assert(synced_count==0);
 stats_reply="{\"connections\":3}";count_at=0;run();assert(synced_count==3);
 stats_reply="{\"connections\":0}";stats_status=500;count_at=0;run();assert(synced_count==3);
 stats_status=200;stats_reply="{\"connections\":-1}";count_at=0;run();assert(synced_count==3);
 stats_reply="{}";count_at=0;run();assert(synced_count==3);
 puts("PASS: clock starts after Wi-Fi, fallback/reconnect sync, immediate link after sync, cancel/reopen, polling priority and HTTP cancellation");
}
'''
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'scheduling.c'
    test.write_text(prefix + state + http + worker + main)
    binary = Path(directory) / 'scheduling'
    subprocess.run(['cc', '-Wall', '-Wno-deprecated-declarations', '-fsanitize=address,undefined',
                    '-Ifirmware/main', '-I' + str(cjson), str(test),
                    'firmware/main/pet_protocol.c', str(cjson / 'cJSON.c'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
