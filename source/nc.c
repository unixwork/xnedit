/*******************************************************************************
*									       *
* nc.c -- Nirvana Editor client program for nedit server processes	       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
* 									       *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.							       *
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* November, 1995							       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "server_common.h"
#include "../util/fileUtils.h"
#include "../util/utils.h"
#include "../util/prefFile.h"
#include "../util/system.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#ifdef VMS
#include <lib$routines.h>
#include descrip
#include ssdef
#include syidef
#include "../util/VMSparam.h"
#include "../util/VMSutils.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pwd.h>
#include "../util/clearcase.h"
#endif /* VMS */
#ifdef __EMX__
#include <process.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/Xatom.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define APP_NAME "nc"
#define APP_CLASS "NEditClient"

#define PROPERTY_CHANGE_TIMEOUT (Preferences.timeOut * 1000) /* milliseconds */
#define SERVER_START_TIMEOUT    (Preferences.timeOut * 3000) /* milliseconds */
#define REQUEST_TIMEOUT         (Preferences.timeOut * 1000) /* milliseconds */
#define FILE_OPEN_TIMEOUT       (Preferences.timeOut * 3000) /* milliseconds */

typedef struct
{
   char* shell;
   char* serverRequest;
} CommandLine;

static void timeOutProc(Boolean *timeOutReturn, XtIntervalId *id);
static int startServer(const char *message, const char *commandLine);
static CommandLine processCommandLine(int argc, char** argv);
static void parseCommandLine(int argc, char **arg, CommandLine *cmdLine);
static void nextArg(int argc, char **argv, int *argIndex);
static void copyCommandLineArg(CommandLine *cmdLine, const char *arg);
static void printNcVersion(void);
static Boolean findExistingServer(XtAppContext context,
                                  Window rootWindow,
                                  Atom serverExistsAtom);
static void startNewServer(XtAppContext context,
                           Window rootWindow,
                           char* commandLine,
                           Atom serverExistsAtom);
static void waitUntilRequestProcessed(XtAppContext context,
                                      Window rootWindow,
                                      char* commandString,
                                      Atom serverRequestAtom);
static void waitUntilFilesOpenedOrClosed(XtAppContext context,
                                         Window rootWindow);

Display *TheDisplay;
XtAppContext AppContext;
static Atom currentWaitForAtom;
static Atom noAtom = (Atom)(-1);

static const char cmdLineHelp[] =
#ifdef VMS
"[Sorry, no on-line help available.]\n"; /* Why is that ? */
#else
"Usage:  nc [-read] [-create]\n"
"           [-line n | +n] [-do command] [-lm languagemode]\n"
"           [-svrname name] [-svrcmd command]\n"
"           [-ask] [-noask] [-timeout seconds]\n"
"           [-geometry geometry | -g geometry] [-icon | -iconic]\n"
"           [-tabbed] [-untabbed] [-group] [-wait]\n"
"           [-V | -version] [-h|-help]\n"
"           [-xrm resourcestring] [-display [host]:server[.screen]]\n"
"           [--] [file...]\n";
#endif /*VMS*/

/* Structure to hold X Resource values */
static struct {
    int autoStart;
    char serverCmd[2*MAXPATHLEN]; /* holds executable name + flags */
    char serverName[MAXPATHLEN];
    int waitForClose;
    int timeOut;
} Preferences;

/* Application resources */
static PrefDescripRec PrefDescrip[] = {
    {"autoStart", "AutoStart", PREF_BOOLEAN, "True",
      &Preferences.autoStart, NULL, True},
    {"serverCommand", "ServerCommand", PREF_STRING, "nedit -server",
      Preferences.serverCmd, (void *)sizeof(Preferences.serverCmd), False},
    {"serverName", "serverName", PREF_STRING, "", Preferences.serverName,
      (void *)sizeof(Preferences.serverName), False},
    {"waitForClose", "WaitForClose", PREF_BOOLEAN, "False",
      &Preferences.waitForClose, NULL, False},
    {"timeOut", "TimeOut", PREF_INT, "10",
      &Preferences.timeOut, NULL, False}
};

/* Resource related command line options */
static XrmOptionDescRec OpTable[] = {
    {"-ask", ".autoStart", XrmoptionNoArg, (caddr_t)"False"},
    {"-noask", ".autoStart", XrmoptionNoArg, (caddr_t)"True"},
    {"-svrname", ".serverName", XrmoptionSepArg, (caddr_t)NULL},
    {"-svrcmd", ".serverCommand", XrmoptionSepArg, (caddr_t)NULL},
    {"-wait", ".waitForClose", XrmoptionNoArg, (caddr_t)"True"},
    {"-timeout", ".timeOut", XrmoptionSepArg, (caddr_t)NULL}
};

