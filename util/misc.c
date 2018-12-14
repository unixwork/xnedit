/*******************************************************************************
*									       *
* misc.c -- Miscelaneous Motif convenience functions			       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.							       *
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* July 28, 1992								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "misc.h"
#include "DialogF.h"
#include "nedit_malloc.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#ifdef __unix__
#include <sys/time.h>
#include <sys/select.h>
#endif

#ifdef __APPLE__ 
#ifdef __MACH__
#include <sys/select.h>
#endif
#endif

#ifdef VMS
#include <types.h>
#include <unixio.h>
#include <file.h>
#endif /*VMS*/

#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <Xm/Xm.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/AtomMgr.h>
#include <Xm/Protocols.h>
#include <Xm/Text.h>
#include <Xm/MessageB.h>
#include <Xm/DialogS.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/FileSB.h>
#include <Xm/ScrolledW.h>
#include <Xm/PrimitiveP.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#ifndef LESSTIF_VERSION
extern void _XmDismissTearOff(Widget w, XtPointer call, XtPointer x);
#endif

/* structure for passing history-recall data to callbacks */
typedef struct {
    char ***list;
    int *nItems;
    int index;
} histInfo;

typedef Widget (*MotifDialogCreationCall)(Widget, String, ArgList, Cardinal);

/* Maximum size of a history-recall list.  Typically never invoked, since
   user must first make this many entries in the text field, limited for
   safety, to the maximum reasonable number of times user can hit up-arrow
   before carpal tunnel syndrome sets in */
#define HISTORY_LIST_TRIM_TO 1000
#define HISTORY_LIST_MAX 2000

/* flags to enable/disable delete key remapping and pointer centered dialogs */
static int RemapDeleteEnabled = True;
static int PointerCenteredDialogsEnabled = False;

/* bitmap and mask for waiting (wrist-watch) cursor */
#define watch_x_hot 7
#define watch_y_hot 7
#define watch_width 16
#define watch_height 16
static unsigned char watch_bits[] = {
   0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0x10, 0x08, 0x08, 0x11,
   0x04, 0x21, 0x04, 0x21, 0xe4, 0x21, 0x04, 0x20, 0x08, 0x10, 0x10, 0x08,
   0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07
};
#define watch_mask_width 16
#define watch_mask_height 16
static unsigned char watch_mask_bits[] = {
   0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf8, 0x1f, 0xfc, 0x3f,
   0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfc, 0x3f, 0xf8, 0x1f,
   0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f
};

static void addMnemonicGrabs(Widget addTo, Widget w, int unmodified);
static void mnemonicCB(Widget w, XtPointer callData, XKeyEvent *event);
static void findAndActivateMnemonic(Widget w, unsigned int keycode);
static void addAccelGrabs(Widget topWidget, Widget w);
static void addAccelGrab(Widget topWidget, Widget w);
static int parseAccelString(Display *display, const char *string, KeySym *keysym,
	unsigned int *modifiers);
static void lockCB(Widget w, XtPointer callData, XEvent *event,
	Boolean *continueDispatch);
static int findAndActivateAccel(Widget w, unsigned int keyCode,
	unsigned int modifiers, XEvent *event);
static void removeWhiteSpace(char *string);
static int stripCaseCmp(const char *str1, const char *str2);
static void warnHandlerCB(String message);
static void histDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void histArrowKeyEH(Widget w, XtPointer callData, XEvent *event,
	Boolean *continueDispatch);
static ArgList addParentVisArgs(Widget parent, ArgList arglist, 
   Cardinal *argcount);
static Widget addParentVisArgsAndCall(MotifDialogCreationCall callRoutine,
	Widget parent, char *name, ArgList arglist, Cardinal  argcount);
static void scrollDownAP(Widget w, XEvent *event, String *args, 
	Cardinal *nArgs);
static void scrollUpAP(Widget w, XEvent *event, String *args, 
	Cardinal *nArgs);
static void pageDownAP(Widget w, XEvent *event, String *args, 
	Cardinal *nArgs);
static void pageUpAP(Widget w, XEvent *event, String *args, 
	Cardinal *nArgs);
static long queryDesktop(Display *display, Window window, Atom deskTopAtom);
static void warning(const char* mesg);
static void microsleep(long usecs);

/*
** Set up closeCB to be called when the user selects close from the
** window menu.  The close menu item usually activates f.kill which
** sends a WM_DELETE_WINDOW protocol request for the window.
*/
void AddMotifCloseCallback(Widget shell, XtCallbackProc closeCB, void *arg)
{
    static Atom wmpAtom, dwAtom = 0;
    Display *display = XtDisplay(shell);

    /* deactivate the built in delete response of killing the application */
    XtVaSetValues(shell, XmNdeleteResponse, XmDO_NOTHING, NULL);

    /* add a delete window protocol callback instead */
    if (dwAtom == 0) {
    	wmpAtom = XmInternAtom(display, "WM_PROTOCOLS", FALSE);
    	dwAtom = XmInternAtom(display, "WM_DELETE_WINDOW", FALSE);
    }
    XmAddProtocolCallback(shell, wmpAtom, dwAtom, closeCB, arg);
}

/*
** Motif still generates spurious passive grab warnings on both IBM and SGI
** This routine suppresses them.
** (amai, 20011121:)
** And triggers an annoying message on DEC systems on alpha ->
** See Xt sources, xc/lib/Xt/Error.c:DefaultMsg()):
**    actually for some obscure reasons they check for XtError/Warning
**    handlers being installed when running as a root process!
** Since this handler doesn't help on non-effected systems we should only
** use it if necessary.
*/
void SuppressPassiveGrabWarnings(void)
{
#if !defined(__alpha) && !defined(__EMX__)
    XtSetWarningHandler(warnHandlerCB);
#endif
}

/*
** This routine kludges around the problem of backspace not being mapped
** correctly when Motif is used between a server with a delete key in
** the traditional typewriter backspace position and a client that
** expects a backspace key in that position.  Though there are three
** distinct levels of key re-mapping in effect when a user is running
** a Motif application, none of these is really appropriate or effective
** for eliminating the delete v.s. backspace problem.  Our solution is,
** sadly, to eliminate the forward delete functionality of the delete key
** in favor of backwards delete for both keys.  So as not to prevent the
** user or the application from applying other translation table re-mapping,
** we apply re-map the key as a post-processing step, applied after widget
** creation.  As a result, the re-mapping necessarily becomes embedded
** throughout an application (wherever text widgets are created), and
** within library routines, including the Nirvana utility library.  To
** make this remapping optional, the SetDeleteRemap function provides a
** way for an application to turn this functionality on and off.  It is
** recommended that applications that use this routine provide an
** application resource called remapDeleteKey so savvy users can get
** their forward delete functionality back.
*/
void RemapDeleteKey(Widget w)
{
    static XtTranslations table = NULL;
    static char *translations =
    	"~Shift~Ctrl~Meta~Alt<Key>osfDelete: delete-previous-character()\n";

    if (RemapDeleteEnabled) {
    	if (table == NULL)
    	    table = XtParseTranslationTable(translations);
    	XtOverrideTranslations(w, table);
    }
}

void SetDeleteRemap(int state)
{
    RemapDeleteEnabled = state;
}


/* 
** The routine adds the passed in top-level Widget's window to our
** window group.  On the first call a dummy unmapped window will
** be created to be our leader.  This must not be called before the
** Widget has be realized and should be called before the window is
** mapped.
*/
static void setWindowGroup(Widget shell) {
    static int firstTime = True;
    static Window groupLeader;
    Display *display = XtDisplay(shell);
    XWMHints *wmHints;

    if (firstTime) {
    	/* Create a dummy window to be the group leader for our windows */
        String name, class;
    	XClassHint *classHint;
	
    	groupLeader = XCreateSimpleWindow(display, 
                RootWindow(display, DefaultScreen(display)), 
		1, 1, 1, 1, 0, 0, 0);
		
	/* Set it's class hint so it will be identified correctly by the 
	   window manager */
    	XtGetApplicationNameAndClass(display, &name, &class);
	classHint = XAllocClassHint();
	classHint->res_name = name;
	classHint->res_class = class;
	XSetClassHint(display, groupLeader, classHint);
	XFree(classHint);
	
    	firstTime = False;
    }

    /* Set the window group hint for this shell's window */
    wmHints = XGetWMHints(display, XtWindow(shell));
    wmHints->window_group = groupLeader;
    wmHints->flags |= WindowGroupHint;
    XSetWMHints(display, XtWindow(shell), wmHints);
    XFree(wmHints);
}

/*
** This routine resolves a window manager protocol incompatibility between
** the X toolkit and several popular window managers.  Using this in place
** of XtRealizeWidget will realize the window in a way which allows the
** affected window managers to apply their own placement strategy to the
** window, as opposed to forcing the window to a specific location.
**
** One of the hints in the WM_NORMAL_HINTS protocol, PPlacement, gets set by
** the X toolkit (probably part of the Core or Shell widget) when a shell
** widget is realized to the value stored in the XmNx and XmNy resources of the
** Core widget.  While callers can set these values, there is no "unset" value
** for these resources.  On systems which are more Motif aware, a PPosition
** hint of 0,0, which is the default for XmNx and XmNy, is interpreted as,
** "place this as if no hints were specified".  Unfortunately the fvwm family
** of window managers, which are now some of the most popular, interpret this
** as "place this window at (0,0)".  This routine intervenes between the
** realizing and the mapping of the window to remove the inappropriate
** PPlacement hint.
*/ 

void RemovePPositionHint(Widget shell)
{
    XSizeHints *hints = XAllocSizeHints();
    long suppliedHints;

    /* Get rid of the incorrect WMNormal hint */
    if (XGetWMNormalHints(XtDisplay(shell), XtWindow(shell), hints,
    	    &suppliedHints)) 
    {
	hints->flags &= ~PPosition;
	XSetWMNormalHints(XtDisplay(shell), XtWindow(shell), hints);
    }

    XFree(hints);
}

