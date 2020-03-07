/*
 * Copyright 2020 Olaf Wintermann
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

#include "xdnd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Atom XdndAware;
static Atom XdndSelection;
static Atom XdndStatus;
static Atom XdndActionCopy;

static Atom selType;

static int canDrop;

XtCallbackProc dropCallback;
XtPointer dropData;

static void checkSelectionValue(Widget w, XtPointer clientData, Atom *selType,
	Atom *type, XtPointer value, unsigned long *length, int *format)
{
    if(value) {
        canDrop = 1; // we have a text/uri-list value
        XtFree(value);
    }
}

static void getSelectionValue(Widget w, XtPointer clientData, Atom *selType,
	Atom *type, XtPointer value, unsigned long *length, int *format)
{
    if(value) {
        if(dropCallback) {
            dropCallback(w, value, dropData);
        }
        XtFree(value);
    }
}

static void xdnd_enter(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    //printf("xdnd_enter\n");
    XtGetSelectionValue(w, XdndSelection, selType, checkSelectionValue, NULL, 0);
}

static void xdnd_position(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    //printf("xdnd_position\n");
    
    XEvent msg;
    memset(&msg, 0, sizeof(XEvent));
    msg.xany.type = ClientMessage;
    msg.xany.display = XtDisplay(w);
    msg.xclient.window = event->xclient.data.l[0];
    msg.xclient.message_type = XdndStatus;
    msg.xclient.format = 32;
    msg.xclient.data.l[0] = XtWindow(w);
    msg.xclient.data.l[1] = canDrop;
    msg.xclient.data.l[4] = XdndActionCopy;
    XSendEvent(XtDisplay(w), msg.xclient.window, 0, 0, &msg);
}

static void xdnd_drop(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    //printf("xdnd_drop\n");
    
    XtGetSelectionValue(w, XdndSelection, selType, getSelectionValue, NULL, 0);
    
    canDrop = 0;
}

static void xdnd_leave(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    //printf("xdnd_leave\n");
    canDrop = 0;
}

static XtActionsRec xdndactions[] = {
    {"xdnd_enter", xdnd_enter},
    {"xdnd_position", xdnd_position},
    {"xdnd_drop", xdnd_drop},
    {"xdnd_leave", xdnd_leave}
};


void XdndInit(
        Display *dpy,
        XtAppContext app,
        XtCallbackProc dropCB,
        XtPointer dropCBData)
{
    // init atoms
    XdndAware = XInternAtom(dpy, "XdndAware", False);
    XdndSelection = XInternAtom(dpy, "XdndSelection", False);
    XdndStatus = XInternAtom(dpy, "XdndStatus", False);
    XdndActionCopy = XInternAtom(dpy, "XdndActionCopy", False);
    selType = XInternAtom(dpy, "text/uri-list", False);
    
    XtAppAddActions(
            app,
            xdndactions,
            sizeof(xdndactions) / sizeof(XtActionsRec));
    
    dropCallback = dropCB;
    dropData = dropCBData;
}

void XdndEnable(Widget w) {
    int version = 4;
    XChangeProperty(
            XtDisplay(w),
            XtWindow(w),
            XdndAware,
            XA_ATOM,
            32,
            PropModeReplace,
            (XtPointer)&version,
            1);
    
    if(XtIsShell(w)) {
        XtOverrideTranslations(w, XtParseTranslationTable(
                "<Message>XdndEnter:    xdnd_enter()\n"
                "<Message>XdndPosition: xdnd_position()\n"
                "<Message>XdndDrop:     xdnd_drop()\n"
                "<Message>XdndLeave:    xdnd_leave()\n"));
    }
}
