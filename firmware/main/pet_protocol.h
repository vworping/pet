#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Pure helpers shared by firmware and malformed-input tests. IPv4 is host order. */
bool pet_portal_peer_allowed(uint32_t local, uint32_t peer);
size_t pet_dns_reply(const uint8_t *query,size_t length,uint8_t *reply,size_t capacity);
bool pet_text_valid(const char *text,size_t max_characters);
