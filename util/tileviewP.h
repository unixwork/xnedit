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

#include <Xft/Xft.h>

#include "tileview.h"

typedef struct TileFieldClassPart {
    int unused;
} TileViewClassPart;

typedef struct TileFieldClassRec {
    CoreClassPart        core_class;
    XmPrimitiveClassPart primitive_class;
    TileViewClassPart   tileview_class;
} TileViewClassRec;


typedef struct TileViewPart {
    XtCallbackList focusCB;
    XtCallbackList activateCB;
    XtCallbackList realizeCB;
    
    TileDrawFunc   drawFunc;
    void           *drawData;
    
    void           **data;
    int            length;
    
    int            tileWidth;
    int            tileHeight;
    
    int            selection;
    
    Boolean        recalcSize;
    
    GC             gc;
    XftDraw        *d;
} TileViewPart;

typedef struct TileViewRec {
   CorePart        core;
   XmPrimitivePart primitive;
   TileViewPart    tileview;
} TileViewRec;

typedef struct TileViewRec *TileViewWidget;

#endif /* XNE_TILEVIEWP_H */

