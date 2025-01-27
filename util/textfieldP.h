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

#ifndef XNE_TEXTFIELDP_H
#define XNE_TEXTFIELDP_H

#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include <X11/CoreP.h>

#include "../source/textDisp.h" /* NFont */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TextFieldClassPart {
    int unused;
} TextFieldClassPart;

typedef struct TextFieldClassRec {
    CoreClassPart        core_class;
    XmPrimitiveClassPart primitive_class;
    TextFieldClassPart   textfield_class;
} TextFieldClassRec;

extern TextFieldClassRec nTextFieldClassRec;

typedef struct TextFieldPart {
    XtCallbackList valueChangedCB;
    XtCallbackList focusCB;
    XtCallbackList losingFocusCB;
    XtCallbackList activateCB;
    
    int            hasFocus;
    
    char           *renderTable;
    NFont          *font;    
    XIM            xim;
    XIC            xic;
    GC             gc;
    GC             gcInv;
    GC             highlightBackground;
    
    XftColor       foregroundColor;
    XftColor       backgroundColor;
    XftDraw        *d;
    
    char           *buffer;
    size_t         alloc;
    size_t         length;
    size_t         pos;
    
    int            posCalc;
    int            posX;
    
    int            scrollX;
    
    int            textarea_xoff;
    int            textarea_yoff;
    
    int            hasSelection;
    int            selStart;
    int            selEnd;
    
    int            selStartX;
    int            selEndX;
    
    Time           btn1ClickPrev;
    Time           btn1ClickPrev2;
    int            dontAdjustSel;
    
    int            blinkrate;
    int            cursorOn;
    XtIntervalId   blinkProcId;
    short          columns;
} TextFieldPart;

typedef struct TextFieldRec {
   CorePart        core;
   XmPrimitivePart primitive;
   TextFieldPart   textfield;
} TextFieldRec;


#ifdef __cplusplus
}
#endif

#endif /* XNE_TEXTFIELDP_H */

