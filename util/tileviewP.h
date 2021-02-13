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

#ifndef XNE_TILEVIEWP_H
#define XNE_TILEVIEWP_H

#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include <X11/CoreP.h>

#include "tileview.h"

typedef struct TextFieldClassPart {
    int unused;
} TileViewClassPart;

typedef struct TextFieldClassRec {
    CoreClassPart        core_class;
    XmPrimitiveClassPart primitive_class;
    TextFieldClassPart   tileview_class;
} TileViewClassRec;

//extern TileViewClassRec nTextFieldClassRec;

typedef struct TileViewPart {
    XtCallbackList focusCB;
    XtCallbackList activateCB;
    XtCallbackList realizeCB;
    
    TileDrawFunc   drawFunc;
    void           *userData;
    
    void           **data;
    long           length;
} TextFieldPart;

typedef struct TextFieldRec {
   CorePart        core;
   XmPrimitivePart primitive;
   TileViewPart    tileview;
} TileViewRec;

#endif /* XNE_TILEVIEWP_H */

