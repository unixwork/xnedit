/************************************************************************
 *                                                                      *
 * dragAndDrop.c -- CDE drag and drop functions to provide              *
 *                  basic support for file drops.                       *
 *                                                                      *
 * Originally Written By Fredrik Jönsson                                *
 * Modified by Per Grahn                                                *  
 *             Olaf Wintermann                                          *
 ************************************************************************/
/* $Id$ */

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/AtomMgr.h>
#include "../source/textBuf.h"
#include "../source/nedit.h"
#include "../source/window.h"
/* Added to avoid warnings */
#include "../source/file.h"
#include "../source/preferences.h" 
#include "dragAndDrop.h"
#include "DialogF.h"
#ifdef CDE
#include <Dt/Dnd.h>
#else
#include <Xm/DragDrop.h>

static Atom xa_targets[10];
static int n_targets = 0;

static void TransferDone(Widget transfer, XtPointer client_data,
        Atom * selection, Atom * type, XtPointer value,
        unsigned long *length, int *format);
#endif

/* #define DBGS 1 */

static char *dndgeom = NULL;

/*======================================================================
Edit the file with path "filePath"
======================================================================*/
static void editdatafile(Widget w, char *filePath) {
    // open file
    char *params[2];
    params[0] = filePath;
    params[1] = NULL;
    XtCallActionProc(w, "open", NULL, params, 1);
}

/*======================================================================
Create a new file with content "buffer"
======================================================================*/
static void newdatafile(Widget widget, char *buffer) {
    WindowInfo *window = WidgetToWindow(widget);
    int openInTab = GetPrefOpenInTab();
    
    if (window->filenameSet || window->fileChanged) {
        window = EditNewFile(openInTab? window : NULL, NULL, False, NULL, window->path);
    }
    
    BufInsert(window->buffer, 0, buffer);
}

#ifdef CDE

/*======================================================================
 * fileTransferCallback -- Called by neditTransferCallback if the dropaction
 *						   was a filedrop.
======================================================================*/
void fileTransferCallback(Widget widget, XtPointer clientData, XtPointer callData) {
    DtDndTransferCallbackStruct *transferInfo =
            (DtDndTransferCallbackStruct *) callData;
    char *filePath, *name, *path;
    int ii;

    if (transferInfo == NULL) {
        return;
    }

    /* Verify the protocol and callback reasons */
    if (transferInfo->dropData->protocol != DtDND_FILENAME_TRANSFER ||
            transferInfo->reason != DtCR_DND_TRANSFER_DATA) {
        return;
    }

    /* Open the file(s). */
    for (ii = 0; ii < transferInfo->dropData->numItems; ii++) {
        editdatafile(widget, transferInfo->dropData->data.files[ii]);
    }
}

/*======================================================================
 * dataTransferCallback -- Called by neditTransferCallback if the dropaction
 *		           was a data drop.
======================================================================*/
void dataTransferCallback(Widget widget, XtPointer clientData, XtPointer callData) {
    DtDndTransferCallbackStruct *transferInfo =
            (DtDndTransferCallbackStruct *) callData;
    int ii;

    if (transferInfo == NULL) return;

    /* Verify the protocol and callback reasons */
    if (transferInfo->dropData->protocol != DtDND_BUFFER_TRANSFER ||
            transferInfo->reason != DtCR_DND_TRANSFER_DATA) {
        return;
    }

    for (ii = 0; ii < transferInfo->dropData->numItems; ii++) {
        WindowInfo *window, *w;
        int i;
        char name[20];
        char *p, *q;
        char *string = (char *) transferInfo->dropData->data.buffers[ii].bp;
        p = strstr(string, "file:");
#ifdef DBGS
        printf("STRING: %s\nwidget: %s", string, XtName(widget));
#endif
        if (p) {
            while (p) {
                q = strchr(p, '\r');
                *q = '\0';
                editdatafile(widget, p + 5);
                p = strstr(q + 1, "file:");
            }
        } else {
            newdatafile(widget, string);
        }
    }
}
#endif


#ifndef CDE

/* ===============================================================
 * The selection callback proc after drop transfer.
 * Bytes have been received. Check they are OK
 * If data received is corrupted, issue a warning.
 */
