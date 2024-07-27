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

#include "textfieldP.h"
#include "textfield.h"
#include <Xm/DrawP.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "../source/textBuf.h" /* Utf8CharLen */
#include "../source/textSel.h"
#include "../source/textDisp.h" /* PixelToColor */
#include "../source/text.h" /* TextPrintXIMError */

#define TF_DEFAULT_FONT_NAME "Monospace:size=10"

#define TF_VPADDING 3
#define TF_HPADDING 2

#define TF_BUF_BLOCK 128

#define TF_TAB_STR "    "

static NFont *defaultFont;

static void textfield_class_init(void);

static void mouse1DownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void mouse1UpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void adjustselectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void insertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void actionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deletePrevCharAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deleteNextCharAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deletePrevWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deleteNextWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void moveLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void moveRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void moveLeftWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void moveRightWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void focusInAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void focusOutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void enterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void leaveAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void cutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void copyAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void pasteAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void endLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void beginLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void selectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void insertPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void tfCalcCursorPos(TextFieldWidget tf);

static int  tfPosToIndex(TextFieldWidget tf, int pos);

static int  tfXToPos(TextFieldWidget tf, int x);
static int  tfIndexToX(TextFieldWidget tf, int pos);
static void tfSelection(TextFieldWidget tf, int *start, int *end, int *startX, int *endX);
static void tfSelectionIndex(TextFieldWidget tf, int *start, int *end);

static void tfSetSelection(TextFieldWidget tf, int from, int to);
static void tfClearSelection(TextFieldWidget tf);

static void tfInsertPrimary(TextFieldWidget tf, XEvent *event);

static void TFInsert(TextFieldWidget tf, const char *chars, size_t nchars);
static  int TFLeftPos(TextFieldWidget tf);
static  int TFRightPos(TextFieldWidget tf);
static void TFDelete(TextFieldWidget tf, int from, int to);

static void wordbounds(TextFieldWidget tf, int index, int *out_wleft, int *out_wright);

static Dimension tfCalcHeight(TextFieldWidget tf);

static Atom aTargets;
static Atom aUtf8String;

static Boolean convertSelection(
        Widget w,
        Atom *seltype,
        Atom *target,
        Atom *type,
        XtPointer *value,
        unsigned long *length,
        int *format);

static void loseSelection(Widget w, Atom *type);

static XtResource resources[] = {
    {XmNtextRenderTable, XmCTextRenderTable, XmRString,sizeof(XmString),XtOffset(TextFieldWidget, textfield.renderTable), XmRString, NULL},
    {textNXftFont, textCXftFont, textTXftFont, sizeof(NFont *), XtOffset(TextFieldWidget, textfield.font), textTXftFont, &defaultFont},
    {XmNvalueChangedCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TextFieldWidget, textfield.valueChangedCB), XmRCallback, NULL},
    {XmNfocusCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TextFieldWidget, textfield.focusCB), XmRCallback, NULL},
    {XmNlosingFocusCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TextFieldWidget, textfield.losingFocusCB), XmRCallback, NULL},
    {XmNactivateCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(TextFieldWidget, textfield.activateCB), XmRCallback, NULL},
    {XmNblinkRate, XmCBlinkRate , XmRInt, sizeof(int), XtOffset(TextFieldWidget, textfield.blinkrate), XmRImmediate, (XtPointer)500}
};

static XtActionsRec actionslist[] = {
  {"mouse1down",mouse1DownAP},
  {"mouse1up",mouse1UpAP},
  {"adjustselection",adjustselectionAP},
  {"insert",insertAP},
  {"action",actionAP},
  {"moveleft",moveLeftAP},
  {"moveright",moveRightAP},
  {"moveleftword",moveLeftWordAP},
  {"moverightword",moveRightWordAP},
  {"deleteprev",deletePrevCharAP},
  {"deletenext",deleteNextCharAP},
  {"deleteprevword",deletePrevWordAP},
  {"deletenextword",deleteNextWordAP},
  {"focusIn",focusInAP},
  {"focusOut",focusOutAP},
  {"enter",enterAP},
  {"leave",leaveAP},
  {"cut-clipboard",cutAP},
  {"copy-clipboard",copyAP},
  {"paste-clipboard",pasteAP},
  {"endLine",endLineAP},
  {"beginLine",beginLineAP},
  {"selectAll",selectAllAP},
  {"insertPrimary",insertPrimaryAP},
  {"NULL",NULL}
};


static char defaultTranslations[] = "\
<FocusIn>:		        focusIn()\n\
<FocusOut>:                     focusOut()\n\
<EnterWindow>:		        leave()\n\
<LeaveWindow>:                  enter()\n\
s ~m ~a <Key>Tab:               PrimitivePrevTabGroup()\n\
~m ~a <Key>Tab:                 PrimitiveNextTabGroup()\n\
<Btn1Down>:                     mouse1down()\n\
<Btn1Up>:                       mouse1up()\n\
<Btn2Up>:                       insertPrimary()\n\
Ctrl<KeyPress>v:                paste-clipboard()\n\
Button1<MotionNotify>:          adjustselection()\n\
:<Key>KP_7:                     insert()\n\
:<Key>KP_8:                     insert()\n\
:<Key>KP_9:                     insert()\n\
:<Key>KP_4:                     insert()\n\
:<Key>KP_6:                     insert()\n\
:<Key>KP_1:                     insert()\n\
:<Key>KP_2:                     insert()\n\
:<Key>KP_3:                     insert()\n\
:<Key>KP_0:                     insert()\n\
<KeyPress>Return:               PrimitiveParentActivate() action()\n\
<Key>osfActivate:               PrimitiveParentActivate() action()\n\
<Key>osfCancel:                 PrimitiveParentCancel()\n\
Ctrl<KeyPress>osfBackSpace:     deleteprevword()\n\
Ctrl<KeyPress>osfDelete:        deletenextword()\n\
<KeyPress>osfBackSpace:         deleteprev()\n\
<KeyPress>osfDelete:            deletenext()\n\
Shift<Key>osfBeginLine:         beginLine(l)\n\
<Key>osfBeginLine:              beginLine()\n\
Shift<Key>osfEndLine:           endLine(r)\n\
<Key>osfEndLine:                endLine()\n\
Ctrl Shift<KeyPress>osfLeft:    moveleftword(l)\n\
Ctrl Shift<KeyPress>osfRight:   moverightword(r)\n\
Ctrl<KeyPress>osfLeft:          moveleftword()\n\
Ctrl<KeyPress>osfRight:         moverightword()\n\
Ctrl<Key>a:                     selectAll()\n\
Shift<KeyPress>osfLeft:         moveleft(l)\n\
Shift<KeyPress>osfRight:        moveright(r)\n\
<KeyPress>osfLeft:              moveleft()\n\
<KeyPress>osfRight:             moveright()\n\
<KeyPress>:	                insert()";



