/*******************************************************************************
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
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
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *                                                                         *
*                                                                              *
*******************************************************************************/

#include "filter.h"
#include "window.h"
#include "nedit.h"
#include "preferences.h"

#include "../util/nedit_malloc.h"
#include "../util/misc.h"
#include "../util/managedList.h"

#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <Xm/XmAll.h>

static IOFilter **filters;
static size_t numFilters;


typedef struct {
    Widget shell;
    WindowInfo *window;
    Widget managedListW;
    Widget nameW;
    Widget patternW;
    Widget extW;
    Widget cmdInW;
    Widget cmdOutW;

    IOFilter **filters;
    int nfilters;
    int filteralloc;
} filterDialog;

static filterDialog fd  = { NULL, NULL, NULL, NULL, NULL, NULL};


static void fdDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void fdOkCB(Widget w, XtPointer clientData, XtPointer callData);
static void fdApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static void fdCloseCB(Widget w, XtPointer clientData, XtPointer callData);

static int fdUpdateList(void);


static int fdIsEmpty(void)
{
    Widget w[] = { fd.nameW, fd.patternW, fd.extW, fd.cmdInW, fd.cmdOutW };
    size_t n = sizeof(w) / sizeof(Widget);
    for(int i=0;i<n;i++) {
        char *s = XmTextGetString(w[i]);
        size_t len = s ? strlen(s) : 0;
        XtFree(s);
        if(len > 0) {
            return 0;
        }
    }
    return 1;
}

static void fdFreeItemCB(void *item)
{
    IOFilter *f = item;
    XtFree(f->name);
    XtFree(f->pattern);
    XtFree(f->ext);
    XtFree(f->cmdin);
    XtFree(f->cmdout);
    NEditFree(f);
}

static void *fdGetDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg)
{
    if(fdIsEmpty()) {
        return NULL;
    }
    
    IOFilter *filter = NEditMalloc(sizeof(IOFilter));
    filter->name = XmTextGetString(fd.nameW);
    filter->pattern = XmTextGetString(fd.patternW);
    filter->ext = XmTextGetString(fd.extW);
    filter->cmdin = XmTextGetString(fd.cmdInW);
    filter->cmdout = XmTextGetString(fd.cmdOutW);
    
    if (strlen(filter->name) == 0 ||
        strlen(filter->pattern) == 0 ||
        strlen(filter->ext) == 0 ||
        strlen(filter->cmdin) == 0 ||
        strlen(filter->cmdout) == 0 ||
        fd.nfilters == fd.filteralloc)
    {
        fdFreeItemCB(filter);
        *abort = 1;
    }
    
    return filter;
}

static void fdSetDisplayedCB(void *item, void *cbArg)
{
    if(!item) {
        XmTextSetString(fd.nameW, "");
        XmTextSetString(fd.patternW, "");
        XmTextSetString(fd.extW, "");
        XmTextSetString(fd.cmdInW, "");
        XmTextSetString(fd.cmdOutW, "");
    } else {
        IOFilter *f = item;
        XmTextSetString(fd.nameW, f->name);
        XmTextSetString(fd.patternW, f->pattern);
        XmTextSetString(fd.extW, f->ext);
        XmTextSetString(fd.cmdInW, f->cmdin);
        XmTextSetString(fd.cmdOutW, f->cmdout);
    }
}

static IOFilter* fdCopyFilter(IOFilter *f) {
    IOFilter *cp = NEditMalloc(sizeof(IOFilter));
    cp->name = NEditStrdup(f->name ? f->name : "");
    cp->pattern = NEditStrdup(f->pattern ? f->pattern : "");
    cp->ext = NEditStrdup(f->ext ? f->ext : "");
    cp->cmdin = NEditStrdup(f->cmdin ? f->cmdin : "");
    cp->cmdout = NEditStrdup(f->cmdout ? f->cmdout : "");
    return cp;
}

#define FD_LIST_RIGHT 30
#define FD_LEFT_MARGIN_POS 1
#define FD_RIGHT_MARGIN_POS 99
#define FD_H_MARGIN 10