void RealizeWithoutForcingPosition(Widget shell)
{
    Boolean mappedWhenManaged;
    
    /* Temporarily set value of XmNmappedWhenManaged
       to stop the window from popping up right away */
    XtVaGetValues(shell, XmNmappedWhenManaged, &mappedWhenManaged, NULL);
    XtVaSetValues(shell, XmNmappedWhenManaged, False, NULL);
    
    /* Realize the widget in unmapped state */
    XtRealizeWidget(shell);

    /* Remove the hint */
    RemovePPositionHint(shell);
    
    /* Set WindowGroupHint so the NEdit icons can be grouped; this
       seems to be necessary starting with Gnome 2.0  */
    setWindowGroup(shell);
    
    /* Map the widget */
    XtMapWidget(shell);
    
    /* Restore the value of XmNmappedWhenManaged */
    XtVaSetValues(shell, XmNmappedWhenManaged, mappedWhenManaged, NULL);
}

/*
** Older X applications and X servers were mostly designed to operate with
** visual class PseudoColor, because older displays were at most 8 bits
** deep.  Modern X servers, however, usually support 24 bit depth and other
** color models.  Sun (and others?) still sets their default visual to
** 8-bit PseudoColor, because some of their X applications don't work
** properly with the other color models.  The problem with PseudoColor, of
** course, is that users run out of colors in the default colormap, and if
** they install additional colormaps for individual applications, colors
** flash and change weirdly when you change your focus from one application
** to another.
**
** In addition to the poor choice of default, a design flaw in Xt makes it
** impossible even for savvy users to specify the XtNvisual resource to
** switch to a deeper visual.  The problem is that the colormap resource is
** processed independently of the visual resource, and usually results in a
** colormap for the default visual rather than for the user-selected one.
**
** This routine should be called before creating a shell widget, to
** pre-process the visual, depth, and colormap resources, and return the
** proper values for these three resources to be passed to XtAppCreateShell.
** Applications which actually require a particular color model (i.e. for
** doing color table animation or dynamic color assignment) should not use
** this routine.
**
** Note that a consequence of using the "best" as opposed to the default
** visual is that some color resources are still converted with the default
** visual (particularly *background), and these must be avoided by widgets
** which are allowed to handle any visual.
**
** Returns True if the best visual is the default, False otherwise.
*/
Boolean FindBestVisual(Display *display, const char *appName, const char *appClass,
	Visual **visual, int *depth, Colormap *colormap)
{
    char rsrcName[256], rsrcClass[256], *valueString, *type, *endPtr;
    XrmValue value;
    int screen = DefaultScreen(display);
    int reqDepth = -1;
    long reqID = -1; /* should hold a 'VisualID' and a '-1' ... */
    int reqClass = -1;
    int installColormap = FALSE;
    int maxDepth, bestClass, bestVisual, nVis, i, j;
    XVisualInfo visTemplate, *visList = NULL;
    static Visual *cachedVisual = NULL;
    static Colormap cachedColormap;
    static int cachedDepth = 0;
    int bestClasses[] = {StaticGray, GrayScale, StaticColor, PseudoColor,
    	    DirectColor, TrueColor};

    /* If results have already been computed, just return them */
    if (cachedVisual != NULL) {
	*visual = cachedVisual;
	*depth = cachedDepth;
	*colormap = cachedColormap;
	return (*visual == DefaultVisual(display, screen));
    }
    
    /* Read the visualID and installColormap resources for the application.
       visualID can be specified either as a number (the visual id as
       shown by xdpyinfo), as a visual class name, or as Best or Default. */
    sprintf(rsrcName,"%s.%s", appName, "visualID");
    sprintf(rsrcClass, "%s.%s", appClass, "VisualID");
    if (XrmGetResource(XtDatabase(display), rsrcName, rsrcClass, &type,
	    &value)) {
	valueString = value.addr;
	reqID = (int)strtol(valueString, &endPtr, 0);
	if (endPtr == valueString) {
	    reqID = -1;
	    if (stripCaseCmp(valueString, "Default"))
		reqID = DefaultVisual(display, screen)->visualid;
	    else if (stripCaseCmp(valueString, "StaticGray"))
		reqClass = StaticGray;
	    else if (stripCaseCmp(valueString, "StaticColor"))
		reqClass = StaticColor;
	    else if (stripCaseCmp(valueString, "TrueColor"))
		reqClass = TrueColor;
	    else if (stripCaseCmp(valueString, "GrayScale"))
		reqClass = GrayScale;
	    else if (stripCaseCmp(valueString, "PseudoColor"))
		reqClass = PseudoColor;
	    else if (stripCaseCmp(valueString, "DirectColor"))
		reqClass = DirectColor;
	    else if (!stripCaseCmp(valueString, "Best"))
		fprintf(stderr, "Invalid visualID resource value\n");
	}
    }
    sprintf(rsrcName,"%s.%s", appName, "installColormap");
    sprintf(rsrcClass, "%s.%s", appClass, "InstallColormap");
    if (XrmGetResource(XtDatabase(display), rsrcName, rsrcClass, &type,
	    &value)) {
	if (stripCaseCmp(value.addr, "Yes") || stripCaseCmp(value.addr, "True"))
	    installColormap = TRUE;
    }

    visTemplate.screen = screen;
        
    /* Generate a list of visuals to consider.  (Note, vestigial code for
       user-requested visual depth is left in, just in case that function
       might be needed again, but it does nothing). */
    if (reqID != -1) {
	visTemplate.visualid = reqID;
	visList = XGetVisualInfo(display, VisualScreenMask|VisualIDMask,
                                 &visTemplate, &nVis);
	if (visList == NULL)
	    fprintf(stderr, "VisualID resource value not valid\n");
    }
    if (visList == NULL && reqClass != -1 && reqDepth != -1) {
	visTemplate.class = reqClass;
	visTemplate.depth = reqDepth;
    	visList = XGetVisualInfo(display,
		                 VisualScreenMask| VisualClassMask | VisualDepthMask,
                                 &visTemplate, &nVis);
    	if (visList == NULL)
	    fprintf(stderr, "Visual class/depth combination not available\n");
    }
    if (visList == NULL && reqClass != -1) {
 	visTemplate.class = reqClass;
    	visList = XGetVisualInfo(display, VisualScreenMask|VisualClassMask, 
                                 &visTemplate, &nVis);
    	if (visList == NULL)
	    fprintf(stderr,
		    "Visual Class from resource \"visualID\" not available\n");
    }
    if (visList == NULL && reqDepth != -1) {
	visTemplate.depth = reqDepth;
    	visList = XGetVisualInfo(display, VisualScreenMask|VisualDepthMask,
                                 &visTemplate, &nVis);
    	if (visList == NULL)
	    fprintf(stderr, "Requested visual depth not available\n");
    }
    if (visList == NULL) {
	visList = XGetVisualInfo(display, VisualScreenMask, &visTemplate, &nVis);
	if (visList == NULL) {
	    fprintf(stderr, "Internal Error: no visuals available?\n");
	    *visual = DefaultVisual(display, screen);
	    *depth =  DefaultDepth(display, screen);
    	    *colormap = DefaultColormap(display, screen);
    	    return True;
	}
    }
    
    /* Choose among the visuals in the candidate list.  Prefer maximum
       depth first then matching default, then largest value of bestClass
       (I'm not sure whether we actually care about class) */
    maxDepth = 0;
    bestClass = 0;
    bestVisual = 0;
    for (i=0; i < nVis; i++) {
        /* X.Org 6.8+ 32-bit visuals (with alpha-channel) cause a lot of
           problems, so we have to skip them. We already try this by setting
           the environment variable XLIB_SKIP_ARGB_VISUALS at startup (in
           nedit.c), but that doesn't cover the case where NEdit is running on
           a host that doesn't use the X.Org X libraries but is displaying
           remotely on an X.Org server. Therefore, this additional check is
           added. 
           Note that this check in itself is not sufficient. There have been
           bug reports that seemed to indicate that non-32-bit visuals with an
           alpha-channel exist. The combined approach (env. var. + 32-bit
           check) should cover the vast majority of the cases, though. */
	if (visList[i].depth >= 32 &&
	    strstr(ServerVendor(display), "X.Org") != 0) {
	    continue;
	}
	if (visList[i].depth > maxDepth) {
	    maxDepth = visList[i].depth;
	    bestClass = 0;
	    bestVisual = i;
	}
	if (visList[i].depth == maxDepth) {
	    if (visList[i].visual == DefaultVisual(display, screen))
		bestVisual = i;
	    if (visList[bestVisual].visual != DefaultVisual(display, screen)) {
		for (j = 0; j < (int)XtNumber(bestClasses); j++) {
		    if (visList[i].class == bestClasses[j] && j > bestClass) {
			bestClass = j;
			bestVisual = i;
		    }
		}
	    }
	}
    }
    *visual = cachedVisual = visList[bestVisual].visual;
    *depth = cachedDepth = visList[bestVisual].depth;
    
    /* If the chosen visual is not the default, it needs a colormap allocated */
    if (*visual == DefaultVisual(display, screen) && !installColormap)
	*colormap = cachedColormap = DefaultColormap(display, screen);
    else {
	*colormap = cachedColormap = XCreateColormap(display,
		RootWindow(display, screen), cachedVisual, AllocNone);
	XInstallColormap(display, cachedColormap);
    }
    /* printf("Chose visual with depth %d, class %d, colormap %ld, id 0x%x\n",
	    visList[bestVisual].depth, visList[bestVisual].class,
	    *colormap, cachedVisual->visualid); */
    /* Fix memory leak */
    if (visList != NULL) {
       XFree(visList);
    }
    
    return (*visual == DefaultVisual(display, screen));
}

/*
** If you want to use a non-default visual with Motif, shells all have to be
** created with that visual, depth, and colormap, even if the parent has them
** set up properly. Substituting these routines, will append visual args copied
** from the parent widget (CreatePopupMenu and CreatePulldownMenu), or from the
** best visual, obtained via FindBestVisual above (CreateShellWithBestVis).
*/
Widget CreateDialogShell(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    return addParentVisArgsAndCall(XmCreateDialogShell, parent, name, arglist,
	    argcount);
}


Widget CreatePopupMenu(Widget parent, char *name, ArgList arglist,
	Cardinal argcount)
{
    return addParentVisArgsAndCall(XmCreatePopupMenu, parent, name,
	    arglist, argcount);
}


Widget CreatePulldownMenu(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    return addParentVisArgsAndCall(XmCreatePulldownMenu, parent, name, arglist,
	    argcount);
}


Widget CreatePromptDialog(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    return addParentVisArgsAndCall(XmCreatePromptDialog, parent, name, arglist,
	    argcount);
}


Widget CreateSelectionDialog(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    Widget dialog = addParentVisArgsAndCall(XmCreateSelectionDialog, parent, name,
	    arglist, argcount);
    AddMouseWheelSupport(XmSelectionBoxGetChild(dialog, XmDIALOG_LIST));
    return dialog;
}