TextFieldClassRec tfWidgetClassRec = {
    // Core Class
    {
        (WidgetClass)&xmPrimitiveClassRec,
        "XmTextField",                   // class_name
        sizeof(TextFieldRec),            // widget_size
        textfield_class_init,            // class_initialize
        NULL,                            // class_part_initialize
        FALSE,                           // class_inited
        textfield_init,                  // initialize
        NULL,                            // initialize_hook
        textfield_realize,               // realize
        actionslist,                     // actions
        XtNumber(actionslist),           // num_actions
        resources,                       // resources
        XtNumber(resources),             // num_resources
        NULLQUARK,                       // xrm_class
        True,                            // compress_motion
        True,                            // compress_exposure
        True,                            // compress_enterleave
        False,                           // visible_interest
        textfield_destroy,               // destroy
        textfield_resize,                // resize
        textfield_expose,                // expose
        textfield_set_values,            // set_values
        NULL,                            // set_values_hook
        XtInheritSetValuesAlmost,        // set_values_almost
        NULL,                            // get_values_hook
        textfield_acceptfocus,           // accept_focus
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

WidgetClass textfieldWidgetClass = (WidgetClass)&tfWidgetClassRec;


static Boolean XftFontConvert(
    Display*   dpy,
    XrmValue*  args,
    Cardinal*  num_args,
    XrmValue*  from,
    XrmValue*  to,
    XtPointer* converter_data)
{
    NFont *font = FontFromName(dpy, from->addr);
    if(font) {
        memcpy(to->addr, &font, sizeof(NFont*));
        return True;
    } else {
        to->addr = 0;
        to->size = 0;
        return True;
    }
}

static void textfield_class_init(void) {
    XtSetTypeConverter(XmRString, textTXftFont, XftFontConvert, NULL, 0, XtCacheNone, NULL);
}

Widget XNECreateTextField(Widget parent, char *name, ArgList arglist, Cardinal argcount) {
    return XtCreateWidget(name, textfieldWidgetClass, parent, arglist, argcount);
}


void textfield_init(Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    TextFieldWidget tf = (TextFieldWidget)neww;
    tf->textfield.alloc = TF_BUF_BLOCK;
    tf->textfield.length = 0;
    tf->textfield.pos = 0;
    tf->textfield.buffer = XtMalloc(TF_BUF_BLOCK);
    
    tf->textfield.posX = 0;
    tf->textfield.posCalc = 0;
    
    tf->textfield.scrollX = 0;
    
    tf->textfield.hasSelection = 0;
    tf->textfield.selStart = 0;
    tf->textfield.selEnd = 0;
    
    tf->textfield.btn1ClickPrev = 0;
    tf->textfield.btn1ClickPrev2 = 0;
    
    tf->textfield.blinkProcId = 0;
    tf->textfield.cursorOn = 0;
       
    if(aTargets == 0) {
        aTargets = XInternAtom(XtDisplay(request), "TARGETS", 0);
    }
    if(aUtf8String == 0) {
        aUtf8String = XInternAtom(XtDisplay(request), "UTF8_STRING", 0);
    }
    
    if(tf->textfield.font) {
        tf->core.height = tfCalcHeight(tf);
    }
}

static void tfInitXft(TextFieldWidget w) {
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
    w->textfield.d = XftDrawCreate(
            dp,
            XtWindow(w),
            visual,
            w->core.colormap);
    
}

static void get_default_font(TextFieldWidget tf, Display *dp) {
    int ret;
    char *resName;
    XrmValue value;
    char *resourceType = NULL;
    char *rtFontName = NULL;
    char *rtFontSize = NULL;
    
    // We don't use the Motif RenderTable directly, however in case the
    // render table is configured to use Xft Fonts, we try to get the
    // same font settings
    //
    // The textfield has a textRenderTable resource, which is just a string
    // The textRenderTable name is used to get the rendertable font name
    // and size from the resource database
    
    char resNameBuf[256];
    resName = "defaultRT.fontName";
    if(tf->textfield.renderTable) {
        ret = snprintf(resNameBuf, 256, "%s.fontName", tf->textfield.renderTable);
        if(ret < 256) {
            resName = resNameBuf;
        }
    }
    if(XrmGetResource(XtDatabase(dp), resName, NULL, &resourceType, &value)) {
        if(!strcmp(resourceType, "String")) {
            rtFontName = value.addr;
        }
    }
    resName = "defaultRT.fontSize";
    if(tf->textfield.renderTable) {
        ret = snprintf(resNameBuf, 256, "%s.fontSize", tf->textfield.renderTable);
        if(ret < 256) {
            resName = resNameBuf;
        }
    }
    if(XrmGetResource(XtDatabase(dp), "fixedRT.fontSize", NULL, &resourceType, &value)) {
        if(!strcmp(resourceType, "String")) {
            rtFontSize = value.addr;
        }
    }
    
    char buf[256];
    char *fontname = TF_DEFAULT_FONT_NAME;
    if(rtFontName && rtFontSize) {
        ret = snprintf(buf, 256, "%s:size=%s", rtFontName, rtFontSize);
        if(ret < 256) {
            fontname = buf;
        }
    }
    
    defaultFont = FontFromName(dp, fontname);
    if(!defaultFont) {
        defaultFont = FontFromName(dp, TF_DEFAULT_FONT_NAME);
    }
}

void textfield_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes) {
    Display *dpy = XtDisplay(widget);
    TextFieldWidget text = (TextFieldWidget)widget;
    
    if(!defaultFont) {
        get_default_font(text, dpy);
    }
    if(!text->textfield.font) {
        text->textfield.font = defaultFont;
    }
       
    textfield_recalc_size((TextFieldWidget)widget);
    (coreClassRec.core_class.realize)(widget, mask, attributes);
    
    text->textfield.xim = XmImGetXIM(widget);
    if(text->textfield.xim) {
        Window win = XtWindow(widget);
        XIMStyle style = XIMPreeditNothing | XIMStatusNothing;
        text->textfield.xic = XCreateIC(
                text->textfield.xim,
                XNInputStyle,
                style,
                XNClientWindow,
                win, 
                XNFocusWindow,
                win,
                NULL);
    }
    
    tfInitXft(text);
    
    text->textfield.foregroundColor = PixelToColor(widget, text->primitive.foreground);  
    text->textfield.backgroundColor = PixelToColor(widget, text->core.background_pixel);
        
    XGCValues gcvals;
    gcvals.foreground = text->primitive.foreground;
    gcvals.background = text->core.background_pixel;
    text->textfield.gc = XCreateGC(dpy, XtWindow(widget), (GCForeground|GCBackground), &gcvals);
    
    gcvals.foreground = text->core.background_pixel;
    gcvals.background = text->primitive.foreground;
    text->textfield.gcInv = XCreateGC(dpy, XtWindow(widget), (GCForeground|GCBackground), &gcvals);
    
    gcvals.foreground = XtParent(text)->core.background_pixel;
    gcvals.background = XtParent(text)->core.background_pixel;
    text->textfield.highlightBackground = XCreateGC(dpy, XtWindow(widget), (GCForeground|GCBackground), &gcvals);
    
    text->textfield.textarea_xoff = text->primitive.shadow_thickness + text->primitive.highlight_thickness + TF_HPADDING;
    text->textfield.textarea_yoff = text->primitive.shadow_thickness + text->primitive.highlight_thickness + TF_VPADDING;
    
    text->textfield.posX = text->textfield.textarea_xoff;
}

void textfield_destroy(Widget widget) {
    TextFieldWidget tf = (TextFieldWidget)widget;
    XtFree(tf->textfield.buffer);
    if(tf->textfield.font != defaultFont) {
        FontUnref(tf->textfield.font);
    }
    if(tf->textfield.gc) {
        XFreeGC(XtDisplay(widget), tf->textfield.gc);
    }
    if(tf->textfield.gcInv) {
        XFreeGC(XtDisplay(widget), tf->textfield.gcInv);
    }
    if(tf->textfield.highlightBackground) {
        XFreeGC(XtDisplay(widget), tf->textfield.highlightBackground);
    }
    if(tf->textfield.blinkProcId != 0) {
        XtRemoveTimeOut(tf->textfield.blinkProcId);
    }
}

void textfield_resize(Widget widget) {
    
}


static int tfDrawString(TextFieldWidget tf, XftFont *font, XftColor *color, int x, const char *text, size_t len) { 
    NFontList *fl = tf->textfield.font->fonts;
    
    int yoff = tf->textfield.textarea_yoff;
    int area = tf->core.height - 2*yoff;
    int pad  = area - (fl->font->ascent + fl->font->descent);
    int hpad = pad/2;
    
    
    XftDrawStringUtf8(
            tf->textfield.d,
            color,
            font,
            x - tf->textfield.scrollX,
            tf->core.height - tf->textfield.textarea_yoff -fl->font->descent - hpad,
            (FcChar8*)text,
            len);
    
    XGlyphInfo extents;
    XftTextExtentsUtf8(XtDisplay(tf), font, (FcChar8*)text, len, &extents);
    return extents.xOff;
}

static void tfDrawCursor(TextFieldWidget tf) {
    XDrawLine(
            XtDisplay(tf),
            XtWindow(tf),
            tf->textfield.cursorOn ? tf->textfield.gc : tf->textfield.gcInv,
            tf->textfield.posX - tf->textfield.scrollX,
            tf->textfield.textarea_yoff,
            tf->textfield.posX - tf->textfield.scrollX,
            tf->core.height-tf->textfield.textarea_yoff);
}

static void tfRedrawText(TextFieldWidget tf) {  
    tfCalcCursorPos(tf);
    
    int border = tf->primitive.shadow_thickness + tf->primitive.highlight_thickness;
    
    XClearArea(XtDisplay(tf), XtWindow(tf), border, border, tf->core.width - 2*border, tf->core.height - 2*border, False);
    
    int selStart, selEnd, selStartX, selEndX;
    
    // draw selection
    if(tf->textfield.hasSelection) {
        tfSelection(tf, &selStart, &selEnd, &selStartX, &selEndX);
        
        XftDrawRect(
                tf->textfield.d,
                &tf->textfield.foregroundColor,
                selStartX - tf->textfield.scrollX,
                tf->textfield.textarea_yoff,
                selEndX - selStartX,
                tf->core.height - 2*tf->textfield.textarea_yoff);
    }
    
    
    XRectangle rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = tf->core.width - 2 * tf->textfield.textarea_xoff;
    rect.height = tf->core.height;
    XftDrawSetClipRectangles(tf->textfield.d, tf->textfield.textarea_xoff, 0, &rect, 1);
    
    XftFont *font = tf->textfield.font->fonts->font;
    XftColor *color = &tf->textfield.foregroundColor;
    
    const char *buf = tf->textfield.buffer;
    size_t length = tf->textfield.length;
    size_t start = 0;
    
    int xoff = tf->textfield.textarea_xoff;
    
    int charlen = 1;
    int pos = 0;
    for(int i=0;i<length;i+=charlen) {
        FcChar32 c;
        charlen = Utf8ToUcs4(buf + i, &c, length - i);
        
        XftFont *cFont = FindFont(tf->textfield.font, c);
        XftColor *cColor = &tf->textfield.foregroundColor;
        if(tf->textfield.hasSelection) {
            if(i >= selStart && i < selEnd) {
                cColor = &tf->textfield.backgroundColor;
            }
        }
        
        if(c == '\t' || cFont != font || color != cColor) {
            // write chars from start to i-1 with previous font
            size_t drawLen = i - start;
            if(drawLen > 0) {
                xoff += tfDrawString(tf, font, color, xoff, buf + start, drawLen);
                start = i;
            }
            font = cFont;
            color = cColor;
            if(c == '\t') {
                xoff += tfDrawString(tf, font, color, xoff, TF_TAB_STR, sizeof(TF_TAB_STR)-1);
                start++;
            }
        }
        
        pos++;
    }
    int drawLen = length - start;
    if(drawLen > 0) {
        tfDrawString(tf, font, color, xoff, buf + start, drawLen);
    }
    
    tfDrawCursor(tf);
}

static void tfDrawHighlight(TextFieldWidget tf) {
    XmeDrawHighlight(
            XtDisplay(tf),
            XtWindow(tf),
            tf->textfield.hasFocus ? tf->primitive.highlight_GC : tf->textfield.highlightBackground,
            0,
            0,
            tf->core.width,
            tf->core.height,
            tf->primitive.highlight_thickness);
}

void textfield_expose(Widget widget, XEvent* event, Region region) {
    TextFieldWidget tf = (TextFieldWidget)widget;
    
    tfDrawHighlight(tf);
    
    ///*
    XmeDrawShadows(
            XtDisplay(tf),
            XtWindow(tf),
            tf->primitive.bottom_shadow_GC,
            tf->primitive.top_shadow_GC,
            tf->primitive.highlight_thickness,
            tf->primitive.highlight_thickness,
            tf->core.width - (2 * tf->primitive.highlight_thickness),
            tf->core.height - (2 * tf->primitive.highlight_thickness),
            tf->primitive.shadow_thickness,
            XmSHADOW_OUT);
     //*/
    
    tfRedrawText((TextFieldWidget)widget);
}

Boolean textfield_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    Boolean redraw = False;
    
    TextFieldWidget cur = (TextFieldWidget)old;
    TextFieldWidget new = (TextFieldWidget)neww;
    
    if(!new->textfield.font) {
        if(!defaultFont) {
            get_default_font(new, XtDisplay(neww));
        }
        new->textfield.font = defaultFont;
    }
    
    if(cur->textfield.font != new->textfield.font) {
        textfield_recalc_size(new);
        redraw = True;
    }
    
    
    return redraw;
}

