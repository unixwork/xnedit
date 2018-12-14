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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<XmL/GridUtil.c>										*/
/*																		*/
/* Description:	XmLGrid misc utilities.  They are here in order not to	*/
/*	            continue bloating Grid.c beyond hope.					*/
/*																		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/* Created: 	Thu May 28 21:55:45 PDT 1998		 					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include "GridP.h"

#include <assert.h>

/*----------------------------------------------------------------------*/
/* extern */ int 
XmLGridGetRowCount(Widget w)
{
	XmLGridWidget g = (XmLGridWidget) w;

	assert( g != NULL );

#ifdef DEBUG_ramiro
	{
		int rows = 0;
		
		XtVaGetValues(w,XmNrows,&rows,NULL);

		assert( rows == g->grid.rowCount );
	}
#endif	

	return g->grid.rowCount;
}
/*----------------------------------------------------------------------*/
/* extern */ int 
XmLGridGetColumnCount(Widget w)
{
	XmLGridWidget g = (XmLGridWidget) w;

	assert( g != NULL );

#ifdef DEBUG_ramiro
	{
		int columns = 0;
		
		XtVaGetValues(w,XmNcolumns,&columns,NULL);

		assert( columns == g->grid.colCount );
	}
#endif	

	return g->grid.colCount;
}
/*----------------------------------------------------------------------*/
/* extern */ void 
XmLGridXYToCellTracking(Widget			widget, 
						int				x, /* input only args. */
						int				y, /* input only args. */
						Boolean *		m_inGrid, /* input/output args. */
						int *			m_lastRow, /* input/output args. */
						int *			m_lastCol, /* input/output args. */
						unsigned char *	m_lastRowtype, /* input/output args. */
						unsigned char *	m_lastColtype,/* input/output args. */
						int *			outRow, /* output only args. */
						int *			outCol, /* output only args. */
						Boolean *		enter, /* output only args. */
						Boolean *		leave) /* output only args. */
{
	int				m_totalLines = 0;
	int				m_numcolumns = 0;
	int				row = 0;
	int				column = 0;
	unsigned char	rowtype = XmCONTENT;
	unsigned char	coltype = XmCONTENT;
	
	if (0 > XmLGridXYToRowColumn(widget,
								 x,
								 y,
								 &rowtype,
								 &row,
								 &coltype,
								 &column)) 
	{
		/* In grid; but, not in any cells
		 */
		/* treat it as a leave
		 */
		*enter = FALSE;
		*leave = TRUE;		
		return;
	}/* if */
	
	m_totalLines = XmLGridGetRowCount(widget);
	m_numcolumns = XmLGridGetColumnCount(widget);

	if ((row < m_totalLines) &&
		(column < m_numcolumns) &&
		((*m_lastRow != row)||
		 (*m_lastCol != column) ||
		 (*m_lastRowtype != rowtype)||
		 (*m_lastColtype != coltype))) 
	{
		*outRow = (rowtype == XmHEADING)?-1:row;
		*outCol = column;
		
		if (*m_inGrid == False) 
		{
			*m_inGrid = True;
			
			/* enter a cell
			 */
			*enter = TRUE;
			*leave = FALSE;
			
		}/* if */
		else 
		{
			/* Cruising among cells
			 */
			*enter = TRUE;
			*leave = TRUE;
		}/* else */
		*m_lastRow = row;
		*m_lastCol = column;
		*m_lastRowtype = rowtype ;
		*m_lastColtype  = coltype ;
	}/* row /col in grid */				
}
/*----------------------------------------------------------------------*/
