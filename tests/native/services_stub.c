#include "pet_services.h"
#include <string.h>
static pet_status_t s;
void pet_services_init(void){memset(&s,0,sizeof(s));s.brightness=100;s.motion=true;strcpy(s.wifi_status,"Wi-Fi off");strcpy(s.ap_ssid,"PET-TEST");strcpy(s.ap_password,"test-password");}
void pet_services_snapshot(pet_status_t *out){*out=s;}
void pet_services_scan(void){s.scanning=false;s.network_count=8;const char *names[]={"Studio","Guest","Workshop","Lobby","Office","eduroam","Atrium","Library"};for(unsigned i=0;i<8;i++)strcpy(s.networks[i].ssid,names[i]);s.networks[0].locked=true;s.networks[5].enterprise=true;s.revision++;}
void pet_services_connect(const char *n,const char *p){(void)p;s.wifi_connected=true;s.wifi_channel=6;s.wifi_rssi=-48;strncpy(s.ssid,n,32);strcpy(s.ip,"192.0.2.10");strcpy(s.wifi_status,"Connected");s.revision++;}
void pet_services_wifi_off(void){s.wifi_connected=false;s.revision++;}
void pet_services_forget_wifi(void){pet_services_wifi_off();s.ssid[0]=0;}
void pet_services_hello_start(void){s.hello_starting=true;s.hello_stage=PET_HELLO_LINK;s.hello_active=false;s.phone_joined=false;s.form_opened=false;s.introduction_ready=false;s.card_stage=0;s.card_status[0]=0;s.hello_error[0]=0;s.revision++;}
void test_hello_opened(void){s.hello_starting=false;s.hello_stage=PET_HELLO_READY;s.hello_active=true;strcpy(s.hello_url,"https://pet-hello.vworp.chatgpt.site/h/12345678901234567890123456789012");s.revision++;}
void pet_services_hello_cancel(void){s.hello_starting=false;s.hello_active=false;s.introduction_ready=false;s.revision++;}
bool pet_services_accept(void){if(!s.introduction_ready||s.card_stage==2)return false;s.connections++;s.hello_active=false;s.introduction_ready=false;s.revision++;return true;}
void pet_services_set_brightness(int v){s.brightness=v;s.revision++;}
void pet_services_set_motion(bool v){s.motion=v;s.revision++;}
void pet_services_set_saver(bool v){s.save_power=v;s.revision++;}
void pet_services_sleep(bool v){s.asleep=v;s.revision++;}
void test_phone_joined(bool joined){s.phone_joined=joined;if(!joined)s.form_opened=false;s.revision++;}
void test_form_opened(void){s.form_opened=true;s.phone_joined=true;s.revision++;}
void test_link_closed(void){s.hello_active=false;s.phone_joined=false;s.form_opened=false;s.revision++;}
void test_introduction(void){s.card_stage=0;strcpy(s.card_status,"Add this conversation to a business card.");strcpy(s.guest,"Zoë García");strcpy(s.interest,"Color and human connection");s.introduction_ready=true;s.revision++;}
void test_long_introduction(void){memset(s.guest,'W',40);s.guest[40]=0;memset(s.interest,'W',80);s.interest[80]=0;s.introduction_ready=true;s.revision++;}
void test_hello_offline(void){strcpy(s.hello_error,"Connect PET to Wi-Fi to open Hello.");s.revision++;}

void pet_services_card_arm(void){s.card_stage=1;s.revision++;}
void pet_services_card_save(void){s.card_stage=3;s.revision++;}
void pet_services_card_clear(void){s.card_stage=6;strcpy(s.card_status,"Remove any card, then tap one writable NFC tag. Its link will be erased.");s.revision++;}
void test_card_cleared(void){s.card_stage=7;strcpy(s.card_status,"Tag cleared. Ready to set a business card.");s.revision++;}

void test_card_found(void){s.card_stage=2;strcpy(s.card_id,"22222222222222222222222222222222");strcpy(s.card_status,"Card found. Save this conversation to it?");s.revision++;}

void test_hello_stage(pet_hello_stage_t stage){s.hello_stage=stage;s.revision++;}

void test_card_updated(void){s.card_stage=4;strcpy(s.card_status,"Updated. This card is ready to give.");s.revision++;}

void pet_services_connect_enterprise(const char *n,const char *u,const char *p){(void)u;pet_services_connect(n,p);s.wifi_enterprise=true;}
