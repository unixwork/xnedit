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

#include "filedialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>

#include "icons.h"

#include <X11/xpm.h>

#include"../Microline/XmL/Grid.h"

/* nedit utils */
#include "nedit_malloc.h"
#include "utils.h"
#include "fileUtils.h"
#include "getfiles.h"
#include "misc.h"

#include "../source/preferences.h"

#include "DialogF.h"

#define WIDGET_SPACING 5
#define WINDOW_SPACING 8

static int pixmaps_initialized = 0;
static Pixmap folderIcon;
static Pixmap fileIcon;
static Pixmap folderShape;
static Pixmap fileShape;

static int LastView = -1; // 0: icon   1: list   2: grid(not finished yet)

void initPixmaps(Display *dp, Drawable d)
{
    if(XpmCreatePixmapFromData(dp, d, DtdirB_m_pm, &folderIcon, &folderShape, NULL)) {
        fprintf(stderr, "failed to create folder pixmap\n");
    }
    if(XpmCreatePixmapFromData(dp, d, Dtdata_m_pm, &fileIcon, &fileShape, NULL)) {
        fprintf(stderr, "failed to create file pixmap\n");
    }
    pixmaps_initialized = 1;
}


/* -------------------- path utils -------------------- */

char* ConcatPath(const char *parent, const char *name)
{ 
    size_t parentlen = strlen(parent);
    size_t namelen = strlen(name);
    
    size_t pathlen = parentlen + namelen + 2;
    char *path = NEditMalloc(pathlen);
    
    memcpy(path, parent, parentlen);
    if(parentlen > 0 && parent[parentlen-1] != '/') {
        path[parentlen] = '/';
        parentlen++;
    }
    if(name[0] == '/') {
        name++;
        namelen--;
    }
    memcpy(path+parentlen, name, namelen);
    path[parentlen+namelen] = '\0';
    return path;
}

char* FileName(char *path) {
    int si = 0;
    int osi = 0;
    int i = 0;
    int p = 0;
    char c;
    while((c = path[i]) != 0) {
        if(c == '/') {
            osi = si;
            si = i;
            p = 1;
        }
        i++;
    }
    
    char *name = path + si + p;
    if(name[0] == 0) {
        name = path + osi + p;
        if(name[0] == 0) {
            return path;
        }
    }
    
    return name;
}

char* ParentPath(char *path) {
    char *name = FileName(path);
    size_t namelen = strlen(name);
    size_t pathlen = strlen(path);
    size_t parentlen = pathlen - namelen;
    if(parentlen == 0) {
        parentlen++;
    }
    char *parent = NEditMalloc(parentlen + 1);
    memcpy(parent, path, parentlen);
    parent[parentlen] = '\0';
    return parent;
}

/* -------------------- path bar -------------------- */

typedef void(*updatedir_callback)(void*,char*);

typedef struct PathBar {  
    Widget widget;
    Widget textfield;
    
    Widget left;
    Widget right;
    Dimension lw;
    Dimension rw;
    
    int shift;
    
    Widget *pathSegments;
    size_t numSegments;
    size_t segmentAlloc;
    
    char *path;
    int selection;
    Boolean input;
    
    updatedir_callback updateDir;
    void *updateDirData;
} PathBar;

void PathBarSetPath(PathBar *bar, char *path);

void pathbar_resize(Widget w, PathBar *p, XtPointer d)
{
    Dimension width, height;
    XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
    
    Dimension *segW = NEditCalloc(p->numSegments, sizeof(Dimension));
    
    Dimension maxHeight = 0;
    
    /* get width/height from all widgets */
    Dimension pathWidth = 0;
    for(int i=0;i<p->numSegments;i++) {
        Dimension segWidth;
        Dimension segHeight;
        XtVaGetValues(p->pathSegments[i], XmNwidth, &segWidth, XmNheight, &segHeight, NULL);
        segW[i] = segWidth;
        pathWidth += segWidth;
        if(segHeight > maxHeight) {
            maxHeight = segHeight;
        }
    }
    Dimension tfHeight;
    XtVaGetValues(p->textfield, XmNheight, &tfHeight, NULL);
    if(tfHeight > maxHeight) {
        maxHeight = tfHeight;
    }
    
    Boolean arrows = False;
    if(pathWidth + 10 > width) {
        arrows = True;
        pathWidth += p->lw + p->rw;
    }
    
    /* calc max visible widgets */
    int start = 0;
    if(arrows) {
        Dimension vis = p->lw+p->rw;
        for(int i=p->numSegments;i>0;i--) {
            Dimension segWidth = segW[i-1];
            if(vis + segWidth + 10 > width) {
                start = i;
                arrows = True;
                break;
            }
            vis += segWidth;
        }
    } else {
        p->shift = 0;
    }
    
    int leftShift = 0;
    if(p->shift < 0) {
        if(start + p->shift < 0) {
            leftShift = start;
            start = 0;
            p->shift = -leftShift;
        } else {
            leftShift = -p->shift; /* negative shift */
            start += p->shift;
        }
    }
    
    int x = 0;
    if(arrows) {
        XtManageChild(p->left);
        XtManageChild(p->right);
        x = p->lw;
    } else {
        XtUnmanageChild(p->left);
        XtUnmanageChild(p->right);
    }
    
    for(int i=0;i<p->numSegments;i++) {
        if(i >= start && i < p->numSegments - leftShift && !p->input) {
            XtVaSetValues(p->pathSegments[i], XmNx, x, XmNy, 0, XmNheight, maxHeight, NULL);
            x += segW[i];
            XtManageChild(p->pathSegments[i]);
        } else {
            XtUnmanageChild(p->pathSegments[i]);
        }
    }
    
    if(arrows) {
        XtVaSetValues(p->left, XmNx, 0, XmNy, 0, XmNheight, maxHeight, NULL);
        XtVaSetValues(p->right, XmNx, x, XmNy, 0, XmNheight, maxHeight, NULL);
    }
    
    NEditFree(segW);
    
    Dimension rw, rh;
    XtMakeResizeRequest(w, width, maxHeight, &rw, &rh);
    
    XtVaSetValues(p->textfield, XmNwidth, rw, XmNheight, rh, NULL);
}

void pathbar_input(Widget w, PathBar *p, XtPointer c)
{
    XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct*)c;
    XEvent *xevent = cbs->event;
    
    if (cbs->reason == XmCR_INPUT) {
        if (xevent->xany.type == ButtonPress) {
            XtUnmanageChild(p->left);
            XtUnmanageChild(p->right);
            
            XtManageChild(p->textfield);
            p->input = 1;
            
            XmProcessTraversal(p->textfield, XmTRAVERSE_CURRENT);
            
            pathbar_resize(p->widget, p, NULL);
        }
    }
}

