/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Microline Widget Library, originally made available under the NPL by Neuron Data <http://www.neurondata.com>.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * In addition, as a special exception to the GNU GPL, the copyright holders
 * give permission to link the code of this program with the Motif and Open
 * Motif libraries (or with modified versions of these that use the same
 * license), and distribute linked combinations including the two. You
 * must obey the GNU General Public License in all respects for all of
 * the code used other than linking with Motif/Open Motif. If you modify
 * this file, you may extend this exception to your version of the file,
 * but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 *
 * ***** END LICENSE BLOCK ***** */


#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <XmL/Grid.h>

Widget label, text, grid, gridText;

static int busy = 0;

void cellFocus();
void textModify();
void copy();
void paste();

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, copyButton, pasteButton;
	XmString str;
	char buf[4];
	int i;

	shell =  XtAppInitialize(&app, "Grid3", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNshadowThickness, 0,
		NULL);

	str = XmStringCreateSimple("(A 1)");
	label = XtVaCreateManagedWidget("label",
		xmLabelWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNtopAttachment, XmATTACH_FORM,
		XmNmarginHeight, 4,
		XmNleftAttachment, XmATTACH_FORM,
		XmNlabelString, str,
		NULL);
	XmStringFree(str);

	pasteButton = XtVaCreateManagedWidget("Paste To Focus",
		xmPushButtonWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNrightAttachment, XmATTACH_FORM,
		XmNmarginHeight, 0,
		NULL);
	XtAddCallback(pasteButton, XmNactivateCallback, paste, NULL);

	copyButton = XtVaCreateManagedWidget("Copy Selected",
		xmPushButtonWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, pasteButton,
		XmNmarginHeight, 0,
		NULL);
	XtAddCallback(copyButton, XmNactivateCallback, copy, NULL);

	text = XtVaCreateManagedWidget("text",
		xmTextWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "white", 6,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, label,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, copyButton,
		XmNmarginHeight, 0,
		NULL);

	/* Create a Grid with 1 heading column and 1 heading row */
	grid = XtVaCreateManagedWidget("grid",
		xmlGridWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNheadingColumns, 1,
		XmNcolumns, 26,
		XmNvisibleColumns, 8,
		XmNhsbDisplayPolicy, XmSTATIC,
		XmNrows, 100,
		XmNheadingRows, 1,
		XmNvisibleRows, 12,
		XmNvsbDisplayPolicy, XmSTATIC,
		XmNallowDragSelected, True,
		XmNallowDrop, True,
		XmNallowRowResize, True,
		XmNallowColumnResize, True,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, text,
		XmNtopOffset, 2,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNshadowThickness, 0,
		XmNselectionPolicy, XmSELECT_CELL,
		NULL);

	/* Make all cells in the content rows and content columns */
	/* (the data cells) editable and set their borders and color */
	XtVaSetValues(grid,
		XmNcellEditable, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XmNcellAlignment, XmALIGNMENT_RIGHT,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XtVaTypedArg, XmNcellRightBorderColor, XmRString, "#606060", 8,
		XtVaTypedArg, XmNcellBottomBorderColor, XmRString, "#606060", 8,
		NULL);

	/* Add callbacks which update the Label and to synchronize */
	/* the Text widget outside the Grid with the one inside the Grid */
	XtAddCallback(grid, XmNcellFocusCallback, cellFocus, NULL);
	XtAddCallback(text, XmNvalueChangedCallback, textModify, NULL);
	XtVaGetValues(grid,
		XmNtextWidget, &gridText,
		NULL);
	XtAddCallback(gridText, XmNvalueChangedCallback, textModify, NULL);

	XtVaSetValues(gridText,
		XtVaTypedArg, XmNbackground, XmRString, "white", 6,
		NULL);

	/* Set the labels on the heading rows and columns */
	for (i = 0; i < 26; i++)
	{
		sprintf(buf, "%c", 'A' + i);
		XmLGridSetStringsPos(grid, XmHEADING, 0, XmCONTENT, i, buf);
	}
	for (i = 0; i < 100; i++)
	{
		sprintf(buf, "%d", i + 1);
		XmLGridSetStringsPos(grid, XmCONTENT, i, XmHEADING, 0, buf);
	}

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}


void cellFocus(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	XmLGridCallbackStruct *cbs;
	XmLGridRow row;
	XmLGridColumn column;
	Widget sharedText;
	XmString str;
	char buf[10], *c;

	cbs = (XmLGridCallbackStruct *)callData;
	if (cbs->reason != XmCR_CELL_FOCUS_IN)
		return;

	/* Update the Label to reflect the new focus position */
	sprintf(buf, "(%c %d)", 'A' + cbs->column, cbs->row + 1);
	str = XmStringCreateSimple(buf);
	XtVaSetValues(label,
		XmNlabelString, str,
		NULL);
	XmStringFree(str);

	/* Set the Text widget outside the Grid to the string contained */
	/* in the new focus cell.  We set busy here because this change will */
	/* generate a valueChanged callback and we want to notify our */
	/* valueChanged callback not to do any processing because of */
	/* this change */
	if (busy)
		return;
	busy = 1;
	row = XmLGridGetRow(w, cbs->rowType, cbs->row);
	column = XmLGridGetColumn(w, cbs->columnType, cbs->column);
	XtVaGetValues(w,
		XmNrowPtr, row,
		XmNcolumnPtr, column,
		XmNcellString, &str,
		NULL);
	c = 0;
	if (str)
		XmStringGetLtoR(str, XmSTRING_DEFAULT_CHARSET, &c);
	if (c)
	{
		XmTextSetString(text, c);
		XtFree(c);
		XmTextSetSelection(text, 0, XmTextGetLastPosition(text),
			CurrentTime);
	}
	else
		XmTextSetString(text, "");
	if (str)
		XmStringFree(str);
	busy = 0;
}

void textModify(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	int row, column;
	Boolean focusIn;
	XmString str;
	char *c;

	/* If either Text widget changes (the Grid's Text widget or */
	/* the Text widget outside of the Grid), update the other one */
	/* to reflect to change and update the Grid itself if the */
	/* Text widget outside the Grid changes.  We set busy in this */
	/* function to keep the XmTextSetString() from causing this */
	/* callback to be called while inside the callback. */
	if (busy)
		return;
	busy = 1;
	c = XmTextGetString(w);
	if (w == gridText)
		XmTextSetString(text, c);
	else
	{
		XmLGridGetFocus(grid, &row, &column, &focusIn);
		if (row != -1 && column != -1)
		{
			str = XmStringCreateSimple(c);
			XtVaSetValues(grid,
				XmNrow, row,
				XmNcolumn, column,
				XmNcellString, str,
				NULL);
			XmStringFree(str);
			XmTextSetString(gridText, c);
		}
	}
	XtFree(c);
	busy = 0;
}

void copy(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	XmLGridCopySelected(grid, CurrentTime);
}

void paste(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	/* This pastes starting at the current focus cell, an alternative */
	/* method of pasting would be to have the user select an area */
	/* to paste into. To perform this, we could get the selected area */
	/* and use XmLGridPastePos() to paste into that area */
	XmLGridPaste(grid);
}
