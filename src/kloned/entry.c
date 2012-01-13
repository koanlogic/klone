/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 */

#include "klone_conf.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <u/libu.h>
#include <klone/server.h>
#include <klone/os.h>
#include <klone/context.h>
#include <klone/utils.h>
#include <klone/version.h>
#include "main.h"

int facility = LOG_LOCAL0;

static context_t c;
context_t  *ctx = &c; /* exported */

#ifdef OS_WIN
    /* Win32 service name and description */
    enum { SS_NAME_BUFSZ = 64, SS_DESC_BUFSZ = 256 };
    static char ss_name[SS_NAME_BUFSZ] = "kloned";
    static char ss_desc[SS_DESC_BUFSZ] = "kloned daemon";

    int InstallService(); 
    int RemoveService();
#endif

static void usage (void)
{
    static const char *us = 
"Usage: kloned OPTIONS ARGUMENTS                                            \n"
"Version: %s "
#ifdef SSL_OPENSSL
"(with OpenSSL)"
#endif
#ifdef SSL_CYASSL
"(with CyaSSL)"
#endif
            " - Copyright (c) 2005-2012 KoanLogic s.r.l.                     \n"
"All rights reserved.                                                       \n"
"\n"
"    -d          turn on debugging (forces iterative mode)                  \n"
"    -f file     load an external config file                               \n"
"    -c          override configuration lines via standard input            \n"
"    -p file     save daemon PID to file                                    \n"
"    -F          run in foreground                                          \n"
"    -h          display this help                                          \n"
#ifdef OS_WIN
"    -i          install KLone Windows service                              \n"
"    -u          remove KLone Windows service                               \n"
#endif
"    -V          print KLone version and exit                               \n"
"    -n          do not chdir when daemonizing                              \n"
"\n";

    fprintf(stderr, us, klone_version());

    exit(1);
}

static int parse_opt(int argc, char **argv)
{
    int ret;
#ifdef OS_WIN
        #define CMDLINE_FORMAT "nhVFdiuf:cp:"
#else
        #define CMDLINE_FORMAT "nhVFdf:cp:"
#endif

    /* set defaults */
    ctx->daemon++;

    while((ret = getopt(argc, argv, CMDLINE_FORMAT)) != -1)
    {
        switch(ret)
        {
        case 'f':   /* source a config file */
            ctx->ext_config = u_strdup(optarg);
            dbg_err_if(ctx->ext_config == NULL);
            u_dbg("ext config: %s", ctx->ext_config);
            break;

        case 'c':   /* override config from command-line */
            ctx->cmd_config = 1;
            break;

        case 'p':   /* PID file */
            ctx->pid_file = u_strdup(optarg);
            dbg_err_if(ctx->pid_file == NULL);
            u_dbg("PID file: %s", ctx->pid_file);
            break;

        case 'd':   /* turn on debugging */
            ctx->debug++;
            break;

        case 'F':   /* run in foreground (not as a daemon/service) */
            ctx->daemon = 0;
            break;

        case 'V':   /* print version and exit */
            u_print_version_and_exit();
            break;

        case 'n':   /* don't chdir in daemon mode */
            ctx->nochdir = 1;
            break;

#ifdef OS_WIN
        case 'i':   /* install kloned service and exit */
            ctx->serv_op = SERV_INSTALL;
            break;

        case 'u':   /* uninstall kloned service and exit */
            ctx->serv_op = SERV_REMOVE;
            break;
#endif

        default:
        case 'h': 
            usage();
        }
    }

    ctx->narg = argc - optind;
    ctx->arg = argv + optind;

    return 0;
err:
    return ~0;
}

#if defined(OS_WIN)

/* install the service with the service manager. after successful installation
   you can run the service from ControlPanel->AdminTools->Services */
int InstallService(void) 
{
    SC_HANDLE hSCM, hService;
    char szModulePathname[_MAX_PATH];
    SERVICE_DESCRIPTION sd = { ss_desc };
    int rc;

    // Open the SCM on this machine.
    hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

    dbg_err_if(hSCM == NULL);

    dbg_err_if(GetModuleFileName(GetModuleHandle(NULL), szModulePathname, 
        _MAX_PATH) == 0 );

    /* add this service to the SCM's database */
    hService = CreateService(hSCM, ss_name, ss_name,
        SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS, 
        SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
        szModulePathname, NULL, NULL, NULL, NULL, NULL);

    dbg_err_if(hService == NULL);

    rc = ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);

    dbg_err_if(rc == 0);

    /* success */
    MessageBox(NULL, "Service installation succeded", ss_name, MB_OK);

    return 0; 
