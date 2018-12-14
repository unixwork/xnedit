/*******************************************************************************
*                                                                              *
* smartIndent.h -- Nirvana Editor Smart Indent Header File                     *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
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

#ifndef NEDIT_SMARTINDENT_H_INCLUDED
#define NEDIT_SMARTINDENT_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>

void BeginSmartIndent(WindowInfo *window, int warn);
void EndSmartIndent(WindowInfo *window);
void SmartIndentCB(Widget w, XtPointer clientData, XtPointer callData);
int LoadSmartIndentString(char *inString);
int LoadSmartIndentCommonString(char *inString);
char *WriteSmartIndentString(void);
char *WriteSmartIndentCommonString(void);
int SmartIndentMacrosAvailable(char *languageMode);
void EditSmartIndentMacros(WindowInfo *window);
void EditCommonSmartIndentMacro(void);
Boolean InSmartIndentMacros(WindowInfo *window);
int LMHasSmartIndentMacros(const char *languageMode);
void RenameSmartIndentMacros(const char *oldName, const char *newName);
void UpdateLangModeMenuSmartIndent(void);

#endif /* NEDIT_SMARTINDENT_H_INCLUDED */