/* Struct to hold info about files being opened and edited. */
typedef struct _FileListEntry {
    Atom                  waitForFileOpenAtom;
    Atom                  waitForFileClosedAtom;
    char*                 path;
    struct _FileListEntry *next;
} FileListEntry;

typedef struct {
    int            waitForOpenCount;
    int            waitForCloseCount;
    FileListEntry* fileList;
} FileListHead;
static FileListHead fileListHead;

static void setPropertyValue(Atom atom) {
    XChangeProperty(TheDisplay,
	            RootWindow(TheDisplay, DefaultScreen(TheDisplay)),
	            atom, XA_STRING,
	            8, PropModeReplace,
	            (unsigned char *)"True", 4);
}

/* Add another entry to the file entry list, if it doesn't exist yet. */
static void addToFileList(const char *path)
{
    FileListEntry *item;
    
    /* see if the file already exists in the list */
    for (item = fileListHead.fileList; item; item = item->next) {
        if (!strcmp(item->path, path))
           break;
    }

    /* Add the atom to the head of the file list if it wasn't found. */
    if (item == 0) {
        item = malloc(sizeof(item[0]));
        item->waitForFileOpenAtom = None;
        item->waitForFileClosedAtom = None;
        item->path = (char*)malloc(strlen(path)+1);
        strcpy(item->path, path);
        item->next = fileListHead.fileList;
        fileListHead.fileList = item;
    }
}

/* Creates the properties for the various paths */
static void createWaitProperties(void)
{
    FileListEntry *item;

    for (item = fileListHead.fileList; item; item = item->next) {
        fileListHead.waitForOpenCount++;
        item->waitForFileOpenAtom = 
            CreateServerFileOpenAtom(Preferences.serverName, item->path);
        setPropertyValue(item->waitForFileOpenAtom);

        if (Preferences.waitForClose == True) {
            fileListHead.waitForCloseCount++;
            item->waitForFileClosedAtom =
                      CreateServerFileClosedAtom(Preferences.serverName,
                                                 item->path,
                                                 False);
            setPropertyValue(item->waitForFileClosedAtom);
        }
    }
}

int main(int argc, char **argv)
{
    XtAppContext context;
    Window rootWindow;
    CommandLine commandLine;
    Atom serverExistsAtom, serverRequestAtom;
    XrmDatabase prefDB;
    Boolean serverExists;

    /* Initialize toolkit and get an application context */
    XtToolkitInitialize();
    AppContext = context = XtCreateApplicationContext();
    
#ifdef VMS
    /* Convert the command line to Unix style */
    ConvertVMSCommandLine(&argc, &argv);
#endif /*VMS*/
#ifdef __EMX__
    /* expand wildcards if necessary */
    _wildcard(&argc, &argv);
#endif
    
    /* Read the preferences command line into a database (note that we
       don't support the .nc file anymore) */
    prefDB = CreatePreferencesDatabase(NULL, APP_CLASS, 
	    OpTable, XtNumber(OpTable), (unsigned *)&argc, argv);
    
    /* Process the command line before calling XtOpenDisplay, because the
       latter consumes certain command line arguments that we still need
       (-icon, -geometry ...) */
    commandLine = processCommandLine(argc, argv);
        
    /* Open the display and find the root window */
    TheDisplay = XtOpenDisplay (context, NULL, APP_NAME, APP_CLASS, NULL,
    	    0, &argc, argv);
    if (!TheDisplay) {
	XtWarning ("nc: Can't open display\n");
	exit(EXIT_FAILURE);
    }
    rootWindow = RootWindow(TheDisplay, DefaultScreen(TheDisplay));
    
    /* Read the application resources into the Preferences data structure */
    RestorePreferences(prefDB, XtDatabase(TheDisplay), APP_NAME,
    	    APP_CLASS, PrefDescrip, XtNumber(PrefDescrip));
    
    /* Make sure that the time out unit is at least 1 second and not too
       large either (overflow!). */
    if (Preferences.timeOut < 1) {
	Preferences.timeOut = 1;
    } else if (Preferences.timeOut > 1000) {
	Preferences.timeOut = 1000;
    }
    
#ifndef VMS
    /* For Clearcase users who have not set a server name, use the clearcase
       view name.  Clearcase views make files with the same absolute path names
       but different contents (and therefore can't be edited in the same nedit
       session). This should have no bad side-effects for non-clearcase users */
    if (Preferences.serverName[0] == '\0') {
        const char* viewTag = GetClearCaseViewTag();
        if (viewTag != NULL && strlen(viewTag) < MAXPATHLEN) {
            strcpy(Preferences.serverName, viewTag);
        }
    }
#endif /* VMS */
    
    /* Create the wait properties for the various files. */
    createWaitProperties();
        
    /* Monitor the properties on the root window */
    XSelectInput(TheDisplay, rootWindow, PropertyChangeMask);
	
    /* Create the server property atoms on the current DISPLAY. */
    CreateServerPropertyAtoms(Preferences.serverName,
                              &serverExistsAtom,
                              &serverRequestAtom);

    serverExists = findExistingServer(context,
                                      rootWindow,
                                      serverExistsAtom);

    if (serverExists == False)
        startNewServer(context, rootWindow, commandLine.shell, serverExistsAtom);

    waitUntilRequestProcessed(context,
                              rootWindow,
                              commandLine.serverRequest,
                              serverRequestAtom);

    waitUntilFilesOpenedOrClosed(context, rootWindow);

    XtCloseDisplay(TheDisplay);
    XtFree(commandLine.shell);
    XtFree(commandLine.serverRequest);
    return 0;
}