void pathbar_losingfocus(Widget w, PathBar *p, XtPointer c)
{
    p->input = False;
    XtUnmanageChild(p->textfield);
}

void pathbar_pathinput(Widget w, PathBar *p, XtPointer d)
{
    char *newpath = XmTextFieldGetString(p->textfield);
    if(newpath) {
        if(newpath[0] == '~') {
            char *p = newpath+1;
            char *cp = ConcatPath(GetHomeDir(), p);
            XtFree(newpath);
            newpath = cp;
        } else if(newpath[0] != '/') {
            char *cp = ConcatPath(GetCurrentDir(), newpath);
            XtFree(newpath);
            newpath = cp;
        }
        
        /* update path */
        PathBarSetPath(p, newpath);
        if(p->updateDir) {
            p->updateDir(p->updateDirData, newpath);
        }
        XtFree(newpath);
        
        /* hide textfield and show path as buttons */
        XtUnmanageChild(p->textfield);
        pathbar_resize(p->widget, p, NULL);
    }
}

void pathbar_shift_left(Widget w, PathBar *p, XtPointer d)
{
    p->shift--;
    pathbar_resize(p->widget, p, NULL);
}

void pathbar_shift_right(Widget w, PathBar *p, XtPointer d)
{
    if(p->shift < 0) {
        p->shift++;
    }
    pathbar_resize(p->widget, p, NULL);
}

PathBar* CreatePathBar(Widget parent, ArgList args, int n)
{
    PathBar *bar = NEditMalloc(sizeof(PathBar));
    bar->path = NULL;
    bar->updateDir = NULL;
    bar->updateDirData = NULL;
    
    bar->shift = 0;
    
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    bar->widget = XmCreateDrawingArea(parent, "pathbar", args, n);
    XtAddCallback(
            bar->widget,
            XmNresizeCallback,
            (XtCallbackProc)pathbar_resize,
            bar);
    XtAddCallback(
            bar->widget,
            XmNinputCallback,
            (XtCallbackProc)pathbar_input,
            bar);
    
    Arg a[4];
    XtSetArg(a[0], XmNshadowThickness, 0);
    XtSetArg(a[1], XmNx, 0);
    XtSetArg(a[2], XmNy, 0);
    bar->textfield = XmCreateTextField(bar->widget, "pbtext", a, 3);
    bar->input = 0;
    XtAddCallback(
            bar->textfield,
            XmNlosingFocusCallback,
            (XtCallbackProc)pathbar_losingfocus,
            bar);
    XtAddCallback(bar->textfield, XmNactivateCallback,
                 (XtCallbackProc)pathbar_pathinput, bar);
    
    XtSetArg(a[0], XmNarrowDirection, XmARROW_LEFT);
    bar->left = XmCreateArrowButton(bar->widget, "pbbutton", a, 1);
    XtSetArg(a[0], XmNarrowDirection, XmARROW_RIGHT);
    bar->right = XmCreateArrowButton(bar->widget, "pbbutton", a, 1);
    XtAddCallback(
                bar->left,
                XmNactivateCallback,
                (XtCallbackProc)pathbar_shift_left,
                bar);
    XtAddCallback(
                bar->right,
                XmNactivateCallback,
                (XtCallbackProc)pathbar_shift_right,
                bar);
    
    Pixel bg;
    XtVaGetValues(bar->textfield, XmNbackground, &bg, NULL);
    XtVaSetValues(bar->widget, XmNbackground, bg, NULL);
    
    XtManageChild(bar->left);
    XtManageChild(bar->right);
    
    XtVaGetValues(bar->left, XmNwidth, &bar->lw, NULL);
    XtVaGetValues(bar->right, XmNwidth, &bar->rw, NULL);
    
    bar->segmentAlloc = 16;
    bar->numSegments = 0;
    bar->pathSegments = calloc(16, sizeof(Widget));
    
    bar->selection = 0;
    
    return bar;
}

void PathBarChangeDir(Widget w, PathBar *bar, XtPointer c)
{
    XmToggleButtonSetState(bar->pathSegments[bar->selection], False, False);
    
    for(int i=0;i<bar->numSegments;i++) {  
        if(bar->pathSegments[i] == w) {
            bar->selection = i;
            XmToggleButtonSetState(w, True, False);
            break;
        }
    }
    
    int plen = strlen(bar->path);
    int countSeg = 0;
    for(int i=0;i<=plen;i++) {
        char c = bar->path[i];
        if(c == '/' || c == '\0') {
            if(countSeg == bar->selection) {
                char *dir = NEditMalloc(i+2);
                memcpy(dir, bar->path, i+1);
                dir[i+1] = '\0';
                if(bar->updateDir) {
                    bar->updateDir(bar->updateDirData, dir);
                }
                NEditFree(dir);
            }
            countSeg++;
        }
    }
}

void PathBarSetPath(PathBar *bar, char *path)
{
    if(bar->path) {
        NEditFree(bar->path);
    }
    bar->path = NEditStrdup(path);
    
    for(int i=0;i<bar->numSegments;i++) {
        XtDestroyWidget(bar->pathSegments[i]);
    }
    XtUnmanageChild(bar->textfield);
    XtManageChild(bar->left);
    XtManageChild(bar->right);
    bar->input = False;
    
    Arg args[4];
    int n;
    XmString str;
    
    bar->numSegments = 0;
    
    int i=0;
    if(path[0] == '/') {
        n = 0;
        str = XmStringCreateLocalized("/");
        XtSetArg(args[0], XmNlabelString, str);
        XtSetArg(args[1], XmNfillOnSelect, True);
        XtSetArg(args[2], XmNindicatorOn, False);
        bar->pathSegments[0] = XmCreateToggleButton(
                bar->widget, "pbbutton", args, 3);
        XtAddCallback(
                bar->pathSegments[0],
                XmNvalueChangedCallback,
                (XtCallbackProc)PathBarChangeDir,
                bar);
        XmStringFree(str);
        bar->numSegments++;
        i++;
    }
    
    int len = strlen(path);
    int begin = i;
    for(;i<=len;i++) {
        char c = path[i];
        if(c == '/' || c == '\0' && i > begin+1) {
            char *segStr = NEditMalloc(i - begin + 1);
            memcpy(segStr, path+begin, i-begin);
            segStr[i-begin] = '\0';
            begin = i+1;
            
            str = XmStringCreateLocalized(segStr);
            NEditFree(segStr);
            XtSetArg(args[0], XmNlabelString, str);
            XtSetArg(args[1], XmNfillOnSelect, True);
            XtSetArg(args[2], XmNindicatorOn, False);
            Widget button = XmCreateToggleButton(bar->widget, "pbbutton", args, 3);
            XtAddCallback(
                    button,
                    XmNvalueChangedCallback,
                    (XtCallbackProc)PathBarChangeDir,
                    bar);
            XmStringFree(str);
            
            if(bar->numSegments >= bar->segmentAlloc) {
                bar->segmentAlloc += 8;
                bar->pathSegments = realloc(bar->pathSegments, bar->segmentAlloc * sizeof(Widget));
            }
            
            bar->pathSegments[bar->numSegments++] = button;
        }
    }
    
    bar->selection = bar->numSegments-1;
    XmToggleButtonSetState(bar->pathSegments[bar->selection], True, False);
    
    XmTextFieldSetString(bar->textfield, path);
    XmTextFieldSetInsertionPosition(bar->textfield, XmTextFieldGetLastPosition(bar->textfield));
    
    pathbar_resize(bar->widget, bar, NULL);
}




