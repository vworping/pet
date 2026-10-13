"""Exercise the real HTTP wrapper with a fake ESP client and real JSON parser."""
from pathlib import Path
import os
import subprocess
import tempfile

source = Path('firmware/main/pet_cloud.c').read_text()
transport = source[source.index('typedef struct {char bytes'):source.index('static cJSON *object')]
cjson = Path(os.environ.get('PET_IDF_PATH', '/private/tmp/pet-v4-sdk')) / 'components/json/cJSON'
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
#define ESP_OK 0
#define ESP_LOGW(...) ((void)0)
static struct {char service_status[80];} view;
static void changed(void){}
static time_t clock_time=0;
static time_t test_time(time_t *out){if(out)*out=clock_time;return clock_time;}
#define time test_time
#define ESP_FAIL -1
#define HTTP_EVENT_ON_DATA 1
#define HTTP_METHOD_POST 1
#define HTTP_METHOD_GET 0
typedef int wifi_ps_type_t;
#define WIFI_PS_NONE 0
static wifi_ps_type_t power_save=1;
static bool fail_power_get;
static int esp_wifi_get_ps(wifi_ps_type_t *mode){*mode=power_save;return fail_power_get?ESP_FAIL:ESP_OK;}
static int esp_wifi_set_ps(wifi_ps_type_t mode){power_save=mode;return ESP_OK;}
typedef int esp_err_t;
typedef struct {int event_id,data_len;void *user_data;char *data;} esp_http_client_event_t;
static void esp_crt_bundle_attach(void){}
typedef struct {
 const char *url;int timeout_ms,buffer_size;bool disable_auto_redirect,keep_alive_enable;
 void (*crt_bundle_attach)(void);esp_err_t (*event_handler)(esp_http_client_event_t *);void *user_data;
} esp_http_client_config_t;
typedef struct {
 esp_http_client_config_t config;char url[240];int method,length;const char *body;void *user_data;
} mock_client_t;
typedef mock_client_t *esp_http_client_handle_t;
static unsigned inits,cleanups,requests;
static bool fail_request,fail_url;
static int response_status=200;
static const char *expected_path,*expected_body;
static int expected_method;
static struct {char base[96],key[65];} config={"https://pet.example","test-device-key"};
typedef typeof(config) config_t;
static esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg){
 assert(cfg->crt_bundle_attach&&cfg->disable_auto_redirect&&cfg->keep_alive_enable);
 assert(cfg->timeout_ms==8000);inits++;mock_client_t *c=calloc(1,sizeof(*c));c->config=*cfg;return c;
}
static void esp_http_client_cleanup(esp_http_client_handle_t c){cleanups++;free(c);}
static int esp_http_client_set_url(esp_http_client_handle_t c,const char *url){strlcpy(c->url,url,sizeof(c->url));return fail_url?ESP_FAIL:ESP_OK;}
static void esp_http_client_set_user_data(esp_http_client_handle_t c,void *data){c->user_data=data;}
static void esp_http_client_set_method(esp_http_client_handle_t c,int method){c->method=method;}
static void esp_http_client_set_header(esp_http_client_handle_t c,const char *key,const char *value){
 (void)c;if(!strcmp(key,"X-PET-Device"))assert(!strcmp(value,"test-device-key"));
}
static void esp_http_client_set_post_field(esp_http_client_handle_t c,const char *data,int n){c->body=data;c->length=n;}
static int esp_http_client_get_status_code(esp_http_client_handle_t c){(void)c;return response_status;}
static int esp_http_client_perform(esp_http_client_handle_t c){
 assert(fail_power_get||power_save==WIFI_PS_NONE);
 requests++;assert(c->method==expected_method);assert(!strcmp(c->url+strlen(config.base),expected_path));
 if(expected_body){assert(c->body&&c->length==(int)strlen(expected_body)&&!strcmp(c->body,expected_body));}
 else assert(!c->body&&!c->length);
 if(fail_request)return ESP_FAIL;
 esp_http_client_event_t e={HTTP_EVENT_ON_DATA,6,c->user_data,"{\"ok\":"};assert(c->config.event_handler(&e)==ESP_OK);
 e.data="true}";e.data_len=5;assert(c->config.event_handler(&e)==ESP_OK);return ESP_OK;
}
'''
main = r'''
int main(void){
 #ifdef CONFIG_MBEDTLS_HAVE_TIME_DATE
 int blocked_status=99;assert(!http("/api/device/stats",NULL,&blocked_status)&&blocked_status==0&&requests==0);
 clock_time=1700000000;
#endif
 cJSON *body=cJSON_CreateObject();cJSON_AddStringToObject(body,"id","example");int status;
 expected_path="/api/device/encounters";expected_body="{\"id\":\"example\"}";expected_method=HTTP_METHOD_POST;
 cJSON *r=http(expected_path,body,&status);assert(status==200&&cJSON_IsTrue(cJSON_GetObjectItem(r,"ok")));cJSON_Delete(r);
 assert(power_save==1);assert(!strcmp(view.service_status,"Hello service online."));
 assert(inits==1&&cleanups==0&&http_client&&!http_client->body&&!http_client->user_data);
 expected_path="/api/device/encounters/example";expected_body=NULL;expected_method=HTTP_METHOD_GET;
 r=http(expected_path,NULL,&status);assert(status==200&&r);cJSON_Delete(r);assert(inits==1);
 expected_path="/api/device/cards";expected_body="{\"id\":\"example\"}";expected_method=HTTP_METHOD_POST;
 r=http(expected_path,body,&status);assert(status==200&&r);cJSON_Delete(r);assert(inits==1);
 fail_request=true;r=http(expected_path,body,&status);assert(!r&&!status&&!http_client&&cleanups==1&&power_save==1);assert(strstr(view.service_status,"hotspot"));
 fail_request=false;r=http(expected_path,body,&status);assert(r&&status==200&&inits==2);cJSON_Delete(r);
 fail_url=true;r=http(expected_path,body,&status);assert(!r&&!status&&!http_client&&cleanups==2&&requests==5);
 fail_url=false;fail_power_get=true;r=http(expected_path,body,&status);assert(r&&power_save==1);cJSON_Delete(r);
 response_status=302;r=http(expected_path,body,&status);assert(status==302&&strstr(view.service_status,"redirected"));cJSON_Delete(r);
 response_status=403;r=http(expected_path,body,&status);assert(status==403&&strstr(view.service_status,"USB"));cJSON_Delete(r);
 response_status=503;r=http(expected_path,body,&status);assert(status==503&&strstr(view.service_status,"unavailable"));cJSON_Delete(r);
 esp_http_client_cleanup(http_client);http_client=NULL;
 cJSON_Delete(body);
 reply_t reply={0};char oversized[2048]={0};esp_http_client_event_t e={HTTP_EVENT_ON_DATA,sizeof(oversized),&reply,oversized};
 assert(http_event(&e)==ESP_FAIL&&!reply.used);
 puts("PASS: HTTPS reuse, POST/GET reset, response chunks, timeout recovery, Wi-Fi power restoration and response bounds");
}
'''
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'transport.c'
    test.write_text(prefix + transport + main)
    binary = Path(directory) / 'transport'
    for flags in ([], ['-DCONFIG_MBEDTLS_HAVE_TIME_DATE=1']):
        subprocess.run(['cc', '-Wall', '-fsanitize=address,undefined', *flags, '-I' + str(cjson),
                        str(test), str(cjson / 'cJSON.c'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
