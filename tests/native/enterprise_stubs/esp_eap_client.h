#pragma once
#include <stdbool.h>
#define ESP_OK 0
#define ESP_FAIL -1
int esp_eap_client_use_default_cert_bundle(bool enabled);
int esp_eap_client_set_domain_name(const char *name);
