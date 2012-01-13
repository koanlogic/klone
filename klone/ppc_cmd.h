/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ppc_cmd.h,v 1.9 2007/11/09 22:06:26 tat Exp $
 */

#ifndef _KLONE_PPC_CMD_H_
#define _KLONE_PPC_CMD_H_

/* ppc centralized command list */
enum {
    PPC_CMD_UNKNOWN,            /* wrong command                    */
    PPC_CMD_NOP,                /* no operation                     */
    PPC_CMD_FORK_CHILD,         /* launch a new child               */
    PPC_CMD_RESPONSE_OK,        /* ppc response success             */
    PPC_CMD_RESPONSE_ERROR,     /* ppc response error               */

    /* in-memory sessions ppc commands                              */
    PPC_CMD_MSES_SAVE,          /* save a session                   */
    PPC_CMD_MSES_GET,           /* get session data                 */
    PPC_CMD_MSES_DELOLD,        /* delete the oldest ession         */
    PPC_CMD_MSES_REMOVE,        /* remove a session                 */

    /* logging ppc commands                                         */
    PPC_CMD_LOG_ADD,            /* add a log line                   */
    PPC_CMD_LOG_GET,            /* get a log line (in-mem logging)  */

    /* access log commands                                          */
    PPC_CMD_ACCESS_LOG          /* add an access log line           */
};

#endif
