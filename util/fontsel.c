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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Xm/XmAll.h>

#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>

#include "nedit_malloc.h"
#include "misc.h"

#include "fontsel.h"

#define PREVIEW_STR "ABCDEFGHIJabcdefghijklmn[](){}.:,;-_$%&/\"'"

#define MONOSPACE_ONLY_DEFAULT 1

typedef struct FontSelector {
    FcPattern *filter;
    FcFontSet *list;
    
    Widget pattern;
    Widget fontlist;
    Widget size;
    Widget preview;
    Widget name;
    
    XftFont *font;
    XftFont *bold;
    XftFont *italic;
    XftDraw *draw;
    
    int selected_item;
    int end;
    int cancel;
} FontSelector;

static void UpdatePreview(FontSelector *sel, const char *fontStr)
{
    Display *dp = XtDisplay(sel->preview);
    XftFont *font = XftFontOpenName(dp, DefaultScreen(dp), fontStr);
    if(!font) {
        return;
    }
    if(sel->font) {
        XftFontClose(dp, sel->font);
    }
    
    size_t fontStrLen = strlen(fontStr);
    char *boldFontStr = FontNameAddAttribute(fontStr, fontStrLen,
                                             "weight", "bold");
    char *italicFontStr = FontNameAddAttribute(fontStr, fontStrLen,
                                               "slant", "italic");
    
    XftFont *boldFont = XftFontOpenName(dp, DefaultScreen(dp), boldFontStr);
    XftFont *italicFont = XftFontOpenName(dp, DefaultScreen(dp), italicFontStr);
    
    NEditFree(boldFontStr);
    NEditFree(italicFontStr);
    
    if(sel->bold) {
        XftFontClose(dp, sel->bold);
    }
    if(sel->italic) {
        XftFontClose(dp, sel->italic);
    }
    
    sel->font = font;
    sel->bold = boldFont;
    sel->italic = italicFont;
}

static void InitXftDraw(FontSelector *sel)
{
    if(sel->draw) {
        return;
    }
    
    XWindowAttributes attributes;
    XGetWindowAttributes(XtDisplay(sel->preview), XtWindow(sel->preview), &attributes); 
    Screen *screen = XtScreen(sel->preview);
    Visual *visual = screen->root_visual;
    
    Cardinal depth;
    Colormap colormap;
    XtVaGetValues(sel->preview, XtNdepth, &depth, XtNcolormap, &colormap, NULL);
    
    for(int i=0;i<screen->ndepths;i++) {
        Depth d = screen->depths[i];
        if(d.depth == depth) {
            visual = d.visuals;
            break;
        }
    }
    
    Display *dp = XtDisplay(sel->preview);
    sel->draw = XftDrawCreate(
            dp,
            XtWindow(sel->preview),
            visual,
            colormap);
}

static void exposeFontPreview(Widget w, FontSelector *sel, XtPointer data)
{
    InitXftDraw(sel);
    if(!sel->font) {
        return;
    }
    
    XClearWindow(XtDisplay(w), XtWindow(w));
    
    XftColor color;
    color.color.red = 0;
    color.color.green = 0;
    color.color.blue = 0;
    color.color.alpha = 0xFFFF;
    
    Dimension width, height;
    XtVaGetValues(
            w,
            XmNwidth,
            &width,
            XmNheight,
            &height,
            NULL);
    
    int fontHeight = sel->font->ascent + sel->font->descent;
    
    int boldHeight = 0;
    int italicHeight = 0;
    int extraSpace = 0;
    if(sel->bold) {
        boldHeight = sel->bold->ascent + sel->bold->descent;
        extraSpace += 5;
    }
    if(sel->italic) {
        italicHeight = sel->italic->ascent + sel->italic->descent;
        extraSpace += 5;
    }
    
    int space = height - fontHeight - boldHeight - italicHeight - extraSpace;
    
    int y = space/2;
    XftDrawStringUtf8(
            sel->draw,
            &color,
            sel->font,
            10,
            y + sel->font->ascent,
            (unsigned char*) PREVIEW_STR,
            sizeof(PREVIEW_STR)-1);
    y += fontHeight + 5;
    
    if(sel->bold) {
        XftDrawStringUtf8(
            sel->draw,
            &color,
            sel->bold,
            10,
            y + sel->bold->ascent,
            (unsigned char*) PREVIEW_STR,
            sizeof(PREVIEW_STR)-1);
        y += boldHeight + 5;
    }
    if(sel->italic) {
        XftDrawStringUtf8(
            sel->draw,
            &color,
            sel->italic,
            10,
            y + sel->italic->ascent,
            (unsigned char*) PREVIEW_STR,
            sizeof(PREVIEW_STR)-1);
    }
}

