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

#include <Xm/XmAll.h>

static void tileview_class_init(void);
static void tileview_init(Widget request, Widget neww, ArgList args, Cardinal *num_args);
static void tileview_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes);
static void tileview_destroy(Widget widget);
static void tileview_resize(Widget widget);
static void tileview_expose(Widget widget, XEvent* event, Region region);
static Boolean tileview_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args);
static Boolean tileview_acceptfocus(Widget widget, Time *time);
static XtGeometryResult tileview_geometrymanager(Widget widget, XtWidgetGeometry *request, XtWidgetGeometry *reply);

static void vscrollbar_valuechanged(Widget w, TileViewWidget tv, XmScrollBarCallbackStruct *cb);

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
    {XnHtileWidth, XnCtileWidth, XmRInt, sizeof(int), XtOffset(TileViewWidget, tileview.tileWidth), XmRString, "134"},
    {XnHtileHeight, XnCtileHeight, XmRInt, sizeof(int), XtOffset(TileViewWidget, tileview.tileHeight), XmRString, "80"}
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

static XtResource constraints[] = {};

TileViewClassRec tileviewWidgetClassRec = {
    // Core Class
    {
        (WidgetClass)&xmManagerClassRec,
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
    // Composite Class
    {
        XtInheritGeometryManager, /* geometry_manager */
        XtInheritChangeManaged,  /* change_managed */   
        XtInheritInsertChild,  /* insert_child */ 
        XtInheritDeleteChild,  /* delete_child */  
        NULL,                 /* extension */    
    },
    // Constraint Class
    {
        constraints,    /* resources */  
        XtNumber(constraints),  /* num_resources */    
        0,  /* constraint_size */  
        NULL,  /* initialize */  
        NULL,  /* destroy */
        NULL,  /* set_values */   
        NULL,  /* extension */    
    },
    // XmManager Class
    {
        XtInheritTranslations, // translations
        NULL, // syn_resources
        0,    // num_syn_resources
        NULL, // syn_constraint_resources
        0,    // num_syn_constraint_resources
        XmInheritParentProcess, // parent_process
        NULL  // extension
    }
};

WidgetClass tileviewWidgetClass = (WidgetClass)&tileviewWidgetClassRec;


static void tileview_class_init(void) {
    
}

static void adjust_vscrollbar_pos(TileViewWidget tv) {
    XtUnmanageChild(tv->tileview.vscrollbar);
    XtVaSetValues(
            tv->tileview.vscrollbar,
            XmNx, tv->core.width - tv->tileview.vscrollbarWidth,
            XmNy, 0,
            XmNheight, tv->core.height,
            NULL);
    
    XtManageChild(tv->tileview.vscrollbar);
}

static void adjust_vscrollbar(TileViewWidget tv, int cols, int rows, int height) {
    int value = 0;
    
    int wh = tv->core.height;
    
    if(height < wh) height = wh;
    
    XtVaSetValues(tv->tileview.vscrollbar,
            XmNmaximum, height,
            //XmNvalue, value,
            XmNsliderSize, wh,
            NULL);
}

static void tileview_init(Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    TileViewWidget tv = (TileViewWidget)neww;
    Display *dp = XtDisplay(neww);
    
    tv->tileview.recalcSize = True;
    tv->tileview.btn1ClickPrev = 0;
    
    tv->tileview.cols = 0;
    tv->tileview.rows = 0;
    tv->tileview.scroll_pos = 0;
    tv->tileview.scroll_mul = 1;
    
    FcPattern *pattern = FcNameParse((FcChar8*)"Sans:size=9");
    tv->tileview.font = pattern;
    
    FcResult result;
    pattern = FcPatternDuplicate(pattern);
    FcPattern *match = XftFontMatch(dp, DefaultScreen(dp), pattern, &result);
    
    XftFont *font = XftFontOpenPattern(dp, match);
    
    XnFontList *fontlist = malloc(sizeof(XnFontList));
    XnFontList *fontlist_end = fontlist;
    fontlist->font = font;
    fontlist->next = NULL;
    
    tv->tileview.fontlist_begin = fontlist;
    tv->tileview.fontlist_end = fontlist;
    
    Arg a[16];
    int n = 0;
    
    tv->tileview.vscrollbar = XmCreateScrollBar(neww, "tileview_vscrollbar", a, n);
    tv->tileview.vscrollbarWidth = tv->tileview.vscrollbar->core.width; 
    XtAddCallback(tv->tileview.vscrollbar, XmNdragCallback, (XtCallbackProc)vscrollbar_valuechanged, tv);
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

static int calcHeight(TileViewWidget tv, int *cols, int *rows) {
    long len = tv->tileview.length;
    int elemPerLine = 0;
    int lines = 0;
    
    Dimension tileWidth = tv->tileview.tileWidth;
    Dimension tileHeight = tv->tileview.tileHeight;

    elemPerLine = (tv->core.width - tv->tileview.vscrollbarWidth) / tileWidth;
    if(elemPerLine == 0) {
        elemPerLine = 1;
    }

    lines = (len+elemPerLine-1) / elemPerLine;
    
    *cols = elemPerLine;
    *rows = lines;
    
    return lines * tileHeight;
}

static void tileview_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes) {
    //(coreClassRec.core_class.realize)(widget, mask, attributes);
    (xmManagerClassRec.core_class.realize)(widget, mask, attributes);
    
    
    TileViewWidget tv = (TileViewWidget)widget;
    Display *dpy = XtDisplay(widget);
    
    XGCValues gcvals;
    gcvals.foreground = tv->manager.foreground;
    gcvals.background = tv->core.background_pixel;
    tv->tileview.gc = XCreateGC(dpy, XtWindow(widget), (GCForeground|GCBackground), &gcvals);
    
    tvInitXft(tv);
    
    XtCallCallbacks(widget, XmNrealizeCallback, NULL);
    
    adjust_vscrollbar_pos(tv);
    int cols, rows;
    int height = calcHeight(tv, &cols, &rows);
    adjust_vscrollbar(tv, cols, rows, height);
}

static void tileview_destroy(Widget widget) {
    TileViewWidget tv = (TileViewWidget)widget;
    XFreeGC(XtDisplay(widget), tv->tileview.gc);
    XftDrawDestroy(tv->tileview.d);
}

static void tileview_resize(Widget widget) {
    TileViewWidget tv = (TileViewWidget)widget;
    int cols, rows;
    
    int height = calcHeight(tv, &cols, &rows);
    if(cols != tv->tileview.cols || rows != tv->tileview.rows) {
        XClearArea(XtDisplay(widget), XtWindow(widget), 0, 0, 0, 0, TRUE);
    }
    
    adjust_vscrollbar_pos((TileViewWidget)widget);
    adjust_vscrollbar((TileViewWidget)widget, cols, rows, height);
}



static void tileview_expose(Widget widget, XEvent* event, Region region) {
    TileViewWidget tv = (TileViewWidget)widget;
    Display *dpy = XtDisplay(widget);
    XExposeEvent *e = &event->xexpose;

    XClearArea(dpy, XtWindow(widget), e->x, e->y, e->width, e->height, False);
    
    XRectangle rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = e->width;
    rect.height = e->height;
    XftDrawSetClipRectangles(tv->tileview.d, e->x, e->y, &rect, 1);
    
    Dimension tileWidth = tv->tileview.tileWidth;
    Dimension tileHeight = tv->tileview.tileHeight;
    
    long len = tv->tileview.length;
    int cols = 0;
    int rows = 0;
    
    Dimension width = tv->core.width - tv->tileview.vscrollbarWidth;;

    Dimension height = calcHeight(tv, &cols, &rows);
    
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
        
        int yt = y - tv->tileview.scroll_pos * tv->tileview.scroll_mul;
        if(yt+tileHeight >= e->y && yt <= e->y + e->height) {
            rect.x = 0;
            rect.y = 0;
            rect.width = e->x + e->width > x+tileWidth ? x+tileWidth - e->x - 6 : e->width;
            rect.height = e->y + e->height > yt+tileHeight ? yt+tileHeight - e->y : e->height; 
            
            XftDrawSetClipRectangles(tv->tileview.d, e->x, e->y, &rect, 1);
            tv->tileview.drawFunc(widget, tv->tileview.data[i], tileWidth, tileHeight, x, yt, tv->tileview.drawData, isSelected);
        }
        
        c++;
    }
    
    //XDrawLine(dpy, XtWindow(widget), tv->tileview.gc, 0, 0, cols*tileWidth, rows*tileHeight);
    //XDrawLine(dpy, XtWindow(widget), tv->tileview.gc, cols*tileWidth, 0, 0, rows*tileHeight);
    
    //XDrawLine(dpy, XtWindow(widget), tv->tileview.gc, 0, 0, widget->core.width, widget->core.height);
    //XDrawLine(dpy, XtWindow(widget), tv->tileview.gc, widget->core.width, 0, 0, widget->core.height);
}