err:
    /* common error handling */
    warn_strerror(GetLastError());
    MessageBox(NULL, "Service installation error", ss_name, MB_OK);
    return ~0;
}

/* uninstall this service from the system */
int RemoveService(void) 
{
    SC_HANDLE hSCM, hService;
    int rc;

    hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    dbg_err_if(hSCM == NULL);

    /* Open this service for DELETE access */
    hService = OpenService(hSCM, ss_name, DELETE);

    dbg_err_if(hService == NULL);

    /* Remove this service from the SCM's database */
    rc = DeleteService(hService);

    dbg_err_if(rc == 0);

    /* success */
    MessageBox(NULL, "Uninstall succeeded", ss_name, MB_OK);
    return 0;
err:
    /* common error handling */
    warn_strerror(GetLastError());
    MessageBox(NULL, "Uninstall failed", ss_name, MB_OK);
    return ~0;
}

/* this function will be called by the SCM to request an action */
DWORD WINAPI HandlerEx(DWORD dwControl, DWORD dwEventType, 
        LPVOID lpEventData, LPVOID lpContext)
{
    enum { DENY_ACTION = 0xff };

    switch(dwControl)
    {
    case SERVICE_CONTROL_INTERROGATE:
        u_dbg("SERVICE_CONTROL_INTERROGATE" );
        SetServiceStatus(ctx->hServiceStatus, &ctx->status);
        return NO_ERROR;

    case SERVICE_CONTROL_STOP:
        u_dbg("SERVICE_CONTROL_STOP");

        if(ctx->status.dwCurrentState == SERVICE_STOPPED)
            return NO_ERROR; /* service already stopped */

        /* start the stop procedure, move to stop_pending state */
        ctx->status.dwCheckPoint = 1;
        ctx->status.dwWaitHint = 2000;
        ctx->status.dwCurrentState = SERVICE_STOP_PENDING; 
        SetServiceStatus(ctx->hServiceStatus, &ctx->status);

        server_stop(ctx->server);
        return NO_ERROR;

    case SERVICE_CONTROL_PAUSE:
        u_dbg("SERVICE_CONTROL_PAUSE");
        break;

    case SERVICE_CONTROL_CONTINUE:
        u_dbg("SERVICE_CONTROL_CONTINUE");
        break;

    case SERVICE_CONTROL_SHUTDOWN:
        u_dbg("SERVICE_CONTROL_SHUTDOWN");
        break;

    case SERVICE_CONTROL_PARAMCHANGE:
        u_dbg("SERVICE_CONTROL_PARAMCHANGE");
        break;

    default:
        u_dbg("SERVICE_CONTROL_UNKNOWN!!!!");
    }
    if(dwControl > 127 && dwControl < 255)
    {
        /* user defined control code */
        u_dbg("SERVICE_CONTROL_USER_DEFINED");
    }

    return ERROR_CALL_NOT_IMPLEMENTED;
}

/* this is the main function of the service. when this function returns the
 * service will be terminated by the SCM */
