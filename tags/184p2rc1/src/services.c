/** \file services.c
 * \brief Win32 services routines 
 *
 * Original author: Nick Gammon 
 */


#include "copyrite.h"
#include "config.h"

#ifdef WIN32

#include <windows.h>            /* for service and thread routines */

#include <stdlib.h>
#include <process.h>
#include <direct.h>

#include "conf.h"
#include "mushdb.h"

#include "match.h"
#include "externs.h"
#include "mymalloc.h"
#include "confmagic.h"

#ifdef WIN32SERVICES

#define THIS_SERVICE "PennMUSH"
#define THIS_SERVICE_DISPLAY "PennMUSH for Win32"

int WIN32_CDECL main(int argc, char **argv);
int mainthread(int argc, char **argv);


SERVICE_STATUS ssStatus;        /*  current status of the service */

SERVICE_STATUS_HANDLE sshStatusHandle;
DWORD dwGlobalErr;
DWORD TID = 0;
HANDLE threadHandle = NULL;

SC_HANDLE service = NULL;
SC_HANDLE SCmanager = NULL;

/*   declare the service threads: */
VOID service_main(DWORD dwArgc, LPTSTR * lpszArgv);
VOID WINAPI service_ctrl(DWORD dwCtrlCode);
BOOL ReportStatusToSCMgr(DWORD dwCurrentState,
                         DWORD dwWin32ExitCode,
                         DWORD dwCheckPoint, DWORD dwWaitHint);
VOID worker_thread(VOID * notused);
VOID StopService(LPTSTR lpszMsg);


static int CmdInstallService(int argc, char *argv[]);
static int CmdRemoveService(void);
static int CmdStartService(void);
static int CmdStopService(void);
static int CmdStatusService(void);
static void CmdDisplayFormat(void);
static char *convert_error(DWORD error);
static DWORD get_service_status(SERVICE_STATUS * svcstatus, int leave_open);
int WIN32_CDECL service_error(DWORD error_code, char *themessage, ...);


/* Need to include library: advapi32.lib for services routines */


int WIN32_CDECL
main(int argc, char **argv)
{

  SERVICE_TABLE_ENTRY dispatchTable[] = {
    {THIS_SERVICE, (LPSERVICE_MAIN_FUNCTION) service_main},
    {NULL, NULL}
  };

  SERVICE_STATUS svcstatus;
  DWORD status;

/*
   Get the command line parameters and see what the user wants us to do.
 */

  if ((argc == 2) &&
      ((*argv[1] == '-') || (*argv[1] == '/') || (*argv[1] == '\\'))) {
    if (!_stricmp("install", argv[1] + 1))
      CmdInstallService(argc, argv);
    else if (!_stricmp("remove", argv[1] + 1))
      CmdRemoveService();
    else if (!_stricmp("start", argv[1] + 1))
      CmdStartService();
    else if (!_stricmp("stop", argv[1] + 1))
      CmdStopService();
    else if (!_stricmp("status", argv[1] + 1))
      CmdStatusService();
    else if (!_stricmp("run", argv[1] + 1)) {

/*  do not start the MUSH if it is already a running service */

      status = get_service_status(&svcstatus, TRUE);
      if (status == 0 && svcstatus.dwCurrentState == SERVICE_RUNNING) {
        fprintf(stderr, "The MUSH is already running as a service.\n");
        return 1;
      }
      worker_thread(NULL);
    } else
      CmdDisplayFormat();
  } else if (argc != 1)
    CmdDisplayFormat();
  else {

    /*  do not start the MUSH if it is already a running service */

    status = get_service_status(&svcstatus, TRUE);
    if (status == 0 && svcstatus.dwCurrentState == SERVICE_RUNNING) {
      fprintf(stderr, "The MUSH is already running as a service.\n");
      return 1;
    }
    /*  Under Windows 95 they won't be able to use the service manager */

    if (status == ERROR_CALL_NOT_IMPLEMENTED) {
      worker_thread(NULL);
      return 0;
    }
    /*
       Register the dispatch table with the service controller.

       If this fails then we are running interactively.

     */

    fprintf(stderr, "Attempting to start PennMUSH as a service ...\n");
    if (!StartServiceCtrlDispatcher(dispatchTable)) {
      fprintf(stderr,
              "Unable to start service, assuming running console-mode application.\n");
      fprintf(stderr,
              "You can save time on the next invocation by specifying: pennmush /run\n");
      worker_thread(NULL);
    }
  }                             /*  end of argc == 1 */

  return 0;
}                               /* end of main */


