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
#include <XmL/Grid.h>

#define TITLEFONT "-*-helvetica-bold-r-*--*-140-*-*-*-*-iso8859-1"
#define BOLDFONT "-*-helvetica-bold-r-*--*-120-*-*-*-*-iso8859-1"
#define TEXTFONT "-*-helvetica-medium-r-*--*-100-*-*-*-*-iso8859-1"

static char *data =
"|1996 Income Summary\n\
|Shampoo|Conditioner|Soap|Total\n\
Revenues:\n\
Sales|$ 1,600,000|$ 1,000,000|$  800,000|$ 3,400,000\n\
Less Discounts|(16,000)|(10,000)|(8,000)|(34,000)\n\
Less Return Allowance|(8,000)|(5,000)|(4,000)|(17,000)\n\
  Net Revenue|1,576,000|985,000|792,000|3,349,000\n\
\n\
Less Expenses:\n\
Cost of Goods Sold|(640,000)|(330,000)|(264,000)|(1,234,000)\n\
Salary Expense|(380,000)|(280,000)|(180,000)|(840,000)\n\
Marketing Expense|(157,600)|(98,500)|(79,200)|(335,300)\n\
Rent Expense|(36,000)|(36,000)|(36,000)|(108,000)\n\
Misc. Other Expense|(36,408)|(22,335)|(16,776)|(75,519)\n\
  Total Expenses|(1,250,008)|(766,835)|(575,976)|(2,592,819)\n\
\n\
Income Tax Expense|(130,397)|(87,266)|(86,410)|(304,072)\n\
Net Income|195,595|130,899|129,614|456,109";

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, grid;
	int i, r;

	shell =  XtAppInitialize(&app, "Grid5", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	grid = XtVaCreateManagedWidget("grid",
		xmlGridWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNheadingColumns, 1,
		XmNcolumns, 3,
		XmNfooterColumns, 1,
		XmNsimpleWidths, "24c 11c 11c 11c 11c",
		XmNvisibleColumns, 10,
		XmNvisibleRows, 14,
		XtVaTypedArg, XmNfontList, XmRString, TEXTFONT,
		strlen(TEXTFONT) + 1,
		XmNselectionPolicy, XmSELECT_NONE,
		NULL);

	XtVaSetValues(grid,
		XmNlayoutFrozen, True,
		NULL);

	/* Add 'Income Summary' heading row with yellow background */
	XtVaSetValues(grid, 
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "#FFFF00", 8,
		XtVaTypedArg, XmNcellForeground, XmRString, "#000080", 8,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XtVaTypedArg, XmNcellBottomBorderColor, XmRString, "black", 6,
		XmNcellAlignment, XmALIGNMENT_CENTER,
		NULL);
	XmLGridAddRows(grid, XmHEADING, -1, 1);

	/* Set span on '1996 Income Summary' cell in heading row */
	XtVaSetValues(grid,
		XmNrowType, XmHEADING,
		XmNrow, 0,
		XmNcolumn, 0,
		XmNcellColumnSpan, 2,
		XtVaTypedArg, XmNcellFontList, XmRString, TITLEFONT,
		strlen(TITLEFONT) + 1,
		NULL);

	/* Add 'Shampoo Conditioner Soap' heading row with white background */
	XtVaSetValues(grid, 
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellFontList, XmRString, BOLDFONT,
		strlen(BOLDFONT) + 1,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNcellForeground, XmRString, "black", 6,
		XmNcellBottomBorderType, XmBORDER_NONE,
		NULL);
	XmLGridAddRows(grid, XmHEADING, -1, 1);

	/* Add content and footer rows with heading column 0 left justified */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcellAlignment, XmALIGNMENT_RIGHT,
		XtVaTypedArg, XmNcellFontList, XmRString, TEXTFONT,
		strlen(TEXTFONT) + 1,
		NULL);
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumnType, XmHEADING,
		XmNcolumn, 0,
		XmNcellAlignment, XmALIGNMENT_LEFT,
		NULL);
	XmLGridAddRows(grid, XmCONTENT, -1, 15);

	/* Add footer row with blue background */
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNcellBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNcellFontList, XmRString, BOLDFONT,
		strlen(BOLDFONT) + 1,
		NULL);
	XmLGridAddRows(grid, XmFOOTER, -1, 1);

	/* Bold 'Revenues' cell */
	XtVaSetValues(grid,
		XmNcolumnType, XmHEADING,
		XmNcolumn, 0,
		XmNrow, 0,
		XtVaTypedArg, XmNcellFontList, XmRString, BOLDFONT,
		strlen(BOLDFONT) + 1,
		NULL);

	/* Bold 'Less Expenses' cell */
	XtVaSetValues(grid,
		XmNcolumnType, XmHEADING,
		XmNcolumn, 0,
		XmNrow, 6,
		XtVaTypedArg, XmNcellFontList, XmRString, BOLDFONT,
		strlen(BOLDFONT) + 1,
		NULL);

	/* Grey middle and footer content column */
	XtVaSetValues(grid,
		XmNcolumnType, XmALL_TYPES,
		XmNcolumnRangeStart, 2,
		XmNcolumnRangeEnd, 4,
		XmNcolumnStep, 2,
		XtVaTypedArg, XmNcellBackground, XmRString, "#E8E8E8", 8,
		NULL);

	/* Grey 'Conditioner' and 'Total' cell in heading row 1 */
	XtVaSetValues(grid,
		XmNrowType, XmHEADING,
		XmNrow, 1,
		XmNcolumnType, XmALL_TYPES,
		XmNcolumnRangeStart, 2,
		XmNcolumnRangeEnd, 4,
		XmNcolumnStep, 2,
		XtVaTypedArg, XmNcellBackground, XmRString, "#E8E8E8", 8,
		NULL);

	/* Blue and bold 'Net Revenue' and 'Total Expenses' rows */
	XtVaSetValues(grid,
		XmNrowRangeStart, 4,
		XmNrowRangeEnd, 12,
		XmNrowStep, 8,
		XmNcolumnType, XmALL_TYPES,
		XtVaTypedArg, XmNcellForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNcellBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNcellFontList, XmRString, BOLDFONT,
		strlen(BOLDFONT) + 1,
		NULL);

	XtVaSetValues(grid,
		XmNlayoutFrozen, False,
		NULL);

	XmLGridSetStrings(grid, data);

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
