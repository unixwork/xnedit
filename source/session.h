/*******************************************************************************
*                                                                              *
* session.h -- XNEdit session storage                                          *
*                                                                              *
* Copyright 2021 Olaf Wintermann                                               *
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
*******************************************************************************/



#ifndef XNE_SESSION_H
#define XNE_SESSION_H

#include "nedit.h"

typedef struct XNESession XNESession;


struct XNESession {
    char *name;
    
    int file;
};

/*
 * Create a new session file
 */
XNESession* CreateSession(WindowInfo *window);

/*
 * Add a document to a session file
 * This will store the document path (if available), document settings
 * and the content (if not saved) to the session
 */
int SessionAddDocument(XNESession *session, WindowInfo *doc);

#endif /* XNE_SESSION_H */