static void TransferDone(Widget transfer, XtPointer client_data,
        Atom *selection, Atom *type, XtPointer value,
        unsigned long *length, int *format) {
    Widget widget = (Widget) client_data;
    String string = NULL;

#ifdef DBGS
    printf("TransferDone: Type %s format %d, Selection: %s\n",
            XmGetAtomName(XtDisplay(widget), *type),
            *format, XmGetAtomName(XtDisplay(widget), *selection));
#endif
    if (*format == 8) {
        if (*type == xa_targets[3] || *type == xa_targets[0]) {
            char *p, *q;
            string = (char *) value;
            p = strstr(string, "file:");
#ifdef DBGS
            printf("STRING: %s\nwidget: %s", string, XtName(widget));
#endif
            if (p) {
                while (p) {
                    q = strchr(p, '\r');
                    *q = '\0';
                    editdatafile(widget, p + 5);
                    p = strstr(q + 1, "file:");
                }
            } else
                newdatafile(widget, string);
        } else if (*type == xa_targets[2]) {
            char *p, *q;
            int dlen;
            char path[1024];
            q = (char *) value;
#ifdef DBGS
            printf("FILES: %s\nwidget: %s", q, XtName(widget));
#endif
            p = strchr(q, ' ');
            dlen = p - q;
            while (p) {
                memset(path, 0, 1024);
                strncpy(path, (char *) value, dlen);
                strcat(path, "/");
                q = p + 1;
                p = strchr(q, ' ');
                if (p) strncat(path, q, p - q);
                else strcat(path, q);
#ifdef DBGS
                printf("FILE: %s\n", path);
#endif
                editdatafile(widget, path);
            }
        } else if (*type == xa_targets[1]) {
            string = (char *) value;
#ifdef DBGS
            printf("FILE_NAME: %s\n", string);
#endif
            editdatafile(widget, string);
        }
    } else {
        XtVaSetValues(transfer,
                XmNtransferStatus, XmTRANSFER_FAILURE,
                XmNnumDropTransfers, 0,
                NULL);
        DialogF(DF_INF, widget, 1, "Transfer Done", "Dropped data is corrupted. Type %s format %d",
                "Ok!", XmGetAtomName(XtDisplay(widget), *type), *format);
        return;
    }
    // Do something
}
#endif

/*
 * neditDropCallback -- Called when a dropaction on the area
 *			    set up by neditDropSetup is received.
 */
