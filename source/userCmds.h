/*******************************************************************************
*                                                                              *
* userCmds.h -- Nirvana Editor user commands header file                       *
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

#ifndef NEDIT_USERCMDS_H_INCLUDED
#define NEDIT_USERCMDS_H_INCLUDED

#include "nedit.h"

void EditShellMenu(WindowInfo *window);
void EditMacroMenu(WindowInfo *window);
void EditBGMenu(WindowInfo *window);
void UpdateUserMenus(WindowInfo *window);
char *WriteShellCmdsString(void);
char *WriteMacroCmdsString(void);
char *WriteBGMenuCmdsString(void);
int LoadShellCmdsString(char *inString);
int LoadMacroCmdsString(char *inString);
int LoadBGMenuCmdsString(char *inString);
int DoNamedShellMenuCmd(WindowInfo *window, const char *itemName, int fromMacro);
int DoNamedMacroMenuCmd(WindowInfo *window, const char *itemName);
int DoNamedBGMenuCmd(WindowInfo *window, const char *itemName);
void RebuildAllMenus(WindowInfo *window);
void SetBGMenuUndoSensitivity(WindowInfo *window, int sensitive);
void SetBGMenuRedoSensitivity(WindowInfo *window, int sensitive);
void DimSelectionDepUserMenuItems(WindowInfo *window, int sensitive);
void DimPasteReplayBtns(int sensitive);
UserMenuCache *CreateUserMenuCache(void);
void FreeUserMenuCache(UserMenuCache *cache);
void InitUserBGMenuCache(UserBGMenuCache *cache);
void FreeUserBGMenuCache(UserBGMenuCache *cache);
void SetupUserMenuInfo(void);
void UpdateUserMenuInfo(void);

#endif /* NEDIT_USERCMDS_H_INCLUDED */
