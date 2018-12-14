/*******************************************************************************
*                                                                              *
* shift.h -- Nirvana Editor Shift Header File                                  *
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

#ifndef NEDIT_SHIFT_H_INCLUDED
#define NEDIT_SHIFT_H_INCLUDED

#include "nedit.h"

enum ShiftDirection {SHIFT_LEFT, SHIFT_RIGHT};

void ShiftSelection(WindowInfo *window, int direction, int byTab);
void UpcaseSelection(WindowInfo *window);
void DowncaseSelection(WindowInfo *window);
void FillSelection(WindowInfo *window);
char *ShiftText(char *text, int direction, int tabsAllowed, int tabDist,
	int nChars, int *newLen);

#endif /* NEDIT_SHIFT_H_INCLUDED */
