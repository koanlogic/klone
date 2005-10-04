#include "conf.h"
#ifndef HAVE_UNLINK

int unlink(const char *pathname)
{
    implement unlink() for this platform
}

#else
int unlink_dummy_decl_stub = 0;
#endif 