/* -------------------- file dialog -------------------- */

typedef struct FileElm FileElm;
struct FileElm {
    char *path;
    int isDirectory;
};

typedef struct FileDialogData {
    Widget shell;
    
    Widget path;
    PathBar *pathBar;
    Widget filter;
    
    int selectedview;
    
    // icon view
    Widget scrollw;
    Widget container;
    
    // dir/file list view
    Widget listform;
    Widget dirlist;
    Widget filelist;
    
    // detail view
    Widget grid;
    
    Widget name;
    Widget wrap;
    Widget unixFormat;
    Widget dosFormat;
    Widget macFormat;
    Widget encoding;
    Widget bom;
    Widget xattr;
    
    FileElm *dirs;
    FileElm *files;
    int dircount;
    int filecount;
    int maxnamelen;
    
    WidgetList gadgets;
    int numGadgets;
    
    char *currentPath;
    char *selectedPath;
    int selIsDir;
    int showHidden;
      
    int type;
    
    int end;
    int status;
} FileDialogData;

static void filedialog_cancel(Widget w, FileDialogData *data, XtPointer d)
{
    data->end = 1;
    data->status = FILEDIALOG_CANCEL;
}

static int cleanupFileView(FileDialogData *data)
{
    if(!data->gadgets) {
        return 0;
    }
    
    XtUnmanageChildren(data->gadgets, data->numGadgets);
    for(int i=0;i<data->numGadgets;i++) {
        XtDestroyWidget(data->gadgets[i]);
    }
    
    int ret = data->numGadgets;
    NEditFree(data->gadgets);
    data->gadgets = NULL;
    data->numGadgets = 0;
    return ret;
}

static void cleanupLists(FileDialogData *data)
{
    XmListDeleteAllItems(data->dirlist);
    XmListDeleteAllItems(data->filelist);
}

static void filedialog_cleanup(FileDialogData *data)
{
    cleanupFileView(data);
    if(data->selectedPath) {
        NEditFree(data->selectedPath);
    }
    if(data->currentPath) {
        NEditFree(data->currentPath);
    }
}

static int filecmp(const void *f1, const void *f2)
{
    const FileElm *file1 = f1;
    const FileElm *file2 = f2;
    if(file1->isDirectory != file2->isDirectory) {
        return file1->isDirectory < file2->isDirectory;
    }
    
    return strcmp(FileName(file1->path), FileName(file2->path));
}

static void resize_container(Widget w, FileDialogData *data, XtPointer d) 
{
    Dimension width, height;
    Dimension cw, ch;
    
    XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);
    XtVaGetValues(data->container, XtNwidth, &cw, XtNheight, &ch, NULL);
    
    if(ch < height) {
        XtVaSetValues(data->container, XtNwidth, width, XtNheight, height, NULL);
    } else {
        XtVaSetValues(data->container, XtNwidth, width, NULL);
    }
}

static void init_container_size(FileDialogData *data)
{
    Widget parent = XtParent(data->container);
    Dimension width;
    
    XtVaGetValues(parent, XtNwidth, &width, NULL);
    XtVaSetValues(data->container, XtNwidth, width, XtNheight, 100, NULL);
}

typedef void(*ViewUpdateFunc)(FileDialogData*,FileElm*,FileElm*,int,int,int);

static void filedialog_update_iconview(
        FileDialogData *data,
        FileElm *dirs,
        FileElm *files,
        int dircount,
        int filecount,
        int maxnamelen)
{
    Arg args[16];
    XmString str;
    int n;
    WidgetList gadgets = NEditCalloc(dircount+filecount, sizeof(Widget));
    
    // TODO: better width calculation
    // FIXME: for some reason setting XmNlargeCellWidth on Solaris doesn't work
#ifndef __sun
    Dimension cellwidth = maxnamelen * 8;
    XtVaSetValues(data->container, XmNlargeCellWidth, cellwidth, NULL);
#endif
    
    char *filter = XmTextFieldGetString(data->filter);
    char *filterStr = filter;
    if(!filter || strlen(filter) == 0) {
        filterStr = "*";
    }
    
    int numgadgets = 0;
    FileElm *ls = dirs;
    int count = dircount;
    int pos = 0;
    for(int i=0;i<2;i++) {
        for(int j=0;j<count;j++) {
            FileElm *e = &ls[j];
            
            char *name = FileName(e->path);
            if((!data->showHidden && name[0] == '.') || (!e->isDirectory && fnmatch(filterStr, name, 0))) {
                continue;
            }
            
            n = 0;
            str = XmStringCreateLocalized(name);
            XtSetArg(args[n], XmNuserData, e); n++;
            XtSetArg(args[n], XmNlabelString, str); n++;
            XtSetArg(args[n], XmNshadowThickness, 0); n++;
            XtSetArg(args[n], XmNpositionIndex, pos); n++;
            XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
            if(e->isDirectory) {
                XtSetArg(args[n], XmNlargeIconPixmap, folderIcon); n++;
                XtSetArg(args[n], XmNlargeIconMask, folderShape); n++;
            } else {
                XtSetArg(args[n], XmNlargeIconPixmap, fileIcon); n++;
                XtSetArg(args[n], XmNlargeIconMask, fileShape); n++;
            }
            Widget item = XmCreateIconGadget(data->container, "table", args, n);
            XtManageChild(item);
            numgadgets++;

            gadgets[pos] = item;
            XmStringFree(str);
            pos++;
        }
        ls = files;
        count = filecount;
    }
    
    if(filter) {
        XtFree(filter);
    }
    
    data->gadgets = gadgets;
    data->numGadgets = numgadgets;
    
    //XmContainerRelayout(data->container);   
    resize_container(XtParent(data->container), data, NULL);
}

