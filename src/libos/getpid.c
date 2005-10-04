#include "conf.h"
#ifndef HAVE_GETPID

pid_t getpid()
{
    #ifdef OS_WIN
    return GetCurrentProcessId();
    #endif
}
#else
int getpid_dummy_decl_stub = 0;
#endif 