Widget CreateFormDialog(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    return addParentVisArgsAndCall(XmCreateFormDialog, parent, name, arglist,
	    argcount);
}


Widget CreateFileSelectionDialog(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    Widget dialog = addParentVisArgsAndCall(XmCreateFileSelectionDialog, parent, 
            name, arglist, argcount);

    AddMouseWheelSupport(XmFileSelectionBoxGetChild(dialog, XmDIALOG_LIST));
    AddMouseWheelSupport(XmFileSelectionBoxGetChild(dialog, XmDIALOG_DIR_LIST));
    return dialog;
}


Widget CreateQuestionDialog(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    return addParentVisArgsAndCall(XmCreateQuestionDialog, parent, name,
	    arglist, argcount);
}


Widget CreateMessageDialog(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    return addParentVisArgsAndCall(XmCreateMessageDialog, parent, name,
	    arglist, argcount);
}


Widget CreateErrorDialog(Widget parent, char *name,
	ArgList arglist, Cardinal  argcount)
{
    return addParentVisArgsAndCall(XmCreateErrorDialog, parent, name, arglist,
	    argcount);
}

Widget CreateWidget(Widget parent, const char *name, WidgetClass class,
	ArgList arglist, Cardinal  argcount)
{
    Widget result;
    ArgList al = addParentVisArgs(parent, arglist, &argcount);
    result = XtCreateWidget(name, class, parent, al, argcount);
    NEditFree((char *)al);
    return result;
}

Widget CreateShellWithBestVis(String appName, String appClass,
	   WidgetClass class, Display *display, ArgList args, Cardinal nArgs)
{
    Visual *visual;
    int depth;
    Colormap colormap;
    ArgList al;
    Cardinal ac = nArgs;
    Widget result;

    FindBestVisual(display, appName, appClass, &visual, &depth, &colormap);
    al = (ArgList)NEditMalloc(sizeof(Arg) * (nArgs + 3));
    if (nArgs != 0)
    	memcpy(al, args, sizeof(Arg) * nArgs);
    XtSetArg(al[ac], XtNvisual, visual); ac++;
    XtSetArg(al[ac], XtNdepth, depth); ac++;
    XtSetArg(al[ac], XtNcolormap, colormap); ac++;
    result = XtAppCreateShell(appName, appClass, class, display, al, ac);
    NEditFree((char *)al);
    return result;
}


Widget CreatePopupShellWithBestVis(String shellName, WidgetClass class,
    Widget parent, ArgList arglist, Cardinal argcount)
{
   Widget result;
   ArgList al = addParentVisArgs(parent, arglist, &argcount);
   result = XtCreatePopupShell(shellName, class, parent, al, argcount);
   NEditFree((char *)al);
   return result;
}

/*
** Extends an argument list for widget creation with additional arguments
** for visual, colormap, and depth. The original argument list is not altered
** and it's the caller's responsability to free the returned list.
*/
static ArgList addParentVisArgs(Widget parent, ArgList arglist, 
   Cardinal *argcount)
{
    Visual *visual;
    int depth;
    Colormap colormap;
    ArgList al;
    Widget parentShell = parent;
    
    /* Find the application/dialog/menu shell at the top of the widget
       hierarchy, which has the visual resource being used */
    while (True) {
    	if (XtIsShell(parentShell))
    	    break;
    	if (parentShell == NULL) {
	    fprintf(stderr, "failed to find shell\n");
	    exit(EXIT_FAILURE);
	}
    	parentShell = XtParent(parentShell);
    }

    /* Add the visual, depth, and colormap resources to the argument list */
    XtVaGetValues(parentShell, XtNvisual, &visual, XtNdepth, &depth,
	    XtNcolormap, &colormap, NULL);
    al = (ArgList)NEditMalloc(sizeof(Arg) * ((*argcount) + 3));
    if ((*argcount) != 0)
    	memcpy(al, arglist, sizeof(Arg) * (*argcount));

    /* For non-Lesstif versions, the visual, depth, and colormap are now set 
       globally via the resource database. So strictly spoken, it is no 
       longer necessary to set them explicitly for every shell widget. 
       
       For Lesstif, however, this doesn't work. Luckily, Lesstif handles 
       non-default visuals etc. properly for its own shells and 
       we can take care of things for our shells (eg, call tips) here. */
    XtSetArg(al[*argcount], XtNvisual, visual); (*argcount)++;
    XtSetArg(al[*argcount], XtNdepth, depth); (*argcount)++;
    XtSetArg(al[*argcount], XtNcolormap, colormap); (*argcount)++;
    return al;
}


/*
** Calls one of the Motif widget creation routines, splicing in additional
** arguments for visual, colormap, and depth.
*/
static Widget addParentVisArgsAndCall(MotifDialogCreationCall createRoutine,
	Widget parent, char *name, ArgList arglist, Cardinal argcount)
{
    Widget result;
    ArgList al = addParentVisArgs(parent, arglist, &argcount);
    result = (*createRoutine)(parent, name, al, argcount);
    NEditFree((char *)al);
    return result;
}

/*
** ManageDialogCenteredOnPointer is used in place of XtManageChild for
** popping up a dialog to enable the dialog to be centered under the
** mouse pointer.  Whether it pops up the dialog centered under the pointer
** or in its default position centered over the parent widget, depends on
** the value set in the SetPointerCenteredDialogs call.
** Additionally, this function constrains the size of the dialog to the
** screen's size, to avoid insanely wide dialogs with obscured buttons.
*/ 
void ManageDialogCenteredOnPointer(Widget dialogChild)
{
    Widget shell = XtParent(dialogChild);
    Window root, child;
    unsigned int mask;
    unsigned int width, height, borderWidth, depth;
    int x, y, winX, winY, maxX, maxY, maxWidth, maxHeight;
    Dimension xtWidth, xtHeight;
    Boolean mappedWhenManaged;
    static const int slop = 25;
    
    /* Temporarily set value of XmNmappedWhenManaged
       to stop the dialog from popping up right away */
    XtVaGetValues(shell, XmNmappedWhenManaged, &mappedWhenManaged, NULL);
    XtVaSetValues(shell, XmNmappedWhenManaged, False, NULL);

    /* Ensure that the dialog doesn't get wider/taller than the screen.
       We use a hard-coded "slop" size because we don't know the border
       width until it's on screen, and by then it's too late.  Putting
       this before managing the widgets allows it to get the geometry
       right on the first pass and also prevents the user from
       accidentally resizing too wide. */
    maxWidth = XtScreen(shell)->width - slop;
    maxHeight = XtScreen(shell)->height - slop;

    XtVaSetValues(shell, 
                  XmNmaxWidth, maxWidth,
                  XmNmaxHeight, maxHeight,
                  NULL);
    
    /* Manage the dialog */
    XtManageChild(dialogChild);

    /* Check to see if the window manager doesn't respect XmNmaxWidth
       and XmNmaxHeight on the first geometry pass (sawfish, twm, fvwm).
       For this to work XmNresizePolicy must be XmRESIZE_NONE, otherwise
       the dialog will try to expand anyway. */
    XtVaGetValues(shell, XmNwidth, &xtWidth, XmNheight, &xtHeight, NULL);
    if (xtWidth > maxWidth)
        XtVaSetValues(shell, XmNwidth, (Dimension) maxWidth, NULL);
    if (xtHeight > maxHeight)
        XtVaSetValues(shell, XmNheight, (Dimension) maxHeight, NULL);
        
    /* Only set the x/y position if the centering option is enabled.
       Avoid getting the coordinates if not so, to save a few round-trips
       to the server. */
    if (PointerCenteredDialogsEnabled) {
        /* Get the pointer position (x, y) */
        XQueryPointer(XtDisplay(shell), XtWindow(shell), &root, &child,
	        &x, &y, &winX, &winY, &mask);

        /* Translate the pointer position (x, y) into a position for the new
           window that will place the pointer at its center */
        XGetGeometry(XtDisplay(shell), XtWindow(shell), &root, &winX, &winY,
    	        &width, &height, &borderWidth, &depth);
        width += 2 * borderWidth;
        height += 2 * borderWidth;
    
	x -= width/2;
	y -= height/2;

	/* Ensure that the dialog remains on screen */
	maxX = maxWidth - width;
	maxY = maxHeight - height;
	if (x > maxX) x = maxX;
	if (x < 0) x = 0;
	if (y > maxY) y = maxY;
	if (y < 0) y = 0;

        /* Some window managers (Sawfish) don't appear to respond
           to the geometry set call in synchronous mode.  This causes
           the window to delay XmNwmTimeout (default 5 seconds) before
           posting, and it is very annoying.  See "man VendorShell" for
           more info. */
        XtVaSetValues(shell, XmNuseAsyncGeometry, True, NULL);
 
        /* Set desired window position in the DialogShell */
        XtVaSetValues(shell, XmNx, x, XmNy, y, NULL);
    }
    
    /* Map the widget */
    XtMapWidget(shell);

    /* Restore the value of XmNmappedWhenManaged */
    XtVaSetValues(shell, XmNmappedWhenManaged, mappedWhenManaged, NULL);
}

/*
** Cause dialogs created by libNUtil.a routines (such as DialogF),
** and dialogs which use ManageDialogCenteredOnPointer to pop up
** over the pointer (state = True), or pop up in their default
** positions (state = False)
*/
void SetPointerCenteredDialogs(int state)
{
    PointerCenteredDialogsEnabled = state;
}


/*
** Raise a window to the top and give it the input focus.  Setting input focus
** is important on systems which use explict (rather than pointer) focus.
**
** The X alternatives XMapRaised, and XSetInputFocus both have problems.
** XMapRaised only gives the window the focus if it was initially not visible,
** and XSetInputFocus sets the input focus, but crashes if the window is not
** visible.
**
** This routine should also be used in the case where a dialog is popped up and
** subsequent calls to the dialog function use a flag, or the XtIsManaged, to
** decide whether to create a new instance of the dialog, because on slower
** systems, events can intervene while a dialog is still on its way up and its
** window is still invisible, causing a subtle crash potential if
** XSetInputFocus is used.
*/
void RaiseDialogWindow(Widget shell)
{
    RaiseWindow(XtDisplay(shell), XtWindow(shell), True);
}

void RaiseShellWindow(Widget shell, Boolean focus)
{
    RaiseWindow(XtDisplay(shell), XtWindow(shell), focus);
}