static int compare_font(const void *d1, const void *d2) {
    const FcPattern *f1 = *((const FcPattern**)d1);
    const FcPattern *f2 = *((const FcPattern**)d2);
    
    FcChar8 *name1 = NULL;
    FcChar8 *name2 = NULL;
    FcPatternGetString(f1, FC_FULLNAME, 0, &name1);
    FcPatternGetString(f2, FC_FULLNAME, 0, &name2);
    
    FcChar8 *family1 = NULL;
    FcChar8 *family2 = NULL;
    FcPatternGetString(f1, FC_FAMILY, 0, &family1);
    FcPatternGetString(f2, FC_FAMILY, 0, &family2);
    
    if(name1 && name2) {
        return strcmp((char*)name1, (char*)name2);
    } else if(family1 && family2) {
        return strcmp((char*)family1, (char*)family2);
    } else {
        return 0;
    }
}

static char* CreateFontName(FcChar8 *family, FcChar8 *style) {
    size_t flen = family ? strlen((char*)family) : 0;
    size_t slen = style ? strlen((char*)style) : 0;
    
    size_t len = flen + slen + 4;
    char *name = NEditMalloc(len);
    
    if(!family) {
        snprintf(name, len, "-");
    } else if(style) {
        snprintf(name, len, "%s %s", (char*)family, (char*)style);
    } else {
        snprintf(name, len, "%s", (char*)family);
    }
    return name;
}

static void UpdateFontList(FontSelector *sel, const char *pattern)
{
    XmStringTable items;
    FcChar8 *name;
    FcChar8 *family;
    FcChar8 *style;
    int nfonts, nfound;
      
    if(pattern) {
        if(sel->filter) {
            FcPatternDestroy(sel->filter);
            sel->filter = NULL;
        }
        sel->filter = FcNameParse((FcChar8*)pattern);
    }
    if(!sel->filter) {
        sel->filter = FcPatternCreate();
    }
    
    FcObjectSet *os = FcObjectSetCreate();
    FcObjectSetAdd(os, FC_FAMILY);
    FcObjectSetAdd(os, FC_STYLE);
    FcObjectSetAdd(os, FC_FULLNAME);
    FcObjectSetAdd(os, FC_SCALABLE);
    FcObjectSetAdd(os, FC_SPACING);
    
    if(sel->list) {
        FcFontSetDestroy(sel->list);
    }
    sel->list = FcFontList(NULL, sel->filter, os);
    FcObjectSetDestroy(os);
    
    nfonts = sel->list->nfont;
    nfound = 0;
    
    // sort fonts
    qsort(sel->list->fonts, nfonts, sizeof(FcPattern*), compare_font);
    
    items = NEditCalloc(sel->list->nfont, sizeof(XmString));
    for(int i=0;i<nfonts;i++) {
        FcBool scalable = 0;
        FcPatternGetBool(sel->list->fonts[i], FC_SCALABLE, 0, &scalable);
        int spacing = 0;
        FcPatternGetInteger(sel->list->fonts[i], FC_SPACING, 0, &spacing);
        
        name = NULL;
        FcPatternGetString(sel->list->fonts[i], FC_FULLNAME, 0, &name);
        
        family = NULL;
        FcPatternGetString(sel->list->fonts[i], FC_FAMILY, 0, &family);
        
        style = NULL;
        FcPatternGetString(sel->list->fonts[i], FC_STYLE, 0, &style);
        
        if(name) {
            items[nfound] = XmStringCreateSimple((char*)name);
        } else {
            name = (FcChar8*)CreateFontName(family, style);
            items[nfound] = XmStringCreateSimple((char*)name);
            NEditFree(name);
        }
        
        nfound++;
    }
    
    XtVaSetValues(sel->fontlist, XmNitems, items, XmNitemCount, nfound, NULL);
    for(int i=0;i<nfound;i++) {
        XmStringFree(items[i]);
    }
    NEditFree(items);
}

