/*******************************************************************************
*									       *
* printUtils.c -- Nirvana library Printer Menu	& Printing Routines   	       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
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
*                                        				       *
* April 20, 1992							       *
*									       *
* Written by Arnulfo Zepeda-Navratil				               *
*            Centro de Investigacion y Estudio Avanzados ( CINVESTAV )         *
*            Dept. Fisica - Mexico                                             *
*            BITNET: ZEPEDA@CINVESMX                                           *
*									       *
* Modified by Donna Reid and Joy Kyriakopulos 4/8/93 - VMS port		       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "printUtils.h"
#include "DialogF.h"
#include "misc.h"
#include "prefFile.h"
#include "nedit_malloc.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef VMS
#include "vmsparam.h"
#include <ssdef.h>
#include <lib$routines.h>
#include <descrip.h>
#include <starlet.h>
#include <lnmdef.h>
#include <clidef.h>
#else
#ifdef USE_DIRENT
#include <dirent.h>
#else
#include <sys/dir.h>
#endif /* USE_DIRENT */
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/
#include <sys/stat.h>

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/SeparatoG.h>
#include <Xm/Text.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* Separator between directory references in PATH environmental variable */
#ifdef __EMX__  /* For OS/2 */
#define SEPARATOR ';'
#else
#define SEPARATOR ':'  
#endif

/* Number of extra pixels down to place a label even with a text widget */
#define LABEL_TEXT_DIFF 6

/* Maximum text string lengths */
#define MAX_OPT_STR 20
#define MAX_QUEUE_STR 60
#define MAX_INT_STR 13
#define MAX_HOST_STR 100
#define MAX_PCMD_STR 100
#define MAX_NAME_STR 100
#define MAX_CMD_STR 256
#define VMS_MAX_JOB_NAME_STR 39

#define N_PRINT_PREFS 7 /* must agree with number of preferences below */
struct printPrefDescrip {
    PrefDescripRec printCommand;
    PrefDescripRec copiesOption;
    PrefDescripRec queueOption;
    PrefDescripRec nameOption;
    PrefDescripRec hostOption;
    PrefDescripRec defaultQueue;
    PrefDescripRec defaultHost;
};

/* Function Prototypes */
static Widget createForm(Widget parent);
static void allowOnlyNumInput(Widget widget, caddr_t client_data,
	XmTextVerifyCallbackStruct *call_data);
static void noSpaceOrPunct(Widget widget, caddr_t client_data,
	XmTextVerifyCallbackStruct *call_data);
static void updatePrintCmd(Widget w, caddr_t client_data, caddr_t call_data);
static void printCmdModified(Widget w, caddr_t client_data, caddr_t call_data);
static void printButtonCB(Widget widget, caddr_t client_data, caddr_t call_data);
static void cancelButtonCB(Widget widget, caddr_t client_data, caddr_t call_data);
static void setQueueLabelText(void);
static int fileInDir(const char *filename, const char *dirpath, unsigned short mode_flags);
static int fileInPath(const char *filename, unsigned short mode_flags);
static int flprPresent(void);
#ifdef USE_LPR_PRINT_CMD
static void getLprQueueDefault(char *defqueue);
#endif
#ifndef USE_LPR_PRINT_CMD
static void getLpQueueDefault(char *defqueue);
#endif
static void setHostLabelText(void);
#ifdef VMS
static void getVmsQueueDefault(char *defqueue);
#else
static void getFlprHostDefault(char *defhost);
static void getFlprQueueDefault(char *defqueue);
#endif

/* Module Global Variables */
static Boolean  DoneWithDialog;
static Boolean	PreferencesLoaded = False;
static Widget	Form;
static Widget	Label2;
static Widget	Label3;
static Widget	Text1;
static Widget	Text2;
static Widget	Text3;
static Widget	Text4;
static const char *PrintFileName;
static const char *PrintJobName;
static char PrintCommand[MAX_PCMD_STR];	/* print command string */
static char CopiesOption[MAX_OPT_STR];	/* # of copies argument string */
static char QueueOption[MAX_OPT_STR];	/* queue name argument string */
static char NameOption[MAX_OPT_STR];	/* print job name argument string */
static char HostOption[MAX_OPT_STR];	/* host name argument string */
static char DefaultQueue[MAX_QUEUE_STR];/* default print queue */
static char DefaultHost[MAX_HOST_STR];	/* default host name */
static char Copies[MAX_INT_STR] = "";	/* # of copies last entered by user */
static char Queue[MAX_QUEUE_STR] = "";	/* queue name last entered by user */
static char Host[MAX_HOST_STR] = "";	/* host name last entered by user */
static char CmdText[MAX_CMD_STR] = "";	/* print command last entered by user */
static int  CmdFieldModified = False;	/* user last changed the print command
					   field, so don't trust the rest */
