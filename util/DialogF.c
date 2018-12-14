/*******************************************************************************
*                                                                              *
* DialogF -- modal dialog printf routine                                       *
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
* for more details.                                                               *
*                                                                                *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                       *
*                                                                               *
* Nirvana Text Editor                                                                   *
* April 26, 1991                                                               *
*                                                                               *
* Written by Joy Kyriakopulos                                                       *
*                                                                               *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "DialogF.h"
#include "misc.h"
#include "nedit_malloc.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <Xm/Xm.h>
#include <Xm/MessageB.h>
#include <Xm/DialogS.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/SelectioB.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define NUM_DIALOGS_SUPPORTED 6
#define NUM_BUTTONS_SUPPORTED 3		/* except prompt dialog */
#define NUM_BUTTONS_MAXPROMPT 4
#define MAX_TITLE_LEN 256

enum dialogBtnIndecies {OK_BTN, APPLY_BTN, CANCEL_BTN, HELP_BTN};

struct dfcallbackstruct {
    unsigned button;		/* button pressed by user		     */
    Boolean done_with_dialog;	/* set by callbacks; dialog can be destroyed */
    unsigned apply_up;		/* will = 1 when apply button managed	     */
    Boolean destroyed;		/* set when dialog is destroyed unexpectedly */
};

static char **PromptHistory = NULL;
static int NPromptHistoryItems = -1;

static void apply_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data);
static void help_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data);
static void cancel_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data);
static void ok_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data);
static void destroy_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data);
static void focusCB(Widget w, Widget dialog, caddr_t call_data);
static void addEscapeHandler(Widget dialog, struct dfcallbackstruct *df,
    	int whichBtn);
static void escapeHelpCB(Widget w, XtPointer callData, XEvent *event,
    	Boolean *cont);
static void escapeApplyCB(Widget w, XtPointer callData, XEvent *event,
    	Boolean *cont);
static void createMnemonics(Widget w);

