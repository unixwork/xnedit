/**
 *
 * $Id: BubbleButton.c,v 1.9 2005/12/01 14:31:43 tringali Exp $
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

#include <unistd.h>
#include <stdio.h>

#include <X11/StringDefs.h>

#include <Xm/XmosP.h>
#include <Xm/MessageB.h>
#include <Xm/Form.h>
#include <Xm/TextF.h>
#include <Xm/LabelP.h>
#include <Xm/DialogS.h>
#include <Xm/MenuShell.h>
#include <Xm/Display.h>

#include "BubbleButtonP.h"
#include "SlideC.h"

#include "../util/misc.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

static const char rcsid[] = "$Id: BubbleButton.c,v 1.9 2005/12/01 14:31:43 tringali Exp $";

/*
   Widget methods, forward declarations
 */

static void class_initialize(void);
static void class_part_initialize(WidgetClass widget_class);
static void initialize(Widget request, Widget new_w, ArgList args, Cardinal *num_args);
static void destroy(Widget w);
static Boolean set_values(Widget old, Widget request, Widget new_w, ArgList args, Cardinal *num_args);
static void _XmExportLabelString(Widget w, int offset, XtArgVal *value);

/*
   Helper functions, forward declarations
 */

/*
   Widget default resources
 */

#define Offset(field) XtOffsetOf(XltBubbleButtonRec, bubble_button.field)
static XtResource resources[] =
{
	{
		XltNbubbleString, XltCBubbleString, XmRXmString,
		sizeof(XmString), Offset(BubbleString),
		XmRImmediate, (XtPointer)NULL
	},
	{
		XltNshowBubble, XltCShowBubble, XmRBoolean,
		sizeof(Boolean), Offset(show_bubble),
		XmRImmediate, (XtPointer)True
	},
	{
		XltNdelay, XltCDelay, XtRInt,
		sizeof(int), Offset(Delay),
		XtRImmediate, (XtPointer)1000
	},
	{
		XltNmouseOverString, XltCMouseOverString, XmRXmString,
		sizeof(XmString), Offset(MouseOverString),
		XtRImmediate, (XtPointer)NULL
	},
	{
		XltNmouseOverPixmap, XltCMouseOverPixmap, XmRPrimForegroundPixmap,
		sizeof(Pixmap), Offset(MouseOverPixmap),
		XtRImmediate, (XtPointer)None
	},
	{
		XltNbubbleDuration, XltCBubbleDuration, XtRInt,
		sizeof(int), Offset(Duration),
		XtRImmediate, (XtPointer)0
	},
	{
		XltNslidingBubble, XltCslidingBubble, XmRBoolean,
		sizeof(Boolean), Offset(slidingBubble),
		XmRImmediate, (XtPointer)True
	},
	{
		XltNautoParkBubble, XltCautoParkBubble, XmRBoolean,
		sizeof(Boolean), Offset(autoParkBubble),
		XmRImmediate, (XtPointer)False
	},
};

static XmSyntheticResource syn_resources[] =
{
	{
		XltNbubbleString,
		sizeof(XmString), Offset(BubbleString),
		_XmExportLabelString, NULL
	}
};
#undef Offset

/*
   Widget class record
 */

static void EnterWindow(Widget w, XEvent *event, String *params, Cardinal *num_params);
static void LeaveWindow(Widget w, XEvent *event, String *params, Cardinal *num_params);

static XtActionsRec actions[] =
{
	{"Enter", EnterWindow},
	{"Leave", LeaveWindow},
};

