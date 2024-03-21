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

#ifndef XNE_TEXTFIELD_H
#define XNE_TEXTFIELD_H


#ifndef XNE_TEXTFIELD
#define USE_XM_TEXTFIELD
#endif

#ifndef USE_XM_TEXTFIELD
#define XNEtextfieldWidgetClass                 textfieldWidgetClass

#define XNECreateText(parent,name,args,count)   XNECreateTextField(parent,name,args,count)
#define XNETextSetString(widget,value)          XNETextFieldSetString(widget,value)
#define XNETextGetString(widget)                XNETextFieldGetString(widget)
#define XNETextGetLastPosition(widget)          XNETextFieldGetLastPosition(widget)
#define XNETextSetInsertionPosition(widget, i)  XNETextFieldSetInsertionPosition(widget, i) 
#define XNETextGetLastPosition(widget)          XNETextFieldGetLastPosition(widget)
#define XNETextSetSelection(w, f, l, t)         XNETextFieldSetSelection(w, f, l, t)
#else
#define XNEtextfieldWidgetClass                 xmTextWidgetClass

#define XNECreateText(parent,name,args,count)   XmCreateTextField(parent,name,args,count)
#define XNETextSetString(widget,value)          XmTextFieldSetString(widget,value)
#define XNETextGetString(widget)                TextGetStringUtf8(widget)
#define XNETextGetLastPosition(widget)          XmTextFieldGetLastPosition(widget)  
#define XNETextSetInsertionPosition(widget, i)  XmTextFieldSetInsertionPosition(widget, i)  
#define XNETextSetSelection(w, f, l, t)         XmTextFieldSetSelection(w, f, l, t)
#endif

#include <X11/Intrinsic.h>
#include <Xm/TextF.h>
#include "../source/text.h" /* textNXftFont */

extern WidgetClass textfieldWidgetClass;

struct TextFieldClassRec;
struct TextFieldRec;

typedef struct TextFieldRec *TextFieldWidget;


void textfield_init(Widget request, Widget neww, ArgList args, Cardinal *num_args);
void textfield_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes);
void textfield_destroy(Widget widget);
void textfield_resize(Widget widget);
void textfield_expose(Widget widget, XEvent* event, Region region);
Boolean textfield_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args);
Boolean textfield_acceptfocus(Widget widget, Time *time);

void textfield_recalc_size(TextFieldWidget w);

void  XNETextFieldSetString(Widget widget, char *value);
char* XNETextFieldGetString(Widget widget);
XmTextPosition XNETextFieldGetLastPosition(Widget widget);
void XNETextFieldSetInsertionPosition(Widget widget, XmTextPosition i);
void XNETextFieldSetSelection(Widget w, XmTextPosition first, XmTextPosition last, Time sel_time);


#endif /* XNE_TEXTFIELD_H */