void neditDropCallback(Widget widget, XtPointer clientData, XtPointer callData) {
#ifndef CDE
    XmDropProcCallbackStruct *dropInfo =
            (XmDropProcCallbackStruct *) callData;
    Arg args[8];
    Arg get_args[4];
    Atom *exports;
    int i = 0, n = 0;
    Cardinal num_targets = 0;
    Cardinal num_transfer = 0;
    XmDropTransferEntryRec target_data[2];
    unsigned char status;

    XtSetArg(get_args[i], XmNexportTargets, &exports);
    i++;
    XtSetArg(get_args[i], XmNnumExportTargets, &num_targets);
    i++;
    XtGetValues(dropInfo->dragContext, get_args, i);
    if (dropInfo->dropSiteStatus == XmVALID_DROP_SITE) {
        int targ_ix = num_targets;
        dropInfo->operation = XmDROP_COPY;
        for (i = 0; i < num_targets; i++) {
#ifdef DBGS
            printf("%s: target %d of %d: %s\n", XtName(widget), i, num_targets,
                    XmGetAtomName(XtDisplay(widget), exports[i]));
#endif
            if (exports[i] == xa_targets[0]
                    || exports[i] == xa_targets[1]
                    || exports[i] == xa_targets[2]
                    || exports[i] == xa_targets[3]) {
                targ_ix = i;
                break;
            }
        }
        if (targ_ix < num_targets) {
            status = XmTRANSFER_SUCCESS;
            num_transfer = 1;
            target_data[0].target = exports[targ_ix];
            target_data[0].client_data = (XtPointer) widget;
            XtSetArg(args[n], XmNtransferProc, TransferDone);
            n++;
            XtSetArg(args[n], XmNdropTransfers, target_data);
            n++;
#ifdef DBGS
            printf("selected target is %d (%s)\n", targ_ix,
                    XmGetAtomName(XtDisplay(widget), exports[targ_ix]));
#endif
        } else {
            char *msg = (num_targets > 0 ? XmGetAtomName(XtDisplay(widget), exports[0]) : strdup(""));
            DialogF(DF_WARN, widget, 1, "DropCallback", "Non Identified Object \"%s\" is Dropped.",
                    "Ok!", msg);
            status = XmTRANSFER_FAILURE;
            num_transfer = 0;
            XFree(msg);
        }
    } else {
        char *msg = (num_targets > 0 ? XmGetAtomName(XtDisplay(widget), exports[0]) : strdup(""));
        DialogF(DF_WARN, widget, 1, "DropCallback", "Not a vaild drop site. Non Identified Object \"%s\" is Dropped.",
                "Ok!", msg);
        status = XmTRANSFER_FAILURE;
        num_transfer = 0;
        XFree(msg);
    }
    XtSetArg(args[n], XmNnumDropTransfers, num_transfer);
    n++;
    XtSetArg(args[n], XmNtransferStatus, status);
    n++;
    XmDropTransferStart(dropInfo->dragContext, args, n);

#else
    DtDndTransferCallbackStruct *transferInfo =
            (DtDndTransferCallbackStruct *) callData;

    if (transferInfo == NULL) {
        return;
    }

    switch (transferInfo->dropData->protocol) {
        case DtDND_FILENAME_TRANSFER:
            fileTransferCallback(widget, clientData, callData);
            break;
        case DtDND_BUFFER_TRANSFER:
            dataTransferCallback(widget, clientData, callData);
            break;
    }
#endif
}

void neditDropInit(Widget w, char *geometry) {
#ifndef CDE
#ifdef DBGS
    printf("neditDropInit w=%s, n_targets=%d, geom=%s\n", XtName(w), n_targets,
            (geometry ? geometry : "<no geometry>"));
#endif
    if (n_targets == 0) {
        xa_targets[n_targets++] = XmInternAtom(XtDisplay(w), "text/uri-list", False);
        xa_targets[n_targets++] = XmInternAtom(XtDisplay(w), XmSFILE_NAME, False);
        xa_targets[n_targets++] = XmInternAtom(XtDisplay(w), "FILES", False);
        xa_targets[n_targets++] = XmInternAtom(XtDisplay(w), "STRING", False);
    }
#endif
    dndgeom = geometry;
}

/*
 * neditDropSetup -- Registers widget to accept drops of files.
 */
void neditDropWidget(Widget w) {
#ifndef CDE
    Arg args[10];
    Cardinal n = 0;

    if (n_targets == 0) neditDropInit(w, NULL);
#ifdef DBGS
    printf("neditDropWidget dropDraw=%s, %x\n", XtName(w), w);
#endif
    XtSetArg(args[n], XmNdropSiteOperations, XmDROP_COPY);
    n++;
    XtSetArg(args[n], XmNdropSiteActivity, XmDROP_SITE_ACTIVE);
    n++;
    XtSetArg(args[n], XmNdropSiteType, XmDROP_SITE_COMPOSITE);
    n++;
    XtSetArg(args[n], XmNimportTargets, xa_targets);
    n++;
    XtSetArg(args[n], XmNnumImportTargets, n_targets);
    n++;
    XtSetArg(args[n], XmNdragProc, NULL);
    n++;
    XtSetArg(args[n], XmNdropProc, neditDropCallback);
    n++;
    XmDropSiteRegister(w, args, n);
#else
    static XtCallbackRec transferCBRec[] ={
        {neditDropCallback, NULL},
        {NULL, NULL}
    };
    DtDndVaDropRegister(w,
            DtDND_FILENAME_TRANSFER | DtDND_BUFFER_TRANSFER,
            XmDROP_LINK | XmDROP_COPY,
            transferCBRec,
            DtNtextIsBuffer, True,
            DtNpreserveRegistration, False,
            NULL);
#endif
}