/*
** Xt timer procedure for timeouts on NEdit server requests
*/
static void timeOutProc(Boolean *timeOutReturn, XtIntervalId *id)
{
   /* NOTE: XtAppNextEvent() does call this routine but
   ** doesn't return unless there are more events.
   ** Hence, we generate this (synthetic) event to break the deadlock
   */
    Window rootWindow = RootWindow(TheDisplay, DefaultScreen(TheDisplay));
    if (currentWaitForAtom != noAtom) {
	XChangeProperty(TheDisplay, rootWindow, currentWaitForAtom, XA_STRING, 
	    8, PropModeReplace, (unsigned char *)"", strlen(""));
    }

    /* Flag that the timeout has occurred. */
    *timeOutReturn = True;
}



static Boolean findExistingServer(XtAppContext context,
                                  Window rootWindow,
                                  Atom serverExistsAtom)
{
    Boolean serverExists = True;
    unsigned char *propValue;
    int getFmt;
    Atom dummyAtom;
    unsigned long dummyULong, nItems;

    /* See if there might be a server (not a guaranty), by translating the
       root window property NEDIT_SERVER_EXISTS_<user>_<host> */
    if (XGetWindowProperty(TheDisplay, rootWindow, serverExistsAtom, 0,
    	    INT_MAX, False, XA_STRING, &dummyAtom, &getFmt, &nItems,
    	    &dummyULong, &propValue) != Success || nItems == 0) {
        serverExists = False;
    } else {
        Boolean timeOut = False;
        XtIntervalId timerId;

        XFree(propValue);

        /* Remove the server exists property to make sure the server is
        ** running. If it is running it will get recreated.
        */
        XDeleteProperty(TheDisplay, rootWindow, serverExistsAtom);
        XSync(TheDisplay, False);
        timerId = XtAppAddTimeOut(context,
                                  PROPERTY_CHANGE_TIMEOUT,
                                  (XtTimerCallbackProc)timeOutProc,
                                  &timeOut);
        currentWaitForAtom = serverExistsAtom;

        while (!timeOut) {
            /* NOTE: XtAppNextEvent() does call the timeout routine but
            ** doesn't return unless there are more events. */
            XEvent event;
            const XPropertyEvent *e = (const XPropertyEvent *)&event;
            XtAppNextEvent(context, &event);

            /* We will get a PropertyNewValue when the server recreates
            ** the server exists atom. */
            if (e->type == PropertyNotify &&
                e->window == rootWindow &&
                e->atom == serverExistsAtom) {
                if (e->state == PropertyNewValue) {
                    break;
                }
            }
            XtDispatchEvent(&event);
        }

        /* Start a new server if the timeout expired. The server exists
        ** property was not recreated. */
        if (timeOut) {
            serverExists = False;
        } else {
            XtRemoveTimeOut(timerId);
        }
    }
    
    return(serverExists);
} 