/*   service_main() -- */
/*       this function takes care of actually starting the service, */
/*       informing the service controller at each step along the way. */
/*       After launching the worker thread, it waits on the event */
/*       that the worker thread will signal at its termination. */
static VOID
service_main(DWORD dwArgc, LPTSTR * lpszArgv)
{
  DWORD dwWait;

  /*  register our service control handler: */
  sshStatusHandle = RegisterServiceCtrlHandler(THIS_SERVICE, service_ctrl);

  if (!sshStatusHandle)
    goto cleanup;

  /*  SERVICE_STATUS members that don't change in example */
  ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  ssStatus.dwServiceSpecificExitCode = 0;


  /*  report the status to Service Control Manager. */
  if (!ReportStatusToSCMgr(SERVICE_START_PENDING,       /*  service state */
                           NO_ERROR,    /*  exit code */
                           1,   /*  checkpoint */
                           3000))       /*  wait hint */
    goto cleanup;


  /*  start the thread that performs the work of the service. */
  threadHandle = (HANDLE) _beginthreadex(NULL,  /*  security attributes */
                                         0,     /*  stack size (0 means inherit parent's stack size) */
                                         (LPTHREAD_START_ROUTINE) worker_thread, NULL,  /*  argument to thread */
                                         0,     /*  thread creation flags */
                                         &TID); /*  pointer to thread ID */

  if (!threadHandle)
    goto cleanup;

  /*  report the status to the service control manager. */
  if (!ReportStatusToSCMgr(SERVICE_RUNNING,     /*  service state */
                           NO_ERROR,    /*  exit code */
                           0,   /*  checkpoint */
                           0))  /*  wait hint */
    goto cleanup;

  /*  wait indefinitely until threadHandle is signaled. */
  /*  The thread handle is signalled when the thread terminates */

  dwWait = WaitForSingleObject(threadHandle,    /*  event object */
                               INFINITE);       /*  wait indefinitely */

cleanup:

  /*  try to report the stopped status to the service control manager. */
  if (sshStatusHandle)
    (VOID) ReportStatusToSCMgr(SERVICE_STOPPED, dwGlobalErr, 0, 0);

  /*  When SERVICE MAIN FUNCTION returns in a single service */
  /*  process, the StartServiceCtrlDispatcher function in */
  /*  the main thread returns, terminating the process. */
  return;
}                               /*  end of service_main */



/*   service_ctrl() -- */
/*       this function is called by the Service Controller whenever */
/*       someone calls ControlService in reference to our service. */
static VOID WINAPI
service_ctrl(DWORD dwCtrlCode)
{
  DWORD dwState = SERVICE_RUNNING;

  /*  Handle the requested control code. */
  switch (dwCtrlCode) {

    /*  Pause the service if it is running. */
  case SERVICE_CONTROL_PAUSE:

    if (ssStatus.dwCurrentState == SERVICE_RUNNING) {
      SuspendThread(threadHandle);
      dwState = SERVICE_PAUSED;
    }
    break;

    /*  Resume the paused service. */
  case SERVICE_CONTROL_CONTINUE:

    if (ssStatus.dwCurrentState == SERVICE_PAUSED) {
      ResumeThread(threadHandle);
      dwState = SERVICE_RUNNING;
    }
    break;

    /*  Stop the service. */
  case SERVICE_CONTROL_STOP:

    dwState = SERVICE_STOP_PENDING;

    /*  Report the status, specifying the checkpoint and waithint, */
    /*   before setting the termination event. */
    ReportStatusToSCMgr(SERVICE_STOP_PENDING,   /*  current state */
                        NO_ERROR,       /*  exit code */
                        1,      /*  checkpoint */
                        10000); /*  waithint (10 secs) */

    shutdown_flag = 1;

    flag_broadcast(0, 0, T("GAME: Game shutdown by system operator"));

    return;

    /*  Update the service status. */
  case SERVICE_CONTROL_INTERROGATE:
    break;

    /*  invalid control code */
  default:
    break;

  }                             /*  end of switch */

  /*  send a status response. */
  ReportStatusToSCMgr(dwState, NO_ERROR, 0, 0);
}                               /*  end of service_ctrl */


/*  utility functions... */



