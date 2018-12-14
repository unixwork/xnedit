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


#ifndef XmLGridH
#define XmLGridH

#include "XmL.h"
#include <stdio.h>

#ifdef XmL_CPP
extern "C" {
#endif

extern WidgetClass xmlGridWidgetClass;
typedef struct _XmLGridClassRec *XmLGridWidgetClass;
typedef struct _XmLGridRec *XmLGridWidget;
typedef struct _XmLGridRowRec *XmLGridRow;
typedef struct _XmLGridColumnRec *XmLGridColumn;
typedef struct _XmLGridCellRec *XmLGridCell;

#define XmLIsGrid(w) XtIsSubclass((w), xmlGridWidgetClass)

Widget XmLCreateGrid(Widget parent, char *name, ArgList arglist,
	Cardinal argcount);
void XmLGridAddColumns(Widget w, unsigned char type, int position, int count);
void XmLGridAddRows(Widget w, unsigned char type, int position, int count);
Boolean XmLGridColumnIsVisible(Widget w, int column);
Boolean XmLGridCopyPos(Widget w, Time time, unsigned char rowType, int row,
	unsigned char columnType, int column, int nrow, int ncolumn);
Boolean XmLGridCopySelected(Widget w, Time time);
void XmLGridDeleteAllColumns(Widget w, unsigned char type);
void XmLGridDeleteAllRows(Widget w, unsigned char type);
void XmLGridDeleteColumns(Widget w, unsigned char type, int position,
	int count);
void XmLGridDeleteRows(Widget w, unsigned char type, int position, int count);
void XmLGridDeselectAllCells(Widget w, Boolean notify);
void XmLGridDeselectAllColumns(Widget w, Boolean notify);
void XmLGridDeselectAllRows(Widget w, Boolean notify);
void XmLGridDeselectCell(Widget w, int row, int column, Boolean notify);
void XmLGridDeselectColumn(Widget w, int column, Boolean notify);
void XmLGridDeselectRow(Widget w, int row, Boolean notify);
int XmLGridEditBegin(Widget w, Boolean insert, int row, int column);
void XmLGridEditCancel(Widget w);
void XmLGridEditComplete(Widget w);
XmLGridColumn XmLGridGetColumn(Widget w, unsigned char columnType, int column);
void XmLGridGetFocus(Widget w, int *row, int *column, Boolean *focusIn);
XmLGridRow XmLGridGetRow(Widget w, unsigned char rowType, int row);
int XmLGridGetSelectedCellCount(Widget w);
int XmLGridGetSelectedCells(Widget w, int *rowPositions,
	int *columnPositions, int count);
int XmLGridGetSelectedColumnCount(Widget w);
int XmLGridGetSelectedColumns(Widget w, int *positions, int count);
int XmLGridGetSelectedRow(Widget w);
int XmLGridGetSelectedRowCount(Widget w);
int XmLGridGetSelectedRows(Widget w, int *positions, int count);
void XmLGridMoveColumns(Widget w, int newPosition, int position, int count);
void XmLGridMoveRows(Widget w, int newPosition, int position, int count);
Boolean XmLGridPaste(Widget w);
Boolean XmLGridPastePos(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column);
int XmLGridRead(Widget w, FILE *file, int format, char delimiter);
int XmLGridReadPos(Widget w, FILE *file, int format, char delimiter,
	unsigned char rowType, int row, unsigned char columnType, int column);
void XmLGridRedrawAll(Widget w);
void XmLGridRedrawCell(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column);
void XmLGridRedrawColumn(Widget w, unsigned char type, int column);
void XmLGridRedrawRow(Widget w, unsigned char type, int row);
void XmLGridReorderColumns(Widget w, int *newPositions,
	int position, int count);
void XmLGridReorderRows(Widget w, int *newPositions,
	int position, int count);
int XmLGridRowColumnToXY(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column, Boolean clipped, XRectangle *rect);
Boolean XmLGridRowIsVisible(Widget w, int row);
void XmLGridSelectAllCells(Widget w, Boolean notify);
void XmLGridSelectAllColumns(Widget w, Boolean notify);
void XmLGridSelectAllRows(Widget w, Boolean notify);
void XmLGridSelectCell(Widget w, int row, int column, Boolean notify);
void XmLGridSelectColumn(Widget w, int column, Boolean notify);
void XmLGridSelectRow(Widget w, int row, Boolean notify);
int XmLGridSetFocus(Widget w, int row, int column);
int XmLGridSetStrings(Widget w, char *data);
int XmLGridSetStringsPos(Widget w, unsigned char rowType, int row,
	unsigned char columnType, int column, char *data);
int XmLGridWrite(Widget w, FILE *file, int format, char delimiter,
	Boolean skipHidden);
int XmLGridWritePos(Widget w, FILE *file, int format, char delimiter,
	Boolean skipHidden, unsigned char rowType, int row,
	unsigned char columnType, int column, int nrow, int ncolumn);
int XmLGridXYToRowColumn(Widget w, int x, int y, unsigned char *rowType,
	int *row, unsigned char *columnType, int *column);

int XmLGridPosIsResize(Widget g, int x, int y);

void XmLGridSetVisibleColumnCount(Widget w, int num_visible);
void XmLGridHideRightColumn(Widget w);
void XmLGridUnhideRightColumn(Widget w);

int XmLGridGetRowCount(Widget w);
int XmLGridGetColumnCount(Widget w);

/* extern */ void 
XmLGridXYToCellTracking(Widget			widget, 
						int				x, /* input only args. */
						int				y, /* input only args. */
						Boolean *		m_inGrid, /* input/output args. */
						int *			m_lastRow, /* input/output args. */
						int *			m_lastCol, /* input/output args. */
						unsigned char * m_lastRowtype,/* input/output args. */
						unsigned char * m_lastColtype,/* input/output args. */
						int *			outRow, /* output only args. */
						int *			outCol, /* output only args. */
						Boolean *		enter, /* output only args. */
						Boolean *		leave); /* output only args. */

void XmLGridGetSort(Widget w, int *column, unsigned char *sortType);
void XmLGridSetSort(Widget w, int column, unsigned char sortType);

#ifdef XmL_CPP
}
#endif
#endif
