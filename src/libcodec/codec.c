#include <klone/codec.h>
#include <u/libu.h>

/**
 *  \addtogroup CODEC
 *  \{
 */

/**
 * \brief   Dispose all the resources allocated to the supplied codec
 *
 * Dispose all the resources allocated to the supplied \p codec
 *
 * \param   codec   the \c codec_t object to be disposed
 *
 * \return  always successful, i.e. \c 0
 */
int codec_free(codec_t *codec)
{
    if(codec)
    {
        if(codec->free)
            codec->free(codec);
        else
            u_free(codec);
    }
    return 0;
}

/**
 *  \}
 */