static void startNewServer(XtAppContext context,
                           Window rootWindow,
                           char* commandLine,
                           Atom serverExistsAtom)
{
    Boolean timeOut = False;
    XtIntervalId timerId;

    /* Add back the server name resource from the command line or resource
       database to the command line for starting the server.  If -svrcmd
       appeared on the original command line, it was removed by
       CreatePreferencesDatabase before the command line was recorded
       in commandLine.shell. Moreover, if no server name was specified, it
       may have defaulted to the ClearCase view tag. */
    if (Preferences.serverName[0] != '\0') {
        strcat(commandLine, " -svrname ");
        strcat(commandLine, Preferences.serverName);
    }
    switch (startServer("No servers available, start one? (y|n) [y]: ",
                        commandLine))
    {
        case -1: /* Start failed */
            XtCloseDisplay(TheDisplay);
            exit(EXIT_FAILURE);
            break;
        case -2: /* Start canceled by user */
            XtCloseDisplay(TheDisplay);
            exit(EXIT_SUCCESS);
            break;
    }

    /* Set up a timeout proc in case the server is dead.  The standard
       selection timeout is probably a good guess at how long to wait
       for this style of inter-client communication as well */
    timerId = XtAppAddTimeOut(context,
                              SERVER_START_TIMEOUT,
                              (XtTimerCallbackProc)timeOutProc,
                              &timeOut);
    currentWaitForAtom = serverExistsAtom;

    /* Wait for the server to start */
    while (!timeOut) {
        XEvent event;
        const XPropertyEvent *e = (const XPropertyEvent *)&event;
        /* NOTE: XtAppNextEvent() does call the timeout routine but
        ** doesn't return unless there are more events. */
        XtAppNextEvent(context, &event);

        /* We will get a PropertyNewValue when the server updates
        ** the server exists atom. If the property is deleted the
        ** the server must have died. */
        if (e->type == PropertyNotify &&
            e->window == rootWindow &&
            e->atom == serverExistsAtom) {
            if (e->state == PropertyNewValue) {
                break;
            } else if (e->state == PropertyDelete) {
                fprintf(stderr, "%s: The server failed to start.\n", APP_NAME);
                XtCloseDisplay(TheDisplay);
                exit(EXIT_FAILURE);
            }
        }
        XtDispatchEvent(&event);
    }
    /* Exit if the timeout expired. */
    if (timeOut) {
        fprintf(stderr, "%s: The server failed to start (time-out).\n", APP_NAME);
        XtCloseDisplay(TheDisplay);
        exit(EXIT_FAILURE);
    } else {
        XtRemoveTimeOut(timerId);
    }
}

/*
** Prompt the user about starting a server, with "message", then start server
*/
static int startServer(const char *message, const char *commandLineArgs)
{
    char c, *commandLine;
#ifdef VMS
    int spawnFlags = 1 /* + 1<<3 */;			/* NOWAIT, NOKEYPAD */
    int spawn_sts;
    struct dsc$descriptor_s *cmdDesc;
    char *nulDev = "NL:";
    struct dsc$descriptor_s *nulDevDesc;
#else
    int sysrc;
#endif /* !VMS */
    
    /* prompt user whether to start server */
    if (!Preferences.autoStart) {
	printf("%s", message);
	do {
    	    c = getc(stdin);
	} while (c == ' ' || c == '\t');
	if (c != 'Y' && c != 'y' && c != '\n')
    	    return (-2);
    }
    
    /* start the server */
#ifdef VMS
    commandLine = XtMalloc(strlen(Preferences.serverCmd) +
    	    strlen(commandLineArgs) + 3);
    sprintf(commandLine, "%s %s", Preferences.serverCmd, commandLineArgs);
    cmdDesc = NulStrToDesc(commandLine);	/* build command descriptor */
    nulDevDesc = NulStrToDesc(nulDev);		/* build "NL:" descriptor */
    spawn_sts = lib$spawn(cmdDesc, nulDevDesc, 0, &spawnFlags, 0,0,0,0,0,0,0,0);
    XtFree(commandLine);
    if (spawn_sts != SS$_NORMAL) {
	fprintf(stderr, "Error return from lib$spawn: %d\n", spawn_sts);
	fprintf(stderr, "Nedit server not started.\n");
	return (-1);
    } else {
       FreeStrDesc(cmdDesc);
       return 0;
    }
#else
#if defined(__EMX__)  /* OS/2 */
    /* Unfortunately system() calls a shell determined by the environment
       variables COMSPEC and EMXSHELL. We have to figure out which one. */
    {
    char *sh_spec, *sh, *base;
    char *CMD_EXE="cmd.exe";

    commandLine = XtMalloc(strlen(Preferences.serverCmd) +
	   strlen(commandLineArgs) + 15);
    sh = getenv ("EMXSHELL");
    if (sh == NULL)
      sh = getenv ("COMSPEC");
    if (sh == NULL)
      sh = CMD_EXE;
    sh_spec=XtNewString(sh);
    base=_getname(sh_spec);
    _remext(base);
    if (stricmp (base, "cmd") == 0 || stricmp (base, "4os2") == 0) {
       sprintf(commandLine, "start /C /MIN %s %s",
               Preferences.serverCmd, commandLineArgs);
    } else {
       sprintf(commandLine, "%s %s &", 
               Preferences.serverCmd, commandLineArgs);
    }
    XtFree(sh_spec);
    }
#else /* Unix */
    commandLine = XtMalloc(strlen(Preferences.serverCmd) +
    	    strlen(commandLineArgs) + 3);
    sprintf(commandLine, "%s %s&", Preferences.serverCmd, commandLineArgs);
#endif

    sysrc=system(commandLine);
    XtFree(commandLine);
    
    if (sysrc==0)
       return 0;
    else
       return (-1);
#endif /* !VMS */
}

