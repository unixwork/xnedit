/*******************************************************************************
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
* May 10, 1991								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "selection.h"
#include "textBuf.h"
#include "text.h"
#include "nedit.h"
#include "file.h"
#include "window.h"
#include "menu.h"
#include "preferences.h"
#include "server.h"
#include "../util/DialogF.h"
#include "../util/fileUtils.h"
#include "../util/nedit_malloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/
#if !defined(DONT_HAVE_GLOB) && !defined(USE_MOTIF_GLOB) && !defined(VMS)
#include <glob.h>
#endif

#include <Xm/Xm.h>
#include <X11/Xatom.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif


static void gotoCB(Widget widget, XtPointer window, Atom *sel,
	Atom *type, XtPointer value, unsigned long *length, int *format);
static void fileCB(Widget widget, XtPointer window, Atom *sel,
	Atom *type, XtPointer value, unsigned long *length, int *format);
static void getAnySelectionCB(Widget widget, XtPointer result, Atom *sel,
	Atom *type, XtPointer value, unsigned long *length, int *format);
static void processMarkEvent(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch, char *action, int extend);
static void markTimeoutProc(XtPointer clientData, XtIntervalId *id);
static void markKeyCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch);
static void gotoMarkKeyCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch);
static void gotoMarkExtendKeyCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch);
static void maintainSelection(selection *sel, int pos, int nInserted,
    	int nDeleted);
static void maintainPosition(int *position, int modPos, int nInserted,
    	int nDeleted);

/*
** Extract the line and column number from the text string.
** Set the line and/or column number to -1 if not specified, and return -1 if
** both line and column numbers are not specified.
*/
int StringToLineAndCol(const char *text, int *lineNum, int *column) {
    char *endptr;
    long  tempNum;
    int   textLen;

    /* Get line number */
    tempNum = strtol( text, &endptr, 10 );

    /* If user didn't specify a line number, set lineNum to -1 */
    if      ( endptr  == text    ) { *lineNum = -1;      }
    else if ( tempNum >= INT_MAX ) { *lineNum = INT_MAX; }
    else if ( tempNum <  0       ) { *lineNum = 0;       }
    else                           { *lineNum = tempNum; }

    /* Find the next digit */
    for ( textLen = strlen(endptr); textLen > 0; endptr++, textLen-- ) {
        if (isdigit((unsigned char)*endptr) ||
            *endptr == '-' || *endptr == '+') {
            break;
        }
    }

    /* Get column */
    if ( *endptr != '\0' ) {
        tempNum = strtol( endptr, NULL, 10 );
        if      ( tempNum >= INT_MAX ) { *column = INT_MAX; }
        else if ( tempNum <  0       ) { *column = 0;       }
        else                           { *column = tempNum; }
    }
    else { *column = -1; }

    return *lineNum == -1 && *column == -1 ? -1 : 0;
}

void GotoLineNumber(WindowInfo *window)
{
    char lineNumText[DF_MAX_PROMPT_LENGTH], *params[1];
    int lineNum, column, response;
    
    response = DialogF(DF_PROMPT, window->shell, 2, "Goto Line Number",
            "Goto Line (and/or Column)  Number:", lineNumText, "OK", "Cancel");
    if (response == 2)
    	return;

    if (StringToLineAndCol(lineNumText, &lineNum, &column) == -1) {
    	XBell(TheDisplay, 0);
	return;
    }
    params[0] = lineNumText;
    XtCallActionProc(window->lastFocus, "goto_line_number", NULL, params, 1);
}
    
void GotoSelectedLineNumber(WindowInfo *window, Time time)
{
    XtGetSelectionValue(window->textArea, XA_PRIMARY, XA_STRING,
    	    gotoCB, window, time);
}

void OpenSelectedFile(WindowInfo *window, Time time)
{
    XtGetSelectionValue(window->textArea, XA_PRIMARY, XA_STRING,
    	    fileCB, window, time);
}

/*
** Getting the current selection by making the request, and then blocking
** (processing events) while waiting for a reply.  On failure (timeout or
** bad format) returns NULL, otherwise returns the contents of the selection.
*/
char *GetAnySelection(WindowInfo *window)
{
    static char waitingMarker[1] = "";
    char *selText = waitingMarker;
    XEvent nextEvent;	 
    
    /* If the selection is in the window's own buffer get it from there,
       but substitute null characters as if it were an external selection */
    if (window->buffer->primary.selected) {
	selText = BufGetSelectionText(window->buffer);
	BufUnsubstituteNullChars(selText, window->buffer);
	return selText;
    }
    
    /* Request the selection value to be delivered to getAnySelectionCB */
    XtGetSelectionValue(window->textArea, XA_PRIMARY, XA_STRING,
	    getAnySelectionCB, &selText, 
	    XtLastTimestampProcessed(XtDisplay(window->textArea)));

    /* Wait for the value to appear */
    while (selText == waitingMarker) {
	XtAppNextEvent(XtWidgetToApplicationContext(window->textArea), 
		&nextEvent);
	ServerDispatchEvent(&nextEvent);
    }
    return selText;
}