void WINAPI ServiceMain(DWORD argc, PTSTR *argv)
{
    SERVICE_STATUS *pSt = &ctx->status;

    /* register the service with the ServiceControlManager */
    ctx->hServiceStatus = RegisterServiceCtrlHandlerEx(ss_name, HandlerEx, ctx);
    dbg_err_if( ctx->hServiceStatus == 0 );

    /* init the status struct and update the service status */
    ZeroMemory(pSt, sizeof(SERVICE_STATUS));
    /* just one service in this exe */
    pSt->dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    /* action supported by the service */
    pSt->dwControlsAccepted = SERVICE_ACCEPT_STOP;
    /* error returned while starting/stopping */
    pSt->dwWin32ExitCode = NO_ERROR;          
    /* service specific exit code */
    pSt->dwServiceSpecificExitCode = 0;          
    /* we're still initializing */
    pSt->dwCurrentState = SERVICE_START_PENDING;
    /* for progress operation */
    pSt->dwCheckPoint = 1;
    /* wait hint */
    pSt->dwWaitHint = 1000;
    /* set status */
    dbg_err_if(SetServiceStatus(ctx->hServiceStatus, pSt) == 0);

    dbg_err_if(parse_opt(argc, argv));

    /* load config and initialize */
    dbg_err_if(app_init());

    /* this should happen after initialization but I don't want to
       mess main.c with win32-only code */

    /* notify the end of initialization */
    u_dbg("SERVICE_RUNNING");
    ctx->status.dwCurrentState = SERVICE_RUNNING;
    ctx->status.dwCheckPoint = ctx->status.dwWaitHint = 0;    
    dbg_err_if(!SetServiceStatus(ctx->hServiceStatus, &ctx->status));

    /* run the main loop */
    app_run();

    /* let the service terminate */
    ctx->status.dwCurrentState = SERVICE_STOPPED;
    dbg_err_if(!SetServiceStatus(ctx->hServiceStatus, &ctx->status));

    return;

err:
    warn_strerror(GetLastError());

    /* let the service terminate */
    ctx->status.dwCurrentState = SERVICE_STOPPED;
    dbg_err_if(!SetServiceStatus(ctx->hServiceStatus, &ctx->status));
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, 
    LPSTR lpCmdLine, int nCmdShow)
{
    SERVICE_TABLE_ENTRY ServiceTable[] = 
    {
        {   ss_name, ServiceMain }, 
        {   NULL, NULL }    /* end of list */
    };
    int rc = 0;
    const char *name, *desc;

    memset(ctx, 0, sizeof(context_t));

    /* parse command line parameters (and set ctx vars). NOTE: this work only 
       if launched by command line, for services see ServiceMain */
    dbg_err_if(parse_opt(__argc, __argv));

    if(ctx->serv_op)
    {
        /* load config and initialize */
        dbg_err_if(app_init());

        /* set up service name and description reading from the config file */
        name = u_config_get_subkey_value(ctx->config, "daemon.name");
        if(name)
            u_strlcpy(ss_name, name, sizeof ss_name);

        desc = u_config_get_subkey_value(ctx->config, "daemon.description");
        if(desc)
            u_strlcpy(ss_desc, desc, sizeof ss_desc);

        if(ctx->serv_op == SERV_INSTALL)
            dbg_err_if(InstallService());
        else    
            dbg_err_if(RemoveService());
    } else if(ctx->daemon) {
        u_dbg("Starting in service mode...");
        /* StartServiceCtrlDispatcher does not return until the service 
           has stopped running...  */
        if(!StartServiceCtrlDispatcher(ServiceTable))
            warn_strerror(GetLastError());
    } else {
        /* load config and initialize */
        dbg_err_if(app_init());

        rc = app_run();
    }

    dbg_err_if(app_term());

    /* if debugging then call exit(3) because it's needed to gprof to dump 
       its stats file (gmon.out) */
    if(ctx->debug)
        return rc;

    /* don't use return because exit(3) will be called and we don't want
       FILE* buffers to be automatically flushed (klog_file_t will write same 
       lines more times, once by the parent process and N times by any child
       created when FILE buffer was not empty) */
    _exit(rc); 
err:
    app_term();

    if(ctx->debug) 
        return rc;
    _exit(EXIT_FAILURE);
}

#elif defined(OS_UNIX)

int main(int argc, char **argv)
{
    int rc = 0;

    memset(ctx, 0, sizeof(context_t));

    /* parse command line parameters (and set ctx vars) */
    dbg_err_if(parse_opt(argc, argv));

    if(getenv("GATEWAY_INTERFACE"))
        ctx->cgi = 1;
        
    /* load config and initialize */
    warn_err_ifm(app_init(), "kloned init error (more info in the log file)");

#ifdef HAVE_FORK
    /* daemonize if not -F */
    if(ctx->daemon && !ctx->cgi)
        con_err_ifm(daemon(ctx->nochdir, 0), "daemon error");
#endif  /* HAVE_FORK */

    /* save the PID in a file */
    if(ctx->pid_file)
        dbg_err_if(u_savepid(ctx->pid_file));

    /* jump to the main loop */
    rc = app_run();

    dbg_err_if(app_term());

    /* if debugging then call exit(3) because it's needed to gprof to dump 
       its stats file (gmon.out) */
    if(ctx->debug)
        return rc;

    /* don't use return because exit(3) will be called and we don't want
       FILE* buffers to be automatically flushed (klog_file_t will write same 
       lines more times, once by the parent process and N times by any child
       created when FILE buffer was not empty) */
    _exit(rc);
err:
    app_term();
    if(ctx->debug)
        return ~0;
    _exit(EXIT_FAILURE);
}

#else
    #error unsupported platform
#endif

