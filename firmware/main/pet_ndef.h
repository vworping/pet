#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PET_NDEF_BYTES 144
#define PET_NDEF_URL_MAX 139
/* Builds an NTAG213 user-memory image while preserving its known lock TLV.
 * NULL URL clears the message and zeroes the former payload. */
bool pet_ndef_image(const uint8_t original[PET_NDEF_BYTES], const char *url,
                    uint8_t out[PET_NDEF_BYTES]);
/* Read one short HTTPS URI record from a Type 2 user-memory image. */
bool pet_ndef_url(const uint8_t *data, unsigned length, char *url, unsigned capacity);
