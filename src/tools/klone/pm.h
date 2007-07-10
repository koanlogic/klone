#ifndef _KLONE_PM_H_
#define _KLONE_PM_H_

/* pattern matching object */
struct pm_s;
typedef struct pm_s pm_t;

int pm_create(pm_t **ppm);
int pm_free(pm_t *pm);
int pm_is_empty(pm_t *pm);
int pm_add(pm_t *pm, const char *pattern);
int pm_remove(pm_t *pm, const char *pattern);
int pm_match(pm_t *pm, const char *uri);

#endif