Boolean textfield_acceptfocus(Widget widget, Time *time) {
    return 0;
}

static Dimension tfCalcHeight(TextFieldWidget tf) {
    NFont *font = tf->textfield.font;
    Dimension height = font->fonts->font->ascent + font->fonts->font->descent;
    height += 2*(tf->primitive.highlight_thickness + tf->primitive.shadow_thickness) + 2*TF_VPADDING;
    return height;
}

void textfield_recalc_size(TextFieldWidget w) {
    NFont *font = w->textfield.font;
    int height = tfCalcHeight(w);
    int width = w->core.width;
    
    XtMakeResizeRequest((Widget)w, width, height, NULL, NULL);
}


// actions

static void adjustSelection(TextFieldWidget tf, int x) {
    int pos = tfXToPos(tf, x);
    int index = tfPosToIndex(tf, pos);
    
    tf->textfield.selEnd = index;
    tf->textfield.pos = index;
    tf->textfield.selEndX = tfIndexToX(tf, index);
}

static void mouse1DownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    XmProcessTraversal(w, XmTRAVERSE_CURRENT);
    
    int pos = tfXToPos(tf, event->xbutton.x);
    int index = tfPosToIndex(tf, pos);
    tf->textfield.pos = index;
    
    int selStart, selEnd;
    
    Time t = event->xbutton.time;
    int multiclicktime = XtGetMultiClickTime(XtDisplay(w));
    if(t - tf->textfield.btn1ClickPrev2 < 2*multiclicktime) {
        // triple click
        t = 0;
        
        selStart = 0;
        selEnd = tf->textfield.length;
        tf->textfield.pos = selEnd;
        tf->textfield.dontAdjustSel = 1;
    } else if(t - tf->textfield.btn1ClickPrev < multiclicktime) {
        // double click
        
        int wleft, wright;
        wordbounds(tf, index, &wleft, &wright);
        
        selStart = wleft;
        selEnd = wright;
        tf->textfield.pos = wright;
        tf->textfield.dontAdjustSel = 1;
    } else {
        selStart = index;
        selEnd = index;
        
        tf->textfield.dontAdjustSel = 0;
    }
    
    tf->textfield.btn1ClickPrev2 = tf->textfield.btn1ClickPrev;
    tf->textfield.btn1ClickPrev = t;
    
    tfSetSelection(tf, selStart, selEnd);
    
    tfRedrawText(tf);
    
    
}

