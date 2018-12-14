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
#include <Xm/PushB.h>
#include <XmL/Grid.h>

void showSelected();

static char *data = 
"Country|Media|Price\n\
Europe|CD-ROM|$29\n\
Yugoslovia|Floppy|$39\n\
North America|Tape|$29\n\
South America|CD-ROM|$49\n\
Japan|Tape|$49\n\
Russia|Floppy|$49\n\
Poland|CD-ROM|$39\n\
Norway|CD-ROM|$29\n\
England|Tape|$49\n\
Jordan|CD-ROM|$39";

Widget grid;

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, button;
	XmString str;

	shell =  XtAppInitialize(&app, "Grid2", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNmarginWidth, 5,
		XmNmarginHeight, 5,
		XmNverticalSpacing, 5,
		XmNshadowThickness, 0,
		NULL);

	str = XmStringCreateSimple("Print Selected");
	button = XtVaCreateManagedWidget("button",
		xmPushButtonWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNlabelString, str,
		NULL);
	XmStringFree(str);
	XtAddCallback(button, XmNactivateCallback, showSelected, NULL);

	/* Create a Grid in multiple row select mode with 1 heading row */
	/* and 3 columns */
	grid = XtVaCreateManagedWidget("grid",
		xmlGridWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XmNheadingRows, 1,
		XmNvisibleRows, 7,
		XmNcolumns, 3,
		XmNsimpleWidths, "20c 10c 10c",
		XmNhorizontalSizePolicy, XmVARIABLE,
		XmNvsbDisplayPolicy, XmSTATIC,
		XmNhighlightRowMode, True,
		XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
		XmNshadowThickness, 0,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, button,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);
	/* Set default cell values for new cells (which will be the */
	/* cells created when we add content rows) */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		NULL);
	/* Set default cell alignment for new cells in columns 0 and 1 */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumnRangeStart, 0,
		XmNcolumnRangeEnd, 1,
		XmNcellAlignment, XmALIGNMENT_LEFT,
		NULL);
	/* Set default cell alignment for new cells in column 2 */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumn, 2,
		XmNcellAlignment, XmALIGNMENT_RIGHT,
		NULL);
	/* Add 10 content rows */
	XtVaSetValues(grid,
		XmNrows, 10,
		NULL);
	XmLGridSetStrings(grid, data);

	XtRealizeWidget(shell);

	XtAppMainLoop(app);
}

void showSelected(w, clientData, callData)
Widget w;
XtPointer clientData;
XtPointer callData;
{
	int i, count, *pos;

	printf ("Selected Rows: ");
	count = XmLGridGetSelectedRowCount(grid);
	if (count)
	{
		pos = (int *)malloc(sizeof(int) * count);
		XmLGridGetSelectedRows(grid, pos, count);
		for (i = 0; i < count; i++)
			printf ("%d ", pos[i]);
		free((char *)pos);
	}
	else
		printf ("none");
	printf ("\n");
}
