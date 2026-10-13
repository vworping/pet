#pragma once
#include "pet_services.h"
void pet_cloud_init(void);
void pet_cloud_snapshot(pet_status_t *out);
void pet_cloud_start(void);
void pet_cloud_cancel(void);
bool pet_cloud_accept(void);
void pet_cloud_arm(void);
void pet_cloud_save(void);
void pet_cloud_clear_arm(void);
bool pet_cloud_clear_requested(void);
void pet_cloud_clear_result(bool success);
void pet_cloud_card(const char *url);
void pet_cloud_card_error(const char *reason);
bool pet_cloud_card_armed(void);
bool pet_cloud_card_url(char *url, unsigned capacity);
bool pet_cloud_configure(const char *base, const char *key);
