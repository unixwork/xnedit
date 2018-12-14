/*******************************************************************************
*                                                                              *
* file.h -- Nirvana Editor File Header File                                    *
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

#ifndef NEDIT_FILE_H_INCLUDED
#define NEDIT_FILE_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>

/* flags for EditExistingFile */
#define CREATE 1
#define SUPPRESS_CREATE_WARN 2
#define PREF_READ_ONLY 4

#define PROMPT_SBC_DIALOG_RESPONSE 0
#define YES_SBC_DIALOG_RESPONSE 1
#define NO_SBC_DIALOG_RESPONSE 2

WindowInfo *EditNewFile(WindowInfo *inWindow, char *geometry, int iconic,
        const char *languageMode, const char *defaultPath);
WindowInfo *EditExistingFile(WindowInfo *inWindow, const char *name,
        const char *path, int flags, char *geometry, int iconic,
	const char *languageMode, int tabbed, int bgOpen);
void RevertToSaved(WindowInfo *window);
int SaveWindow(WindowInfo *window);
int SaveWindowAs(WindowInfo *window, const char *newName, int addWrap);
int CloseAllFilesAndWindows(void);
int CloseFileAndWindow(WindowInfo *window, int preResponse);
void PrintWindow(WindowInfo *window, int selectedOnly);
void PrintString(const char *string, int length, Widget parent, const char *jobName);
int WriteBackupFile(WindowInfo *window);
int IncludeFile(WindowInfo *window, const char *name);
int PromptForExistingFile(WindowInfo *window, char *prompt, char *fullname);
int PromptForNewFile(WindowInfo *window, char *prompt, char *fullname,
    	int *fileFormat, int *addWrap);
int CheckReadOnly(WindowInfo *window);
void RemoveBackupFile(WindowInfo *window);
void UniqueUntitledName(char *name);
void CheckForChangesToFile(WindowInfo *window);

#endif /* NEDIT_FILE_H_INCLUDED */
