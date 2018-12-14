/*******************************************************************************
*                                                                              *
* linkdate.c -- Compile time configuration                                     *
*                                                                              *
* Copyright (C) 2001 Scott Tringali                                            *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* Noveber 30, 2001                                                             *
*                                                                              *
* Written by Scott Tringali, http://www.tringali.org                           *
*                                                                              *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

const char linkdate[] = __DATE__;
const char linktime[] = __TIME__;
