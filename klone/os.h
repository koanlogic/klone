#ifndef _KLONE_OS_H_
#define _KLONE_OS_H_
#include <stdarg.h>
#include "conf.h"

/* OS compatibility layer */
#ifdef OS_WIN
    #include <windows.h>
    #include <process.h>
    #include <io.h>
    #include <time.h>
    #include <stdio.h>
    #include <winsock2.h>

    typedef unsigned int uint;
    typedef void (*sig_t)(int);

    #ifndef isblank
    #define isblank(c) (c == ' ' || c == '\t')
    #endif

#elif defined(OS_UNIX)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/wait.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/un.h>
    #include <netdb.h>
    #include <netinet/in.h>
#else
    #error unsupported platform
#endif

#ifndef HAVE_STRTOK_R
    char * strtok_r(char *s, const char *delim, char **last);
#endif

#ifndef HAVE_UNLINK
    int unlink(const char *pathname)
#endif

#ifndef HAVE_GETPID
    pid_t getpid();
#endif

#ifndef HAVE_SYSLOG
    void syslog(int priority, const char *msg, ...);
    #define LOG_LOCAL0 0
    #define LOG_DEBUG 0
#else
    #include <syslog.h>
#endif

#endif 

