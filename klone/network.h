#ifndef _KLONE_NETWORK_H_
#define _KLONE_NETWORK_H_
#include <klone/klone.h>

#ifdef OS_UNIX
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <sys/un.h>
    #include <netdb.h>
    #include <netinet/in.h>
#else
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif

#endif
