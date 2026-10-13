#include "pet_socket.h"
#include "pet_protocol.h"
#include <string.h>

static bool ipv4_address(const struct sockaddr *address,socklen_t len,uint32_t *out){
    if(!address||len<sizeof(struct sockaddr))return false;
    if(address->sa_family==AF_INET&&len>=sizeof(struct sockaddr_in)){
        struct sockaddr_in v4;memcpy(&v4,address,sizeof(v4));
        *out=ntohl(v4.sin_addr.s_addr);return true;
    }
    if(address->sa_family==AF_INET6&&len>=sizeof(struct sockaddr_in6)){
        struct sockaddr_in6 v6;memcpy(&v6,address,sizeof(v6));
        static const unsigned char mapped[12]={0,0,0,0,0,0,0,0,0,0,0xff,0xff};
        if(memcmp(v6.sin6_addr.s6_addr,mapped,sizeof(mapped)))return false;
        uint32_t v4;memcpy(&v4,v6.sin6_addr.s6_addr+12,sizeof(v4));
        *out=ntohl(v4);return true;
    }
    return false;
}
bool pet_socket_peer_allowed(const struct sockaddr *local,socklen_t local_len,
                             const struct sockaddr *peer,socklen_t peer_len){
    uint32_t local_v4,peer_v4;
    return ipv4_address(local,local_len,&local_v4)&&
           ipv4_address(peer,peer_len,&peer_v4)&&
           pet_portal_peer_allowed(local_v4,peer_v4);
}
