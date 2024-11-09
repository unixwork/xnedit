/*******************************************************************************
*                                                                              *
* Getfiles.c -- File Interface Routines                                        *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* May 23, 1991                                                                 *
*                                                                              *
* Written by Donna Reid                                                        *
*                                                                              *
* modified 11/5/91 by JMK: integrated changes made by M. Edel; updated for     *
*                          destroy widget problem (took out ManageModalDialog  *
*                          call; added comments.                               *
*          10/1/92 by MWE: Added help dialog and fixed a few bugs              *
*           4/7/93 by DR:  Port to VMS                                         *
*           6/1/93 by JMK: Integrate Port and changes by MWE to make           *
*                          directories "sticky" and a fix to prevent opening   *
*                          a directory when no filename was specified          *
*          6/24/92 by MWE: Made filename list and directory list typeable,     *
*                          set initial focus to filename list                  *
*          6/25/93 by JMK: Fix memory leaks found by Purify.                   *
*                                                                              *
* Included are two routines written using Motif for accessing files:           *
*                                                                              *
* GetExistingFilename  presents a FileSelectionBox dialog where users can      *
*                      choose an existing file to open.                        *
*                                                                              *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "getfiles.h"
#include "fileUtils.h"
#include "misc.h"
#include "nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/XmAll.h>

#include "utils.h"
#include "fileUtils.h"

#include "filedialog.h"

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define MAX_ARGS 20			/* Maximum number of X arguments */
#define PERMS 0666     			/* UNIX file permission, RW for owner,
					   group, world */
#define MAX_LIST_KEYSTROKES 100		/* Max # of keys user can type to 
					   a file list */
