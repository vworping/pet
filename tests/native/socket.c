#include "pet_socket.h"
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct sockaddr_in v4(const char *text){
    struct sockaddr_in address={.sin_family=AF_INET};
    assert(inet_pton(AF_INET,text,&address.sin_addr)==1);return address;
}
static struct sockaddr_in6 v6(const char *text){
    struct sockaddr_in6 address={.sin6_family=AF_INET6};
    assert(inet_pton(AF_INET6,text,&address.sin6_addr)==1);return address;
}
#define allowed(a,b) pet_socket_peer_allowed((struct sockaddr*)&(a),sizeof(a),(struct sockaddr*)&(b),sizeof(b))
int main(void){
    struct sockaddr_in local=v4("192.168.4.1"),phone=v4("192.168.4.2"),station=v4("10.0.0.2"),outside=v4("192.168.5.2");
    struct sockaddr_in6 mapped_local=v6("::ffff:192.168.4.1"),mapped_phone=v6("::ffff:192.168.4.2"),mapped_station=v6("::ffff:10.0.0.2"),mapped_outside=v6("::ffff:192.168.5.2");
    assert(allowed(local,phone));assert(allowed(mapped_local,mapped_phone));
    assert(allowed(local,mapped_phone));assert(allowed(mapped_local,phone));
    assert(!allowed(station,phone));assert(!allowed(local,outside));
    assert(!allowed(mapped_station,mapped_phone));assert(!allowed(mapped_local,mapped_outside));
    const char *denied[]={"::1","fe80::c0a8:402","2001:db8::c0a8:402","::192.168.4.2","::ffff:192.168.4.0","::ffff:192.168.4.1","::ffff:192.168.4.255"};
    for(unsigned i=0;i<sizeof(denied)/sizeof(*denied);i++){struct sockaddr_in6 peer=v6(denied[i]);assert(!allowed(mapped_local,peer));}
    for(socklen_t n=0;n<sizeof(mapped_phone);n++)assert(!pet_socket_peer_allowed((struct sockaddr*)&mapped_local,sizeof(mapped_local),(struct sockaddr*)&mapped_phone,n));
    assert(!pet_socket_peer_allowed(NULL,0,(struct sockaddr*)&phone,sizeof(phone)));
    struct sockaddr unsupported={.sa_family=AF_UNSPEC};assert(!allowed(local,unsupported));
    puts("PASS: IPv4/mapped IPv6 AP sockets accepted; station, foreign, native IPv6 and truncated addresses rejected");
}