#ifdef VMS
static int DeleteFile;			/* append /DELETE to VMS print command*/
#endif /*VMS*/

static struct printPrefDescrip PrintPrefDescrip = {
    {"printCommand", "PrintCommand", PREF_STRING, NULL,
    	PrintCommand, (void *)MAX_PCMD_STR, False},
    {"printCopiesOption", "PrintCopiesOption", PREF_STRING, NULL,
    	CopiesOption, (void *)MAX_OPT_STR, False},
    {"printQueueOption", "PrintQueueOption", PREF_STRING, NULL,
    	QueueOption, (void *)MAX_OPT_STR, False},
    {"printNameOption", "PrintNameOption", PREF_STRING, NULL,
    	NameOption, (void *)MAX_OPT_STR, False},
    {"printHostOption", "PrintHostOption", PREF_STRING, NULL,
    	HostOption, (void *)MAX_OPT_STR, False},
    {"printDefaultQueue", "PrintDefaultQueue", PREF_STRING, NULL,
    	DefaultQueue, (void *)MAX_QUEUE_STR, False},
    {"printDefaultHost", "PrintDefaultHost", PREF_STRING, NULL,
    	DefaultHost, (void *)MAX_HOST_STR, False},
};

/*
** PrintFile(Widget parent, char *printFile, char *jobName);
**
** function to put up an application-modal style Print Panel dialog 
** box.
**
**	parent		Parent widget for displaying dialog
**	printFile	File to print (assumed to be a temporary file
**			and not revealed to the user)
**	jobName		Title for the print banner page
*/
#ifdef VMS
void PrintFile(Widget parent, const char *printFile, const char *jobName, int delete)
#else
void PrintFile(Widget parent, const char *printFile, const char *jobName)
#endif /*VMS*/
{
    /* In case the program hasn't called LoadPrintPreferences, set up the
       default values for the print preferences */
    if (!PreferencesLoaded)
    	LoadPrintPreferences(NULL, "", "", True);
    
    /* Make the PrintFile information available to the callback routines */
    PrintFileName = printFile;
    PrintJobName = jobName;
#ifdef VMS
    DeleteFile = delete;
#endif /*VMS*/

    /* Create and display the print dialog */
    DoneWithDialog = False;
    Form = createForm(parent);
    ManageDialogCenteredOnPointer(Form);

    /* Process events until the user is done with the print dialog */
    while (!DoneWithDialog)
        XtAppProcessEvent(XtWidgetToApplicationContext(Form), XtIMAll);
    
    /* Destroy the dialog.  Print dialogs are not preserved across calls
       to PrintFile so that it may be called with different parents and
       to generally simplify the call (this, of course, makes it slower) */ 
    XtDestroyWidget(Form);
}

/*
** LoadPrintPreferences
**
** Read an X database to obtain print dialog preferences.
**
**	prefDB		X database potentially containing print preferences
**	appName		Application name which can be used to qualify 
**			resource names for database lookup.
**	appClass	Application class which can be used to qualify 
**			resource names for database lookup.
**	lookForFlpr	Check if the flpr print command is installed
**			and use that for the default if it's found.
**			(flpr is a Fermilab utility for printing on
**			arbitrary systems that support the lpr protocol)
*/
void LoadPrintPreferences(XrmDatabase prefDB, const char *appName,
        const char *appClass, int lookForFlpr)
{
    static char defaultQueue[MAX_QUEUE_STR], defaultHost[MAX_HOST_STR];

#ifdef VMS
    /* VMS built-in print command */
    getVmsQueueDefault(defaultQueue);
    PrintPrefDescrip.printCommand.defaultString = "print";
    PrintPrefDescrip.copiesOption.defaultString = "/copies=";
    PrintPrefDescrip.queueOption.defaultString = "/queue=";
    PrintPrefDescrip.nameOption.defaultString = "/name=";
    PrintPrefDescrip.hostOption.defaultString = "";
    PrintPrefDescrip.defaultQueue.defaultString = defaultQueue;
    PrintPrefDescrip.defaultHost.defaultString = "";
#else

    /* check if flpr is installed, and otherwise choose appropriate
       printer per system type */
    if (lookForFlpr && flprPresent()) {
    	getFlprQueueDefault(defaultQueue);
    	getFlprHostDefault(defaultHost);
    	PrintPrefDescrip.printCommand.defaultString = "flpr";
    	PrintPrefDescrip.copiesOption.defaultString = "";
    	PrintPrefDescrip.queueOption.defaultString = "-q";
    	PrintPrefDescrip.nameOption.defaultString = "-j ";
    	PrintPrefDescrip.hostOption.defaultString = "-h";
    	PrintPrefDescrip.defaultQueue.defaultString = defaultQueue;
    	PrintPrefDescrip.defaultHost.defaultString = defaultHost;
    } else {
#ifdef USE_LPR_PRINT_CMD
	getLprQueueDefault(defaultQueue);
     	PrintPrefDescrip.printCommand.defaultString = "lpr";
    	PrintPrefDescrip.copiesOption.defaultString = "-# ";
    	PrintPrefDescrip.queueOption.defaultString = "-P ";
    	PrintPrefDescrip.nameOption.defaultString = "-J ";
    	PrintPrefDescrip.hostOption.defaultString = "";
    	PrintPrefDescrip.defaultQueue.defaultString = defaultQueue;
    	PrintPrefDescrip.defaultHost.defaultString = "";
#else
     	getLpQueueDefault(defaultQueue);
     	PrintPrefDescrip.printCommand.defaultString = "lp"; /* was lp -c */
    	PrintPrefDescrip.copiesOption.defaultString = "-n";
    	PrintPrefDescrip.queueOption.defaultString = "-d";
    	PrintPrefDescrip.nameOption.defaultString = "-t";
    	PrintPrefDescrip.hostOption.defaultString = "";
    	PrintPrefDescrip.defaultQueue.defaultString = defaultQueue;
    	PrintPrefDescrip.defaultHost.defaultString = "";
#endif
    }
#endif
    
    /* Read in the preferences from the X database using the mechanism from
       prefFile.c (this allows LoadPrintPreferences to work before any
       widgets are created, which is more convenient than XtGetApplication-
       Resources for applications which have no main window) */
    RestorePreferences(NULL, prefDB, appName, appClass,
    	    (PrefDescripRec *)&PrintPrefDescrip, N_PRINT_PREFS);
    
    PreferencesLoaded = True;
}