#define MAX_LIST_KESTROKE_WAIT 2000	/* Allowable delay in milliseconds
					   between characters typed to a list
					   before starting over (throwing
					   out the accumulated characters */

#define SET_ONE_RSRC(widget, name, newValue) \
{ \
    static Arg tmpargs[1] = {{name, (XtArgVal)0}}; \
    tmpargs[0].value = (XtArgVal)newValue; \
    XtSetValues(widget, tmpargs, 1); \
}	

enum yesNoValues {ynNone, ynYes, ynNo};

/* Saved default directory and pattern from last successful call */
static XmString DefaultDirectory = NULL;

static char* DefaultDirectoryStr = NULL;

/*                    Local Callback Routines and variables                */

static void createYesNoDialog(Widget parent);
static void createErrorDialog(Widget parent);
static int  doYesNoDialog(const char *msg);
static void doErrorDialog(const char *errorString, const char *filename);
static void errorOKCB(Widget w, caddr_t client_data, caddr_t call_data);
static void yesNoOKCB(Widget w, caddr_t client_data, caddr_t call_data);
static void yesNoCancelCB(Widget w, caddr_t client_data, caddr_t call_data);

static Widget YesNoDialog;		/* "Overwrite?" dialog widget	   */
static int YesNoResult;			/* Result of overwrite dialog	   */
static Widget ErrorDialog;		/* Dialog widget for error msgs	   */
static int ErrorDone;			/* Flag to mark dialog completed   */

/*  GetExistingFilename				  	                   */
/*									   */
/*  This routine will popup a file selection box so that the user can      */
/*  select an existing file from the scrollable list.  The user is         */
/*  prevented from entering a new filename because the edittable text      */
/*  area of the file selection box widget is unmanaged.  After the user    */
/*  selects a file, GetExistingFilename returns the selected filename and  */
/*  GFN_OK, indicating that the OK button was pressed.  If the user        */
/*  pressed the cancel button, the return value is GFN_CANCEL, and the     */
/*  filename character string supplied in the call is not altered.	   */
/*									   */
/*  Arguments:								   */
/*									   */
/*	Widget  parent	      - parent widget id			   */
/*	char *  promptString  - prompt string				   */
/*	char *  filename      - a string to receive the selected filename  */
/*				(this string will not be altered if the    */
/*				user pressed the cancel button)		   */
/*									   */
/*  Returns:	GFN_OK	      - file was selected and OK button pressed	   */
/*		GFN_CANCEL    - Cancel button pressed and no returned file */
/*									   */
int GetExistingFilename(Widget parent, char *promptString, FileSelection *file) 
{ 
    return FileDialog(parent, promptString, file, FILEDIALOG_OPEN, NULL);
}

/* GetNewFilename
 *
 * Same as GetExistingFilename but pick a new file instead of an existing one.
 * In this case the text area of the FSB is *not* unmanaged, so the user can
 * enter a new filename.
 */
int GetNewFilename(Widget parent, char *promptString, FileSelection *file,
        char *defaultName)
{
    return FileDialog(parent, promptString, file, FILEDIALOG_SAVE, defaultName);
}


/*
** Return current default directory used by GetExistingFilename.
** Can return NULL if no default directory has been set (meaning
** use the application's current working directory) String must
** be freed by the caller using NEditFree.
*/
char *GetFileDialogDefaultDirectory(void)
{
    char *string;
    
    if (DefaultDirectory == NULL)
    	return NULL;
    XmStringGetLtoR(DefaultDirectory, XmSTRING_DEFAULT_CHARSET, &string);
    return string;
}

/*
** Set the current default directory to be used by GetExistingFilename.
** "dir" can be passed as NULL to clear the current default directory
** and use the application's working directory instead.
*/
void SetFileDialogDefaultDirectory(char *dir)
{
    if (DefaultDirectory != NULL)
    	XmStringFree(DefaultDirectory);
    DefaultDirectory = dir==NULL ? NULL : XmStringCreateSimple(dir);
    
    if(DefaultDirectoryStr) {
        NEditFree(DefaultDirectoryStr);
    }
    DefaultDirectoryStr = NEditStrdup(dir);
}

char* GetDefaultDirectoryStr(void)
{
    return DefaultDirectoryStr;
}

/*
** createYesNoDialog, createErrorDialog, doYesNoDialog, doErrorDialog
**
** Error Messages and question dialogs to be used with the file selection
** box.  Due to a crash bug in Motif 1.1.1 thru (at least) 1.1.5
** getfiles can not use DialogF.  According to OSF, there is an error
** in the creation of pushButtonGadgets involving the creation and
** destruction of some sort of temporary object.  These routines create
** the dialogs along with the file selection dialog and manage them
** to display messages.  This somehow avoids the problem
*/
static void createYesNoDialog(Widget parent)
{
    XmString  buttonString;	      /* compound string for dialog buttons */
    int       n;                      /* number of arguments               */ 
    Arg       args[MAX_ARGS];	      /* arg list                          */

    n = 0;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;
    XtSetArg(args[n], XmNtitle, " "); n++;
    YesNoDialog = CreateQuestionDialog(parent, "yesNo", args, n);
    XtAddCallback (YesNoDialog, XmNokCallback, (XtCallbackProc)yesNoOKCB, NULL);
    XtAddCallback (YesNoDialog, XmNcancelCallback,
    	    (XtCallbackProc)yesNoCancelCB, NULL);
    XtUnmanageChild(XmMessageBoxGetChild (YesNoDialog, XmDIALOG_HELP_BUTTON));
    buttonString = XmStringCreateSimple("Yes");
    SET_ONE_RSRC(YesNoDialog, XmNokLabelString, buttonString);
    XmStringFree(buttonString);
    buttonString = XmStringCreateSimple("No");
    SET_ONE_RSRC(YesNoDialog, XmNcancelLabelString, buttonString);
    XmStringFree(buttonString);
}

static void createErrorDialog(Widget parent)
{
    XmString  buttonString;	      /* compound string for dialog button */
    int       n;                      /* number of arguments               */ 
    Arg       args[MAX_ARGS];	      /* arg list                          */

    n = 0;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;
    XtSetArg(args[n], XmNtitle, " "); n++;
    ErrorDialog = CreateErrorDialog(parent, "error", args, n);
    XtAddCallback(ErrorDialog, XmNcancelCallback, (XtCallbackProc)errorOKCB,
    	    NULL);
    XtUnmanageChild(XmMessageBoxGetChild(ErrorDialog, XmDIALOG_OK_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(ErrorDialog, XmDIALOG_HELP_BUTTON));
    buttonString = XmStringCreateLtoR("OK", XmSTRING_DEFAULT_CHARSET);
    XtVaSetValues(ErrorDialog, XmNcancelLabelString, buttonString, NULL);
    XtVaSetValues(XmMessageBoxGetChild(ErrorDialog, XmDIALOG_CANCEL_BUTTON),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
            NULL);
    XmStringFree(buttonString);
}

int OverrideFileDialog(Widget parent, const char *filename)
{
    createYesNoDialog(parent);
    int ret = doYesNoDialog(filename);
    XtDestroyWidget(YesNoDialog);
    YesNoDialog = NULL;
    return ret;
}


void FileOpenErrorDialog(Widget parent, const char *filename)
{
    createErrorDialog(parent);
    doErrorDialog("Error: can't open %s ", filename);
    return;
}

static int doYesNoDialog(const char *filename)
{
    char string[255];
    XmString mString;

    YesNoResult = ynNone;

    sprintf(string, "File %s already exists,\nOk to overwrite?", filename);
    mString = XmStringCreateLtoR(string, XmSTRING_DEFAULT_CHARSET);
    
    SET_ONE_RSRC(YesNoDialog, XmNmessageString, mString);
    XmStringFree(mString);
    ManageDialogCenteredOnPointer(YesNoDialog);

    while (YesNoResult == ynNone)
	XtAppProcessEvent(XtWidgetToApplicationContext(YesNoDialog), XtIMAll);
    
    XtUnmanageChild(YesNoDialog);

    /* Nasty motif bug here, patched around by waiting for a ReparentNotify
       event (with timeout) before allowing file selection dialog to pop
       down.  If this routine returns too quickly, and the file selection
       dialog (and thereby, this dialog as well) are destroyed while X
       is still sorting through the events generated by the pop-down,
       something bad happens and we get a crash */
    if (YesNoResult == ynYes)
    	PopDownBugPatch(YesNoDialog);

    return YesNoResult == ynYes;
}

static void doErrorDialog(const char *errorString, const char *filename)
{
    char string[255];
    XmString mString;

    ErrorDone = False;

    sprintf(string, errorString, filename);
    mString = XmStringCreateLtoR(string, XmSTRING_DEFAULT_CHARSET);
    
    SET_ONE_RSRC(ErrorDialog, XmNmessageString, mString);
    XmStringFree(mString);
    ManageDialogCenteredOnPointer(ErrorDialog);

    while (!ErrorDone)
	XtAppProcessEvent (XtWidgetToApplicationContext(ErrorDialog), XtIMAll);
    
    XtUnmanageChild(ErrorDialog);
}

static void yesNoOKCB(Widget w, caddr_t client_data, caddr_t call_data)
{
    YesNoResult = ynYes;
}

static void errorOKCB(Widget w, caddr_t client_data, caddr_t call_data)
{
    ErrorDone = True;
}

static void yesNoCancelCB(Widget w, caddr_t client_data, caddr_t call_data)
{
    YesNoResult = ynNo;
}


/*
 * code from nedit source/file.c
 * This function exists only to avoid mixing old nedit code with the
 * new file dialog code
 */
Widget CreateFormatButtons(
        Widget form,
        Widget bottom,
        int format,
        Widget *u,
        Widget *d,
        Widget *m)
{
    XmString str;
    Widget formatBtns = XtVaCreateManagedWidget("formatBtns",
            xmRowColumnWidgetClass, form,
            XmNradioBehavior, XmONE_OF_MANY,
            XmNorientation, XmHORIZONTAL,
            XmNpacking, XmPACK_TIGHT,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, bottom,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNleftOffset, 5,
            XmNrightOffset, 5,
            XmNbottomOffset, 2,
            NULL);
    XtVaCreateManagedWidget("formatBtns", xmLabelWidgetClass, formatBtns,
            XmNlabelString, str=XmStringCreateSimple("Format:"), NULL);
    XmStringFree(str);
    *u = XtVaCreateManagedWidget("unixFormat",
            xmToggleButtonWidgetClass, formatBtns,
            XmNlabelString, str = XmStringCreateSimple("Unix"),
            XmNset, format == UNIX_FILE_FORMAT,
            XmNuserData, (XtPointer)UNIX_FILE_FORMAT,
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNmnemonic, 'U',
            NULL);
    XmStringFree(str);
    *d = XtVaCreateManagedWidget("dosFormat",
            xmToggleButtonWidgetClass, formatBtns,
            XmNlabelString, str = XmStringCreateSimple("DOS"),
            XmNset, format == DOS_FILE_FORMAT,
            XmNuserData, (XtPointer)DOS_FILE_FORMAT,
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNmnemonic, 'O',
            NULL);
    XmStringFree(str);
    *m= XtVaCreateManagedWidget("macFormat",
            xmToggleButtonWidgetClass, formatBtns,
            XmNlabelString, str = XmStringCreateSimple("Macintosh"),
            XmNset, format == MAC_FILE_FORMAT,
            XmNuserData, (XtPointer)MAC_FILE_FORMAT,
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNmnemonic, 'M',
            NULL);
    XmStringFree(str);
    return formatBtns;
}