static void gotoCB(Widget widget, XtPointer wi, Atom *sel,
    	Atom *type, XtPointer v, unsigned long *length, int *format)
{
    WindowInfo *window = wi;
    char *value = v;
    
     /* two integers and some space in between */
    char lineText[(TYPE_INT_STR_SIZE(int) * 2) + 5];
    int rc, lineNum, column, position, curCol;
    
    /* skip if we can't get the selection data, or it's obviously not a number */
    if (*type == XT_CONVERT_FAIL || value == NULL) {
    	XBell(TheDisplay, 0);
	return;
    }
    if (((size_t) *length) > sizeof(lineText) - 1) {
    	XBell(TheDisplay, 0);
	NEditFree(value);
	return;
    }
    /* should be of type text??? */
    if (*format != 8) {
    	fprintf(stderr, "NEdit: Can't handle non 8-bit text\n");
    	XBell(TheDisplay, 0);
	NEditFree(value);
	return;
    }
    strncpy(lineText, value, sizeof(lineText));
    lineText[sizeof(lineText) - 1] = '\0';
    
    rc = StringToLineAndCol(lineText, &lineNum, &column);
    NEditFree(value);
    if (rc == -1) {
    	XBell(TheDisplay, 0);
	return;
    }

    /* User specified column, but not line number */
    if ( lineNum == -1 ) {
        position = TextGetCursorPos(widget);
        if (TextPosToLineAndCol(widget, position, &lineNum, &curCol) == False) {
            XBell(TheDisplay, 0);
            return;
        }
    }
    /* User didn't specify a column */
    else if ( column == -1 ) {
        SelectNumberedLine(window, lineNum);
        return;
    }

    position = TextLineAndColToPos(widget, lineNum, column );
    if ( position == -1 ) {
        XBell(TheDisplay, 0);
        return;
    }
    TextSetCursorPos(widget, position);
}

