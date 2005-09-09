#ifndef _KLONE_CODEC_GZIP_H__
#define _KLONE_CODEC_GZIP_H__

/* the codec [un]compresses (using libz) the stream to whom it's applied */

/* possibile values for io_gzip_create */
enum { GZIP_COMPRESS, GZIP_UNCOMPRESS };

struct codec_gzip_s;
typedef struct codec_gzip_s codec_gzip_t;

int codec_gzip_create(int operation, codec_gzip_t **pioz);

#endif
