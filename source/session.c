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

#include "file.h"
#include "preferences.h"
#include "window.h"

#include "../util/utils.h"
#include "../util/nedit_malloc.h"
#include "../util/filedialog.h"

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <pthread.h>

#include <Xm/XmAll.h>

#define XNSESSION_EXT ".xnsession"

XNESessionWriter* CreateSession(WindowInfo *window, const char *sessionName) {
    char *sessionFilePath = NULL;
    // if sessionName is an absolute path, use it as sessionFilePath
    // otherwise, create a session file in .xnedit/sessions/
    if(sessionName[0] == '/') {
        sessionFilePath = (char*)sessionName;
    } else {
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
        size_t sfilelen = sdirlen + 16 + strlen(sessionName);
        sessionFilePath = NEditMalloc(sfilelen);

        snprintf(sessionFilePath, sfilelen, "%s/%s%s", sessionDir, sessionName, XNSESSION_EXT);
    }
    
    // open file and create session struct
    int sessionFile = open(sessionFilePath, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(sessionFile < 0) {
        // TODO: error
        if(sessionName != sessionFilePath) NEditFree(sessionFilePath);
        return NULL;
    }
    if(sessionName != sessionFilePath) NEditFree(sessionFilePath);
    
    ftruncate(sessionFile, 0);
    
    XNESessionWriter *session = NEditMalloc(sizeof(XNESessionWriter));
    session->file = sessionFile;
    
    return session;
}

#define SN_BUFLEN 512
int SessionAddDocument(XNESessionWriter *session, WindowInfo *doc) {
    /*
     * Format:
     * 
     * Header { File }
     * 
     * Header: currently not specified
     * 
     * File:
     *   Name: <file name>
     *   [Path: <file path>]
     *   [Length: content length]
     *   [content]
     * 
     * Each file entry is separated by a line break
     * 
     * Path is only saved, if the file was already saved in xnedit
     * Length/Content is saved, when the file is in the modified state
     * 
     * 
     */
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


int CloseSession(XNESessionWriter *session) {
    int ret = close(session->file);
    
    // free data
    NEditFree(session);
    
    return ret;
}



static int parse_line(char *line, size_t len, size_t *contentlength, XNESessionEntry **cur) {
    if(len == 0) {
        return 0;
    }
    
    char *name = line;
    size_t name_end = 0;
    size_t value_start = 0;
    for(size_t i=0;i<len;i++) {
        if(line[i] == ':') {
            line[i] = 0; // terminate name
            name_end = i;
            value_start = i+1;
            break;
        }
    }
    
    if(name_end == 0) {
        return 1;
    }
    
    char *value = line + value_start;
    size_t value_len = len - value_start;
    
    while(value[0] == ' ') {
        value++;
        value_len--;
    }
    
    if(!strcmp(name, "Name")) {
        XNESessionEntry *new_entry = NEditMalloc(sizeof(XNESessionEntry));
        new_entry->name = value;
        new_entry->path = NULL;
        new_entry->length = 0;
        new_entry->content = NULL;
        new_entry->next = (*cur);
        *cur = new_entry;
    } else if(!strcmp(name, "Path")) {
        (*cur)->path = value;
    } else if(!strcmp(name, "Length")) {
        char *end;
        errno = 0;
        errno = 0;
        long long val = strtoll(value, &end, 0);
        if(errno != 0) {
             return 1;
        }
        *contentlength = val;
    }
    
    
    return 0;
}

static char readErrorMsg[256];

XNESession ReadSessionFile(const char *path) {
    XNESession session;
    session.buffer = NULL;
    session.entries = NULL;
    session.error = NULL;
    
    // open file and stat
    int fd = open(path, O_RDONLY);
    if(fd < 0) {
        session.error = strerror(errno);
        return session;
    }
    
    struct stat s;
    if(fstat(fd, &s)) {
        session.error = strerror(errno);
        return session;
    }
    
    // create buffer
    // the buffer contains the whole session file content
    // entry content will point directly to the buffer
    session.buffer = NEditMalloc(s.st_size + 1);
    session.buffer[s.st_size] = 0;
    
    // read file
    ssize_t r = read(fd, session.buffer, s.st_size);
    close(fd);
    if(r < s.st_size) {
        close(fd);
        session.error = "Could not read whole file"; // should not happen
        return session;
    }
    
    XNESessionEntry *current = NULL;
    
    size_t line_start = 0;
    size_t line_end = 0;
    size_t linenumber = 1;
    for(size_t i=0;i<=s.st_size;i++) {
        char c = session.buffer[i];
        if(c == '\n' || i == s.st_size) {
            // line end found (or eof)
            line_end = i;
            // make the line a null-terminated string
            // which makes the live easier later
            session.buffer[i] = 0;
            size_t line_len = line_end - line_start;
            char *line = session.buffer + line_start;
            line_start = i+1; // next line index
            
            // parse the line
            // this will set the entry pointers current and last
            size_t contentlength = 0;
            if(parse_line(line, line_len, &contentlength, &current)) {
                snprintf(readErrorMsg, 256, "Cannot parse session file: Syntax error on line %zu", linenumber);
                session.error = readErrorMsg;
                return session;
            }
            
            if(contentlength > 0) {
                if(contentlength > s.st_size - i) {
                    snprintf(readErrorMsg, 256, "Invalid content length on line %zu", linenumber);
                    return session;
                }
                
                current->length = contentlength;
                current->content = session.buffer + line_start;
                current->content[contentlength] = 0;
                
                // skip content
                i += contentlength + 2;
                line_start = i;
            }
            
            linenumber++;
        }
    }
    
    session.entries = current;
    
    return session;
}

void OpenDocumentsFromSession(WindowInfo *window, const char *sessionFile)
{
    XNESession session = ReadSessionFile(sessionFile);
    if(session.error) {
        // TODO: error
        return;
    }
    
    XNESessionEntry *entry = session.entries;
    while(entry) {
        XNESessionEntry *next_entry = entry->next;
        char *todo_enc = NULL;
        
        EditExistingFileExt(window, entry->name, entry->path, todo_enc, 0, NULL, False, 
            NULL, True, False, entry->content);
        
        
        NEditFree(entry);
        entry = next_entry;
    }
    
    NEditFree(session.buffer);
}

static int openSessionsDir(const char **sessionDirP, DIR **dirp, int *fd)
{
    const char *sessionDir = GetRCFileName(SESSION_DIR);
    
    int dir_fd = open(sessionDir, O_RDONLY);
    if(dir_fd == -1) {
        return 1;
    }
    DIR *dir = fdopendir(dir_fd);
    if(!dir) {
        close(dir_fd);
        return 1;
    }
    
    *sessionDirP = sessionDir;
    *dirp = dir;
    *fd = dir_fd;
    return 0;
}

static int readSessionFileEntry(int dir_fd, struct dirent *ent, size_t *nlen, struct stat *s)
{
    size_t namelen = strlen(ent->d_name);
    if(namelen < sizeof(XNSESSION_EXT)) {
        return 1;
    }
    char *ext = &ent->d_name[namelen - sizeof(XNSESSION_EXT) + 1];
    if(strcmp(ext, XNSESSION_EXT)) {
        return 1;
    }

    if(fstatat(dir_fd, ent->d_name, s, 0)) {
        return 1;
    }
    
    *nlen = namelen;
    return 0;
}

char* GetLatestSessionFile(void)
{
    const char *sessionDir;
    int dir_fd;
    DIR *dir;
    if(openSessionsDir(&sessionDir, &dir, &dir_fd)) {
        return NULL;
    }
    
    struct stat s;
    char *name = NULL;
    size_t name_alloc = 0;
    time_t mtime = 0;
    
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL) {
        size_t namelen;
        if(readSessionFileEntry(dir_fd, ent, &namelen, &s)) {
            continue;
        }
        
        if(s.st_mtime > mtime) {
            mtime = s.st_mtime;
            if(name_alloc < namelen + 1) {
                name_alloc = namelen + 1 < 512 ? 512 : namelen + 1;
                name = NEditRealloc(name, name_alloc);
            }
            memcpy(name, ent->d_name, namelen);
            name[namelen] = '\0';
        }
    }
    
    closedir(dir);
    
    char *fullPath = NULL;
    if(name) {
        fullPath = ConcatPath(sessionDir, name);
        NEditFree(name);
    }
    
    return fullPath;
}

/* ---------------------- CreateSessionMenu ---------------------- */

typedef struct MenuPaneList MenuPaneList;
typedef struct SessionFile SessionFile;

struct MenuPaneList {
    Widget menuPane;
    Widget placeHolder;
    MenuPaneList *next;
};

struct SessionFile {
    char *path; /* full file path */
    char *name; /* session name (without file extension) */
    time_t mtime;
};

static MenuPaneList *waitingMenus = NULL;
static pthread_mutex_t wmlock = PTHREAD_MUTEX_INITIALIZER;

static int sessionsLoaded = 0;
static int sessionsLoading = 0;

static SessionFile *sessionFiles = NULL;
static size_t numSessionFiles = 0;
static size_t allocSessionFiles = 0;


static void openSessionFileCB(Widget w, XtPointer clientData, XtPointer callData)
{
    char *path = clientData;
    OpenDocumentsFromSession(WidgetToWindow(w), path);
}

static void createSessionMenuItems(Widget menuPane, Widget placeHolder)
{
    if(placeHolder) {
        XtUnmanageChild(placeHolder);
        XtDestroyWidget(placeHolder);
    }
    
    Widget menuItem;
    XmString label;
    
    for(int i=0;i<numSessionFiles;i++) {
        SessionFile s = sessionFiles[i];
        
        label = XmStringCreateLocalized(s.name);
        menuItem = XtVaCreateManagedWidget(
                "sessionMenuItem", xmPushButtonWidgetClass, menuPane, 
                XmNlabelString, label,
                NULL);
        XtAddCallback(menuItem, XmNactivateCallback, (XtCallbackProc)openSessionFileCB, s.path);
        XmStringFree(label);
    }
}

static void finishLoadingSessions(XtPointer clientData, XtIntervalId *id)
{
    MenuPaneList *w = waitingMenus;
    while(w) {
        MenuPaneList *w_next = w->next;
        createSessionMenuItems(w->menuPane, w->placeHolder);
        NEditFree(w);
        w = w_next;
    }
    waitingMenus = NULL;
}

static int sessionFileCmp(const void *f1, const void *f2)
{
    const SessionFile *s1 = f1;
    const SessionFile *s2 = f2;
    
    return s1->mtime < s2->mtime;
}

static void* loadSessions(void *data)
{
    const char *sessionDir;
    int dir_fd;
    DIR *dir;
    if(openSessionsDir(&sessionDir, &dir, &dir_fd)) {
        return NULL;
    }
    
    struct stat s; 
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL) {
        size_t namelen;
        if(readSessionFileEntry(dir_fd, ent, &namelen, &s)) {
            continue;
        }
        
        if(numSessionFiles >= allocSessionFiles) {
            allocSessionFiles += 16;
            sessionFiles = NEditRealloc((void*)sessionFiles, allocSessionFiles * sizeof(SessionFile));
        }
        
        SessionFile f;
        f.path = ConcatPath(sessionDir, ent->d_name);
        f.name = NEditStrdup(FileName(f.path));
        f.name[strlen(f.name) - sizeof(XNSESSION_EXT) + 1] = '\0';
        f.mtime = s.st_mtime;
        sessionFiles[numSessionFiles++] = f;
    }
    
    closedir(dir);
    
    qsort(sessionFiles, numSessionFiles, sizeof(SessionFile), sessionFileCmp);
    
    // create menu items for all open windows
    pthread_mutex_lock(&wmlock);
    sessionsLoaded = 1;
    
    if(waitingMenus) {
        XtAppAddTimeOut(
                XtWidgetToApplicationContext((Widget)waitingMenus->menuPane),
                0,
                finishLoadingSessions,
                NULL);
    }
    
    pthread_mutex_unlock(&wmlock);
    return NULL;
}

void CreateSessionMenu(Widget menuPane)
{
    pthread_mutex_lock(&wmlock);
    
    // if the session dir is already loaded, just create the menu items
    if(sessionsLoaded) {
        createSessionMenuItems(menuPane, NULL);
        return;
    }
    
    // sessions not loaded yet, load in a separate thread
    if(!sessionsLoading) {
        pthread_t tid;
        pthread_create(&tid, NULL, loadSessions, NULL);
        sessionsLoading = 1;
    }
    
    // create placeholder menu item and add it to the waiting list
    Widget menuitem;
    XmString s1;
    
    s1 = XmStringCreateSimple("Loading...");
    menuitem = XtVaCreateWidget(
            "placeholder",xmLabelWidgetClass, menuPane, 
    	    XmNlabelString, s1,
    	    NULL);
    XmStringFree(s1);
    XtManageChild(menuitem);
    
    MenuPaneList *l = NEditMalloc(sizeof(MenuPaneList));
    l->menuPane = menuPane;
    l->next = waitingMenus;
    l->placeHolder = menuitem;
    waitingMenus = l;
    
    pthread_mutex_unlock(&wmlock);
}
