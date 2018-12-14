/**
 *
 * $Id: SlideCP.h,v 1.4 2005/12/01 14:31:43 tringali Exp $
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

#ifndef _SLIDECP_H
#define _SLIDECP_H

#include <X11/IntrinsicP.h>
#include <X11/ObjectP.h>
#include <Xm/XmP.h>
#include "SlideC.h"


#ifdef __cplusplus
extern "C" {
#endif


#ifndef XmUNSPECIFIED
#define XmUNSPECIFIED  (~0)
#endif

#ifndef XmUNSPECIFIED_POSITION
#define XmUNSPECIFIED_POSITION (-1)
#endif

typedef struct {
    XtPointer extension;
} XltSlideContextClassPart;

typedef struct _XltSlideContextClassRec {
	ObjectClassPart object_class;
	XltSlideContextClassPart slide_class;
} XltSlideContextClassRec;

extern XltSlideContextClassRec xltSlideContextClassRec;

typedef struct _XmSlideContextPart {
	XtIntervalId id;
	XtCallbackList slideFinishCallback;
	XtCallbackList slideMotionCallback;
	Widget slide_widget;
	unsigned long interval;
	Dimension dest_width;
	Dimension dest_height;
	Position dest_x;
	Position dest_y;
} XltSlideContextPart;

typedef struct _XltSlideContextRec {
	ObjectPart object;
	XltSlideContextPart slide;
} XltSlideContextRec;

#define Slide_Id(w) (((XltSlideContextWidget)w)->slide.id)
#define Slide_Widget(w) (((XltSlideContextWidget)w)->slide.slide_widget)
#define Slide_Interval(w) (((XltSlideContextWidget)w)->slide.interval)
#define Slide_DestWidth(w) (((XltSlideContextWidget)w)->slide.dest_width)
#define Slide_DestHeight(w) (((XltSlideContextWidget)w)->slide.dest_height)
#define Slide_DestX(w) (((XltSlideContextWidget)w)->slide.dest_x)
#define Slide_DestY(w) (((XltSlideContextWidget)w)->slide.dest_y)
#define Slide_FinishCallback(w) (((XltSlideContextWidget)w)->slide.slideFinishCallback)
#define Slide_MotionCallback(w) (((XltSlideContextWidget)w)->slide.slideMotionCallback)

#ifdef __cplusplus
}
#endif

#endif /* ifndef _SLIDECP_H */
