#include "pet_services.h"
#include "pet_cloud.h"
#include "pet_portal.h"
#include "pet_protocol.h"
#include "pet_socket.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "pet_enterprise.h"
#ifdef PET_ENTERPRISE_CA
extern const unsigned char enterprise_ca_start[] asm("_binary_enterprise_ca_pem_start");
extern const unsigned char enterprise_ca_end[] asm("_binary_enterprise_ca_pem_end");
#endif
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"

static pet_status_t state;
static SemaphoreHandle_t lock;
static QueueHandle_t commands;
static esp_netif_t *station,*access_point;
static nvs_handle_t store;
static bool wifi_started, want_wifi, ap_enabled;
static bool reconnect_after_scan;
static int retries;
static int64_t reconnect_at;
static int64_t deadline;
static char token[33];
static const char *TAG="pet_services";
typedef struct { int type; char ssid[33],password[65],username[128]; } command_t;
enum { SCAN, CONNECT, CONNECT_ENTERPRISE, WIFI_OFF, FORGET, HELLO_START, HELLO_STOP };
#define LOCK() xSemaphoreTake(lock,portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(lock)
static void changed(void){state.revision++;}
void pet_services_snapshot(pet_status_t *out){LOCK();*out=state;UNLOCK();pet_cloud_snapshot(out);}
static bool send_command(int type,const char *ssid,const char *password){
 command_t cmd={.type=type};if(ssid)strlcpy(cmd.ssid,ssid,sizeof(cmd.ssid));if(password)strlcpy(cmd.password,password,sizeof(cmd.password));
 bool queued=xQueueSend(commands,&cmd,0)==pdTRUE;
 if(!queued){LOCK();strlcpy(state.wifi_status,"Busy; try again",sizeof(state.wifi_status));changed();UNLOCK();}
 memset(&cmd,0,sizeof(cmd));return queued;
}
void pet_services_scan(void){send_command(SCAN,NULL,NULL);}
void pet_services_connect(const char *s,const char *p){send_command(CONNECT,s,p);}
void pet_services_connect_enterprise(const char *s,const char *u,const char *p){
 command_t cmd={.type=CONNECT_ENTERPRISE};strlcpy(cmd.ssid,s,sizeof(cmd.ssid));strlcpy(cmd.username,u,sizeof(cmd.username));strlcpy(cmd.password,p,sizeof(cmd.password));
 if(xQueueSend(commands,&cmd,0)!=pdTRUE){LOCK();strlcpy(state.wifi_status,"Busy; try again",sizeof(state.wifi_status));changed();UNLOCK();}memset(&cmd,0,sizeof(cmd));
}
void pet_services_wifi_off(void){send_command(WIFI_OFF,NULL,NULL);}
void pet_services_forget_wifi(void){send_command(FORGET,NULL,NULL);}
void pet_services_hello_start(void){pet_cloud_start();}
void pet_services_hello_cancel(void){pet_cloud_cancel();}
void pet_services_card_arm(void){pet_cloud_arm();}
void pet_services_card_save(void){pet_cloud_save();}
void pet_services_card_clear(void){pet_cloud_clear_arm();}
void pet_services_set_brightness(int v){LOCK();state.brightness=v<20?20:v>100?100:v;changed();nvs_set_u8(store,"brightness",state.brightness);nvs_commit(store);UNLOCK();}
void pet_services_set_motion(bool on){LOCK();state.motion=on;changed();nvs_set_u8(store,"motion",on);nvs_commit(store);UNLOCK();}
void pet_services_set_saver(bool on){
 LOCK();state.save_power=on;changed();bool active=wifi_started;UNLOCK();
 if(active)esp_wifi_set_ps(on?WIFI_PS_MAX_MODEM:WIFI_PS_MIN_MODEM);
}
void pet_services_sleep(bool asleep){LOCK();state.asleep=asleep;changed();UNLOCK();}
bool pet_services_accept(void){return pet_cloud_accept();}
void pet_services_set_connections(uint32_t count){
 LOCK();if(state.connections!=count){state.connections=count;nvs_set_u32(store,"connections",count);nvs_commit(store);changed();}UNLOCK();
}
static void random_hex(char *out,int bytes){static const char hex[]="0123456789abcdef";uint8_t b[16];esp_fill_random(b,bytes);for(int i=0;i<bytes;i++){out[i*2]=hex[b[i]>>4];out[i*2+1]=hex[b[i]&15];}out[bytes*2]=0;}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data){
 (void)arg;
 if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){
  wifi_ap_record_t ap={0};bool have_ap=esp_wifi_sta_get_ap_info(&ap)==ESP_OK;
  ip_event_got_ip_t *ip=data;LOCK();state.wifi_channel=have_ap?ap.primary:0;state.wifi_rssi=have_ap?ap.rssi:0;state.wifi_connected=true;snprintf(state.ip,sizeof(state.ip),IPSTR,IP2STR(&ip->ip_info.ip));
  strlcpy(state.wifi_status,"Connected",sizeof(state.wifi_status));changed();UNLOCK();retries=0;ESP_LOGI(TAG,"Wi-Fi acquired an address");
 }else if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){
  LOCK();state.wifi_connected=false;state.ip[0]=0;strlcpy(state.wifi_status,want_wifi?"Connection lost; retrying":"Wi-Fi off",sizeof(state.wifi_status));changed();UNLOCK();
  if(want_wifi&&retries++<5)esp_wifi_connect();else{LOCK();strlcpy(state.wifi_status,want_wifi?"Waiting for Wi-Fi; retrying":"Wi-Fi off",sizeof(state.wifi_status));changed();UNLOCK();}
 }else if(base==WIFI_EVENT&&id==WIFI_EVENT_AP_START){
  esp_netif_dhcp_status_t dhcp;esp_netif_dhcps_get_status(access_point,&dhcp);
  ESP_LOGI(TAG,"Hello AP started; DHCP %s",dhcp==ESP_NETIF_DHCP_STARTED?"running":"not running");
  ESP_LOGI(TAG,"Wi-Fi event stack headroom=%u bytes",(unsigned)uxTaskGetStackHighWaterMark(NULL));
 }else if(base==IP_EVENT&&id==IP_EVENT_AP_STAIPASSIGNED){
  LOCK();if(state.hello_active){state.phone_joined=true;changed();}UNLOCK();
  ESP_LOGI(TAG,"Hello phone received an address");
 }else if(base==WIFI_EVENT&&id==WIFI_EVENT_AP_STADISCONNECTED){
  LOCK();state.phone_joined=false;state.form_opened=false;changed();UNLOCK();
 }else if(base==WIFI_EVENT&&id==WIFI_EVENT_SCAN_DONE){
  /* Only this serialized event loop uses the scan buffer. A stack allocation
     reserves it for every event, including AP startup and DHCP callbacks. */
  static wifi_ap_record_t aps[PET_NETWORKS];
  uint16_t n=PET_NETWORKS;esp_err_t err=esp_wifi_scan_get_ap_records(&n,aps);
  LOCK();state.scanning=false;state.network_count=err==ESP_OK?n:0;
  for(int i=0;i<state.network_count;i++){strlcpy(state.networks[i].ssid,(char*)aps[i].ssid,33);state.networks[i].rssi=aps[i].rssi;state.networks[i].locked=aps[i].authmode!=WIFI_AUTH_OPEN;state.networks[i].enterprise=aps[i].authmode==WIFI_AUTH_WPA2_ENTERPRISE||aps[i].authmode==WIFI_AUTH_WPA3_ENT_192;}
  changed();UNLOCK();
  if(reconnect_after_scan){reconnect_after_scan=false;want_wifi=true;retries=6;reconnect_at=esp_timer_get_time()+5000000;}
 }
}
static void start_radio(bool ap){
 esp_wifi_set_mode(ap?WIFI_MODE_APSTA:WIFI_MODE_STA);
 if(!wifi_started){esp_wifi_start();wifi_started=true;}
 ap_enabled=ap;
}
static void start_hello(void){
 LOCK();if(state.hello_active||state.introduction_ready){UNLOCK();return;}state.phone_joined=false;state.form_opened=false;state.introduction_ready=false;state.guest[0]=0;state.interest[0]=0;state.hello_error[0]=0;
 random_hex(token,16);random_hex(state.ap_password,6);deadline=esp_timer_get_time()+300000000;
 snprintf(state.hello_url,sizeof(state.hello_url),"http://192.168.4.1/hello?t=%s",token);UNLOCK();
 /* A direct phone link works even on venue networks that isolate their clients. */
 wifi_config_t ap={0};LOCK();strlcpy((char*)ap.ap.ssid,state.ap_ssid,sizeof(ap.ap.ssid));strlcpy((char*)ap.ap.password,state.ap_password,sizeof(ap.ap.password));UNLOCK();
 ap.ap.ssid_len=strlen((char*)ap.ap.ssid);ap.ap.max_connection=1;ap.ap.authmode=WIFI_AUTH_WPA2_PSK;ap.ap.channel=6;
 esp_err_t err=esp_wifi_set_mode(WIFI_MODE_APSTA);
 if(err==ESP_OK)err=esp_wifi_set_storage(WIFI_STORAGE_RAM);
 if(err==ESP_OK)err=esp_wifi_set_config(WIFI_IF_AP,&ap);
 esp_wifi_set_storage(WIFI_STORAGE_FLASH);
 /* Explicitly re-arm DHCP; a stopped server does not restart with AP_START. */
 if(err==ESP_OK){esp_err_t dhcp=esp_netif_dhcps_start(access_point);if(dhcp!=ESP_OK&&dhcp!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)err=dhcp;}
 if(err==ESP_OK&&!wifi_started){err=esp_wifi_start();if(err==ESP_OK)wifi_started=true;}
 LOCK();state.hello_starting=false;state.hello_active=err==ESP_OK;ap_enabled=err==ESP_OK;
 if(err!=ESP_OK){token[0]=0;state.hello_url[0]=0;strlcpy(state.hello_error,"Could not open the phone link. Try again.",sizeof(state.hello_error));}
 changed();UNLOCK();
 if(err!=ESP_OK){esp_wifi_set_mode(WIFI_MODE_STA);ESP_LOGE(TAG,"Hello access point failed: %s",esp_err_to_name(err));}
}
static void close_hello(bool keep_intro){
 LOCK();state.hello_starting=false;state.hello_active=false;if(!keep_intro)state.introduction_ready=false;state.phone_joined=false;state.form_opened=false;token[0]=0;if(!keep_intro){state.guest[0]=0;state.interest[0]=0;}state.hello_url[0]=0;changed();UNLOCK();
 if(ap_enabled){esp_wifi_set_mode(WIFI_MODE_STA);ap_enabled=false;if(!want_wifi){esp_wifi_stop();wifi_started=false;}}
}
static void stop_hello(void){close_hello(false);}
static void worker(void *arg){
 (void)arg;command_t cmd;
 for(;;){
  if(xQueueReceive(commands,&cmd,pdMS_TO_TICKS(500))){
   if(cmd.type==SCAN){
    LOCK();bool connected=state.wifi_connected;UNLOCK();
    /* A saved, unreachable network must not starve the network picker. */
    if(want_wifi&&!connected){reconnect_after_scan=true;want_wifi=false;esp_wifi_disconnect();vTaskDelay(pdMS_TO_TICKS(150));}
    start_radio(ap_enabled);LOCK();state.scanning=true;changed();UNLOCK();
    if(esp_wifi_scan_start(NULL,false)!=ESP_OK){
     LOCK();state.scanning=false;strlcpy(state.wifi_status,"Scan could not start. Try again.",sizeof(state.wifi_status));changed();UNLOCK();
     if(reconnect_after_scan){reconnect_after_scan=false;want_wifi=true;retries=6;reconnect_at=esp_timer_get_time()+5000000;}
    }
   }else if(cmd.type==CONNECT||cmd.type==CONNECT_ENTERPRISE){
    bool enterprise=cmd.type==CONNECT_ENTERPRISE;
    esp_err_t err=ESP_OK;
#if !defined(PET_ENTERPRISE_CA) && !defined(CONFIG_PET_UMB_EDUROAM)
    if(enterprise){LOCK();strlcpy(state.wifi_status,"Enterprise needs your school's CA setup",sizeof(state.wifi_status));changed();UNLOCK();memset(&cmd,0,sizeof(cmd));continue;}
#endif
    reconnect_after_scan=false;want_wifi=false;esp_wifi_scan_stop();esp_wifi_disconnect();vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_sta_enterprise_disable();esp_eap_client_clear_identity();esp_eap_client_clear_username();esp_eap_client_clear_password();
#if defined(PET_ENTERPRISE_CA) || defined(CONFIG_PET_UMB_EDUROAM)
    if(enterprise){
#ifdef CONFIG_PET_UMB_EDUROAM
     err=pet_enterprise_use_umb_trust();
#else
     err=esp_eap_client_set_ca_cert(enterprise_ca_start,enterprise_ca_end-enterprise_ca_start);
     if(err==ESP_OK)err=esp_eap_client_set_domain_name(CONFIG_PET_ENTERPRISE_DOMAIN);
#endif
     if(err==ESP_OK)err=esp_eap_client_set_identity((const unsigned char*)cmd.username,strlen(cmd.username));
     if(err==ESP_OK)err=esp_eap_client_set_username((const unsigned char*)cmd.username,strlen(cmd.username));
     if(err==ESP_OK)err=esp_eap_client_set_password((const unsigned char*)cmd.password,strlen(cmd.password));
     if(err==ESP_OK)err=esp_wifi_sta_enterprise_enable();
    }
#endif
    wifi_config_t cfg={0};strlcpy((char*)cfg.sta.ssid,cmd.ssid,sizeof(cfg.sta.ssid));if(!enterprise)strlcpy((char*)cfg.sta.password,cmd.password,sizeof(cfg.sta.password));cfg.sta.pmf_cfg.capable=true;
    if(err==ESP_OK)err=esp_wifi_set_mode(ap_enabled?WIFI_MODE_APSTA:WIFI_MODE_STA);
    if(err==ESP_OK)err=esp_wifi_set_config(WIFI_IF_STA,&cfg);
    if(err==ESP_OK){
     nvs_set_str(store,"eap_ssid",enterprise?cmd.ssid:"");nvs_set_str(store,"eap_user",enterprise?cmd.username:"");nvs_set_str(store,"eap_pass",enterprise?cmd.password:"");nvs_commit(store);
     start_radio(ap_enabled);want_wifi=true;retries=0;err=esp_wifi_connect();
    }
    LOCK();state.wifi_connected=false;state.ip[0]=0;if(err==ESP_OK){strlcpy(state.ssid,cmd.ssid,33);state.wifi_enterprise=enterprise;}strlcpy(state.wifi_status,err==ESP_OK?"Connecting...":"Could not configure Wi-Fi; try again",sizeof(state.wifi_status));changed();UNLOCK();memset(&cfg,0,sizeof(cfg));
   }else if(cmd.type==WIFI_OFF||cmd.type==FORGET){
    reconnect_after_scan=false;want_wifi=false;esp_wifi_scan_stop();esp_wifi_disconnect();if(cmd.type==FORGET){wifi_config_t empty={0};esp_wifi_set_config(WIFI_IF_STA,&empty);esp_wifi_sta_enterprise_disable();esp_eap_client_clear_identity();esp_eap_client_clear_username();esp_eap_client_clear_password();nvs_erase_key(store,"eap_ssid");nvs_erase_key(store,"eap_user");nvs_erase_key(store,"eap_pass");nvs_commit(store);}
    if(!ap_enabled){esp_wifi_stop();wifi_started=false;}LOCK();state.wifi_connected=false;state.ip[0]=0;strlcpy(state.wifi_status,"Wi-Fi off",sizeof(state.wifi_status));if(cmd.type==FORGET)state.ssid[0]=0;changed();UNLOCK();
   }else if(cmd.type==HELLO_START)start_hello();else if(cmd.type==HELLO_STOP)stop_hello();
   memset(&cmd,0,sizeof(cmd));
  }
  LOCK();bool reconnect=want_wifi&&!state.wifi_connected&&!state.scanning&&retries>5&&esp_timer_get_time()>reconnect_at;UNLOCK();
  if(reconnect){reconnect_at=esp_timer_get_time()+30000000;retries=0;esp_wifi_connect();}
  LOCK();bool expired=state.hello_active&&esp_timer_get_time()>deadline;bool cleanup=!state.hello_active&&ap_enabled;bool preserve=state.introduction_ready;UNLOCK();if(expired||cleanup)close_hello(preserve);
 }
}
static void headers(httpd_req_t *r){httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");httpd_resp_set_hdr(r,"Referrer-Policy","no-referrer");httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'");}
static bool valid_token(const char *provided){
 LOCK();bool ok=state.hello_active&&esp_timer_get_time()<deadline&&strlen(provided)==32;
 unsigned mismatch=0;for(int i=0;i<32&&ok;i++)mismatch|=(unsigned char)(provided[i]^token[i]);ok=ok&&!mismatch;UNLOCK();return ok;
}
static bool on_badge_ap(httpd_req_t *r){
 /* ESP-IDF's dual-stack HTTP listener returns IPv4-mapped IPv6 addresses.
    sockaddr_in truncates those and rejects phones that are already on the AP. */
 struct sockaddr_storage local={0},peer={0};socklen_t l=sizeof(local),p=sizeof(peer);int fd=httpd_req_to_sockfd(r);
 return getsockname(fd,(struct sockaddr*)&local,&l)==0&&getpeername(fd,(struct sockaddr*)&peer,&p)==0&&pet_socket_peer_allowed((struct sockaddr*)&local,l,(struct sockaddr*)&peer,p);
}
static esp_err_t forbidden(httpd_req_t *r){httpd_resp_set_status(r,"403 Forbidden");return httpd_resp_sendstr(r,"Open Hello on the badge and join its temporary Wi-Fi.");}
static esp_err_t portal(httpd_req_t *r){
 headers(r);if(!on_badge_ap(r))return forbidden(r);
 LOCK();bool active=state.hello_active&&esp_timer_get_time()<deadline;char url[128];strlcpy(url,state.hello_url,sizeof(url));UNLOCK();
 if(!active)return forbidden(r);
 ESP_LOGI(TAG,"Captive portal redirect served");
 httpd_resp_set_status(r,"302 Found");httpd_resp_set_hdr(r,"Location",url);
 httpd_resp_set_type(r,"text/html");return httpd_resp_sendstr(r,"Open your badge introduction at http://192.168.4.1/");
}
static esp_err_t portal_redirect(httpd_req_t *r,httpd_err_code_t err){(void)err;return portal(r);}
static esp_err_t opened(httpd_req_t *r){
 headers(r);if(!on_badge_ap(r))return forbidden(r);char query[80]={0},t[40]={0};
 if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"t",t,sizeof(t))!=ESP_OK||!valid_token(t))return forbidden(r);
 LOCK();if(state.hello_active&&!strcmp(t,token)){state.form_opened=true;changed();}UNLOCK();ESP_LOGI(TAG,"Introduction form opened");return httpd_resp_sendstr(r,"ok");
}
static esp_err_t page(httpd_req_t *r){
 if(!on_badge_ap(r))return forbidden(r);
 char query[80]={0},t[40]={0};headers(r);
 if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"t",t,sizeof(t))!=ESP_OK||!valid_token(t)){httpd_resp_set_status(r,"403 Forbidden");return httpd_resp_sendstr(r,"Open Hello on the badge and scan the current QR code.");}
 LOCK();if(!state.hello_active||strcmp(t,token)){UNLOCK();httpd_resp_set_status(r,"403 Forbidden");return httpd_resp_sendstr(r,"This hello has ended. Scan a new code on the badge.");}state.phone_joined=true;changed();UNLOCK();httpd_resp_set_type(r,"text/html; charset=utf-8");
 extern const unsigned char pet_guest_start[] asm("_binary_guest_html_start");extern const unsigned char pet_guest_end[] asm("_binary_guest_html_end");
 ESP_LOGI(TAG,"Introduction page served");
 return httpd_resp_send(r,(const char*)pet_guest_start,pet_guest_end-pet_guest_start);
}
static esp_err_t intro(httpd_req_t *r){
 headers(r);if(!on_badge_ap(r))return forbidden(r);if(r->content_len<=0||r->content_len>1024)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid introduction");
 char body[1025];int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
 cJSON *j=cJSON_Parse(body);cJSON *t=cJSON_GetObjectItemCaseSensitive(j,"token"),*name=cJSON_GetObjectItemCaseSensitive(j,"name"),*interest=cJSON_GetObjectItemCaseSensitive(j,"interest"),*consent=cJSON_GetObjectItemCaseSensitive(j,"consent");
 bool ok=cJSON_IsString(t)&&valid_token(t->valuestring)&&cJSON_IsString(name)&&pet_text_valid(name->valuestring,40)&&cJSON_IsString(interest)&&pet_text_valid(interest->valuestring,80)&&cJSON_IsTrue(consent);
 if(!ok){cJSON_Delete(j);return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Check the fields or scan a fresh QR code.");}
 LOCK();/* Recheck under lock: one introduction per live session. */
 if(!state.hello_active||esp_timer_get_time()>=deadline||strcmp(t->valuestring,token)){UNLOCK();cJSON_Delete(j);httpd_resp_set_status(r,"409 Conflict");return httpd_resp_sendstr(r,"This introduction is already submitted or closed.");}
 if(state.introduction_ready){bool same=!strcmp(state.guest,name->valuestring)&&!strcmp(state.interest,interest->valuestring);UNLOCK();cJSON_Delete(j);if(!same)httpd_resp_set_status(r,"409 Conflict");return httpd_resp_sendstr(r,same?"Your introduction is already on the badge.":"An introduction has already arrived for this session.");}
 strlcpy(state.guest,name->valuestring,sizeof(state.guest));strlcpy(state.interest,interest->valuestring,sizeof(state.interest));state.introduction_ready=true;state.asleep=false;deadline=esp_timer_get_time()+8000000;changed();UNLOCK();cJSON_Delete(j);ESP_LOGI(TAG,"Introduction received");return httpd_resp_sendstr(r,"Look at the badge. Your introduction has arrived.");
}
void pet_services_init(void){
 lock=xSemaphoreCreateMutex();commands=xQueueCreate(8,sizeof(command_t));assert(lock&&commands);
 /* Never erase existing user NVS as an automatic recovery action. */
 ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(nvs_open("pet",NVS_READWRITE,&store));
 state.brightness=100;state.motion=true;uint8_t v;
 if(nvs_get_u8(store,"brightness",&v)==ESP_OK)state.brightness=v;
 if(nvs_get_u8(store,"motion",&v)==ESP_OK)state.motion=v;
 uint32_t count=0;nvs_get_u32(store,"connections",&count);state.connections=count;strlcpy(state.wifi_status,"Wi-Fi off",72);
 uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);snprintf(state.ap_ssid,33,"PET-%02X%02X",mac[4],mac[5]);random_hex(state.ap_password,6);
 ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());station=esp_netif_create_default_wifi_sta();access_point=esp_netif_create_default_wifi_ap();
 esp_netif_dhcps_stop(access_point);uint8_t offer_dns=0x02;
 ESP_ERROR_CHECK(esp_netif_dhcps_option(access_point,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer_dns,sizeof(offer_dns)));
 esp_netif_dns_info_t dns={0};dns.ip.type=ESP_IPADDR_TYPE_V4;dns.ip.u_addr.ip4.addr=htonl(0xc0a80401);
 ESP_ERROR_CHECK(esp_netif_set_dns_info(access_point,ESP_NETIF_DNS_MAIN,&dns));
 wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&cfg));esp_wifi_set_storage(WIFI_STORAGE_FLASH);
 esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL);esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL);
 wifi_config_t saved={0};esp_wifi_get_config(WIFI_IF_STA,&saved);
 httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.stack_size=6144;hc.max_open_sockets=5;hc.recv_wait_timeout=3;hc.send_wait_timeout=3;hc.lru_purge_enable=true;httpd_handle_t server=NULL;
 ESP_ERROR_CHECK(httpd_start(&server,&hc));httpd_uri_t root={.uri="/",.method=HTTP_GET,.handler=portal};
 httpd_uri_t form={.uri="/hello",.method=HTTP_GET,.handler=page};httpd_uri_t loaded={.uri="/opened",.method=HTTP_POST,.handler=opened};httpd_uri_t post={.uri="/intro",.method=HTTP_POST,.handler=intro};httpd_register_uri_handler(server,&root);httpd_register_uri_handler(server,&post);httpd_register_uri_handler(server,&form);httpd_register_uri_handler(server,&loaded);httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,portal_redirect);
 pet_cloud_init();
 configASSERT(xTaskCreate(worker,"pet_services",6144,NULL,4,NULL)==pdPASS);pet_portal_init();
 if(saved.sta.ssid[0]){
  char eap_ssid[33]={0},user[128]={0},pass[65]={0};size_t n=sizeof(eap_ssid);nvs_get_str(store,"eap_ssid",eap_ssid,&n);
  if(!strcmp(eap_ssid,(char*)saved.sta.ssid)){n=sizeof(user);nvs_get_str(store,"eap_user",user,&n);n=sizeof(pass);nvs_get_str(store,"eap_pass",pass,&n);pet_services_connect_enterprise(eap_ssid,user,pass);}else pet_services_connect((char*)saved.sta.ssid,(char*)saved.sta.password);
  memset(pass,0,sizeof(pass));
 }
 memset(&saved,0,sizeof(saved));ESP_LOGI(TAG,"Wi-Fi and phone introduction server initialized");
}