/*******************************************************************************
* DialogF()                                                                    *
*                                                                              *
* function to put up modal versions of the XmCreate<type>Dialog functions      *
* (where <type> is Error, Information, Prompt, Question, Message, or Warning). *
* The purpose of this routine is to allow a printf-style dialog box to be      *
* invoked in-line without giving control back to the main loop. The message    *
* string can contain vsprintf specifications.                                  *
* DialogF displays the dialog in application-modal style, blocking the         *
* application and keeping the modal dialog as the top window until the user    *
* responds. DialogF accepts a variable number of arguments, so the calling     *
* routine needs to #include <stdarg.h>. The first button is automatically      *
* marked as the default button (activated when the user types Return,          *
* surrounded by a special outline), and any button named either Cancel, or     *
* Dismiss is marked as the cancel button (activated by the ESC key). Buttons   *
* marked Dismiss or Cancel are also triggered by close of dialog via the       *
* window close box. If there's no Cancel or Dismiss button, button 1 is        *
* invoked when the close box is pressed.                                       *
*                                                                              *
* Arguments:                                                                   *
*                                                                              *
* unsigned dialog_type  dialog type (e.g. DF_ERR for error dialog, refer to    *
*                       DialogF.h for dialog type values)                      *
* Widget parent         parent widget ID                                       *
* unsigned n            # of buttons to display; if set to 0, use defaults in  *
*                       XmCreate<type>Dialog; value in range 0 to              *
*                       NUM_BUTTONS_SUPPORTED (for prompt dialogs:             *
*                       NUM_BUTTONS_MAXPROMPT)                                 *
* char* title           dialog title                                           *
* char* msgstr          message string (may contain conversion specifications  *
*                       for vsprintf)                                          *
* char* input_string    if dialog type = DF_PROMPT, then: a character string   *
*                       array in which to put the string input by the user. Do *
*                       NOT include an input_string argument for other dialog  *
*                       types.                                                 *
* char* but_lbl         button label(s) for buttons requested (if n > 0, one   *
*                       but_lbl argument per button)                           *
* <anytype> <args>      arguments for vsprintf (if any)                        *
*                                                                              *
*                                                                              *
* Returns:                                                                     *
*                                                                              *
* button selected by user (i.e. 1, 2, or 3. or 4 for prompt)                   *
*                                                                              *
*                                                                              *
* Examples:                                                                    *
*                                                                              *
* but_pressed = DialogF (DF_QUES, toplevel, 3, "Direction?", "up",             *
*              "down", "other");                                               *
* but_pressed = DialogF (DF_ERR, toplevel, 1, "You can't do that!",            *
*              "Acknowledged");                                                *
* but_pressed = DialogF (DF_PROMPT, toplevel, 0, "New %s",                     *
*             new_sub_category, categories[i]);                                *
*/
unsigned DialogF(int dialog_type, Widget parent, unsigned n, const char* title,
        const char* msgstr, ...)                    /* variable # arguments */
{
    va_list var;

    Widget dialog, dialog_shell;
    unsigned dialog_num, prompt;
    XmString but_lbl_xms[NUM_BUTTONS_MAXPROMPT];
    XmString msgstr_xms, input_string_xms, titstr_xms;
    char msgstr_vsp[DF_MAX_MSG_LENGTH+1];
    char *but_lbl, *input_string = NULL, *input_string_ptr;
    int argcount, num_but_lbls = 0, i, but_index, cancel_index = -1;
    Arg args[256];
    char titleCopy[MAX_TITLE_LEN];
    
    struct dfcallbackstruct df;

    static int dialog_types[] = {		/* Supported dialog types */
		XmDIALOG_ERROR,
		XmDIALOG_INFORMATION,
		XmDIALOG_MESSAGE,
		XmDIALOG_QUESTION,
		XmDIALOG_WARNING,
		XmDIALOG_PROMPT
    };
    static char *dialog_name[] = {		/* Corresponding dialog names */
		"Error",
		"Information",
		"Message",
		"Question",
		"Warning",
		"Prompt"
    };

    static unsigned char selectionButton_id[] =
    {
        XmDIALOG_OK_BUTTON,
        XmDIALOG_APPLY_BUTTON,
        XmDIALOG_CANCEL_BUTTON,
        XmDIALOG_HELP_BUTTON
    };
    Cardinal N_SELECTION_BUTTONS = XtNumber(selectionButton_id);

    static unsigned char messageButton_id[] =
    {
        XmDIALOG_OK_BUTTON,
        XmDIALOG_CANCEL_BUTTON,
        XmDIALOG_HELP_BUTTON
    };
    Cardinal N_MESSAGE_BUTTONS = XtNumber(messageButton_id);

    static char *button_name[] = {		/* Motif button names */
		XmNokLabelString,
		XmNapplyLabelString,		/* button #2, if managed */
		XmNcancelLabelString,
		XmNhelpLabelString
    };

						/* Validate input parameters */
    if ((dialog_type > NUM_DIALOGS_SUPPORTED) || (dialog_type <= 0)) {
	printf ("\nError calling DialogF - Unsupported dialog type\n");
	return (0);
    }
    dialog_num = dialog_type - 1;
    prompt = (dialog_type == DF_PROMPT);
    if  ((!prompt && (n > NUM_BUTTONS_SUPPORTED)) ||
	  (prompt && (n > NUM_BUTTONS_MAXPROMPT))) {
	printf ("\nError calling DialogF - Too many buttons specified\n");
	return (0);
    }

    df.done_with_dialog = False;
    df.destroyed = False;

    va_start (var, msgstr);
    if (prompt) {		      /* Get where to put dialog input string */
	input_string = va_arg(var, char*);
    }
    if (n == NUM_BUTTONS_MAXPROMPT)
	df.apply_up = 1;		/* Apply button will be managed */
    else
    	df.apply_up = 0;		/* Apply button will not be managed */
    for (argcount = 0; argcount < (int)n; ++argcount) {   /* Set up button labels */
	but_lbl = va_arg(var, char *);
	but_lbl_xms[num_but_lbls] = XmStringCreateLtoR (but_lbl, 
		XmSTRING_DEFAULT_CHARSET);
	but_index = df.apply_up ? argcount : 
	    	((argcount == 0) ? argcount : argcount+1);
	XtSetArg (args[argcount], button_name[but_index], 
		but_lbl_xms[num_but_lbls++]);
	if (!strcmp(but_lbl, "Cancel") || !strcmp(but_lbl, "Dismiss"))
	    cancel_index = but_index;
    }
    if (n == 1)
       cancel_index = 0;

    /* Get & translate msg string. NOTE: the use of vsprintf is inherently
       dangerous because there is no way to control the length of the written
       string. vnsprintf isn't available on all platforms, unfortunately.
       Therefore we have to make sure that msgstr_vsp is large enough such
       that it cannot overflow under any circumstances (within the context
       of any DialogF call). A necessary condition is that it can at least
       hold one file name (MAXPATHLEN). Therefore, DF_MAX_MSG_LENGTH must be
       sufficiently larger than MAXPATHLEN.
    */
    vsprintf (msgstr_vsp, msgstr, var);
    va_end(var);
    
    strncpy(&titleCopy[0], title, MAX_TITLE_LEN);
    titleCopy[MAX_TITLE_LEN-1] = '\0';

    msgstr_xms = XmStringCreateLtoR(msgstr_vsp, XmSTRING_DEFAULT_CHARSET);
    titstr_xms = XmStringCreateLtoR(titleCopy, XmSTRING_DEFAULT_CHARSET);

    if (prompt) {				/* Prompt dialog */
	XtSetArg (args[argcount], XmNselectionLabelString, msgstr_xms);
		argcount++;
	XtSetArg (args[argcount], XmNdialogTitle, titstr_xms);
		argcount++;
	XtSetArg (args[argcount], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
		argcount ++;

	dialog = CreatePromptDialog(parent, dialog_name[dialog_num], args,
			argcount);
	XtAddCallback (dialog, XmNokCallback, (XtCallbackProc)ok_callback,
		(char *)&df);
	XtAddCallback (dialog, XmNcancelCallback,
		(XtCallbackProc)cancel_callback, (char *)&df);
	XtAddCallback (dialog, XmNhelpCallback, (XtCallbackProc)help_callback,
		(char *)&df);
	XtAddCallback (dialog, XmNapplyCallback, (XtCallbackProc)apply_callback,
		(char *)&df);
	RemapDeleteKey(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT));

	/* Text area in prompt dialog should get focus, not ok button
	   since user enters text first.  To fix this, we need to turn
	   off the default button for the dialog, until after keyboard
	   focus has been established */
	XtVaSetValues(dialog, XmNdefaultButton, NULL, NULL);
    	XtAddCallback(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT),
    		XmNfocusCallback, (XtCallbackProc)focusCB, (char *)dialog);

	/* Limit the length of the text that can be entered in text field */
	XtVaSetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT),
		XmNmaxLength, DF_MAX_PROMPT_LENGTH-1, NULL);
	
	/* Turn on the requested number of buttons in the dialog by
	   managing/unmanaging the button widgets */
	switch (n) {		/* number of buttons requested */
	case 0: case 3:
	    break;		/* or default of 3 buttons */
	case 1:
	    XtUnmanageChild (XmSelectionBoxGetChild (dialog,
		XmDIALOG_CANCEL_BUTTON) );
	case 2:
	    XtUnmanageChild (XmSelectionBoxGetChild (dialog,
		XmDIALOG_HELP_BUTTON) );
	    break;
	case 4:
	    XtManageChild (XmSelectionBoxGetChild (dialog,
		XmDIALOG_APPLY_BUTTON) );
	    df.apply_up = 1;		/* apply button managed */
	default:
	    break;
	}				/* end switch */

        /*  Set margin width to managed buttons.  */
        for (i = 0; (unsigned) i < N_SELECTION_BUTTONS; i++)
        {
            Widget button = XmSelectionBoxGetChild(dialog, selectionButton_id[i]);

            if (XtIsManaged(button))
            {
                XtVaSetValues(button, XmNmarginWidth, BUTTON_WIDTH_MARGIN, NULL);
            }
        }

    	/* If the button labeled cancel or dismiss is not the cancel button, or
    	   if there is no button labeled cancel or dismiss, redirect escape key
    	   events (this is necessary because the XmNcancelButton resource in
    	   the bulletin board widget class is blocked from being reset) */
    	if (cancel_index == -1)
    	    addEscapeHandler(dialog, NULL, 0);
    	else if (cancel_index != CANCEL_BTN)
    	    addEscapeHandler(dialog, &df, cancel_index);

    	/* Add a callback to the window manager close callback for the dialog */
    	AddMotifCloseCallback(XtParent(dialog),
    	    	(XtCallbackProc)(cancel_index == APPLY_BTN ? apply_callback :
    	    	(cancel_index == CANCEL_BTN ? cancel_callback :
    	    	(cancel_index == HELP_BTN ? help_callback : ok_callback))), &df);
 
        /* Also add a callback to detect unexpected destruction (eg, because
           the parent window is destroyed) */
        XtAddCallback(dialog, XmNdestroyCallback, 
            (XtCallbackProc)destroy_callback, &df);

	/* A previous call to SetDialogFPromptHistory can request that an
	   up-arrow history-recall mechanism be attached.  If so, do it here */
	if (NPromptHistoryItems != -1)
	    AddHistoryToTextWidget(XmSelectionBoxGetChild(dialog,XmDIALOG_TEXT),
		    &PromptHistory, &NPromptHistoryItems);
	
    	/* Pop up the dialog */
	ManageDialogCenteredOnPointer(dialog);
	
	/* Get the focus to the input string.  There is some timing problem
	   within Motif that requires this to be called several times */
	for (i=0; i<20; i++)
	    XmProcessTraversal(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT),
		    XmTRAVERSE_CURRENT);
	
	/* Wait for a response to the dialog */
	while (!df.done_with_dialog && !df.destroyed)
	    XtAppProcessEvent (XtWidgetToApplicationContext(dialog), XtIMAll);
        
        if (!df.destroyed) {
	    argcount = 0; /* Pass back string user entered */
	    XtSetArg (args[argcount], XmNtextString, &input_string_xms); argcount++;
	    XtGetValues (dialog, args, argcount);
	    XmStringGetLtoR (input_string_xms, XmSTRING_DEFAULT_CHARSET,
		&input_string_ptr);
	    strcpy (input_string, input_string_ptr);  /* This step is necessary */
	    XmStringFree(input_string_xms );
            NEditFree(input_string_ptr);
             /* Important! Only intercept unexpected destroy events. */
	    XtRemoveCallback(dialog, XmNdestroyCallback, 
            	(XtCallbackProc)destroy_callback, &df);
	    XtDestroyWidget(dialog);
	}
	PromptHistory = NULL;
    	NPromptHistoryItems = -1;
    }						  /* End prompt dialog path */

    else {				/* MessageBox dialogs */
	XtSetArg (args[argcount], XmNmessageString, msgstr_xms); argcount++;

	XtSetArg (args[argcount], XmNdialogType, dialog_types[dialog_num]);
		argcount ++;
	XtSetArg (args[argcount], XmNdialogTitle, titstr_xms);
		argcount++;
	XtSetArg (args[argcount], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
		argcount ++;

	dialog_shell = CreateDialogShell (parent, dialog_name[dialog_num],
			0, 0);
	dialog = XmCreateMessageBox (dialog_shell, "msg box", args, argcount);
        
	XtAddCallback (dialog, XmNokCallback, (XtCallbackProc)ok_callback,
		(char *)&df);
	XtAddCallback (dialog, XmNcancelCallback,
		(XtCallbackProc)cancel_callback, (char *)&df);
	XtAddCallback (dialog, XmNhelpCallback, (XtCallbackProc)help_callback,
		(char *)&df);

	/* Make extraneous buttons disappear */
	switch (n) {		/* n = number of buttons requested */
	case 0: case 3:
	    break;		/* default (0) gets you 3 buttons */
	case 1:
	    XtUnmanageChild (XmMessageBoxGetChild (dialog,
			XmDIALOG_CANCEL_BUTTON) );
	case 2:
	    XtUnmanageChild (XmMessageBoxGetChild (dialog,
			XmDIALOG_HELP_BUTTON) );
	    break;
	default:
	    break;
	}

        /*  Set margin width to managed buttons.  */
        for (i = 0; (unsigned) i < N_MESSAGE_BUTTONS; i++)
        {
            Widget button = XmMessageBoxGetChild(dialog, messageButton_id[i]);

            if (XtIsManaged(button))
            {
                XtVaSetValues(button, XmNmarginWidth, BUTTON_WIDTH_MARGIN, NULL);
            }
        }

        /* Try to create some sensible default mnemonics */
        createMnemonics(dialog_shell);
        AddDialogMnemonicHandler(dialog_shell, TRUE);

    	/* If the button labeled cancel or dismiss is not the cancel button, or
    	   if there is no button labeled cancel or dismiss, redirect escape key
    	   events (this is necessary because the XmNcancelButton resource in
    	   the bulletin board widget class is blocked from being reset) */
    	if (cancel_index == -1)
    	    addEscapeHandler(dialog, NULL, 0);
    	else if (cancel_index != CANCEL_BTN)
    	    addEscapeHandler(dialog, &df, cancel_index);

    	/* Add a callback to the window manager close callback for the dialog */
    	AddMotifCloseCallback(XtParent(dialog),
    	    	(XtCallbackProc)(cancel_index == APPLY_BTN ? apply_callback :
    	    	(cancel_index == CANCEL_BTN ? cancel_callback :
    	    	(cancel_index == HELP_BTN ? help_callback : ok_callback))),&df);
        
        /* Also add a callback to detect unexpected destruction (eg, because
           the parent window is destroyed) */
        XtAddCallback(dialog_shell, XmNdestroyCallback, 
	    (XtCallbackProc)destroy_callback, &df);

	/* Pop up the dialog, wait for response*/
	ManageDialogCenteredOnPointer(dialog);
	while (!df.done_with_dialog && !df.destroyed)
	    XtAppProcessEvent (XtWidgetToApplicationContext(dialog), XtIMAll);
	
	if (!df.destroyed) {
             /* Important! Only intercept unexpected destroy events. */
	    XtRemoveCallback(dialog_shell, XmNdestroyCallback, 
            	(XtCallbackProc)destroy_callback, &df);
	    XtDestroyWidget(dialog_shell);
        }
    }

    XmStringFree(msgstr_xms);
    XmStringFree(titstr_xms);
    for (i = 0; i < num_but_lbls; ++i)
	XmStringFree(but_lbl_xms[i]);
    
    /* If the dialog was destroyed unexpectedly, the button was not set yet,
       so we must set the index of the cancel button. */
    if (df.destroyed) {
	df.button = cancel_index == APPLY_BTN ? 2 :
        	(cancel_index == CANCEL_BTN ? 2 + df.apply_up :
                (cancel_index == HELP_BTN ? 3 + df.apply_up : 1));
    }
		
    df.apply_up = 0;			/* default is apply button unmanaged */

    return (df.button);
}

