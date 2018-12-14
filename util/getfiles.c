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
#ifdef VMS
#include <unixio.h>
#include <file.h>
#include "VMSparam.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/
#include <sys/stat.h>

#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/FileSB.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/MessageB.h>
#include <Xm/PushBG.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

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
static XmString DefaultPattern = NULL;

/* User settable option for leaving the file name text field in
   GetExistingFilename dialogs.  Off by default so new users will get
   used to typing in the list rather than in the text field */
static int RemoveRedundantTextField = True;

/* Text for help button help display */
/* ... needs variant for VMS */
#ifndef SGI_CUSTOM
static const char *HelpExist =
"The file open dialog shows a list of directories on the left, and a list \
of files on the right.  Double clicking on a file name in the list on the \
right, or selecting it and pressing the OK button, will open that file.  \
Double clicking on a directory name, or selecting \
it and pressing \"Filter\", will move into that directory.  To move upwards in \
the directory tree, double click on the directory entry ending in \"..\".  \
You can also begin typing a file name to select from the file list, or \
directly type in directory and file specifications in the \
field labeled \"Filter\".\n\
\n\
If you use the filter field, remember to include \
either a file name, \"*\" is acceptable, or a trailing \"/\".  If \
you don't, the name after the last \"/\" is interpreted as the file name to \
match.  When you leave off the file name or trailing \"/\", you won't see \
any files to open in the list \
because the filter specification matched the directory file itself, rather \
than the files in the directory.";

static const char *HelpNew = 
"This dialog allows you to create a new file, or to save the current file \
under a new name.  To specify a file \
name in the current directory, complete the name displayed in the \"Save File \
As:\" field near the bottom of the dialog.  If you delete or change \
the path shown in the field, the file will be saved using whatever path \
you type, provided that it is a valid Unix file specification.\n\
\n\
To replace an existing file, select it from the Files list \
and press \"OK\", or simply double click on the name.\n\
\n\
To save a file in another directory, use the Directories list \
to move around in the file system hierarchy.  Double clicking on \
directory names in the list, or selecting them and pressing the \
\"Filter\" button will select that directory.  To move upwards \
in the directory tree, double \
click on the directory entry ending in \"..\".  You can also move directly \
to a directory by typing the file specification of the path in the \"Filter\" \
field and pressing the \"Filter\" button.";

#else /* SGI_CUSTOM */
static const char *HelpExist =
"The \"File to Edit:\" field shows a list of directories and files in the \
current directory.\n\
\n\
Double clicking on a file name in the list, or selecting it and pressing \
the OK button, will open that file.\n\
\n\
Double clicking on a directory name, or selecting it and pressing the OK \
button will move into that directory.  To navigate upwards in the file \
system hierarchy you can use the buttons above the \"Selection\" field  \
(each of these buttons represent a directory level). \n\
\n\
You can also enter a file or directory name to open in the field \
labeled \"Selection\".  Pressing the space bar will complete a partial file \
name, or beep if no files match.  The drop pocket to the right of the field \
will accept icons dragged from the desktop, and the button with the circular \
arrows, to the right, of the field recalls previously selected \
directories.\n\
\n\
The \"Filter\" button allows you to narrow down the list of files and \
directories shown in the \"File to Edit:\" field.  The default filter of \
\"*\" allows all files to be listed.";

static const char *HelpNew = 
"This dialog allows you to create a new file or to save the current file \
under a new name.\n\
\n\
To specify a file name in the current directory, complete the name displayed \
in the \"Save File As:\" field.  If you delete or change the path shown \
in the field, the file will be saved using whatever path you type, provided \
that it is a valid Unix file specification.\n\
\n\
To replace an existing file, select it from the \"Files\" list and press \
\"OK\", or simply double click on the name in the \"Files\" list.\n\
\n\
To save a file in another directory, use the \"Files\" list to move around \
in the file system hierarchy.  Double clicking on a directory name, or \
selecting it and pressing the OK button, will move into that directory. \
To navigate upwards in the file system hierarchy you can use the buttons \
above the \"Selection\" field (each of these buttons represent a directory \
level).\n\
\n\
You can also move directly to a directory by typing the file specification \
of the path in the \"Save File As:\" field.  Pressing the space bar will \
complete a partial directory or file \
name, or beep if nothing matches.  The drop pocket to the right of the field \
will accept icons dragged from the desktop, and the button with the circular \
arrows, to the right, of the field recalls previously selected \
directories.\n\
\n\
The \"Filter\" button allows you to narrow down the list of files and \
directories shown in the \"Files\" field.  The default filter of \
\"*\" allows all files to be listed.";
#endif /* SGI_CUSTOM */