static Widget createForm(Widget parent)
{
    Widget form, printOk, printCancel, label1, separator;
    Widget topWidget = NULL;
    XmString st0;
    Arg args[65];
    int argcnt;
    Widget bwidgetarray [30];
    int bwidgetcnt = 0;
 
    /************************ FORM ***************************/
    argcnt = 0;
    XtSetArg(args[argcnt], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
    argcnt++;
    XtSetArg(args[argcnt], XmNdialogTitle, (st0=XmStringCreateLtoR(
		"Print", XmSTRING_DEFAULT_CHARSET))); argcnt++;
    XtSetArg(args[argcnt], XmNautoUnmanage, False); argcnt++;
    form = CreateFormDialog(parent, "printForm", args, argcnt);
    XtVaSetValues(form, XmNshadowThickness, 0, NULL);
    
    XmStringFree( st0 );

    /*********************** LABEL 1 and TEXT BOX 1 *********************/
    if (CopiesOption[0] != '\0') {
	argcnt = 0;
	XtSetArg(args[argcnt], XmNlabelString, (st0=XmStringCreateLtoR(
		"Number of copies (1)", XmSTRING_DEFAULT_CHARSET))); argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'N'); argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM); argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, LABEL_TEXT_DIFF+5); argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM); argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 8); argcnt++;
	label1 = XmCreateLabelGadget(form, "label1", args, argcnt);
	XmStringFree( st0 );
	bwidgetarray[bwidgetcnt] = label1; bwidgetcnt++;
    
	argcnt = 0;
	XtSetArg(args[argcnt], XmNshadowThickness, (short)2); argcnt++;
	XtSetArg(args[argcnt], XmNcolumns, 3); argcnt++;
	XtSetArg(args[argcnt], XmNrows, 1); argcnt++;
	XtSetArg(args[argcnt], XmNvalue , Copies); argcnt++;
	XtSetArg(args[argcnt], XmNmaxLength, 3); argcnt++;
        XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_FORM); argcnt++;  
	XtSetArg(args[argcnt], XmNtopOffset, 5); argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET); argcnt++; 
	XtSetArg(args[argcnt], XmNleftWidget, label1); argcnt++;
	Text1 = XmCreateText(form, "text1", args, argcnt);
	bwidgetarray[bwidgetcnt] = Text1; bwidgetcnt++; 
	XtAddCallback(Text1, XmNmodifyVerifyCallback,
		(XtCallbackProc)allowOnlyNumInput, NULL);
	XtAddCallback(Text1, XmNvalueChangedCallback,
		(XtCallbackProc)updatePrintCmd, NULL);
	RemapDeleteKey(Text1);
	topWidget = Text1;
	XtVaSetValues(label1, XmNuserData, Text1, NULL); /* mnemonic procesing */
    }
    
    /************************ LABEL 2 and TEXT 2 ************************/
    if (QueueOption[0] != '\0') {
	argcnt = 0;
	XtSetArg(args[argcnt], XmNlabelString, (st0=XmStringCreateLtoR(
		    "  ", XmSTRING_DEFAULT_CHARSET))); argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'Q'); argcnt++;
	XtSetArg(args[argcnt], XmNrecomputeSize, True); argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment,
		topWidget==NULL?XmATTACH_FORM:XmATTACH_WIDGET); argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, topWidget); argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM); argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, LABEL_TEXT_DIFF+4); argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 8); argcnt++;
	Label2 = XmCreateLabelGadget(form, "label2", args, argcnt);
	XmStringFree(st0);
	bwidgetarray[bwidgetcnt] = Label2; bwidgetcnt++;
	setQueueLabelText();

	argcnt = 0;
	XtSetArg(args[argcnt], XmNshadowThickness, (short)2); argcnt++;
	XtSetArg(args[argcnt], XmNcolumns, (short)17); argcnt++;
	XtSetArg(args[argcnt], XmNmaxLength, MAX_QUEUE_STR); argcnt++;
	XtSetArg(args[argcnt], XmNvalue, Queue); argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM); argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET); argcnt++;
	XtSetArg(args[argcnt], XmNleftWidget, Label2 ); argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment,
		topWidget==NULL?XmATTACH_FORM:XmATTACH_WIDGET); argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, topWidget ); argcnt++;
	XtSetArg(args[argcnt], XmNrightOffset, 8); argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, 4); argcnt++;
	Text2 = XmCreateText(form, "text2", args, argcnt);
	XtAddCallback(Text2, XmNmodifyVerifyCallback,
		(XtCallbackProc)noSpaceOrPunct, NULL);
	XtAddCallback(Text2, XmNvalueChangedCallback,
		(XtCallbackProc)updatePrintCmd, NULL);
	bwidgetarray[bwidgetcnt] = Text2; bwidgetcnt++;
	RemapDeleteKey(Text2);
	XtVaSetValues(Label2, XmNuserData, Text2, NULL); /* mnemonic procesing */
	topWidget = Text2;
    }
    
    /****************** LABEL 3 and TEXT 3 *********************/
    if (HostOption[0] != '\0') {
	argcnt = 0;
	XtSetArg(args[argcnt], XmNlabelString, (st0=XmStringCreateLtoR(
		   "  ", XmSTRING_DEFAULT_CHARSET))); argcnt++;
	XtSetArg(args[argcnt], XmNmnemonic, 'H'); argcnt++;
	XtSetArg(args[argcnt], XmNrecomputeSize, True); argcnt++;
	XtSetArg(args[argcnt], XmNvalue , ""); argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment,
		topWidget==NULL?XmATTACH_FORM:XmATTACH_WIDGET); argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, topWidget ); argcnt++;  
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM); argcnt++;
	XtSetArg(args[argcnt], XmNleftOffset, 8); argcnt++;
	XtSetArg(args[argcnt], XmNtopOffset, LABEL_TEXT_DIFF+4); argcnt++;
	Label3 = XmCreateLabelGadget(form, "label3", args, argcnt);
	XmStringFree(st0);
	bwidgetarray[bwidgetcnt] = Label3; bwidgetcnt++;
	setHostLabelText();      

	argcnt = 0;
	XtSetArg(args[argcnt], XmNcolumns, 17); argcnt++;
	XtSetArg(args[argcnt], XmNrows, 1); argcnt++;
	XtSetArg(args[argcnt], XmNvalue, Host); argcnt++;
	XtSetArg(args[argcnt], XmNmaxLength, MAX_HOST_STR); argcnt++;
	XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM); argcnt++;
	XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_WIDGET); argcnt++;
	XtSetArg(args[argcnt], XmNleftWidget, Label3 ); argcnt++;
	XtSetArg(args[argcnt], XmNtopAttachment,
		topWidget==NULL?XmATTACH_FORM:XmATTACH_WIDGET); argcnt++;
	XtSetArg(args[argcnt], XmNtopWidget, topWidget ); argcnt++;  
	XtSetArg(args[argcnt], XmNrightOffset, 8); argcnt++;
    	XtSetArg(args[argcnt], XmNtopOffset, 4); argcnt++;
	Text3 = XmCreateText(form, "Text3", args, argcnt);
	XtAddCallback(Text3, XmNmodifyVerifyCallback,
		(XtCallbackProc)noSpaceOrPunct, NULL);
	XtAddCallback(Text3, XmNvalueChangedCallback,
		(XtCallbackProc)updatePrintCmd, NULL);
	bwidgetarray[bwidgetcnt] = Text3; bwidgetcnt++; 
    	RemapDeleteKey(Text3);
	XtVaSetValues(Label3, XmNuserData, Text3, NULL); /* mnemonic procesing */
    	topWidget = Text3;
    }

    /************************** TEXT 4 ***************************/
    argcnt = 0;
    XtSetArg(args[argcnt], XmNvalue, CmdText); argcnt++;
    XtSetArg(args[argcnt], XmNcolumns, 50); argcnt++;
    XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM); argcnt++;
    XtSetArg(args[argcnt], XmNleftOffset, 8); argcnt++;
    XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET); argcnt++;
    XtSetArg(args[argcnt], XmNtopOffset, 8); argcnt++;
    XtSetArg(args[argcnt], XmNtopWidget, topWidget ); argcnt++;
    XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM); argcnt++;
    XtSetArg(args[argcnt], XmNrightOffset, 8); argcnt++;
    Text4 = XmCreateText(form, "Text4", args, argcnt);
    XtAddCallback(Text4, XmNmodifyVerifyCallback,
	    (XtCallbackProc)printCmdModified, NULL);
    bwidgetarray[bwidgetcnt] = Text4; bwidgetcnt++; 
    RemapDeleteKey(Text4);
    topWidget = Text4;
    if (!CmdFieldModified)
    	updatePrintCmd(NULL, NULL, NULL);

    /*********************** SEPARATOR **************************/
    argcnt = 0;
    XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_FORM); argcnt++;
    XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET); argcnt++;
    XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_FORM); argcnt++;
    XtSetArg(args[argcnt], XmNtopOffset, 8); argcnt++;
    XtSetArg(args[argcnt], XmNtopWidget, topWidget ); argcnt++;
    separator = XmCreateSeparatorGadget(form, "separator", args, argcnt);
    bwidgetarray[bwidgetcnt] = separator; bwidgetcnt++; 
    topWidget = separator;

    /********************** CANCEL BUTTON *************************/
    argcnt = 0;
    XtSetArg(args[argcnt], XmNlabelString, (st0=XmStringCreateLtoR(
		"Cancel", XmSTRING_DEFAULT_CHARSET))); argcnt++;
    XtSetArg(args[argcnt], XmNleftAttachment, XmATTACH_POSITION); argcnt++;
    XtSetArg(args[argcnt], XmNleftPosition, 60); argcnt++;
    XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET); argcnt++;
    XtSetArg(args[argcnt], XmNtopWidget, topWidget ); argcnt++;  
    XtSetArg(args[argcnt], XmNtopOffset, 7); argcnt++;
    printCancel = XmCreatePushButton(form, "printCancel", args, argcnt);
    XmStringFree( st0 );
    bwidgetarray[bwidgetcnt] = printCancel; bwidgetcnt++;
    XtAddCallback (printCancel, XmNactivateCallback,
    	    (XtCallbackProc)cancelButtonCB, NULL);

    /*********************** PRINT BUTTON **************************/
    argcnt = 0;
    XtSetArg(args[argcnt], XmNlabelString, (st0=XmStringCreateLtoR(
		"Print", XmSTRING_DEFAULT_CHARSET))); argcnt++;
    XtSetArg(args[argcnt], XmNshowAsDefault, True); argcnt++;
    XtSetArg(args[argcnt], XmNrightAttachment, XmATTACH_POSITION); argcnt++;
    XtSetArg(args[argcnt], XmNrightPosition, 40); argcnt++;
    XtSetArg(args[argcnt], XmNtopAttachment, XmATTACH_WIDGET); argcnt++;
    XtSetArg(args[argcnt], XmNtopWidget, topWidget ); argcnt++;  
    XtSetArg(args[argcnt], XmNtopOffset, 7); argcnt++;
    printOk = XmCreatePushButton(form, "printOk", args, argcnt);
    XmStringFree( st0 );
    bwidgetarray[bwidgetcnt] = printOk; bwidgetcnt++; 
    XtAddCallback (printOk, XmNactivateCallback,
    	    (XtCallbackProc)printButtonCB, NULL);

    argcnt = 0;
    XtSetArg(args[argcnt], XmNcancelButton, printCancel); argcnt++;
    XtSetArg(args[argcnt], XmNdefaultButton, printOk); argcnt++;
    XtSetValues(form, args, argcnt);

    XtManageChildren(bwidgetarray, bwidgetcnt);
    AddDialogMnemonicHandler(form, FALSE);
    return form;
}

