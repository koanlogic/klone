#ifndef _KLONE_TIMERM_H_
#define _KLONE_TIMERM_H_

struct timerm_s;
typedef struct timerm_s timerm_t;

struct alarm_s;
typedef struct alarm_s alarm_t;

typedef int (*alarm_cb_t)(alarm_t *, void *arg);

int timerm_add(int secs, alarm_cb_t cb, void *arg, alarm_t **pa);
int timerm_del(alarm_t *a);

#endif
