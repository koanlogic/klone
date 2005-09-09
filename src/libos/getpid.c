#include "conf.h"
#ifndef HAVE_GETPID

pid_t getpid()
{
    #ifdef OS_WIN
    return GetCurrentProcessId();
    #endif
}

#endif 
