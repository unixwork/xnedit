/*******************************************************************************
*                                                                              *
* calltips.h -- Nirvana Editor Calltips Header File                            *
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

#ifndef NEDIT_CALLTIPS_H_INCLUDED
#define NEDIT_CALLTIPS_H_INCLUDED

#include "nedit.h"      /* For WindowInfo */
#include "textDisp.h"   /* for textDisp */

#define NEDIT_DEFAULT_CALLTIP_FG "black"
#define NEDIT_DEFAULT_CALLTIP_BG "LemonChiffon1"


enum TipHAlignMode {TIP_LEFT, TIP_CENTER, TIP_RIGHT};
enum TipVAlignMode {TIP_ABOVE, TIP_BELOW};
enum TipAlignStrict {TIP_SLOPPY, TIP_STRICT};

int  ShowCalltip(WindowInfo *window, char *text, Boolean anchored, 
        int pos, int hAlign, int vAlign, int alignMode);
void KillCalltip(WindowInfo *window, int calltipID);
void TextDKillCalltip(textDisp *textD, int calltipID);
int  GetCalltipID(WindowInfo *window, int calltipID);
void TextDRedrawCalltip(textDisp *textD, int calltipID);

#endif /* ifndef NEDIT_CALLTIPS_H_INCLUDED */
