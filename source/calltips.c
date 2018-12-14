/*******************************************************************************
*									       *
* calltips.c -- Calltip UI functions  (calltip *file* functions are in tags.c) *
*									       *
* Copyright (C) 2002 Nathaniel Gray					       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
* 									       *
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
* April, 1997								       *
*									       *
* Written by Mark Edel  						       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "text.h"
#include "textP.h"
#include "calltips.h"
#include "../util/misc.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <Xm/Xm.h>
#include <Xm/Label.h>
#include <X11/Shell.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

static char *expandAllTabs( char *text, int tab_width );

/*
** Pop-down a calltip if one exists, else do nothing
*/
void KillCalltip(WindowInfo *window, int calltipID) {
    textDisp *textD = ((TextWidget)window->lastFocus)->text.textD;
    TextDKillCalltip( textD, calltipID );
}
    
void TextDKillCalltip(textDisp *textD, int calltipID) {
    if( textD->calltip.ID == 0 ) 
        return;
    if( calltipID == 0 || calltipID == textD->calltip.ID ) {
        XtPopdown( textD->calltipShell );
        textD->calltip.ID = 0;
    }
}

/*
** Is a calltip displayed?  Returns the calltip ID of the currently displayed 
** calltip, or 0 if there is no calltip displayed.  If called with 
** calltipID != 0, returns 0 unless there is a calltip being 
** displayed with that calltipID.
*/
int GetCalltipID(WindowInfo *window, int calltipID) {
    textDisp *textD = ((TextWidget)window->lastFocus)->text.textD;
    if( calltipID == 0 )
        return textD->calltip.ID;
    else {
        if( calltipID == textD->calltip.ID)
            return calltipID;
        else
            return 0;
    }
} 

#define CALLTIP_EDGE_GUARD 5
static Boolean offscreenV(XWindowAttributes *screenAttr, int top, int height) {
    return (top < CALLTIP_EDGE_GUARD || 
            top + height >= screenAttr->height - CALLTIP_EDGE_GUARD);
}

/*
** Update the position of the current calltip if one exists, else do nothing
*/
void TextDRedrawCalltip(textDisp *textD, int calltipID) {
    int lineHeight = textD->ascent + textD->descent;
    Position txtX, txtY, borderWidth, abs_x, abs_y, tipWidth, tipHeight;
    XWindowAttributes screenAttr;
    int rel_x, rel_y, flip_delta;
    
    if( textD->calltip.ID == 0 ) 
        return;
    if( calltipID != 0 && calltipID != textD->calltip.ID )
        return;
    
    /* Get the location/dimensions of the text area */
    XtVaGetValues(textD->w, XmNx, &txtX, XmNy, &txtY, NULL);
    
    if( textD->calltip.anchored ) {
        /* Put it at the anchor position */
        if (!TextDPositionToXY(textD, textD->calltip.pos, &rel_x, &rel_y)) {
            if (textD->calltip.alignMode == TIP_STRICT)
                TextDKillCalltip(textD, textD->calltip.ID);
            return;
        }
    } else {
        if (textD->calltip.pos < 0) {
            /* First display of tip with cursor offscreen (detected in 
                ShowCalltip) */
            textD->calltip.pos = textD->width/2;
            textD->calltip.hAlign = TIP_CENTER;
            rel_y = textD->height/3;
        } else if (!TextDPositionToXY(textD, textD->cursorPos, &rel_x, &rel_y)){
            /* Window has scrolled and tip is now offscreen */
            if (textD->calltip.alignMode == TIP_STRICT)
                TextDKillCalltip(textD, textD->calltip.ID);
            return;
        }
        rel_x = textD->calltip.pos;
    }

    XtVaGetValues(textD->calltipShell, XmNwidth, &tipWidth, XmNheight, 
            &tipHeight, XmNborderWidth, &borderWidth, NULL);
    rel_x += borderWidth;
    rel_y += lineHeight/2 + borderWidth;
    
    /* Adjust rel_x for horizontal alignment modes */
    if (textD->calltip.hAlign == TIP_CENTER)
        rel_x -= tipWidth/2;
    else if (textD->calltip.hAlign == TIP_RIGHT)
        rel_x -= tipWidth;
    
    /* Adjust rel_y for vertical alignment modes */
    if (textD->calltip.vAlign == TIP_ABOVE) {
        flip_delta = tipHeight + lineHeight + 2*borderWidth;
        rel_y -= flip_delta;
    } else
        flip_delta = -(tipHeight + lineHeight + 2*borderWidth);
    
    XtTranslateCoords(textD->w, rel_x, rel_y, &abs_x, &abs_y);
    
    /* If we're not in strict mode try to keep the tip on-screen */
    if (textD->calltip.alignMode == TIP_SLOPPY) {
        XGetWindowAttributes(XtDisplay(textD->w), 
                RootWindowOfScreen(XtScreen(textD->w)), &screenAttr);

        /* make sure tip doesn't run off right or left side of screen */
        if (abs_x + tipWidth >= screenAttr.width - CALLTIP_EDGE_GUARD)
    	    abs_x = screenAttr.width - tipWidth - CALLTIP_EDGE_GUARD;
        if (abs_x < CALLTIP_EDGE_GUARD)
            abs_x = CALLTIP_EDGE_GUARD;

        /* Try to keep the tip onscreen vertically if possible */
        if (screenAttr.height > tipHeight && 
                offscreenV(&screenAttr, abs_y, tipHeight)) {
            /* Maybe flipping from below to above (or vice-versa) will help */
            if (!offscreenV(&screenAttr, abs_y + flip_delta, tipHeight))
    	        abs_y += flip_delta;
            /* Make sure the tip doesn't end up *totally* offscreen */
            else if (abs_y + tipHeight < 0)
                abs_y = CALLTIP_EDGE_GUARD;
            else if (abs_y >= screenAttr.height)
                abs_y = screenAttr.height - tipHeight - CALLTIP_EDGE_GUARD;
            /* If no case applied, just go with the default placement. */
        }
    }
    
    XtVaSetValues( textD->calltipShell, XmNx, abs_x, XmNy, abs_y, NULL );
}