static void filelistwidget_add(Widget w, int showHidden, char *filter, FileElm *ls, int count)
{   
    if(count > 0) {
        XmStringTable items = NEditCalloc(count, sizeof(XmString));
        int i = 0;
        
        for(int j=0;j<count;j++) {
            FileElm *e = &ls[j];
            
            char *name = FileName(e->path);
            if((!showHidden && name[0] == '.') || fnmatch(filter, name, 0)) {
                continue;
            }
            
            items[i] = XmStringCreateLocalized(name);
            i++;
        }
        XmListAddItems(w, items, i, 0);
        for(int i=0;i<count;i++) {
            XmStringFree(items[i]);
        }
        NEditFree(items);
    }
}


static void filedialog_update_lists(
        FileDialogData *data,
        FileElm *dirs,
        FileElm *files,
        int dircount,
        int filecount,
        int maxnamelen)
{
    char *filter = XmTextFieldGetString(data->filter);
    char *filterStr = filter;
    if(!filter || strlen(filter) == 0) {
        filterStr = "*";
    }
    
    filelistwidget_add(data->dirlist, data->showHidden, "*", dirs, dircount);
    filelistwidget_add(data->filelist, data->showHidden, filterStr, files, filecount);
    
    if(filter) {
        XtFree(filter);
    }
}

static void filedialog_update_grid(
        FileDialogData *data,
        FileElm *dirs,
        FileElm *files,
        int dircount,
        int filecount,
        int maxnamelen)
{
    int row = 0;
    
    char *filter = XmTextFieldGetString(data->filter);
    char *filterStr = filter;
    if(!filter || strlen(filter) == 0) {
        filterStr = "*";
    }
    
    XmLGridAddRows(data->grid, XmCONTENT, 1, dircount+filecount);
    FileElm *ls = dirs;
    int count = dircount;
    for(int i=0;i<2;i++) {
        for(int j=0;j<count;j++) {
            FileElm *e = &ls[j];
            
            char *name = FileName(e->path);
            XmString str = XmStringCreateLocalized(name);

            XtVaSetValues(data->grid,
                    XmNcolumn, 0, 
                    XmNrow, row,
                    XmNcellString, str, NULL);
            XmStringFree(str);

            row++;
        }
        ls = files;
        count = filecount;
    }
    
    if(filter) {
        XtFree(filter);
    }
}

static void cleanupGrid(FileDialogData *data)
{
    Cardinal rows = 0;
    XtVaGetValues(data->grid, XmNrows, &rows, NULL);
    XmLGridDeleteRows(data->grid, XmCONTENT, 0, rows);
}

static void free_files(FileElm *ls, int count)
{
    for(int i=0;i<count;i++) {
        if(ls[i].path) {
            free(ls[i].path);
        }
    }
    free(ls);
}

static void filedialog_cleanup_filedata(FileDialogData *data)
{
    free_files(data->dirs, data->dircount);
    free_files(data->files, data->filecount);
    data->dirs = NULL;
    data->files = NULL;
    data->dircount = 0;
    data->filecount = 0;
    data->maxnamelen = 0;
}

#define FILEDIALOG_FALLBACK_PATH "/"

#define FILE_ARRAY_SIZE 1024

void file_array_add(FileElm **files, int *alloc, int *count, FileElm elm) {
    int c = *count;
    int a = *alloc;
    if(c >= a) {
        a *= 2;
        FileElm *newarray = realloc(*files, sizeof(FileElm) * a);
        
        *files = newarray;
        *alloc = a;
    }
    
    (*files)[c] = elm;
    c++;
    *count = c;
}

static void filedialog_update_dir(FileDialogData *data, char *path)
{
    Arg args[16];
    int n;
    XmString str;
    
    ViewUpdateFunc update_view = NULL;
    switch(data->selectedview) {
        case 0: {
            if(cleanupFileView(data)) {
                init_container_size(data);
            }
            update_view = filedialog_update_iconview;
            break;
        }
        case 1: {
            cleanupLists(data);
            update_view = filedialog_update_lists;
            break;
        }
        case 2: {
            cleanupGrid(data);
            update_view = filedialog_update_grid;
            break;
        }
    }
    
    /* read dir and insert items */
    if(path) {
        FileElm *dirs = calloc(sizeof(FileElm), FILE_ARRAY_SIZE);
        FileElm *files = calloc(sizeof(FileElm), FILE_ARRAY_SIZE);
        int dirs_alloc = FILE_ARRAY_SIZE;
        int files_alloc = FILE_ARRAY_SIZE;
        
        int dircount = 0; 
        int filecount = 0;
        size_t maxNameLen = 0;
        DIR *dir = opendir(path);
        if(!dir) {
            if(path == FILEDIALOG_FALLBACK_PATH) {
                // TODO: ERROR
                fprintf(stderr, "Cannot open directory: %s\n", path);
                perror("opendir");
            } else {
                filedialog_update_dir(data, FILEDIALOG_FALLBACK_PATH);
            }
            return;
        }
        filedialog_cleanup_filedata(data);
    
        /* dir reading complete - set the path textfield */  
        XmTextFieldSetString(data->path, path);
        char *oldPath = data->currentPath;
        data->currentPath = NEditStrdup(path);
        if(oldPath) {
            NEditFree(oldPath);
        }
        path = data->currentPath;

        struct dirent *ent;
        while((ent = readdir(dir)) != NULL) {
            if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
                continue;
            }

            char *entpath = ConcatPath(path, ent->d_name);

            struct stat s;
            if(stat(entpath, &s)) {
                NEditFree(entpath);
                continue;
            }

            FileElm new_entry;
            new_entry.path = entpath;
            new_entry.isDirectory = S_ISDIR(s.st_mode);

            size_t nameLen = strlen(ent->d_name);
            if(nameLen > maxNameLen) {
                maxNameLen = nameLen;
            }

            if(new_entry.isDirectory) {
                file_array_add(&dirs, &dirs_alloc, &dircount, new_entry);
            } else {
                file_array_add(&files, &files_alloc, &filecount, new_entry);
            }
        }
        closedir(dir);
        
        data->dirs = dirs;
        data->files = files;
        data->dircount = dircount;
        data->filecount = filecount;
        data->maxnamelen = maxNameLen;
        
        // sort file arrays
        qsort(dirs, dircount, sizeof(FileElm), filecmp);
        qsort(files, filecount, sizeof(FileElm), filecmp);
    }
    
    update_view(data, data->dirs, data->files,
            data->dircount, data->filecount, data->maxnamelen);
}

static void filedialog_goup(Widget w, FileDialogData *data, XtPointer d)
{
    char *newPath = ParentPath(data->currentPath);
    filedialog_update_dir(data, newPath);
    PathBarSetPath(data->pathBar, newPath);
    NEditFree(newPath);
}