/*
** Add up-arrow history recall to the next DialogF(DF_PROMPT... call (see
** AddHistoryToTextWidget in misc.c).  This must be re-set before each call.
** calling DialogF with a dialog type of DF_PROMPT automatically resets
** this mode back to no-history-recall.
*/
void SetDialogFPromptHistory(char **historyList, int nItems)
{
    PromptHistory = historyList;
    NPromptHistoryItems = nItems;
}

static void ok_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data)
{
    client_data->done_with_dialog = True;
    client_data->button = 1;		/* Return Button number pressed */
}

static void cancel_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data)
{
    client_data->done_with_dialog = True;
    client_data->button = 2 + client_data->apply_up; /* =3 if apply button managed */
}

static void help_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data)
{
    client_data->done_with_dialog = True;
    client_data->button = 3 + client_data->apply_up; /* =4 if apply button managed */
}

static void apply_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data)
{
    client_data->done_with_dialog = True;
    client_data->button = 2;		/* Motif puts between OK and cancel */
}

static void destroy_callback (Widget w, struct dfcallbackstruct *client_data,
	caddr_t call_data)
{
    client_data->destroyed = True;
}

/*
** callback for returning default button status to the ok button once we're
** sure the text area in the prompt dialog has input focus.
*/
static void focusCB(Widget w, Widget dialog, caddr_t call_data)
{
    XtVaSetValues(dialog, XmNdefaultButton,
    	    XmSelectionBoxGetChild(dialog, XmDIALOG_OK_BUTTON), NULL);
}