static void mouse1UpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    if(tf->textfield.dontAdjustSel) return;
    
    adjustSelection(tf, event->xbutton.x);
    
    if(tf->textfield.selStart == tf->textfield.selEnd) {
        tfClearSelection(tf);
    }
    
    tfRedrawText(tf);
}

static void adjustselectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    adjustSelection(tf, event->xbutton.x);
    
    tfRedrawText(tf);
}

static void insertText(TextFieldWidget tf, char *chars, int nchars, XEvent *event) {
    if(nchars == 0) return;
    
    if(tf->textfield.hasSelection) {
        int selStart, selEnd;
        tfSelectionIndex(tf, &selStart, &selEnd);
        TFDelete(tf, selStart, selEnd);
        tfClearSelection(tf);
        tf->textfield.pos = selStart;
    }
    TFInsert(tf, chars, nchars);
    
    // value changed callback
    XmAnyCallbackStruct cb;
    cb.reason = XmCR_VALUE_CHANGED;
    cb.event = event;
    XtCallCallbacks((Widget)tf, XmNvalueChangedCallback, &cb);
    
    
    tfRedrawText(tf);
}

static void insertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    char chars[128];
    KeySym keysym;
    int nchars;
    int status;
    static XComposeStatus compose = {NULL, 0};
    
    if(tf->textfield.xic) {
#ifdef X_HAVE_UTF8_STRING
        nchars = Xutf8LookupString(tf->textfield.xic, &event->xkey, chars, 127, &keysym, &status); 
#else
        nchars = XmbLookupString(tf->textfield.xic, &event->xkey, chars, 127, &keysym, &status);
#endif
    } else {
        nchars = XLookupString(&event->xkey, chars, 127, &keysym, &compose);
    }
    
    insertText(tf, chars, nchars, event);
}