static void setQueueLabelText(void)
{
    Arg args[15];
    int	argcnt;
    XmString	st0;
    char        tmp_buf[MAX_QUEUE_STR+8];

    if (DefaultQueue[0] != '\0')
    	sprintf(tmp_buf, "Queue (%s)", DefaultQueue);
    else
    	sprintf(tmp_buf, "Queue");
    argcnt = 0;
    XtSetArg(args[argcnt], XmNlabelString, (st0=XmStringCreateLtoR(
		tmp_buf, XmSTRING_DEFAULT_CHARSET))); argcnt++;
    XtSetValues (Label2, args, argcnt);
    XmStringFree( st0 );
}

static void setHostLabelText(void)
{
    Arg args[15];
    int	argcnt;
    XmString st0;
    char tmp_buf[MAX_HOST_STR+7];

    if (strcmp(DefaultHost, ""))
    	sprintf(tmp_buf, "Host (%s)", DefaultHost);
    else
    	sprintf(tmp_buf, "Host");
    argcnt = 0;
    XtSetArg(args[argcnt], XmNlabelString, (st0=XmStringCreateLtoR(
		tmp_buf, XmSTRING_DEFAULT_CHARSET))); argcnt++;

    XtSetValues (Label3, args, argcnt);
    XmStringFree( st0 );
}

