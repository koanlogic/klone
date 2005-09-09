/* this header is included by all embedded pages */
#ifndef _KLONE_EMB_PAGE_H_
#define _KLONE_EMB_PAGE_H_
#include <klone/page.h>
#include <klone/queue.h>

void register_page(page_t *pg);
void unregister_page(page_t *pg);

#endif
