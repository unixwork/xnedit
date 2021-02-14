/*
 * Copyright 2021 Olaf Wintermann
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

#include "tileviewP.h"

#include <stdio.h>
#include <stdlib.h>

static void tileview_class_init(void);
static void tileview_init(Widget request, Widget neww, ArgList args, Cardinal *num_args);
static void tileview_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes);
static void tileview_destroy(Widget widget);
static void tileview_resize(Widget widget);
static void tileview_expose(Widget widget, XEvent* event, Region region);
static Boolean tileview_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args);
static Boolean tileview_acceptfocus(Widget widget, Time *time);


static void mouse1DownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static XtResource resources[] = {
    {XmNfocusCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TileViewWidget, tileview.focusCB), XmRCallback, NULL},
    {XmNactivateCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TileViewWidget, tileview.activateCB), XmRCallback, NULL},
    {XmNrealizeCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TileViewWidget, tileview.realizeCB), XmRCallback, NULL},
    {XmNselectionCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TileViewWidget, tileview.selectionCB), XmRCallback, NULL},
    {XnHtileDrawFunc, XnCtileDrawFunc, XtRFunction, sizeof(TileDrawFunc), XtOffset(TileViewWidget, tileview.drawFunc), XmRPointer, NULL},
    {XnHtileDrawData, XnCtileDrawData, XmRPointer, sizeof(void*), XtOffset(TileViewWidget, tileview.drawData), XmRPointer, NULL},
    {XnHtileData, XnCtileData, XmRPointer, sizeof(void**), XtOffset(TileViewWidget, tileview.data), XmRPointer, NULL},
    {XnHtileDataLength, XnCtileDataLength, XmRInt, sizeof(int), XtOffset(TileViewWidget, tileview.length), XmRString, "0"},
    {XnHtileSelection, XnCtileSelection, XmRInt, sizeof(int), XtOffset(TileViewWidget, tileview.selection), XmRString, "-1"},
    {XnHtileWidth, XnCtileWidth, XmRInt, sizeof(int), XtOffset(TileViewWidget, tileview.tileWidth), XmRString, "100"},
    {XnHtileHeight, XnCtileHeight, XmRInt, sizeof(int), XtOffset(TileViewWidget, tileview.tileHeight), XmRString, "100"}
};

static XtActionsRec actionslist[] = {
  {"mouse1down",mouse1DownAP}
};


static char defaultTranslations[] = "\
<FocusIn>:		        focusIn()\n\
<FocusOut>:                     focusOut()\n\
<EnterWindow>:		        leave()\n\
<LeaveWindow>:                  enter()\n\
<Btn1Down>:                     mouse1down()";



TileViewClassRec tileviewWidgetClassRec = {
    // Core Class
    {
        (WidgetClass)&xmPrimitiveClassRec,
        "TileView",                      // class_name
        sizeof(TileViewRec),             // widget_size
        tileview_class_init,             // class_initialize
        NULL,                            // class_part_initialize
        FALSE,                           // class_inited
        tileview_init,                  // initialize
        NULL,                            // initialize_hook
        tileview_realize,               // realize
        actionslist,                     // actions
        XtNumber(actionslist),           // num_actions
        resources,                       // resources
        XtNumber(resources),             // num_resources
        NULLQUARK,                       // xrm_class
        True,                            // compress_motion
        True,                            // compress_exposure
        True,                            // compress_enterleave
        False,                           // visible_interest
        tileview_destroy,               // destroy
        tileview_resize,                // resize
        tileview_expose,                // expose
        tileview_set_values,            // set_values
        NULL,                            // set_values_hook
        XtInheritSetValuesAlmost,        // set_values_almost
        NULL,                            // get_values_hook
        tileview_acceptfocus,           // accept_focus
        XtVersion,                       // version
        NULL,                            // callback_offsets
        defaultTranslations,             // tm_table
        XtInheritQueryGeometry,          // query_geometry
        NULL,                            // display_accelerator
        NULL,                            // extension
    },
    // XmPrimitive
    {
        (XtWidgetProc)_XtInherit,        // border_highlight
        (XtWidgetProc)_XtInherit,        // border_unhighlight
        NULL,                            // translations
        NULL,                            // arm_and_activate
        NULL,                            // syn_resources
        0,                               // num_syn_resources
        NULL                             // extension
    },
    // TextField
    {
        0
    }
};

WidgetClass tileviewWidgetClass = (WidgetClass)&tileviewWidgetClassRec;


static void tileview_class_init(void) {
    
}

static void tileview_init(Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    TileViewWidget tv = (TileViewWidget)neww;
    
    tv->tileview.recalcSize = True;
}

static void tvInitXft(TileViewWidget w) {
    XWindowAttributes attributes;
    XGetWindowAttributes(XtDisplay(w), XtWindow(w), &attributes); 
    
    Screen *screen = w->core.screen;
    Visual *visual = screen->root_visual;
    for(int i=0;i<screen->ndepths;i++) {
        Depth d = screen->depths[i];
        if(d.depth == w->core.depth) {
            visual = d.visuals;
            break;
        }
    }
    
    Display *dp = XtDisplay(w);
    w->tileview.d = XftDrawCreate(
            dp,
            XtWindow(w),
            visual,
            w->core.colormap);
    
}

static void tileview_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes) {
     (coreClassRec.core_class.realize)(widget, mask, attributes);
    
    
    TileViewWidget tv = (TileViewWidget)widget;
    Display *dpy = XtDisplay(widget);
    
    XGCValues gcvals;
    gcvals.foreground = tv->primitive.foreground;
    gcvals.background = tv->core.background_pixel;
    tv->tileview.gc = XCreateGC(dpy, XtWindow(widget), (GCForeground|GCBackground), &gcvals);
    
    tvInitXft(tv);
    
    XtCallCallbacks(widget, XmNrealizeCallback, NULL);
}

static void tileview_destroy(Widget widget) {
    TileViewWidget tv = (TileViewWidget)widget;
    XFreeGC(XtDisplay(widget), tv->tileview.gc);
    XftDrawDestroy(tv->tileview.d);
}

static void tileview_resize(Widget widget) {
    
}

static Dimension calcHeight(TileViewWidget tv, int *cols, int *rows) {
    long len = tv->tileview.length;
    int elemPerLine = 0;
    int lines = 0;
    
    Dimension tileWidth = tv->tileview.tileWidth;
    Dimension tileHeight = tv->tileview.tileHeight;

    elemPerLine = tv->core.width / tileWidth;
    if(elemPerLine == 0) {
        elemPerLine = 1;
    }

    lines = (len+elemPerLine-1) / elemPerLine;
    
    *cols = elemPerLine;
    *rows = lines;
    
    return lines * tileHeight;
}

static void tileview_expose(Widget widget, XEvent* event, Region region) {
    TileViewWidget tv = (TileViewWidget)widget;
    Display *dpy = XtDisplay(widget);
    XExposeEvent *e = &event->xexpose;
    
    Dimension tileWidth = tv->tileview.tileWidth;
    Dimension tileHeight = tv->tileview.tileHeight;
    
    long len = tv->tileview.length;
    int cols = 0;
    int rows = 0;
    
    Dimension width = tv->core.width;

    Dimension height = calcHeight(tv, &cols, &rows);

    if(tv->tileview.recalcSize) {
        if(width < tileWidth) width = tileWidth;
        
        Widget parent = XtParent(widget);
        if(height < parent->core.height) height = parent->core.height;
        
        XtMakeResizeRequest(widget, width, height, NULL, NULL);
        tv->tileview.recalcSize = False;
    }
    
    tv->tileview.recalcSize = False;
    if(!tv->tileview.drawFunc || !tv->tileview.data) {
        return;
    }
    
    int c = 0;
    int r = 0;
    for(int i=0;i<len;i++) {
        if(c >= cols) {
            c = 0;
            r++;
        }
        
        int x = c*tileWidth;
        int y = r*tileHeight;
        
        Boolean isSelected = i == tv->tileview.selection ? True : False;
        tv->tileview.drawFunc(widget, tv->tileview.data[i], tileWidth, tileHeight, x, y, tv->tileview.drawData, isSelected);
        
        c++;
    }
    
    //XDrawLine(dpy, XtWindow(widget), tv->tileview.gc, 0, 0, widget->core.width, widget->core.height);
    //XDrawLine(dpy, XtWindow(widget), tv->tileview.gc, widget->core.width, 0, 0, widget->core.height);
}

static Boolean tileview_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    Boolean r = False;
    TileViewWidget o = (TileViewWidget)old;
    TileViewWidget n = (TileViewWidget)neww;
    
    if(o->tileview.data != n->tileview.data) {
        r = True;
    }
    if(o->tileview.length != n->tileview.length) {
        r = True;
    }
    if(o->tileview.drawFunc != n->tileview.drawFunc) {
        r = True;
    }
    if(o->tileview.drawData != n->tileview.drawData) {
        r = True;
    }
    
    n->tileview.recalcSize = True;
    
    return r;
}

static Boolean tileview_acceptfocus(Widget widget, Time *time) {
    return True;
}


/* ------------------------------ Actions ------------------------------ */

