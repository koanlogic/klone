#ifndef _KLONE_PPC_CMD_H_
#define _KLONE_PPC_CMD_H_

/* ppc centralized command list */
enum {
    PPC_CMD_UNKNOWN,            /* wrong command                    */
    PPC_CMD_NOP,                /* no operation                     */

    /* in-memory sessions ppc commands                              */
    PPC_CMD_MSES_SAVE,          /* save a session                   */
    PPC_CMD_MSES_DELOLD,        /* delete the oldest ession         */
    PPC_CMD_MSES_REMOVE,        /* remove a session                 */

    /* logging ppc commands                                         */
    PPC_CMD_LOG_ADD             /* add a log row                    */

};

#endif
