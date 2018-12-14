/*******************************************************************************
*                                                                              *
* fontsel.h -- Nirvana Editor Font Selector Dialog Header File                 *
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

#ifndef NEDIT_FONTSEL_H_INCLUDED
#define NEDIT_FONTSEL_H_INCLUDED

#include <X11/Intrinsic.h>

/*******************************************************************************
*                                                                              *
*     FontSel ()                                                               *
*                                                                              *
*                                                                              *
*            Function to put up a modal font selection dialog box. The purpose *
*            of this routine is to allow the user to interactively view sample *
*            fonts and to choose a font for current use.                       *
*                                                                              *
*     Arguments:                                                               *
*                                                                              *
*            Widget	parent 		- parent widget ID                     *
*                                                                              *
*            int        showPropFont    - ONLY_FIXED : shows only fixed fonts  *
*                                                      doesn't show prop font  *
*                                                      toggle button also.     *
*                                         PREF_FIXED : can select either fixed *
*                                                      or proportional fonts;  *
*                                                      but starting option is  *
*                                                      Fixed fonts.            *
*                                         PREF_PROP  : can select either fixed *
*                                                      or proportional fonts;  *
*                                                      but starting option is  *
*                                                      proportional fonts.     *
*                                                                              *
*           char *	currFont        - ASCII string that contains the name  *
*                                         of the currently selected font.      *
*                                                                              *
*     Returns:                                                                 *
*                                                                              *
*           pointer to an ASCII character string that contains the name of     *
*           the selected font (in X format for naming fonts); it is the users  *
*           responsibility to free the space allocated to this string.         *
*                                                                              *
*     Comments:                                                                *
*                                                                              *
*           The calling function has to call the appropriate routines to set   *
*           the current font to the one represented by the returned string.    *
*                                                                              *
*******************************************************************************/

/* constant values for controlling the proportional font toggle */

#define ONLY_FIXED	0
#define PREF_FIXED	1
#define PREF_PROP	2


/* function prototype */

char    *FontSel(Widget parent, int showPropFont, const char *currFont,
        Pixel sampleFG, Pixel sampleBG);

#endif /* NEDIT_FONTSEL_H_INCLUDED */
