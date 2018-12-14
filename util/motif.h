/*******************************************************************************
*                                                                              *
* motif.h:  Determine stability of Motif                                       *
*                                                                              *
* Copyright (C) 2004 Scott Tringali                                            *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
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
* Written by Scott Tringali                                                    *
*                                                                              *
*******************************************************************************/

enum MotifStability
{
    MotifKnownGood,
    MotifUnknown,
    MotifKnownBad
};

/* Return the stability of the Motif compiled-in */
enum MotifStability GetMotifStability(void);

/* Acquire a list of good version for showing to the user. */
const char *GetMotifStableVersions(void);
