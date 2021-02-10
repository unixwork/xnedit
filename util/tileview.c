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
#include "tileview.h"


static void tileview_class_init(void);
static void tileview_init(Widget request, Widget neww, ArgList args, Cardinal *num_args);
static void tileview_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes);
static void tileview_destroy(Widget widget);
static void tileview_resize(Widget widget);
static void tileview_expose(Widget widget, XEvent* event, Region region);
static Boolean tileview_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args);
static Boolean tileview_acceptfocus(Widget widget, Time *time);




static XtResource resources[] = {
    {XmNfocusCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TileViewWidget, tileview.focusCB), XmRCallback, NULL},
    {XmNactivateCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TileViewWidget, tileview.activateCB), XmRCallback, NULL}
};

static XtActionsRec actionslist[] = {
  {"NULL",NULL}
};


static char defaultTranslations[] = "\
<FocusIn>:		        focusIn()\n\
<FocusOut>:                     focusOut()\n\
<EnterWindow>:		        leave()\n\
<LeaveWindow>:                  enter()";



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
    
}

static void tileview_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes) {
    
}

static void tileview_destroy(Widget widget) {
    
}

static void tileview_resize(Widget widget) {
    
}

static void tileview_expose(Widget widget, XEvent* event, Region region) {
    
}

static Boolean tileview_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    
}

static Boolean tileview_acceptfocus(Widget widget, Time *time) {
    
}