/*                    Local Callback Routines and variables                */

static void newFileOKCB(Widget w, Boolean *client_data,
	       XmFileSelectionBoxCallbackStruct *call_data);
static void newFileCancelCB(Widget w, Boolean *client_data, caddr_t 
               call_data);
static void newHelpCB(Widget w, Widget helpPanel, caddr_t call_data);
static void createYesNoDialog(Widget parent);
static void createErrorDialog(Widget parent);
static int  doYesNoDialog(const char *msg);
static void doErrorDialog(const char *errorString, const char *filename);
static void existOkCB(Widget w, Boolean * client_data,
	              XmFileSelectionBoxCallbackStruct *call_data);
static void existCancelCB(Widget w, Boolean * client_data, caddr_t call_data);
static void existHelpCB(Widget w, Widget helpPanel, caddr_t call_data);
static void errorOKCB(Widget w, caddr_t client_data, caddr_t call_data);
static void yesNoOKCB(Widget w, caddr_t client_data, caddr_t call_data);
static void yesNoCancelCB(Widget w, caddr_t client_data, caddr_t call_data);
static Widget createPanelHelp(Widget parent, const char *text, const char *title);
static void helpDismissCB(Widget w, Widget helpPanel, caddr_t call_data);
static void makeListTypeable(Widget listW);
static void listCharEH(Widget w, XtPointer callData, XEvent *event,
	Boolean *continueDispatch);
static void replacementDirSearchProc(Widget w, XtPointer searchData);
static void replacementFileSearchProc(Widget w, XtPointer searchData);
static void sortWidgetList(Widget listWidget);
static int compareXmStrings(const void *string1, const void *string2);

static int  SelectResult = GFN_CANCEL;  /*  Initialize results as cancel   */
static Widget YesNoDialog;		/* "Overwrite?" dialog widget	   */
static int YesNoResult;			/* Result of overwrite dialog	   */
static Widget ErrorDialog;		/* Dialog widget for error msgs	   */
static int ErrorDone;			/* Flag to mark dialog completed   */
static void (*OrigDirSearchProc)();	/* Built in Motif directory search */
static void (*OrigFileSearchProc)();	/* Built in Motif file search proc */

/* 
 * Do the hard work of setting up a file selection dialog
 */
Widget getFilenameHelper(Widget parent, char *promptString, char *filename, 
        int existing) 
{
    int       n;                      /* number of arguments               */
    Arg	      args[MAX_ARGS];	      /* arg list	                   */
    Widget    fileSB;	              /* widget file select box 	   */
    XmString  titleString;	      /* compound string for dialog title  */

    n = 0;
    titleString = XmStringCreateSimple(promptString);
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;
    XtSetArg(args[n], XmNdialogTitle, titleString); n++;
    fileSB = CreateFileSelectionDialog(parent,"FileSelect",args,n);
    XmStringFree(titleString);
#ifndef SGI_CUSTOM
    if (existing && RemoveRedundantTextField)
        XtUnmanageChild(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_TEXT)); 
    XtUnmanageChild(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_SELECTION_LABEL));

    XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_LABEL),
            XmNmnemonic, 'l',
            XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_TEXT),
            NULL);
    XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_DIR_LIST_LABEL),
            XmNmnemonic, 'D',
            XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_DIR_LIST),
            NULL);
    XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_LIST_LABEL),
            XmNmnemonic, promptString[strspn(promptString, "lD")],
            XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_LIST),
            NULL);
    AddDialogMnemonicHandler(fileSB, FALSE);
    RemapDeleteKey(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_TEXT));
    RemapDeleteKey(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_TEXT));
#endif
    return fileSB;
}

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
int GetExistingFilename(Widget parent, char *promptString, char *filename) 
{
    Widget existFileSB = getFilenameHelper(parent, promptString, filename, 
            True);
    return HandleCustomExistFileSB(existFileSB, filename);
}

/* GetNewFilename
 *
 * Same as GetExistingFilename but pick a new file instead of an existing one.
 * In this case the text area of the FSB is *not* unmanaged, so the user can
 * enter a new filename.
 */
int GetNewFilename(Widget parent, char *promptString, char *filename,
        char *defaultName)
{
    Widget fileSB = getFilenameHelper(parent, promptString, filename, False);
    return HandleCustomNewFileSB(fileSB, filename, defaultName);
}