static void fileCB(Widget widget, XtPointer wi, Atom *sel,
    	Atom *type, XtPointer v, unsigned long *length, int *format)
{
    WindowInfo *window = wi;
    char *value = v;

    char nameText[MAXPATHLEN], includeName[MAXPATHLEN];
    char filename[MAXPATHLEN], pathname[MAXPATHLEN];
    char *inPtr, *outPtr;
#ifdef VMS
#ifndef __DECC
    static char includeDir[] = "sys$library:";
#else
    static char includeDir[] = "decc$library_include:";
#endif
#else
    static char includeDir[] = "/usr/include/";
#endif /* VMS */
    
    /* get the string, or skip if we can't get the selection data, or it's
       obviously not a file name */
    if (*type == XT_CONVERT_FAIL || value == NULL) {
    	XBell(TheDisplay, 0);
	return;
    }
    if (*length > MAXPATHLEN || *length == 0) {
    	XBell(TheDisplay, 0);
	NEditFree(value);
	return;
    }
    /* should be of type text??? */
    if (*format != 8) {
    	fprintf(stderr, "NEdit: Can't handle non 8-bit text\n");
    	XBell(TheDisplay, 0);
	NEditFree(value);
	return;
    }
    strncpy(nameText, value, *length);
    NEditFree(value);
    nameText[*length] = '\0';
    
    /* extract name from #include syntax */
    if (sscanf(nameText, "#include \"%[^\"]\"", includeName) == 1)
    	strcpy(nameText, includeName);
    else if (sscanf(nameText, "#include <%[^<>]>", includeName) == 1)
    	sprintf(nameText, "%s%s", includeDir, includeName);
    
    /* strip whitespace from name */
    for (inPtr=nameText, outPtr=nameText; *inPtr!='\0'; inPtr++)
    	if (*inPtr != ' ' && *inPtr != '\t' && *inPtr != '\n')
    	    *outPtr++ = *inPtr;
    *outPtr = '\0';

#ifdef VMS
    /* If path name is relative, make it refer to current window's directory */
    if ((strchr(nameText, ':') == NULL) && (strlen(nameText) > 1) &&
      	    !((nameText[0] == '[') && (nameText[1] != '-') &&
	      (nameText[1] != '.'))) {
	strcpy(filename, window->path);
	strcat(filename, nameText);
	strcpy(nameText, filename);
    }
#else
    /* Process ~ characters in name */
    ExpandTilde(nameText);
        
    /* If path name is relative, make it refer to current window's directory */
    if (nameText[0] != '/') {
	strcpy(filename, window->path);
	strcat(filename, nameText);
	strcpy(nameText, filename);
    }
#endif
    
    /* Expand wildcards in file name.
       Some older systems don't have the glob subroutine for expanding file
       names, in these cases, either don't expand names, or try to use the
       Motif internal parsing routine _XmOSGetDirEntries, which is not
       guranteed to be available, but in practice is there and does work. */
#if defined(DONT_HAVE_GLOB) || defined(VMS)
    /* Open the file */
    if (ParseFilename(nameText, filename, pathname) != 0) {
        XBell(TheDisplay, 0);
	return;
    }	
    EditExistingFile(window, filename, 
            pathname, 0, NULL, False, NULL, GetPrefOpenInTab(), False);
#elif defined(USE_MOTIF_GLOB)
    { char **nameList = NULL;
      int i, nFiles = 0, maxFiles = 30;

      if (ParseFilename(nameText, filename, pathname) != 0) {
           XBell(TheDisplay, 0);
	   return;
      }
      _XmOSGetDirEntries(pathname, filename, XmFILE_ANY_TYPE, False, True,
	      &nameList, &nFiles, &maxFiles);
      for (i=0; i<nFiles; i++) {
	  if (ParseFilename(nameList[i], filename, pathname) != 0) {
	      XBell(TheDisplay, 0);
	  }
        else {
    	      EditExistingFile(window, filename, pathname, 0, 
	              NULL, False, NULL, GetPrefOpenInTab(), False);
	  }
      }
      for (i=0; i<nFiles; i++) {
	   NEditFree(nameList[i]);
	}
      NEditFree(nameList);
    }
#else
    { glob_t globbuf;
      int i;

      glob(nameText, GLOB_NOCHECK, NULL, &globbuf);
      for (i=0; i<(int)globbuf.gl_pathc; i++) {
	  if (ParseFilename(globbuf.gl_pathv[i], filename, pathname) != 0)
	      XBell(TheDisplay, 0);
	  else
    	      EditExistingFile(GetPrefOpenInTab()? window : NULL, 
	              filename, pathname, 0, NULL, False, NULL, 
		      GetPrefOpenInTab(), False);
      }
      globfree(&globbuf);
    }
#endif
    CheckCloseDim();
}

static void getAnySelectionCB(Widget widget, XtPointer voidresult, Atom *sel,
	Atom *type, XtPointer voidvalue, unsigned long *length, int *format)
{
    char **result = voidresult;
    char *value = voidvalue;
    
    /* Confirm that the returned value is of the correct type */
    if (*type != XA_STRING || *format != 8) {
	XBell(TheDisplay, 0);
        NEditFree(value);
	*result = NULL;
	return;
    }

    /* Append a null, and return the string */
    *result = (char*)NEditMalloc(*length + 1);
    strncpy(*result, value, *length);
    NEditFree(value);
    (*result)[*length] = '\0';
}

void SelectNumberedLine(WindowInfo *window, int lineNum)
{
    int i, lineStart = 0, lineEnd;

    /* count lines to find the start and end positions for the selection */
    if (lineNum < 1)
    	lineNum = 1;
    lineEnd = -1;
    for (i=1; i<=lineNum && lineEnd<window->buffer->length; i++) {
    	lineStart = lineEnd + 1;
    	lineEnd = BufEndOfLine(window->buffer, lineStart);
    }
    
    /* highlight the line */
    if (i>lineNum) {
	/* Line was found */
	if (lineEnd < window->buffer->length) {
	    BufSelect(window->buffer, lineStart, lineEnd+1);
	} else { 
	    /* Don't select past the end of the buffer ! */
	    BufSelect(window->buffer, lineStart, window->buffer->length);
	}
    } else {
	/* Line was not found -> position the selection & cursor at the end 
	   without making a real selection and beep */
	lineStart = window->buffer->length;
	BufSelect(window->buffer, lineStart, lineStart);
	XBell(TheDisplay, 0);
    }     
    MakeSelectionVisible(window, window->lastFocus);
    TextSetCursorPos(window->lastFocus, lineStart);
}