static void MatchFont(const char *name, XmString *retName, XmString *retSize)
{
    *retName = NULL;
    *retSize = NULL;
    
    FcPattern* pat = FcNameParse((const FcChar8*)name);
    
    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult result;
    FcPattern* font = FcFontMatch(NULL, pat, &result);
    if(font) {
        FcChar8* nameStr = NULL;
        FcChar8* familyStr = NULL;
        double fontSize = 0;
        if (FcPatternGetString(font, FC_FULLNAME, 0, &nameStr) == FcResultMatch) {
            *retName = XmStringCreateSimple((char*)nameStr);
        } else if (FcPatternGetString(font, FC_FAMILY, 0, &familyStr) == FcResultMatch) {
            FcChar8 *styleStr = NULL;
            FcPatternGetString(font, FC_STYLE, 0, &styleStr);
            if(styleStr) {
                char *fontName = CreateFontName(familyStr, styleStr);
                *retName = XmStringCreateSimple(fontName);
                NEditFree(fontName);
            }
        }
        if(FcPatternGetDouble(font, FC_SIZE, 0, &fontSize) == FcResultMatch) {
            char buf[8];
            snprintf(buf, 8, "%d", (int)fontSize);
            *retSize = XmStringCreateSimple(buf);
        }
        FcPatternDestroy(font);
    }
    FcPatternDestroy(pat);
}

static void CreateSizeList(Widget w)
{
    XmStringTable items = NEditCalloc(30, sizeof(XmString));
    char buf[8];
    for(int i=0;i<30;i++) {
        snprintf(buf, 8, "%d", i+5);
        items[i] = XmStringCreateSimple(buf);
    }
    
     XtVaSetValues(w, XmNitems, items, XmNitemCount, 30, NULL);
     for(int i=0;i<30;i++) {
         XmStringFree(items[i]);
     }
     NEditFree(items);
}

static char* GetFontString(FontSelector *sel)
{
    XmStringTable table = NULL;
    int count = 0;
    int i = sel->selected_item - 1;
    char *font = NULL;
    char *style = NULL;
    char *size = NULL;
    size_t fontLen, styleLen, sizeLen, outLen;
    char *out;
    
    if(i < 0) return NULL;
    
    FcPatternGetString(sel->list->fonts[i], FC_FAMILY, 0, (FcChar8**)&font);
    FcPatternGetString(sel->list->fonts[i], FC_STYLE, 0, (FcChar8**)&style);
    
    XtVaGetValues(
            sel->size,
            XmNselectedItems,
            &table,
            XmNselectedItemCount,
            &count,
            NULL);
    if(count > 0) {
        XmStringGetLtoR(table[0], XmFONTLIST_DEFAULT_TAG, &size);
    }
    
    if(style && !strcmp(style, "Regular")) {
        style = NULL;
    }
     
    fontLen = font ? strlen(font) : 0;
    styleLen = style ? strlen(style) : 0;
    sizeLen = size ? strlen(size) : 0;
    
    if(style && size) {
        outLen = fontLen + styleLen + sizeLen + 16;
        out = XtMalloc(outLen);
        snprintf(out, outLen, "%s:style=%s:size=%s", font, style, size);
        XmTextSetString(sel->name, out);
        XtFree(size);
        return out;
    } else if(size) {
        outLen = fontLen + sizeLen + 16;
        out = XtMalloc(outLen);
        snprintf(out, outLen, "%s:size=%s", font, size);
        XmTextSetString(sel->name, out);
        XtFree(size);
        return out;
    } else {
        return font;
    }
}

static void UpdateFontName(FontSelector *sel)
{
    char *fontStr = GetFontString(sel);
    if(fontStr) {
        UpdatePreview(sel, fontStr);
        exposeFontPreview(sel->preview, sel, NULL);
        XmTextSetString(sel->name, fontStr);
        XtFree(fontStr);
    }
}

static void fontlist_callback(Widget w, FontSelector *sel, XmListCallbackStruct *cb)
{
    sel->selected_item = cb->item_position;
    UpdateFontName(sel);
}

static void SetMonospaceFilter(FontSelector *sel, Boolean on)
{
    if(sel->filter) {
        FcPatternDestroy(sel->filter);
    }
    sel->filter = FcPatternCreate();
    if(on) {
        FcPatternAddInteger(sel->filter, FC_SPACING, FC_MONO);
    }
}

