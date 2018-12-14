/*******************************************************************************
*                                                                              *
* menu.h -- Nirvana Editor Menu Header File                                    *
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

#ifndef NEDIT_MENU_H_INCLUDED
#define NEDIT_MENU_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

#define PERMANENT_MENU_ITEM (XtPointer)1
#define TEMPORARY_MENU_ITEM (XtPointer)2

Widget CreateMenuBar(Widget parent, WindowInfo *window);
void InstallMenuActions(XtAppContext context);
XtActionsRec *GetMenuActions(int *nActions);
void InvalidateWindowMenus(void);
void CheckCloseDim(void);
void AddToPrevOpenMenu(const char *filename);
void WriteNEditDB(void);
void ReadNEditDB(void);
Widget CreateBGMenu(WindowInfo *window);
void AddBGMenuAction(Widget widget);
void HidePointerOnKeyedEvent(Widget w, XEvent *event);
Widget CreateTabContextMenu(Widget parent, WindowInfo *window);
void AddTabContextMenuAction(Widget widget);
void ShowHiddenTearOff(Widget menuPane);

#endif /* NEDIT_MENU_H_INCLUDED */