static void actionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    XmAnyCallbackStruct cb;
    cb.reason = XmCR_ACTIVATE;
    cb.event = event;
    XtCallCallbacks((Widget)tf, XmNactivateCallback, &cb);
}

static void deleteText(TextFieldWidget tf, int from, int to, XEvent *event) {
    TFDelete(tf, from, to);
    
    XmAnyCallbackStruct cb;
    cb.reason = XmCR_VALUE_CHANGED;
    cb.event = event;
    XtCallCallbacks((Widget)tf, XmNvalueChangedCallback, &cb);
    
    tf->textfield.pos = from;
    tfRedrawText(tf);
}

static void deletePrevCharAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    int from;
    int to;
    if(tf->textfield.hasSelection) {
        tfSelectionIndex(tf, &from, &to);
        tfClearSelection(tf);
    } else {
        from = TFLeftPos(tf);
        to = tf->textfield.pos;
    }
    
    deleteText(tf, from, to, event);
}

static void deleteNextCharAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    int from;
    int to;
    if(tf->textfield.hasSelection) {
        tfSelectionIndex(tf, &from, &to);
        tfClearSelection(tf);
    } else {
        from = tf->textfield.pos;
        to = TFRightPos(tf);
    }
    
    deleteText(tf, from, to, event);
}

static void wordbounds(TextFieldWidget tf, int index, int *out_wleft, int *out_wright) {
    // is current char space?
    int spc = index < tf->textfield.length ? isspace(tf->textfield.buffer[index]) : 1;

    int wleft, wright;
    
    // get left word bound
    if(out_wleft) {
        for(wleft=index;wleft>=0;wleft--) {
            if(isspace(tf->textfield.buffer[wleft]) != spc) {
                wleft++;
                break;
            }
        }
        if(wleft < 0) wleft = 0;
        *out_wleft = wleft;
    }
    
    // get right word bound
    if(out_wright) {
        for(wright=index;wright<tf->textfield.length;wright++) {
            if(isspace(tf->textfield.buffer[wright]) != spc) {
                break;
            }
        }
        *out_wright = wright;
    }
}

static void deletePrevWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    if(tf->textfield.pos == 0 || tf->textfield.length == 0) return;
    
    int wleft;
    int index = tf->textfield.pos > 0 ? tf->textfield.pos - 1 : 0;
    for(int n=0;n<2;n++) {
        wordbounds(tf, index, &wleft, NULL);
        // in case we found only space, try wordbounds again
        if(wleft == tf->textfield.length || !isspace(tf->textfield.buffer[wleft])) {
            break; // char at wleft is not space
        }
        index = wleft > 0 ? wleft - 1 : 0;
    }
    
    TFDelete(tf, wleft, tf->textfield.pos);
    tf->textfield.pos = wleft;
    tfRedrawText(tf);
}

static void deleteNextWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    if(tf->textfield.pos == tf->textfield.length || tf->textfield.length == 0) return;
    
    int wright;
    int index = tf->textfield.pos;
    wordbounds(tf, index, NULL, &wright);
    
    TFDelete(tf, index, wright);
    tfRedrawText(tf);
}

static void moveSelect(TextFieldWidget tf, String *args, Cardinal *nArgs, int old_pos, int new_pos) {
    if(*nArgs == 1) {
        String d = args[0];
        // select
        if(tf->textfield.hasSelection) {
            if(new_pos == (d[0] == 'l' ? tf->textfield.selStart : tf->textfield.selEnd)) {
                tf->textfield.hasSelection = 0;
            } else if(d[0] == 'r' && new_pos < tf->textfield.selEnd) {
                tfSetSelection(tf, new_pos, tf->textfield.selEnd);
            } else if(new_pos > tf->textfield.selStart) {
                tfSetSelection(tf, tf->textfield.selStart, new_pos);
            } else {
                tfSetSelection(tf, new_pos, tf->textfield.selEnd);
            }
        } else {
            tfSetSelection(tf, old_pos, new_pos);
        }
    } else {
        tf->textfield.hasSelection = 0;
    }
}

static void moveLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    int old_pos = tf->textfield.pos;
    tf->textfield.pos = TFLeftPos(tf);
    moveSelect(tf, args, nArgs, old_pos, tf->textfield.pos);
    tfRedrawText(tf);
}

static void moveRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    int old_pos = tf->textfield.pos;
    tf->textfield.pos = TFRightPos(tf);
    moveSelect(tf, args, nArgs, old_pos, tf->textfield.pos);
    tfRedrawText(tf);
}

