#include <klone_conf.h>

#if ENABLE_SUP_KILT
#include <klone/kilt.h>
#include <klone/kilt_urls.h>
kilt_url_t *kilt_urls = NULL;
size_t kilt_nurls = 0;
#endif

void register_pages(void);void unregister_pages(void);
void register_pages(void){} void unregister_pages(void){}