/*  ReportStatusToSCMgr() -- */
/*       This function is called by the ServMainFunc() and */
/*       ServCtrlHandler() functions to update the service's status */
/*       to the service control manager. */
static BOOL
ReportStatusToSCMgr(DWORD dwCurrentState,
                    DWORD dwWin32ExitCode, DWORD dwCheckPoint, DWORD dwWaitHint)
{
  BOOL fResult;

  /*  Disable control requests until the service is started. */
  if (dwCurrentState == SERVICE_START_PENDING)
    ssStatus.dwControlsAccepted = 0;
  else
    ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
      SERVICE_ACCEPT_PAUSE_CONTINUE;

  /*  These SERVICE_STATUS members are set from parameters. */
  ssStatus.dwCurrentState = dwCurrentState;
  ssStatus.dwWin32ExitCode = dwWin32ExitCode;
  ssStatus.dwCheckPoint = dwCheckPoint;

  ssStatus.dwWaitHint = dwWaitHint;

  /*  Report the status of the service to the service control manager. */
  if (!(fResult = SetServiceStatus(sshStatusHandle,     /*  service reference handle */
                                   &ssStatus))) {       /*  SERVICE_STATUS structure */

    /*  If an error occurs, stop the service. */
    StopService("SetServiceStatus");
  }
  return fResult;
}                               /*  end of ReportStatusToSCMgr */



/*  The StopService function can be used by any thread to report an */
/*   error, or stop the service. */
static VOID
StopService(LPTSTR lpszMsg)
{
  CHAR chMsg[256];
  HANDLE hEventSource;
  LPTSTR lpszStrings[2];

  dwGlobalErr = GetLastError();

  /*  Use event logging to log the error. */
  hEventSource = RegisterEventSource(NULL, THIS_SERVICE);

  sprintf(chMsg, "%s error: %s", THIS_SERVICE, convert_error(dwGlobalErr));
  lpszStrings[0] = chMsg;
  lpszStrings[1] = lpszMsg;

  if (hEventSource) {
    ReportEvent(hEventSource,   /*  handle of event source */
                EVENTLOG_ERROR_TYPE,    /*  event type */
                0,              /*  event category */
                0,              /*  event ID */
                NULL,           /*  current user's SID */
                2,              /*  strings in lpszStrings */
                0,              /*  no bytes of raw data */
                lpszStrings,    /*  array of error strings */
                NULL);          /*  no raw data */

    (VOID) DeregisterEventSource(hEventSource);
  }
  if (threadHandle)
    TerminateThread(threadHandle, 1);
}                               /*  end of StopService */


/*  called at shutdown, ctrl-c etc. */

BOOL WINAPI
shut_down_handler(DWORD dwCtrlType)
{


  if (dwCtrlType != CTRL_LOGOFF_EVENT) {

    if (threadHandle)
      TerminateThread(threadHandle, 1);
    threadHandle = NULL;

    mush_panic("System shutdown by system operator");

    _exit(99);

  }
  return FALSE;
}                               /*  end of  shut_down_handler */


/*
   This is the service "worker" thread (as opposed to the main thread which
   has called StartServiceCtrlDispatcher and is waiting on the worker thread
   to end, by waiting on the thread handle).

   The other threads are the timer thread (set up in timer.C) and the service
   control thread (service_ctrl), which is called by the service controller.

   If not running as a service, then this is not a separate thread, but is
   called directly from "main". (In this case, threadHandle will be zero).

   All this routine does is change directory to the same directory as the
   executable file, set up MUSH.CFG as the configuration file, and then
   call the "real" MUSH "main" routine in BSD.C.

   All this is designed to hide the service control stuff from the main MUSH
   code so as to make implementing the next version much easier.

 */

static VOID
worker_thread(VOID * notused)
{
  int argc = 3;
  char fullfilename[MAX_PATH];
  char directory[MAX_PATH];
  char configname[] = "mush.cnf";
  char errorlogname[] = "log\\game.log";
  char *argv[3] = { fullfilename, configname, errorlogname };
  char *p;

  if (!GetModuleFileName(NULL, fullfilename, sizeof(fullfilename))) {
    service_error(GetLastError(), "Cannot locate full filename");
    Win32_Exit(1);
  }
/*  remove last part of file name to get working directory */

  strcpy(directory, fullfilename);

  p = strrchr(directory, '\\');
  if (p)
    *p = 0;

/*  make sure we are running in the MUSH directory */

  _chdir(directory);

/*  if running as a service, redirect stderr to a log file. */

  if (threadHandle)
    freopen("log\\game.log", "w", stderr);

/*  handle shutdowns and ctrl-c */

  SetConsoleCtrlHandler(shut_down_handler, TRUE);

/*  start up the main MUSH code */

  exit(mainthread(argc, argv));

}                               /*  end of worker_thread */

void WIN32_CDECL
Win32_Exit(int exit_code)
{

/*  if running as a thread, end the thread, otherwise just exit */

  fflush(stderr);
  if (threadHandle)
    _endthread();
  else
    _exit(exit_code);

}                               /*  end of Win32_Exit */