static void moveLeftWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    int pos = tf->textfield.pos;
    int old_pos = pos;
    int word = 0; // cursor inside a word?
    while(pos > 0) {
        pos = TFLeftPos(tf);
        
        if(isspace(tf->textfield.buffer[pos])) {
            if(word) {
                break;
            }
        } else {
            word = 1;
        }
        tf->textfield.pos = pos;
    }
    
    int new_pos = tf->textfield.pos;
    
    moveSelect(tf, args, nArgs, old_pos, new_pos);
    
    tfRedrawText(tf);
}

static void moveRightWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    int old_pos = tf->textfield.pos;
    
    int space = 0; // cursor inside a word?
    while(tf->textfield.pos < tf->textfield.length) {
        tf->textfield.pos = TFRightPos(tf);
        
        if(!isspace(tf->textfield.buffer[tf->textfield.pos])) {
            if(space) {
                break;
            }
        } else {
            space = 1;
        }
    }
    int new_pos = tf->textfield.pos;
    
    moveSelect(tf, args, nArgs, old_pos, new_pos);
    
    tfRedrawText(tf);
}

static void blinkCB(XtPointer data, XtIntervalId *id) {
    TextFieldWidget tf = data;
    tf->textfield.cursorOn = !tf->textfield.cursorOn;
    
    tfDrawCursor(tf);
    
    tf->textfield.blinkProcId = XtAppAddTimeOut(
                XtWidgetToApplicationContext((Widget)tf),
                tf->textfield.blinkrate,
                blinkCB,
                tf);
}

static void focusInAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    if(!event->xfocus.send_event) return;
     
    tf->textfield.hasFocus = 1;
    
    if(tf->textfield.xic) {
        XSetICFocus(tf->textfield.xic);
    }
    
    // focus/losingFocus events
    XmAnyCallbackStruct cb;
    cb.reason = XmCR_FOCUS;
    cb.event = event;
    XtCallCallbackList (w, tf->textfield.focusCB, (XtPointer) &cb);
    
    if(tf->textfield.blinkProcId == 0) {
        tf->textfield.blinkProcId = XtAppAddTimeOut(
                XtWidgetToApplicationContext(w),
                tf->textfield.blinkrate,
                blinkCB,
                tf);
    }
    
    Region r = NULL;
    textfield_expose(w, event, r);
}

static void focusOutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    if(tf->textfield.xic) {
        XUnsetICFocus(tf->textfield.xic);
    }
    
    if(tf->textfield.blinkProcId != 0) {
        XtRemoveTimeOut(tf->textfield.blinkProcId);
        tf->textfield.blinkProcId = 0;
    }
    tf->textfield.cursorOn = 1;
    
    tf->textfield.hasFocus = 0;
    
    XmAnyCallbackStruct cb;
    cb.reason = XmCR_LOSING_FOCUS;
    cb.event = event;
    XtCallCallbackList (w, tf->textfield.losingFocusCB, (XtPointer) &cb);
    
    Region r = NULL;
    textfield_expose(w, event, r);
}

static void enterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    
}

static void leaveAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    
}

static void cutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    if(!tf->textfield.hasSelection) return;
    
    int from, to;
    tfSelectionIndex(tf, &from, &to);
    
    size_t len = to - from;
    
    CopyStringToClipboard(w, event->xkey.time, tf->textfield.buffer + from, len);
    TFDelete(tf, from, to);
    tf->textfield.pos = from;
    
    tfRedrawText(tf);
}

static void copyAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    if(!tf->textfield.hasSelection) return;
    
    int from, to;
    tfSelectionIndex(tf, &from, &to);
    
    size_t len = to - from;
    
    CopyStringToClipboard(w, event->xkey.time, tf->textfield.buffer + from, len);
}

static void pasteAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    
    char *clipboard = GetClipboard(w);
    if(!clipboard) return;
    
    int len = strlen(clipboard);
    
    insertText(tf, clipboard, len, event);
    XtFree(clipboard);
}

static void endLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    int old_pos = tf->textfield.pos;
    tf->textfield.pos = tf->textfield.length;
    moveSelect(tf, args, nArgs, old_pos, tf->textfield.pos);
    tfRedrawText(tf);
}

static void beginLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    int old_pos = tf->textfield.pos;
    tf->textfield.pos = 0;
    moveSelect(tf, args, nArgs, 0, old_pos);
    tfRedrawText(tf);
}

static void selectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    tfSetSelection(tf, 0, tf->textfield.length);
    tfRedrawText(tf);
}

static void insertPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    TextFieldWidget tf = (TextFieldWidget)w;
    tfInsertPrimary(tf, event);
}

static int  tfIndexToX(TextFieldWidget tf, int pos) {
    XftFont *font = tf->textfield.font->fonts->font;
    const char *buf = tf->textfield.buffer;
    size_t length = tf->textfield.length;
    size_t start = 0;
    
    XGlyphInfo extents;
    
    int xoff = tf->textfield.textarea_xoff;
    
    int i;
    int charlen = 1;
    for(i=0;i<pos;i+=charlen) {
        FcChar32 c;
        charlen = Utf8ToUcs4(buf + i, &c, length - i);
        
        XftFont *cFont = FindFont(tf->textfield.font, c);
        if(c == '\t' || cFont != font) {
            // write chars from start to i-1 with previous font
            size_t drawLen = i - start;
            if(drawLen > 0) {
                XftTextExtentsUtf8(
                        XtDisplay(tf),
                        font,
                        (FcChar8*)buf + start,
                        drawLen,
                        &extents
                        );
                xoff += extents.xOff;
                start = i;
            }
            font = cFont;
            if(c == '\t') {
                start++;
                XftTextExtentsUtf8(
                        XtDisplay(tf),
                        font,
                        (FcChar8*)TF_TAB_STR,
                        sizeof(TF_TAB_STR)-1,
                        &extents
                        );
                xoff += extents.xOff;
            }
        }
    }
    int drawLen = i - start;
    if(drawLen > 0) {
        XftTextExtentsUtf8(
                XtDisplay(tf),
                font,
                (FcChar8*)buf + start,
                drawLen,
                &extents
                );
        xoff += extents.xOff;
    }
    
    return xoff;
}