void MarkDialog(WindowInfo *window)
{
    char letterText[DF_MAX_PROMPT_LENGTH], *params[1];
    int response;
    
    response = DialogF(DF_PROMPT, window->shell, 2, "Mark",
            "Enter a single letter label to use for recalling\n"
            "the current selection and cursor position.\n\n"
            "(To skip this dialog, use the accelerator key,\n"
            "followed immediately by a letter key (a-z))", letterText, "OK",
            "Cancel");
    if (response == 2)
    	return;
    if (strlen(letterText) != 1 || !isalpha((unsigned char)letterText[0])) {
    	XBell(TheDisplay, 0);
	return;
    }
    params[0] = letterText;
    XtCallActionProc(window->lastFocus, "mark", NULL, params, 1);
}

void GotoMarkDialog(WindowInfo *window, int extend)
{
    char letterText[DF_MAX_PROMPT_LENGTH], *params[2];
    int response;
    
    response = DialogF(DF_PROMPT, window->shell, 2, "Goto Mark",
            "Enter the single letter label used to mark\n"
            "the selection and/or cursor position.\n\n"
            "(To skip this dialog, use the accelerator\n"
            "key, followed immediately by the letter)", letterText, "OK",
            "Cancel");
    if (response == 2)
    	return;
    if (strlen(letterText) != 1 || !isalpha((unsigned char)letterText[0])) {
    	XBell(TheDisplay, 0);
	return;
    }
    params[0] = letterText;
    params[1] = "extend";
    XtCallActionProc(window->lastFocus, "goto_mark", NULL, params,
	    extend ? 2 : 1);
}

/*
** Process a command to mark a selection.  Expects the user to continue
** the command by typing a label character.  Handles both correct user
** behavior (type a character a-z) or bad behavior (do nothing or type
** something else).
*/
void BeginMarkCommand(WindowInfo *window)
{
    XtInsertEventHandler(window->lastFocus, KeyPressMask, False,
    	    markKeyCB, window, XtListHead);
    window->markTimeoutID = XtAppAddTimeOut(
    	    XtWidgetToApplicationContext(window->shell), 4000,
    	    markTimeoutProc, window->lastFocus);
}

/*
** Process a command to go to a marked selection.  Expects the user to
** continue the command by typing a label character.  Handles both correct
** user behavior (type a character a-z) or bad behavior (do nothing or type
** something else).
*/
void BeginGotoMarkCommand(WindowInfo *window, int extend)
{
    XtInsertEventHandler(window->lastFocus, KeyPressMask, False,
    	    extend ? gotoMarkExtendKeyCB : gotoMarkKeyCB, window, XtListHead);
    window->markTimeoutID = XtAppAddTimeOut(
    	    XtWidgetToApplicationContext(window->shell), 4000,
    	    markTimeoutProc, window->lastFocus);
}

/*
** Xt timer procedure for removing event handler if user failed to type a
** mark character withing the allowed time
*/
static void markTimeoutProc(XtPointer clientData, XtIntervalId *id)
{
    Widget w = (Widget)clientData;
    WindowInfo *window = WidgetToWindow(w);
    
    XtRemoveEventHandler(w, KeyPressMask, False, markKeyCB, window);
    XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkKeyCB, window);
    XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkExtendKeyCB, window);
    window->markTimeoutID = 0;
}

/*
** Temporary event handlers for keys pressed after the mark or goto-mark
** commands, If the key is valid, grab the key event and call the action
** procedure to mark (or go to) the selection, otherwise, remove the handler
** and give up.
*/
static void processMarkEvent(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch, char *action, int extend)
{
    XKeyEvent *e = (XKeyEvent *)event;
    WindowInfo *window = WidgetToWindow(w);
    Modifiers modifiers;
    KeySym keysym;
    char *params[2], string[2];

    XtTranslateKeycode(TheDisplay, e->keycode, e->state, &modifiers,
    	    &keysym);
    if ((keysym >= 'A' && keysym <= 'Z') || (keysym >= 'a' && keysym <= 'z')) {
    	string[0] = toupper(keysym);
    	string[1] = '\0';
    	params[0] = string;
	params[1] = "extend";
    	XtCallActionProc(window->lastFocus, action, event, params,
		extend ? 2 : 1);
    	*continueDispatch = False;
    }
    XtRemoveEventHandler(w, KeyPressMask, False, markKeyCB, window);
    XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkKeyCB, window);
    XtRemoveEventHandler(w, KeyPressMask, False, gotoMarkExtendKeyCB, window);
    XtRemoveTimeOut(window->markTimeoutID);
}
static void markKeyCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch)
{
    processMarkEvent(w, clientData, event, continueDispatch, "mark", False);
}
static void gotoMarkKeyCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch)
{
    processMarkEvent(w, clientData, event, continueDispatch, "goto_mark",False);
}
static void gotoMarkExtendKeyCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *continueDispatch)
{
    processMarkEvent(w, clientData, event, continueDispatch, "goto_mark", True);
}

