/*
 * Copyright 2019 Olaf Wintermann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef XNEDIT_FILEDIALOG_H
#define XNEDIT_FILEDIALOG_H

#include <sys/types.h>
#include <Xm/XmAll.h>

#ifdef __cplusplus
extern "C" {
#endif
    
#define FILEDIALOG_OPEN 1
#define FILEDIALOG_SAVE 2
    
#define FILEDIALOG_OK 1
#define FILEDIALOG_CANCEL 2

typedef struct FileSelection {
    char    *path;
    char    *encoding;
    char    *filter;
    Boolean setenc;
    Boolean setxattr;
    Boolean writebom;
    Boolean addwrap;
    int     format;
} FileSelection;

int FileDialog(Widget parent, char *promptString, FileSelection *file, int type);

const char ** FileDialogDefaultEncodings(void);

char* ConcatPath(const char *parent, const char *name);
char* FileName(char *path);
char* ParentPath(char *path);

#ifdef __cplusplus
}
#endif

#endif /* XNEDIT_FILEDIALOG_H */