/* Reconstruct the command line in string commandLine in case we have to
 * start a server (nc command line args parallel nedit's).  Include
 * -svrname if nc wants a named server, so nedit will match. Special
 * characters are protected from the shell by escaping EVERYTHING with \
 */
static CommandLine processCommandLine(int argc, char** argv)
{
    CommandLine commandLine;
    int i;
    int length = 0;
   
    for (i=1; i<argc; i++) {
    	length += 1 + strlen(argv[i])*4 + 2;
    }
    commandLine.shell = XtMalloc(length+1 + 9 + MAXPATHLEN);
    *commandLine.shell = '\0';

    /* Convert command line arguments into a command string for the server */
    parseCommandLine(argc, argv, &commandLine);
    if (commandLine.serverRequest == NULL) {
        fprintf(stderr, "nc: Invalid commandline argument\n");
	exit(EXIT_FAILURE);
    }

    return(commandLine);
} 


/*
** Converts command line into a command string suitable for passing to
** the server
*/
static void parseCommandLine(int argc, char **argv, CommandLine *commandLine)
{
#define MAX_RECORD_HEADER_LENGTH 38
    char name[MAXPATHLEN], path[MAXPATHLEN];
    const char *toDoCommand = "", *langMode = "", *geometry = "";
    char *commandString, *outPtr;
    int lineNum = 0, read = 0, create = 0, iconic = 0, tabbed = -1, length = 0;
    int i, lineArg, nRead, charsWritten, opts = True;
    int fileCount = 0, group = 0, isTabbed;

    /* Allocate a string for output, for the maximum possible length.  The
       maximum length is calculated by assuming every argument is a file,
       and a complete record of maximum length is created for it */
    for (i=1; i<argc; i++) {
    	length += MAX_RECORD_HEADER_LENGTH + strlen(argv[i]) + MAXPATHLEN;
    }
    /* In case of no arguments, must still allocate space for one record header */
    if (length < MAX_RECORD_HEADER_LENGTH)
    {
       length = MAX_RECORD_HEADER_LENGTH;
    }
    commandString = XtMalloc(length+1);
    
    /* Parse the arguments and write the output string */
    outPtr = commandString;
    for (i=1; i<argc; i++) {
        if (opts && !strcmp(argv[i], "--")) { 
    	    opts = False; /* treat all remaining arguments as filenames */
	    continue;
	} else if (opts && !strcmp(argv[i], "-do")) {
    	    nextArg(argc, argv, &i);
    	    toDoCommand = argv[i];
    	} else if (opts && !strcmp(argv[i], "-lm")) {
	    copyCommandLineArg(commandLine, argv[i]);
    	    nextArg(argc, argv, &i);
    	    langMode = argv[i];
	    copyCommandLineArg(commandLine, argv[i]);
    	} else if (opts && (!strcmp(argv[i], "-g")  || 
	                    !strcmp(argv[i], "-geometry"))) {
	    copyCommandLineArg(commandLine, argv[i]);
    	    nextArg(argc, argv, &i);
    	    geometry = argv[i];
	    copyCommandLineArg(commandLine, argv[i]);
    	} else if (opts && !strcmp(argv[i], "-read")) {
    	    read = 1;
    	} else if (opts && !strcmp(argv[i], "-create")) {
    	    create = 1;
    	} else if (opts && !strcmp(argv[i], "-tabbed")) {
    	    tabbed = 1;
    	    group = 0;	/* override -group option */
    	} else if (opts && !strcmp(argv[i], "-untabbed")) {
    	    tabbed = 0;
    	    group = 0;  /* override -group option */
    	} else if (opts && !strcmp(argv[i], "-group")) {
    	    group = 2; /* 2: start new group, 1: in group */
    	} else if (opts && (!strcmp(argv[i], "-iconic") || 
	                    !strcmp(argv[i], "-icon"))) {
    	    iconic = 1;
	    copyCommandLineArg(commandLine, argv[i]);
    	} else if (opts && !strcmp(argv[i], "-line")) {
    	    nextArg(argc, argv, &i);
	    nRead = sscanf(argv[i], "%d", &lineArg);
	    if (nRead != 1)
    		fprintf(stderr, "nc: argument to line should be a number\n");
    	    else
    	    	lineNum = lineArg;
    	} else if (opts && (*argv[i] == '+')) {
    	    nRead = sscanf((argv[i]+1), "%d", &lineArg);
	    if (nRead != 1)
    		fprintf(stderr, "nc: argument to + should be a number\n");
    	    else
    	    	lineNum = lineArg;
    	} else if (opts && (!strcmp(argv[i], "-ask") || !strcmp(argv[i], "-noask"))) {
    	    ; /* Ignore resource-based arguments which are processed later */
    	} else if (opts && (!strcmp(argv[i], "-svrname") || 
		            !strcmp(argv[i], "-svrcmd"))) {
    	    nextArg(argc, argv, &i); /* Ignore rsrc args with data */
    	} else if (opts && (!strcmp(argv[i], "-xrm") ||
	                    !strcmp(argv[i], "-display"))) {
	    copyCommandLineArg(commandLine, argv[i]);
    	    nextArg(argc, argv, &i); /* Ignore rsrc args with data */
	    copyCommandLineArg(commandLine, argv[i]);
    	} else if (opts && (!strcmp(argv[i], "-version") || !strcmp(argv[i], "-V"))) {
    	    printNcVersion();
	    exit(EXIT_SUCCESS);
	} else if (opts && (!strcmp(argv[i], "-h") ||
			    !strcmp(argv[i], "-help"))) {
	    fprintf(stderr, "%s", cmdLineHelp);
	    exit(EXIT_SUCCESS);
    	} else if (opts && (*argv[i] == '-')) {
#ifdef VMS
	    *argv[i] = '/';
#endif /*VMS*/
    	    fprintf(stderr, "nc: Unrecognized option %s\n%s", argv[i],
    	    	    cmdLineHelp);
    	    exit(EXIT_FAILURE);
    	} else {
#ifdef VMS
	    int numFiles, j, oldLength;
	    char **nameList = NULL, *newCommandString;
	    /* Use VMS's LIB$FILESCAN for filename in argv[i] to process */
	    /* wildcards and to obtain a full VMS file specification     */
	    numFiles = VMSFileScan(argv[i], &nameList, NULL, INCLUDE_FNF);
            /* Should we warn the user if he provided a -line or -do switch
               and a wildcard pattern that expands to more than one file?  */
	    /* for each expanded file name do: */
	    for (j = 0; j < numFiles; ++j) {
	    	oldLength = outPtr-commandString;
	    	newCommandString = XtMalloc(oldLength+length+1);
	    	strncpy(newCommandString, commandString, oldLength);
	    	XtFree(commandString);
	    	commandString = newCommandString;
	    	outPtr = newCommandString + oldLength;
	    	if (ParseFilename(nameList[j], name, path) != 0) {
	           /* An Error, most likely too long paths/strings given */
	           commandLine->serverRequest = NULL;
	           return;
		}
    		strcat(path, name);

		/* determine if file is to be openned in new tab, by
		   factoring the options -group, -tabbed & -untabbed */
    		if (group == 2) {
	            isTabbed = 0;  /* start a new window for new group */
		    group = 1;     /* next file will be within group */
		} 
		else if (group == 1) {
	    	    isTabbed = 1;  /* new tab for file in group */
		}
		else {
	    	    isTabbed = tabbed; /* not in group */
		}
	    
                /* See below for casts */
    		sprintf(outPtr, "%d %d %d %d %d %ld %ld %ld %ld\n%s\n%s\n%s\n%s\n%n",
			lineNum, read, create, iconic, tabbed, (long) strlen(path),
			(long) strlen(toDoCommand), (long) strlen(langMode), 
                        (long) strlen(geometry),
			path, toDoCommand, langMode, geometry, &charsWritten);
		outPtr += charsWritten;
		free(nameList[j]);

                /* Create the file open atoms for the paths supplied */
                addToFileList(path);
	        fileCount++;
	    }
	    if (nameList != NULL)
	    	free(nameList);
#else
    	    if (ParseFilename(argv[i], name, path) != 0) {
	       /* An Error, most likely too long paths/strings given */
	       commandLine->serverRequest = NULL;
	       return;
	    }
    	    strcat(path, name);
	    
	    /* determine if file is to be openned in new tab, by
	       factoring the options -group, -tabbed & -untabbed */
    	    if (group == 2) {
	        isTabbed = 0;  /* start a new window for new group */
		group = 1;     /* next file will be within group */
	    } 
	    else if (group == 1) {
	    	isTabbed = 1;  /* new tab for file in group */
	    }
	    else {
	    	isTabbed = tabbed; /* not in group */
	    }
	    
    	    /* SunOS 4 acc or acc and/or its runtime library has a bug
    	       such that %n fails (segv) if it follows a string in a
    	       printf or sprintf.  The silly code below avoids this.
               
               The "long" cast on strlen() is necessary because size_t
               is 64 bit on Alphas, and 32-bit on most others.  There is
               no printf format specifier for "size_t", thanx, ANSI. */
    	    sprintf(outPtr, "%d %d %d %d %d %ld %ld %ld %ld\n%n", lineNum,
		    read, create, iconic, isTabbed, (long) strlen(path), 
		    (long) strlen(toDoCommand), (long) strlen(langMode),
		    (long) strlen(geometry), &charsWritten);
    	    outPtr += charsWritten;
    	    strcpy(outPtr, path);
    	    outPtr += strlen(path);
    	    *outPtr++ = '\n';
    	    strcpy(outPtr, toDoCommand);
    	    outPtr += strlen(toDoCommand);
    	    *outPtr++ = '\n';
    	    strcpy(outPtr, langMode);
    	    outPtr += strlen(langMode);
    	    *outPtr++ = '\n';
    	    strcpy(outPtr, geometry);
    	    outPtr += strlen(geometry);
    	    *outPtr++ = '\n';

            /* Create the file open atoms for the paths supplied */
            addToFileList(path);
	    fileCount++;
#endif /* VMS */

            /* These switches only affect the next filename argument, not all */
            toDoCommand = "";
            lineNum = 0;
    	}
    }
#ifdef VMS
    VMSFileScanDone();
#endif /*VMS*/
    
    /* If there's an un-written -do command, or we are to open a new window,
     * or user has requested iconic state, but not provided a file name,
     * create a server request with an empty file name and requested
     * iconic state (and optional language mode and geometry).
     */
    if (toDoCommand[0] != '\0' || fileCount == 0) {
	sprintf(outPtr, "0 0 0 %d %d 0 %ld %ld %ld\n\n%n", iconic, tabbed,
		(long) strlen(toDoCommand),
		(long) strlen(langMode), (long) strlen(geometry), &charsWritten);
	outPtr += charsWritten;
	strcpy(outPtr, toDoCommand);
	outPtr += strlen(toDoCommand);
	*outPtr++ = '\n';
	strcpy(outPtr, langMode);
	outPtr += strlen(langMode);
	*outPtr++ = '\n';
	strcpy(outPtr, geometry);
	outPtr += strlen(geometry);
	*outPtr++ = '\n';
    }
    
    *outPtr = '\0';
    commandLine->serverRequest = commandString;
}


