/*******************************************************************************
*                                                                              *
* highlightData.h -- Nirvana Editor Highlight Data Header File                 *
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

#ifndef NEDIT_HIGHLIGHTDATA_H_INCLUDED
#define NEDIT_HIGHLIGHTDATA_H_INCLUDED

#include "nedit.h"
#include "highlight.h"

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

patternSet *FindPatternSet(const char *langModeName);
int LoadHighlightString(char *inString, int convertOld);
char *WriteHighlightString(void);
int LoadStylesString(char *inString);
char *WriteStylesString(void);
void EditHighlightStyles(const char *initialStyle);
void EditHighlightPatterns(WindowInfo *window);
void UpdateLanguageModeMenu(void);
int LMHasHighlightPatterns(const char *languageMode);
XFontStruct *FontOfNamedStyle(WindowInfo *window, const char *styleName);
int FontOfNamedStyleIsBold(char *styleName);
int FontOfNamedStyleIsItalic(char *styleName);
char *ColorOfNamedStyle(const char *styleName);
char *BgColorOfNamedStyle(const char *styleName);
int IndexOfNamedStyle(const char *styleName);
int NamedStyleExists(const char *styleName);
void RenameHighlightPattern(const char *oldName, const char *newName);

#endif /* NEDIT_HIGHLIGHTDATA_H_INCLUDED */
