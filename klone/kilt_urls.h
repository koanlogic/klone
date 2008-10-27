#ifndef _KLONE_KILT_URLS_H_
#define _KLONE_KILT_URLS_H_
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif 

#define KILT_URLS(u) size_t kilt_nurls = (sizeof(u)/sizeof(kilt_url_t)); kilt_url_t *kilt_urls = u;
#define NO_PARAMS { { NULL, NULL } } 
#define PARAMS(...) { __VA_ARGS__ },
#define P(k,v) {k,v}

#ifdef __cplusplus
}
#endif 

#endif
