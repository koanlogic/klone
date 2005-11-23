#ifndef _KLONE_CODEC_GZIP_H_
#define _KLONE_CODEC_GZIP_H_

/* the codec [un]compresses (using libz) the stream to whom it's applied */

/* possibile values for io_gzip_create */
enum { GZIP_COMPRESS, GZIP_UNCOMPRESS };

int codec_gzip_create(int operation, codec_t **pioz);

#endif
