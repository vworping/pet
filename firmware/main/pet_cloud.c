/* Public Hello transport. This worker alone performs HTTP; LVGL never waits on it. */
#include "pet_cloud.h"
#include "pet_protocol.h"
#include "pet_ndef.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#define CAPACITY 8
#define LOCK() xSemaphoreTake(mutex,portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(mutex)
typedef struct {char event[33],card[33],encounter[33];int blocked;} assignment_t;
typedef struct {char base[96],key[65];} config_t;
typedef struct {unsigned version,count;char encounter[33];assignment_t pending[CAPACITY];char new_card[33];} saved_t;
typedef struct {unsigned version,count;char encounter[33];assignment_t pending[CAPACITY];} saved_v1_t;
static SemaphoreHandle_t mutex;
static TaskHandle_t worker_task;
static nvs_handle_t nvs;
static config_t config;
static saved_t saved;
static pet_status_t view;
static char created_card_url[PET_NDEF_URL_MAX + 1];
static bool request_start,request_cancel,needs_create;
static char cancel_id[33];
static int64_t poll_at,sync_at,count_at;
static int64_t card_create_at;
static unsigned sync_failures;
static void wake_worker(void){if(worker_task)xTaskNotifyGive(worker_task);}
static void changed(void){view.revision++;}
static void opening(pet_hello_stage_t stage,const char *detail){
 if(view.hello_starting&&(view.hello_stage!=stage||strcmp(view.hello_error,detail))){
  view.hello_stage=stage;strlcpy(view.hello_error,detail,sizeof(view.hello_error));changed();
 }
}
static bool persist(void){return nvs_set_blob(nvs,"state",&saved,sizeof(saved))==ESP_OK&&nvs_commit(nvs)==ESP_OK;}
static void id(char out[33]){uint8_t b[16];esp_fill_random(b,16);for(int i=0;i<16;i++)snprintf(out+i*2,3,"%02x",b[i]);}
static bool valid_id(const char *s){if(strlen(s)!=32)return false;for(int i=0;i<32;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
static void message(const char *s){if(strcmp(view.card_status,s)){strlcpy(view.card_status,s,sizeof(view.card_status));changed();}}
void pet_cloud_snapshot(pet_status_t *out){
 if(!mutex){return;}
 LOCK();out->revision+=view.revision;out->hello_stage=view.hello_stage;out->hello_starting=view.hello_starting;out->hello_active=view.hello_active;out->introduction_ready=view.introduction_ready;out->form_opened=view.form_opened;out->phone_joined=view.form_opened;
 strlcpy(out->hello_url,view.hello_url,sizeof(out->hello_url));strlcpy(out->hello_error,view.hello_error,sizeof(out->hello_error));strlcpy(out->guest,view.guest,sizeof(out->guest));strlcpy(out->interest,view.interest,sizeof(out->interest));
 out->card_stage=view.card_stage;out->pending_cards=saved.count;strlcpy(out->card_status,view.card_status,sizeof(out->card_status));strlcpy(out->card_id,view.card_id,sizeof(out->card_id));
 strlcpy(out->service_status,!out->wifi_connected?"Hello needs Wi-Fi.":!config.base[0]?"Hello needs setup over USB.":view.service_status[0]?view.service_status:"Checking Hello service...",sizeof(out->service_status));UNLOCK();
}
void pet_cloud_start(void){LOCK();if(request_cancel||(!view.hello_starting&&!view.hello_active&&!view.introduction_ready)){request_start=true;view.hello_starting=true;view.hello_stage=PET_HELLO_LINK;view.hello_active=false;view.introduction_ready=false;view.form_opened=false;view.hello_error[0]=0;changed();}UNLOCK();wake_worker();}
void pet_cloud_cancel(void){LOCK();request_cancel=true;request_start=false;changed();UNLOCK();wake_worker();}
bool pet_cloud_accept(void){LOCK();bool ok=view.introduction_ready&&view.card_stage!=2;
 if(ok){saved_t backup=saved;saved.encounter[0]=0;saved.new_card[0]=0;if(!persist()){saved=backup;message("Could not save. Try again.");ok=false;}else{created_card_url[0]=0;view.introduction_ready=false;view.hello_active=false;view.hello_starting=false;view.card_stage=0;view.card_id[0]=0;changed();}}
 UNLOCK();return ok;
}
void pet_cloud_arm(void){LOCK();if(view.introduction_ready){
 for(unsigned i=0;i<saved.count;){if(saved.pending[i].blocked&&!strcmp(saved.pending[i].encounter,saved.encounter)){saved_t backup=saved;memmove(saved.pending+i,saved.pending+i+1,(saved.count-i-1)*sizeof(assignment_t));saved.count--;if(!persist()){saved=backup;message("Could not clear failed assignment. Try again.");UNLOCK();return;}}else i++;}
 saved_t backup=saved;
 if(!saved.new_card[0])id(saved.new_card);
 if(!persist()){saved=backup;message("Could not prepare a card page. Try again.");UNLOCK();return;}
 created_card_url[0]=0;card_create_at=0;view.card_stage=1;view.card_id[0]=0;message("Creating your card page. Hold a blank tag on PET.");}UNLOCK();}
void pet_cloud_clear_arm(void){LOCK();if(view.introduction_ready){view.card_stage=6;view.card_id[0]=0;message("Remove any card, then tap one writable NFC tag. Its link will be erased.");}UNLOCK();}
bool pet_cloud_clear_requested(void){if(!mutex)return false;LOCK();bool requested=view.introduction_ready&&view.card_stage==6;UNLOCK();return requested;}
void pet_cloud_clear_result(bool success){LOCK();if(view.introduction_ready&&view.card_stage==6){view.card_stage=7;message(success?"Tag cleared. Ready to set a business card.":"Could not clear this tag. It may be unsupported or locked.");}UNLOCK();}
void pet_cloud_card_error(const char *reason){LOCK();if(view.card_stage==1&&view.introduction_ready)message(reason);UNLOCK();}
bool pet_cloud_card_armed(void){if(!mutex)return false;LOCK();bool armed=view.introduction_ready&&view.card_stage==1;UNLOCK();return armed;}
bool pet_cloud_card_url(char *url,unsigned capacity){if(!url||!capacity||!mutex)return false;LOCK();bool ready=view.card_stage==1&&created_card_url[0];if(ready)strlcpy(url,created_card_url,capacity);UNLOCK();return ready;}
void pet_cloud_card(const char *url){LOCK();if(view.card_stage==1&&view.introduction_ready){size_t n=strlen(config.base);if(n&&strlen(url)==n+3+32&&!strncmp(url,config.base,n)&&!strncmp(url+n,"/c/",3)&&valid_id(url+n+3)){bool created=created_card_url[0]&&!strcmp(url,created_card_url);strlcpy(view.card_id,url+n+3,sizeof(view.card_id));view.card_stage=2;message(created?"Card link written. Save this conversation?":"Card found. Save this conversation to it?");}else message("Use a card prepared for this PET website.");}UNLOCK();}
void pet_cloud_save(void){LOCK();if(view.card_stage==2&&view.introduction_ready){
 if(saved.count>=CAPACITY){message("Queue full. Connect to sync cards first.");UNLOCK();return;}
 for(unsigned i=0;i<saved.count;i++)if(!strcmp(saved.pending[i].card,view.card_id)||!strcmp(saved.pending[i].encounter,saved.encounter)){message("This card or hello is already waiting to sync.");UNLOCK();return;}
 saved_t backup=saved;assignment_t *p=&saved.pending[saved.count++];memset(p,0,sizeof(*p));id(p->event);strlcpy(p->card,view.card_id,sizeof(p->card));strlcpy(p->encounter,saved.encounter,sizeof(p->encounter));if(!strcmp(saved.new_card,view.card_id))saved.new_card[0]=0;
 if(!persist()){saved=backup;message("Could not save. Keep the card and try again.");}else{created_card_url[0]=0;view.card_stage=3;message("Saved on PET. Waiting for internet confirmation.");sync_at=0;sync_failures=0;}}
 UNLOCK();}
bool pet_cloud_configure(const char *base,const char *key){
 size_t n=strlen(base);if(n<9||n>=sizeof(config.base)||strncmp(base,"https://",8)||strpbrk(base+8,"/?#@: \\ \t\r\n")||strlen(key)<32||strlen(key)>=sizeof(config.key))return false;
 LOCK();if(saved.count||saved.encounter[0]){UNLOCK();return false;}config_t next={0};strlcpy(next.base,base,sizeof(next.base));strlcpy(next.key,key,sizeof(next.key));bool ok=nvs_set_blob(nvs,"config",&next,sizeof(next))==ESP_OK&&nvs_commit(nvs)==ESP_OK;if(ok){config=next;view.hello_error[0]=0;changed();}UNLOCK();return ok;
}
typedef struct {char bytes[2048];size_t used;} reply_t;
/* Only the worker uses this client. Keeping it alive avoids a TLS handshake on
   every poll and on the next Hello. Failed connections are discarded. */
static esp_http_client_handle_t http_client;
static esp_err_t http_event(esp_http_client_event_t *e){if(e->event_id==HTTP_EVENT_ON_DATA){reply_t *r=e->user_data;if(e->data_len<0||r->used+(size_t)e->data_len>=sizeof(r->bytes))return ESP_FAIL;memcpy(r->bytes+r->used,e->data,e->data_len);r->used+=e->data_len;r->bytes[r->used]=0;}return ESP_OK;}
static cJSON *http(const char *path,cJSON *body,int *status){
 *status=0;
#ifdef CONFIG_MBEDTLS_HAVE_TIME_DATE
 if(time(NULL)<1700000000)return NULL;
#endif
 config_t cfg;LOCK();cfg=config;UNLOCK();if(!cfg.base[0])return NULL;
 char url[240];snprintf(url,sizeof(url),"%s%s",cfg.base,path);reply_t *reply=calloc(1,sizeof(*reply));if(!reply)return NULL;
 esp_http_client_config_t hc={.url=url,.timeout_ms=8000,.crt_bundle_attach=esp_crt_bundle_attach,.event_handler=http_event,.user_data=reply,.disable_auto_redirect=true,.buffer_size=1024,.keep_alive_enable=true};
 if(!http_client)http_client=esp_http_client_init(&hc);
 esp_http_client_handle_t client=http_client;if(!client){free(reply);return NULL;}
 if(esp_http_client_set_url(client,url)!=ESP_OK){esp_http_client_cleanup(client);http_client=NULL;free(reply);return NULL;}
 esp_http_client_set_user_data(client,reply);esp_http_client_set_method(client,body?HTTP_METHOD_POST:HTTP_METHOD_GET);
 esp_http_client_set_header(client,"X-PET-Device",cfg.key);esp_http_client_set_header(client,"Content-Type","application/json");memset(&cfg,0,sizeof(cfg));char *payload=body?cJSON_PrintUnformatted(body):NULL;
 if(body&&!payload){esp_http_client_set_user_data(client,NULL);free(reply);return NULL;}
 esp_http_client_set_post_field(client,payload,payload?strlen(payload):0);
 /* Modem sleep can delay each TLS/HTTP exchange until the next beacon.
    Keep the radio awake only for the request and restore the caller's mode. */
 wifi_ps_type_t prior_ps=WIFI_PS_NONE;
 bool restore_ps=esp_wifi_get_ps(&prior_ps)==ESP_OK&&prior_ps!=WIFI_PS_NONE&&esp_wifi_set_ps(WIFI_PS_NONE)==ESP_OK;
 esp_err_t err=esp_http_client_perform(client);if(err==ESP_OK)*status=esp_http_client_get_status_code(client);cJSON *j=err==ESP_OK?cJSON_Parse(reply->bytes):NULL;
 const char *detail=err!=ESP_OK?"Hello unreachable. Check venue sign-in or use a hotspot.":
  *status==401||*status==403?"Hello access needs setup over USB.":
  *status>=300&&*status<400?"Hello was redirected. Check venue sign-in or use a hotspot.":
  *status>=500?"Hello service unavailable. Retrying.":
  !j?"Unexpected Hello response. Check the network and retry.":"Hello service online.";
 LOCK();if(strcmp(view.service_status,detail)){strlcpy(view.service_status,detail,sizeof(view.service_status));changed();}UNLOCK();
 if(err!=ESP_OK)ESP_LOGW("pet_cloud","HTTPS failed: %s (errno=%d)",esp_err_to_name(err),esp_http_client_get_errno(client));
 if(restore_ps)esp_wifi_set_ps(prior_ps);
 esp_http_client_set_post_field(client,NULL,0);esp_http_client_set_user_data(client,NULL);
 if(err!=ESP_OK){esp_http_client_cleanup(client);http_client=NULL;}
 free(payload);free(reply);return j;
}
static cJSON *object(const char *key,const char *value){cJSON *j=cJSON_CreateObject();if(j)cJSON_AddStringToObject(j,key,value);return j;}
static bool interactive_requested(void){LOCK();bool requested=request_start||request_cancel;UNLOCK();return requested;}
static bool clock_running,network_was_connected;
static int64_t clock_retry_at;
static unsigned clock_server;
static bool prepare_clock(bool connected,int64_t now){
 if(!connected){network_was_connected=false;return false;}
 if(!network_was_connected){clock_retry_at=0;network_was_connected=true;}
 if(time(NULL)>=1700000000)return true;
 /* Starting SNTP before Wi-Fi causes failed DNS requests and retry backoff.
    Start on connection, then try another provider if the first is unreachable. */
 if(now>=clock_retry_at){
  static const char *servers[]={"time.cloudflare.com","time.google.com","pool.ntp.org"};
  esp_sntp_setservername(0,servers[clock_server++%3]);
  if(clock_running)esp_sntp_restart();else{esp_sntp_init();clock_running=true;}
  clock_retry_at=now+8000000;
 }
 /* The shipped SDK verifies CA chains and hostnames without date checks.
    Do not make otherwise-working HTTPS depend on venue UDP/123 access.
    Builds that enable certificate dates must still wait for real time. */
#ifdef CONFIG_MBEDTLS_HAVE_TIME_DATE
 return false;
#else
 return true;
#endif
}
static void worker(void *arg){(void)arg;
 for(;;){
  LOCK();
  if(request_cancel){request_cancel=false;strlcpy(cancel_id,saved.encounter,sizeof(cancel_id));saved.encounter[0]=0;persist();view.hello_active=false;view.hello_starting=false;view.introduction_ready=false;view.form_opened=false;view.card_stage=0;view.card_id[0]=0;view.hello_url[0]=0;needs_create=false;changed();}
  if(request_start){request_start=false;if(!config.base[0]){view.hello_starting=false;strlcpy(view.hello_error,"Public Hello needs setup over USB.",sizeof(view.hello_error));changed();}else{if(!saved.encounter[0])id(saved.encounter);if(!persist()){view.hello_starting=false;strlcpy(view.hello_error,"Could not save this Hello. Try again.",sizeof(view.hello_error));changed();}else{needs_create=true;view.form_opened=false;view.card_stage=0;view.card_id[0]=0;view.card_status[0]=0;poll_at=0;}}}
  char encounter[33];strlcpy(encounter,saved.encounter,sizeof(encounter));bool create=needs_create;bool poll=encounter[0]&&!view.introduction_ready;int64_t now=esp_timer_get_time();UNLOCK();
  pet_status_t network;pet_services_snapshot(&network);
  bool clock_ready=prepare_clock(network.wifi_connected,now);
  if(network.wifi_connected&&clock_ready){
   LOCK();if(view.hello_starting&&(view.hello_stage==PET_HELLO_WIFI||view.hello_stage==PET_HELLO_CLOCK))poll_at=0;UNLOCK();
   LOCK();char new_card[33];strlcpy(new_card,saved.new_card,sizeof(new_card));bool create_card=view.introduction_ready&&view.card_stage==1&&new_card[0]&&!created_card_url[0]&&esp_timer_get_time()>=card_create_at;UNLOCK();
   if(create_card){cJSON *b=object("id",new_card);int status;cJSON *r=http("/api/device/cards",b,&status);cJSON_Delete(b);LOCK();
    char expected[PET_NDEF_URL_MAX+1];snprintf(expected,sizeof(expected),"%s/c/%s",config.base,new_card);cJSON *created_url=r?cJSON_GetObjectItem(r,"url"):NULL;
    if(status==200&&r&&cJSON_IsString(cJSON_GetObjectItem(r,"id"))&&!strcmp(cJSON_GetObjectItem(r,"id")->valuestring,new_card)&&cJSON_IsString(created_url)&&!strcmp(created_url->valuestring,expected)){
     int n=snprintf(created_card_url,sizeof(created_card_url),"%s/c/%s",config.base,new_card);
     if(n>0&&(size_t)n<sizeof(created_card_url))message("Card page ready. Tap a blank writable tag.");
     else{created_card_url[0]=0;card_create_at=esp_timer_get_time()+3000000;message("Card address is too long. Check setup over USB.");}
    }else{card_create_at=esp_timer_get_time()+3000000;if(status==403)message("Card service needs setup over USB.");else if(status==404)message("Update the hosted card service to create pages.");else message("Waiting for Wi-Fi to create the card page.");}
    UNLOCK();cJSON_Delete(r);
   }
   /* New Hello creation and its live poll take priority over old-link cleanup
      and the durable card outbox, either of which can hit a network timeout. */
   if(interactive_requested())continue;
   if(poll&&now>=poll_at){
    LOCK();opening(PET_HELLO_LINK,"Creating your introduction link.");UNLOCK();
    int status;char path[100];cJSON *b=create?object("id",encounter):NULL;snprintf(path,sizeof(path),create?"/api/device/encounters":"/api/device/encounters/%s",encounter);
    int64_t request_at=esp_timer_get_time();cJSON *r=http(path,b,&status);cJSON_Delete(b);
    if(create||status!=200)ESP_LOGI("pet_cloud","Hello %s HTTP=%d elapsed=%u ms",create?"create":"poll",status,(unsigned)((esp_timer_get_time()-request_at)/1000));
    LOCK();if(!strcmp(encounter,saved.encounter)&&!request_cancel){
     if(status==200&&r){
      if(create){cJSON *url=cJSON_GetObjectItem(r,"url");if(cJSON_IsString(url)&&strlen(url->valuestring)<sizeof(view.hello_url)){strlcpy(view.hello_url,url->valuestring,sizeof(view.hello_url));view.hello_starting=false;view.hello_stage=PET_HELLO_READY;view.hello_active=true;needs_create=false;view.hello_error[0]=0;changed();}}
      else{view.form_opened=cJSON_IsTrue(cJSON_GetObjectItem(r,"opened"));if(cJSON_IsTrue(cJSON_GetObjectItem(r,"expired"))){view.hello_active=false;view.hello_starting=false;saved.encounter[0]=0;persist();strlcpy(view.hello_error,"This Hello expired. Start a fresh one.",sizeof(view.hello_error));}
       if(cJSON_IsTrue(cJSON_GetObjectItem(r,"submitted"))){cJSON *name=cJSON_GetObjectItem(r,"name"),*topic=cJSON_GetObjectItem(r,"topic");if(cJSON_IsString(name)&&cJSON_IsString(topic)&&pet_text_valid(name->valuestring,40)&&pet_text_valid(topic->valuestring,80)){strlcpy(view.guest,name->valuestring,sizeof(view.guest));strlcpy(view.interest,topic->valuestring,sizeof(view.interest));view.introduction_ready=true;view.hello_starting=false;view.hello_active=false;message("Add this conversation to a business card.");cJSON *assigned=cJSON_GetObjectItem(r,"card_id");if(cJSON_IsString(assigned)&&valid_id(assigned->valuestring)){strlcpy(view.card_id,assigned->valuestring,sizeof(view.card_id));view.card_stage=4;message("Updated. This card is ready to give.");}for(unsigned k=0;k<saved.count;k++)if(!strcmp(saved.pending[k].encounter,encounter)){view.card_stage=saved.pending[k].blocked?5:3;strlcpy(view.card_id,saved.pending[k].card,sizeof(view.card_id));message(saved.pending[k].blocked?"Card needs attention in owner dashboard.":"Saved on PET. Waiting for internet confirmation.");}}}changed();}
     }else if(status==410){view.hello_active=false;view.hello_starting=false;saved.encounter[0]=0;persist();strlcpy(view.hello_error,"This Hello ended. Start a fresh one.",sizeof(view.hello_error));changed();}
     else{view.hello_stage=PET_HELLO_RETRY;strlcpy(view.hello_error,view.service_status[0]?view.service_status:"Hello unavailable. Check the network and retry.",sizeof(view.hello_error));changed();}
    }poll_at=esp_timer_get_time()+(create&&status==200?250000:1500000);UNLOCK();cJSON_Delete(r);
   }
   if(interactive_requested())continue;
   if(!poll&&cancel_id[0]){char path[100];snprintf(path,sizeof(path),"/api/device/encounters/%s/close",cancel_id);int status;cJSON *b=cJSON_CreateObject(),*r=http(path,b,&status);cJSON_Delete(b);cJSON_Delete(r);if(status==200||status==410)cancel_id[0]=0;}
   if(interactive_requested())continue;
   LOCK();assignment_t item={0};if(!poll&&now>=sync_at){for(unsigned i=0;i<saved.count;i++)if(!saved.pending[i].blocked){item=saved.pending[i];break;}}UNLOCK();
   if(item.event[0]){cJSON *b=object("event_id",item.event);cJSON_AddStringToObject(b,"card_id",item.card);cJSON_AddStringToObject(b,"encounter_id",item.encounter);int status;cJSON *r=http("/api/device/assign",b,&status);cJSON_Delete(b);
    LOCK();for(unsigned i=0;i<saved.count;i++)if(!strcmp(saved.pending[i].event,item.event)){
     if(status==200&&cJSON_IsTrue(cJSON_GetObjectItem(r,"ok"))){saved_t backup=saved;memmove(saved.pending+i,saved.pending+i+1,(saved.count-i-1)*sizeof(item));saved.count--;if(!persist()){saved=backup;}else{sync_failures=0;if(!strcmp(view.card_id,item.card)){view.card_stage=4;message("Updated. This card is ready to give.");}changed();}}
     else if(status==409||status==404||status==400){saved.pending[i].blocked=1;persist();if(!strcmp(saved.encounter,item.encounter)){view.card_stage=5;message("Card unavailable or already used. Choose another card.");}else message("A pending card needs attention in your records.");}
     else{sync_failures=sync_failures<5?sync_failures+1:5;}break;
    }sync_at=esp_timer_get_time()+(int64_t)(2u<<sync_failures)*1000000;UNLOCK();cJSON_Delete(r);
   }
   /* Refresh saved Hellos while idle, without delaying an active introduction. */
   if(interactive_requested())continue;
   if(!poll&&now>=count_at){
    int status;cJSON *r=http("/api/device/stats",NULL,&status);
    cJSON *count=r?cJSON_GetObjectItem(r,"connections"):NULL;
    if(status==200&&cJSON_IsNumber(count)&&count->valuedouble>=0&&count->valuedouble<=UINT32_MAX&&count->valuedouble==(uint32_t)count->valuedouble)
     pet_services_set_connections((uint32_t)count->valuedouble);
    cJSON_Delete(r);count_at=esp_timer_get_time()+5000000;
   }
  }else if(poll){LOCK();opening(network.wifi_connected?PET_HELLO_CLOCK:PET_HELLO_WIFI,network.wifi_connected?"Setting the clock for a secure connection.":"Connect PET to Wi-Fi to open Hello.");UNLOCK();}
  ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(250));
 }
}
void pet_cloud_init(void){mutex=xSemaphoreCreateMutex();assert(mutex);ESP_ERROR_CHECK(nvs_open("pet_cloud",NVS_READWRITE,&nvs));size_t n=sizeof(config);if(nvs_get_blob(nvs,"config",&config,&n)!=ESP_OK||n!=sizeof(config))memset(&config,0,sizeof(config));n=sizeof(saved);if(nvs_get_blob(nvs,"state",&saved,&n)!=ESP_OK||n!=sizeof(saved)||saved.version!=2||saved.count>CAPACITY){saved_v1_t old={0};size_t old_size=sizeof(old);if(nvs_get_blob(nvs,"state",&old,&old_size)==ESP_OK&&old_size==sizeof(old)&&old.version==1&&old.count<=CAPACITY){memset(&saved,0,sizeof(saved));saved.version=2;saved.count=old.count;strlcpy(saved.encounter,old.encounter,sizeof(saved.encounter));memcpy(saved.pending,old.pending,sizeof(saved.pending));}else memset(&saved,0,sizeof(saved));}saved.version=2;nvs_set_blob(nvs,"state",&saved,sizeof(saved));nvs_commit(nvs);
 if(saved.encounter[0]){view.hello_starting=true;needs_create=true;}
 esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
 configASSERT(xTaskCreate(worker,"pet_cloud",8192,NULL,3,&worker_task)==pdPASS);
}