static void tfSelection(TextFieldWidget tf, int *start, int *end, int *startX, int *endX) {
    int selStart, selEnd, selStartX, selEndX; 
    if(tf->textfield.selStart > tf->textfield.selEnd) {
        selStart  = tf->textfield.selEnd;
        selEnd    = tf->textfield.selStart;
        selStartX = tf->textfield.selEndX;
        selEndX   = tf->textfield.selStartX;
    } else {
        selEnd    = tf->textfield.selEnd;
        selStart  = tf->textfield.selStart;
        selEndX   = tf->textfield.selEndX;
        selStartX = tf->textfield.selStartX;
    }
    
    if(start)  *start  = selStart;
    if(end)    *end    = selEnd;
    if(startX) *startX = selStartX;
    if(endX)   *endX   = selEndX;
}

static void tfSelectionIndex(TextFieldWidget tf, int *start, int *end) {
    tfSelection(tf, start, end, NULL, NULL);
}

static void tfSetSelection(TextFieldWidget tf, int from, int to) {
    if(!tf->textfield.hasSelection) {
        XtOwnSelection((Widget)tf, XA_PRIMARY, XtLastTimestampProcessed(XtDisplay((Widget)tf)), convertSelection, loseSelection, NULL);
    }
    
    if(from > to) {
        int f = from;
        from = to;
        to = f;
    }
    
    tf->textfield.hasSelection = 1;
    tf->textfield.selStart = from;
    tf->textfield.selEnd = to > tf->textfield.length ? tf->textfield.length : to;
    tf->textfield.selStartX = tfIndexToX(tf, tf->textfield.selStart);
    tf->textfield.selEndX = tfIndexToX(tf, tf->textfield.selEnd);
}

static void tfClearSelection(TextFieldWidget tf) {
    tf->textfield.hasSelection = 0;
    tf->textfield.selStart = 0;
    tf->textfield.selEnd = 0;
}

static void tfCalcCursorPos(TextFieldWidget tf) {
    if(tf->textfield.pos == tf->textfield.posCalc) return;
    
    int xoff = tfIndexToX(tf, tf->textfield.pos);

    tf->textfield.posX = xoff;
    tf->textfield.posCalc = tf->textfield.pos;
    
    int posX = tf->textfield.posX;
    int margin = tf->textfield.textarea_xoff;
    if(posX > tf->core.width + tf->textfield.scrollX - margin) {
        tf->textfield.scrollX = -tf->core.width + posX + margin;
    } else if(posX < tf->textfield.scrollX + margin) {
        tf->textfield.scrollX = posX - tf->textfield.textarea_xoff;
    }
}

static int  tfPosToIndex(TextFieldWidget tf, int pos) {
    const char *buf = tf->textfield.buffer;
    size_t length = tf->textfield.length;
    
    int p = 0;
    int charlen = 1;
    int i;
    for(i=0;i<length;i+=charlen) {
        if(p == pos) break;
        
        FcChar32 c;
        charlen = Utf8ToUcs4(buf + i, &c, length - i);
        
        p++;
    }
    return i;
}

static int tfXToPos(TextFieldWidget tf, int x) {
    const char *buf = tf->textfield.buffer;
    size_t length = tf->textfield.length;
    
    x += tf->textfield.scrollX;
    
    int pos = 0;
    
    XGlyphInfo extents;
    
    int xoff = tf->textfield.textarea_xoff;
    
    int charlen = 1;
    for(int i=0;i<length;i+=charlen) {
        FcChar32 c;
        charlen = Utf8ToUcs4(buf + i, &c, length - i);
        
        const char *str;
        size_t slen;
        if(c == '\t') {
            str = TF_TAB_STR;
            slen = sizeof(TF_TAB_STR)-1;
        } else {
            str = buf+i;
            slen = charlen;
        }
        
        XftFont *font = FindFont(tf->textfield.font, c);
        XftTextExtentsUtf8(
                XtDisplay(tf),
                font,
                (FcChar8*)str,
                slen,
                &extents
                );
        xoff += extents.xOff;
        if(xoff > x + (extents.xOff / 2)) {
            break;
        }
        pos++;
    }
    
    return pos;
}


// ---------------- text buffer functions --------------------

static void TFInsert(TextFieldWidget tf, const char *chars, size_t nchars) {
    // realloc buffer if needed
    if(tf->textfield.length + nchars >= tf->textfield.alloc) {
        tf->textfield.alloc += TF_BUF_BLOCK;
        tf->textfield.buffer = XtRealloc(tf->textfield.buffer, tf->textfield.alloc);
    }
    
    if(tf->textfield.pos == tf->textfield.length) {
        // append
        memcpy(tf->textfield.buffer + tf->textfield.length, chars, nchars);
    } else {
        // insert
        char *insertpos = tf->textfield.buffer + tf->textfield.pos;
        memmove(insertpos + nchars, insertpos, tf->textfield.length - tf->textfield.pos);
        memmove(insertpos, chars, nchars);
    }
    
    tf->textfield.length += nchars;
    tf->textfield.pos += nchars;
    
    tfClearSelection(tf);
}

static int TFLeftPos(TextFieldWidget tf) {
    int left = 0;
    int cur = 0;
    while(cur < tf->textfield.pos) {
        left = cur;
        cur += Utf8CharLen((unsigned char*)tf->textfield.buffer + cur);
    }
    return left;
}

static int TFRightPos(TextFieldWidget tf) {
    int pos = tf->textfield.pos;
    int right = pos + Utf8CharLen((unsigned char*)tf->textfield.buffer + pos);
    return right > tf->textfield.length ? tf->textfield.length : right;
}

