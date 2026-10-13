#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Length-delimited certificate DNS name; accepts umb.edu and its subdomains. */
bool pet_umb_server_name(const unsigned char *name, size_t length);
#ifdef ESP_PLATFORM
int pet_enterprise_use_umb_trust(void);
#endif
