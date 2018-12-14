/*******************************************************************************
*                                                                              *
* selection.h -- Nirvana Editor Selection Header File                          *
*                                                                              *
* Copyright 2002 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef NEDIT_SELECTION_H_INCLUDED
#define NEDIT_SELECTION_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>
#include <X11/X.h>

int StringToLineAndCol(const char *text, int *lineNum, int *column );
void GotoSelectedLineNumber(WindowInfo *window, Time time);
void GotoLineNumber(WindowInfo *window);
void SelectNumberedLine(WindowInfo *window, int lineNum);
void OpenSelectedFile(WindowInfo *window, Time time);
char *GetAnySelection(WindowInfo *window);
void BeginMarkCommand(WindowInfo *window);
void BeginGotoMarkCommand(WindowInfo *window, int extend);
void AddMark(WindowInfo *window, Widget widget, char label);
void UpdateMarkTable(WindowInfo *window, int pos, int nInserted,
   	int nDeleted);
void GotoMark(WindowInfo *window, Widget w, char label, int extendSel);
void MarkDialog(WindowInfo *window);
void GotoMarkDialog(WindowInfo *window, int extend);

#endif /* NEDIT_SELECTION_H_INCLUDED */
