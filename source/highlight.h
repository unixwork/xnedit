/*******************************************************************************
*                                                                              *
* highlight.h -- Nirvana Editor Syntax Highlighting Header File                *
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

#ifndef NEDIT_HIGHLIGHT_H_INCLUDED
#define NEDIT_HIGHLIGHT_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>

/* Pattern flags for modifying pattern matching behavior */
#define PARSE_SUBPATS_FROM_START 1
#define DEFER_PARSING 2
#define COLOR_ONLY 4

/* Don't use plain 'A' or 'B' for style indices, it causes problems 
   with EBCDIC coding (possibly negative offsets when subtracting 'A'). */
#define ASCII_A ((char)65)

/* Pattern specification structure */
typedef struct {
    char *name;
    char *startRE;
    char *endRE;
    char *errorRE;
    char *style;
    char *subPatternOf;
    int flags;
} highlightPattern;

/* Header for a set of patterns */
typedef struct {
    char *languageMode;
    int lineContext;
    int charContext;
    int nPatterns;
    highlightPattern *patterns;
} patternSet;

void SyntaxHighlightModifyCB(int pos, int nInserted, int nDeleted,
    	int nRestyled, const char *deletedText, void *cbArg);
void StartHighlighting(WindowInfo *window, int warn);
void StopHighlighting(WindowInfo *window);
void AttachHighlightToWidget(Widget widget, WindowInfo *window);
void FreeHighlightingData(WindowInfo *window);
void RemoveWidgetHighlight(Widget widget);
void UpdateHighlightStyles(WindowInfo *window);
int TestHighlightPatterns(patternSet *patSet);
Pixel AllocateColor(Widget w, const char *colorName);
Pixel AllocColor(Widget w, const char *colorName, int *r, int *g, int *b);
void* GetHighlightInfo(WindowInfo *window, int pos);
highlightPattern *FindPatternOfWindow(WindowInfo *window, char *name);
int HighlightCodeOfPos(WindowInfo *window, int pos);
int HighlightLengthOfCodeFromPos(WindowInfo *window, int pos, int *checkCode);
int StyleLengthOfCodeFromPos(WindowInfo *window, int pos, const char **checkStyleName);
char *HighlightNameOfCode(WindowInfo *window, int hCode);
char *HighlightStyleOfCode(WindowInfo *window, int hCode);
Pixel HighlightColorValueOfCode(WindowInfo *window, int hCode,
      int *r, int *g, int *b);
Pixel GetHighlightBGColorOfCode(WindowInfo *window, int hCode,
      int *r, int *g, int *b);

#endif /* NEDIT_HIGHLIGHT_H_INCLUDED */
