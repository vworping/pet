#pragma once
#include <stdbool.h>
#ifdef ESP_PLATFORM
#include "lwip/sockets.h"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* Normalize IPv4 and IPv4-mapped IPv6 before enforcing the AP-only rule. */
bool pet_socket_peer_allowed(const struct sockaddr *local,socklen_t local_len,
                             const struct sockaddr *peer,socklen_t peer_len);
