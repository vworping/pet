#include "pet_portal.h"
#include "pet_protocol.h"
#include "pet_services.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include <errno.h>
#include <unistd.h>

/* Listen only on the temporary badge AP address, never the station interface. */
static void dns_task(void *arg){
 (void)arg;int fd=-1;bool measured=false;
 /* This single task owns its scratch buffers. Keep them off the socket stack. */
 static pet_status_t state;static uint8_t query[512],reply[528];
 for(;;){
  pet_services_snapshot(&state);
  if(!state.hello_active){if(fd>=0){close(fd);fd=-1;}measured=false;vTaskDelay(pdMS_TO_TICKS(250));continue;}
  if(fd<0){
   fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
   if(fd<0){vTaskDelay(pdMS_TO_TICKS(500));continue;}
   struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=htonl(0xc0a80401)};
   struct timeval timeout={.tv_sec=0,.tv_usec=250000};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
   if(bind(fd,(struct sockaddr*)&addr,sizeof(addr))<0){close(fd);fd=-1;vTaskDelay(pdMS_TO_TICKS(250));continue;}
   ESP_LOGI("pet_portal","Captive DNS ready on badge AP");
  }
  struct sockaddr_in peer;socklen_t plen=sizeof(peer);int len=recvfrom(fd,query,sizeof(query),0,(struct sockaddr*)&peer,&plen);
  if(!measured){ESP_LOGI("pet_portal","DNS running; stack headroom=%u bytes",(unsigned)uxTaskGetStackHighWaterMark(NULL));measured=true;}
  if(len<=0)continue;
  if(!pet_portal_peer_allowed(0xc0a80401,ntohl(peer.sin_addr.s_addr)))continue;
  pet_services_snapshot(&state);if(!state.hello_active)continue;
  size_t out=pet_dns_reply(query,(size_t)len,reply,sizeof(reply));if(out)sendto(fd,reply,out,0,(struct sockaddr*)&peer,plen);
 }
}
void pet_portal_init(void){configASSERT(xTaskCreate(dns_task,"pet_portal",8192,NULL,3,NULL)==pdPASS);}