static void mouse1DownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TileViewWidget tv = (TileViewWidget)w;
    int x = event->xbutton.x;
    int y = event->xbutton.y;
    
    XmProcessTraversal(w, XmTRAVERSE_CURRENT);
    
    int cols, rows;
    (void)calcHeight(tv, &cols, &rows);
    
    int selectedCol = x / tv->tileview.tileWidth;
    int selectedRow = y / tv->tileview.tileHeight;
    
    long sel = selectedRow * cols + selectedCol;
    if(sel > tv->tileview.length || selectedCol >= cols) {
        sel = -1;
    }
    tv->tileview.selection = sel;
    
    XClearArea(XtDisplay(w), XtWindow(w), 0, 0, 0, 0, TRUE);
    XFlush(XtDisplay(w));
    
    XnTileViewCallbackStruct cb;
    cb.selection = sel;
    cb.selected_item = sel >= 0 ? tv->tileview.data[sel] : NULL;  
    XtCallCallbacks(w, XmNselectionCallback, &cb);
}

/* ------------------------------ Public ------------------------------ */

Widget XnCreateTileView(Widget parent, char *name, ArgList arglist, Cardinal argcount) {
    return XtCreateWidget(name, tileviewWidgetClass, parent, arglist, argcount);
}

int XnTileViewGetSelection(Widget tileView) {
    TileViewWidget tv = (TileViewWidget)tileView;
    return tv->tileview.selection;
}

void XnTileViewSetSelection(Widget tileView, int selection) {
    TileViewWidget tv = (TileViewWidget)tileView;
    tv->tileview.selection = selection;
}

GC XnTileViewGC(Widget tileView) {
    TileViewWidget tv = (TileViewWidget)tileView;
    return tv->tileview.gc;
}

XftDraw* XnTileViewXftDraw(Widget tileView) {
    TileViewWidget tv = (TileViewWidget)tileView;
    return tv->tileview.d;
}
