/*******************************************************************************
*									       *
* server_common.c -- Nirvana Editor common server stuff			       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
* 									       *
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
#include <stdio.h>
#include <Xm/Xm.h>
#include <sys/types.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/
#include "nedit.h"
#include "server_common.h"
#include "../util/utils.h"

/*
 * Create the server property atoms for the server with serverName.
 * Atom names are generated as follows:
 * 
 * NEDIT_SERVER_EXISTS_<host_name>_<user>_<server_name>
 * NEDIT_SERVER_REQUEST_<host_name>_<user>_<server_name>
 * 
 * <server_name> is the name that can be set by the user to allow
 * for multiple servers to run on the same display. <server_name>
 * defaults to "" if not supplied by the user.
 * 
 * <user> is the user name of the current user.
 */
void CreateServerPropertyAtoms(const char *serverName, 
			       Atom *serverExistsAtomReturn, 
			       Atom *serverRequestAtomReturn)
{
    char propName[20+1+MAXNODENAMELEN+1+MAXUSERNAMELEN+1+MAXSERVERNAMELEN];
    const char *userName = GetUserName();
    const char *hostName = GetNameOfHost();

    sprintf(propName, "NEDIT_SERVER_EXISTS_%s_%s_%s", hostName, userName, serverName);
    *serverExistsAtomReturn = XInternAtom(TheDisplay, propName, False);
    sprintf(propName, "NEDIT_SERVER_REQUEST_%s_%s_%s", hostName, userName, serverName);
    *serverRequestAtomReturn = XInternAtom(TheDisplay, propName, False);
}

/*
 * Create the individual property atoms for each file being
 * opened by the server with serverName. This atom is used
 * by nc to monitor if the file has been closed.
 *
 * Atom names are generated as follows:
 * 
 * NEDIT_FILE_<host_name>_<user>_<server_name>_<path>
 * 
 * <server_name> is the name that can be set by the user to allow
 * for multiple servers to run on the same display. <server_name>
 * defaults to "" if not supplied by the user.
 * 
 * <user> is the user name of the current user.
 * 
 * <path> is the path of the file being edited.
 */
Atom CreateServerFileOpenAtom(const char *serverName, 
	                      const char *path)
{
    char propName[10+1+MAXNODENAMELEN+1+MAXUSERNAMELEN+1+MAXSERVERNAMELEN+1+MAXPATHLEN+1+7];
    const char *userName = GetUserName();
    const char *hostName = GetNameOfHost();
    Atom        atom;

    sprintf(propName, "NEDIT_FILE_%s_%s_%s_%s_WF_OPEN", hostName, userName, serverName, path);
    atom = XInternAtom(TheDisplay, propName, False);
    return(atom);
}

Atom CreateServerFileClosedAtom(const char *serverName, 
	                        const char *path,
                                Bool only_if_exist)
{
    char propName[10+1+MAXNODENAMELEN+1+MAXUSERNAMELEN+1+MAXSERVERNAMELEN+1+MAXPATHLEN+1+9];
    const char *userName = GetUserName();
    const char *hostName = GetNameOfHost();
    Atom        atom;

    sprintf(propName, "NEDIT_FILE_%s_%s_%s_%s_WF_CLOSED", hostName, userName, serverName, path);
    atom = XInternAtom(TheDisplay, propName, only_if_exist);
    return(atom);
}

/*
 * Delete all File atoms that belong to this server (as specified by
 * <host_name>_<user>_<server_name>).
 */
void DeleteServerFileAtoms(const char* serverName, Window rootWindow)
{
    char propNamePrefix[10+1+MAXNODENAMELEN+1+MAXUSERNAMELEN+1+MAXSERVERNAMELEN+1];
    const char *userName = GetUserName();
    const char *hostName = GetNameOfHost();
    int length = sprintf(propNamePrefix, "NEDIT_FILE_%s_%s_%s_", hostName, userName, serverName);

    int nProperties;
    Atom* atoms = XListProperties(TheDisplay, rootWindow, &nProperties);
    if (atoms != NULL) {
        int i;
        for (i = 0; i < nProperties; i++) {
            /* XGetAtomNames() is more efficient, but doesn't exist in X11R5. */
            char *name = XGetAtomName(TheDisplay, atoms[i]);
            if (name != NULL && strncmp(propNamePrefix, name, length) == 0) {
                XDeleteProperty(TheDisplay, rootWindow, atoms[i]);
            }
            XFree(name);
        }
        XFree((char*)atoms);
    }
}    