void RaiseWindow(Display *display, Window w, Boolean focus)
{
    if (focus) {
        XWindowAttributes winAttr;

        XGetWindowAttributes(display, w, &winAttr);
        if (winAttr.map_state == IsViewable)
            XSetInputFocus(display, w, RevertToParent, CurrentTime);	
    }

    WmClientMsg(display, w, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
    XMapRaised(display, w);
}

/*
** Add a handler for mnemonics in a dialog (Motif currently only handles
** mnemonics in menus) following the example of M.S. Windows.  To add
** mnemonics to a dialog, set the XmNmnemonic resource, as you would in
** a menu, on push buttons or toggle buttons, and call this function
** when the dialog is fully constructed.  Mnemonics added or changed
** after this call will not be noticed.  To add a mnemonic to a text field
** or list, set the XmNmnemonic resource on the appropriate label and set
** the XmNuserData resource of the label to the widget to get the focus
** when the mnemonic is typed.
*/
void AddDialogMnemonicHandler(Widget dialog, int unmodifiedToo)
{
    XtAddEventHandler(dialog, KeyPressMask, False,
    	    (XtEventHandler)mnemonicCB, (XtPointer)0);
    addMnemonicGrabs(dialog, dialog, unmodifiedToo);
}

/*
** Removes the event handler and key-grabs added by AddDialogMnemonicHandler
*/
void RemoveDialogMnemonicHandler(Widget dialog)
{
    XtUngrabKey(dialog, AnyKey, Mod1Mask);
    XtRemoveEventHandler(dialog, KeyPressMask, False,
    	    (XtEventHandler)mnemonicCB, (XtPointer)0);
}

/*
** Patch around Motif's poor handling of menu accelerator keys.  Motif
** does not process menu accelerators when the caps lock or num lock
** keys are engaged.  To enable accelerators in these cases, call this
** routine with the completed menu bar widget as "topMenuContainer", and
** the top level shell widget as "topWidget".  It will add key grabs for
** all of the accelerators it finds in the topMenuContainer menu tree, and
** an event handler which can process dropped accelerator events by (again)
** traversing the menu tree looking for matching accelerators, and invoking
** the appropriate button actions.  Any dynamic additions to the menus
** require a call to UpdateAccelLockPatch to add the additional grabs.
** Unfortunately, these grabs can not be removed.
*/
void AccelLockBugPatch(Widget topWidget, Widget topMenuContainer)
{
    XtAddEventHandler(topWidget, KeyPressMask, False, lockCB, topMenuContainer);
    addAccelGrabs(topWidget, topMenuContainer);
}

/*
** Add additional key grabs for new menu items added to the menus, for
** patching around the Motif Caps/Num Lock problem. "topWidget" must be
** the same widget passed in the original call to AccelLockBugPatch.
*/
void UpdateAccelLockPatch(Widget topWidget, Widget newButton)
{
    addAccelGrab(topWidget, newButton);
}

/*
** PopDownBugPatch
**
** Under some circumstances, popping down a dialog and its parent in
** rapid succession causes a crash.  This routine delays and
** processs events until receiving a ReparentNotify event.
** (I have no idea why a ReparentNotify event occurs at all, but it does
** mark the point where it is safe to destroy or pop down the parent, and
** it might have something to do with the bug.)  There is a failsafe in
** the form of a ~1.5 second timeout in case no ReparentNotify arrives.
** Use this sparingly, only when real crashes are observed, and periodically
** check to make sure that it is still necessary.
*/
void PopDownBugPatch(Widget w)
{
    time_t stopTime;

    stopTime = time(NULL) + 1;
    while (time(NULL) <= stopTime) {
    	XEvent event;
    	XtAppContext context = XtWidgetToApplicationContext(w);
    	XtAppPeekEvent(context, &event);
    	if (event.xany.type == ReparentNotify)
    	    return;
    	XtAppProcessEvent(context, XtIMAll);
    }
}

/*
** Convert a compound string to a C style null terminated string.
** Returned string must be freed by the caller.
*/
char *GetXmStringText(XmString fromString)
{
    XmStringContext context;
    char *text, *toPtr, *toString, *fromPtr;
    XmStringCharSet charset;
    XmStringDirection direction;
    Boolean separator;
    
    /* Malloc a buffer large enough to hold the string.  XmStringLength
       should always be slightly longer than necessary, but won't be
       shorter than the equivalent null-terminated string */ 
    toString = (char*)NEditMalloc(XmStringLength(fromString));
    
    /* loop over all of the segments in the string, copying each segment
       into the output string and converting separators into newlines */
    XmStringInitContext(&context, fromString);
    toPtr = toString;
    while (XmStringGetNextSegment(context, &text,
    	    &charset, &direction, &separator)) {
    	for (fromPtr=text; *fromPtr!='\0'; fromPtr++)
    	    *toPtr++ = *fromPtr;
    	if (separator)
    	    *toPtr++ = '\n';
	NEditFree(text);
	NEditFree(charset);
    }
    
    /* terminate the string, free the context, and return the string */
    *toPtr++ = '\0';
    XmStringFreeContext(context);
    return toString;
}

/*
** Get the XFontStruct that corresponds to the default (first) font in
** a Motif font list.  Since Motif stores this, it saves us from storing
** it or querying it from the X server.
*/
XFontStruct *GetDefaultFontStruct(Display *d, XmFontList font)
{
    XFontStruct *fs;
    XmFontContext context;
    XmStringCharSet charset;

    XmFontListInitFontContext(&context, font);
    XmFontListGetNextFont(context, &charset, &fs);
    XmFontListFreeFontContext(context);
    NEditFree(charset);

    /* FontList might be a render table with no only XFT fonts */    
    if (fs == NULL) {
        fs = XLoadQueryFont(d, "fixed");
    }

    if (fs == NULL) {
        fprintf(stderr, "Unabled to load any fallback fonts.\n");
        exit(EXIT_FAILURE);
    }
        
    return fs;
}
   
/*
** Create a string table suitable for passing to XmList widgets
*/
XmString* StringTable(int count, ... )
{
    va_list ap;
    XmString *array;
    int i;
    char *str;

    va_start(ap, count);
    array = (XmString*)NEditMalloc((count+1) * sizeof(XmString));
    for(i = 0;  i < count; i++ ) {
    	str = va_arg(ap, char *);
	array[i] = XmStringCreateSimple(str);
    }
    array[i] = (XmString)0;
    va_end(ap);
    return(array);
}

void FreeStringTable(XmString *table)
{
    int i;

    for(i = 0; table[i] != 0; i++)
	XmStringFree(table[i]);
    NEditFree((char *)table);
}

/*
** Simulate a button press.  The purpose of this routine is show users what
** is happening when they take an action with a non-obvious side effect,
** such as when a user double clicks on a list item.  The argument is an
** XmPushButton widget to "press"
*/ 
void SimulateButtonPress(Widget widget)
{
    XEvent keyEvent;
    
    memset((char *)&keyEvent, 0, sizeof(XKeyPressedEvent));
    keyEvent.type = KeyPress;
    keyEvent.xkey.serial = 1;
    keyEvent.xkey.send_event = True;

    if (XtIsSubclass(widget, xmGadgetClass))
    {
        /* On some Motif implementations, asking a gadget for its
           window will crash, rather than return the window of its
           parent. */
        Widget parent = XtParent(widget);
        keyEvent.xkey.display = XtDisplay(parent);
        keyEvent.xkey.window = XtWindow(parent);

        XtCallActionProc(parent, "ManagerGadgetSelect",
                         &keyEvent, NULL, 0);
    }                 
    else
    {
        keyEvent.xkey.display = XtDisplay(widget);
        keyEvent.xkey.window = XtWindow(widget);

        XtCallActionProc(widget, "ArmAndActivate", &keyEvent, NULL, 0);
    }
}

/*
** Add an item to an already established pull-down or pop-up menu, including
** mnemonics, accelerators and callbacks.
*/
Widget AddMenuItem(Widget parent, char *name, char *label,
			  char mnemonic, char *acc, char *accText,
			  XtCallbackProc callback, void *cbArg)
{
    Widget button;
    XmString st1, st2;
    
    button = XtVaCreateManagedWidget(name, xmPushButtonWidgetClass, parent, 
    	XmNlabelString, st1=XmStringCreateSimple(label),
    	XmNmnemonic, mnemonic,
    	XmNacceleratorText, st2=XmStringCreateSimple(accText),
    	XmNaccelerator, acc, NULL);
    XtAddCallback(button, XmNactivateCallback, callback, cbArg);
    XmStringFree(st1);
    XmStringFree(st2);
    return button;
}

/*
** Add a toggle button item to an already established pull-down or pop-up
** menu, including mnemonics, accelerators and callbacks.
*/
Widget AddMenuToggle(Widget parent, char *name, char *label,
		 	    char mnemonic, char *acc, char *accText,
		  	    XtCallbackProc callback, void *cbArg, int set)
{
    Widget button;
    XmString st1, st2;
    
    button = XtVaCreateManagedWidget(name, xmToggleButtonWidgetClass, parent, 
    	XmNlabelString, st1=XmStringCreateSimple(label),
    	XmNmnemonic, mnemonic,
    	XmNacceleratorText, st2=XmStringCreateSimple(accText),
    	XmNaccelerator, acc,
    	XmNset, set, NULL);
    XtAddCallback(button, XmNvalueChangedCallback, callback, cbArg);
    XmStringFree(st1);
    XmStringFree(st2);
    return button;
}

/*
** Add a sub-menu to an established pull-down or pop-up menu, including
** mnemonics, accelerators and callbacks.  Returns the menu pane of the
** new sub menu.
*/
Widget AddSubMenu(Widget parent, char *name, char *label, char mnemonic)
{
    Widget menu;
    XmString st1;
    
    menu = CreatePulldownMenu(parent, name, NULL, 0);
    XtVaCreateManagedWidget(name, xmCascadeButtonWidgetClass, parent, 
    	XmNlabelString, st1=XmStringCreateSimple(label),
    	XmNmnemonic, mnemonic,
    	XmNsubMenuId, menu, NULL);
    XmStringFree(st1);
    return menu;
}

/*
** SetIntText
**
** Set the text of a motif label to show an integer
*/
void SetIntText(Widget text, int value)
{
    char labelString[20];
    
    sprintf(labelString, "%d", value);
    XmTextSetString(text, labelString);
}

/*
** GetIntText, GetFloatText, GetIntTextWarn, GetFloatTextWarn
**
** Get the text of a motif text widget as an integer or floating point number.
** The functions will return TEXT_READ_OK of the value was read correctly.
** If not, they will return either TEXT_IS_BLANK, or TEXT_NOT_NUMBER.  The
** GetIntTextWarn and GetFloatTextWarn will display a dialog warning the
** user that the value could not be read.  The argument fieldName is used
** in the dialog to help the user identify where the problem is.  Set
** warnBlank to true if a blank field is also considered an error.
*/
int GetFloatText(Widget text, double *value)
{
    char *strValue, *endPtr;
    int retVal;

    strValue = XmTextGetString(text);	/* Get Value */
    removeWhiteSpace(strValue);		/* Remove blanks and tabs */
    *value = strtod(strValue, &endPtr);	/* Convert string to double */
    if (strlen(strValue) == 0)		/* String is empty */
	retVal = TEXT_IS_BLANK;
    else if (*endPtr != '\0')		/* Whole string not parsed */
    	retVal = TEXT_NOT_NUMBER;
    else
	retVal = TEXT_READ_OK;
    NEditFree(strValue);
    return retVal;
}

int GetIntText(Widget text, int *value)
{
    char *strValue, *endPtr;
    int retVal;

    strValue = XmTextGetString(text);		/* Get Value */
    removeWhiteSpace(strValue);			/* Remove blanks and tabs */
    *value = strtol(strValue, &endPtr, 10);	/* Convert string to long */
    if (strlen(strValue) == 0)			/* String is empty */
	retVal = TEXT_IS_BLANK;
    else if (*endPtr != '\0')			/* Whole string not parsed */
    	retVal = TEXT_NOT_NUMBER;
    else
	retVal = TEXT_READ_OK;
    NEditFree(strValue);
    return retVal;
}

int GetFloatTextWarn(Widget text, double *value, const char *fieldName,
                     int warnBlank)
{
    int result;
    char *valueStr;
    
    result = GetFloatText(text, value);
    if (result == TEXT_READ_OK || (result == TEXT_IS_BLANK && !warnBlank))
    	return result;
    valueStr = XmTextGetString(text);

    if (result == TEXT_IS_BLANK)
    {
        DialogF(DF_ERR, text, 1, "Warning", "Please supply %s value", "OK",
                fieldName);
    } else /* TEXT_NOT_NUMBER */
    {
        DialogF (DF_ERR, text, 1, "Warning", "Can't read %s value: \"%s\"",
                "OK", fieldName, valueStr);
    }

    NEditFree(valueStr);
    return result;
}

int GetIntTextWarn(Widget text, int *value, const char *fieldName, int warnBlank)
{
    int result;
    char *valueStr;
    
    result = GetIntText(text, value);
    if (result == TEXT_READ_OK || (result == TEXT_IS_BLANK && !warnBlank))
    	return result;
    valueStr = XmTextGetString(text);

    if (result == TEXT_IS_BLANK)
    {
        DialogF (DF_ERR, text, 1, "Warning", "Please supply a value for %s",
                "OK", fieldName);
    } else /* TEXT_NOT_NUMBER */
    {
        DialogF (DF_ERR, text, 1, "Warning",
                "Can't read integer value \"%s\" in %s", "OK", valueStr,
                fieldName);
    }

    NEditFree(valueStr);
    return result;
}

int TextWidgetIsBlank(Widget textW)
{
    char *str;
    int retVal;
    
    str = XmTextGetString(textW);
    removeWhiteSpace(str);
    retVal = *str == '\0';
    NEditFree(str);
    return retVal;
}

/*
** Turn a multi-line editing text widget into a fake single line text area
** by disabling the translation for Return.  This is a way to give users
** extra space, by allowing wrapping, but still prohibiting newlines.
** (SINGLE_LINE_EDIT mode can't be used, in this case, because it forces
** the widget to be one line high).
*/
void MakeSingleLineTextW(Widget textW)
{
    static XtTranslations noReturnTable = NULL;
    static char *noReturnTranslations = "<Key>Return: activate()\n";
    
    if (noReturnTable == NULL)
    	noReturnTable = XtParseTranslationTable(noReturnTranslations);
    XtOverrideTranslations(textW, noReturnTable);
}

/*
** Add up-arrow/down-arrow recall to a single line text field.  When user
** presses up-arrow, string is cleared and recent entries are presented,
** moving to older ones as each successive up-arrow is pressed.  Down-arrow
** moves to more recent ones, final down-arrow clears the field.  Associated
** routine, AddToHistoryList, makes maintaining a history list easier.
**
** Arguments are the widget, and pointers to the history list and number of
** items, which are expected to change periodically.
*/
void AddHistoryToTextWidget(Widget textW, char ***historyList, int *nItems)
{
    histInfo *histData;
    
    /* create a data structure for passing history info to the callbacks */
    histData = (histInfo *)NEditMalloc(sizeof(histInfo));
    histData->list = historyList;
    histData->nItems = nItems;
    histData->index = -1;
    
    /* Add an event handler for handling up/down arrow events */
    XtAddEventHandler(textW, KeyPressMask, False,
    	    (XtEventHandler)histArrowKeyEH, histData);
    
    /* Add a destroy callback for freeing history data structure */
    XtAddCallback(textW, XmNdestroyCallback, histDestroyCB, histData);
}

static void histDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    NEditFree((char *)clientData);
}