void FilterSettings(WindowInfo *window)
{
    XmString s1;

    if(fd.shell) {
        RaiseDialogWindow(fd.shell);
        return;
    }
    
    fd.filteralloc = 256;
    fd.filters = NEditCalloc(sizeof(IOFilter), fd.filteralloc);
    fd.nfilters = numFilters;
    for(int i=0;i<numFilters;i++) {
        fd.filters[i] = fdCopyFilter(filters[i]);
    }

    int ac = 0;
    Arg args[20];
    //XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
    XtSetArg(args[ac], XmNtitle, "Filters"); ac++;
    fd.shell = CreateWidget(TheAppShell, "filters",
	    topLevelShellWidgetClass, args, ac);
    AddSmallIcon(fd.shell);
    Widget form = XtVaCreateManagedWidget("filtersForm", xmFormWidgetClass,
	    fd.shell, XmNautoUnmanage, False,
	    XmNresizePolicy, XmRESIZE_NONE, NULL);

    Widget topLbl = XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form,
            XmNlabelString, s1=MKSTRING(
"To modify the properties of an existing filter, select the name\n\
from the list on the left.  Select \"New\" to add a new filter to the list."),
        XmNmnemonic, 'N',
        XmNtopAttachment, XmATTACH_POSITION,
        XmNtopPosition, 2,
        XmNleftAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 6,
        NULL);
    XmStringFree(s1);

    Widget okBtn = XtVaCreateManagedWidget("ok",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 10,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 30,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(okBtn, XmNactivateCallback, fdOkCB, NULL);
    XmStringFree(s1);

    Widget applyBtn = XtVaCreateManagedWidget("apply",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("Apply"),
            XmNmnemonic, 'A',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 40,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 60,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, fdApplyCB, NULL);
    XmStringFree(s1);

    Widget closeBtn = XtVaCreateManagedWidget("close",
            xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Close"),
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 70,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 90,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, fdCloseCB, NULL);
    XmStringFree(s1);

    Widget sep1 = XtVaCreateManagedWidget("sep1", xmSeparatorGadgetClass, form,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, okBtn,
        XmNbottomOffset, 4,
        NULL);

    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopOffset, FD_H_MARGIN); ac++;
    XtSetArg(args[ac], XmNtopWidget, topLbl); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNleftPosition, FD_LEFT_MARGIN_POS); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNrightPosition, FD_LIST_RIGHT-1); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNbottomWidget, sep1); ac++;
    XtSetArg(args[ac], XmNbottomOffset, FD_H_MARGIN); ac++;
    fd.managedListW = CreateManagedList(form, "list", args, ac,
            (void **)fd.filters, &fd.nfilters,
            256, 20, fdGetDisplayedCB, NULL, fdSetDisplayedCB,
            form, fdFreeItemCB);

    s1 = XmStringCreateLocalized("Name");
    Widget lblName = XtVaCreateManagedWidget("nameLbl", xmLabelGadgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, topLbl,
        XmNtopOffset, FD_H_MARGIN,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        XmNlabelString, s1,
        XmNalignment, XmALIGNMENT_BEGINNING,
        NULL);
    XmStringFree(s1);
     
    fd.nameW = XtVaCreateManagedWidget("patternLbl", xmTextWidgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, lblName,
        XmNtopOffset, 6,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        NULL);
      
    s1 = XmStringCreateLocalized("File Pattern");
    Widget lblPattern = XtVaCreateManagedWidget("nameLbl", xmLabelGadgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, fd.nameW,
        XmNtopOffset, 6,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        //XmNrightAttachment, XmATTACH_POSITION,
        //XmNrightPosition, 75,
        XmNleftOffset, 6,
        XmNlabelString, s1,
        XmNalignment, XmALIGNMENT_BEGINNING,
        NULL);
    XmStringFree(s1);
    
    fd.patternW = XtVaCreateManagedWidget("patternLbl", xmTextWidgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, lblPattern,
        XmNtopOffset, 6,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        XmNrightAttachment, XmATTACH_POSITION,
        XmNrightPosition, 75,
        XmNleftOffset, 6,
        NULL);
    
    s1 = XmStringCreateLocalized("Default Extension");
    Widget lblExt = XtVaCreateManagedWidget("extLbl", xmLabelGadgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, fd.nameW,
        XmNtopOffset, 6,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, fd.patternW,
            XmNleftOffset, 6,
        XmNleftPosition, 76,
        XmNrightOffset, 6,
        XmNlabelString, s1,
        XmNalignment, XmALIGNMENT_BEGINNING,
        NULL);
    XmStringFree(s1);
    
    fd.extW = XtVaCreateManagedWidget("patternLbl", xmTextWidgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, lblExt,
        XmNtopOffset, 6,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.patternW,
        XmNleftOffset, 6,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 6,
        NULL);
    
    s1 = XmStringCreateLocalized("Input Filter Command");
    Widget lblCmdIn = XtVaCreateManagedWidget("cmdInLbl", xmLabelGadgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, fd.patternW,
        XmNtopOffset, 6,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        XmNlabelString, s1,
        XmNalignment, XmALIGNMENT_BEGINNING,
        NULL);
    XmStringFree(s1);

    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, lblCmdIn); ac++;
    XtSetArg(args[ac], XmNtopOffset, 6); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNleftWidget, fd.managedListW); ac++;
    XtSetArg(args[ac], XmNleftOffset, 6); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(args[ac], XmNrightOffset, 6); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNbottomPosition, 61); ac++;
    XtSetArg(args[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
    XtSetArg(args[ac], XmNrows, 4); ac++;
    fd.cmdInW = XmCreateScrolledText(form, "cmdInText", args, ac);
    XtManageChild(fd.cmdInW);

    s1 = XmStringCreateLocalized("Output Filter Command");
    Widget lblCmdOut = XtVaCreateManagedWidget("cmdOutLbl", xmLabelGadgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, fd.cmdInW,
        XmNtopOffset, FD_H_MARGIN,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        XmNlabelString, s1,
        XmNalignment, XmALIGNMENT_BEGINNING,
        NULL);
    XmStringFree(s1);

    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, lblCmdOut); ac++;
    XtSetArg(args[ac], XmNtopOffset, 6); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNleftWidget, fd.managedListW); ac++;
    XtSetArg(args[ac], XmNleftOffset, 6); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(args[ac], XmNrightOffset, 6); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNbottomWidget, sep1); ac++;
    XtSetArg(args[ac], XmNbottomOffset, FD_H_MARGIN); ac++;
    XtSetArg(args[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
    XtSetArg(args[ac], XmNrows, 4); ac++;
    fd.cmdOutW = XmCreateScrolledText(form, "cmdInText", args, ac);
    XtManageChild(fd.cmdOutW);

    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);

    XtAddCallback(form, XmNdestroyCallback, fdDestroyCB, NULL);
    AddMotifCloseCallback(fd.shell, fdCloseCB, NULL);

    RealizeWithoutForcingPosition(fd.shell);
}