/*
** HandleCustomExistFileSB
**
** Manage a customized file selection box for opening existing files.
** Use this if you want to change the standard file selection dialog
** from the defaults provided in GetExistingFilename, but still
** want take advantage of the button processing, help messages, and
** file checking of GetExistingFilename.
**
**  Arguments:
**
**	Widget  existFileSB   - your custom file selection box widget id
**	char *  filename      - a string to receive the selected filename
**				(this string will not be altered if the
**				user pressed the cancel button)	
**
**  Returns:	GFN_OK	      - file was selected and OK button pressed	
**		GFN_CANCEL    - Cancel button pressed and no returned file
**
*/
int HandleCustomExistFileSB(Widget existFileSB, char *filename)
{
    Boolean   done_with_dialog=False; /* ok to destroy dialog flag	   */
    char      *fileString;            /* C string for file selected        */
    char      *dirString;             /* C string for dir of file selected */
    XmString  cFileString;            /* compound string for file selected */
    XmString  cDir;	              /* compound directory selected	   */
    XmString  cPattern;               /* compound filter pattern	   */
    Widget    help;		      /* help window form dialog	   */
#if XmVersion < 1002
    int       i;
#endif

    XtAddCallback(existFileSB, XmNokCallback, (XtCallbackProc)existOkCB,
    	    &done_with_dialog);
    XtAddCallback(existFileSB, XmNcancelCallback, (XtCallbackProc)existCancelCB,
	    &done_with_dialog);
    AddMotifCloseCallback(XtParent(existFileSB), (XtCallbackProc)existCancelCB,
	    &done_with_dialog);
    help = createPanelHelp(existFileSB, HelpExist, "Selecting Files to Open");
    createErrorDialog(existFileSB);
    XtAddCallback(existFileSB, XmNhelpCallback, (XtCallbackProc)existHelpCB,
    	    (char *)help);
    if (DefaultDirectory != NULL || DefaultPattern != NULL)
    	XtVaSetValues(existFileSB, XmNdirectory, DefaultDirectory,
    		XmNpattern, DefaultPattern, NULL);
#ifndef SGI_CUSTOM
    makeListTypeable(XmFileSelectionBoxGetChild(existFileSB,XmDIALOG_LIST));
    makeListTypeable(XmFileSelectionBoxGetChild(existFileSB,XmDIALOG_DIR_LIST));
#if XmVersion >= 1002
    XtVaSetValues(existFileSB, XmNinitialFocus, XtParent(
    	    XmFileSelectionBoxGetChild(existFileSB, XmDIALOG_LIST)), NULL);
#endif
#endif
    ManageDialogCenteredOnPointer(existFileSB);

#ifndef SGI_CUSTOM
    /* Typing in the directory list is dependent on the list being in the
       same form of alphabetical order expected by the character processing
       routines.  As of about 1.2.3, some Motif libraries seem to have a
       different idea of ordering than is usual for Unix directories.
       To sort them properly, we have to patch the directory and file
       searching routines to re-sort the lists when they change */
    XtVaGetValues(existFileSB, XmNdirSearchProc, &OrigDirSearchProc,
    	    XmNfileSearchProc, &OrigFileSearchProc, NULL);
    XtVaSetValues(existFileSB, XmNdirSearchProc, replacementDirSearchProc,
    	    XmNfileSearchProc, replacementFileSearchProc, NULL);
    sortWidgetList(XmFileSelectionBoxGetChild(existFileSB, XmDIALOG_DIR_LIST));
    sortWidgetList(XmFileSelectionBoxGetChild(existFileSB, XmDIALOG_LIST));
#if XmVersion < 1002
    /* To give file list initial focus, revoke default button status for
       the "OK" button.  Dynamic defaulting will restore it as the default
       button after the keyboard focus is established.  Note the voodoo
       below: calling XmProcess traversal extra times (a recommendation from
       OSF technical support) somehow succeedes in giving the file list focus */
    XtVaSetValues(existFileSB, XmNdefaultButton, NULL, NULL);
    for (i=1; i<30; i++)
    	XmProcessTraversal(XmFileSelectionBoxGetChild(existFileSB,
    		XmDIALOG_LIST), XmTRAVERSE_CURRENT);
#endif
#endif /* SGI_CUSTOM */

    while (!done_with_dialog)
        XtAppProcessEvent(XtWidgetToApplicationContext(existFileSB), XtIMAll);
    
    if (SelectResult == GFN_OK) {
	XtVaGetValues(existFileSB, XmNdirSpec, &cFileString, XmNdirectory,
		&cDir, XmNpattern, &cPattern, NULL);
	/* Undocumented: file selection box widget allocates copies of these
	   strings on getValues calls.  I have risked freeing them to avoid
	   memory leaks, since I assume other developers have made this same
	   realization, therefore OSF can't easily go back and change it */
	if (DefaultDirectory != NULL) XmStringFree(DefaultDirectory);
	if (DefaultPattern != NULL) XmStringFree(DefaultPattern);
	DefaultDirectory = cDir;
	DefaultPattern = cPattern;
	XmStringGetLtoR(cFileString, XmSTRING_DEFAULT_CHARSET, &fileString);
        /* Motif 2.x seem to contain a bug that causes it to return only 
           the relative name of the file in XmNdirSpec when XmNpathMode is set
           to XmPATH_MODE_RELATIVE (through X resources), although the man
           page states that it always returns the full path name. We can
           easily work around this by checking that the first character of the
           file name is a `/'. */
#ifdef VMS
       /* VMS  won't return `/' as the 1st character of the full file spec.
         `:' terminates the device name and is not allowed elsewhere */
        if (strchr(fileString, ':') != NULL) {
#else
        if (fileString[0] == '/') {
#endif        /* VMS */
	    /* The directory name is already present in the file name or
	       the user entered a full path name. */
	    strcpy(filename, fileString);
	} else {
	    /* Concatenate the directory name and the file name */
   	    XmStringGetLtoR(cDir, XmSTRING_DEFAULT_CHARSET, &dirString);
	    strcpy(filename, dirString);
	    strcat(filename, fileString);
  	    NEditFree(dirString);
	}
	XmStringFree(cFileString);
	NEditFree(fileString);
    }
    /* Destroy the dialog _shell_ iso. the dialog. Normally, this shouldn't
       be necessary as the shell is destroyed automatically when the dialog
       is. However, due to a bug in various Lesstif versions, the latter 
       messes up the grab cascades and leaves new windows without grabs, such
       that they appear to be frozen. */
    XtDestroyWidget(XtParent(existFileSB));
    return SelectResult;
}


/*
** HandleCustomNewFileSB
**
** Manage a customized file selection box for opening new files.
**
**  Arguments:
**
**	Widget  newFileSB     - your custom file selection box widget id
**	char *  filename      - a string to receive the selected filename
**				(this string will not be altered if the
**				user pressed the cancel button)	
**  	char*	defaultName   - default name to be pre-entered in filename
**  	    	    	    	text field.
**
**  Returns:	GFN_OK	      - file was selected and OK button pressed	
**		GFN_CANCEL    - Cancel button pressed and no returned file
**
*/
int HandleCustomNewFileSB(Widget newFileSB, char *filename, char *defaultName)
{
    Boolean   done_with_dialog=False; /* ok to destroy dialog flag	   */
    Widget    help;		      /* help window form dialog	   */
    XmString  cFileString;            /* compound string for file selected */
    XmString  cDir;	              /* compound directory selected	   */
    XmString  cPattern;               /* compound filter pattern	   */
    char      *fileString;            /* C string for file selected        */
    char      *dirString;             /* C string for dir of file selected */
#if XmVersion < 1002
    int       i;
#endif

    XtAddCallback(newFileSB, XmNokCallback, (XtCallbackProc)newFileOKCB,
    	    &done_with_dialog);
    XtAddCallback(newFileSB, XmNcancelCallback, (XtCallbackProc)newFileCancelCB,
	    &done_with_dialog);

#ifndef SGI_CUSTOM
    makeListTypeable(XmFileSelectionBoxGetChild(newFileSB,XmDIALOG_LIST));
    makeListTypeable(XmFileSelectionBoxGetChild(newFileSB,XmDIALOG_DIR_LIST));
#endif
    if (DefaultDirectory != NULL || DefaultPattern != NULL)
    	XtVaSetValues(newFileSB, XmNdirectory, DefaultDirectory,
    		XmNpattern, DefaultPattern, NULL);
    help = createPanelHelp(newFileSB, HelpNew, "Saving a File");
    createYesNoDialog(newFileSB);
    createErrorDialog(newFileSB);
    XtAddCallback(newFileSB, XmNhelpCallback, (XtCallbackProc)newHelpCB, 
    	    (char *)help);
#if XmVersion >= 1002
#ifndef SGI_CUSTOM
    XtVaSetValues(newFileSB, XmNinitialFocus, 
    	    XmFileSelectionBoxGetChild(newFileSB, XmDIALOG_TEXT), NULL);
#else /* SGI_CUSTOM */
    { Widget finder = XmFileSelectionBoxGetChild(newFileSB, SgDIALOG_FINDER);
      if ( finder != NULL )
    	  XtVaSetValues(newFileSB, XmNinitialFocus, finder, NULL);
    }
#endif
#endif
    ManageDialogCenteredOnPointer(newFileSB);

#ifndef SGI_CUSTOM
#if XmVersion < 1002
    /* To give filename text initial focus, revoke default button status for
       the "OK" button.  Dynamic defaulting will restore it as the default
       button after the keyboard focus is established.  Note the voodoo
       below: calling XmProcess traversal FOUR times (a recommendation from
       OSF technical support) somehow succeedes in changing the focus */
    XtVaSetValues(newFileSB, XmNdefaultButton, NULL, NULL);
    for (i=1; i<30; i++)
    	XmProcessTraversal(XmFileSelectionBoxGetChild(newFileSB, XmDIALOG_TEXT),
	    XmTRAVERSE_CURRENT);
#endif

    /* Typing in the directory list is dependent on the list being in the
       same form of alphabetical order expected by the character processing
       routines.  As of about 1.2.3, some Motif libraries seem to have a
       different idea of ordering than is usual for Unix directories.
       To sort them properly, we have to patch the directory and file
       searching routines to re-sort the lists when they change */
    XtVaGetValues(newFileSB, XmNdirSearchProc, &OrigDirSearchProc,
    	    XmNfileSearchProc, &OrigFileSearchProc, NULL);
    XtVaSetValues(newFileSB, XmNdirSearchProc, replacementDirSearchProc,
    	    XmNfileSearchProc, replacementFileSearchProc, NULL);
    sortWidgetList(XmFileSelectionBoxGetChild(newFileSB, XmDIALOG_DIR_LIST));
    sortWidgetList(XmFileSelectionBoxGetChild(newFileSB, XmDIALOG_LIST));
#endif /* SGI_CUSTOM */

    /* Delay the setting of the default name till after the replacement of 
       the search procedures. Otherwise the field is cleared again by certain
       *tif implementations */
    if (defaultName != NULL) {
	Widget nameField = XmFileSelectionBoxGetChild(newFileSB, XmDIALOG_TEXT);
#ifdef LESSTIF_VERSION
        /* Workaround for Lesstif bug (0.93.94 and possibly other versions): 
           if a proportional font is used for the text field and text is
           inserted while the dialog is managed, Lesstif crashes because it
           tries to access a non-existing selection. By creating a temporary 
           dummy selection, the crash is avoided. */
        XmTextFieldSetSelection(nameField, 0, 1, CurrentTime);
	XmTextInsert(nameField, XmTextGetLastPosition(nameField), defaultName);
        XmTextFieldSetSelection(nameField, 0, 0, CurrentTime);
#else
	XmTextInsert(nameField, XmTextGetLastPosition(nameField), defaultName);
#endif
    }

    while (!done_with_dialog)
        XtAppProcessEvent (XtWidgetToApplicationContext(newFileSB), XtIMAll);

    if (SelectResult == GFN_OK) {
	/* See note in existing file routines about freeing the values
	   obtained in the following call */
	XtVaGetValues(newFileSB, XmNdirSpec, &cFileString, XmNdirectory,
		&cDir, XmNpattern, &cPattern, NULL);
	if (DefaultDirectory != NULL) XmStringFree(DefaultDirectory);
	if (DefaultPattern != NULL) XmStringFree(DefaultPattern);
	DefaultDirectory = cDir;
	DefaultPattern = cPattern;
	XmStringGetLtoR(cFileString, XmSTRING_DEFAULT_CHARSET, &fileString);
	/* See note in existing file routines about Motif 2.x bug. */
#ifdef VMS
	/* VMS  won't return `/' as the 1st character of the full file spec.
	 `:' terminates the device name and is not allowed elsewhere */
	if (strchr(fileString, ':') != NULL) {
#else
	if (fileString[0] == '/') {
#endif /* VMS */
	    /* The directory name is already present in the file name or
	       the user entered a full path name. */
	    strcpy(filename, fileString);
	} else {
	    /* Concatenate the directory name and the file name */
   	    XmStringGetLtoR(cDir, XmSTRING_DEFAULT_CHARSET, &dirString);
	    strcpy(filename, dirString);
	    strcat(filename, fileString);
  	    NEditFree(dirString);
	}
	XmStringFree(cFileString);
	NEditFree(fileString);
    }
    XtDestroyWidget(newFileSB);
    return SelectResult;
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
** Return current default match pattern used by GetExistingFilename.
** Can return NULL if no default pattern has been set (meaning use
** a pattern matching all files in the directory) String must be
** freed by the caller using NEditFree.
*/
char *GetFileDialogDefaultPattern(void)
{
    char *string;
    
    if (DefaultPattern == NULL)
    	return NULL;
    XmStringGetLtoR(DefaultPattern, XmSTRING_DEFAULT_CHARSET, &string);
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
}

/*
** Set the current default match pattern to be used by GetExistingFilename.
** "pattern" can be passed as NULL as the equivalent a pattern matching
** all files in the directory.
*/
void SetFileDialogDefaultPattern(char *pattern)
{
    if (DefaultPattern != NULL)
    	XmStringFree(DefaultPattern);
    DefaultPattern = pattern==NULL ? NULL : XmStringCreateSimple(pattern);
}

/*
** Turn on or off the text fiend in the GetExistingFilename file selection
** box, where users can enter the filename by typing.  This is redundant
** with typing in the list, and leads users who are new to nedit to miss
** the more powerful feature in favor of changing the focus and typing
** in the text field.
*/
void SetGetEFTextFieldRemoval(int state)
{
    RemoveRedundantTextField = state;
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

static void newFileOKCB(Widget	w, Boolean *client_data, 
                 XmFileSelectionBoxCallbackStruct *call_data)

{
    char *filename;                   /* name of chosen file             */
    int  fd;                          /* file descriptor                 */
    int  length;		      /* length of file name		 */
    int  response;		      /* response to dialog		 */
    struct stat buf;		      /* status from fstat		 */
    
    XmStringGetLtoR(call_data->value, XmSTRING_DEFAULT_CHARSET, &filename);
    SelectResult = GFN_OK;
    length = strlen(filename);
    if (length == 0 || filename[length-1] == '/') {
    	doErrorDialog("Please supply a name for the file", NULL);
	NEditFree(filename);
    	return;
    }

#ifdef VMS
    if (strchr(filename,';') && (fd = open(filename, O_RDONLY, 0)) != -1) {
#else  /* not VMS*/
    if ((fd = open(filename, O_RDONLY, 0)) != -1) {     /* exists */
#endif /*VMS*/
	fstat(fd, &buf);
        close(fd);
	if (buf.st_mode & S_IFDIR) {
            doErrorDialog("Error: %s is a directory", filename);
	    NEditFree(filename);
	    return;
	}
        response = doYesNoDialog(filename);
#ifdef VMS
	if (response) {
            if (access(filename, 2) != 0) { /* have write/delete access? */
		doErrorDialog("Error: can't overwrite %s ", filename);
		NEditFree(filename);
		return;
	    }
	} else {
#else
        if (!response) {
#endif /*VMS*/
             return;
	}
    } else {
	if ((fd = creat(filename, PERMS)) == -1) {
            doErrorDialog("Error: can't create %s ", filename);
	    NEditFree(filename);
	    return;
	} else {
	    close(fd);
            remove(filename);
	}
    }
    NEditFree(filename);
    *client_data = True;			    /* done with dialog */
}


static void newFileCancelCB(Widget w, Boolean *client_data, caddr_t call_data)
{
    SelectResult = GFN_CANCEL;
    *client_data = True;
}

static void newHelpCB(Widget w, Widget helpPanel, caddr_t call_data)
{
    ManageDialogCenteredOnPointer(helpPanel);
}

static void existOkCB(Widget w, Boolean * client_data,
	       XmFileSelectionBoxCallbackStruct *call_data)
{
    char *filename;                   /* name of chosen file             */
    int  fd;                          /* file descriptor                 */
    int  length;		      /* length of file name		 */
    
    XmStringGetLtoR(call_data->value, XmSTRING_DEFAULT_CHARSET, &filename);
    SelectResult = GFN_OK;
    length = strlen(filename);
    if (length == 0 || filename[length-1] == '/') {
    	doErrorDialog("Please select a file to open", NULL);
    	NEditFree(filename);
    	return;
    } else    if ((fd = open(filename, O_RDONLY,0))  == -1) {
    	doErrorDialog("Error: can't open %s ", filename);
    	NEditFree(filename);
        return;
    } else
    	close(fd);
    NEditFree(filename);
    	
    *client_data = True;		/* done with dialog		*/
}


static void existCancelCB(Widget w, Boolean * client_data, caddr_t call_data)
{
    SelectResult = GFN_CANCEL;
    *client_data = True;		/* done with dialog		*/
}

static void yesNoOKCB(Widget w, caddr_t client_data, caddr_t call_data)
{
    YesNoResult = ynYes;
}

static void existHelpCB(Widget w, Widget helpPanel, caddr_t call_data)
{
    ManageDialogCenteredOnPointer(helpPanel);
}

static void errorOKCB(Widget w, caddr_t client_data, caddr_t call_data)
{
    ErrorDone = True;
}

static void yesNoCancelCB(Widget w, caddr_t client_data, caddr_t call_data)
{
    YesNoResult = ynNo;
}

static Widget createPanelHelp(Widget parent, const char *helpText, const char *title)
{
    Arg al[20];
    int ac;
    Widget form, text, button;
    XmString st1;
    
    ac = 0;
    form = CreateFormDialog(parent, "helpForm", al, ac);

    ac = 0;
    XtSetArg (al[ac], XmNbottomAttachment, XmATTACH_FORM);  ac++;
    XtSetArg (al[ac], XmNtopAttachment, XmATTACH_NONE);  ac++;
    XtSetArg(al[ac], XmNlabelString, st1=XmStringCreateLtoR ("OK", 
                      XmSTRING_DEFAULT_CHARSET)); ac++;
    XtSetArg (al[ac], XmNmarginWidth, BUTTON_WIDTH_MARGIN);  ac++;
    button = XmCreatePushButtonGadget(form, "ok", al, ac);
    XtAddCallback(button, XmNactivateCallback, (XtCallbackProc)helpDismissCB,
    	    (char *)form);
    XmStringFree(st1);
    XtManageChild(button);
    SET_ONE_RSRC(form, XmNdefaultButton, button);
    
    ac = 0;
    XtSetArg(al[ac], XmNrows, 15);  ac++;
    XtSetArg(al[ac], XmNcolumns, 60);  ac++;
    XtSetArg(al[ac], XmNresizeHeight, False);  ac++;
    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNwordWrap, True);  ac++;
    XtSetArg(al[ac], XmNscrollHorizontal, False);  ac++;
    XtSetArg(al[ac], XmNeditMode, XmMULTI_LINE_EDIT);  ac++;
    XtSetArg(al[ac], XmNeditable, False);  ac++;
    XtSetArg(al[ac], XmNvalue, helpText);  ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM);  ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM);  ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_WIDGET);  ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM);  ac++;
    XtSetArg(al[ac], XmNbottomWidget, button);  ac++;
    text = XmCreateScrolledText(form, "helpText", al, ac);
    AddMouseWheelSupport(text);
    XtManageChild(text);
    
    SET_ONE_RSRC(XtParent(form), XmNtitle, title);
    
    return form;
}

static void helpDismissCB(Widget w, Widget helpPanel, caddr_t call_data)
{
    XtUnmanageChild(helpPanel);
}

/*
** Add ability for user to type filenames to a list widget
*/
static void makeListTypeable(Widget listW)
{
    XtAddEventHandler(listW, KeyPressMask, False, listCharEH, NULL);
}

/*
** Action procedure for processing characters typed in a list, finds the
** first item matching the characters typed so far.
*/
static int nKeystrokes = 0; /* Global key stroke history counter */
static void listCharEH(Widget w, XtPointer callData, XEvent *event,
	Boolean *continueDispatch)
{
    char charString[5], c, *itemString;
    int nChars, nItems, i, cmp, selectPos, topPos, nVisible;
    XmString *items;
    KeySym kSym;
    char name[MAXPATHLEN], path[MAXPATHLEN];
    static char keystrokes[MAX_LIST_KEYSTROKES];
    static Time lastKeyTime = 0;
    
    /* Get the ascii character code represented by the event */
    nChars = XLookupString((XKeyEvent *)event, charString, sizeof(charString),
    	    &kSym, NULL);
    c = charString[0];
    
    /* Process selected control keys, but otherwise ignore the keystroke
       if it isn't a single printable ascii character */
    *continueDispatch = False;
    if (kSym==XK_BackSpace || kSym==XK_Delete) {
    	nKeystrokes = nKeystrokes > 0 ? nKeystrokes-1 : 0;
    	return;
    } else if (kSym==XK_Clear || kSym==XK_Cancel || kSym==XK_Break) {
    	nKeystrokes = 0;
    	return;
    } else if (nChars!=1 || c<0x021 || c>0x07e) {
    	*continueDispatch = True;
    	return;
    }
    
    /* Throw out keystrokes and start keystroke accumulation over from 
       scratch if user waits more than MAX_LIST_KESTROKE_WAIT milliseconds */
    if (((XKeyEvent *)event)->time - lastKeyTime > MAX_LIST_KESTROKE_WAIT)
    	nKeystrokes = 0;
    lastKeyTime = ((XKeyEvent *)event)->time;
    	
    /* Accumulate the current keystroke, just beep if there are too many */
    if (nKeystrokes >= MAX_LIST_KEYSTROKES)
    	XBell(XtDisplay(w), 0);
    else
#ifdef VMS
    	keystrokes[nKeystrokes++] = toupper(c);
#else
    	keystrokes[nKeystrokes++] = c;
#endif
    
    /* Get the items (filenames) in the list widget */
    XtVaGetValues(w, XmNitems, &items, XmNitemCount, &nItems, NULL);
    
    /* compare them with the accumulated user keystrokes & decide the
       appropriate line in the list widget to select */
    selectPos = 0;
    for (i=0; i<nItems; i++) {
    	XmStringGetLtoR(items[i], XmSTRING_DEFAULT_CHARSET, &itemString);
    	if (ParseFilename(itemString, name, path) != 0) {
	   NEditFree(itemString);
	   return;
	}
	NEditFree(itemString);
    	cmp = strncmp(name, keystrokes, nKeystrokes);
    	if (cmp == 0) {
    	    selectPos = i+1;
    	    break;
    	} else if (cmp > 0) {
    	    selectPos = i;
    	    break;
    	}
    }

    /* Make the selection, and make sure it will be visible */
    XmListSelectPos(w, selectPos, True);
    if (selectPos == 0) /* XmListSelectPos curiously returns 0 for last item */
    	selectPos = nItems + 1;
    XtVaGetValues(w, XmNtopItemPosition, &topPos,
    	    XmNvisibleItemCount, &nVisible, NULL);
    if (selectPos < topPos)
    	XmListSetPos(w, selectPos-2 > 1 ? selectPos-2 : 1);
    else if (selectPos > topPos+nVisible-1)
    	XmListSetBottomPos(w, selectPos+2 <= nItems ? selectPos+2 : 0);
    /* For LessTif 0.89.9. Obsolete now? */
    XmListSelectPos(w, selectPos, True);
}

/*
** Replacement directory and file search procedures for the file selection
** box to re-sort the items in a standard order.  This is a patch, and not
** a very good one, for the problem that in some Motif versions, the directory
** list is sorted differently, such that typing of filenames fails because
** it expects strcmp alphabetical order, as opposed to strcasecmp.  Most
** users prefer the old ordering, which is what this enforces, but if
** ifdefs can be found that will correctly predict the ordering and adjust
** listCharEH above, instead of resorting to re-sorting, it should be done.
** This obviously wastes valuable time as the selection box is popping up
** and should be removed.  These routines also leak memory like a seive,
** because Motif's inconsistent treatment of memory in list widgets does
** not allow us to free lists that we pass in, and most Motif versions
** don't clean it up properly.
*/
static void replacementDirSearchProc(Widget w, XtPointer searchData)
{
    Boolean updated;
    
    /* Call the original search procedure to do the actual search */
    (*OrigDirSearchProc)(w, searchData);
    /* Refreshing a list clears the keystroke history, even if no update. */
    nKeystrokes = 0;
    XtVaGetValues(w, XmNlistUpdated, &updated, NULL);
    if (!updated)
    	return;
    	
    /* Sort the items in the list */
    sortWidgetList(XmFileSelectionBoxGetChild(w, XmDIALOG_DIR_LIST));
}

static void replacementFileSearchProc(Widget w, XtPointer searchData)
{
    Boolean updated;
    
    /* Call the original search procedure to do the actual search */
    (*OrigFileSearchProc)(w, searchData);
    /* Refreshing a list clears the keystroke history, even if no update. */
    nKeystrokes = 0;
    XtVaGetValues(w, XmNlistUpdated, &updated, NULL);
    if (!updated)
    	return;
    	
    /* Sort the items in the list */
    sortWidgetList(XmFileSelectionBoxGetChild(w, XmDIALOG_LIST));
}

/*
** Sort the items in a list widget "listWidget"
*/
static void sortWidgetList(Widget listWidget)
{
    XmString *items, *sortedItems;
    int nItems, i;
    
    /* OpenMotif 2.3 will crash if we try to replace the items, when they
       are selected. This function is only called when we refresh the
       contents anyway. */
    XmListDeselectAllItems(listWidget);
    XtVaGetValues(listWidget, XmNitems, &items, XmNitemCount, &nItems, NULL);
    sortedItems = (XmString *)NEditMalloc(sizeof(XmString) * nItems);
    for (i=0; i<nItems; i++)
    	sortedItems[i] = XmStringCopy(items[i]);
    qsort(sortedItems, nItems, sizeof(XmString), compareXmStrings);
    XmListReplaceItemsPos(listWidget, sortedItems, nItems, 1);
    for (i=0; i<nItems; i++)
    	XmStringFree(sortedItems[i]);
    NEditFree((char *)sortedItems);
}

/*
** Compare procedure for qsort for sorting a list of XmStrings
*/
static int compareXmStrings(const void *string1, const void *string2)
{
    char *s1, *s2;
    int result;
    
    XmStringGetLtoR(*(XmString *)string1, XmSTRING_DEFAULT_CHARSET, &s1);
    XmStringGetLtoR(*(XmString *)string2, XmSTRING_DEFAULT_CHARSET, &s2);
    result = strcmp(s1, s2);
    NEditFree(s1);
    NEditFree(s2);
    return result;
}