static void histArrowKeyEH(Widget w, XtPointer callData, XEvent *event,
	Boolean *continueDispatch)
{
    histInfo *histData = (histInfo *)callData;
    KeySym keysym = XLookupKeysym((XKeyEvent *)event, 0);
    
    /* only process up and down arrow keys */
    if (keysym != XK_Up && keysym != XK_Down)
    	return;
    
    /* increment or decrement the index depending on which arrow was pressed */
    histData->index += (keysym == XK_Up) ? 1 : -1;

    /* if the index is out of range, beep, fix it up, and return */
    if (histData->index < -1) {
    	histData->index = -1;
	XBell(XtDisplay(w), 0);
    	return;
    }
    if (histData->index >= *histData->nItems) {
    	histData->index = *histData->nItems - 1;
	XBell(XtDisplay(w), 0);
    	return;
    }
    
    /* Change the text field contents */
    XmTextSetString(w, histData->index == -1 ? "" :
	    (*histData->list)[histData->index]);
}

/*
** Copies a string on to the end of history list, which may be reallocated
** to make room.  If historyList grows beyond its internally set boundary
** for size (HISTORY_LIST_MAX), it is trimmed back to a smaller size
** (HISTORY_LIST_TRIM_TO).  Before adding to the list, checks if the item
** is a duplicate of the last item.  If so, it is not added.
*/	
void AddToHistoryList(char *newItem, char ***historyList, int *nItems)
{
    char **newList;
    int i;
    
    if (*nItems != 0 && !strcmp(newItem, **historyList))
	return;
    if (*nItems == HISTORY_LIST_MAX) {
	for (i=HISTORY_LIST_TRIM_TO; i<HISTORY_LIST_MAX; i++)
	    NEditFree((*historyList)[i]);
	*nItems = HISTORY_LIST_TRIM_TO;
    }
    newList = (char **)NEditMalloc(sizeof(char *) * (*nItems + 1));
    for (i=0; i < *nItems; i++)
	newList[i+1] = (*historyList)[i];
    if (*nItems != 0 && *historyList != NULL)
	NEditFree(*historyList);
    (*nItems)++;
    newList[0] = NEditStrdup(newItem);
    *historyList = newList;
}

/*
** BeginWait/EndWait
**
** Display/Remove a watch cursor over topCursorWidget and its descendents
*/
void BeginWait(Widget topCursorWidget)
{
    Display *display = XtDisplay(topCursorWidget);
    Pixmap pixmap;
    Pixmap maskPixmap;
    XColor xcolors[2];
    static Cursor  waitCursor = 0;
    
    /* if the watch cursor hasn't been created yet, create it */
    if (!waitCursor) {
	pixmap = XCreateBitmapFromData(display, DefaultRootWindow(display),
		(char *)watch_bits, watch_width, watch_height);

	maskPixmap = XCreateBitmapFromData(display, DefaultRootWindow(display),
		(char *)watch_mask_bits, watch_width, watch_height);

	xcolors[0].pixel = BlackPixelOfScreen(DefaultScreenOfDisplay(display));
	xcolors[1].pixel = WhitePixelOfScreen(DefaultScreenOfDisplay(display));

	XQueryColors(display, DefaultColormapOfScreen(
		DefaultScreenOfDisplay(display)), xcolors, 2);
	waitCursor = XCreatePixmapCursor(display, pixmap, maskPixmap,
		&xcolors[0], &xcolors[1], watch_x_hot, watch_y_hot);
	XFreePixmap(display, pixmap);
	XFreePixmap(display, maskPixmap);
    }

    /* display the cursor */
    XDefineCursor(display, XtWindow(topCursorWidget), waitCursor);
}

void BusyWait(Widget widget)
{
#ifdef __unix__
    static const int timeout = 100000;  /* 1/10 sec = 100 ms = 100,000 us */
    static struct timeval last = { 0, 0 };
    struct timeval current;
    gettimeofday(&current, NULL);

    if ((current.tv_sec != last.tv_sec) ||
        (current.tv_usec - last.tv_usec > timeout))
    {
        XmUpdateDisplay(widget);
        last = current;
    }    
#else
    static time_t last = 0;
    time_t current;
    time(&current);
    
    if (difftime(current, last) > 0)
    {
        XmUpdateDisplay(widget);
        last = current;
    }
#endif
}

void EndWait(Widget topCursorWidget)
{
    XUndefineCursor(XtDisplay(topCursorWidget), XtWindow(topCursorWidget));
}

