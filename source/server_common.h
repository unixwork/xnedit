/*******************************************************************************
*									       *
* server_common.h -- Nirvana Editor common server stuff			       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
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
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* November, 1995							       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifndef NEDIT_SERVER_COMMON_H_INCLUDED
#define NEDIT_SERVER_COMMON_H_INCLUDED

#include <X11/Intrinsic.h>

/* Lets limit the unique server name to MAXPATHLEN */
#define MAXSERVERNAMELEN MAXPATHLEN

#define DEFAULTSERVERNAME ""

void CreateServerPropertyAtoms(const char *serverName, 
			       Atom *serverExistsAtomReturn, 
			       Atom *serverRequestAtomReturn);
Atom CreateServerFileOpenAtom(const char *serverName, 
	                      const char *path);
Atom CreateServerFileClosedAtom(const char *serverName, 
	                        const char *path,
                                Bool only_if_exists);
void DeleteServerFileAtoms(const char* serverName, Window rootWindow);

#endif /* NEDIT_SERVER_COMMON_H_INCLUDED */
