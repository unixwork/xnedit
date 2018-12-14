/**
 *
 * $Id: BubbleButton.h,v 1.2 2003/12/25 06:55:07 tksoh Exp $
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
#ifndef _BUBBLEBUTTON_H
#define _BUBBLEBUTTON_H

#include <X11/IntrinsicP.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XltNfadeRate
#define XltNfadeRate "fadeRate"
#endif
#ifndef XltCFadeRate
#define XltCFadeRate "FadeRate"
#endif
#ifndef XltNdelay
#define XltNdelay "delay"
#endif
#ifndef XltCDelay
#define XltCDelay "Delay"
#endif
#define XltNbubbleString "bubbleString"
#define XltCBubbleString "BubbleString"
#define XltNshowBubble "showBubble"
#define XltCShowBubble "ShowBubble"

#ifndef XltNmouseOverPixmap
#define XltNmouseOverPixmap "mouseOverPixmap"
#define XltCMouseOverPixmap "MouseOverPixmap"
#endif
#ifndef XltNmouseOverString
#define XltNmouseOverString "mouseOverString"
#define XltCMouseOverString "MouseOverString"
#endif
#ifndef XltNbubbleDuration
#define XltNbubbleDuration "bubbleDuration"
#endif
#ifndef XltCBubbleDuration
#define XltCBubbleDuration "BubbleDuration"
#endif
#ifndef XltNslidingBubble
#define XltNslidingBubble "slidingBubble"
#endif
#ifndef XltCslidingBubble
#define XltCslidingBubble "SlidingBubble"
#endif
#ifndef XltNautoParkBubble
#define XltNautoParkBubble "autoParkBubble"
#endif
#ifndef XltCautoParkBubble
#define XltCautoParkBubble "AutoParkBubble"
#endif

extern WidgetClass xrwsBubbleButtonWidgetClass;

typedef struct _XltBubbleButtonRec *XltBubbleButtonWidget;
typedef struct _XltBubbleButtonClassRec *XltBubbleButtonWidgetClass;
#if 0
typedef struct {
	int reason;
	char *data;
	int len;
} XltHostCallbackStruct, _XltHostCallbackStruct;
#endif


#define XltIsBubbleButton(w) XtIsSubclass((w), xrwsBubbleButtonWidgetClass)

extern Widget XltCreateBubbleButton(Widget parent,
			     char *name,
			     Arg *arglist,
			     Cardinal argCount);
#ifdef __cplusplus
}  /* Close scope of 'extern "C"' declaration which encloses file. */
#endif

#endif