/*  this is called from db_write (every 256 objects) */
/*  to keep the service manager happy (it needs a checkpoint every 3 seconds) */

void
shutdown_checkpoint(void)
{
  static DWORD checkpoint = 1;

  if (threadHandle && shutdown_flag)
    ReportStatusToSCMgr(SERVICE_STOP_PENDING,   /*  current state */
                        NO_ERROR,       /*  exit code */
                        ++checkpoint,   /*  checkpoint */
                        3000);  /*  waithint  (3 seconds) */

}                               /*  end of shutdown_checkpoint */


/*  We need to close these handles so often I'll do it in a separate routine */

static void
close_service_handles(void)
{
  if (service)
    CloseServiceHandle(service);
  service = NULL;
  if (SCmanager)
    CloseServiceHandle(SCmanager);
  SCmanager = NULL;
}                               /*  end of close_service_handles */


/*  We put out *so* many error messages, let's centralise the whole thing */

int WIN32_CDECL
service_error(DWORD error_code, char *themessage, ...)
{
  va_list arglist;

  char buff[200];

/* print the message as if it was a PRINTF type message */

  va_start(arglist, themessage);
  _vsnprintf(buff, sizeof(buff), themessage, arglist);
  va_end(arglist);

  fprintf(stderr, "%s\n", buff);

  if (error_code)
    fprintf(stderr, "  ** Error %ld\n  ** %s\n",
            error_code, convert_error(error_code));

  close_service_handles();

  return TRUE;
}                               /*  end of service_error */

/*
   Open a handle to the Service Control Manager.
 */

static int
open_service_manager(void)
{

  SCmanager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

  if (!SCmanager)
    return service_error(GetLastError(),
                         "Unable to talk to the Service Control Manager");

  return FALSE;

}                               /*  end of openServiceManager */


/*
   Open a handle to the Service.
 */
static int
get_service(void)
{
  service = OpenService(SCmanager, THIS_SERVICE, SERVICE_ALL_ACCESS);

  if (!service)
    return service_error(GetLastError(), "Cannot access service definition");

  return FALSE;
}                               /*  end of get_service */

/*
   Opens the service manager and gets the status, optionally leaving
   the manager open.
 */

static DWORD
get_service_status(SERVICE_STATUS * svcstatus, int leave_open)
{

/*
   Open a handle to the Service Control Manager.
 */

  SCmanager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

  if (!SCmanager) {
    close_service_handles();
    return GetLastError();
  }
/*
   Open a handle to the Service.
 */

  service = OpenService(SCmanager, THIS_SERVICE, SERVICE_ALL_ACCESS);

  if (!service) {
    close_service_handles();
    return GetLastError();
  }
/*
   Check to see that the service is not running.
 */

  if (!QueryServiceStatus(service, svcstatus)) {
    close_service_handles();
    return GetLastError();
  }
/*  leave handles open if requested */

  if (!leave_open)
    close_service_handles();

  return 0;

}                               /*  end of get_service_status */

/*
   Install this service.
 */

static int
CmdInstallService(int argc, char *argv[])
{
  char fullfilename[MAX_PATH];

/*
   Pick up our full path and file name.
 */

  if (!GetModuleFileName(NULL, fullfilename, sizeof(fullfilename)))
    return service_error(GetLastError(), "Cannot locate full filename");

/*
   Open a handle to the Service Control Manager.
 */

  if (open_service_manager())
    return TRUE;

/*
   Now create the service definition.
 */

  service = CreateService(SCmanager, THIS_SERVICE, THIS_SERVICE_DISPLAY, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, fullfilename, NULL,   /*  no load ordering group */
                          NULL, /*  no tag identifier */
                          NULL, /*  no dependencies */
                          NULL, /*  LocalSystem account */
                          NULL);        /*  no password */

  if (!service)
    return service_error(GetLastError(), "Unable to create service");

  close_service_handles();

  fprintf(stderr, "Service successfully installed\n");

  return FALSE;
}                               /*  end of CmdInstallService */

/*
   Remove this service.
 */

static int
CmdRemoveService(void)
{
  SERVICE_STATUS svcstatus;
  DWORD status;

/*
   Open the service manager and find its status
 */

  if (status = get_service_status(&svcstatus, TRUE))
    return service_error(status, "Unable to access service details");

/*
   Check to see that the service is not running.
 */

  if (svcstatus.dwCurrentState != SERVICE_STOPPED)
    return service_error(0,
                         "You must stop the service before you can remove it.");

/*
   Everything is fine, so delete the service definition.
 */

  if (!DeleteService(service))
    return service_error(GetLastError(), "Cannot remove service");

  close_service_handles();

  fprintf(stderr, "Service successfully removed\n");

  return FALSE;
}                               /*  end of CmdRemoveService */