static void allowOnlyNumInput(Widget widget, caddr_t client_data,
			      XmTextVerifyCallbackStruct *call_data)
{
    int i, textInserted, nInserted;

    nInserted = call_data->text->length;
    textInserted = (nInserted > 0);
    if ((call_data->reason == XmCR_MODIFYING_TEXT_VALUE) && textInserted) {
	for (i=0; i<nInserted; i++) {
            if (!isdigit((unsigned char)call_data->text->ptr[i])) {
            	call_data->doit = False;
            	return;
            }
        }
    }
    call_data->doit = True;
}

/*
** Prohibit a relatively random sampling of characters that will cause
** problems on command lines
*/
static void noSpaceOrPunct(Widget widget, caddr_t client_data,
			      XmTextVerifyCallbackStruct *call_data)
{
    int i, j, textInserted, nInserted;
#ifndef VMS
    static char prohibited[] = " \t,;|<>()[]{}!@?";
#else
    static char prohibited[] = " \t,;|@+";
#endif

    nInserted = call_data->text->length;
    textInserted = (nInserted > 0);
    if ((call_data->reason == XmCR_MODIFYING_TEXT_VALUE) && textInserted) {
	for (i=0; i<nInserted; i++) {
            for (j=0; j<(int)XtNumber(prohibited); j++) {
        	if (call_data->text->ptr[i] == prohibited[j]) {
            	    call_data->doit = False;
            	    return;
        	}
            }
        }
    }
    call_data->doit = True;
}