static void fdDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    for(int i=0;i<fd.nfilters;i++) {
        fdFreeItemCB(fd.filters[i]);
    }
    NEditFree(fd.filters);
}

static void fdApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fdUpdateList();
}

static void fdOkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if(!fdUpdateList()) {
        return;
    }
    
    XtDestroyWidget(fd.shell);
    fd.shell = NULL;
}

static void fdCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    XtDestroyWidget(fd.shell);
    fd.shell = NULL;
}

static int fdUpdateList(void)
{
    if (!UpdateManagedList(fd.managedListW, True))
    	return False;
    
    for(int i=0;i<numFilters;i++) {
        fdFreeItemCB(filters[i]);
    }
    NEditFree(filters);
    
    filters = NEditCalloc(fd.nfilters, sizeof(IOFilter*));
    numFilters = fd.nfilters;
    for(int i=0;i<numFilters;i++) {
        filters[i] = fdCopyFilter(fd.filters[i]);
    }
    
    /* Note that preferences have been changed */
    MarkPrefsChanged();
    
    return True;
}


static int valueListNext(char *str, int len)
{
    for(int i=0;i<len;i++) {
        if(str[i] == ';') {
            str[i] = '\0';
            len = i;
            break;
        }
    }
    return len;
}

static char *valuedup(char *str, int len)
{
    char *newvalue = NEditMalloc(len+1);
    newvalue[len] = '\0';
    memcpy(newvalue, str, len);
    return newvalue;
}

static IOFilter* ParseFilterStr(char *str, int len)
{
    int pos = 0;
    char *name = str;
    int namelen = valueListNext(str, len);
    int skip_space = 0;
    while(isspace(*name) && namelen > 0) {
        name++;
        namelen--;
        skip_space++;
    }
    if(namelen <= 0) {
        return NULL;
    }   
    str += skip_space + namelen + 1;
    len -= skip_space + namelen + 1;
    char *pattern = str;
    int patternlen = valueListNext(str, len);
    if(patternlen <= 0) {
        return NULL;
    }
    str += patternlen + 1;
    len -= patternlen + 1;
    char *ext = str;
    int extlen = valueListNext(str, len);
    if(extlen <= 0) {
        return NULL;
    }
    str += extlen + 1;
    len -= extlen + 1;
    char *cmdin = str;
    int cmdinlen = valueListNext(str, len);
    if(cmdinlen <= 0) {
        return NULL;
    }
    str += cmdinlen + 1;
    len -= cmdinlen + 1;
    char *cmdout = str;
    
    int cmdoutlen = valueListNext(str, len);
    if(cmdoutlen <= 0) {
        return NULL;
    }
    str += cmdinlen + 1;
    len -= cmdinlen + 1;
    
    IOFilter *filter = NEditMalloc(sizeof(IOFilter));
    filter->name = valuedup(name, namelen);
    filter->pattern = valuedup(pattern, patternlen);
    filter->ext = valuedup(ext, extlen);
    filter->cmdin = valuedup(cmdin, cmdinlen);
    filter->cmdout = valuedup(cmdout, cmdoutlen);
    return filter;
}