static void TFDelete(TextFieldWidget tf, int from, int to) {
    if(from >= to) return;
    
    if(to >= tf->textfield.length) {
        tf->textfield.length = from;
        return;
    }
    
    int len = tf->textfield.length - to;
    memmove(tf->textfield.buffer + from, tf->textfield.buffer + to, len);
    
    tf->textfield.length -= to - from;
    
    tfClearSelection(tf);
}


struct PSelection {
    TextFieldWidget tf;
    XEvent *event;
    char *xastring;
    char *utf8string;
    int target;
};

static void getPrimary(
        Widget w,
        XtPointer clientData,
        Atom *selType,
	Atom *type,
        XtPointer value,
        unsigned long *length,
        int *format)
{
    struct PSelection *sel = clientData;
    sel->target++;
    
    if(value && *format == 8 && *length > 0 && (*type == XA_STRING || *type == aUtf8String)) {
        char *str = XtMalloc((*length) + 1);
        memcpy(str, value, *length);
        str[*length] = 0;
        
        if(*type == aUtf8String) {
            sel->utf8string = str;
        } else {
            sel->xastring = str;
        }
    }
    
    if(sel->target == 2) {
        char *insert = sel->utf8string ? sel->utf8string : sel->xastring;
        if(insert) {
            insertText(sel->tf, insert, strlen(insert), sel->event);
        }
        
        if(sel->utf8string) {
            XtFree(sel->utf8string);
        }
        if(sel->xastring) {
            XtFree(sel->xastring);
        }
        XtFree((void*)sel);
    }
}

static void tfInsertPrimary(TextFieldWidget tf, XEvent *event) {
    struct PSelection *sel = (void*)XtMalloc(sizeof(struct PSelection));
    sel->tf = tf;
    sel->event = event;
    sel->target = 0;
    sel->xastring = NULL;
    sel->utf8string = NULL;
    
    Atom targets[2] = {aUtf8String, XA_STRING};
    Time time = XtLastTimestampProcessed(XtDisplay((Widget)tf));
    
    void *data[2] = { sel, sel };
    
#ifdef __APPLE__
    XtGetSelectionValue((Widget)tf, XA_PRIMARY, targets[0], getPrimary, sel, time);
    XtGetSelectionValue((Widget)tf, XA_PRIMARY, targets[1], getPrimary, sel, time);
#else
    XtGetSelectionValues((Widget)tf, XA_PRIMARY, targets, 2, getPrimary, data, time);
#endif
}


// Atoms: aTargets, XA_STRING, aUtf8String

static Boolean convertSelection(
        Widget w,
        Atom *seltype,
        Atom *target,
        Atom *type,
        XtPointer *value,
        unsigned long *length,
        int *format)
{
    TextFieldWidget tf = (TextFieldWidget)w;
    
    if(*target == aTargets) {
        Atom *retTargets = calloc(3, sizeof(Atom));
        retTargets[0] = XA_STRING;
        retTargets[1] = aUtf8String;
        retTargets[2] = aTargets;
        *type = XA_ATOM;
	*value = retTargets;
	*length = 3;
	*format = 32;
        return True;
    }
    
    if(*target == XA_STRING || *target == aUtf8String) {
        char *selectedText = NULL;
        size_t len = 0;
        
        if(tf->textfield.hasSelection) {
            int from, to;
            tfSelectionIndex(tf, &from, &to);
            len = to - from;
            selectedText = XtMalloc(len + 1);
            memcpy(selectedText, tf->textfield.buffer + from, len);
            selectedText[len] = 0;
        } else {
            selectedText = XtMalloc(4);
            selectedText[0] = 0;
        }
        
        *type = *target == aUtf8String ? aUtf8String : XA_STRING;
        *value = selectedText;
        *length = len;
        *format = 8;
        return True;
    }
    
    return False;
}

static void loseSelection(Widget w, Atom *type) {
    TextFieldWidget tf = (TextFieldWidget)w;
    tfClearSelection(tf);
    tfRedrawText(tf);
}


// --------------------- public API --------------------------

void XNETextFieldSetString(Widget widget, char *value) {
    if(!value) {
        value = "";
    }
    
    size_t len = strlen(value);
    
    TextFieldWidget tf = (TextFieldWidget)widget;
    if(len > tf->textfield.alloc) {
        size_t alloc = len + TF_BUF_BLOCK - (len % TF_BUF_BLOCK);
        tf->textfield.buffer = XtRealloc(tf->textfield.buffer, alloc);
        tf->textfield.alloc = alloc;
    }
    
    memcpy(tf->textfield.buffer, value, len);
    
    tf->textfield.length = len;
    tf->textfield.pos = 0;
    
    tfClearSelection(tf);
    if(XtIsRealized(widget)) {
        tfRedrawText(tf);
    } 
}

char* XNETextFieldGetString(Widget widget) {
    TextFieldWidget tf = (TextFieldWidget)widget;
    
    char *r = XtMalloc(tf->textfield.length + 1);
    memcpy(r, tf->textfield.buffer, tf->textfield.length);
    r[tf->textfield.length] = '\0';
    return r;
}

XmTextPosition XNETextFieldGetLastPosition(Widget widget) {
    TextFieldWidget tf = (TextFieldWidget)widget;
    return tf->textfield.length;
}

void XNETextFieldSetInsertionPosition(Widget widget, XmTextPosition i) {
    TextFieldWidget tf = (TextFieldWidget)widget;
    tf->textfield.pos = i <= tf->textfield.length ? i : tf->textfield.length;
    if(XtIsRealized(widget)) {
        tfRedrawText(tf);
    }
} 

void XNETextFieldSetSelection(Widget w, XmTextPosition first, XmTextPosition last, Time sel_time) {
    TextFieldWidget tf = (TextFieldWidget)w;
    tfSetSelection(tf, first, last);
    if(XtIsRealized(w)) {
        tfRedrawText(tf);
    }
}