static void updatePrintCmd(Widget w, caddr_t client_data, caddr_t call_data)
{
    char command[MAX_CMD_STR], copiesArg[MAX_OPT_STR+MAX_INT_STR];
    char jobArg[MAX_NAME_STR], hostArg[MAX_OPT_STR+MAX_HOST_STR];
    char queueArg[MAX_OPT_STR+MAX_QUEUE_STR];
    char *str;
    int nCopies;
#ifdef VMS
    char printJobName[VMS_MAX_JOB_NAME_STR+1];
#endif /*VMS*/

    /* read each text field in the dialog and generate the corresponding
       command argument */
    if (CopiesOption[0] == '\0') {
    	copiesArg[0] = '\0';
    } else {
	str = XmTextGetString(Text1);
	if (str[0] == '\0') {
	    copiesArg[0] = '\0';
	} else {
	    if (sscanf(str, "%d", &nCopies) != 1) {
            	copiesArg[0] = '\0';
            } else {
            	sprintf(copiesArg, " %s%s", CopiesOption, str);
            }
	}
	NEditFree(str);
    }
    if (QueueOption[0] == '\0') {
    	queueArg[0] = '\0';
    } else {
	str = XmTextGetString(Text2);  
	if (str[0] == '\0')
	    queueArg[0] = '\0';
	else 	
	    sprintf(queueArg, " %s%s", QueueOption, str);	
	NEditFree(str);
    }
    if (HostOption[0] == '\0') {
    	hostArg[0] = '\0';
    } else {
	str = XmTextGetString(Text3);  
	if (str[0] == '\0')
	    hostArg[0] = '\0';
	else  	
	    sprintf(hostArg, " %s%s", HostOption, str);
	NEditFree(str);
    }
    if (NameOption[0] == '\0')
    	jobArg[0] = '\0';
    else {
#ifdef VMS
    /* truncate job name on VMS systems or it will cause problems */
        strncpy(printJobName,PrintJobName,VMS_MAX_JOB_NAME_STR);
        printJobName[VMS_MAX_JOB_NAME_STR] = '\0';
        sprintf(jobArg, " %s\"%s\"", NameOption, printJobName);
#else
        sprintf(jobArg, " %s\"%s\"", NameOption, PrintJobName);
#endif
    }

    /* Compose the command from the options determined above */
    sprintf(command, "%s%s%s%s%s", PrintCommand, copiesArg,
    	    queueArg, hostArg, jobArg);

    /* display it in the command text area */
    XmTextSetString(Text4, command);
    
    /* Indicate that the command field was synthesized from the other fields,
       so future dialog invocations can safely re-generate the command without
       overwriting commands specifically entered by the user */
    CmdFieldModified = False;
}

static void printCmdModified(Widget w, caddr_t client_data, caddr_t call_data)
{
    /* Indicate that the user has specifically modified the print command
       and that this field should be left as is in subsequent dialogs */
    CmdFieldModified = True;
}