static Boolean tileview_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    Boolean r = False;
    TileViewWidget o = (TileViewWidget)old;
    TileViewWidget n = (TileViewWidget)neww;
    
    if(o->tileview.data != n->tileview.data) {
        r = True;
        int cols, rows;
        int height = calcHeight((TileViewWidget)neww, &cols, &rows);
        adjust_vscrollbar((TileViewWidget)neww, cols, rows, height);
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

static XtGeometryResult tileview_geometrymanager(Widget widget, XtWidgetGeometry *request, XtWidgetGeometry *reply) {
    return XtGeometryYes;
}

/* ------------------------------ Actions ------------------------------ */

static void vscrollbar_valuechanged(Widget w, TileViewWidget tv, XmScrollBarCallbackStruct *cb) {
    tv->tileview.scroll_pos = cb->value;
    tv->tileview.scroll_mul = 1;
    XClearArea(XtDisplay(tv), XtWindow(tv), 0, 0, 0, 0, TRUE);
}

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
    
    Time t = event->xbutton.time;
    int multiclicktime = XtGetMultiClickTime(XtDisplay(w));
    if(t - tv->tileview.btn1ClickPrev < multiclicktime) {
        XtCallCallbacks(w, XmNactivateCallback, &cb);
    } else {
        XtCallCallbacks(w, XmNselectionCallback, &cb);
        tv->tileview.btn1ClickPrev = t;
    }
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

/* ------------------------------ Text API ------------------------------ */



static XftFont* get_font_for_char(TileViewWidget tv, XnFontList **begin, XnFontList **end, FcChar32 c) {
    Display *dp = XtDisplay(tv);
    
    XnFontList *elm = *begin;
    while(elm) {
        if(FcCharSetHasChar(elm->font->charset, c)) {
            return elm->font;
        }
        elm = elm->next;
    }
    
    /* charset for char c */
    FcCharSet *charset = FcCharSetCreate();
    FcValue value;
    value.type = FcTypeCharSet;
    value.u.c = charset;
    FcCharSetAddChar(charset, c);
    if(!FcCharSetHasChar(charset, c)) {
        FcCharSetDestroy(charset);
        return elm->font;
    }
    
    /* font lookup based on the NFont pattern */ 
    FcPattern *pattern = FcPatternDuplicate(tv->tileview.font);
    FcPatternAdd(pattern, FC_CHARSET, value, 0);
    FcResult result;
    FcPattern *match = XftFontMatch (
            dp, DefaultScreen(dp), pattern, &result);
    if(!match) {
        FcPatternDestroy(pattern);
        return elm->font;
    }
    
    XftFont *newFont = XftFontOpenPattern(dp, match);   
    if(!newFont || !FcCharSetHasChar(newFont->charset, c)) {
        FcPatternDestroy(pattern);
        FcPatternDestroy(match);
        if(newFont) {
            XftFontClose(dp, newFont);
        }
    }
    
    XnFontList *nextFont = malloc(sizeof(XnFontList));
    nextFont->font = newFont;
    nextFont->next = NULL;
    (*end)->next = nextFont;
    
    FcCharSetDestroy(charset);
    
    return newFont;
}

XnText* XnCreateText(Widget tileView, const char *str, size_t len, int width) {
    Display *dp = XtDisplay(tileView);
    TileViewWidget tv = (TileViewWidget)tileView;
    
    XnText *text = malloc(sizeof(XnText));
    text->dp = dp;
    text->font = XftFontOpenName(dp, DefaultScreen(dp), "Sans:size=9");
    text->chinfo = calloc(len, sizeof(XnTextCh));
    text->str = calloc(len, sizeof(FcChar32));
    text->width = width;
    
    int br_i = 0;
    int linebreak_at = 0;
    
    int xoff = 0;
    
    int l = len;
    int charlen = 1;
    int ci = 0;
    for(int i=0;i<len;i+=charlen) {
        FcChar32 c32;
        charlen = FcUtf8ToUcs4((FcChar8*)str+i, &c32, l);
        l -= charlen;
        
        XftFont *font = get_font_for_char(tv, &tv->tileview.fontlist_begin, &tv->tileview.fontlist_end, c32);
        XGlyphInfo ex;
        XftTextExtents32(dp, font, &c32, 1, &ex); 
        
        XnTextCh ch;
        ch.font = font;
        ch.width = ex.xOff;
        
        xoff += ex.xOff;
        if(xoff > width) {
            if(br_i > 0) {
                linebreak_at = br_i;
            } else {
                linebreak_at = ci-1;
            }
            
            xoff = INT_MIN;
        }
        
        char c = str[i];
        if((c < 65 && c > 90) || (c < 97 && c > 122)) {
            br_i = ci;
        }
        
        text->chinfo[ci] = ch;
        text->str[ci] = c32;
        ci++;
    }
    text->len = ci;
    
    text->newlineat = linebreak_at > 0 ? linebreak_at : ci;
    
    return text;
}

static int text_offset(XnText *text, int start, int length) {
    int width = 0;
    int x0 = 0;
    int str_end = length + start;
    for(int i=start;i<str_end;i++) {
        width += text->chinfo[i].width;
    }
    if(width < text->width) {
        x0 = (text->width - width) / 2;
    }
    return x0;
}

static void text_draw(XnText *text, XftDraw *d, XftColor *color, int start, int length, int x, int y) {
    XftFont *current = text->chinfo[start].font;
    
    int str_start = start;
    int xoff_start = x;
    int xoff = x;
    
    int str_end = length + start;
    int i;
    for(i=start;i<str_end;i++) {
        XftFont *f = text->chinfo[i].font;
        if(f != current) {
            
            XftDrawString32(d, color, current, xoff_start, y, text->str+str_start, i-str_start);
            str_start = i;
            xoff_start = xoff;
        }
        xoff += text->chinfo[i].width;
        current = f;
    }
    if(i-str_start > 0) {
        XftDrawString32(d, color, current, xoff_start, y, text->str+str_start, i-str_start);
    }
}

void XnTextDraw(XnText *text, XftDraw *d, XftColor *color, int x, int y) {
    y += text->font->ascent + text->font->descent;
    
    int line1 = text->newlineat;
    XGlyphInfo ex;
    int xoff = text_offset(text, 0, line1);
    text_draw(text, d, color, 0, line1, x+xoff, y);
    int line2 = text->len - line1;
    if(line2 > 0) {
        xoff = text_offset(text, line1, line2);
        text_draw(text, d, color, line1, line2, x+xoff, y + text->font->ascent + text->font->descent);
    }
}

void XnTextDestroy(XnText *text) {
    if(text->font) {
        XftFontClose(text->dp, text->font);
    }
    free(text);
}
