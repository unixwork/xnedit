/**
 *
 * $Id: BubbleButtonP.h,v 1.5 2005/12/01 14:31:43 tringali Exp $
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
#ifndef _BUBBLEBUTTONP_H
#define _BUBBLEBUTTONP_H

#include "BubbleButton.h"
#include <Xm/PushBP.h>

typedef struct {
	XtIntervalId Timer;
	int Delay;
	Widget BubbleLabel;
	XmString BubbleString;
	Boolean show_bubble;
	XmString MouseOverString;
	Pixmap MouseOverPixmap;
	XtIntervalId DurationTimer;
	int Duration;
	Boolean Swapped;
	Widget slider;
	Boolean slidingBubble;
	Boolean autoParkBubble;
} XltBubbleButtonPart;

#define BubbleButton_Timer(w) (((XltBubbleButtonWidget)w)->bubble_button.Timer)
#define BubbleButton_Delay(w) (((XltBubbleButtonWidget)w)->bubble_button.Delay)
#define BubbleButton_Label(w) (((XltBubbleButtonWidget)w)->bubble_button.BubbleLabel)
#define BubbleButton_BubbleString(w) (((XltBubbleButtonWidget)w)->bubble_button.BubbleString)
#define BubbleButton_ShowBubble(w) (((XltBubbleButtonWidget)w)->bubble_button.show_bubble)
#define BubbleButton_MouseOverString(w) (((XltBubbleButtonWidget)w)->bubble_button.MouseOverString)
#define BubbleButton_MouseOverPixmap(w) (((XltBubbleButtonWidget)w)->bubble_button.MouseOverPixmap)
#define BubbleButton_DurationTimer(w) (((XltBubbleButtonWidget)w)->bubble_button.DurationTimer)
#define BubbleButton_Duration(w) (((XltBubbleButtonWidget)w)->bubble_button.Duration)
#define BubbleButton_Swapped(w) (((XltBubbleButtonWidget)w)->bubble_button.Swapped)
#define BubbleButton_Slider(w) (((XltBubbleButtonWidget)w)->bubble_button.slider)
#define BubbleButton_SlidingBubble(w) (((XltBubbleButtonWidget)w)->bubble_button.slidingBubble)
#define BubbleButton_AutoParkBubble(w) (((XltBubbleButtonWidget)w)->bubble_button.autoParkBubble)

#define BubbleButtonClass_LeaveTime(w) (((XltBubbleButtonWidgetClass)XtClass(w))->bubble_button_class.leave_time)

typedef struct _XltBubbleButtonRec {
	CorePart core;
	XmPrimitivePart primitive;
	XmLabelPart label;
	XmPushButtonPart pushbutton;
	XltBubbleButtonPart bubble_button;
} XltBubbleButtonRec;

typedef struct {
	Time leave_time;
	XtPointer extension;
} XltBubbleButtonClassPart;

typedef struct _XltBubbleButtonClassRec {
	CoreClassPart core_class;
	XmPrimitiveClassPart primitive_class;
	XmLabelClassPart label_class;
	XmPushButtonClassPart pushbutton_class;
	XltBubbleButtonClassPart bubble_button_class;
} XltBubbleButtonClassRec;

extern XltBubbleButtonClassRec xrwsBubbleButtonClassRec;
#endif