static void filedialog_setselection(
        FileDialogData *data,
        XmContainerSelectCallbackStruct *sel)
{
    if(sel->selected_item_count > 0) {
        FileElm *file = NULL;
        XtVaGetValues(sel->selected_items[0], XmNuserData, &file, NULL);
        if(file) {
            if(data->selectedPath) {
                NEditFree(data->selectedPath);
            }
            data->selectedPath = NEditStrdup(file->path);
            data->selIsDir = file->isDirectory;
            
            if(!file->isDirectory) {
                if(data->name) {
                    XmTextFieldSetString(data->name, FileName(file->path));
                }
            }
        }
    }
}

static void filedialog_select(
        Widget w,
        FileDialogData *data,
        XmContainerSelectCallbackStruct *sel)
{
    filedialog_setselection(data, sel);
}

static void filedialog_action(
        Widget w,
        FileDialogData *data,
        XmContainerSelectCallbackStruct *sel)
{
    filedialog_setselection(data, sel);
    
    if(data->selIsDir) {
        filedialog_update_dir(data, data->selectedPath);
        PathBarSetPath(data->pathBar, data->selectedPath);
    } else {
        data->end = True;
        data->status = FILEDIALOG_OK;
    }
}

char* set_selected_path(FileDialogData *data, XmString item)
{
    char *name = NULL;
    XmStringGetLtoR(item, XmFONTLIST_DEFAULT_TAG, &name);
    if(!name) {
        return NULL;
    }
    char *path = ConcatPath(data->currentPath, name);
    XtFree(name);
    
    if(data->selectedPath) {
        NEditFree(data->selectedPath);
    }
    data->selectedPath = path;
    
    return path;
}

void dirlist_activate(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    char *path = set_selected_path(data, cb->item);
    if(path) {
        filedialog_update_dir(data, path);
        PathBarSetPath(data->pathBar, path);
        data->selIsDir = TRUE;
    }    
}

void dirlist_select(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    char *path = set_selected_path(data, cb->item);
    if(path) {
        data->selIsDir = TRUE;
    }
}

void filelist_activate(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    char *path = set_selected_path(data, cb->item);
    if(path) {
        data->end = True;
        data->status = FILEDIALOG_OK;
        data->selIsDir = False;
    }
}

void filelist_select(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    if(data->type == FILEDIALOG_SAVE) {
        char *name = NULL;
        XmStringGetLtoR(cb->item, XmFONTLIST_DEFAULT_TAG, &name);
        XmTextFieldSetString(data->name, name);
        XtFree(name);
    } else {
        char *path = set_selected_path(data, cb->item);
        if(path) {
            data->selIsDir = False;
        }
    }
}
 
static void filedialog_setshowhidden(
        Widget w,
        FileDialogData *data,
        XmToggleButtonCallbackStruct *tb)
{
    data->showHidden = tb->set;
    filedialog_update_dir(data, NULL);
}

static void filedialog_ok(Widget w, FileDialogData *data, XtPointer d)
{
    if(data->selectedPath) {
        if(!data->selIsDir) {
            data->status = FILEDIALOG_OK;
            data->end = True;
            return;
        }
    }
    
    if(data->type == FILEDIALOG_SAVE) {
        char *newName = XmTextFieldGetString(data->name);
        if(newName) {
            if(strlen(newName) > 0) {
                data->selectedPath = ConcatPath(data->currentPath, newName);
                data->status = FILEDIALOG_OK;
                data->end = True;
                data->selIsDir = 0;
            }
            XtFree(newName);
        }
    }
}

#define DETECT_ENCODING "detect"
static char *default_encodings[] = {
    DETECT_ENCODING,
    "UTF-8",
    "UTF-16",
    "UTF-16BE",
    "UTF-16LE",
    "UTF-32",
    "UTF-32BE",
    "UTF-32LE",
    "ISO8859-1",
    "ISO8859-2",
    "ISO8859-3",
    "ISO8859-4",
    "ISO8859-5",
    "ISO8859-6",
    "ISO8859-7",
    "ISO8859-8",
    "ISO8859-9",
    "ISO8859-10",
    "ISO8859-13",
    "ISO8859-14",
    "ISO8859-15",
    "ISO8859-16",
    NULL
};

static void adjust_enc_settings(FileDialogData *data){
    /* this should never happen, but make sure it will not do anything */
    if(data->type != FILEDIALOG_SAVE) return;
    
    int encPos;
    XtVaGetValues(data->encoding, XmNselectedPosition, &encPos, NULL);
    
    /*
     * the save file dialog doesn't has the "detect" item
     * the default_encodings index is encPos + 1
     */
    if(encPos > 6) {
        /* no unicode no bom */
        XtSetSensitive(data->bom, False);
        if(GetAutoEnableXattr()) {
            XtVaSetValues(data->xattr, XmNset, 1, NULL);
        }
    } else {
        XtSetSensitive(data->bom, True);
        if(encPos > 0) {
            /* enable bom for all non-UTF-8 unicode encodings */
            XtVaSetValues(data->bom, XmNset, 1, NULL);
        }
    }
}

static void filedialog_select_encoding(
        Widget w,
        FileDialogData *data,
        XmComboBoxCallbackStruct *cb)
{
    if(cb->reason == XmCR_SELECT) {
        adjust_enc_settings(data);
    }
}

static void filedialog_filter(Widget w, FileDialogData *data, XtPointer c)
{
    filedialog_update_dir(data, NULL);
}

static void unselect_view(FileDialogData *data)
{
    switch(data->selectedview) {
        case 0: {
            XtUnmanageChild(data->scrollw);
            cleanupFileView(data);
            break;
        }
        case 1: {
            XtUnmanageChild(data->listform);
            cleanupLists(data);
            break;
        }
        case 2: {
            // TODO
            break;
        }
    }
}

static void select_iconview(Widget w, FileDialogData *data, XtPointer u)
{
    unselect_view(data);
    data->selectedview = 0;
    XtManageChild(data->scrollw);
    filedialog_update_dir(data, NULL);
    filedialog_update_dir(data, NULL); // workaround for what I do not know
}

static void select_listview(Widget w, FileDialogData *data, XtPointer u)
{
    unselect_view(data);
    data->selectedview = 1;
    XtManageChild(data->listform);
    filedialog_update_dir(data, NULL);
}

static void select_detailview(Widget w, FileDialogData *data, XtPointer u)
{
    unselect_view(data);
    data->selectedview = 2;
    XtManageChild(data->grid);
    filedialog_update_dir(data, NULL);
}

