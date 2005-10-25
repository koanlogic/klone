#ifndef _KLONE_RSFILTER_H_
#define _KLONE_RSFILTER_H_
#include <klone/codec.h>

/* the filter will buffer the first RFBUFSZ bytes printed so page developers
 * can postpone header modifications (i.e. header will be sent after RFBUFSZ
 * bytes of printed data or on io_flush()
 */
enum { RFBUFSZ = 4096 };

struct response_s;
typedef struct response_filter_s response_filter_t;

int response_filter_create(response_t *rs, response_filter_t **prf);

#endif
