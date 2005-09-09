#include "conf.h"
#ifndef HAVE_UNLINK

int unlink(const char *pathname)
{
    implement unlink() for this platform
}

#endif 
