#include "conf.h"
#ifndef HAVE_UNLINK

int unlink(const char *pathname)
{
    implement unlink() for this platform
}

#else
#include <unistd.h>
int unlink(const char *);
#endif 