static void printButtonCB(Widget widget, caddr_t client_data, caddr_t call_data)
{
    char *str, command[MAX_CMD_STR];
#ifdef VMS
    int spawn_sts;
    int spawnFlags=CLI$M_NOCLISYM;
    struct dsc$descriptor cmdDesc;

    /* get the print command from the command text area */
    str = XmTextGetString(Text4);

    /* add the file name to the print command */
    sprintf(command, "%s %s", str, PrintFileName);

    NEditFree(str);

    /* append /DELETE to print command if requested */
    if (DeleteFile)
	strcat(command, "/DELETE");

    /* spawn the print command */
    cmdDesc.dsc$w_length  = strlen(command);
    cmdDesc.dsc$b_dtype   = DSC$K_DTYPE_T;
    cmdDesc.dsc$b_class   = DSC$K_CLASS_S;
    cmdDesc.dsc$a_pointer = command;
    spawn_sts = lib$spawn(&cmdDesc,0,0,&spawnFlags,0,0,0,0,0,0,0,0);

    if (spawn_sts != SS$_NORMAL)
    {
        DialogF(DF_WARN, widget, 1, "Print Error",
                "Unable to Print:\n%d - %s\n  spawnFlags = %d\n", "OK",
                spawn_sts, strerror(EVMSERR, spawn_sts), spawnFlags);
        return;
    }
#else
    int nRead;
    FILE *pipe;
    char errorString[MAX_PRINT_ERROR_LENGTH], discarded[1024];

    /* get the print command from the command text area */
    str = XmTextGetString(Text4);

    /* add the file name and output redirection to the print command */
    sprintf(command, "cat %s | %s 2>&1", PrintFileName, str);
    NEditFree(str);
    
    /* Issue the print command using a popen call and recover error messages
       from the output stream of the command. */
    pipe = popen(command,"r");
    if (pipe == NULL)
    {
        DialogF(DF_WARN, widget, 1, "Print Error", "Unable to Print:\n%s",
                "OK", strerror(errno));
        return;
    }

    errorString[0] = 0;
    nRead = fread(errorString, sizeof(char), MAX_PRINT_ERROR_LENGTH-1, pipe);
    /* Make sure that the print command doesn't get stuck when trying to 
       write a lot of output on stderr (pipe may fill up). We discard 
       the additional output, though. */
    while (fread(discarded, sizeof(char), 1024, pipe) > 0);

    if (!ferror(pipe))
    {
        errorString[nRead] = '\0';
    }

    if (pclose(pipe))
    {
        DialogF(DF_WARN, widget, 1, "Print Error", "Unable to Print:\n%s",
                "OK", errorString);
        return;
    }
#endif /*(VMS)*/
    
    /* Print command succeeded, so retain the current print parameters */
    if (CopiesOption[0] != '\0') {
    	str = XmTextGetString(Text1);
    	strcpy(Copies, str);
    	NEditFree(str);
    }
    if (QueueOption[0] != '\0') {
    	str = XmTextGetString(Text2);
    	strcpy(Queue, str);
    	NEditFree(str);
    }
    if (HostOption[0] != '\0') {
    	str = XmTextGetString(Text3);
    	strcpy(Host, str);
    	NEditFree(str);
    }
    str = XmTextGetString(Text4);
    strcpy(CmdText, str);
    NEditFree(str);

    
    /* Pop down the dialog */
    DoneWithDialog = True;
}

static void cancelButtonCB(Widget widget, caddr_t client_data, caddr_t call_data)
{
    DoneWithDialog = True;
    CmdFieldModified = False;
}

#ifndef VMS
/*
** Is the filename file in the directory dirpath
** and does it have at least some of the mode_flags enabled ?
*/
static int fileInDir(const char *filename, const char *dirpath, unsigned short mode_flags)
{
    DIR           *dfile;
#ifdef USE_DIRENT
    struct dirent *DirEntryPtr;
#else
    struct direct *DirEntryPtr;
#endif
    struct stat   statbuf;
    char          fullname[MAXPATHLEN];

    dfile = opendir(dirpath);
    if (dfile != NULL) {
	while ((DirEntryPtr=readdir(dfile)) != NULL) {
            if (!strcmp(DirEntryPtr->d_name, filename)) {
        	strcpy(fullname,dirpath);
        	strcat(fullname,"/");
		strcat(fullname,filename);
        	stat(fullname,&statbuf);
        	closedir(dfile);
        	return statbuf.st_mode & mode_flags;
            }    
        }
    	closedir(dfile);
    }
    return False;
}