/* *INDENT-OFF* */
XltBubbleButtonClassRec xrwsBubbleButtonClassRec = {
    /* Core class part */
    {
	/* superclass            */ (WidgetClass) &xmPushButtonClassRec,
        /* class_name            */ "XltBubbleButton",
	/* widget_size           */ sizeof(XltBubbleButtonRec),
	/* class_initialize      */ class_initialize,
	/* class_part_initialize */ class_part_initialize,
	/* class_inited          */ False,
	/* initialize            */ initialize,
	/* initialize_hook       */ NULL,
	/* realize               */ XtInheritRealize,
	/* actions               */ actions,
	/* num_actions           */ XtNumber(actions),
	/* resources             */ resources,
	/* num_resources         */ XtNumber(resources),
	/* xrm_class             */ NULLQUARK,
	/* compress_motion       */ True,
	/* compress_exposure     */ XtExposeCompressMaximal,
	/* compress_enterleave   */ True,
	/* visible_interest      */ False,
	/* destroy               */ destroy,
	/* resize                */ XtInheritResize,
	/* expose                */ XtInheritExpose,
	/* set_values            */ set_values,
	/* set_values_hook       */ NULL,
	/* set_values_almost     */ XtInheritSetValuesAlmost,
	/* get_values_hook       */ NULL,
	/* accept_focus          */ NULL,
	/* version               */ XtVersion,
	/* callback offsets      */ NULL,
	/* tm_table              */ NULL,
	/* query_geometry        */ XtInheritQueryGeometry,
	/* display_accelerator   */ NULL,
	/* extension             */ (XtPointer)NULL
    },
    /* Primitive Class part */
    {
	/* border_highlight      */ XmInheritBorderHighlight,
       	/* border_unhighlight    */ XmInheritBorderUnhighlight,
       	/* translations          */ XtInheritTranslations,
       	/* arm_and_activate_proc */ XmInheritArmAndActivate,
       	/* synthetic resources   */ syn_resources, 
        /* num syn res           */ XtNumber(syn_resources),
	/* extension             */ (XtPointer)NULL
    },
    /* Label Class part */
    {
        /* setOverrideCallback */ XmInheritSetOverrideCallback,
        /* menuProcs           */ XmInheritMenuProc,
        /* translations        */ XtInheritTranslations,
	/* extension           */ NULL
    },
    /* PushButton Class part */
    {
	/* extension */ NULL
    },
    /* BubbleButton Class part */
    {
	/* leave_time  */ 0,
	/* extension   */ NULL
    }
};
/* *INDENT-ON* */



WidgetClass xrwsBubbleButtonWidgetClass = (WidgetClass)&xrwsBubbleButtonClassRec;

/*
   Helper routines
 */

/*
   Widget methods
 */

static void
class_initialize(void)
{
    xrwsBubbleButtonClassRec.bubble_button_class.leave_time = 0;
}

static void
class_part_initialize(WidgetClass widget_class)
{
}

static void
initialize(Widget request, Widget new_w, ArgList args, Cardinal *num_args)
{
Widget Shell;
Arg arg[10];
int argcnt = 0;

    BubbleButton_Timer(new_w) = (XtIntervalId)NULL;
    BubbleButton_DurationTimer(new_w) = (XtIntervalId)NULL;
    BubbleButton_Swapped(new_w) = False;
    BubbleButton_Slider(new_w) = NULL;
    Shell = CreatePopupShellWithBestVis("BubbleShell", 
             transientShellWidgetClass, new_w, arg, argcnt);
    XtVaSetValues(Shell,
    	XmNoverrideRedirect, True,
    	NULL);
    if (BubbleButton_MouseOverString(new_w) != NULL)
    {
    	BubbleButton_MouseOverString(new_w) = XmStringCopy(BubbleButton_MouseOverString(new_w));
    }
    if (BubbleButton_BubbleString(new_w) == NULL)
    {
    XmString xmstring;

#if XmVERSION >= 2
    	xmstring = XmeGetLocalizedString((char *)NULL,
    			new_w,
    			XmNlabelString,
    			XtName(new_w));
#else
    	xmstring = _XmOSGetLocalizedString((char *)NULL,
    			new_w,
    			XmNlabelString,
    			XtName(new_w));
#endif
    	BubbleButton_BubbleString(new_w) = xmstring;
    }
    else
    {
    	BubbleButton_BubbleString(new_w) = XmStringCopy(BubbleButton_BubbleString(new_w));
    }
    BubbleButton_Label(new_w) = XmCreateLabel(Shell, "BubbleLabel", NULL, 0);
    XtVaSetValues(BubbleButton_Label(new_w),
    	XmNlabelString, BubbleButton_BubbleString(new_w),
    	XmNforeground, ((XltBubbleButtonWidget)new_w)->core.background_pixel,
    	XmNbackground, ((XltBubbleButtonWidget)new_w)->primitive.foreground,
    	NULL);
    XtManageChild(BubbleButton_Label(new_w));
}

static void
destroy(Widget w)
{
    if (BubbleButton_MouseOverString(w))
    {
	XmStringFree(BubbleButton_MouseOverString(w));
    }
    if (BubbleButton_Timer(w))
    {
	XtRemoveTimeOut(BubbleButton_Timer(w));
    }
    if (BubbleButton_DurationTimer(w))
    {
	XtRemoveTimeOut(BubbleButton_DurationTimer(w));
    }
}