void fontlist_toggle_monospace(Widget w, FontSelector *sel, XmToggleButtonCallbackStruct *cb)
{
    SetMonospaceFilter(sel, cb->set);
    UpdateFontList(sel, NULL);
}

void size_callback (Widget w, FontSelector *sel, XtPointer data)
{
    UpdateFontName(sel);
}

static void ok_callback(Widget w, FontSelector *sel, XtPointer data)
{
    sel->end = 1;
}

static void cancel_callback(Widget w, FontSelector *sel, XtPointer data)
{
    sel->end = 1;
    sel->cancel = 1;
}

static void FreeFontSelector(FontSelector *sel)
{
    FcPatternDestroy(sel->filter);
    Display *dp = XtDisplay(sel->preview);
    
    XftDrawDestroy(sel->draw);
    if(sel->font) {
        XftFontClose(dp, sel->font);
    }
    if(sel->bold) {
        XftFontClose(dp, sel->bold);
    }
    if(sel->italic) {
        XftFontClose(dp, sel->italic);
    }
    if(sel->list) {
        FcFontSetDestroy(sel->list);
    }
    
    NEditFree(sel);
}

char *FontSel(Widget parent, const char *curFont)
{
    Arg args[32];
    int n = 0;
    XmString str;
    
    FontSelector *sel = NEditMalloc(sizeof(FontSelector));
    memset(sel, 0, sizeof(FontSelector));
    
    Widget dialog = CreateDialogShell(parent, "Font Selector", args, 0);
    AddMotifCloseCallback(dialog, (XtCallbackProc)cancel_callback, sel);
    Widget form = XmCreateForm(dialog, "form", args, 0);
    
    /* ok button */
    n = 0;
    str = XmStringCreateSimple("OK");
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNbottomOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget okButton = XmCreatePushButton(form, "button", args, n);
    XtManageChild(okButton);
    XmStringFree(str);
    XtAddCallback(
                okButton,
                XmNactivateCallback,
                (XtCallbackProc)ok_callback,
                sel);
    
    /* cancel button */
    n = 0;
    str = XmStringCreateSimple("Cancel");
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNbottomOffset, 5); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget cancelButton = XmCreatePushButton(form, "button", args, n);
    XtManageChild(cancelButton);
    XmStringFree(str);
    XtAddCallback(
                cancelButton,
                XmNactivateCallback,
                (XtCallbackProc)cancel_callback,
                sel);
    
    /* font name */
    n = 0;
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNwidth, 400); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, okButton); n++;
    sel->name = XmCreateText(form, "fcname_textfield", args, n);
    XtManageChild(sel->name);
    
    /* font name label */
    n = 0;
    str = XmStringCreateSimple("Font name:");
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, sel->name); n++;
    Widget fontNameLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(fontNameLabel);
    XmStringFree(str);
    
    /* preview */
    n = 0;
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, fontNameLabel); n++;
    XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
    XtSetArg(args[n], XmNmarginWidth, 3); n++;
    XtSetArg(args[n], XmNmarginHeight, 3); n++;
    Widget previewFrame = XmCreateFrame(form, "frame", args, n);
    XtManageChild(previewFrame);
    
    n = 0;
    sel->preview = XmCreateDrawingArea(previewFrame, "fontpreview", args, n);
    Dimension w, h;
    XtMakeResizeRequest(sel->preview, 450, 180, &w, &h);
    XtManageChild(sel->preview);
    
    XtAddCallback(
            sel->preview,
            XmNexposeCallback,
            (XtCallbackProc)exposeFontPreview,
            sel);
    
    /* toggle monospace */
    n = 0;
    str = XmStringCreateSimple("Show only non-proportional fonts");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, 5); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNset, MONOSPACE_ONLY_DEFAULT); n++;
    Widget toggleMonospace = XmCreateToggleButton(form, "toggle_monospace", args, n);
    XtManageChild(toggleMonospace);
    XmStringFree(str);
    XtAddCallback(
            toggleMonospace,
            XmNvalueChangedCallback,
            (XtCallbackProc)fontlist_toggle_monospace,
            sel);
    
    /* label */
    n = 0;
    str = XmStringCreateSimple("Font:");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, toggleMonospace); n++;
    XtSetArg(args[n], XmNtopOffset, 5); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget fontListLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(fontListLabel);
    XmStringFree(str);
    
    n = 0;
    str = XmStringCreateSimple("Size:");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, toggleMonospace); n++;
    XtSetArg(args[n], XmNtopOffset, 5); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget fontSizeLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(fontSizeLabel);
    XmStringFree(str);
    
    /* font list */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, fontListLabel); n++;
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, previewFrame); n++;
    XtSetArg(args[n], XmNvisibleItemCount, 10); n++;
    sel->fontlist = XmCreateScrolledList(form, "fontlist", args, n);
    AddMouseWheelSupport(sel->fontlist);
    XtManageChild(sel->fontlist);
    XtAddCallback(
                sel->fontlist,
                XmNbrowseSelectionCallback,
                (XtCallbackProc)fontlist_callback,
                sel);
    
    /* font size list */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, fontSizeLabel); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, sel->fontlist); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, previewFrame); n++;
    XtSetArg(args[n], XmNleftOffset, 2); n++;
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    sel->size = XmCreateScrolledList(form, "sizelist", args, n);
    CreateSizeList(sel->size);
    XtAddCallback (sel->size, XmNbrowseSelectionCallback, (XtCallbackProc)size_callback, sel);
    XmListSelectPos(sel->size, 6, 0);
    XtManageChild(sel->size);
    
    /*
    CreateSizeList(sel->size);
    XtManageChild(sel->size);
    str = XmStringCreateSimple("10");
    XmComboBoxSelectItem(sel->size, str);
    XtAddCallback (sel->size, XmNselectionCallback, (XtCallbackProc)size_callback, sel);
    XmStringFree(str);
    */
    
    UpdatePreview(sel, curFont);
    
    SetMonospaceFilter(sel, MONOSPACE_ONLY_DEFAULT);
    UpdateFontList(sel, NULL);
    XmString fontSelection;
    XmString sizeSelection;
    MatchFont(curFont, &fontSelection, &sizeSelection);
    if(fontSelection) {
        XmListSelectItem(sel->fontlist, fontSelection, 0);
        XmStringFree(fontSelection);
        int *pos;
        int selcount = 0;
        if(XmListGetSelectedPos(sel->fontlist, &pos, &selcount)) {
            sel->selected_item = pos[0];
            XtFree((void*)pos);
        }
    }
    if(sizeSelection) {
        XmListSelectItem(sel->size, sizeSelection, 0);
        XmStringFree(sizeSelection);
    }
    
    ManageDialogCenteredOnPointer(form);
    
    XtAppContext app = XtWidgetToApplicationContext(dialog);
    while(!sel->end && !XtAppGetExitFlag(app)) {
        XEvent event;
        XtAppNextEvent(app, &event);
        XtDispatchEvent(&event);
    }
    
    XtDestroyWidget(dialog);
    char *retStr = NULL;
    if(!sel->cancel) {
        retStr = GetFontString(sel);
    }
    if(!retStr) {
        retStr = NEditStrdup(curFont);
    }
    
    FreeFontSelector(sel);
    return retStr;
}


char* FontNameAddAttribute(
        const char *name,
        size_t len,
        const char *attribute,
        const char *value)
{
    size_t attributelen = strlen(attribute);
    size_t valuelen = strlen(value);
    size_t newlen = len + attributelen + valuelen + 4;
    char *attr = NEditMalloc(attributelen+3);
    char *newfont = NEditMalloc(newlen);
    char *oldattr;
    int i = len;
    int b = 0;
    int e = 0;
    
    /* check if the font name already has this attribute */
    attr[0] = ':';
    memcpy(attr+1, attribute, attributelen);
    attr[attributelen+1] = '=';
    attr[attributelen+2] = '\0';
    oldattr = strstr(name, attr);
    if(oldattr) {
        b = (int)(oldattr - name)+1;
        e = len;
        for(i=b;i<len;i++) {
            if(name[i] == ':') {
                e = i;
                break;
            }
        }
    }
    NEditFree(attr);
    
    if(b < len) {
        if(b > 0 && name[b-1] == ':') b--;
        snprintf(newfont, newlen, "%.*s%.*s:%s=%s", b, name, (int)(len-e), name+e, attribute, value);
    } else {
        snprintf(newfont, newlen, "%s:%s=%s", name, attribute, value);
    }
    
    return newfont;
}