/*
   Start this service.
 */

static int
CmdStartService(void)
{
  SERVICE_STATUS svcstatus;
  DWORD status;

/*
   Open the service manager and find its status
 */

  if (status = get_service_status(&svcstatus, TRUE))
    return service_error(status, "Unable to access service details");

  if (svcstatus.dwCurrentState != SERVICE_STOPPED)
    return service_error(0, "The service is not currently stopped.");

/*
   Everything is fine, so start the service
 */

  if (!StartService(service, 0, NULL))
    return service_error(GetLastError(), "Cannot start service");

  close_service_handles();

  fprintf(stderr, "Start request sent to service\n");

  return FALSE;
}                               /*  end of CmdStartService */

/*
   Stop this service.
 */

static int
CmdStopService(void)
{
  SERVICE_STATUS svcstatus;
  DWORD status;

/*
   Open the service manager and find its status
 */

  if (status = get_service_status(&svcstatus, TRUE))
    return service_error(status, "Unable to access service details");

  if (svcstatus.dwCurrentState != SERVICE_RUNNING)
    return service_error(0, "The service is not currently running.");

/*
   Everything is fine, so stop the service
 */

  if (!ControlService(service, SERVICE_CONTROL_STOP, &svcstatus))
    return service_error(GetLastError(), "Cannot stop service");

  close_service_handles();

  fprintf(stderr, "Stop request sent to service\n");

  return FALSE;
}                               /*  end of CmdStopService */


/*
   Show status of this service.
 */

static int
CmdStatusService(void)
{
  SERVICE_STATUS svcstatus;
  DWORD status;
  char *p;

/*
   Open the service manager and find its status
 */

  if (status = get_service_status(&svcstatus, FALSE))
    return service_error(status, "Unable to access service details");

  switch (svcstatus.dwCurrentState) {
  case SERVICE_STOPPED:
    p = "The service is not running.";
    break;
  case SERVICE_START_PENDING:
    p = "The service is starting.";
    break;
  case SERVICE_STOP_PENDING:
    p = "The service is stopping.";
    break;
  case SERVICE_RUNNING:
    p = "The service is running.";
    break;
  case SERVICE_CONTINUE_PENDING:
    p = "The service continue is pending.";
    break;
  case SERVICE_PAUSE_PENDING:
    p = "The service pause is pending.";
    break;
  case SERVICE_PAUSED:
    p = "The service is paused.";
    break;
  default:
    p = "Unrecognised status.";
    break;
  }                             /*  end of switch */

  fprintf(stderr, "%s\n", p);

  return FALSE;
}                               /*  end of CmdStatusService */



/*
   Display the available commands.
 */

static void
CmdDisplayFormat(void)
{
  fprintf(stderr, "Usage is :-\n");
  fprintf(stderr, " %s           - runs as a service, or stand-alone\n",
          THIS_SERVICE);
  fprintf(stderr, " %s /run      - runs stand-alone\n", THIS_SERVICE);
  fprintf(stderr, " %s /start    - starts this service\n", THIS_SERVICE);
  fprintf(stderr, " %s /stop     - stops this service\n", THIS_SERVICE);
  fprintf(stderr, " %s /install  - installs this service\n", THIS_SERVICE);
  fprintf(stderr, " %s /remove   - removes (un-installs) this service\n",
          THIS_SERVICE);
  fprintf(stderr, " %s /status   - displays the status of this service\n",
          THIS_SERVICE);
  fprintf(stderr, " %s /help     - displays this information\n", THIS_SERVICE);
}                               /*  end of CmdDisplayFormat */

static char *
convert_error(DWORD error)
{

  char *formattedmsg;
  static char buff[100];

  if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_ALLOCATE_BUFFER |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL,
                     error, LANG_NEUTRAL, (LPTSTR) & formattedmsg, 0, NULL)) {
    sprintf(buff, "<Error code: %ld>", error);
    return buff;
  } else
    return formattedmsg;
}                               /*  end of convert_error */


#endif                          /* WIN32SERVICES */
#endif                          /* WIN32 */

static void dummy(void) __attribute__ ((__unused__));
static void
dummy(void)
{
  /* This function exists solely to supress a warning on non-Windows
     systems about an empty source file. */
}