/*
** Create an X window geometry string from width, height, x, and y values.
** This is a complement to the X routine XParseGeometry, and uses the same
** bitmask values (XValue, YValue, WidthValue, HeightValue, XNegative, and
** YNegative) as defined in <X11/Xutil.h> and documented under XParseGeometry.
** It expects a string of at least MAX_GEOMETRY_STRING_LEN in which to write
** result.  Note that in a geometry string, it is not possible to supply a y
** position without an x position.  Also note that the X/YNegative flags
** mean "add a '-' and negate the value" which is kind of odd.
*/
void CreateGeometryString(char *string, int x, int y,
	int width, int height, int bitmask)
{
    char *ptr = string;
    int nChars;
    
    if (bitmask & WidthValue) {
    	sprintf(ptr, "%d%n", width, &nChars);
	ptr += nChars;
    }
    if (bitmask & HeightValue) {
	sprintf(ptr, "x%d%n", height, &nChars);
	ptr += nChars;
    }
    if (bitmask & XValue) {
	if (bitmask & XNegative)
    	    sprintf(ptr, "-%d%n", -x, &nChars);
	else
    	    sprintf(ptr, "+%d%n", x, &nChars);
	ptr += nChars;
    }
    if (bitmask & YValue) {
	if (bitmask & YNegative)
    	    sprintf(ptr, "-%d%n", -y, &nChars);
	else
    	    sprintf(ptr, "+%d%n", y, &nChars);
	ptr += nChars;
    }
    *ptr = '\0';
}

/*
** Remove the white space (blanks and tabs) from a string
*/
static void removeWhiteSpace(char *string)
{
    char *outPtr = string;
    
    while (TRUE) {
    	if (*string == 0) {
	    *outPtr = 0;
	    return;
    	} else if (*string != ' ' && *string != '\t')
	    *(outPtr++) = *(string++);
	else
	    string++;
    }
}

/*
** Compares two strings and return TRUE if the two strings
** are the same, ignoring whitespace and case differences.
*/
static int stripCaseCmp(const char *str1, const char *str2)
{
    const char *c1, *c2;
    
    for (c1=str1, c2=str2; *c1!='\0' && *c2!='\0'; c1++, c2++) {
	while (*c1 == ' ' || *c1 == '\t')
	    c1++;
	while (*c2 == ' ' || *c2 == '\t')
	    c2++;
    	if (toupper((unsigned char)*c1) != toupper((unsigned char)*c2))
    	    return FALSE;
    }
    return *c1 == '\0' && *c2 == '\0';
}

static void warnHandlerCB(String message)
{
    if (strstr(message, "XtRemoveGrab"))
    	return;
    if (strstr(message, "Attempt to remove non-existant passive grab"))
    	return;
    fputs(message, stderr);
    fputc('\n', stderr);
}

static XModifierKeymap *getKeyboardMapping(Display *display) {
    static XModifierKeymap *keyboardMap = NULL;

    if (keyboardMap == NULL) {
        keyboardMap = XGetModifierMapping(display);
    }
    return(keyboardMap);
}

/*
** get mask for a modifier
**
*/

static Modifiers findModifierMapping(Display *display, KeyCode keyCode) {
    int i, j;
    KeyCode *mapentry;
    XModifierKeymap *modMap = getKeyboardMapping(display);

    if (modMap == NULL || keyCode == 0) {
        return(0);
    }

    mapentry = modMap->modifiermap;
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < (modMap->max_keypermod); ++j) {
            if (keyCode == *mapentry) {
                return(1 << ((mapentry - modMap->modifiermap) / modMap->max_keypermod));
            }
            ++mapentry;
        }
    }
    return(0);
}

Modifiers GetNumLockModMask(Display *display) {
    static int numLockMask = -1;

    if (numLockMask == -1) {
        numLockMask = findModifierMapping(display, XKeysymToKeycode(display, XK_Num_Lock));
    }
    return(numLockMask);
}

/*
** Grab a key regardless of caps-lock and other silly latching keys.
**
*/

static void reallyGrabAKey(Widget dialog, int keyCode, Modifiers mask) {
    Modifiers numLockMask = GetNumLockModMask(XtDisplay(dialog));

    if (keyCode == 0)  /* No anykey grabs, sorry */
        return;

    XtGrabKey(dialog, keyCode, mask, True, GrabModeAsync, GrabModeAsync);
    XtGrabKey(dialog, keyCode, mask|LockMask, True, GrabModeAsync, GrabModeAsync);
    if (numLockMask && numLockMask != LockMask) {
        XtGrabKey(dialog, keyCode, mask|numLockMask, True, GrabModeAsync, GrabModeAsync);
        XtGrabKey(dialog, keyCode, mask|LockMask|numLockMask, True, GrabModeAsync, GrabModeAsync);
    }
}

/*
** Part of dialog mnemonic processing.  Search the widget tree under w
** for widgets with mnemonics.  When found, add a passive grab to the
** dialog widget for the mnemonic character, thus directing mnemonic
** events to the dialog widget.
*/
static void addMnemonicGrabs(Widget dialog, Widget w, int unmodifiedToo)
{
    char mneString[2];
    WidgetList children;
    Cardinal numChildren;
    int i, isMenu;
    KeySym mnemonic = '\0';
    unsigned char rowColType;
    unsigned int keyCode;
    
    if (XtIsComposite(w)) {
	if (XtClass(w) == xmRowColumnWidgetClass) {
	    XtVaGetValues(w, XmNrowColumnType, &rowColType, NULL);
	    isMenu = rowColType != XmWORK_AREA;
	} else
	    isMenu = False;
	if (!isMenu) {
	    XtVaGetValues(w, XmNchildren, &children, XmNnumChildren,
		    &numChildren, NULL);
	    for (i=0; i<(int)numChildren; i++)
    		addMnemonicGrabs(dialog, children[i], unmodifiedToo);
    	}
    } else {
	XtVaGetValues(w, XmNmnemonic, &mnemonic, NULL);
	if (mnemonic != XK_VoidSymbol && mnemonic != '\0') {
	    mneString[0] = mnemonic; mneString[1] = '\0';
	    keyCode = XKeysymToKeycode(XtDisplay(dialog),
	    	    XStringToKeysym(mneString));
            reallyGrabAKey(dialog, keyCode, Mod1Mask);
            if (unmodifiedToo)
                reallyGrabAKey(dialog, keyCode, 0);
	}
    }
}

/*
** Callback routine for dialog mnemonic processing.
*/
static void mnemonicCB(Widget w, XtPointer callData, XKeyEvent *event)
{
    findAndActivateMnemonic(w, event->keycode);
}

/*
** Look for a widget in the widget tree w, with a mnemonic matching
** keycode.  When one is found, simulate a button press on that widget
** and give it the keyboard focus.  If the mnemonic is on a label,
** look in the userData field of the label to see if it points to
** another widget, and give that the focus.  This routine is just
** sufficient for NEdit, no doubt it will need to be extended for
** mnemonics on widgets other than just buttons and text fields.
*/
static void findAndActivateMnemonic(Widget w, unsigned int keycode)
{
    WidgetList children;
    Cardinal numChildren;
    int i, isMenu;
    KeySym mnemonic = '\0';
    char mneString[2];
    Widget userData;
    unsigned char rowColType;
    
    if (XtIsComposite(w)) {
	if (XtClass(w) == xmRowColumnWidgetClass) {
	    XtVaGetValues(w, XmNrowColumnType, &rowColType, NULL);
	    isMenu = rowColType != XmWORK_AREA;
	} else
	    isMenu = False;
	if (!isMenu) {
	    XtVaGetValues(w, XmNchildren, &children, XmNnumChildren,
		    &numChildren, NULL);
	    for (i=0; i<(int)numChildren; i++)
    		findAndActivateMnemonic(children[i], keycode);
    	}
    } else {
	XtVaGetValues(w, XmNmnemonic, &mnemonic, NULL);
	if (mnemonic != '\0') {
	    mneString[0] = mnemonic; mneString[1] = '\0';
	    if (XKeysymToKeycode(XtDisplay(XtParent(w)),
	    	    XStringToKeysym(mneString)) == keycode) {
	    	if (XtClass(w) == xmLabelWidgetClass ||
	    		XtClass(w) == xmLabelGadgetClass) {
	    	    XtVaGetValues(w, XmNuserData, &userData, NULL);
	    	    if (userData!=NULL && XtIsWidget(userData) && 
                        XmIsTraversable(userData))
	    	    	XmProcessTraversal(userData, XmTRAVERSE_CURRENT);
	    	} else if (XmIsTraversable(w)) {
	    	    XmProcessTraversal(w, XmTRAVERSE_CURRENT);
	    	    SimulateButtonPress(w);
	    	}
	    }
	}
    }
}

/*
** Part of workaround for Motif Caps/Num Lock bug.  Search the widget tree
** under w for widgets with accelerators.  When found, add three passive
** grabs to topWidget, one for the accelerator keysym + modifiers + Caps
** Lock, one for Num Lock, and one for both, thus directing lock +
** accelerator events to topWidget.
*/
static void addAccelGrabs(Widget topWidget, Widget w)
{
    WidgetList children;
    Widget menu;
    Cardinal numChildren;
    int i;
    
    if (XtIsComposite(w)) {
	XtVaGetValues(w, XmNchildren, &children, XmNnumChildren,
		&numChildren, NULL);
	for (i=0; i<(int)numChildren; i++)
    	    addAccelGrabs(topWidget, children[i]);
    } else if (XtClass(w) == xmCascadeButtonWidgetClass) {
	XtVaGetValues(w, XmNsubMenuId, &menu, NULL);
	if (menu != NULL)
	    addAccelGrabs(topWidget, menu);
    } else
	addAccelGrab(topWidget, w);
}

/*
** Grabs the key + modifier defined in the widget's accelerator resource,
** in combination with the Caps Lock and Num Lock accelerators.
*/
static void addAccelGrab(Widget topWidget, Widget w)
{
    char *accelString = NULL;
    KeySym keysym;
    unsigned int modifiers;
    KeyCode code;
    Modifiers numLockMask = GetNumLockModMask(XtDisplay(topWidget));
    
    XtVaGetValues(w, XmNaccelerator, &accelString, NULL);
    if (accelString == NULL || *accelString == '\0') {
        NEditFree(accelString);
        return;
    }

    if (!parseAccelString(XtDisplay(topWidget), accelString, &keysym, &modifiers)) {
        NEditFree(accelString);
	return;
    }
    NEditFree(accelString);

    /* Check to see if this server has this key mapped.  Some cruddy PC X
       servers (Xoftware) have terrible default keymaps. If not,
       XKeysymToKeycode will return 0.  However, it's bad news to pass
       that to XtGrabKey because 0 is really "AnyKey" which is definitely
       not what we want!! */
       
    code = XKeysymToKeycode(XtDisplay(topWidget), keysym);
    if (code == 0)
        return;
        
    XtGrabKey(topWidget, code,
	    modifiers | LockMask, True, GrabModeAsync, GrabModeAsync);
    if (numLockMask && numLockMask != LockMask) {
        XtGrabKey(topWidget, code,
	        modifiers | numLockMask, True, GrabModeAsync, GrabModeAsync);
        XtGrabKey(topWidget, code,
	        modifiers | LockMask | numLockMask, True, GrabModeAsync, GrabModeAsync);
    }
}