static Boolean
set_values(Widget old, Widget request, Widget new_w, ArgList args, Cardinal *num_args)
{
    if (BubbleButton_MouseOverString(new_w) != BubbleButton_MouseOverString(old))
    {
    	XmStringFree(BubbleButton_MouseOverString(old));
    	BubbleButton_MouseOverString(new_w) = XmStringCopy(BubbleButton_MouseOverString(new_w));
    }
    if (BubbleButton_BubbleString(new_w) != BubbleButton_BubbleString(old))
    {
    	XmStringFree(BubbleButton_BubbleString(old));
    	BubbleButton_BubbleString(new_w) = XmStringCopy(BubbleButton_BubbleString(new_w));
	XtVaSetValues(BubbleButton_Label(new_w),
	    XmNlabelString, BubbleButton_BubbleString(new_w),
	    NULL);
    }
    if (XtIsSensitive(old) != XtIsSensitive(new_w))
    {
	if (!XtIsSensitive(new_w))
	{
	Cardinal num_params = 0;

	    LeaveWindow(new_w, NULL, NULL, &num_params);
	}
    }
    return (False);
}

/*
 * Short-term solution. Doesn't belong here. See SF bug #923924.
 */
extern XmString _XmStringCreateExternal(XmFontList fontlist, _XmString cs);

static void
_XmExportLabelString(Widget w, int offset, XtArgVal *value)
{
    _XmString str;
    XmString ret;

    str = *(_XmString *)(((char *)w) + offset);
    if (str)
    {
	if (XmIsLabel(w))
	{
	    ret = _XmStringCreateExternal(Lab_Font(w), str);
	}
	else
	{
	    ret = NULL;
	}
    }
    else
    {
	ret = NULL;
    }

    *value = (XtArgVal)ret;
}

static void
fadeOutFinish(Widget slide, Widget w, XtPointer call_data)
{
    BubbleButton_Slider(w) = NULL;
    XtPopdown(XtParent(BubbleButton_Label(w)));
}

static void
UnpostIt(Widget w)
{
    BubbleButton_DurationTimer(w) = (XtIntervalId)NULL;
    if (BubbleButton_SlidingBubble(w))
    {
	BubbleButton_Slider(w) = XtVaCreateWidget("Slide", xltSlideContextWidgetClass,
	    XmGetXmDisplay(XtDisplay(w)),
	    XltNslideWidget, XtParent(BubbleButton_Label(w)),
	    XltNslideDestHeight, 1,
	    NULL);
	XtAddCallback(BubbleButton_Slider(w), XltNslideFinishCallback,
            (XtCallbackProc)fadeOutFinish, w);
	}
    else
    {
    	XtPopdown(XtParent(BubbleButton_Label(w)));
    }
}

static void
fadeInFinish(Widget slide, Widget w, XtPointer call_data)
{
    BubbleButton_Slider(w) = NULL;
    if (BubbleButton_Duration(w) > 0)
    {
	BubbleButton_DurationTimer(w) = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
			BubbleButton_Duration(w),
			(XtTimerCallbackProc)UnpostIt, 
			w);
    }
}

#define TOOLTIP_EDGE_GUARD 5
static void
PostIt(Widget w)
{
int rx, ry, x, y;
unsigned int key;
Window root, child;
Dimension xPos, yPos;
XWindowAttributes screenAttr;

    	BubbleButton_Timer(w) = (XtIntervalId)NULL;
    	XQueryPointer(XtDisplay(w),
    			XtWindow(w),
    			&root,
    			&child,
    			&rx, &ry,
    			&x, &y,
    			&key);
    	if (BubbleButton_DurationTimer(w) != (XtIntervalId)NULL)
    	{
	    XtRemoveTimeOut(BubbleButton_DurationTimer(w));
	    BubbleButton_DurationTimer(w) = (XtIntervalId)NULL;
    	}
	{
	XtWidgetGeometry geo;

	    XtQueryGeometry(BubbleButton_Label(w), NULL, &geo);

	    xPos = rx - x + XtWidth(w) / 2 ;
	    yPos = ry - y + XtHeight(w);

	    if (BubbleButton_AutoParkBubble(w))
	    {
		xPos = rx + 3;
		yPos = ry + 15;

		/* keep tooltip within screen */
		 XGetWindowAttributes(XtDisplay(w), 
			 RootWindowOfScreen(XtScreen(w)), &screenAttr);

		if (xPos + geo.width >= screenAttr.width - TOOLTIP_EDGE_GUARD)
		    xPos = screenAttr.width - geo.width - TOOLTIP_EDGE_GUARD;

		if (yPos + geo.height >= screenAttr.height - TOOLTIP_EDGE_GUARD)
		    yPos = ry - 15 - geo.height;
	    }

	    
	    if (BubbleButton_SlidingBubble(w))
	    {
	    	int xAdjust, yAdjust;
		
		/* FIXME: slider must avoid pointer */
		xAdjust = rx < xPos? 1 : -1;
		yAdjust = ry < yPos? 1 : - geo.height/2;

		XtVaSetValues(XtParent(BubbleButton_Label(w)),
		    XmNx, rx + xAdjust /*- x + XtWidth(w) / 2*/,
		    XmNy, ry + yAdjust /*- y + XtHeight(w)*/,
		    XmNheight, 1,
		    XmNwidth, 1 /*geo.width*/,
		    NULL);
		XtPopup(XtParent(BubbleButton_Label(w)), XtGrabNone);

		BubbleButton_Slider(w) = XtVaCreateWidget("Slide", xltSlideContextWidgetClass,
		    XmGetXmDisplay(XtDisplay(w)),
		    XltNslideWidget, XtParent(BubbleButton_Label(w)),
		    XltNslideDestX, xPos,
		    XltNslideDestY, yPos,
		    XltNslideDestWidth, geo.width,
		    XltNslideDestHeight, geo.height,
		    NULL);
		XtAddCallback(BubbleButton_Slider(w), XltNslideFinishCallback, (XtCallbackProc)fadeInFinish, w);
	    }
	    else
	    {
		XtVaSetValues(XtParent(BubbleButton_Label(w)),
		    XmNx, xPos /*- x + XtWidth(w) / 2*/,
		    XmNy, yPos /*- y + XtHeight(w)*/,
		    XmNheight, geo.height,
		    XmNwidth, geo.width /*geo.width*/,
		    NULL);
		XtPopup(XtParent(BubbleButton_Label(w)), XtGrabNone);

		if (BubbleButton_Duration(w) > 0)
		{
		    BubbleButton_DurationTimer(w) = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
				    BubbleButton_Duration(w),
				    (XtTimerCallbackProc)UnpostIt, 
				    w);
		}
	    }
	}
}

