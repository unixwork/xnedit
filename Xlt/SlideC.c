/**
 *
 * $Id: SlideC.c,v 1.5 2005/12/01 14:31:43 tringali Exp $
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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <Xm/XmP.h>

#include "SlideCP.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#include <inttypes.h>

/*
   Widget methods, forward declarations
 */

static void class_initialize(void);
static void class_part_initialize(WidgetClass widget_class);
static void initialize(Widget request, Widget new_w, ArgList args, Cardinal *num_args);
static void destroy(Widget w);
static Boolean set_values(Widget old, Widget request, Widget new_w, ArgList args, Cardinal *num_args);
static void get_values_hook(Widget w, ArgList args, Cardinal *num_args);

static void _XltSlideProc(Widget w);
static void targetDestroy(Widget target, Widget w);

/*
   Widget default resources
 */

#define Offset(field) XtOffsetOf(XltSlideContextRec, slide.field)
static XtResource resources[] =
{
    {
	XltNslideFinishCallback, XtCCallback, XtRCallback,
	sizeof(XtCallbackList), Offset(slideFinishCallback),
	XtRCallback, NULL
    },
    {
	XltNslideMotionCallback, XtCCallback, XtRCallback,
	sizeof(XtCallbackList), Offset(slideMotionCallback),
	XtRCallback, NULL
    },
    {
	XltNslideWidget, XltCSlideWidget, XtRWidget,
	sizeof(Widget), Offset(slide_widget),
	XtRImmediate, NULL
    },
    {
	XltNslideInterval, XltCSlideInterval, XtRInt,
	sizeof(int), Offset(interval),
	XtRImmediate, (XtPointer)5
    },
    {
	XltNslideDestWidth, XltCSlideDestWidth, XtRDimension,
	sizeof(Dimension), Offset(dest_width),
	XtRImmediate, (XtPointer)XmUNSPECIFIED
    },
    {
	XltNslideDestHeight, XltCSlideDestHeight, XtRDimension,
	sizeof(Dimension), Offset(dest_height),
	XtRImmediate, (XtPointer)XmUNSPECIFIED
    },
    {
	XltNslideDestX, XltCSlideDestX, XtRPosition,
	sizeof(Position), Offset(dest_x),
	XtRImmediate, (XtPointer)XmUNSPECIFIED_POSITION
    },
    {
	XltNslideDestY, XltCSlideDestY, XtRPosition,
	sizeof(Position), Offset(dest_y),
	XtRImmediate, (XtPointer)XmUNSPECIFIED_POSITION
    },
};
#undef Offset

/*
   Widget class record
 */
XltSlideContextClassRec xltSlideContextClassRec = {
	/* Object Class Part */
	{
/* pointer to superclass ClassRec   WidgetClass       */ (WidgetClass) &objectClassRec,
/* widget resource class name       String            */ "XltSlideContext",
/* size in bytes of widget record   Cardinal          */ sizeof(XltSlideContextRec),
/* class initialization proc        XtProc            */ class_initialize,
/* dynamic initialization           XtWidgetClassProc */ class_part_initialize,
/* has class been initialized?      XtEnum            */ False,
/* initialize subclass fields       XtInitProc        */ initialize,
/* notify that initialize called    XtArgsProc        */ NULL,
/* NULL                             XtProc            */ NULL,
/* NULL                             XtPointer         */ NULL,
/* NULL                             Cardinal          */ (Cardinal)(intptr_t)NULL,
/* resources for subclass fields    XtResourceList    */ resources,
/* number of entries in resources   Cardinal          */ XtNumber(resources),
/* resource class quarkified        XrmClass          */ NULLQUARK,
/* NULL                             Boolean           */ (Boolean)(intptr_t)NULL,
/* NULL                             XtEnum            */ (XtEnum)(intptr_t)NULL,
/* NULL				    Boolean           */ (Boolean)(intptr_t)NULL,
/* NULL                             Boolean           */ (Boolean)(intptr_t)NULL,
/* free data for subclass pointers  XtWidgetProc      */ destroy,
/* NULL                             XtProc            */ NULL,
/* NULL			            XtProc            */ NULL,
/* set subclass resource values     XtSetValuesFunc   */ set_values,
/* notify that set_values called    XtArgsFunc        */ NULL,
/* NULL                             XtProc            */ NULL,
/* notify that get_values called    XtArgsProc        */ get_values_hook,
/* NULL                             XtProc            */ NULL,
/* version of intrinsics used       XtVersionType     */ XtVersion,
/* list of callback offsets         XtPointer         */ NULL,
/* NULL                             String            */ NULL,
/* NULL                             XtProc            */ NULL,
/* NULL                             XtProc            */ NULL,
/* pointer to extension record      XtPointer         */ NULL
	},
	/* SlideContext Class Part */
	{
		NULL
	}
};

WidgetClass xltSlideContextWidgetClass = (WidgetClass)&xltSlideContextClassRec;

static void 
class_initialize(void)
{
}