/*
** Read a Motif accelerator string and translate it into a keysym + modifiers.
** Returns TRUE if the parse was successful, FALSE, if not.
*/
static int parseAccelString(Display *display, const char *string, KeySym *keySym,
	unsigned int *modifiers)
{
#define N_MODIFIERS 12
    /*... Is NumLock always Mod3? */
    static char *modifierNames[N_MODIFIERS] = {"Ctrl", "Shift", "Alt", "Mod2",
	    "Mod3", "Mod4", "Mod5", "Button1", "Button2", "Button3", "Button4",
	    "Button5"};
    static unsigned int modifierMasks[N_MODIFIERS] = {ControlMask, ShiftMask,
	    Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask, Button1Mask, Button2Mask,
	    Button3Mask, Button4Mask, Button5Mask};
    Modifiers numLockMask = GetNumLockModMask(display);
    char modStr[MAX_ACCEL_LEN];
    char evtStr[MAX_ACCEL_LEN];
    char keyStr[MAX_ACCEL_LEN];
    const char *c, *evtStart, *keyStart;
    int i;
    
    if (strlen(string) >= MAX_ACCEL_LEN)
	return FALSE;
    
    /* Get the modifier part */
    for (c = string; *c != '<'; c++)
	if (*c == '\0')
	    return FALSE;
    strncpy(modStr, string, c - string);
    modStr[c - string] = '\0';
    
    /* Verify the <key> or <keypress> part */
    evtStart = c;
    for ( ; *c != '>'; c++)
	if (*c == '\0')
	    return FALSE;
    c++;
    strncpy(evtStr, evtStart, c - evtStart);
    evtStr[c - evtStart] = '\0';
    if (!stripCaseCmp(evtStr, "<key>") && !stripCaseCmp(evtStr, "<keypress>"))
	return FALSE;
    
    /* Get the keysym part */
    keyStart = c;
    for ( ; *c != '\0' && !(c != keyStart && *c == ':'); c++);
    strncpy(keyStr, keyStart, c - keyStart);
    keyStr[c - keyStart] = '\0';
    *keySym = XStringToKeysym(keyStr);
    
    /* Parse the modifier part */
    *modifiers = 0;
    c = modStr;
    while (*c != '\0') {
	while (*c == ' ' || *c == '\t')
	    c++;
	if (*c == '\0')
	    break;
	for (i = 0; i < N_MODIFIERS; i++) {
	    if (!strncmp(c, modifierNames[i], strlen(modifierNames[i]))) {
	    	c += strlen(modifierNames[i]);
                if (modifierMasks[i] != numLockMask) {
		    *modifiers |= modifierMasks[i];
                }
		break;
	    }
	}
	if (i == N_MODIFIERS)
	    return FALSE;
    }
    
    return TRUE;
}

/*
** Event handler for patching around Motif's lock + accelerator problem.
** Looks for a menu item in the patched menu hierarchy and invokes its
** ArmAndActivate action.
*/
static void lockCB(Widget w, XtPointer callData, XEvent *event,
	Boolean *continueDispatch)
{
    Modifiers numLockMask = GetNumLockModMask(XtDisplay(w));
    Widget topMenuWidget = (Widget)callData;
    *continueDispatch = TRUE;
    
    if (!(((XKeyEvent *)event)->state & (LockMask | numLockMask)))
	return;

    if (findAndActivateAccel(topMenuWidget, ((XKeyEvent*) event)->keycode,
            ((XKeyEvent*) event)->state & ~(LockMask | numLockMask), event)) {
        *continueDispatch = FALSE;
    }
}

/*
** Search through menu hierarchy under w and look for a button with
** accelerator matching keyCode + modifiers, and do its action
*/
static int findAndActivateAccel(Widget w, unsigned int keyCode,
	unsigned int modifiers, XEvent *event)
{
    WidgetList children;
    Widget menu;
    Cardinal numChildren;
    int i;
    char *accelString = NULL;
    KeySym keysym;
    unsigned int mods;
    
    if (XtIsComposite(w)) {
	XtVaGetValues(w, XmNchildren, &children, XmNnumChildren,
		&numChildren, NULL);
	for (i=0; i<(int)numChildren; i++)
    	    if (findAndActivateAccel(children[i], keyCode, modifiers, event))
		return TRUE;
    } else if (XtClass(w) == xmCascadeButtonWidgetClass) {
	XtVaGetValues(w, XmNsubMenuId, &menu, NULL);
	if (menu != NULL)
	    if (findAndActivateAccel(menu, keyCode, modifiers, event))
		return TRUE;
    } else {
	XtVaGetValues(w, XmNaccelerator, &accelString, NULL);
	if (accelString != NULL && *accelString != '\0') {
	    if (!parseAccelString(XtDisplay(w), accelString, &keysym, &mods))
		return FALSE;
	    if (keyCode == XKeysymToKeycode(XtDisplay(w), keysym) &&
		    modifiers == mods) {
		if (XtIsSensitive(w)) {
		    XtCallActionProc(w, "ArmAndActivate", event, NULL, 0);
		    return TRUE;
		}
	    }
	}
    }
    return FALSE;
}

/*
** Global installation of mouse wheel actions for scrolled windows.
*/
void InstallMouseWheelActions(XtAppContext context)
{   
    static XtActionsRec Actions[] = {
      	{"scrolled-window-scroll-up",   scrollUpAP},
      	{"scrolled-window-page-up",     pageUpAP},
      	{"scrolled-window-scroll-down", scrollDownAP},
      	{"scrolled-window-page-down",   pageDownAP} 
    };

    XtAppAddActions(context, Actions, XtNumber(Actions));
}

/*
** Add mouse wheel support to a specific widget, which must be the scrollable
** widget of a ScrolledWindow.
*/
void AddMouseWheelSupport(Widget w)
{
    if (XmIsScrolledWindow(XtParent(w))) 
    {
        static const char scrollTranslations[] =
           "Shift<Btn4Down>,<Btn4Up>: scrolled-window-scroll-up(1)\n"
           "Shift<Btn5Down>,<Btn5Up>: scrolled-window-scroll-down(1)\n"
           "Ctrl<Btn4Down>,<Btn4Up>:  scrolled-window-page-up()\n"
           "Ctrl<Btn5Down>,<Btn5Up>:  scrolled-window-page-down()\n"
           "<Btn4Down>,<Btn4Up>:      scrolled-window-scroll-up(3)\n"
           "<Btn5Down>,<Btn5Up>:      scrolled-window-scroll-down(3)\n";
        static XtTranslations trans_table = NULL;
        
        if (trans_table == NULL)
        {
            trans_table = XtParseTranslationTable(scrollTranslations);
        }
        XtOverrideTranslations(w, trans_table);
    }
}

static void pageUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    Widget scrolledWindow, scrollBar;
    String al[1];
    
    al[0] = "Up";
    scrolledWindow = XtParent(w);
    scrollBar = XtNameToWidget (scrolledWindow, "VertScrollBar");
    if (scrollBar)
        XtCallActionProc(scrollBar, "PageUpOrLeft", event, al, 1) ;
    return;
}

static void pageDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    Widget scrolledWindow, scrollBar;
    String al[1];
    
    al[0] = "Down";
    scrolledWindow = XtParent(w);
    scrollBar = XtNameToWidget (scrolledWindow, "VertScrollBar");
    if (scrollBar)
        XtCallActionProc(scrollBar, "PageDownOrRight", event, al, 1) ;
    return;
}

static void scrollUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    Widget scrolledWindow, scrollBar;
    String al[1];
    int i, nLines;
    
    if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
       return;
    al[0] = "Up";
    scrolledWindow = XtParent(w);
    scrollBar = XtNameToWidget (scrolledWindow, "VertScrollBar");
    if (scrollBar)
        for (i=0; i<nLines; i++)
            XtCallActionProc(scrollBar, "IncrementUpOrLeft", event, al, 1) ;
    return;
}

static void scrollDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    Widget scrolledWindow, scrollBar;
    String al[1];
    int i, nLines;
    
    if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
       return;
    al[0] = "Down";
    scrolledWindow = XtParent(w);
    scrollBar = XtNameToWidget (scrolledWindow, "VertScrollBar");
    if (scrollBar)
        for (i=0; i<nLines; i++)
            XtCallActionProc(scrollBar, "IncrementDownOrRight", event, al, 1) ;
    return;
}


/* 
** This is a disguisting hack to work around a bug in OpenMotif.
** OpenMotif's toggle button Select() action routine remembers the last radio
** button that was toggled (stored as global state) and refuses to take any
** action when that button is clicked again. It fails to detect that we may
** have changed the button state and that clicking that button could make
** sense. The result is that radio buttons may apparently get stuck, ie.
** it is not possible to directly select with the mouse the previously
** selected button without selection another radio button first.
** The workaround consist of faking a mouse click on the button that we 
** toggled by calling the Arm, Select, and Disarm action procedures.
** 
** A minor remaining issue is the fact that, if the workaround is used, 
** it is not possible to change the state without notifying potential
** XmNvalueChangedCallbacks. In practice, this doesn't seem to be a problem.
**
*/
void RadioButtonChangeState(Widget widget, Boolean state, Boolean notify)
{
    /* 
      The bug only exists in OpenMotif 2.1.x/2.2.[0-2]. Since it's quite hard 
      to detect OpenMotif reliably, we make a rough cut by excluding Lesstif
      and all Motif versions >= 2.1.x and < 2.2.3.
    */
#ifndef LESSTIF_VERSION
#if XmVersion == 2001 || (XmVersion == 2002 && XmUPDATE_LEVEL < 3)
    /* save the widget with current focus in case it moves */
    Widget focusW, shellW = widget;
    while (shellW && !XtIsShell(shellW)) {
        shellW = XtParent(shellW);
    }
    focusW = XtGetKeyboardFocusWidget(shellW);

    if (state && XtIsRealized(widget))
    {
        /* 
           Simulate a mouse button click.
           The event attributes that matter are the event type and the 
           coordinates. When the button is managed, the coordinates have to
           be inside the button. When the button is not managed, they have to
           be (0, 0) to make sure that the Select routine accepts the event.
        */
        XEvent ev;
        if (XtIsManaged(widget))
        {
            Position x, y;
            /* Calculate the coordinates in the same way as OM. */
            XtTranslateCoords(XtParent(widget), widget->core.x, widget->core.y,
                              &x, &y);
            ev.xbutton.x_root = x + widget->core.border_width;
            ev.xbutton.y_root = y + widget->core.border_width;
        }
        else
        {
            ev.xbutton.x_root = 0;
            ev.xbutton.y_root = 0;
        }
        /* Default button bindings:
              ~c<Btn1Down>: Arm()
              ~c<Btn1Up>: Select() Disarm() */
        ev.xany.type = ButtonPress;
        XtCallActionProc(widget, "Arm", &ev, NULL, 0);
        ev.xany.type = ButtonRelease;
        XtCallActionProc(widget, "Select", &ev, NULL, 0);
        XtCallActionProc(widget, "Disarm", &ev, NULL, 0);
    }
    /* restore focus to the originator */
    if (focusW) {
        XtSetKeyboardFocus(shellW, focusW);
    }
#endif /* XmVersion == 2001 || ... */
#endif /* LESSTIF_VERSION */
    
    /* This is sufficient on non-OM platforms */
    XmToggleButtonSetState(widget, state, notify);
}

