#ifndef _KLONE_KILT_H_
#define _KLONE_KILT_H_
#include <stdlib.h>
#include <klone/kilt_urls.h>
#include <klone/dypage.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* url regex to function mapping */
typedef struct kilt_url_s
{
    const char *pattern;    /* url pattern */
    dypage_fun_t fun;
    dypage_param_t params[DYPAGE_MAX_PARAMS];
} kilt_url_t;

/* run the .kl1 script whose name is in "script" param */
void kilt_run_script(dypage_args_t *dp) ;

/* function that displays passed-in arguments */
void kilt_show_params(dypage_args_t *dp);

/* must be set by the user (helper macros in kilt_urls.h) */
extern kilt_url_t *kilt_urls;
extern size_t kilt_nurls;

#ifdef __cplusplus
}
#endif 

#endif