static void
SwapLabels(Widget w)
{
XmString tmp = NULL;

    if (BubbleButton_MouseOverString(w))
    {
	    XtVaGetValues(w,
		    XmNlabelString, &tmp,
		    NULL);
	    XtVaSetValues(w,
		    XmNlabelString, BubbleButton_MouseOverString(w),
		    NULL);
	    XmStringFree(BubbleButton_MouseOverString(w));
	    BubbleButton_MouseOverString(w) = tmp;
    }
}

static void
SwapPixmaps(Widget w)
{
Pixmap tmp;

    if (BubbleButton_MouseOverPixmap(w))
    {
	    XtVaGetValues(w,
		    XmNlabelPixmap, &tmp,
		    NULL);
	    XtVaSetValues(w,
		    XmNlabelPixmap, BubbleButton_MouseOverPixmap(w),
		    NULL);
	    BubbleButton_MouseOverPixmap(w) = tmp;
    }
}

static void
Swap(Widget w)
{
    if (Lab_IsText(w))
    {
    	SwapLabels(w);
    }
    else
    {
    	SwapPixmaps(w);
    }
    BubbleButton_Swapped(w) = BubbleButton_Swapped(w) ? False : True;
}

static void 
EnterWindow(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    if (BubbleButton_ShowBubble(w) && !BubbleButton_Timer(w))
    {
    unsigned long delay;

	if (event && (event->xcrossing.time - BubbleButtonClass_LeaveTime(w) < BubbleButton_Delay(w)))
	{
		delay = 0;
	}
	else
	{
		delay = BubbleButton_Delay(w);
	}
	BubbleButton_Timer(w) = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
					delay,
					(XtTimerCallbackProc)PostIt, 
					w);
    }
    if (!BubbleButton_Swapped(w))
    {
	Swap(w);
    }
}

static void 
LeaveWindow(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    if (BubbleButton_Timer(w))
    {
    	XtRemoveTimeOut(BubbleButton_Timer(w));
    	BubbleButton_Timer(w) = (XtIntervalId)NULL;
    }
    else
    {
	if (BubbleButton_Slider(w) != NULL)
	{
	    XtDestroyWidget(BubbleButton_Slider(w));
	    BubbleButton_Slider(w) = NULL;
	}
	XtPopdown(XtParent(BubbleButton_Label(w)));
	if (event)
	{
	    if (BubbleButton_DurationTimer(w) || BubbleButton_Duration(w) == 0)
	    {
		BubbleButtonClass_LeaveTime(w) = event->xcrossing.time;
	    }
	}
    }
    if (BubbleButton_DurationTimer(w))
    {
    	XtRemoveTimeOut(BubbleButton_DurationTimer(w));
	BubbleButton_DurationTimer(w) = (XtIntervalId)NULL;
    }
    if (BubbleButton_Swapped(w))
    {
	Swap(w);
    }
}

/*
   Public functions
 */

Widget
XltCreateBubbleButton(Widget parent,
	       char *name,
	       Arg *arglist,
	       Cardinal argCount)
{
	return XtCreateWidget(name, xrwsBubbleButtonWidgetClass, parent, arglist, argCount);
}