static void new_folder(Widget w, FileDialogData *data, XtPointer u)
{
    char fileName[DF_MAX_PROMPT_LENGTH];
    
    int response = DialogF(
            DF_PROMPT,
            data->shell,
            2,
            "Create new directory", "Directory name:",
            fileName,
            "OK",
            "Cancel");
    
    if(response == 2) {
        return;
    }
    
    char *newFolder = ConcatPath(data->currentPath ? data->currentPath : "", fileName);
    if(mkdir(newFolder, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
        DialogF(
                DF_ERR,
                data->shell,
                1,
                "Error creating Directory",
                "Can't create %s:\n%s", "OK",
                newFolder,
                strerror(errno));
    } else {
        char *p = strdup(data->currentPath);
        filedialog_update_dir(data, p);
        free(p);
    }
    free(newFolder);
}

int FileDialog(Widget parent, char *promptString, FileSelection *file, int type)
{
    Arg args[32];
    int n = 0;
    XmString str;
    
    int currentEncItem = 0;
    
    if(!pixmaps_initialized) {
        initPixmaps(XtDisplay(parent), XtWindow(parent));
    }
    
    if(LastView == -1) {
        LastView = GetFsbView();
        if(LastView < 0 || LastView > 1) {
            LastView = 1;
        }
    }
    Boolean showHiddenValue = GetFsbShowHidden();
    
    FileDialogData data;
    memset(&data, 0, sizeof(FileDialogData));
    data.type = type;
    
    file->addwrap = FALSE;
    file->setxattr = FALSE;
    
    Widget dialog = CreateDialogShell(parent, promptString, args, 0);
    AddMotifCloseCallback(dialog, (XtCallbackProc)filedialog_cancel, &data);
    data.shell = dialog;
    
    n = 0;
    XtSetArg(args[n],  XmNautoUnmanage, False); n++;
    Widget form = XmCreateForm(dialog, "form", args, n);
    
    /* upper part of the gui */
     
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNresizable, True); n++;
    XtSetArg(args[n], XmNarrowDirection, XmARROW_UP); n++;
    Widget goUp = XmCreateArrowButton(form, "button", args, n);
    //XtManageChild(goUp);
    XtAddCallback(goUp, XmNactivateCallback,
                 (XtCallbackProc)filedialog_goup, &data);
    
    // View Option Menu
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    Widget viewframe = XmCreateForm(form, "vframe", args, n);
    XtManageChild(viewframe);
    
    XmString v0 = XmStringCreateLocalized("Icons");
    XmString v1 = XmStringCreateLocalized("List");
    XmString v2 = XmStringCreateLocalized("Detail");
    
    Widget menu = XmCreatePulldownMenu(viewframe, "menu", NULL, 0);
    
    XtSetArg(args[0], XmNlabelString, v0);
    XtSetArg(args[1], XmNpositionIndex, LastView == 0 ? 0 : 1);
    Widget mitem0 = XmCreatePushButton(menu, "menuitem", args, 2);
    XtSetArg(args[0], XmNlabelString, v1);
    XtSetArg(args[1], XmNpositionIndex, LastView == 1 ? 0 : 1);
    Widget mitem1 = XmCreatePushButton(menu, "menuitem", args, 2);
    XtSetArg(args[0], XmNlabelString, v2);
    XtSetArg(args[1], XmNpositionIndex, LastView == 2 ? 0 : 2);
    Widget mitem2 = XmCreatePushButton(menu, "menuitem", args, 2);
    XtManageChild(mitem0);
    XtManageChild(mitem1);
    //XtManageChild(mitem2);
    XmStringFree(v0);
    XmStringFree(v1);
    XmStringFree(v2);
    XtAddCallback(
            mitem0,
            XmNactivateCallback,
            (XtCallbackProc)select_iconview,
            &data);
    XtAddCallback(
            mitem1,
            XmNactivateCallback,
            (XtCallbackProc)select_listview,
            &data);
    XtAddCallback(
            mitem2,
            XmNactivateCallback,
            (XtCallbackProc)select_detailview,
            &data);
    
    n = 0;
    XtSetArg(args[n], XmNsubMenuId, menu); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    Widget view = XmCreateOptionMenu(viewframe, "option_menu", args, n);
    XtManageChild(view);
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightWidget, view); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    Widget newFolder = XmCreatePushButton(viewframe, "newFolder", args, n);
    XtManageChild(newFolder);
    XtAddCallback(
            newFolder,
            XmNactivateCallback,
            (XtCallbackProc)new_folder,
            &data);
    
    // pathbar
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, goUp); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, viewframe); n++;
    XtSetArg(args[n], XmNrightOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
    Widget pathBarFrame = XmCreateFrame(form, "pathbar_frame", args, n);
    XtManageChild(pathBarFrame);
    data.pathBar = CreatePathBar(pathBarFrame, args, 0);
    data.pathBar->updateDir = (updatedir_callback)filedialog_update_dir;
    data.pathBar->updateDirData = &data;
    XtManageChild(data.pathBar->widget);
    data.path = XmCreateTextField(form, "textfield", args, 0);
    
    n = 0;
    str = XmStringCreateLocalized("Filter");
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, pathBarFrame); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget filterLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(filterLabel);
    XmStringFree(str);
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, filterLabel); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    Widget filterform = XmCreateForm(form, "filterform", args, n);
    XtManageChild(filterform);
    
    n = 0;
    str = XmStringCreateSimple("Show hidden files");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNset, showHiddenValue); n++;
    Widget showHidden = XmCreateToggleButton(filterform, "showHidden", args, n);
    XtManageChild(showHidden);
    XmStringFree(str);
    XtAddCallback(showHidden, XmNvalueChangedCallback,
                 (XtCallbackProc)filedialog_setshowhidden, &data);
    data.showHidden = showHiddenValue;
    
    n = 0;
    str = XmStringCreateLocalized("Filter");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, showHidden); n++;
    Widget filterButton = XmCreatePushButton(filterform, "filedialog_filter", args, n);
    XtManageChild(filterButton);
    XmStringFree(str);
    XtAddCallback(filterButton, XmNactivateCallback,
                 (XtCallbackProc)filedialog_filter, &data);
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, filterButton); n++;
    XtSetArg(args[n], XmNrightOffset, WIDGET_SPACING); n++;
    data.filter = XmCreateTextField(filterform, "filedialog_filter_textfield", args, n);
    XtManageChild(data.filter);
    XmTextFieldSetString(data.filter, "*");
    XtAddCallback(data.filter, XmNactivateCallback,
                 (XtCallbackProc)filedialog_filter, &data);
    
    /* lower part */
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING * 2); n++;
    Widget buttons = XmCreateForm(form, "buttons", args, n);
    XtManageChild(buttons);
    
    n = 0;
    str = XmStringCreateLocalized(type == FILEDIALOG_OPEN ? "Open" : "Save");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    Widget okBtn = XmCreatePushButton(buttons, "filedialog_open", args, n);
    XtManageChild(okBtn);
    XmStringFree(str);
    XtAddCallback(okBtn, XmNactivateCallback,
                 (XtCallbackProc)filedialog_ok, &data);
    
    n = 0;
    str = XmStringCreateLocalized("Cancel");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    Widget cancelBtn = XmCreatePushButton(buttons, "filedialog_cancel", args, n);
    XtManageChild(cancelBtn);
    XmStringFree(str);
    XtAddCallback(cancelBtn, XmNactivateCallback,
                 (XtCallbackProc)filedialog_cancel, &data);
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, buttons); n++;
    XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, 1); n++;
    Widget separator = XmCreateSeparator(form, "ofd_separator", args, n);
    XtManageChild(separator);
    
    Widget bottomWidget = separator;
    
    if(type == FILEDIALOG_SAVE) {
        n = 0;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNbottomWidget, separator); n++;
        XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
        data.name = XmCreateTextField(form, "textfield", args, n);
        XtManageChild(data.name);
        XtAddCallback(data.name, XmNactivateCallback,
                 (XtCallbackProc)filedialog_ok, &data);

        n = 0;
        str = XmStringCreateSimple("New File Name");
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNbottomWidget, data.name); n++;
        XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
        XtSetArg(args[n], XmNlabelString, str); n++;
        Widget nameLabel = XmCreateLabel(form, "label", args, n);
        XtManageChild(nameLabel);

        n = 0;
        str = XmStringCreateSimple("Add line breaks where wrapped");
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNbottomWidget, nameLabel); n++;
        XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
        XtSetArg(args[n], XmNmnemonic, 'A'); n++;
        XtSetArg(args[n], XmNlabelString, str); n++;
        data.wrap = XmCreateToggleButton(form, "addWrap", args, n);
        XtManageChild(data.wrap);
        XmStringFree(str);

        Widget formatBtns = CreateFormatButtons(
                form,
                data.wrap,
                file->format,
                &data.unixFormat,
                &data.dosFormat,
                &data.macFormat);
        
        bottomWidget = formatBtns;
    }
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, bottomWidget); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    Widget enc = XmCreateRowColumn(form, "enc", args, n);
    XtManageChild(enc);
    
    if(file->setenc) {
        n = 0;
        str = XmStringCreateSimple("Encoding:");
        XtSetArg(args[n], XmNlabelString, str); n++;
        Widget encLabel = XmCreateLabel(enc, "label", args, n);
        XtManageChild(encLabel);
        XmStringFree(str);

        n = 0;
        int arraylen = 22;
        char *encStr;
        XmStringTable encodings = NEditCalloc(arraylen, sizeof(XmString));
        /* skip the "detect" item on type == save */
        int skip = type == FILEDIALOG_OPEN ? 0 : 1;
        char *defEncoding = type == FILEDIALOG_OPEN ? NULL : file->encoding;
        int hasDef = 0;
        int i;
        for(i=skip;encStr=default_encodings[i];i++) {
            if(i >= arraylen) {
                arraylen *= 2;
                encodings = NEditRealloc(encodings, arraylen * sizeof(XmString));
            }
            encodings[i] = XmStringCreateSimple(encStr);
            if(defEncoding) {
                if(!strcasecmp(defEncoding, encStr)) {
                    hasDef = 1;
                    defEncoding = NULL;
                }
            }
        }
        if(skip == 1 && !hasDef && file->encoding) {
            /* Current encoding is not in the list of
             * default encodings
             * Add an extra item at pos 0 for the current encoding
             */
            encodings[0] = XmStringCreateSimple(file->encoding);
            currentEncItem = 1;
            skip = 0;
        }
        XtSetArg(args[n], XmNcolumns, 11); n++;
        XtSetArg(args[n], XmNitemCount, i-skip); n++;
        XtSetArg(args[n], XmNitems, encodings+skip); n++;
        data.encoding = XmCreateDropDownList(enc, "combobox", args, n);
        XtManageChild(data.encoding);
        for(int j=0;j<i;j++) {
            XmStringFree(encodings[j]);
        }
        NEditFree(encodings);
        
        if(file->encoding) {
            char *encStr = NEditStrdup(file->encoding);
            size_t encLen = strlen(encStr);
            for(int i=0;i<encLen;i++) {
                encStr[i] = toupper(encStr[i]);
            }
            str = XmStringCreateSimple(encStr);
            XmComboBoxSelectItem(data.encoding, str);
            XmStringFree(str);
            NEditFree(encStr);
        }
        
        /* bom and xattr option */
        if(type == FILEDIALOG_SAVE) {
            /* only the save file dialog needs an encoding select callback */
            XtAddCallback(
                    data.encoding,
                    XmNselectionCallback,
                    (XtCallbackProc)filedialog_select_encoding,
                    &data);
            
            n = 0;
            str = XmStringCreateSimple("Write BOM");
            XtSetArg(args[n], XmNlabelString, str); n++;
            XtSetArg(args[n], XmNset, file->writebom); n++;
            data.bom = XmCreateToggleButton(enc, "togglebutton", args, n);
            XtManageChild(data.bom);
            XmStringFree(str);
            
            n = 0;
            str = XmStringCreateSimple("Store encoding in extended attribute");
            XtSetArg(args[n], XmNlabelString, str); n++;
            data.xattr = XmCreateToggleButton(enc, "togglebutton", args, n);
            XtManageChild(data.xattr);
            XmStringFree(str);
        }
        
    }
    
    /* middle */
    data.selectedview = LastView;
    
    // form for dir/file lists
    int layout = 2;
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, filterform); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, enc); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNwidth, 580); n++;
    XtSetArg(args[n], XmNheight, 400); n++;
    layout = n;
    data.listform = XmCreateForm(form, "fds_listform", args, n); 
    
    // icon view
    n = layout;
    XtSetArg(args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
    XtSetArg(args[n], XmNscrollBarDisplayPolicy, XmSTATIC); n++;
    //XtSetArg(args[n], XmNwidth, 580); n++;
    //XtSetArg(args[n], XmNheight, 400); n++;
    Widget scrollw = XmCreateScrolledWindow(form, "scroll_win", args, n);
    data.scrollw = scrollw;
    
    // detail view
    n = layout;
    XtSetArg(args[n], XmNcolumns, 3); n++;
    XtSetArg(args[n], XmNheadingColumns, 0); n++;
    XtSetArg(args[n], XmNheadingRows, 1); n++;
    XtSetArg(args[n], XmNallowColumnResize, 1); n++;
    XtSetArg(args[n], XmNsimpleWidths, "40c 15c 25c"); n++;
    data.grid = XmLCreateGrid(form, "grid", args, n);
    XmLGridSetStrings(data.grid, "Name|Size|Last Modified");
    
    // icon view again
    n = 0;
    XtSetArg(args[n], XmNlayoutType,  XmSPATIAL); n++;
    XtSetArg(args[n], XmNselectionPolicy, XmSINGLE_SELECT); n++;
    XtSetArg(args[n], XmNentryViewType, XmLARGE_ICON); n++;
    XtSetArg(args[n], XmNspatialStyle, XmGRID); n++;
    XtSetArg(args[n], XmNspatialIncludeModel, XmAPPEND); n++;
    XtSetArg(args[n], XmNspatialResizeModel, XmGROW_MINOR); n++;
    XtSetArg(args[n], XmNlargeCellWidth, 150); n++;
    data.container = XmCreateContainer(scrollw, "table", args, n);
    XtManageChild(data.container);
    XtAddCallback(XtParent(data.container), XmNresizeCallback,
		(XtCallbackProc)resize_container, &data);
    XmContainerAddMouseWheelSupport(data.container);
    
    XtAddCallback(
            data.container,
            XmNselectionCallback,
            (XtCallbackProc)filedialog_select,
            &data);
    XtAddCallback(
            data.container,
            XmNdefaultActionCallback,
            (XtCallbackProc)filedialog_action,
            &data);
    
    // dir/file lists
    n = 0;
    str = XmStringCreateLocalized("Directories");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget lsDirLabel = XmCreateLabel(data.listform, "label", args, n);
    XtManageChild(lsDirLabel);
    XmStringFree(str);
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, lsDirLabel); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 35); n++;
    data.dirlist = XmCreateScrolledList(data.listform, "dirlist", args, n);
    Dimension w, h;
    XtMakeResizeRequest(data.dirlist, 150, 200, &w, &h);
    XtManageChild(data.dirlist);
    XtAddCallback(
            data.dirlist,
            XmNdefaultActionCallback,
            (XtCallbackProc)dirlist_activate,
            &data); 
    XtAddCallback(
            data.dirlist,
            XmNbrowseSelectionCallback,
            (XtCallbackProc)dirlist_select,
            &data);
    
    n = 0;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, lsDirLabel); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, data.dirlist); n++;
    XtSetArg(args[n], XmNleftOffset, WIDGET_SPACING); n++;
    data.filelist = XmCreateScrolledList(data.listform, "filelist", args, n);
    XtManageChild(data.filelist);
    XtAddCallback(
            data.filelist,
            XmNdefaultActionCallback,
            (XtCallbackProc)filelist_activate,
            &data); 
    XtAddCallback(
            data.filelist,
            XmNbrowseSelectionCallback,
            (XtCallbackProc)filelist_select,
            &data);
    
    n = 0;
    str = XmStringCreateLocalized("Files");
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, data.dirlist); n++;
    XtSetArg(args[n], XmNleftOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, data.filelist); n++;
    XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget lsFileLabel = XmCreateLabel(data.listform, "label", args, n);
    XtManageChild(lsFileLabel);
    XmStringFree(str);
    
    Widget focus = NULL;
    switch(data.selectedview) {
        case 0: XtManageChild(scrollw); focus = data.container; break;
        case 1: XtManageChild(data.listform); focus = data.filelist; break;
        case 2: XtManageChild(data.grid); break;
    }
    
    if(file->path) {
        char *defDir = ParentPath(file->path);
        filedialog_update_dir(&data, defDir);
        PathBarSetPath(data.pathBar, defDir);
        NEditFree(defDir);
        
        XmTextFieldSetString(data.name, FileName(file->path));
    } else {
        char *defDirStr = GetDefaultDirectoryStr();
        char *defDir = defDirStr ? defDirStr : getenv("HOME");
        
        filedialog_update_dir(&data, defDir);
        PathBarSetPath(data.pathBar, defDir);
    }
    
    //init_container_size(&data);
    
    /* event loop */
    ManageDialogCenteredOnPointer(form);
    
    XmProcessTraversal(focus, XmTRAVERSE_CURRENT);
    
    XtAppContext app = XtWidgetToApplicationContext(dialog);
    while(!data.end && !XtAppGetExitFlag(app)) {
        XEvent event;
        XtAppNextEvent(app, &event);
        XtDispatchEvent(&event);
    }
    
    LastView = data.selectedview;
    
    if(data.selectedPath && !data.selIsDir && data.status == FILEDIALOG_OK) {
        file->path = data.selectedPath;
        data.selectedPath = NULL;
        
        if(file->setenc) {
            int encPos;
            XtVaGetValues(data.encoding, XmNselectedPosition, &encPos, NULL);
            if(type == FILEDIALOG_OPEN) {
                if(encPos > 0) {
                    /* index 0 is the "detect" item which is not a valid 
                       encoding string that can be used later */
                    file->encoding = default_encodings[encPos];
                }
            } else {
                if(currentEncItem) {
                    /* first item is the current encoding that is not 
                     * in default_encodings */
                    if(encPos > 0) {
                        file->encoding = default_encodings[encPos];
                    }
                } else {
                    /* first item is "UTF-8" */
                    file->encoding = default_encodings[encPos+1];
                }
            }
        }
        
        if(type == FILEDIALOG_SAVE) {
            int bomVal = 0;
            int xattrVal = 0;
            int wrapVal = 0;
            XtVaGetValues(data.bom, XmNset, &bomVal, NULL);
            XtVaGetValues(data.xattr, XmNset, &xattrVal, NULL);
            XtVaGetValues(data.wrap, XmNset, &wrapVal, NULL);
            file->writebom = bomVal;
            file->setxattr = xattrVal;
            file->addwrap = wrapVal;
            
            int formatVal = 0;
            XtVaGetValues(data.unixFormat, XmNset, &formatVal, NULL);
            if(formatVal) {
                file->format = UNIX_FILE_FORMAT;
            } else {
                XtVaGetValues(data.dosFormat, XmNset, &formatVal, NULL);
                if(formatVal) {
                    file->format = DOS_FILE_FORMAT;
                } else {
                    XtVaGetValues(data.macFormat, XmNset, &formatVal, NULL);
                    if(formatVal) {
                        file->format = MAC_FILE_FORMAT;
                    }
                }
            }
            
        }
    } else {
        data.status = FILEDIALOG_CANCEL;
    }
   
    XtUnmapWidget(dialog);
    XtDestroyWidget(dialog);
    return data.status;
}
