/*******************************************************************************
*                                                                              *
* utils.c -- miscellaneous non-GUI routines                                    *
*                                                                              *
* Copyright (C) 2002 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.*                                                           *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "utils.h"
#include "nedit_malloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef VMS
#include <lib$routines.h>
#include ssdef
#include syidef
#include "../util/VMSparam.h"
#include "../util/VMSutils.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

/* just to get 'Boolean' types defined: */
#include <X11/Intrinsic.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define DEFAULT_NEDIT_HOME ".nedit"
#ifdef VMS
    static char* hiddenFileNames[N_FILE_TYPES] = {".nedit", ".neditmacro", ".neditdb;1"};
    static char* plainFileNames[N_FILE_TYPES] = {"nedit.rc", "autoload.nm", "nedit.history;1"};
#else
    static char* hiddenFileNames[N_FILE_TYPES] = {".nedit", ".neditmacro", ".neditdb"};
    static char* plainFileNames[N_FILE_TYPES] = {"nedit.rc", "autoload.nm", "nedit.history"};
#endif

static void buildFilePath(char* fullPath, const char* dir, const char* file);
static Boolean isDir(const char* file);
static Boolean isRegFile(const char* file);

/* return non-NULL value for the current working directory.
   If system call fails, provide a fallback value */
const char* GetCurrentDir(void)
{
    static char curdir[MAXPATHLEN];

    if (!getcwd(curdir, (size_t) MAXPATHLEN)) {
        perror("nedit: getcwd() fails");
        strcpy(curdir, ".");
    }
    return (curdir);
}


/* return a non-NULL value for the user's home directory,
   without trailing slash.
   We try the  environment var and the system user database. */
const char* GetHomeDir(void)
{
    const char *ptr;
    static char homedir[MAXPATHLEN]="";
    struct passwd *passwdEntry;
    size_t len;

    if (*homedir) {
       return homedir;
    }
    ptr=getenv("HOME");
    if (!ptr) {
       passwdEntry = getpwuid(getuid());
       if (passwdEntry && *(passwdEntry->pw_dir)) {
          ptr= passwdEntry->pw_dir;
       } else {
          /* This is really serious, so just exit. */
          perror("nedit: getpwuid() failed ");
          exit(EXIT_FAILURE);
       }
    }
    strncpy(homedir, ptr, sizeof(homedir)-1);
    homedir[sizeof(homedir)-1]='\0';
    /* Fix trailing slash */
    len=strlen(homedir);
    if (len>1 && homedir[len-1]=='/') {
       homedir[len-1]='\0';
    }
    return homedir;
}

/*
** Return a pointer to the username of the current user in a statically
** allocated string.
*/
const char
*GetUserName(void)
{
#ifdef VMS
    return cuserid(NULL);
#else
    /* cuserid has apparently been dropped from the ansi C standard, and if
       strict ansi compliance is turned on (on Sun anyhow, maybe others), calls
       to cuserid fail to compile.  Older versions of nedit try to use the
       getlogin call first, then if that fails, use getpwuid and getuid.  This
       results in the user-name of the original terminal being used, which is
       not correct when the user uses the su command.  Now, getpwuid only: */

    const struct passwd *passwdEntry;
    static char *userName=NULL;
    
    if (userName)
       return userName;
    
    passwdEntry = getpwuid(getuid());
    if (!passwdEntry) {
       /* This is really serious, but sometimes username service
          is misconfigured through no fault of the user.  Be nice
          and let the user start nc anyway. */
       perror("nedit: getpwuid() failed - reverting to $USER");
       return getenv("USER");
    }
    else {
       userName=malloc(strlen(passwdEntry->pw_name)+1);
       strcpy(userName, passwdEntry->pw_name);
       return userName;
    }
#endif /* VMS */
}


