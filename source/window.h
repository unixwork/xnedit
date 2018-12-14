/*******************************************************************************
*                                                                              *
* window.h -- Nirvana Editor Window header file                                *
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

#ifndef NEDIT_WINDOW_H_INCLUDED
#define NEDIT_WINDOW_H_INCLUDED

#include "nedit.h"
#include "textBuf.h"

#include <X11/Intrinsic.h>

void AttachSessionMgrHandler(Widget appShell);
WindowInfo *CreateWindow(const char *title, char *geometry, int iconic);
void CloseWindow(WindowInfo *window);
int NWindows(void);
void UpdateWindowTitle(const WindowInfo *window);
void UpdateWindowReadOnly(WindowInfo *window);
void UpdateStatsLine(WindowInfo *window);
void UpdateWMSizeHints(WindowInfo *window);
void UpdateMinPaneHeights(WindowInfo *window);
void UpdateNewOppositeMenu(WindowInfo *window, int openInTab);
void SetWindowModified(WindowInfo *window, int modified);
void MakeSelectionVisible(WindowInfo *window, Widget textPane);
int GetSimpleSelection(textBuffer *buf, int *left, int *right);
WindowInfo *FindWindowWithFile(const char *name, const char *path);
void SetAutoIndent(WindowInfo *window, IndentStyle state);
void SetShowMatching(WindowInfo *window, ShowMatchingStyle state);
void SetFonts(WindowInfo *window, const char *fontName, const char *italicName,
	const char *boldName, const char *boldItalicName);
void SetColors(WindowInfo *window, const char *textFg, const char *textBg,  
        const char *selectFg, const char *selectBg, const char *hiliteFg, 
        const char *hiliteBg, const char *lineNoFg, const char *cursorFg);
void SetOverstrike(WindowInfo *window, int overstrike);
void SetAutoWrap(WindowInfo *window, WrapStyle state);
void SetAutoScroll(WindowInfo *window, int margin);
void SplitPane(WindowInfo *window);
Widget GetPaneByIndex(WindowInfo *window, int paneIndex);
int WidgetToPaneIndex(WindowInfo *window, Widget w);
void ClosePane(WindowInfo *window);
int GetShowTabBar(WindowInfo *window);
void ShowTabBar(WindowInfo *window, int state);
void ShowStatsLine(WindowInfo *window, int state);
void ShowISearchLine(WindowInfo *window, int state);
void TempShowISearch(WindowInfo *window, int state);
void ShowLineNumbers(WindowInfo *window, int state);
void SetModeMessage(WindowInfo *window, const char *message);
void ClearModeMessage(WindowInfo *window);
WindowInfo *WidgetToWindow(Widget w);
void AddSmallIcon(Widget shell);
void SetTabDist(WindowInfo *window, int tabDist);
void SetEmTabDist(WindowInfo *window, int emTabDist);
int CloseAllDocumentInWindow(WindowInfo *window);
WindowInfo* CreateDocument(WindowInfo* shellWindow, const char* name);
WindowInfo *TabToWindow(Widget tab);
void RaiseDocument(WindowInfo *window);
void RaiseDocumentWindow(WindowInfo *window);
void RaiseFocusDocumentWindow(WindowInfo *window, Boolean focus);
WindowInfo *MarkLastDocument(WindowInfo *window);
WindowInfo *MarkActiveDocument(WindowInfo *window);
void NextDocument(WindowInfo *window);
void PreviousDocument(WindowInfo *window);
void LastDocument(WindowInfo *window);
int NDocuments(WindowInfo *window);
WindowInfo *MoveDocument(WindowInfo *toWindow, WindowInfo *window);
WindowInfo *DetachDocument(WindowInfo *window);
void MoveDocumentDialog(WindowInfo *window);
WindowInfo* GetTopDocument(Widget w);
Boolean IsTopDocument(const WindowInfo *window);
int IsIconic(WindowInfo *window);
int IsValidWindow(WindowInfo *window);
void RefreshTabState(WindowInfo *window);
void ShowWindowTabBar(WindowInfo *window);
void RefreshMenuToggleStates(WindowInfo *window);
void RefreshWindowStates(WindowInfo *window);
void AllWindowsBusy(const char* message);
void AllWindowsUnbusy(void);
void SortTabBar(WindowInfo *window);
void SetBacklightChars(WindowInfo *window, char *applyBacklightTypes);
void SetToggleButtonState(WindowInfo *window, Widget w, Boolean state, 
        Boolean notify);
void SetSensitive(WindowInfo *window, Widget w, Boolean sensitive);
void CleanUpTabBarExposeQueue(WindowInfo *window);
#endif /* NEDIT_WINDOW_H_INCLUDED */
