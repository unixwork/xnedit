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
#ifndef XNE_TILEVIEW_H
#define XNE_TILEVIEW_H

#include <X11/Intrinsic.h>
#include <Xm/PrimitiveP.h>

#include <Xft/Xft.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XnHtileDrawFunc       "drawfunc"
#define XnCtileDrawFunc       "drawfunc"
#define XnHtileDrawData       "drawdata"
#define XnCtileDrawData       "drawdata"
#define XnHtileData           "tileviewdata"
#define XnCtileData           "tileviewdata"
#define XnHtileDataLength     "tileviewlength"
#define XnCtileDataLength     "tileviewlength"
#define XnHtileSelection      "tileviewselection"
#define XnCtileSelection      "tileviewselection"
#define XnHtileWidth          "tilewidth"
#define XnCtileWidth          "tilewidth"
#define XnHtileHeight         "tileheight"
#define XnCtileHeight         "tileheight"
    
    
/*
 * void TileDrawFunc(Widget tileView,
 *                   void *tileData,
 *                   int width,
 *                   int height,
 *                   int x,
 *                   int y,
 *                   void *userData,
 *                   Boolean isSelected);
 */
typedef void(*TileDrawFunc)(Widget, void *, int, int, int, int, void *, Boolean);

typedef struct
{
    void *selected_item;
    int  selection;
} XnTileViewCallbackStruct;
    
extern WidgetClass tileviewWidgetClass;

Widget XnCreateTileView(Widget parent, char *name, ArgList arglist, Cardinal argcount);

int XnTileViewGetSelection(Widget tileView);
void XnTileViewSetSelection(Widget tileView, int selection);

GC XnTileViewGC(Widget tileView);
XftDraw* XnTileViewXftDraw(Widget tileView);


#ifdef __cplusplus
}
#endif

#endif /* XNE_TILEVIEW_H */