/*
** Message and prompt dialogs hardwire the cancel button to the XmNcancelButton
** resource in the bulletin board dialog.  Since we rename the buttons, the
** cancel label may not be on the dialog's idea of the Cancel button.  The only
** way to make the accelerator for Cancel and Dismiss (the escape key) work
** correctly in this situation is to brutally catch and redirect the event.
** This routine redirects escape key events in the dialog to the callback for
** the button "whichBtn", passing it argument "df".  If "df" is NULL, simply
** block the event from reaching the dialog.
*/
static void addEscapeHandler(Widget dialog, struct dfcallbackstruct *df,
    	int whichBtn)
{
    XtAddEventHandler(dialog, KeyPressMask, False, whichBtn == APPLY_BTN ?
    	    escapeApplyCB : escapeHelpCB, (XtPointer)df);
    XtGrabKey(dialog, XKeysymToKeycode(XtDisplay(dialog), XK_Escape), 0,
	    True, GrabModeAsync, GrabModeAsync);
}

/*
** Event handler for escape key to redirect the event to the help button.
** Attached when cancel label appears on Help button.
*/
static void escapeHelpCB(Widget w, XtPointer callData, XEvent *event,
    	Boolean *cont)
{
    if (event->xkey.keycode != XKeysymToKeycode(XtDisplay(w), XK_Escape))
    	return;
    if (callData != NULL)
    	help_callback(w, (struct dfcallbackstruct *)callData, NULL);
    *cont = False;
}

