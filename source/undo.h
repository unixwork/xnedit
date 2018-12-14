/*******************************************************************************
*                                                                              *
* undo.h -- Nirvana Editor Undo header file                                    *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
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

#ifndef NEDIT_UNDO_H_INCLUDED
#define NEDIT_UNDO_H_INCLUDED

#include "nedit.h"

enum undoTypes {UNDO_NOOP, ONE_CHAR_INSERT, ONE_CHAR_REPLACE, ONE_CHAR_DELETE,
		BLOCK_INSERT, BLOCK_REPLACE, BLOCK_DELETE};

void Undo(WindowInfo *window);
void Redo(WindowInfo *window);
void SaveUndoInformation(WindowInfo *window, int pos, int nInserted,
	int nDeleted, const char *deletedText);
void ClearUndoList(WindowInfo *window);
void ClearRedoList(WindowInfo *window);

#endif /* NEDIT_UNDO_H_INCLUDED */
