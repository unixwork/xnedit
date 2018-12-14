/*******************************************************************************
*                                                                              *
* shell.h -- Nirvana Editor Shell Header File                                  *
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

#ifndef NEDIT_SHELL_H_INCLUDED
#define NEDIT_SHELL_H_INCLUDED

#include "nedit.h"

/* sources for command input and destinations for command output */
enum inSrcs {FROM_SELECTION, FROM_WINDOW, FROM_EITHER, FROM_NONE};
enum outDests {TO_SAME_WINDOW, TO_NEW_WINDOW, TO_DIALOG};

void FilterSelection(WindowInfo *window, const char *command, int fromMacro);
void ExecShellCommand(WindowInfo *window, const char *command, int fromMacro);
void ExecCursorLine(WindowInfo *window, int fromMacro);
void ShellCmdToMacroString(WindowInfo *window, const char *command,
        const char *input);
void DoShellMenuCmd(WindowInfo *window, const char *command, int input,
        int output, int outputReplaceInput,
	int saveFirst, int loadAfter, int fromMacro);
void AbortShellCommand(WindowInfo *window);

#endif /* NEDIT_SHELL_H_INCLUDED */