/*
** Event handler for escape key to redirect event to the apply button.
** Attached when cancel label appears on Apply button.
*/
static void escapeApplyCB(Widget w, XtPointer callData, XEvent *event,
    	Boolean *cont)
{
    if (event->xkey.keycode != XKeysymToKeycode(XtDisplay(w), XK_Escape))
    	return;
    if (callData != NULL)
    	apply_callback(w, (struct dfcallbackstruct *)callData, NULL);
    *cont = False;
}

/*
** Only used by createMnemonics(Widget w)
*/
static void recurseCreateMnemonics(Widget w, Boolean *mnemonicUsed)
{
    WidgetList children;
    Cardinal   numChildren, i;

    XtVaGetValues(w,
                  XmNchildren,    &children,
                  XmNnumChildren, &numChildren,
                  NULL);

    for (i = 0; i < numChildren; i++)
    {
        Widget child = children[i];
        
        if (XtIsComposite(child))
        {
            recurseCreateMnemonics(child, mnemonicUsed);
        }
        else if (XtIsSubclass(child, xmPushButtonWidgetClass) ||
                 XtIsSubclass(child, xmPushButtonGadgetClass))
        {
            XmString xmslabel;
            char *label;
            int c;
            
            XtVaGetValues(child, XmNlabelString, &xmslabel, NULL);
            if (XmStringGetLtoR(xmslabel, XmSTRING_DEFAULT_CHARSET, &label))
            {
                /* Scan through the string to see if the label is already used */            
                int labelLen = strlen(label);
                for (c = 0; c < labelLen; c++)
                {
                    unsigned char lc = tolower((unsigned char)label[c]);

                    if (!mnemonicUsed[lc] && isalnum(lc))
                    {
                        mnemonicUsed[lc] = TRUE;
                        XtVaSetValues(child, XmNmnemonic, label[c], NULL);
                        break;
                    }
                }

                NEditFree(label);
            }
            XmStringFree(xmslabel);
        }
    }
}

/*
** Automatically create mnemonics for a widget.  Traverse all it's
** children.  If the child is a push button, snag the first unused letter
** and make that the mnemonic.  This is useful for DialogF dialogs which
** can have arbitrary text in the buttons.
*/
static void createMnemonics(Widget w)
{
    Boolean mnemonicUsed[UCHAR_MAX + 1];
    
    memset(mnemonicUsed, FALSE, sizeof mnemonicUsed / sizeof *mnemonicUsed);
    recurseCreateMnemonics(w, mnemonicUsed);
}