static void 
class_part_initialize(WidgetClass widget_class)
{
}

static void 
initialize(Widget request, Widget new_w, ArgList args, Cardinal *num_args)
{
    /*
    printf("%s:%s(%d) - %s %li\n", __FILE__, __FUNCTION__, __LINE__,
    	XtName(new_w),
    	Slide_Interval(new_w));
    	*/

    if (Slide_DestWidth(new_w) == (Dimension)XmUNSPECIFIED)
    {
	Slide_DestWidth(new_w) = XtWidth(Slide_Widget(new_w));
    }
    if (Slide_DestHeight(new_w) == (Dimension)XmUNSPECIFIED)
    {
	Slide_DestHeight(new_w) = XtHeight(Slide_Widget(new_w));
    }
    if (Slide_DestX(new_w) == XmUNSPECIFIED_POSITION)
    {
	Slide_DestX(new_w) = XtX(Slide_Widget(new_w));
    }
    if (Slide_DestY(new_w) == XmUNSPECIFIED_POSITION)
    {
	Slide_DestY(new_w) = XtY(Slide_Widget(new_w));
    }
    Slide_Id(new_w) = XtAppAddTimeOut(XtWidgetToApplicationContext(new_w),
	    Slide_Interval(new_w),
	    (XtTimerCallbackProc)_XltSlideProc,
	    new_w);
    XtAddCallback(Slide_Widget(new_w), XmNdestroyCallback, (XtCallbackProc)targetDestroy, new_w);
}

static void 
destroy(Widget w)
{
    if (Slide_Id(w) != (XtIntervalId)NULL)
    {
    	XtRemoveTimeOut(Slide_Id(w));
	Slide_Id(w) = (XtIntervalId)NULL;
    }
    XtRemoveCallback(Slide_Widget(w), XmNdestroyCallback, (XtCallbackProc)targetDestroy, w);
}

static Boolean 
set_values(Widget old, Widget request, Widget new_w, ArgList args, Cardinal *num_args)
{
    if (Slide_Widget(old) != Slide_Widget(new_w))
    {
	XtRemoveCallback(Slide_Widget(old), XmNdestroyCallback, (XtCallbackProc)targetDestroy, old);
	XtAddCallback(Slide_Widget(new_w), XmNdestroyCallback, (XtCallbackProc)targetDestroy, new_w);
    }
    return(False);
}

static void 
get_values_hook(Widget w, ArgList args, Cardinal *num_args)
{
}

static void
targetDestroy(Widget target, Widget w)
{
    XtDestroyWidget(w);
}

static void
_XltSlideProc(Widget w)
{
Dimension width, height;
Position x, y;

    /*
    printf("%s:%s(%d) - %s %li\n", __FILE__, __FUNCTION__, __LINE__,
    	XtName(w),
    	Slide_Interval(w));
    	*/

    width = XtWidth(Slide_Widget(w));
    height = XtHeight(Slide_Widget(w));
    x = XtX(Slide_Widget(w));
    y = XtY(Slide_Widget(w));
    {
	height = height - (((height - Slide_DestHeight(w)) * 10) / 100);
	if (height < Slide_DestHeight(w))
	{
	    height++;
	}
	if (height > Slide_DestHeight(w))
	{
	    height--;
	}
	width = width - (((width - Slide_DestWidth(w)) * 10) / 100);
	if (width < Slide_DestWidth(w))
	{
	    width++;
	}
	if (width > Slide_DestWidth(w))
	{
	    width--;
	}
    }
    {
	y = y - ((((y - Slide_DestY(w)) * 10) / 100) + 0);
	if (y < Slide_DestY(w))
	{
	    y++;
	}
	if (y > Slide_DestY(w))
	{
	    y--;
	}
	x = x - ((((x - Slide_DestX(w)) * 10) / 100) + 0);
	if (x < Slide_DestX(w))
	{
	    x++;
	}
	if (x > Slide_DestX(w))
	{
	    x--;
	}
    }
    /*
    XtCallCallbackList(w, Slide_MotionCallback(w), NULL);
    */
    XtVaSetValues(Slide_Widget(w),
    	XmNx, x,
    	XmNy, y,
    	XmNwidth, width,
    	XmNheight, height,
    	NULL);
    if (Slide_DestX(w) == XtX(Slide_Widget(w)) &&
        Slide_DestY(w) == XtY(Slide_Widget(w)) &&
        Slide_DestWidth(w) == XtWidth(Slide_Widget(w)) &&
        Slide_DestHeight(w) == XtHeight(Slide_Widget(w)))
    {
    	XtCallCallbackList(w, Slide_FinishCallback(w), NULL);
	XtRemoveCallback(Slide_Widget(w), XmNdestroyCallback, (XtCallbackProc)targetDestroy, w);
    	XtDestroyWidget(w);
    }
    else
    {
    	Slide_Id(w) = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
    		Slide_Interval(w),
    		(XtTimerCallbackProc)_XltSlideProc,
    		w);
    }
}
