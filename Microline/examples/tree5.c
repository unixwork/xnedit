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
#include <XmL/Tree.h>

#define sphere_width 16
#define sphere_height 16
static unsigned char sphere_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0x38, 0x3f,
	0xb8, 0x3f, 0xf8, 0x3f, 0xf8, 0x3f, 0xf8, 0x3f, 0xf0, 0x1f, 0xe0, 0x0f,
	0xc0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#define monitor_width 16
#define monitor_height 16
static unsigned char monitor_bits[] = {
	0x00, 0x00, 0xf8, 0x3f, 0xf8, 0x3f, 0x18, 0x30, 0x58, 0x37, 0x18, 0x30,
	0x58, 0x37, 0x18, 0x30, 0xf8, 0x3f, 0xf8, 0x3f, 0x80, 0x03, 0x80, 0x03,
	0xf0, 0x1f, 0xf0, 0x1f, 0x00, 0x00, 0x00, 0x00};

#if 0
void
hide_cb(Widget w, XtPointer clientData, XtPointer cb_data)
{
	Widget tree = (Widget) clientData;

    XmLGridHideRightColumn(tree);
}
void
show_cb(Widget w, XtPointer clientData, XtPointer cb_data)
{
	Widget tree = (Widget) clientData;

    XmLGridUnhideRightColumn(tree);
}
#endif /*0*/


main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, tree, form, hideB, showB;
	XmLTreeRowDefinition *rows;
	Pixmap monitorPixmap, spherePixmap;
	Pixel black, white;
	int i, n, size;
	static struct
			{
		Boolean expands;
		int level;
		char *string;
	} data[] =
	{
		{ True,  0, "Root" },
		{ True,  1, "Parent A" },
		{ False, 2, "Node A1" },
		{ False, 2, "Node A2" },
		{ True,  2, "Parent B" },
		{ False, 3, "Node B1" },
		{ False, 3, "Node B2" },
		{ True,  1, "Parent C" },
		{ False, 2, "Node C1" },
		{ True,  1, "Parent D" },
		{ False, 2, "Node D1" },
	};

	shell =  XtAppInitialize(&app, "Tree3", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	black = BlackPixelOfScreen(XtScreen(shell));
	white = WhitePixelOfScreen(XtScreen(shell));
	spherePixmap = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		sphere_bits, sphere_width, sphere_height,
		black, white,
		DefaultDepthOfScreen(XtScreen(shell)));
	monitorPixmap = XCreatePixmapFromBitmapData(XtDisplay(shell),
		DefaultRootWindow(XtDisplay(shell)),
		monitor_bits, monitor_width, monitor_height,
		black, white,
		DefaultDepthOfScreen(XtScreen(shell)));

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNshadowThickness, 0,
		NULL);

#if 0
	hideB = XtVaCreateManagedWidget("hide",
		xmPushButtonWidgetClass, form,
		XmNleftAttachment, XmATTACH_NONE,
		XmNtopAttachment, XmATTACH_POSITION,
        XmNtopPosition, 30,
		XmNbottomAttachment, XmATTACH_NONE,
		XmNrightAttachment, XmATTACH_FORM,
        NULL);

    showB = XtVaCreateManagedWidget("show",
		xmPushButtonWidgetClass, form,
		XmNleftAttachment, XmATTACH_NONE,
		XmNtopAttachment, XmATTACH_POSITION,
        XmNtopPosition, 30,
		XmNbottomAttachment, XmATTACH_NONE,
		XmNrightAttachment, XmATTACH_WIDGET,
        XmNrightWidget, hideB,
        NULL);
#endif /*0*/

	/* Create a Tree with 3 columns and 1 heading row in multiple */
	/* select mode.  We also set globalPixmapWidth and height here */
	/* which specifys that every Pixmap we set on the Tree will be */
	/* the size specified (16x16).  This will increase performance. */
	tree = XtVaCreateManagedWidget("tree",
		xmlTreeWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNconnectingLineColor, XmRString, "#808080", 8,
        XmNhorizontalSizePolicy,  XmRESIZE_IF_POSSIBLE,
		XmNallowColumnResize, True,
		XmNheadingRows, 1,
		XmNvisibleRows, 14,
		XmNcolumns, 3,
		XmNvisibleColumns, 1,
        XmNhideUnhideButtons, True,
		XmNsimpleWidths, "80c 40c 40c",
		XmNsimpleHeadings, "All Folders|SIZE|DATA2",
		XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
		XmNhighlightRowMode, True,
		XmNglobalPixmapWidth, 16,
		XmNglobalPixmapHeight, 16,
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
#if 0
        XmNrightAttachment, XmATTACH_WIDGET,
        XmNrightWidget, showB,
#endif /*0*/
        XmNdebugLevel, 2,
		NULL);

	/* Set default values for new cells (the cells in the content rows) */
	XtVaSetValues(tree,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
        XmNcellEditable, True,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		NULL);

#if 0
     XtAddCallback(hideB, XmNactivateCallback, hide_cb, (XtPointer)tree);
     XtAddCallback(showB, XmNactivateCallback, show_cb, (XtPointer)tree);
#endif /*0*/

	/* Create a TreeRowDefinition array from the data array */
	/* and add rows to the Tree */
	n = 11;
	size = sizeof(XmLTreeRowDefinition) * n;
	rows = (XmLTreeRowDefinition *)malloc(size);
	for (i = 0; i < n; i++)
	{
		rows[i].level = data[i].level;
		rows[i].expands = data[i].expands;
		rows[i].isExpanded = True;
		if (data[i].expands)
			rows[i].pixmap = spherePixmap;
		else
			rows[i].pixmap = monitorPixmap;
		rows[i].pixmask = XmUNSPECIFIED_PIXMAP;
		rows[i].string = XmStringCreateSimple(data[i].string);
	}
	XmLTreeAddRows(tree, rows, n, -1);

	/* Free the TreeRowDefintion array we created above and set strings */
	/* in column 1 and 2 */
	for (i = 0; i < n; i++)
	{
		XmStringFree(rows[i].string);
		XmLGridSetStringsPos(tree, XmCONTENT, i, XmCONTENT, 1, "1032|1123");
	}
	free((char *)rows);
	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
