/**
 *
 * $Id: SlideC.h,v 1.1 2003/12/23 08:34:36 tksoh Exp $
 *
 * Copyright (C) 1996 Free Software Foundation, Inc.
 * Copyright © 1999-2001 by the LessTif developers.
 *
 * This file is part of the GNU LessTif Extension Library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **/

#ifndef _SLIDEC_H
#define _SLIDEC_H

#include <X11/Intrinsic.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XltIsSlideContext
#define XltIsSlideContext(w) XtIsSubclass(w,xltSlideContextClass)
#endif
#ifndef XltNslideFinishCallback
#define XltNslideFinishCallback "slideFinishCallback"
#endif
#ifndef XltCSlideFinishCallback
#define XltCSlideFinishCallback "SlideFinishCallback"
#endif
#ifndef XltNslideMotionCallback
#define XltNslideMotionCallback "slideMotionCallback"
#endif
#ifndef XltCSlideMotionCallback
#define XltCSlideMotionCallback "SlideMotionCallback"
#endif
#ifndef XltNslideWidget
#define XltNslideWidget "slideWidget"
#endif
#ifndef XltCSlideWidget
#define XltCSlideWidget "SlideWidget"
#endif
#ifndef XltNslideInterval
#define XltNslideInterval "slideInterval"
#endif
#ifndef XltCSlideInterval
#define XltCSlideInterval "SlideInterval"
#endif
#ifndef XltNslideDestWidth
#define XltNslideDestWidth "slideDestWidth"
#endif
#ifndef XltCSlideDestWidth
#define XltCSlideDestWidth "SlideDestWidth"
#endif
#ifndef XltNslideDestHeight
#define XltNslideDestHeight "slideDestHeight"
#endif
#ifndef XltCSlideDestHeight
#define XltCSlideDestHeight "SlideDestHeight"
#endif
#ifndef XltNslideDestX
#define XltNslideDestX "slideDestX"
#endif
#ifndef XltCSlideDestX
#define XltCSlideDestX "SlideDestX"
#endif
#ifndef XltNslideDestY
#define XltNslideDestY "slideDestY"
#endif
#ifndef XltCSlideDestY
#define XltCSlideDestY "SlideDestY"
#endif

extern WidgetClass xltSlideContextWidgetClass;

typedef struct _XltSlideContextRec *XltSlideContextWidget;
typedef struct _XltSlideContextClassRec *XmSlideContextWidgetClass;

typedef struct _XltSlideStruct {
	Widget w;
	XtWidgetGeometry dest;
	unsigned long interval;
	XtIntervalId id;
} XltSlideStruct, *XltSlidePtr;

void XltSlide(XltSlidePtr slide_info);

#ifdef __cplusplus
}
#endif
#endif