/* 
** Returns a new string with each \t replaced with tab_width spaces or
** a pointer to text if there were no tabs.  Returns NULL on malloc failure.
** Note that this is dumb replacement, not smart tab-like behavior!  The goal
** is to prevent tabs from turning into squares in calltips, not to get the
** formatting just right.
*/
static char *expandAllTabs( char *text, int tab_width ) {
    int i, nTabs=0;
    size_t len;
    char *c, *cCpy, *textCpy;
    
    /* First count 'em */
    for( c = text; *c; ++c )
        if( *c == '\t' )
            ++nTabs;
    if( nTabs == 0 )
        return text;

    /* Allocate the new string */
    len = strlen( text ) + ( tab_width - 1 )*nTabs;
    textCpy = (char*)NEditMalloc( len + 1 );
    if( !textCpy ) {
        fprintf(stderr, 
                "nedit: Out of heap memory in expandAllTabs!\n");
        return NULL;
    }
    
    /* Now replace 'em */
    for( c = text, cCpy = textCpy;  *c;  ++c, ++cCpy) {
        if( *c == '\t' ) {
            for( i = 0; i < tab_width; ++i, ++cCpy )
                *cCpy = ' ';
            --cCpy;  /* Will be incremented in outer for loop */
        } else
            *cCpy = *c;
    }
    *cCpy = '\0';
    return textCpy;
}

/*
** Pop-up a calltip.  
** If a calltip is already being displayed it is destroyed and replaced with
** the new calltip.  Returns the ID of the calltip or 0 on failure.
*/
int ShowCalltip(WindowInfo *window, char *text, Boolean anchored, 
        int pos, int hAlign, int vAlign, int alignMode) {
    static int StaticCalltipID = 1;
    textDisp *textD = ((TextWidget)window->lastFocus)->text.textD;
    int rel_x, rel_y;
    Position txtX, txtY;
    char *textCpy;
    XmString str;
    
    /* Destroy any previous calltip */
    TextDKillCalltip( textD, 0 );

    /* Make sure the text isn't NULL */
    if (text == NULL) return 0;
    
    /* Expand any tabs in the calltip and make it an XmString */
    textCpy = expandAllTabs( text, BufGetTabDistance(textD->buffer) );
    if( textCpy == NULL )
        return 0;       /* Out of memory */
    str = XmStringCreateLtoR(textCpy, XmFONTLIST_DEFAULT_TAG);
    if( textCpy != text )
        NEditFree( textCpy );
    
    /* Get the location/dimensions of the text area */
    XtVaGetValues(textD->w,
        XmNx, &txtX,
        XmNy, &txtY,
        NULL);
    
    /* Create the calltip widget on first request */
    if (textD->calltipW == NULL) {
        Arg args[10];
        int argcnt = 0;
        XtSetArg(args[argcnt], XmNsaveUnder, True); argcnt++;
        XtSetArg(args[argcnt], XmNallowShellResize, True); argcnt++;
        
        textD->calltipShell = CreatePopupShellWithBestVis("calltipshell", 
             overrideShellWidgetClass, textD->w, args, argcnt);
                
        /* Might want to make this a read-only XmText eventually so that 
            users can copy from it */
        textD->calltipW = XtVaCreateManagedWidget( 
                "calltip", xmLabelWidgetClass, textD->calltipShell,
                XmNborderWidth, 1,              /* Thin borders */
                XmNhighlightThickness, 0,
                XmNalignment, XmALIGNMENT_BEGINNING,
                XmNforeground, textD->calltipFGPixel,
                XmNbackground, textD->calltipBGPixel,
                NULL );
    }
    
    /* Set the text on the label */
    XtVaSetValues( textD->calltipW, XmNlabelString, str, NULL );
    XmStringFree( str );
    
    /* Figure out where to put the tip */
    if (anchored) {
        /* Put it at the specified position */
        /* If position is not displayed, return 0 */
        if (pos < textD->firstChar || pos > textD->lastChar ) {
            XBell(TheDisplay, 0);
            return 0;
        }
        textD->calltip.pos = pos;
    } else {
        /* Put it next to the cursor, or in the center of the window if the
            cursor is offscreen and mode != strict */
        if (!TextDPositionToXY(textD, textD->cursorPos, &rel_x, &rel_y)) {
            if (alignMode == TIP_STRICT) {
                XBell(TheDisplay, 0);
                return 0;
            }
            textD->calltip.pos = -1;
        } else 
            /* Store the x-offset for use when redrawing */
            textD->calltip.pos = rel_x;
    }

    /* Should really bounds-check these enumerations... */
    textD->calltip.ID = StaticCalltipID;
    textD->calltip.anchored = anchored;
    textD->calltip.hAlign = hAlign;
    textD->calltip.vAlign = vAlign;
    textD->calltip.alignMode = alignMode;
    
    /* Increment the static calltip ID.  Macro variables can only be int, 
        not unsigned, so have to work to keep it > 0 on overflow */
    if(++StaticCalltipID <= 0)
        StaticCalltipID = 1;
    
    /* Realize the calltip's shell so that its width & height are known */
    XtRealizeWidget( textD->calltipShell );
    /* Move the calltip and pop it up */
    TextDRedrawCalltip(textD, 0);
    XtPopup( textD->calltipShell, XtGrabNone );
    return textD->calltip.ID;
}
