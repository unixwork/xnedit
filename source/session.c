/*******************************************************************************
*                                                                              *
* session.c -- XNEdit session storage                                          *
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

#include "session.h"

#include "../util/utils.h"
#include "../util/nedit_malloc.h"

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#define XNE_SESSION_FILE_LEN 16

XNESession* CreateSession(WindowInfo *window) {
    const char *sessionDir = GetRCFileName(SESSION_DIR);
    size_t sdirlen = strlen(sessionDir);
    
    // check if the session directory exists and if not, create it
    struct stat s;
    if(stat(sessionDir, &s)) {
        if(errno == ENOENT) {
            if(mkdir(sessionDir, S_IRWXU)) {
                // TODO: error
                return NULL;
            }
        } else {
            // TODO: error
            return NULL;
        }
    }
    
    // create session file name
    size_t sfilelen = sdirlen + 2 + XNE_SESSION_FILE_LEN;
    char *sessionFilePath = malloc(sfilelen);
    
    snprintf(sessionFilePath, sfilelen, "%s/%s", sessionDir, "last");
    
    // open file and create session struct
    int sessionFile = open(sessionFilePath, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(sessionFile < 0) {
        // TODO: error
        free(sessionFilePath);
        return NULL;
    }
    free(sessionFilePath);
    
    ftruncate(sessionFile, 0);
    
    printf("session: %s\n", sessionFilePath);
    
    XNESession *session = NEditMalloc(sizeof(XNESession));
    session->file = sessionFile;
    
    
    return session;
}

#define SN_BUFLEN 512
int SessionAddDocument(XNESession *session, WindowInfo *doc) {
    char buf[SN_BUFLEN];
    int len = 0;
    
    len = snprintf(buf, SN_BUFLEN, "\nName: ");
    write(session->file, buf, len);
    write(session->file, doc->filename, strlen(doc->filename));
    
    size_t pathLen = strlen(doc->path);
    if(doc->filenameSet) {
        len = snprintf(buf, SN_BUFLEN, "\nPath: ");
        write(session->file, buf, len);
        write(session->file, doc->path, pathLen);
    }
    
    if(doc->fileChanged) {
        char *fileContent = BufGetAll(doc->buffer);
        size_t fileLen = doc->buffer->length;
        
        len = snprintf(buf, SN_BUFLEN, "\nLength: %zu\n", fileLen);
        write(session->file, buf, len);
        write(session->file, fileContent, fileLen);
    }
    
    return 0;
}