/* Workaround for bug in OpenMotif 2.1 and 2.2.  If you have an active tear-off
** menu from a TopLevelShell that is a child of an ApplicationShell, and then 
** close the parent window, Motif crashes.  The problem doesn't
** happen if you close the tear-offs first, so, we programatically close them
** before destroying the shell widget. 
*/
void CloseAllPopupsFor(Widget shell)
{
#ifndef LESSTIF_VERSION
    /* Doesn't happen in LessTif.  The tear-off menus are popup-children of
     * of the TopLevelShell there, which just works.  Motif wants to make
     * them popup-children of the ApplicationShell, where it seems to get
     * into trouble. */

    Widget app = XtParent(shell);
    int i;

    for (i = 0; i < app->core.num_popups; i++) {
        Widget pop = app->core.popup_list[i];
        Widget shellFor;

        XtVaGetValues(pop, XtNtransientFor, &shellFor, NULL);
        if (shell == shellFor)
            _XmDismissTearOff(pop, NULL, NULL);
    }
#endif
}

static long queryDesktop(Display *display, Window window, Atom deskTopAtom)
{
    long deskTopNumber = 0;
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char *prop;

    if (XGetWindowProperty(display, window, deskTopAtom, 0, 1,
           False, AnyPropertyType, &actualType, &actualFormat, &nItems,
           &bytesAfter, &prop) != Success) {
        return -1; /* Property not found */
    }

    if (actualType == None) {
        return -1; /* Property does not exist */
    }

    if (actualFormat != 32 || nItems != 1) {
        XFree((char*)prop);
        return -1; /* Wrong format */
    }

    deskTopNumber = *(long*)prop;
    XFree((char*)prop);
    return deskTopNumber;
}

/*
** Returns the current desktop number, or -1 if no desktop information
** is available.
*/
long QueryCurrentDesktop(Display *display, Window rootWindow)
{
    static Atom currentDesktopAtom = (Atom)-1;

    if (currentDesktopAtom == (Atom)-1)
        currentDesktopAtom = XInternAtom(display, "_NET_CURRENT_DESKTOP", True);

    if (currentDesktopAtom != None)
        return queryDesktop(display, rootWindow, currentDesktopAtom);

    return -1; /* No desktop information */
}

/*
** Returns the number of the desktop the given shell window is currently on,
** or -1 if no desktop information is available.  Note that windows shown
** on all desktops (sometimes called sticky windows) should return 0xFFFFFFFF.
*/
long QueryDesktop(Display *display, Widget shell)
{
    static Atom wmDesktopAtom = (Atom)-1;

    if (wmDesktopAtom == (Atom)-1)
        wmDesktopAtom = XInternAtom(display, "_NET_WM_DESKTOP", True);

    if (wmDesktopAtom != None)
        return queryDesktop(display, XtWindow(shell), wmDesktopAtom);

    return -1;  /* No desktop information */
}


/*
** Clipboard wrapper functions that call the Motif clipboard functions 
** a number of times before giving up. The interfaces are similar to the
** native Motif functions.
*/

#define SPINCOUNT  10   /* Try at most 10 times */
#define USLEEPTIME 1000 /* 1 ms between retries */

/*
** Warning reporting
*/
static void warning(const char* mesg)
{
  fprintf(stderr, "NEdit warning:\n%s\n", mesg);
}

/*
** Sleep routine
*/
static void microsleep(long usecs)
{
  static struct timeval timeoutVal;
  timeoutVal.tv_sec = usecs/1000000;
  timeoutVal.tv_usec = usecs - timeoutVal.tv_sec*1000000;
  select(0, NULL, NULL, NULL, &timeoutVal);
}

/*
** XmClipboardStartCopy spinlock wrapper.
*/
int SpinClipboardStartCopy(Display *display, Window window,
        XmString clip_label, Time timestamp, Widget widget,
        XmCutPasteProc callback, long *item_id)
{
    int i, res;
    for (i=0; i<SPINCOUNT; ++i) {
        res = XmClipboardStartCopy(display, window, clip_label, timestamp,
                                   widget, callback, item_id);
        if (res == XmClipboardSuccess) {
            return res;
        }
        microsleep(USLEEPTIME);
    }
    warning("XmClipboardStartCopy() failed: clipboard locked.");
    return res;
}

/*
** XmClipboardCopy spinlock wrapper.
*/
int SpinClipboardCopy(Display *display, Window window, long item_id,
        char *format_name, XtPointer buffer, unsigned long length,
        long private_id, long *data_id)
{
    int i, res;
    for (i=0; i<SPINCOUNT; ++i) {
        res = XmClipboardCopy(display, window, item_id, format_name,
                              buffer, length, private_id, data_id);
        if (res == XmClipboardSuccess) {
            return res;
        }
        if (res == XmClipboardFail) {
            warning("XmClipboardCopy() failed: XmClipboardStartCopy not "
                    "called or too many formats.");
            return res;
        }
        microsleep(USLEEPTIME);
    }
    warning("XmClipboardCopy() failed: clipboard locked.");
    return res;
}

/*
** XmClipboardEndCopy spinlock wrapper.
*/
int SpinClipboardEndCopy(Display *display, Window window, long item_id)
{
    int i, res;
    for (i=0; i<SPINCOUNT; ++i) {
        res = XmClipboardEndCopy(display, window, item_id);
        if (res == XmClipboardSuccess) {
            return res;
        }
        if (res == XmClipboardFail) {
            warning("XmClipboardEndCopy() failed: XmClipboardStartCopy not "
                    "called.");
            return res;
        }
        microsleep(USLEEPTIME);
    }
    warning("XmClipboardEndCopy() failed: clipboard locked.");
    return res;
}

/*
** XmClipboardInquireLength spinlock wrapper.
*/
int SpinClipboardInquireLength(Display *display, Window window,
        char *format_name, unsigned long *length)
{
    int i, res;
    for (i=0; i<SPINCOUNT; ++i) {
        res = XmClipboardInquireLength(display, window, format_name, length);
        if (res == XmClipboardSuccess) {
            return res;
        }
        if (res == XmClipboardNoData) {
            return res;
        }
        microsleep(USLEEPTIME);
    }
    warning("XmClipboardInquireLength() failed: clipboard locked.");
    return res;
}

/*
** XmClipboardRetrieve spinlock wrapper.
*/
int SpinClipboardRetrieve(Display *display, Window window, char *format_name,
        XtPointer buffer, unsigned long length, unsigned long *num_bytes,
        long *private_id)
{
    int i, res;
    for (i=0; i<SPINCOUNT; ++i) {
        res = XmClipboardRetrieve(display, window, format_name, buffer,
                                  length, num_bytes, private_id);
        if (res == XmClipboardSuccess) {
            return res;
        }
        if (res == XmClipboardTruncate) {
            warning("XmClipboardRetrieve() failed: buffer too small.");
            return res;
        }
        if (res == XmClipboardNoData) {
            return res;
        }
        microsleep(USLEEPTIME);
    }
    warning("XmClipboardRetrieve() failed: clipboard locked.");
    return res;
}

/*
** XmClipboardLock spinlock wrapper.
*/
int SpinClipboardLock(Display *display, Window window)
{
    int i, res;
    for (i=0; i<SPINCOUNT; ++i) {
        res = XmClipboardLock(display, window);
        if (res == XmClipboardSuccess) {
            return res;
        }
        microsleep(USLEEPTIME);
    }
    warning("XmClipboardLock() failed: clipboard locked.");
    return res;
}

/*
** XmClipboardUnlock spinlock wrapper.
*/
int SpinClipboardUnlock(Display *display, Window window)
{
    int i, res;
    /* Spinning doesn't make much sense in this case, I think. */
    for (i=0; i<SPINCOUNT; ++i) {
        /* Remove ALL locks (we don't use nested locking in NEdit) */
        res = XmClipboardUnlock(display, window, True);
        if (res == XmClipboardSuccess) {
            return res;
        }
        microsleep(USLEEPTIME);
    }
    /*
     * This warning doesn't make much sense in practice. It's usually
     * triggered when we try to unlock the clipboard after a failed clipboard
     * operation, in an attempt to work around possible *tif clipboard locking
     * bugs. In these cases, failure _is_ the expected outcome and the warning
     * is bogus. Therefore, the warning is disabled.
    warning("XmClipboardUnlock() failed: clipboard not locked or locked "
            "by another application.");
     */
    return res;
}

/*
** Send a client message to a EWMH/NetWM compatible X Window Manager.
** Code taken from wmctrl-1.07 (GPL licensed)
*/
void WmClientMsg(Display *disp, Window win, const char *msg,
        unsigned long data0, unsigned long data1,
        unsigned long data2, unsigned long data3,
        unsigned long data4)
{
    XEvent event;
    long mask = SubstructureRedirectMask | SubstructureNotifyMask;

    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = XInternAtom(disp, msg, False);
    event.xclient.window = win;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = data1;
    event.xclient.data.l[2] = data2;
    event.xclient.data.l[3] = data3;
    event.xclient.data.l[4] = data4;

    if (!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
        fprintf(stderr, "nedit: cannot send %s EWMH event.\n", msg);
    }
}