/*
** Is the filename file in the environment path directories 
** and does it have at least some of the mode_flags enabled ?
*/
static int fileInPath(const char *filename, unsigned short mode_flags)
{
    char path[MAXPATHLEN];
    char *pathstring,*lastchar;

    /* Get environmental value of PATH */
    pathstring = getenv("PATH");
    if (pathstring == NULL)
    	return False;

    /* parse the pathstring and search on each directory found */
    do {
	/* if final path in list is empty, don't search it */
	if (!strcmp(pathstring, ""))
            return False;
	/* locate address of next : character */
	lastchar = strchr(pathstring, SEPARATOR);                             
	if (lastchar != NULL) { 
            /* if more directories remain in pathstring, copy up to : */
            strncpy(path, pathstring, lastchar-pathstring);
            path[lastchar-pathstring] = '\0';
	} else {
	    /* if it's the last directory, just copy it */
            strcpy(path, pathstring);                                     
	}
	/* search for the file in this path */
	if(fileInDir(filename, path, mode_flags))
            return True; /* found it !! */
	/* point pathstring to start of new dir string */
	pathstring = lastchar + 1;                                       
    } while( lastchar != NULL );
    return False;
}

/*
** Is flpr present in the search path and is it executable ?
*/
static int flprPresent(void)
{
    /* Is flpr present in the search path and is it executable ? */
    return fileInPath("flpr",0111);
}

static int foundTag(const char *tagfilename, const char *tagname, char *result)
{
    FILE *tfile;
    char tagformat[512],line[512];

    strcpy(tagformat, tagname);
    strcat(tagformat, " %s");

    tfile = fopen(tagfilename,"r");
    if (tfile != NULL) {
	while (!feof(tfile)) {
            fgets(line,sizeof(line),tfile);
            if (sscanf(line,tagformat,result) != 0) {
		fclose(tfile);
        	return True;
	    }
	}
	fclose(tfile);
    }
    return False;
}

static int foundEnv(const char *EnvVarName, char *result)
{
    char *dqstr;

    dqstr = getenv(EnvVarName);
    if (dqstr != NULL) {
        strcpy(result,dqstr);
        return True;
    }
    return False;
}

static void getFlprHostDefault(char *defhost)
{   
    if (!foundEnv("FLPHOST",defhost))
        if(!foundTag("/usr/local/etc/flp.defaults", "host", defhost)) 
            strcpy(defhost,"");
}

static void getFlprQueueDefault(char *defqueue)
{   
    if (!foundEnv("FLPQUE",defqueue))
	if (!foundTag("/usr/local/etc/flp.defaults", "queue", defqueue))
            strcpy(defqueue,"");
}

#ifdef USE_LPR_PRINT_CMD
static void getLprQueueDefault(char *defqueue)
{
    if (!foundEnv("PRINTER",defqueue))
        strcpy(defqueue,"");
}
#endif

#ifndef USE_LPR_PRINT_CMD
static void getLpQueueDefault(char *defqueue)
{
    if (!foundEnv("LPDEST",defqueue))
        defqueue[0] = '\0';
}
#endif
#endif

#ifdef VMS
static void getVmsQueueDefault(char *defqueue)
{
    int translate_sts;
    short ret_len;
    char logicl[12], tabl[15];
    struct itemList {
        short bufL;
        short itemCode;
        char *queName;
        short *lngth;
        int end_entry;
    } translStruct;
    struct dsc$descriptor tabName;
    struct dsc$descriptor logName;

    sprintf(tabl, "LNM$FILE_DEV");

    tabName.dsc$w_length  = strlen(tabl);
    tabName.dsc$b_dtype   = DSC$K_DTYPE_T;
    tabName.dsc$b_class   = DSC$K_CLASS_S;
    tabName.dsc$a_pointer = tabl;

    sprintf(logicl, "SYS$PRINT");

    logName.dsc$w_length  = strlen(logicl);
    logName.dsc$b_dtype   = DSC$K_DTYPE_T;
    logName.dsc$b_class   = DSC$K_CLASS_S;
    logName.dsc$a_pointer = logicl;

    translStruct.itemCode = LNM$_STRING;
    translStruct.lngth = &ret_len;
    translStruct.bufL = 99;
    translStruct.end_entry = 0;
    translStruct.queName = defqueue;
    translate_sts = sys$trnlnm(0,&tabName,&logName,0,&translStruct);

    if (translate_sts != SS$_NORMAL && translate_sts != SS$_NOLOGNAM){
       fprintf(stderr, "Error return from sys$trnlnm: %d\n", translate_sts);
       DialogF(DF_WARN, Label2, 1, "Error", "Error translating SYS$PRINT",
               "OK");
       defqueue[0] = '\0';
    } else
    {
/*    printf("return status from sys$trnlnm = %d\n", translate_sts); */
        if (translate_sts == SS$_NOLOGNAM)
        {
            defqueue[0] = '\0';
        } else
        {
            strncpy(defqueue, translStruct.queName, ret_len);
            defqueue[ret_len] = '\0';
/*  printf("defqueue = %s, length = %d\n", defqueue, ret_len); */
        }
    }   
}
#endif /*(VMS)*/
