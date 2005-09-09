#ifndef _KLONE_EMB_H_
#define _KLONE_EMB_H_
#include <stdint.h>
#include <sys/stat.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/queue.h>
#include <klone/session.h>
#include <klone/codec.h>
#include <klone/codgzip.h>
#include <klone/utils.h>
#include <klone/debug.h>

/* supported embedded resource type */
enum {
    ET_FILE,                /* embedded file                */
    ET_PAGE                 /* dynamic web page             */
};

/* define resource list */
LIST_HEAD(emblist_s, embres_s);

/* common struct for embedded resources */
typedef struct embres_s
{
    LIST_ENTRY(embres_s) np;/* next & prev pointers         */
    const char *filename;   /* emb resource file name       */
    int type;               /* emb resource type (ET_*)     */
} embres_t;

/* embedded file */
typedef struct embfile_s
{
    embres_t res;           /* any emb resource must start with a embres_t    */
    size_t size;            /* size of the data block                         */
    uint8_t *data;          /* file data                                      */
    int comp;               /* if data is compressed                          */
    time_t mtime;           /* time of last modification                      */
    const char *mime_type;  /* guessed mime type                              */
    size_t file_size;       /* size of the source file (not compressed)       */
} embfile_t;

/* embedded dynamic klone page */
typedef struct embpage_s
{
    embres_t res;           /* any emb resource must start with a embres_t  */
    void (*run)(request_t*, response_t*, session_t*);   /* page code        */
} embpage_t;

int emb_init();
int emb_term();
int emb_register(embres_t *r);
int emb_unregister(embres_t *r);
int emb_lookup(const char *filename, embres_t **pr);
int emb_count();
int emb_getn(size_t n, embres_t **pr);

#endif