void ParseFilterSettings(char *str)
{
    size_t filterAlloc = 8;
    filters = NEditCalloc(filterAlloc, sizeof(IOFilter*));
    numFilters = 0;
    
    size_t len = strlen(str);
    int lineStart = 0;
    for(int i=0;i<=len;i++) {
        if(i == len || str[i] == '\n') {
            IOFilter *filter = ParseFilterStr(str+lineStart, i-lineStart);
            lineStart = i+1;
            
            if(filter) {
                if(numFilters == filterAlloc) {
                    filterAlloc += 8;
                    NEditRealloc(filters, filterAlloc * sizeof(IOFilter*));
                }
                filters[numFilters] = filter;
                numFilters++;
            }
        }
    }
}

char* WriteFilterString(void)
{
    size_t str_alloc = 2048;
    size_t len = 0;
    char *str = NEditMalloc(str_alloc);
    str[0] = 0;
    
    for(int i=0;i<numFilters;i++) {
        IOFilter *filter = filters[i];
        char *prefix = i > 0 ? "\t" : "";
        char *suffix = i+1 != numFilters ? "\\n\\\n" : "";
        for(;;) {
            size_t remaining = str_alloc - len;
            size_t w = snprintf(
                    str + len,
                    remaining, 
                    "%s%s;%s;%s;%s;%s%s",
                    prefix,
                    filter->name,
                    filter->pattern,
                    filter->ext,
                    filter->cmdin,
                    filter->cmdout,
                    suffix);
            if(w < remaining) {
                len += w;
                break;
            } else {
                str_alloc += 1024;
                str = NEditRealloc(str, str_alloc);
            }
        }
    }
    
    return str;
}

IOFilter** GetFilterList(size_t *num)
{
    *num = numFilters;
    return filters;
}

IOFilter* GetFilterFromName(const char *name)
{
    if(!name) {
        return NULL;
    }
    for(int i=0;i<numFilters;i++) {
        if(!strcmp(filters[i]->name, name)) {
            return filters[i];
        }
    }
    return NULL;
}

/* ----------------------------- FileStream -----------------------------*/

typedef struct FilterIOThreadData {
    FILE *file;
    int fd_in;
    int fd_out;
} FilterIOThreadData;

static void* file_input_thread(void *data) {
    FilterIOThreadData *stream = data;
    
    char buf[16384];
    size_t r;
    while((r = fread(buf, 1, 16384, stream->file)) > 0) {
        write(stream->fd_in, buf, r);
    }
    
    close(stream->fd_in);
    
    return NULL;
}

static void* file_output_thread(void *data) {
    FilterIOThreadData *stream = data;
     
    char buf[16384];
    ssize_t r;
    while((r = read(stream->fd_out, buf, r)) > 0) {
        fwrite(buf, 1, r, stream->file);
    }
    
    close(stream->fd_out);
    fclose(stream->file);
    
    return NULL;
}

static int filestream_create_pipes(FileStream *stream) {
    if(pipe(stream->pin)) {
        return 1;
    }
    if(pipe(stream->pout)) {
        close(stream->pin[0]);
        close(stream->pin[1]);
        return 1;
    }
    return 0;
}