void AddMark(WindowInfo *window, Widget widget, char label)
{
    int index;
    
    /* look for a matching mark to re-use, or advance
       nMarks to create a new one */
    label = toupper(label);
    for (index=0; index<window->nMarks; index++) {
    	if (window->markTable[index].label == label)
   	    break;
    }
    if (index >= MAX_MARKS) {
    	fprintf(stderr, "no more marks allowed\n"); /* shouldn't happen */
    	return;
    }
    if (index == window->nMarks)
    	window->nMarks++;
    
    /* store the cursor location and selection position in the table */
    window->markTable[index].label = label;
    memcpy(&window->markTable[index].sel, &window->buffer->primary,
    	    sizeof(selection));
    window->markTable[index].cursorPos = TextGetCursorPos(widget);
}

void GotoMark(WindowInfo *window, Widget w, char label, int extendSel)
{
    int index, oldStart, newStart, oldEnd, newEnd, cursorPos;
    selection *sel, *oldSel;
    
    /* look up the mark in the mark table */
    label = toupper(label);
    for (index=0; index<window->nMarks; index++) {
    	if (window->markTable[index].label == label)
   	    break;
    }
    if (index == window->nMarks) {
    	XBell(TheDisplay, 0);
    	return;
    }
    
    /* reselect marked the selection, and move the cursor to the marked pos */
    sel = &window->markTable[index].sel;
    oldSel = &window->buffer->primary;
    cursorPos = window->markTable[index].cursorPos;
    if (extendSel) {
	oldStart = oldSel->selected ? oldSel->start : TextGetCursorPos(w);
	oldEnd = oldSel->selected ? oldSel->end : TextGetCursorPos(w);
	newStart = sel->selected ? sel->start : cursorPos;
	newEnd = sel->selected ? sel->end : cursorPos;
	BufSelect(window->buffer, oldStart < newStart ? oldStart : newStart,
		oldEnd > newEnd ? oldEnd : newEnd);
    } else {
	if (sel->selected) {
    	    if (sel->rectangular)
    		BufRectSelect(window->buffer, sel->start, sel->end,
			sel->rectStart, sel->rectEnd);
    	    else
    		BufSelect(window->buffer, sel->start, sel->end);
	} else
    	    BufUnselect(window->buffer);
    }
    
    /* Move the window into a pleasing position relative to the selection
       or cursor.   MakeSelectionVisible is not great with multi-line
       selections, and here we will sometimes give it one.  And to set the
       cursor position without first using the less pleasing capability
       of the widget itself for bringing the cursor in to view, you have to
       first turn it off, set the position, then turn it back on. */
    XtVaSetValues(w, textNautoShowInsertPos, False, NULL);
    TextSetCursorPos(w, cursorPos);
    MakeSelectionVisible(window, window->lastFocus);
    XtVaSetValues(w, textNautoShowInsertPos, True, NULL);
}

/*
** Keep the marks in the windows book-mark table up to date across
** changes to the underlying buffer
*/
void UpdateMarkTable(WindowInfo *window, int pos, int nInserted,
    	int nDeleted)
{
    int i;
    
    for (i=0; i<window->nMarks; i++) {
    	maintainSelection(&window->markTable[i].sel, pos, nInserted,
    	    	nDeleted);
    	maintainPosition(&window->markTable[i].cursorPos, pos, nInserted,
    	    	nDeleted);
    }
}

/*
** Update a selection across buffer modifications specified by
** "pos", "nDeleted", and "nInserted".
*/
static void maintainSelection(selection *sel, int pos, int nInserted,
	int nDeleted)
{
    if (!sel->selected || pos > sel->end)
    	return;
    maintainPosition(&sel->start, pos, nInserted, nDeleted);
    maintainPosition(&sel->end, pos, nInserted, nDeleted);
    if (sel->end <= sel->start)
	sel->selected = False;
}

/*
** Update a position across buffer modifications specified by
** "modPos", "nDeleted", and "nInserted".
*/
static void maintainPosition(int *position, int modPos, int nInserted,
    	int nDeleted)
{
    if (modPos > *position)
    	return;
    if (modPos+nDeleted <= *position)
    	*position += nInserted - nDeleted;
    else
    	*position = modPos;
}
