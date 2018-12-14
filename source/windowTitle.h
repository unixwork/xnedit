/*******************************************************************************
*                                                                              *
* windowTitle.h -- Nirvana Editor window title customization                   *
*                                                                              *
* Copyright (C) 2001, Arne Forlie                                              *
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
* Written by Arne Forlie, http://arne.forlie.com                               *
*                                                                              *
*******************************************************************************/

#ifndef NEDIT_WINDOWTITLE_H_INCLUDED
#define NEDIT_WINDOWTITLE_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>

char *FormatWindowTitle(const char* filename,
                        const char* path,
                        const char* clearCaseViewTag,
                        const char* serverName,
                        int isServer,
                        int filenameSet,
                        int lockReasons,
                        int fileChanged,
                        const char* titleFormat);
                        
void EditCustomTitleFormat(WindowInfo *window);

#endif /* NEDIT_WINDOWTITLE_H_INCLUDED */