/*
** Writes the hostname of the current system in string "hostname".
**
** NOTE: This function used to be called "GetHostName" but that resulted in a 
** linking conflict on VMS with the standard gethostname function, because
** VMS links case-insensitively.
*/
const char
*GetNameOfHost(void)
{
    static char hostname[MAXNODENAMELEN+1];
    static int  hostnameFound = False;
    
    if (!hostnameFound) {
#ifdef VMS
        /* This should be simple, but uname is not supported in the DEC C RTL and
           gethostname on VMS depends either on Multinet or UCX.  So use uname 
           on Unix, and use LIB$GETSYI on VMS. Note the VMS hostname will
           be in DECNET format with trailing double colons, e.g. "FNALV1::".    */
        int syi_status;
        struct dsc$descriptor_s *hostnameDesc;
        unsigned long int syiItemCode = SYI$_NODENAME;	/* get Nodename */
        unsigned long int unused = 0;
        unsigned short int hostnameLen = MAXNODENAMELEN+1;

        hostnameDesc = NulStrWrtDesc(hostname, MAXNODENAMELEN+1);
        syi_status = lib$getsyi(&syiItemCode, &unused, hostnameDesc, &hostnameLen,
    			        0, 0);
        if (syi_status != SS$_NORMAL) {
	    fprintf(stderr, "nedit: Error return from lib$getsyi: %d", syi_status);
	    strcpy(hostname, "VMS");
        } else
    	    hostname[hostnameLen] = '\0';
        FreeStrDesc(hostnameDesc);
#else
        struct utsname nameStruct;
        int rc = uname(&nameStruct);
        if (rc<0) {
            /* Shouldn't ever happen, so we better exit() here */
           perror("nedit: uname() failed ");
           exit(EXIT_FAILURE);
        }
        strcpy(hostname, nameStruct.nodename);
#endif /* VMS */
        hostnameFound = True;
    }
    return hostname;
}


/*
** Create a path: $HOME/filename
** Return "" if it doesn't fit into the buffer
*/
char 
*PrependHome(const char *filename, char *buf, size_t buflen)
{
    const char *homedir;
    size_t home_len, file_len;
    
    homedir=GetHomeDir();
    home_len=strlen(homedir);
    file_len=strlen(filename);
    if ( (home_len+1+file_len)>=buflen ) {
       buf[0]='\0';
    }
    else {
       strcpy(buf, homedir);
       strcat(buf, "/");
       strcat(buf, filename);
    }
    return buf;
}

int Min(int i1, int i2)
{
    return i1 <= i2 ? i1 : i2;
}

/*
**  Returns a pointer to the name of an rc file of the requested type.
**
**  Preconditions:
**      - MAXPATHLEN is set to the max. allowed path length
**      - fullPath points to a buffer of at least MAXPATHLEN
**
**  Returns:
**      - NULL if an error occurs while creating a directory
**      - Pointer to a static array containing the file name
**
*/
const char* GetRCFileName(int type)
{
    static char rcFiles[N_FILE_TYPES][MAXPATHLEN + 1];
    static int namesDetermined = False;

    if (!namesDetermined)
    {
        char* nedit_home;
        int i;

        if ((nedit_home = getenv("NEDIT_HOME")) == NULL)
        {
            /*  No NEDIT_HOME */
#ifdef VMS
            /* This is a default VMS setup */
            for (i = 0; i < N_FILE_TYPES; i++)
            {
                buildFilePath(rcFiles[i], "SYS$LOGIN", hiddenFileNames[i]);
            }
#else /* #ifdef VMS */
            /* Let's try if ~/.nedit is a regular file or not. */
            char legacyFile[MAXPATHLEN + 1];
            buildFilePath(legacyFile, GetHomeDir(), hiddenFileNames[NEDIT_RC]);
            if (isRegFile(legacyFile))
            {
                /* This is a legacy setup with rc files in $HOME */
                for (i = 0; i < N_FILE_TYPES; i++)
                {
                    buildFilePath(rcFiles[i], GetHomeDir(), hiddenFileNames[i]);
                }
            } else
            {
                /* ${HOME}/.nedit does not exist as a regular file. */
                /* FIXME: Devices, sockets and fifos are ignored for now. */
                char defaultNEditHome[MAXPATHLEN + 1];
                buildFilePath(defaultNEditHome, GetHomeDir(), DEFAULT_NEDIT_HOME);
                if (!isDir(defaultNEditHome))
                {
                    /* Create DEFAULT_NEDIT_HOME */
                    if (mkdir(defaultNEditHome, 0777) != 0)
                    {
                        perror("nedit: Error while creating rc file directory"
                                " $HOME/" DEFAULT_NEDIT_HOME "\n"
                                " (Make sure all parent directories exist.)");
                        return NULL;
                    }
                }

                /* All set for DEFAULT_NEDIT_HOME, let's copy the names */
                for (i = 0; i < N_FILE_TYPES; i++)
                {
                    buildFilePath(rcFiles[i], defaultNEditHome, plainFileNames[i]);
                }
            }
#endif /* #ifdef VMS */
        } else
        {
            /*  $NEDIT_HOME is set. */
#ifndef VMS
            /* FIXME: Is this required? Does VMS know stat(), mkdir()? */
            if (!isDir(nedit_home))
            {
                /* Create $NEDIT_HOME */
                if (mkdir(nedit_home, 0777) != 0)
                {
                    perror("nedit: Error while creating rc file directory $NEDIT_HOME\n"
                            "nedit: (Make sure all parent directories exist.)");
                    return NULL;
                }
            }
#endif /* #ifndef VMS */

            /* All set for NEDIT_HOME, let's copy the names */
            for (i = 0; i < N_FILE_TYPES; i++)
            {
                buildFilePath(rcFiles[i], nedit_home, plainFileNames[i]);
            }
        }

        namesDetermined = True;
    }

    return rcFiles[type];
}

