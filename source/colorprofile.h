/*******************************************************************************
*                                                                              *
* colorprofile.h -- Nirvana Editor Text Diplay Header File                     *
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
* colorprofile.h:                                                              *
*    Author: Olaf Wintermann                                                   *
*                                                                              *
*******************************************************************************/

#ifndef XNEDIT_COLORPROFILE_H
#define XNEDIT_COLORPROFILE_H

#include <X11/Xft/Xft.h>
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

typedef struct {
    char *name;
    char *color;
    char *bgColor;
    int font;
} highlightStyleRec;

typedef struct _ColorProfile {
    char     *name;

    char     *textFg;
    char     *textBg;
    char     *textBg2;
    char     *selectFg;
    char     *selectBg;
    char     *hiliteFg;
    char     *hiliteBg;
    char     *lineNoFg;
    char     *lineNoBg;
    char     *cursorFg;
    char     *lineHiBg;
    char     *ansiColorList;
    char     *rainbowColorList;
    char     *rightMargin;

    Boolean  colorsLoaded;
    Boolean  stylesLoaded;
    Boolean  resDBLoaded;

    XftColor textFgColor;
    XftColor textBgColor;
    XftColor textBg2Color;
    XftColor selectFgColor;
    XftColor selectBgColor;
    XftColor hiliteFgColor;
    XftColor hiliteBgColor;
    XftColor lineNoFgColor;
    XftColor lineNoBgColor;
    XftColor cursorFgColor;
    XftColor lineHiBgColor;
    XftColor rightMarginColor;

    XftColor *ansiColors;
    size_t   numAnsiColors;
    XftColor *rainbowColors;
    size_t   numRainbowColors;

    int styleType; // 0: default     styles points to default text styles
                   // 1: lightened   lightened default colors (styles allocated)
                   // 2: custom      custom color profile text styles (styles allocated)
    highlightStyleRec **styles;
    size_t numStyles;
    size_t stylesAlloc;

    char     *resourceFile;
    int      windowThemeVariant; // 0: undefined, 1: light, 2: dark
    XrmDatabase db;

    Boolean modified;
    Boolean removed;
    struct _ColorProfile *orig;

    struct _ColorProfile *next;
} ColorProfile;

#endif /* XNEDIT_COLORPROFILE_H */
