#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PET_NETWORKS 10
#define PET_NAME_SIZE 161
#define PET_INTEREST_SIZE 321
typedef enum { PET_HELLO_IDLE, PET_HELLO_WIFI, PET_HELLO_CLOCK, PET_HELLO_LINK, PET_HELLO_RETRY, PET_HELLO_READY } pet_hello_stage_t;
typedef struct {
 char ssid[33]; int rssi; bool locked, enterprise;
} pet_network_t;
typedef struct {
 bool wifi_connected, scanning, wifi_enterprise;
 bool hello_starting, hello_active, phone_joined, form_opened, introduction_ready;
 pet_hello_stage_t hello_stage;
 bool asleep, motion, save_power;
 int brightness, wifi_rssi;
 unsigned wifi_channel;
 unsigned revision, connections, network_count;
 unsigned card_stage, pending_cards;
 char card_status[112],card_id[33];
 char wifi_status[72], ssid[33], ip[16], ap_ssid[33], ap_password[17];
 char hello_error[80];
 char hello_url[128], guest[PET_NAME_SIZE], interest[PET_INTEREST_SIZE];
 pet_network_t networks[PET_NETWORKS];
} pet_status_t;
void pet_services_init(void);
void pet_services_snapshot(pet_status_t *out);
void pet_services_scan(void);
void pet_services_connect(const char *ssid,const char *password);
void pet_services_connect_enterprise(const char *ssid,const char *username,const char *password);
void pet_services_wifi_off(void);
void pet_services_forget_wifi(void);
void pet_services_hello_start(void);
void pet_services_hello_cancel(void);
bool pet_services_accept(void);
void pet_services_set_connections(uint32_t count);
void pet_services_set_brightness(int value);
void pet_services_set_motion(bool on);
void pet_services_set_saver(bool on);
void pet_services_sleep(bool asleep);

void pet_services_card_arm(void);
void pet_services_card_save(void);
void pet_services_card_clear(void);