static void waitUntilRequestProcessed(XtAppContext context,
                                      Window rootWindow,
                                      char* commandString,
                                      Atom serverRequestAtom)
{
    XtIntervalId timerId;
    Boolean timeOut = False;

    /* Set the NEDIT_SERVER_REQUEST_<user>_<host> property on the root
       window to activate the server */
    XChangeProperty(TheDisplay, rootWindow, serverRequestAtom, XA_STRING, 8,
    	    PropModeReplace, (unsigned char *)commandString,
    	    strlen(commandString));
    
    /* Set up a timeout proc in case the server is dead.  The standard
       selection timeout is probably a good guess at how long to wait
       for this style of inter-client communication as well */
    timerId = XtAppAddTimeOut(context,
                              REQUEST_TIMEOUT,
                              (XtTimerCallbackProc)timeOutProc,
                              &timeOut);
    currentWaitForAtom = serverRequestAtom;

    /* Wait for the property to be deleted to know the request was processed */
    while (!timeOut) {
        XEvent event;
        const XPropertyEvent *e = (const XPropertyEvent *)&event;

        XtAppNextEvent(context, &event);
        if (e->window == rootWindow && 
            e->atom == serverRequestAtom &&
            e->state == PropertyDelete)
            break;
        XtDispatchEvent(&event);
    }

    /* Exit if the timeout expired. */
    if (timeOut) {
        fprintf(stderr, "%s: The server did not respond to the request.\n", APP_NAME);
        XtCloseDisplay(TheDisplay);
        exit(EXIT_FAILURE);
    } else {
        XtRemoveTimeOut(timerId);
    }
} 

