/*******************************************************************************
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
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *                                                                         *
*                                                                              *
*******************************************************************************/

#ifndef XNEDIT_FILTER_H
#define XNEDIT_FILTER_H

#include "nedit.h"

#include <X11/Intrinsic.h>
#include <X11/Xresource.h>
#include <Xm/Xm.h>
#include <X11/Xlib.h>

typedef struct IOFilter IOFilter;
struct IOFilter {
    char *name;
    char *cmdin;
    char *cmdout;
};

void FilterSettings(WindowInfo *window);

#endif //XNEDIT_FILTER_H