/*
**  Builds a file path from 'dir' and 'file', watching for buffer overruns.
**
**  Preconditions:
**      - MAXPATHLEN is set to the max. allowed path length
**      - 'fullPath' points to a buffer of at least MAXPATHLEN
**      - 'dir' and 'file' are valid strings
**
**  Postcondition:
**      - 'fullpath' will contain 'dir/file'
**      - Exits when the result would be greater than MAXPATHLEN
*/
static void buildFilePath(char* fullPath, const char* dir, const char* file)
{
    if ((MAXPATHLEN) < strlen(dir) + strlen(file) + 2)
    {
        /*  We have no way to build the path. */
        fprintf(stderr, "nedit: rc file path too long for %s.\n", file);
        exit(EXIT_FAILURE);
    }

    /*  The length is already checked */
    strcpy(fullPath, dir);
#ifdef VMS
    strcat(fullPath, ":");
#else /* #ifdef VMS */
    strcat(fullPath, "/");
#endif /* #ifdef VMS */
    strcat(fullPath, file);
}

/*
**  Returns true if 'file' is a directory, false otherwise.
**  Links are followed.
**
**  Preconditions:
**      - None
**
**  Returns:
**      - True for directories, false otherwise
*/
static Boolean isDir(const char* file)
{
    struct stat attribute;

    return ((stat(file, &attribute) == 0) && S_ISDIR(attribute.st_mode));
}

/*
**  Returns true if 'file' is a regular file, false otherwise.
**  Links are followed.
**
**  Preconditions:
**      - None
**
**  Returns:
**      - True for regular files, false otherwise
*/
static Boolean isRegFile(const char* file)
{
    struct stat attribute;

    return ((stat(file, &attribute) == 0) && S_ISREG(attribute.st_mode));
}

/*
**  Part of the simple stack. Accepts a stack and the pointer you want to
**  store. NULL is not allowed, as it is used in Pop() to signal an empty
**  stack.
*/
void Push(Stack* stack, const void* value)
{
    stackObject* pushee;

    /*  Throw away invalid parameters.  */
    if (NULL == value) {
        fprintf(stderr, "nedit: Internal error: NULL was pushed.\n");
        return;
    }
    if (NULL == stack) {
        fprintf(stderr, "nedit: Internal error: push() called with NULL stack.\n");
        return;
    }

    /*  Allocate memory for new value.  */
    pushee = (stackObject*) NEditMalloc(sizeof(stackObject));

    /*  Put pushee on top of stack.  */
    pushee->value = (void*) value;
    pushee->next = stack->top;
    stack->top = pushee;
    (stack->size)++;

    return;
}

/*
**  Part of the simple stack, returns the topmost item from the stack or
**  NULL if the stack is empty. It also returns NULL if the stack itself
**  is NULL.
**
**  Precondition: The stack's top element is either NULL or a properly
**  initialised stackObject.
*/
void* Pop(Stack* stack)
{
    stackObject* popee;
    void* value;

    /*  Throw away invalid parameter.  */
    if (NULL == stack) {
        fprintf(stderr, "nedit: Internal error: pop() called with NULL stack.\n");
        return NULL;
    }

    /*  Return NULL if Stack is empty.  */
    if (NULL == stack->top) {
        return NULL;
    }

    /*  Remove top entry in the stack.  */
    popee = stack->top;
    stack->top = popee->next;
    (stack->size)--;

    value = popee->value;
    NEditFree((char*) popee);

    return value;
}

/*
**  We currently don't need this function: In the only situation where we use
**  the stack, we empty it completely. This might come in handy if the stack
**  is ever used anywhere else.
**
**  Beware: Utterly untested.
*/  /*
void FreeStack(Stack* stack)
{
    void* dummy;

    while (NULL != (dummy = pop(progStack))) {}

    NEditFree((char*) stack);
}
*/