static void waitUntilFilesOpenedOrClosed(XtAppContext context,
        Window rootWindow)
{
    XtIntervalId timerId;
    Boolean timeOut = False;

    /* Set up a timeout proc so we don't wait forever if the server is dead.
       The standard selection timeout is probably a good guess at how
       long to wait for this style of inter-client communication as
       well */
    timerId = XtAppAddTimeOut(context, FILE_OPEN_TIMEOUT, 
                (XtTimerCallbackProc)timeOutProc, &timeOut);
    currentWaitForAtom = noAtom;

    /* Wait for all of the windows to be opened by server,
     * and closed if -wait was supplied */
    while (fileListHead.fileList) {
        XEvent event;
        const XPropertyEvent *e = (const XPropertyEvent *)&event;

        XtAppNextEvent(context, &event);

        /* Update the fileList and check if all files have been closed. */
        if (e->type == PropertyNotify && e->window == rootWindow) {
            FileListEntry *item;

            if (e->state == PropertyDelete) {
                for (item = fileListHead.fileList; item; item = item->next) {
                    if (e->atom == item->waitForFileOpenAtom) {
                        /* The 'waitForFileOpen' property is deleted when the file is opened */
                        fileListHead.waitForOpenCount--;
                        item->waitForFileOpenAtom = None;

                        /* Reset the timer while we wait for all files to be opened. */
                        XtRemoveTimeOut(timerId);
                        timerId = XtAppAddTimeOut(context, FILE_OPEN_TIMEOUT, 
                                    (XtTimerCallbackProc)timeOutProc, &timeOut);
                    } else if (e->atom == item->waitForFileClosedAtom) {
                        /* When file is opened in -wait mode the property
                         * is deleted when the file is closed.
                         */
                        fileListHead.waitForCloseCount--;
                        item->waitForFileClosedAtom = None;
                    }
                }
                
                if (fileListHead.waitForOpenCount == 0 && !timeOut) {
                    XtRemoveTimeOut(timerId);
                }

                if (fileListHead.waitForOpenCount == 0 &&
                    fileListHead.waitForCloseCount == 0) {
                    break;
                }
            }
        }

        /* We are finished if we are only waiting for files to open and
        ** the file open timeout has expired. */
        if (!Preferences.waitForClose && timeOut) {
           break;
        }

        XtDispatchEvent(&event);
    }
} 


