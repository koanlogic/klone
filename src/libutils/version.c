#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <klone/version.h>
#include <u/libu.h>

/**
 *  \defgroup utils utils - Versio
 *  \{
 *      \par
 */

/**
 * \brief   Return KLone version string (x.y.z)
 * 
 *  Return KLone version string in the format x.y.z.
 *
 * \return version static string
 */
const char *klone_version(void)
{
    return KLONE_VERSION;
}


/**
 *  \}
 */

   