static FileStream* filestream_open(FILE *f, const char *filter_cmd, int mode) {
    FileStream *stream = NEditMalloc(sizeof(FileStream));
    stream->file = f;
    stream->filter_cmd = filter_cmd ? NEditStrdup(filter_cmd) : NULL;
    stream->pid = 0;
    stream->hdrbufpos = 0;
    stream->hdrbuflen = 0;
    stream->mode = mode;
    
    if(filter_cmd) {
        if(filestream_create_pipes(stream)) {
            NEditFree(stream->filter_cmd);
            NEditFree(stream);
            fclose(f);
            fprintf(stderr, "Failed to create pipe: %s\n", strerror(errno));
            return NULL;
        }
        
        pid_t child = fork();
        if(child == 0) {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);

            // we need stdin, stdout and stderr refer to the previously
            // created pipes
            if(dup2(stream->pin[0], STDIN_FILENO) == -1) {
                perror("dup2");
                exit(1);
            }
            if(dup2(stream->pout[1], STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(1);
            }

            close(stream->pin[1]);

            // execute the command using the shell specified by preferences
            //fprintf(stderr, "info: input filter command: %s\n", filter_cmd);
            execlp(GetPrefShell(), GetPrefShell(), "-c", filter_cmd, NULL);

            // execlp only returns if an error occured
            fprintf(stderr, "Error starting shell: %s\n", GetPrefShell());
            exit(1);
        } else {
            stream->pid = child;
            
            FilterIOThreadData *data = NEditMalloc(sizeof(FilterIOThreadData));
            data->file = stream->file;
            data->fd_in = stream->pin[1];
            data->fd_out = stream->pout[0];
            
            pthread_t tid;
            if(pthread_create(&tid, NULL, mode == 0 ? file_input_thread : file_output_thread, data)) {
                fprintf(stderr, "Errro: cannot create file input thread: %s\n", strerror(errno));
                NEditFree(data);
                close(stream->pin[0]);
                close(stream->pin[1]);
                close(stream->pout[0]);
                close(stream->pout[1]);
                fclose(f);
                NEditFree(stream->filter_cmd);
                NEditFree(stream);
                return NULL;
            }
            
            if(mode == 1) {
                stream->file = NULL; // file will be closed by file_output_thread
            }
            
            close(stream->pout[1]);
        }
    } 
    
    return stream;
}

FileStream* filestream_open_r(FILE *f, const char *filter_cmd) {
    return filestream_open(f, filter_cmd, 0);
}

FileStream* filestream_open_w(FILE *f, const char *filter_cmd) {
    return filestream_open(f, filter_cmd, 1);
}

int filestream_reset(FileStream *stream, int pos) {
    if(stream->pid == 0) {
        fseek(stream->file, pos, SEEK_SET);
    } else {
        if(pos > stream->hdrbuflen || stream->hdrbufpos > stream->hdrbuflen) {
            return 1;
        }
        stream->hdrbufpos = pos;
    }
    return 0;
}

size_t filestream_read(void *buffer, size_t nbytes, FileStream *stream) {
    if(stream->pid == 0) {
        return fread(buffer, 1, nbytes, stream->file);  
    } else {
        if(stream->hdrbufpos < stream->hdrbuflen) {
            // get bytes from hdrbuf before reading more bytes from the pipe
            size_t r = stream->hdrbuflen - stream->hdrbufpos;
            if(r > nbytes) {
                r = nbytes;
            }
            memcpy(buffer, stream->hdrbuf, r);
            nbytes -= r;
            buffer = ((char*)buffer) + r;
            stream->hdrbufpos += r;
            if(nbytes > 0) {
                r += filestream_read(buffer, nbytes, stream);
            }
            return r;
        }
        
        ssize_t sr = read(stream->pout[0], buffer, nbytes);
        //fwrite(buffer, 1, sr, stdout);
        //fflush(stdout);
        if(sr < 0) {
            return 0;
        }
        
        // the first bytes we read from the pipe are stored in hdrbuf
        // because we may want to reset the stream
        if(stream->hdrbuflen < FILESTREAM_HDR_BUFLEN) {
            size_t buflen = FILESTREAM_HDR_BUFLEN - stream->hdrbuflen;
            if(buflen > sr) {
                buflen = sr;
            }
            memcpy(stream->hdrbuf + stream->hdrbuflen, buffer, buflen);
            stream->hdrbuflen += buflen;
        }
        
        stream->hdrbufpos += sr;
        return (size_t)sr;
    }
}

size_t filestream_write(const void *buffer, size_t nbytes, FileStream *stream) {
    if(stream->pid == 0) {
        return fwrite(buffer, 1, nbytes, stream->file);
    } else {
        ssize_t w = write(stream->pin[1], buffer, nbytes);
        if(w < 0) {
            w = 0;
        }
        return w;
    } 
}

int filestream_close(FileStream *stream) {
    if(stream->pid != 0) {
        if(stream->mode == 0) {
            if(close(stream->pin[0])) {
                perror("pipe pin[0] close");
            }
            if(close(stream->pout[0])) {
                perror("pipe pout[0] close");
            }
        } else {
            if(close(stream->pin[1])) {
                perror("pipe pin[1] close");
            }
        }
    }
    int err = 0;
    if(stream->file) {
        err = fclose(stream->file);
    }
    NEditFree(stream->filter_cmd);
    NEditFree(stream);
    return err;
}