static void nextArg(int argc, char **argv, int *argIndex)
{
    if (*argIndex + 1 >= argc) {
#ifdef VMS
	    *argv[*argIndex] = '/';
#endif /*VMS*/
    	fprintf(stderr, "nc: %s requires an argument\n%s",
	        argv[*argIndex], cmdLineHelp);
    	exit(EXIT_FAILURE);
    }
    (*argIndex)++;
}

/* Copies a given nc command line argument to the server startup command
** line (-icon, -geometry, -xrm, ...) Special characters are protected from
** the shell by escaping EVERYTHING with \ 
** Note that the .shell string in the command line structure is large enough
** to hold the escaped characters.
*/
static void copyCommandLineArg(CommandLine *commandLine, const char *arg)
{
    const char *c;
    char *outPtr = commandLine->shell + strlen(commandLine->shell);
#if defined(VMS) || defined(__EMX__)
    /* Non-Unix shells don't want/need esc */
    for (c=arg; *c!='\0'; c++) {
	*outPtr++ = *c;
    }
    *outPtr++ = ' ';
    *outPtr = '\0';
#else
    *outPtr++ = '\'';
    for (c=arg; *c!='\0'; c++) {
	if (*c == '\'') {
	    *outPtr++ = '\'';
	    *outPtr++ = '\\';
	}
	*outPtr++ = *c;
	if (*c == '\'') {
	    *outPtr++ = '\'';
	}
    }
    *outPtr++ = '\'';
    *outPtr++ = ' ';
    *outPtr = '\0';
#endif /* VMS */
}

/* Print version of 'nc' */
static void printNcVersion(void ) {
   static const char *const ncHelpText = \
   "nc (NEdit) Version 5.7 (January 2017)\n\n\
     Built on: %s, %s, %s\n\
     Built at: %s, %s\n";
     
    fprintf(stdout, ncHelpText,
                  COMPILE_OS, COMPILE_MACHINE, COMPILE_COMPILER,
                  __DATE__, __TIME__);
}
