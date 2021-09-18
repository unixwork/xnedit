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

typedef struct XNESessionWriter XNESessionWriter;
typedef struct XNESession XNESession;

typedef struct XNESessionEntry XNESessionEntry;


struct XNESessionWriter {
    int file;
};


struct XNESessionEntry {
    char *name;
    char *path;
    size_t length;
    char *content;
    
    XNESessionEntry *next;
};

struct XNESession {
    XNESessionEntry *entries;
    
    char *error;
    
    char *buffer;
};

/*
 * Create a new session file
 */
XNESessionWriter* CreateSession(WindowInfo *window, const char *sessionName);

/*
 * Add a document to a session file
 * This will store the document path (if available), document settings
 * and the content (if not saved) to the session
 */
int SessionAddDocument(XNESessionWriter *session, WindowInfo *doc);

/*
 * Close the session file and free all data
 */
int CloseSession(XNESessionWriter *session);

/*
 * Open a session file and parse all entries
 *
 * Returns XNESessionEntry list
 */
XNESession ReadSessionFile(const char *path);

/*
 * Read a session file and open all session documents in a window
 */
void OpenDocumentsFromSession(WindowInfo *window, const char *sessionFile);

/*
 * Get the path of latest session file
 */
char* GetLatestSessionFile(void);

/*
 * Create menu items for stored sessions
 * if the menu items are not loaded yet, insert a placeholder menuitem,
 * that will be replaced later
 */
void CreateSessionMenu(Widget menuPane);

#endif /* XNE_SESSION_H */

