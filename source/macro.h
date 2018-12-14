/*******************************************************************************
*                                                                              *
* macro.h -- Nirvana Editor Macro Header File                                  *
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

#ifndef NEDIT_MACRO_H_INCLUDED
#define NEDIT_MACRO_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>

#define REPEAT_TO_END -1
#define REPEAT_IN_SEL -2
	
void RegisterMacroSubroutines(void);
void AddLastCommandActionHook(XtAppContext context);
void BeginLearn(WindowInfo *window);
void FinishLearn(void);
void CancelMacroOrLearn(WindowInfo *window);
void Replay(WindowInfo *window);
void SafeGC(void);
void DoMacro(WindowInfo *window, const char *macro, const char *errInName);
void ResumeMacroExecution(WindowInfo *window);
void AbortMacroCommand(WindowInfo *window);
int MacroWindowCloseActions(WindowInfo *window);
void RepeatDialog(WindowInfo *window);
void RepeatMacro(WindowInfo *window, const char *command, int how);
int ReadMacroFile(WindowInfo *window, const char *fileName, int warnNotExist);
int ReadMacroString(WindowInfo *window, char *string, const char *errIn);
int CheckMacroString(Widget dialogParent, char *string, const char *errIn,
	char **errPos);
char *GetReplayMacro(void);
void ReadMacroInitFile(WindowInfo *window);
void ReturnShellCommandOutput(WindowInfo *window, const char *outText, int status);

#endif /* NEDIT_MACRO_H_INCLUDED */
