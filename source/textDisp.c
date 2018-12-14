/*******************************************************************************
*									       *
* textDisp.c - Display text from a text buffer				       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
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
* June 15, 1995								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "textDisp.h"
#include "textBuf.h"
#include "text.h"
#include "textP.h"
#include "nedit.h"
#include "calltips.h"
#include "highlight.h"
#include "rangeset.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#include <Xm/Xm.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/Label.h>
#include <X11/Shell.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* Masks for text drawing methods.  These are or'd together to form an
   integer which describes what drawing calls to use to draw a string */
#define FILL_SHIFT 8
#define SECONDARY_SHIFT 9
#define PRIMARY_SHIFT 10
#define HIGHLIGHT_SHIFT 11
#define STYLE_LOOKUP_SHIFT 0
#define BACKLIGHT_SHIFT 12

#define FILL_MASK (1 << FILL_SHIFT)
#define SECONDARY_MASK (1 << SECONDARY_SHIFT)
#define PRIMARY_MASK (1 << PRIMARY_SHIFT)
#define HIGHLIGHT_MASK (1 << HIGHLIGHT_SHIFT)
#define STYLE_LOOKUP_MASK (0xff << STYLE_LOOKUP_SHIFT)
#define BACKLIGHT_MASK  (0xff << BACKLIGHT_SHIFT)

#define RANGESET_SHIFT (20)
#define RANGESET_MASK (0x3F << RANGESET_SHIFT)

/* If you use both 32-Bit Style mask layout:
   Bits +----------------+----------------+----------------+----------------+
    hex |1F1E1D1C1B1A1918|1716151413121110| F E D C B A 9 8| 7 6 5 4 3 2 1 0|
    dec |3130292827262524|2322212019181716|151413121110 9 8| 7 6 5 4 3 2 1 0|
        +----------------+----------------+----------------+----------------+
   Type |             r r| r r r r b b b b| b b b b H 1 2 F| s s s s s s s s|
        +----------------+----------------+----------------+----------------+
   where: s - style lookup value (8 bits)
        F - fill (1 bit)
        2 - secondary selection  (1 bit)
        1 - primary selection (1 bit)
        H - highlight (1 bit)
        b - backlighting index (8 bits)
        r - rangeset index (6 bits)
   This leaves 6 "unused" bits */

/* Maximum displayable line length (how many characters will fit across the
   widest window).  This amount of memory is temporarily allocated from the
   stack in the redisplayLine routine for drawing strings */
#define MAX_DISP_LINE_LEN 1000

/* Macro for getting the TextPart from a textD */
#define TEXT_OF_TEXTD(t)    (((TextWidget)((t)->w))->text)

enum positionTypes {CURSOR_POS, CHARACTER_POS};

static void updateLineStarts(textDisp *textD, int pos, int charsInserted,
        int charsDeleted, int linesInserted, int linesDeleted, int *scrolled);
static void offsetLineStarts(textDisp *textD, int newTopLineNum);
static void calcLineStarts(textDisp *textD, int startLine, int endLine);
static void calcLastChar(textDisp *textD);
static int posToVisibleLineNum(textDisp *textD, int pos, int *lineNum);
static void redisplayLine(textDisp *textD, int visLineNum, int leftClip,
        int rightClip, int leftCharIndex, int rightCharIndex);
static void drawString(textDisp *textD, int style, int x, int y, int toX,
        char *string, int nChars);
static void clearRect(textDisp *textD, GC gc, int x, int y, 
        int width, int height);
static void drawCursor(textDisp *textD, int x, int y);
static int styleOfPos(textDisp *textD, int lineStartPos,
        int lineLen, int lineIndex, int dispIndex, int thisChar);
static int stringWidth(const textDisp* textD, const char* string,
        int length, int style);
static int inSelection(selection *sel, int pos, int lineStartPos,
        int dispIndex);
static int xyToPos(textDisp *textD, int x, int y, int posType);
static void xyToUnconstrainedPos(textDisp *textD, int x, int y, int *row,
        int *column, int posType);
static void bufPreDeleteCB(int pos, int nDeleted, void *cbArg);
static void bufModifiedCB(int pos, int nInserted, int nDeleted,
        int nRestyled, const char *deletedText, void *cbArg);
static void setScroll(textDisp *textD, int topLineNum, int horizOffset,
        int updateVScrollBar, int updateHScrollBar);
static void hScrollCB(Widget w, XtPointer clientData, XtPointer callData);
static void vScrollCB(Widget w, XtPointer clientData, XtPointer callData);
static void visibilityEH(Widget w, XtPointer data, XEvent *event,
        Boolean *continueDispatch);
static void redrawLineNumbers(textDisp *textD, int clearAll);
static void updateVScrollBarRange(textDisp *textD);
static int updateHScrollBarRange(textDisp *textD);
static int max(int i1, int i2);
static int min(int i1, int i2);
static int countLines(const char *string);
static int measureVisLine(textDisp *textD, int visLineNum);
static int emptyLinesVisible(textDisp *textD);
static void blankCursorProtrusions(textDisp *textD);
static void allocateFixedFontGCs(textDisp *textD, XFontStruct *fontStruct,
        Pixel bgPixel, Pixel fgPixel, Pixel selectFGPixel, Pixel selectBGPixel,
        Pixel highlightFGPixel, Pixel highlightBGPixel, Pixel lineNumFGPixel);
static GC allocateGC(Widget w, unsigned long valueMask,
        unsigned long foreground, unsigned long background, Font font,
        unsigned long dynamicMask, unsigned long dontCareMask);
static void releaseGC(Widget w, GC gc);
static void resetClipRectangles(textDisp *textD);
static int visLineLength(textDisp *textD, int visLineNum);
static void measureDeletedLines(textDisp *textD, int pos, int nDeleted);
static void findWrapRange(textDisp *textD, const char *deletedText, int pos,
        int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd,
        int *linesInserted, int *linesDeleted);
static void wrappedLineCounter(const textDisp* textD, const textBuffer* buf,
        int startPos, int maxPos, int maxLines,
        Boolean startPosIsLineStart, int styleBufOffset,
        int* retPos, int* retLines, int* retLineStart, int* retLineEnd);
static void findLineEnd(textDisp *textD, int startPos, int startPosIsLineStart,
        int *lineEnd, int *nextLineStart);
static int wrapUsesCharacter(textDisp *textD, int lineEndPos);
static void hideOrShowHScrollBar(textDisp *textD);
static int rangeTouchesRectSel(selection *sel, int rangeStart, int rangeEnd);
static void extendRangeForStyleMods(textDisp *textD, int *start, int *end);
static int getAbsTopLineNum(textDisp *textD);
static void offsetAbsLineNum(textDisp *textD, int oldFirstChar);
static int maintainingAbsTopLineNum(textDisp *textD);
static void resetAbsLineNum(textDisp *textD);
static int measurePropChar(const textDisp* textD, char c,
        int colNum, int pos);
static Pixel allocBGColor(Widget w, char *colorName, int *ok);
static Pixel getRangesetColor(textDisp *textD, int ind, Pixel bground);
static void textDRedisplayRange(textDisp *textD, int start, int end);

textDisp *TextDCreate(Widget widget, Widget hScrollBar, Widget vScrollBar,
        Position left, Position top, Position width, Position height,
        Position lineNumLeft, Position lineNumWidth, textBuffer *buffer,
        XFontStruct *fontStruct, Pixel bgPixel, Pixel fgPixel,
        Pixel selectFGPixel, Pixel selectBGPixel, Pixel highlightFGPixel,
        Pixel highlightBGPixel, Pixel cursorFGPixel, Pixel lineNumFGPixel,
        int continuousWrap, int wrapMargin, XmString bgClassString,
        Pixel calltipFGPixel, Pixel calltipBGPixel)
{
    textDisp *textD;
    XGCValues gcValues;
    int i;
    
    textD = (textDisp *)NEditMalloc(sizeof(textDisp));
    textD->w = widget;
    textD->top = top;
    textD->left = left;
    textD->width = width;
    textD->height = height;
    textD->cursorOn = True;
    textD->cursorPos = 0;
    textD->cursorX = -100;
    textD->cursorY = -100;
    textD->cursorToHint = NO_HINT;
    textD->cursorStyle = NORMAL_CURSOR;
    textD->cursorPreferredCol = -1;
    textD->buffer = buffer;
    textD->firstChar = 0;
    textD->lastChar = 0;
    textD->nBufferLines = 0;
    textD->topLineNum = 1;
    textD->absTopLineNum = 1;
    textD->needAbsTopLineNum = False;
    textD->horizOffset = 0;
    textD->visibility = VisibilityUnobscured;
    textD->hScrollBar = hScrollBar;
    textD->vScrollBar = vScrollBar;
    textD->fontStruct = fontStruct;
    textD->ascent = fontStruct->ascent;
    textD->descent = fontStruct->descent;
    textD->fixedFontWidth = fontStruct->min_bounds.width ==
    	    fontStruct->max_bounds.width ? fontStruct->min_bounds.width : -1;
    textD->styleBuffer = NULL;
    textD->styleTable = NULL;
    textD->nStyles = 0;
    textD->bgPixel = bgPixel;
    textD->fgPixel = fgPixel;
    textD->selectFGPixel = selectFGPixel;
    textD->highlightFGPixel = highlightFGPixel;
    textD->selectBGPixel = selectBGPixel;
    textD->highlightBGPixel = highlightBGPixel;
    textD->lineNumFGPixel = lineNumFGPixel;
    textD->cursorFGPixel = cursorFGPixel;
    textD->wrapMargin = wrapMargin;
    textD->continuousWrap = continuousWrap;
    allocateFixedFontGCs(textD, fontStruct, bgPixel, fgPixel, selectFGPixel,
            selectBGPixel, highlightFGPixel, highlightBGPixel, lineNumFGPixel);
    textD->styleGC = allocateGC(textD->w, 0, 0, 0, fontStruct->fid,
            GCClipMask|GCForeground|GCBackground, GCArcMode);
    textD->lineNumLeft = lineNumLeft;
    textD->lineNumWidth = lineNumWidth;
    textD->nVisibleLines = (height - 1) / (textD->ascent + textD->descent) + 1;
    gcValues.foreground = cursorFGPixel;
    textD->cursorFGGC = XtGetGC(widget, GCForeground, &gcValues);
    textD->lineStarts = (int *)NEditMalloc(sizeof(int) * textD->nVisibleLines);
    textD->lineStarts[0] = 0;
    textD->calltipW = NULL;
    textD->calltipShell = NULL;
    textD->calltip.ID = 0;
    textD->calltipFGPixel = calltipFGPixel;
    textD->calltipBGPixel = calltipBGPixel;
    for (i=1; i<textD->nVisibleLines; i++)
    	textD->lineStarts[i] = -1;
    textD->bgClassPixel = NULL;
    textD->bgClass = NULL;
    TextDSetupBGClasses(widget, bgClassString, &textD->bgClassPixel,
          &textD->bgClass, bgPixel);
    textD->suppressResync = 0;
    textD->nLinesDeleted = 0;
    textD->modifyingTabDist = 0;
    textD->pointerHidden = False;
    textD->graphicsExposeQueue = NULL;

    /* Attach an event handler to the widget so we can know the visibility
       (used for choosing the fastest drawing method) */
    XtAddEventHandler(widget, VisibilityChangeMask, False,
        visibilityEH, textD);    

    /* Attach the callback to the text buffer for receiving modification
       information */
    if (buffer != NULL) {
	BufAddModifyCB(buffer, bufModifiedCB, textD);
	BufAddPreDeleteCB(buffer, bufPreDeleteCB, textD);
    }
    
    /* Initialize the scroll bars and attach movement callbacks */
    if (vScrollBar != NULL) {
	XtVaSetValues(vScrollBar, XmNminimum, 1, XmNmaximum, 2,
    		XmNsliderSize, 1, XmNrepeatDelay, 10, XmNvalue, 1, NULL);
	XtAddCallback(vScrollBar, XmNdragCallback, vScrollCB, (XtPointer)textD);
	XtAddCallback(vScrollBar, XmNvalueChangedCallback, vScrollCB, 
		(XtPointer)textD);
    }
    if (hScrollBar != NULL) {
	XtVaSetValues(hScrollBar, XmNminimum, 0, XmNmaximum, 1,
    		XmNsliderSize, 1, XmNrepeatDelay, 10, XmNvalue, 0,
    		XmNincrement, fontStruct->max_bounds.width, NULL);
	XtAddCallback(hScrollBar, XmNdragCallback, hScrollCB, (XtPointer)textD);
	XtAddCallback(hScrollBar, XmNvalueChangedCallback, hScrollCB,
		(XtPointer)textD);
    }

    /* Update the display to reflect the contents of the buffer */
    if (buffer != NULL)
    	bufModifiedCB(0, buffer->length, 0, 0, NULL, textD);

    /* Decide if the horizontal scroll bar needs to be visible */
    hideOrShowHScrollBar(textD);

    return textD;
}

/*
** Free a text display and release its associated memory.  Note, the text
** BUFFER that the text display displays is a separate entity and is not
** freed, nor are the style buffer or style table.
*/
void TextDFree(textDisp *textD)
{
    BufRemoveModifyCB(textD->buffer, bufModifiedCB, textD);
    BufRemovePreDeleteCB(textD->buffer, bufPreDeleteCB, textD);
    releaseGC(textD->w, textD->gc);
    releaseGC(textD->w, textD->selectGC);
    releaseGC(textD->w, textD->highlightGC);
    releaseGC(textD->w, textD->selectBGGC);
    releaseGC(textD->w, textD->highlightBGGC);
    releaseGC(textD->w, textD->styleGC);
    releaseGC(textD->w, textD->lineNumGC);
    NEditFree(textD->lineStarts);
    while (TextDPopGraphicExposeQueueEntry(textD)) {
    }
    NEditFree(textD->bgClassPixel);
    NEditFree(textD->bgClass);
    NEditFree(textD);
}

/*
** Attach a text buffer to display, replacing the current buffer (if any)
*/
void TextDSetBuffer(textDisp *textD, textBuffer *buffer)
{
    /* If the text display is already displaying a buffer, clear it off
       of the display and remove our callback from it */
    if (textD->buffer != NULL) {
    	bufModifiedCB(0, 0, textD->buffer->length, 0, NULL, textD);
    	BufRemoveModifyCB(textD->buffer, bufModifiedCB, textD);
    	BufRemovePreDeleteCB(textD->buffer, bufPreDeleteCB, textD);
    }
    
    /* Add the buffer to the display, and attach a callback to the buffer for
       receiving modification information when the buffer contents change */
    textD->buffer = buffer;
    BufAddModifyCB(buffer, bufModifiedCB, textD);
    BufAddPreDeleteCB(buffer, bufPreDeleteCB, textD);
    
    /* Update the display */
    bufModifiedCB(0, buffer->length, 0, 0, NULL, textD);
}

/*
** Attach (or remove) highlight information in text display and redisplay.
** Highlighting information consists of a style buffer which parallels the
** normal text buffer, but codes font and color information for the display;
** a style table which translates style buffer codes (indexed by buffer
** character - 65 (ASCII code for 'A')) into fonts and colors; and a callback 
** mechanism for as-needed highlighting, triggered by a style buffer entry of
** "unfinishedStyle".  Style buffer can trigger additional redisplay during
** a normal buffer modification if the buffer contains a primary selection
** (see extendRangeForStyleMods for more information on this protocol).
**
** Style buffers, tables and their associated memory are managed by the caller.
*/
void TextDAttachHighlightData(textDisp *textD, textBuffer *styleBuffer,
    	styleTableEntry *styleTable, int nStyles, char unfinishedStyle,
    	unfinishedStyleCBProc unfinishedHighlightCB, void *cbArg)
{
    textD->styleBuffer = styleBuffer;
    textD->styleTable = styleTable;
    textD->nStyles = nStyles;
    textD->unfinishedStyle = unfinishedStyle;
    textD->unfinishedHighlightCB = unfinishedHighlightCB;
    textD->highlightCBArg = cbArg;
    
    /* Call TextDSetFont to combine font information from style table and
       primary font, adjust font-related parameters, and then redisplay */
    TextDSetFont(textD, textD->fontStruct);
}


/* Change the (non syntax-highlit) colors */ 
void TextDSetColors(textDisp *textD, Pixel textFgP, Pixel textBgP, 
        Pixel selectFgP, Pixel selectBgP, Pixel hiliteFgP, Pixel hiliteBgP, 
        Pixel lineNoFgP, Pixel cursorFgP)
{
    XGCValues values;
    Display *d = XtDisplay(textD->w);
    
    /* Update the stored pixels */
    textD->fgPixel = textFgP;
    textD->bgPixel = textBgP;
    textD->selectFGPixel = selectFgP;
    textD->selectBGPixel = selectBgP;
    textD->highlightFGPixel = hiliteFgP;
    textD->highlightBGPixel = hiliteBgP;
    textD->lineNumFGPixel = lineNoFgP;
    textD->cursorFGPixel = cursorFgP;

    releaseGC(textD->w, textD->gc);
    releaseGC(textD->w, textD->selectGC);
    releaseGC(textD->w, textD->selectBGGC);
    releaseGC(textD->w, textD->highlightGC);
    releaseGC(textD->w, textD->highlightBGGC);
    releaseGC(textD->w, textD->lineNumGC);
    allocateFixedFontGCs(textD, textD->fontStruct, textBgP, textFgP, selectFgP,
            selectBgP, hiliteFgP, hiliteBgP, lineNoFgP);
    
    /* Change the cursor GC (the cursor GC is not shared). */
    values.foreground = cursorFgP;
    XChangeGC( d, textD->cursorFGGC, GCForeground, &values );
    
    /* Redisplay */
    TextDRedisplayRect(textD, textD->left, textD->top, textD->width,
                       textD->height);
    redrawLineNumbers(textD, True);
}

/*
** Change the (non highlight) font
*/
void TextDSetFont(textDisp *textD, XFontStruct *fontStruct)
{
    Display *display = XtDisplay(textD->w);
    int i, maxAscent = fontStruct->ascent, maxDescent = fontStruct->descent;
    int width, height, fontWidth;
    Pixel bgPixel, fgPixel, selectFGPixel, selectBGPixel;
    Pixel highlightFGPixel, highlightBGPixel, lineNumFGPixel;
    XGCValues values;
    XFontStruct *styleFont;
    
    /* If font size changes, cursor will be redrawn in a new position */
    blankCursorProtrusions(textD);
    
    /* If there is a (syntax highlighting) style table in use, find the new
       maximum font height for this text display */
    for (i=0; i<textD->nStyles; i++) {
        styleFont = textD->styleTable[i].font;
        if (styleFont != NULL && styleFont->ascent > maxAscent)
            maxAscent = styleFont->ascent;
        if (styleFont != NULL && styleFont->descent > maxDescent)
            maxDescent = styleFont->descent;
    }
    textD->ascent = maxAscent;
    textD->descent = maxDescent;
    
    /* If all of the current fonts are fixed and match in width, compute */
    fontWidth = fontStruct->max_bounds.width;
    if (fontWidth != fontStruct->min_bounds.width)
        fontWidth = -1;
    else {
        for (i=0; i<textD->nStyles; i++) {
            styleFont = textD->styleTable[i].font;
            if (styleFont != NULL && 
                    (styleFont->max_bounds.width != fontWidth ||
                    styleFont->max_bounds.width != styleFont->min_bounds.width))
                fontWidth = -1;
        }
    }
    textD->fixedFontWidth = fontWidth;
    
    /* Don't let the height dip below one line, or bad things can happen */
    if (textD->height < maxAscent + maxDescent)
        textD->height = maxAscent + maxDescent;

    /* Change the font.  In most cases, this means re-allocating the
       affected GCs (they are shared with other widgets, and if the primary
       font changes, must be re-allocated to change it). Unfortunately,
       this requres recovering all of the colors from the existing GCs */
    textD->fontStruct = fontStruct;
    XGetGCValues(display, textD->gc, GCForeground|GCBackground, &values);
    fgPixel = values.foreground;
    bgPixel = values.background;
    XGetGCValues(display, textD->selectGC, GCForeground|GCBackground, &values);
    selectFGPixel = values.foreground;
    selectBGPixel = values.background;
    XGetGCValues(display, textD->highlightGC,GCForeground|GCBackground,&values);
    highlightFGPixel = values.foreground;
    highlightBGPixel = values.background;
    XGetGCValues(display, textD->lineNumGC, GCForeground, &values);
    lineNumFGPixel = values.foreground;
    releaseGC(textD->w, textD->gc);
    releaseGC(textD->w, textD->selectGC);
    releaseGC(textD->w, textD->highlightGC);
    releaseGC(textD->w, textD->selectBGGC);
    releaseGC(textD->w, textD->highlightBGGC);
    releaseGC(textD->w, textD->lineNumGC);
    allocateFixedFontGCs(textD, fontStruct, bgPixel, fgPixel, selectFGPixel,
            selectBGPixel, highlightFGPixel, highlightBGPixel, lineNumFGPixel);
    XSetFont(display, textD->styleGC, fontStruct->fid);
    
    /* Do a full resize to force recalculation of font related parameters */
    width = textD->width;
    height = textD->height;
    textD->width = textD->height = 0;
    TextDResize(textD, width, height);
    
    /* if the shell window doesn't get resized, and the new fonts are
       of smaller sizes, sometime we get some residual text on the
       blank space at the bottom part of text area. Clear it here. */
    clearRect(textD, textD->gc, textD->left, 
	    textD->top + textD->height - maxAscent - maxDescent, 
	    textD->width, maxAscent + maxDescent);

    /* Redisplay */
    TextDRedisplayRect(textD, textD->left, textD->top, textD->width,
            textD->height);
    
    /* Clean up line number area in case spacing has changed */
    redrawLineNumbers(textD, True);
}

int TextDMinFontWidth(textDisp *textD, Boolean considerStyles)
{
    int fontWidth = textD->fontStruct->max_bounds.width;
    int i;

    if (considerStyles) {
        for (i = 0; i < textD->nStyles; ++i) {
            int thisWidth = (textD->styleTable[i].font)->min_bounds.width;
            if (thisWidth < fontWidth) {
                fontWidth = thisWidth;
            }
        }
    }
    return(fontWidth);
}

int TextDMaxFontWidth(textDisp *textD, Boolean considerStyles)
{
    int fontWidth = textD->fontStruct->max_bounds.width;
    int i;

    if (considerStyles) {
        for (i = 0; i < textD->nStyles; ++i) {
            int thisWidth = (textD->styleTable[i].font)->max_bounds.width;
            if (thisWidth > fontWidth) {
                fontWidth = thisWidth;
            }
        }
    }
    return(fontWidth);
}

/*
** Change the size of the displayed text area
*/
void TextDResize(textDisp *textD, int width, int height)
{
    int oldVisibleLines = textD->nVisibleLines;
    int canRedraw = XtWindow(textD->w) != 0;
    int newVisibleLines = height / (textD->ascent + textD->descent);
    int redrawAll = False;
    int oldWidth = textD->width;
    int exactHeight = height - height % (textD->ascent + textD->descent);
    
    textD->width = width;
    textD->height = height;
    
    /* In continuous wrap mode, a change in width affects the total number of
       lines in the buffer, and can leave the top line number incorrect, and
       the top character no longer pointing at a valid line start */
    if (textD->continuousWrap && textD->wrapMargin==0 && width!=oldWidth) {
        int oldFirstChar = textD->firstChar;
        textD->nBufferLines = TextDCountLines(textD, 0, textD->buffer->length,
                True);
        textD->firstChar = TextDStartOfLine(textD, textD->firstChar);
        textD->topLineNum = TextDCountLines(textD, 0, textD->firstChar, True)+1;
        redrawAll = True;
        offsetAbsLineNum(textD, oldFirstChar);
    }
 
    /* reallocate and update the line starts array, which may have changed
       size and/or contents. (contents can change in continuous wrap mode
       when the width changes, even without a change in height) */
    if (oldVisibleLines < newVisibleLines) {
        NEditFree(textD->lineStarts);
        textD->lineStarts = (int *)NEditMalloc(sizeof(int) * newVisibleLines);
    }
    textD->nVisibleLines = newVisibleLines;
    calcLineStarts(textD, 0, newVisibleLines);
    calcLastChar(textD);
    
    /* if the window became shorter, there may be partially drawn
       text left at the bottom edge, which must be cleaned up */
    if (canRedraw && oldVisibleLines>newVisibleLines && exactHeight!=height)
        XClearArea(XtDisplay(textD->w), XtWindow(textD->w), textD->left,
                textD->top + exactHeight,  textD->width,
                height - exactHeight, False);
    
    /* if the window became taller, there may be an opportunity to display
       more text by scrolling down */
    if (canRedraw && oldVisibleLines < newVisibleLines && textD->topLineNum +
            textD->nVisibleLines > textD->nBufferLines)
        setScroll(textD, max(1, textD->nBufferLines - textD->nVisibleLines + 
                                2 + TEXT_OF_TEXTD(textD).cursorVPadding),
                  textD->horizOffset, False, False);
    
    /* Update the scroll bar page increment size (as well as other scroll
       bar parameters.  If updating the horizontal range caused scrolling,
       redraw */
    updateVScrollBarRange(textD);
    if (updateHScrollBarRange(textD))
        redrawAll = True;

    /* If a full redraw is needed */
    if (redrawAll && canRedraw)
        TextDRedisplayRect(textD, textD->left, textD->top, textD->width,
                textD->height);

    /* Decide if the horizontal scroll bar needs to be visible */
    hideOrShowHScrollBar(textD);
    
    /* Refresh the line number display to draw more line numbers, or
       erase extras */
    redrawLineNumbers(textD, True);
    
    /* Redraw the calltip */
    TextDRedrawCalltip(textD, 0);
}

/*
** Refresh a rectangle of the text display.  left and top are in coordinates of
** the text drawing window
*/
void TextDRedisplayRect(textDisp *textD, int left, int top, int width,
	int height)
{
    int fontHeight, firstLine, lastLine, line;
    
    /* find the line number range of the display */
    fontHeight = textD->ascent + textD->descent;
    firstLine = (top - textD->top - fontHeight + 1) / fontHeight;
    lastLine = (top + height - textD->top) / fontHeight;
    
    /* If the graphics contexts are shared using XtAllocateGC, their
       clipping rectangles may have changed since the last use */
    resetClipRectangles(textD);
    
    /* draw the lines of text */
    for (line=firstLine; line<=lastLine; line++)
    	redisplayLine(textD, line, left, left+width, 0, INT_MAX);
    
    /* draw the line numbers if exposed area includes them */
    if (textD->lineNumWidth != 0 && left <= textD->lineNumLeft + textD->lineNumWidth)
	redrawLineNumbers(textD, False);
}

/*
** Refresh all of the text between buffer positions "start" and "end"
** not including the character at the position "end".
** If end points beyond the end of the buffer, refresh the whole display
** after pos, including blank lines which are not technically part of
** any range of characters.
*/
static void textDRedisplayRange(textDisp *textD, int start, int end)
{
    int i, startLine, lastLine, startIndex, endIndex;
    
    /* If the range is outside of the displayed text, just return */
    if (end < textD->firstChar || (start > textD->lastChar &&
    	    !emptyLinesVisible(textD)))
        return;
       
    /* Clean up the starting and ending values */
    if (start < 0) start = 0;
    if (start > textD->buffer->length) start = textD->buffer->length;
    if (end < 0) end = 0;
    if (end > textD->buffer->length) end = textD->buffer->length;
    
    /* Get the starting and ending lines */
    if (start < textD->firstChar) {
    	start = textD->firstChar;
    }

    if (!posToVisibleLineNum(textD, start, &startLine)) {
    	startLine = textD->nVisibleLines - 1;
    }

    if (end >= textD->lastChar) {
    	lastLine = textD->nVisibleLines - 1;
    } else {
    	if (!posToVisibleLineNum(textD, end, &lastLine)) {
    	    /* shouldn't happen */
    	    lastLine = textD->nVisibleLines - 1;
    	}
    }

    /* Get the starting and ending positions within the lines */
    startIndex = (textD->lineStarts[startLine] == -1)
            ? 0
            : start - textD->lineStarts[startLine];
    if (end >= textD->lastChar)
    {
        /*  Request to redisplay beyond textD->lastChar, so tell
            redisplayLine() to display everything to infy.  */
        endIndex = INT_MAX;
    } else if (textD->lineStarts[lastLine] == -1)
    {
        /*  Here, lastLine is determined by posToVisibleLineNum() (see
            if/else above) but deemed to be out of display according to
            textD->lineStarts. */
        endIndex = 0;
    } else
    {
        endIndex = end - textD->lineStarts[lastLine];
    }
    
    /* Reset the clipping rectangles for the drawing GCs which are shared
       using XtAllocateGC, and may have changed since the last use */
    resetClipRectangles(textD);
    
    /* If the starting and ending lines are the same, redisplay the single
       line between "start" and "end" */
    if (startLine == lastLine) {
        redisplayLine(textD, startLine, 0, INT_MAX, startIndex, endIndex);
    	return;
    }
    
    /* Redisplay the first line from "start" */
    redisplayLine(textD, startLine, 0, INT_MAX, startIndex, INT_MAX);
    
    /* Redisplay the lines in between at their full width */
    for (i=startLine+1; i<lastLine; i++)
	redisplayLine(textD, i, 0, INT_MAX, 0, INT_MAX);

    /* Redisplay the last line to "end" */
    redisplayLine(textD, lastLine, 0, INT_MAX, 0, endIndex);
}

/*
** Set the scroll position of the text display vertically by line number and
** horizontally by pixel offset from the left margin
*/
void TextDSetScroll(textDisp *textD, int topLineNum, int horizOffset)
{
    int sliderSize, sliderMax;
    int vPadding = (int)(TEXT_OF_TEXTD(textD).cursorVPadding);
    
    /* Limit the requested scroll position to allowable values */
    if (topLineNum < 1)
        topLineNum = 1;
    else if ((topLineNum > textD->topLineNum) &&
             (topLineNum > (textD->nBufferLines + 2 - textD->nVisibleLines +
                          vPadding)))
        topLineNum = max(textD->topLineNum,
                textD->nBufferLines + 2 - textD->nVisibleLines + vPadding);
    XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, 
            XmNsliderSize, &sliderSize, NULL);
    if (horizOffset < 0)
        horizOffset = 0;
    if (horizOffset > sliderMax - sliderSize)
        horizOffset = sliderMax - sliderSize;

    setScroll(textD, topLineNum, horizOffset, True, True);
}

/*
** Get the current scroll position for the text display, in terms of line
** number of the top line and horizontal pixel offset from the left margin
*/
void TextDGetScroll(textDisp *textD, int *topLineNum, int *horizOffset)
{
    *topLineNum = textD->topLineNum;
    *horizOffset = textD->horizOffset;
}

/*
** Set the position of the text insertion cursor for text display "textD"
*/
void TextDSetInsertPosition(textDisp *textD, int newPos)
{
    /* make sure new position is ok, do nothing if it hasn't changed */
    if (newPos == textD->cursorPos)
    	return;
    if (newPos < 0) newPos = 0;
    if (newPos > textD->buffer->length) newPos = textD->buffer->length;
 
    /* cursor movement cancels vertical cursor motion column */
    textD->cursorPreferredCol = -1;
   
    /* erase the cursor at it's previous position */
    TextDBlankCursor(textD);
    
    /* draw it at its new position */
    textD->cursorPos = newPos;
    textD->cursorOn = True;
    textDRedisplayRange(textD, textD->cursorPos-1, textD->cursorPos + 1);
}

void TextDBlankCursor(textDisp *textD)
{
    if (!textD->cursorOn)
    	return;
    
    blankCursorProtrusions(textD);
    textD->cursorOn = False;
    textDRedisplayRange(textD, textD->cursorPos-1, textD->cursorPos+1);
}

void TextDUnblankCursor(textDisp *textD)
{
    if (!textD->cursorOn) {
    	textD->cursorOn = True;
        textDRedisplayRange(textD, textD->cursorPos-1, textD->cursorPos+1);
    }
}

void TextDSetCursorStyle(textDisp *textD, int style)
{
    textD->cursorStyle = style;
    blankCursorProtrusions(textD);
    if (textD->cursorOn) {
        textDRedisplayRange(textD, textD->cursorPos-1, textD->cursorPos + 1);
    }
}

void TextDSetWrapMode(textDisp *textD, int wrap, int wrapMargin)
{
    textD->wrapMargin = wrapMargin;
    textD->continuousWrap = wrap;
    
    /* wrapping can change change the total number of lines, re-count */
    textD->nBufferLines = TextDCountLines(textD, 0, textD->buffer->length,True);
    
    /* changing wrap margins wrap or changing from wrapped mode to non-wrapped
       can leave the character at the top no longer at a line start, and/or
       change the line number */
    textD->firstChar = TextDStartOfLine(textD, textD->firstChar);
    textD->topLineNum = TextDCountLines(textD, 0, textD->firstChar, True) + 1;
    resetAbsLineNum(textD);
        
    /* update the line starts array */
    calcLineStarts(textD, 0, textD->nVisibleLines);
    calcLastChar(textD);
    
    /* Update the scroll bar page increment size (as well as other scroll
       bar parameters) */
    updateVScrollBarRange(textD);
    updateHScrollBarRange(textD);
    
    /* Decide if the horizontal scroll bar needs to be visible */
    hideOrShowHScrollBar(textD);

    /* Do a full redraw */
    TextDRedisplayRect(textD, 0, textD->top, textD->width + textD->left,
	    textD->height);
}

int TextDGetInsertPosition(textDisp *textD)
{
    return textD->cursorPos;
}

/*
** Insert "text" at the current cursor location.  This has the same
** effect as inserting the text into the buffer using BufInsert and
** then moving the insert position after the newly inserted text, except
** that it's optimized to do less redrawing.
*/
void TextDInsert(textDisp *textD, char *text)
{
    int pos = textD->cursorPos;
    
    textD->cursorToHint = pos + strlen(text);
    BufInsert(textD->buffer, pos, text);
    textD->cursorToHint = NO_HINT;
}

/*
** Insert "text" (which must not contain newlines), overstriking the current
** cursor location.
*/
void TextDOverstrike(textDisp *textD, char *text)
{
    int startPos = textD->cursorPos;
    textBuffer *buf = textD->buffer;
    int lineStart = BufStartOfLine(buf, startPos);
    int textLen = strlen(text);
    int i, p, endPos, indent, startIndent, endIndent;
    char *c, ch, *paddedText = NULL;
    
    /* determine how many displayed character positions are covered */
    startIndent = BufCountDispChars(textD->buffer, lineStart, startPos);
    indent = startIndent;
    for (c=text; *c!='\0'; c++)
    	indent += BufCharWidth(*c, indent, buf->tabDist, buf->nullSubsChar);
    endIndent = indent;
    
    /* find which characters to remove, and if necessary generate additional
       padding to make up for removed control characters at the end */
    indent=startIndent;
    for (p=startPos; ; p++) {
    	if (p == buf->length)
    	    break;
    	ch = BufGetCharacter(buf, p);
    	if (ch == '\n')
    	    break;
    	indent += BufCharWidth(ch, indent, buf->tabDist, buf->nullSubsChar);
    	if (indent == endIndent) {
    	    p++;
    	    break;
    	} else if (indent > endIndent) {
    	    if (ch != '\t') {
    	    	p++;
    	    	paddedText = (char*)NEditMalloc(textLen + MAX_EXP_CHAR_LEN + 1);
    	    	strcpy(paddedText, text);
    	    	for (i=0; i<indent-endIndent; i++)
    	    	    paddedText[textLen+i] = ' ';
    	    	paddedText[textLen+i] = '\0';
    	    }
    	    break;
    	}
    }
    endPos = p;	    
    
    textD->cursorToHint = startPos + textLen;
    BufReplace(buf, startPos, endPos, paddedText == NULL ? text : paddedText);
    textD->cursorToHint = NO_HINT;
    NEditFree(paddedText);
}

/*
** Translate window coordinates to the nearest text cursor position.
*/
int TextDXYToPosition(textDisp *textD, int x, int y)
{
    return xyToPos(textD, x, y, CURSOR_POS);
}

/*
** Translate window coordinates to the nearest character cell.
*/
int TextDXYToCharPos(textDisp *textD, int x, int y)
{
    return xyToPos(textD, x, y, CHARACTER_POS);
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
void TextDXYToUnconstrainedPosition(textDisp *textD, int x, int y, int *row,
	int *column)
{
    xyToUnconstrainedPos(textD, x, y, row, column, CURSOR_POS);
}

/*
** Translate line and column to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font
** is proportional, since there are no absolute columns.
*/
int TextDLineAndColToPos(textDisp *textD, int lineNum, int column)
{
    int i, lineEnd, charIndex, outIndex;
    int lineStart=0, charLen=0;
    char *lineStr, expandedChar[MAX_EXP_CHAR_LEN];

    /* Count lines */
    if (lineNum < 1)
        lineNum = 1;
    lineEnd = -1;
    for (i=1; i<=lineNum && lineEnd<textD->buffer->length; i++) {
        lineStart = lineEnd + 1;
        lineEnd = BufEndOfLine(textD->buffer, lineStart);
    }

    /* If line is beyond end of buffer, position at last character in buffer */
    if ( lineNum >= i ) {
      return lineEnd;
    }

    /* Start character index at zero */
    charIndex=0;

    /* Only have to count columns if column isn't zero (or negative) */
    if (column > 0) {
      /* Count columns, expanding each character */
      lineStr = BufGetRange(textD->buffer, lineStart, lineEnd);
      outIndex = 0;
      for(i=lineStart; i<lineEnd; i++, charIndex++) {
          charLen = BufExpandCharacter(lineStr[charIndex], outIndex,
                  expandedChar, textD->buffer->tabDist,
                  textD->buffer->nullSubsChar);
          if ( outIndex+charLen >= column ) break;
          outIndex+=charLen;
      }

      /* If the column is in the middle of an expanded character, put cursor
       * in front of character if in first half of character, and behind
       * character if in last half of character
       */
      if (column >= outIndex + ( charLen / 2 ))
        charIndex++;

      /* If we are beyond the end of the line, back up one space */
      if ((i>=lineEnd)&&(charIndex>0)) charIndex--;
    }

    /* Position is the start of the line plus the index into line buffer */
    return lineStart + charIndex;
}

/*
** Translate a buffer text position to the XY location where the center
** of the cursor would be positioned to point to that character.  Returns
** False if the position is not displayed because it is VERTICALLY out
** of view.  If the position is horizontally out of view, returns the
** x coordinate where the position would be if it were visible.
*/
int TextDPositionToXY(textDisp *textD, int pos, int *x, int *y)
{
    int charIndex, lineStartPos, fontHeight, lineLen;
    int visLineNum, charLen, outIndex, xStep, charStyle;
    char *lineStr, expandedChar[MAX_EXP_CHAR_LEN];
    
    /* If position is not displayed, return false */
    if (pos < textD->firstChar ||
    	    (pos > textD->lastChar && !emptyLinesVisible(textD)))
    	return False;
    	
    /* Calculate y coordinate */
    if (!posToVisibleLineNum(textD, pos, &visLineNum))
    	return False;
    fontHeight = textD->ascent + textD->descent;
    *y = textD->top + visLineNum*fontHeight + fontHeight/2;
    
    /* Get the text, length, and  buffer position of the line. If the position
       is beyond the end of the buffer and should be at the first position on
       the first empty line, don't try to get or scan the text  */
    lineStartPos = textD->lineStarts[visLineNum];
    if (lineStartPos == -1) {
    	*x = textD->left - textD->horizOffset;
    	return True;
    }
    lineLen = visLineLength(textD, visLineNum);
    lineStr = BufGetRange(textD->buffer, lineStartPos, lineStartPos + lineLen);
    
    /* Step through character positions from the beginning of the line
       to "pos" to calculate the x coordinate */
    xStep = textD->left - textD->horizOffset;
    outIndex = 0;
    for(charIndex=0; charIndex<pos-lineStartPos; charIndex++) {
    	charLen = BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar,
    		textD->buffer->tabDist, textD->buffer->nullSubsChar);
   	charStyle = styleOfPos(textD, lineStartPos, lineLen, charIndex,
   	    	outIndex, lineStr[charIndex]);
    	xStep += stringWidth(textD, expandedChar, charLen, charStyle);
    	outIndex += charLen;
    }
    *x = xStep;
    NEditFree(lineStr);
    return True;
}

/*
** If the text widget is maintaining a line number count appropriate to "pos"
** return the line and column numbers of pos, otherwise return False.  If
** continuous wrap mode is on, returns the absolute line number (as opposed to
** the wrapped line number which is used for scrolling).  THIS ROUTINE ONLY
** WORKS FOR DISPLAYED LINES AND, IN CONTINUOUS WRAP MODE, ONLY WHEN THE
** ABSOLUTE LINE NUMBER IS BEING MAINTAINED.  Otherwise, it returns False.
*/
int TextDPosToLineAndCol(textDisp *textD, int pos, int *lineNum, int *column)
{
    textBuffer *buf = textD->buffer;
    
    /* In continuous wrap mode, the absolute (non-wrapped) line count is
       maintained separately, as needed.  Only return it if we're actually
       keeping track of it and pos is in the displayed text */
    if (textD->continuousWrap) {
	if (!maintainingAbsTopLineNum(textD) || pos < textD->firstChar ||
		pos > textD->lastChar)
	    return False;
	*lineNum = textD->absTopLineNum + BufCountLines(buf,
		textD->firstChar, pos);
	*column = BufCountDispChars(buf, BufStartOfLine(buf, pos), pos);
	return True;
    }

    /* Only return the data if pos is within the displayed text */
    if (!posToVisibleLineNum(textD, pos, lineNum))
	return False;
    *column = BufCountDispChars(buf, textD->lineStarts[*lineNum], pos);
    *lineNum += textD->topLineNum;
    return True;
}

/*
** Return True if position (x, y) is inside of the primary selection
*/
int TextDInSelection(textDisp *textD, int x, int y)
{
    int row, column, pos = xyToPos(textD, x, y, CHARACTER_POS);
    textBuffer *buf = textD->buffer;
    
    xyToUnconstrainedPos(textD, x, y, &row, &column, CHARACTER_POS);
    if (rangeTouchesRectSel(&buf->primary, textD->firstChar, textD->lastChar))
    	column = TextDOffsetWrappedColumn(textD, row, column);
    return inSelection(&buf->primary, pos, BufStartOfLine(buf, pos), column);
}

/*
** Correct a column number based on an unconstrained position (as returned by
** TextDXYToUnconstrainedPosition) to be relative to the last actual newline
** in the buffer before the row and column position given, rather than the
** last line start created by line wrapping.  This is an adapter
** for rectangular selections and code written before continuous wrap mode,
** which thinks that the unconstrained column is the number of characters
** from the last newline.  Obviously this is time consuming, because it
** invloves character re-counting.
*/
int TextDOffsetWrappedColumn(textDisp *textD, int row, int column)
{
    int lineStart, dispLineStart;
    
    if (!textD->continuousWrap || row < 0 || row > textD->nVisibleLines)
    	return column;
    dispLineStart = textD->lineStarts[row];
    if (dispLineStart == -1)
    	return column;
    lineStart = BufStartOfLine(textD->buffer, dispLineStart);
    return column + BufCountDispChars(textD->buffer, lineStart, dispLineStart);
}

/*
** Correct a row number from an unconstrained position (as returned by
** TextDXYToUnconstrainedPosition) to a straight number of newlines from the
** top line of the display.  Because rectangular selections are based on
** newlines, rather than display wrapping, and anywhere a rectangular selection
** needs a row, it needs it in terms of un-wrapped lines.
*/
int TextDOffsetWrappedRow(textDisp *textD, int row)
{
    if (!textD->continuousWrap || row < 0 || row > textD->nVisibleLines)
    	return row;
    return BufCountLines(textD->buffer, textD->firstChar, 
    	    textD->lineStarts[row]);
}

/*
** Scroll the display to bring insertion cursor into view.
**
** Note: it would be nice to be able to do this without counting lines twice
** (setScroll counts them too) and/or to count from the most efficient
** starting point, but the efficiency of this routine is not as important to
** the overall performance of the text display.
*/
void TextDMakeInsertPosVisible(textDisp *textD)
{
    int hOffset, topLine, x, y;
    int cursorPos = textD->cursorPos;
    int linesFromTop = 0, do_padding = 1; 
    int cursorVPadding = (int)TEXT_OF_TEXTD(textD).cursorVPadding;
    
    hOffset = textD->horizOffset;
    topLine = textD->topLineNum;
    
    /* Don't do padding if this is a mouse operation */
    do_padding = ((TEXT_OF_TEXTD(textD).dragState == NOT_CLICKED) &&
                  (cursorVPadding > 0));
    
    /* Find the new top line number */
    if (cursorPos < textD->firstChar) {
        topLine -= TextDCountLines(textD, cursorPos, textD->firstChar, False);
        /* linesFromTop = 0; */
    } else if (cursorPos > textD->lastChar && !emptyLinesVisible(textD)) {
        topLine += TextDCountLines(textD, textD->lastChar -
                (wrapUsesCharacter(textD, textD->lastChar) ? 0 : 1),
                cursorPos, False);
        linesFromTop = textD->nVisibleLines-1;
    } else if (cursorPos == textD->lastChar && !emptyLinesVisible(textD) &&
            !wrapUsesCharacter(textD, textD->lastChar)) {
        topLine++;
        linesFromTop = textD->nVisibleLines-1;
    } else {
        /* Avoid extra counting if cursorVPadding is disabled */
        if (do_padding)
            linesFromTop = TextDCountLines(textD, textD->firstChar, 
                    cursorPos, True);
    }
    if (topLine < 1) {
        fprintf(stderr, "nedit: internal consistency check tl1 failed\n");
        topLine = 1;
    }
    
    if (do_padding) {
        /* Keep the cursor away from the top or bottom of screen. */
        if (textD->nVisibleLines <= 2*(int)cursorVPadding) {
            topLine += (linesFromTop - textD->nVisibleLines/2);
            topLine = max(topLine, 1);
        } else if (linesFromTop < (int)cursorVPadding) {
            topLine -= (cursorVPadding - linesFromTop);
            topLine = max(topLine, 1);
        } else if (linesFromTop > textD->nVisibleLines-(int)cursorVPadding-1) {
            topLine += (linesFromTop - (textD->nVisibleLines-cursorVPadding-1));
        }
    }
    
    /* Find the new setting for horizontal offset (this is a bit ungraceful).
       If the line is visible, just use TextDPositionToXY to get the position
       to scroll to, otherwise, do the vertical scrolling first, then the
       horizontal */
    if (!TextDPositionToXY(textD, cursorPos, &x, &y)) {
        setScroll(textD, topLine, hOffset, True, True);
        if (!TextDPositionToXY(textD, cursorPos, &x, &y))
            return; /* Give up, it's not worth it (but why does it fail?) */
    }
    if (x > textD->left + textD->width)
        hOffset += x - (textD->left + textD->width);
    else if (x < textD->left)
        hOffset += x - textD->left;
    
    /* Do the scroll */
    setScroll(textD, topLine, hOffset, True, True);
}

/*
** Return the current preferred column along with the current
** visible line index (-1 if not visible) and the lineStartPos
** of the current insert position.
*/
int TextDPreferredColumn(textDisp *textD, int *visLineNum, int *lineStartPos)
{
    int column;

    /* Find the position of the start of the line.  Use the line starts array
    if possible, to avoid unbounded line-counting in continuous wrap mode */
    if (posToVisibleLineNum(textD, textD->cursorPos, visLineNum)) {
        *lineStartPos = textD->lineStarts[*visLineNum];
    }
    else {
        *lineStartPos = TextDStartOfLine(textD, textD->cursorPos);
        *visLineNum = -1;
    }

    /* Decide what column to move to, if there's a preferred column use that */
    column = (textD->cursorPreferredCol >= 0)
            ? textD->cursorPreferredCol
            : BufCountDispChars(textD->buffer, *lineStartPos, textD->cursorPos);
    return(column);
}

/*
** Return the insert position of the requested column given
** the lineStartPos.
*/
int TextDPosOfPreferredCol(textDisp *textD, int column, int lineStartPos)
{
    int newPos;

    newPos = BufCountForwardDispChars(textD->buffer, lineStartPos, column);
    if (textD->continuousWrap) {
        newPos = min(newPos, TextDEndOfLine(textD, lineStartPos, True));
    }
    return(newPos);
}

/*
** Cursor movement functions
*/
int TextDMoveRight(textDisp *textD)
{
    if (textD->cursorPos >= textD->buffer->length)
    	return False;
    TextDSetInsertPosition(textD, textD->cursorPos + 1);
    return True;
}

int TextDMoveLeft(textDisp *textD)
{
    if (textD->cursorPos <= 0)
    	return False;
    TextDSetInsertPosition(textD, textD->cursorPos - 1); 
    return True;
}

int TextDMoveUp(textDisp *textD, int absolute)
{
    int lineStartPos, column, prevLineStartPos, newPos, visLineNum;
    
    /* Find the position of the start of the line.  Use the line starts array
       if possible, to avoid unbounded line-counting in continuous wrap mode */
    if (absolute) {
        lineStartPos = BufStartOfLine(textD->buffer, textD->cursorPos);
        visLineNum = -1;
    } else if (posToVisibleLineNum(textD, textD->cursorPos, &visLineNum))
    	lineStartPos = textD->lineStarts[visLineNum];
    else {
    	lineStartPos = TextDStartOfLine(textD, textD->cursorPos);
    	visLineNum = -1;
    }
    if (lineStartPos == 0)
    	return False;
    
    /* Decide what column to move to, if there's a preferred column use that */
    column = textD->cursorPreferredCol >= 0
            ? textD->cursorPreferredCol
            : BufCountDispChars(textD->buffer, lineStartPos, textD->cursorPos);
    
    /* count forward from the start of the previous line to reach the column */
    if (absolute) {
        prevLineStartPos = BufCountBackwardNLines(textD->buffer, lineStartPos, 1);
    } else if (visLineNum != -1 && visLineNum != 0) {
        prevLineStartPos = textD->lineStarts[visLineNum-1];
    } else {
        prevLineStartPos = TextDCountBackwardNLines(textD, lineStartPos, 1);
    }

    newPos = BufCountForwardDispChars(textD->buffer, prevLineStartPos, column);
    if (textD->continuousWrap && !absolute)
    	newPos = min(newPos, TextDEndOfLine(textD, prevLineStartPos, True));
    
    /* move the cursor */
    TextDSetInsertPosition(textD, newPos);
    
    /* if a preferred column wasn't aleady established, establish it */
    textD->cursorPreferredCol = column;
    
    return True;
}

int TextDMoveDown(textDisp *textD, int absolute)
{
    int lineStartPos, column, nextLineStartPos, newPos, visLineNum;

    if (textD->cursorPos == textD->buffer->length) {
        return False;
    }

    if (absolute) {
        lineStartPos = BufStartOfLine(textD->buffer, textD->cursorPos);
        visLineNum = -1;
    } else if (posToVisibleLineNum(textD, textD->cursorPos, &visLineNum)) {
        lineStartPos = textD->lineStarts[visLineNum];
    } else {
        lineStartPos = TextDStartOfLine(textD, textD->cursorPos);
        visLineNum = -1;
    }

    column = textD->cursorPreferredCol >= 0
            ? textD->cursorPreferredCol
            : BufCountDispChars(textD->buffer, lineStartPos, textD->cursorPos);

    if (absolute)
        nextLineStartPos = BufCountForwardNLines(textD->buffer, lineStartPos, 1);
    else
        nextLineStartPos = TextDCountForwardNLines(textD, lineStartPos, 1, True);

    newPos = BufCountForwardDispChars(textD->buffer, nextLineStartPos, column);

    if (textD->continuousWrap && !absolute) {
        newPos = min(newPos, TextDEndOfLine(textD, nextLineStartPos, True));
    }

    TextDSetInsertPosition(textD, newPos);
    textD->cursorPreferredCol = column;
    
    return True;
}

/*
** Same as BufCountLines, but takes in to account wrapping if wrapping is
** turned on.  If the caller knows that startPos is at a line start, it
** can pass "startPosIsLineStart" as True to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
*/
int TextDCountLines(textDisp *textD, int startPos, int endPos,
    	int startPosIsLineStart)
{
    int retLines, retPos, retLineStart, retLineEnd;
    
    /* If we're not wrapping use simple (and more efficient) BufCountLines */
    if (!textD->continuousWrap)
    	return BufCountLines(textD->buffer, startPos, endPos);
    
    wrappedLineCounter(textD, textD->buffer, startPos, endPos, INT_MAX,
	    startPosIsLineStart, 0, &retPos, &retLines, &retLineStart,
	    &retLineEnd);
    return retLines;
}

/*
** Same as BufCountForwardNLines, but takes in to account line breaks when
** wrapping is turned on. If the caller knows that startPos is at a line start,
** it can pass "startPosIsLineStart" as True to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
*/
int TextDCountForwardNLines(const textDisp* textD, int startPos,
        unsigned nLines, Boolean startPosIsLineStart)
{
    int retLines, retPos, retLineStart, retLineEnd;
    
    /* if we're not wrapping use more efficient BufCountForwardNLines */
    if (!textD->continuousWrap)
    	return BufCountForwardNLines(textD->buffer, startPos, nLines);
    
    /* wrappedLineCounter can't handle the 0 lines case */
    if (nLines == 0)
    	return startPos;
    
    /* use the common line counting routine to count forward */
    wrappedLineCounter(textD, textD->buffer, startPos, textD->buffer->length,
    	    nLines, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart,
    	    &retLineEnd);
    return retPos;
}

/*
** Same as BufEndOfLine, but takes in to account line breaks when wrapping
** is turned on.  If the caller knows that startPos is at a line start, it
** can pass "startPosIsLineStart" as True to make the call more efficient
** by avoiding the additional step of scanning back to the last newline.
**
** Note that the definition of the end of a line is less clear when continuous
** wrap is on.  With continuous wrap off, it's just a pointer to the newline
** that ends the line.  When it's on, it's the character beyond the last
** DISPLAYABLE character on the line, where a whitespace character which has
** been "converted" to a newline for wrapping is not considered displayable.
** Also note that, a line can be wrapped at a non-whitespace character if the
** line had no whitespace.  In this case, this routine returns a pointer to
** the start of the next line.  This is also consistent with the model used by
** visLineLength.
*/
int TextDEndOfLine(const textDisp* textD, int pos,
        Boolean startPosIsLineStart)
{
    int retLines, retPos, retLineStart, retLineEnd;
    
    /* If we're not wrapping use more efficient BufEndOfLine */
    if (!textD->continuousWrap)
    	return BufEndOfLine(textD->buffer, pos);
    
    if (pos == textD->buffer->length)
    	return pos;
    wrappedLineCounter(textD, textD->buffer, pos, textD->buffer->length, 1,
    	    startPosIsLineStart, 0, &retPos, &retLines, &retLineStart,
	    &retLineEnd);
    return retLineEnd;
}

/*
** Same as BufStartOfLine, but returns the character after last wrap point
** rather than the last newline.
*/
int TextDStartOfLine(const textDisp* textD, int pos)
{
    int retLines, retPos, retLineStart, retLineEnd;
    
    /* If we're not wrapping, use the more efficient BufStartOfLine */
    if (!textD->continuousWrap)
    	return BufStartOfLine(textD->buffer, pos);

    wrappedLineCounter(textD, textD->buffer, BufStartOfLine(textD->buffer, pos),
    	    pos, INT_MAX, True, 0, &retPos, &retLines, &retLineStart, 
	    &retLineEnd);
    return retLineStart;
}

/*
** Same as BufCountBackwardNLines, but takes in to account line breaks when
** wrapping is turned on.
*/
int TextDCountBackwardNLines(textDisp *textD, int startPos, int nLines)
{
    textBuffer *buf = textD->buffer;
    int pos, lineStart, retLines, retPos, retLineStart, retLineEnd;
    
    /* If we're not wrapping, use the more efficient BufCountBackwardNLines */
    if (!textD->continuousWrap)
    	return BufCountBackwardNLines(textD->buffer, startPos, nLines);

    pos = startPos;
    while (True) {
	lineStart = BufStartOfLine(buf, pos);
	wrappedLineCounter(textD, textD->buffer, lineStart, pos, INT_MAX,
	    	True, 0, &retPos, &retLines, &retLineStart, &retLineEnd);
	if (retLines > nLines)
    	    return TextDCountForwardNLines(textD, lineStart, retLines-nLines,
    	    	    True);
    	nLines -= retLines;
    	pos = lineStart - 1;
    	if (pos < 0)
    	    return 0;
    	nLines -= 1;
    }
}

/*
** Callback attached to the text buffer to receive delete information before
** the modifications are actually made.
*/
static void bufPreDeleteCB(int pos, int nDeleted, void *cbArg)
{
    textDisp *textD = (textDisp *)cbArg;
    if (textD->continuousWrap && 
        (textD->fixedFontWidth == -1 || textD->modifyingTabDist))
	/* Note: we must perform this measurement, even if there is not a
	   single character deleted; the number of "deleted" lines is the
	   number of visual lines spanned by the real line in which the 
	   modification takes place. 
	   Also, a modification of the tab distance requires the same
	   kind of calculations in advance, even if the font width is "fixed",
	   because when the width of the tab characters changes, the layout 
	   of the text may be completely different. */
	measureDeletedLines(textD, pos, nDeleted);
    else
	textD->suppressResync = 0; /* Probably not needed, but just in case */
}

/*
** Callback attached to the text buffer to receive modification information
*/
static void bufModifiedCB(int pos, int nInserted, int nDeleted,
	int nRestyled, const char *deletedText, void *cbArg)
{
    int linesInserted, linesDeleted, startDispPos, endDispPos;
    textDisp *textD = (textDisp *)cbArg;
    textBuffer *buf = textD->buffer;
    int oldFirstChar = textD->firstChar;
    int scrolled, origCursorPos = textD->cursorPos;
    int wrapModStart, wrapModEnd;
 
    /* buffer modification cancels vertical cursor motion column */
    if (nInserted != 0 || nDeleted != 0)
    	textD->cursorPreferredCol = -1;
    
    /* Count the number of lines inserted and deleted, and in the case
       of continuous wrap mode, how much has changed */
    if (textD->continuousWrap) {
    	findWrapRange(textD, deletedText, pos, nInserted, nDeleted,
    	    	&wrapModStart, &wrapModEnd, &linesInserted, &linesDeleted);
    } else {
	linesInserted = nInserted == 0 ? 0 :
    		BufCountLines(buf, pos, pos + nInserted);
	linesDeleted = nDeleted == 0 ? 0 : countLines(deletedText);
    }

    /* Update the line starts and topLineNum */
    if (nInserted != 0 || nDeleted != 0) {
	if (textD->continuousWrap) {
	    updateLineStarts(textD, wrapModStart, wrapModEnd-wrapModStart,
	    	    nDeleted + pos-wrapModStart + (wrapModEnd-(pos+nInserted)),
	    	    linesInserted, linesDeleted, &scrolled);
	} else {
	    updateLineStarts(textD, pos, nInserted, nDeleted, linesInserted,
    		    linesDeleted, &scrolled);
	}
    } else
    	scrolled = False;
    
    /* If we're counting non-wrapped lines as well, maintain the absolute
       (non-wrapped) line number of the text displayed */
    if (maintainingAbsTopLineNum(textD) && (nInserted != 0 || nDeleted != 0)) {
	if (pos + nDeleted < oldFirstChar)
	    textD->absTopLineNum += BufCountLines(buf, pos, pos + nInserted) -
		    countLines(deletedText);
	else if (pos < oldFirstChar)
	    resetAbsLineNum(textD);
    }    	    
    
    /* Update the line count for the whole buffer */
    textD->nBufferLines += linesInserted - linesDeleted;
        
    /* Update the scroll bar ranges (and value if the value changed).  Note
       that updating the horizontal scroll bar range requires scanning the
       entire displayed text, however, it doesn't seem to hurt performance
       much.  Note also, that the horizontal scroll bar update routine is
       allowed to re-adjust horizOffset if there is blank space to the right
       of all lines of text. */
    updateVScrollBarRange(textD);
    scrolled |= updateHScrollBarRange(textD);
    
    /* Update the cursor position */
    if (textD->cursorToHint != NO_HINT) {
    	textD->cursorPos = textD->cursorToHint;
    	textD->cursorToHint = NO_HINT;
    } else if (textD->cursorPos > pos) {
    	if (textD->cursorPos < pos + nDeleted)
    	    textD->cursorPos = pos;
    	else
    	    textD->cursorPos += nInserted - nDeleted;
    }

    /* If the changes caused scrolling, re-paint everything and we're done. */
    if (scrolled) {
    	blankCursorProtrusions(textD);
    	TextDRedisplayRect(textD, 0, textD->top, textD->width + textD->left,
		textD->height);
        if (textD->styleBuffer) {/* See comments in extendRangeForStyleMods */
    	    textD->styleBuffer->primary.selected = False;
            textD->styleBuffer->primary.zeroWidth = False;
        }
    	return;
    }
    
    /* If the changes didn't cause scrolling, decide the range of characters
       that need to be re-painted.  Also if the cursor position moved, be
       sure that the redisplay range covers the old cursor position so the
       old cursor gets erased, and erase the bits of the cursor which extend
       beyond the left and right edges of the text. */
    startDispPos = textD->continuousWrap ? wrapModStart : pos;
    if (origCursorPos == startDispPos && textD->cursorPos != startDispPos)
    	startDispPos = min(startDispPos, origCursorPos-1);
    if (linesInserted == linesDeleted) {
        if (nInserted == 0 && nDeleted == 0)
            endDispPos = pos + nRestyled;
        else {
    	    endDispPos = textD->continuousWrap ? wrapModEnd :
    	    	    BufEndOfLine(buf, pos + nInserted) + 1;
    	    if (origCursorPos >= startDispPos &&
    	    	    (origCursorPos <= endDispPos || endDispPos == buf->length))
    	    	blankCursorProtrusions(textD);
    	}
        /* If more than one line is inserted/deleted, a line break may have
           been inserted or removed in between, and the line numbers may
           have changed. If only one line is altered, line numbers cannot
           be affected (the insertion or removal of a line break always 
           results in at least two lines being redrawn). */
	if (linesInserted > 1) redrawLineNumbers(textD, False);
    } else { /* linesInserted != linesDeleted */
    	endDispPos = textD->lastChar + 1;
    	if (origCursorPos >= pos)
    	    blankCursorProtrusions(textD);
	redrawLineNumbers(textD, False);
    }
    
    /* If there is a style buffer, check if the modification caused additional
       changes that need to be redisplayed.  (Redisplaying separately would
       cause double-redraw on almost every modification involving styled
       text).  Extend the redraw range to incorporate style changes */
    if (textD->styleBuffer)
    	extendRangeForStyleMods(textD, &startDispPos, &endDispPos);
    
    /* Redisplay computed range */
    textDRedisplayRange(textD, startDispPos, endDispPos);
}

/*
** In continuous wrap mode, internal line numbers are calculated after
** wrapping.  A separate non-wrapped line count is maintained when line
** numbering is turned on.  There is some performance cost to maintaining this
** line count, so normally absolute line numbers are not tracked if line
** numbering is off.  This routine allows callers to specify that they still
** want this line count maintained (for use via TextDPosToLineAndCol).
** More specifically, this allows the line number reported in the statistics
** line to be calibrated in absolute lines, rather than post-wrapped lines.
*/
void TextDMaintainAbsLineNum(textDisp *textD, int state)
{
    textD->needAbsTopLineNum = state;
    resetAbsLineNum(textD);
}

/*
** Returns the absolute (non-wrapped) line number of the first line displayed.
** Returns 0 if the absolute top line number is not being maintained.
*/
static int getAbsTopLineNum(textDisp *textD)
{
    if (!textD->continuousWrap)
	return textD->topLineNum;
    if (maintainingAbsTopLineNum(textD))
	return textD->absTopLineNum;
    return 0;
}

/*
** Re-calculate absolute top line number for a change in scroll position.
*/
static void offsetAbsLineNum(textDisp *textD, int oldFirstChar)
{
    if (maintainingAbsTopLineNum(textD)) {
	if (textD->firstChar < oldFirstChar)
	    textD->absTopLineNum -= BufCountLines(textD->buffer,
		    textD->firstChar, oldFirstChar);
	else
	    textD->absTopLineNum += BufCountLines(textD->buffer,
		    oldFirstChar, textD->firstChar);
    }
}

/*
** Return true if a separate absolute top line number is being maintained
** (for displaying line numbers or showing in the statistics line).
*/
static int maintainingAbsTopLineNum(textDisp *textD)
{
    return textD->continuousWrap &&
	    (textD->lineNumWidth != 0 || textD->needAbsTopLineNum);
}

/*
** Count lines from the beginning of the buffer to reestablish the
** absolute (non-wrapped) top line number.  If mode is not continuous wrap,
** or the number is not being maintained, does nothing.
*/
static void resetAbsLineNum(textDisp *textD)
{
    textD->absTopLineNum = 1;
    offsetAbsLineNum(textD, 0);
}

/*
** Find the line number of position "pos" relative to the first line of
** displayed text. Returns False if the line is not displayed.
*/
static int posToVisibleLineNum(textDisp *textD, int pos, int *lineNum)
{
    int i;
    
    if (pos < textD->firstChar)
    	return False;
    if (pos > textD->lastChar) {
    	if (emptyLinesVisible(textD)) {
    	    if (textD->lastChar < textD->buffer->length) {
    		if (!posToVisibleLineNum(textD, textD->lastChar, lineNum)) {
    		    fprintf(stderr, "nedit: Consistency check ptvl failed\n");
    		    return False;
    		}
    		return ++(*lineNum) <= textD->nVisibleLines-1;
            } else {
            	posToVisibleLineNum(textD, max(textD->lastChar-1, 0), lineNum);
            	return True;
            }
	}
	return False;
    }
    	
    for (i=textD->nVisibleLines-1; i>=0; i--) {
    	if (textD->lineStarts[i] != -1 && pos >= textD->lineStarts[i]) {
    	    *lineNum = i;
    	    return True;
    	}
    }

    return False;
}

/*
** Redisplay the text on a single line represented by "visLineNum" (the
** number of lines down from the top of the display), limited by
** "leftClip" and "rightClip" window coordinates and "leftCharIndex" and
** "rightCharIndex" character positions (not including the character at
** position "rightCharIndex").
**
** The cursor is also drawn if it appears on the line.
*/
static void redisplayLine(textDisp *textD, int visLineNum, int leftClip,
	int rightClip, int leftCharIndex, int rightCharIndex)
{
    textBuffer *buf = textD->buffer;
    int i, x, y, startX, charIndex, lineStartPos, lineLen, fontHeight;
    int stdCharWidth, charWidth, startIndex, charStyle, style;
    int charLen, outStartIndex, outIndex, cursorX = 0, hasCursor = False;
    int dispIndexOffset, cursorPos = textD->cursorPos, y_orig;
    char expandedChar[MAX_EXP_CHAR_LEN], outStr[MAX_DISP_LINE_LEN];
    char *lineStr, *outPtr;
    char baseChar;

    /* If line is not displayed, skip it */
    if (visLineNum < 0 || visLineNum >= textD->nVisibleLines)
    	return;

    /* Shrink the clipping range to the active display area */
    leftClip = max(textD->left, leftClip);
    rightClip = min(rightClip, textD->left + textD->width);
    
    if (leftClip > rightClip) {
        return;
    }

    /* Calculate y coordinate of the string to draw */
    fontHeight = textD->ascent + textD->descent;
    y = textD->top + visLineNum * fontHeight;

    /* Get the text, length, and  buffer position of the line to display */
    lineStartPos = textD->lineStarts[visLineNum];
    if (lineStartPos == -1) {
    	lineLen = 0;
    	lineStr = NULL;
    } else {
	lineLen = visLineLength(textD, visLineNum);
	lineStr = BufGetRange(buf, lineStartPos, lineStartPos + lineLen);
    }
    
    /* Space beyond the end of the line is still counted in units of characters
       of a standardized character width (this is done mostly because style
       changes based on character position can still occur in this region due
       to rectangular selections).  stdCharWidth must be non-zero to prevent a
       potential infinite loop if x does not advance */
    stdCharWidth = textD->fontStruct->max_bounds.width;
    if (stdCharWidth <= 0) {
    	fprintf(stderr, "nedit: Internal Error, bad font measurement\n");
    	NEditFree(lineStr);
    	return;
    }
    
    /* Rectangular selections are based on "real" line starts (after a newline
       or start of buffer).  Calculate the difference between the last newline
       position and the line start we're using.  Since scanning back to find a
       newline is expensive, only do so if there's actually a rectangular
       selection which needs it */
    if (textD->continuousWrap && (rangeTouchesRectSel(&buf->primary,
    	    lineStartPos, lineStartPos + lineLen) || rangeTouchesRectSel(
    	    &buf->secondary, lineStartPos, lineStartPos + lineLen) ||
    	    rangeTouchesRectSel(&buf->highlight, lineStartPos,
    	    lineStartPos + lineLen))) {
    	dispIndexOffset = BufCountDispChars(buf,
    	    	BufStartOfLine(buf, lineStartPos), lineStartPos);
    } else
    	dispIndexOffset = 0;

    /* Step through character positions from the beginning of the line (even if
       that's off the left edge of the displayed area) to find the first
       character position that's not clipped, and the x coordinate for drawing
       that character */
    x = textD->left - textD->horizOffset;
    outIndex = 0;

    for (charIndex = 0; ; charIndex++) {
        baseChar = '\0';
        charLen = charIndex >= lineLen
                ? 1
                : BufExpandCharacter(baseChar = lineStr[charIndex], outIndex,
                        expandedChar, buf->tabDist, buf->nullSubsChar);
    	style = styleOfPos(textD, lineStartPos, lineLen, charIndex,
                outIndex + dispIndexOffset, baseChar);
        charWidth = charIndex >= lineLen
                ? stdCharWidth
                : stringWidth(textD, expandedChar, charLen, style);

    	if (x + charWidth >= leftClip && charIndex >= leftCharIndex) {
    	    startIndex = charIndex;
    	    outStartIndex = outIndex;
    	    startX = x;
    	    break;
    	}
    	x += charWidth;
    	outIndex += charLen;
    }

    /* Scan character positions from the beginning of the clipping range, and
       draw parts whenever the style changes (also note if the cursor is on
       this line, and where it should be drawn to take advantage of the x
       position which we've gone to so much trouble to calculate) */
    outPtr = outStr;
    outIndex = outStartIndex;
    x = startX;
    for (charIndex = startIndex; charIndex < rightCharIndex; charIndex++) {
    	if (lineStartPos+charIndex == cursorPos) {
    	    if (charIndex < lineLen
                    || (charIndex == lineLen && cursorPos >= buf->length)) {
    		hasCursor = True;
    		cursorX = x - 1;
    	    } else if (charIndex == lineLen) {
    	    	if (wrapUsesCharacter(textD, cursorPos)) {
    	    	    hasCursor = True;
    	    	    cursorX = x - 1;
    	    	}
    	    }
    	}

        baseChar = '\0';
     	charLen = charIndex >= lineLen
                ? 1
                : BufExpandCharacter(baseChar = lineStr[charIndex], outIndex,
                        expandedChar, buf->tabDist, buf->nullSubsChar);
   	charStyle = styleOfPos(textD, lineStartPos, lineLen, charIndex,
                outIndex + dispIndexOffset, baseChar);
   	for (i = 0; i < charLen; i++) {
            if (i != 0 && charIndex < lineLen && lineStr[charIndex] == '\t') {
                charStyle = styleOfPos(textD, lineStartPos, lineLen, charIndex,
                        outIndex + dispIndexOffset, '\t');
            }

     	    if (charStyle != style) {
    		drawString(textD, style, startX, y, x, outStr, outPtr - outStr);
    		outPtr = outStr;
    		startX = x;
    		style = charStyle;
    	    }

    	    if (charIndex < lineLen) {
    		*outPtr = expandedChar[i];
    		charWidth = stringWidth(textD, &expandedChar[i], 1, charStyle);
    	    } else {
    		charWidth = stdCharWidth;
            }

    	    outPtr++;
    	    x += charWidth;
    	    outIndex++;
	}

        if (outPtr - outStr + MAX_EXP_CHAR_LEN >= MAX_DISP_LINE_LEN
                || x >= rightClip) {
    	    break;
        }
    }
    
    /* Draw the remaining style segment */
    drawString(textD, style, startX, y, x, outStr, outPtr - outStr);
    
    /* Draw the cursor if part of it appeared on the redisplayed part of
       this line.  Also check for the cases which are not caught as the
       line is scanned above: when the cursor appears at the very end
       of the redisplayed section. */
    y_orig = textD->cursorY;
    if (textD->cursorOn) {
        if (hasCursor) {
    	    drawCursor(textD, cursorX, y);
        } else if (charIndex < lineLen
                && (lineStartPos+charIndex+1 == cursorPos)
	    	&& x == rightClip) {
            if (cursorPos >= buf->length) {
    	    	drawCursor(textD, x - 1, y);
            } else {
                if (wrapUsesCharacter(textD, cursorPos)) {
    	    	    drawCursor(textD, x - 1, y);
                }
    	    }
        } else if ((lineStartPos + rightCharIndex) == cursorPos) {
            drawCursor(textD, x - 1, y);
        }
    }
    
    /* If the y position of the cursor has changed, redraw the calltip */
    if (hasCursor && (y_orig != textD->cursorY || y_orig != y))
        TextDRedrawCalltip(textD, 0);
    
    NEditFree(lineStr);
}

/*
** Draw a string or blank area according to parameter "style", using the
** appropriate colors and drawing method for that style, with top left
** corner at x, y.  If style says to draw text, use "string" as source of
** characters, and draw "nChars", if style is FILL, erase
** rectangle where text would have drawn from x to toX and from y to
** the maximum y extent of the current font(s).
*/
static void drawString(textDisp *textD, int style, int x, int y, int toX,
	char *string, int nChars)
{
    GC gc, bgGC;
    XGCValues gcValues;
    XFontStruct *fs = textD->fontStruct;
    Pixel bground = textD->bgPixel;
    Pixel fground = textD->fgPixel;
    int underlineStyle = FALSE;
    
    /* Don't draw if widget isn't realized */
    if (XtWindow(textD->w) == 0)
    	return;
    
    /* select a GC */
    if (style & (STYLE_LOOKUP_MASK | BACKLIGHT_MASK | RANGESET_MASK)) {
        gc = bgGC = textD->styleGC;
    }
    else if (style & HIGHLIGHT_MASK) {
        gc = textD->highlightGC;
        bgGC = textD->highlightBGGC;
    }
    else if (style & PRIMARY_MASK) {
        gc = textD->selectGC;
        bgGC = textD->selectBGGC;
    }
    else {
        gc = bgGC = textD->gc;
    }

    if (gc == textD->styleGC) {
        /* we have work to do */
        styleTableEntry *styleRec;
        /* Set font, color, and gc depending on style.  For normal text, GCs
           for normal drawing, or drawing within a selection or highlight are
           pre-allocated and pre-configured.  For syntax highlighting, GCs are
           configured here, on the fly. */
        if (style & STYLE_LOOKUP_MASK) {
            styleRec = &textD->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A];
            underlineStyle = styleRec->underline;
            fs = styleRec->font;
            gcValues.font = fs->fid;
            fground = styleRec->color;
            /* here you could pick up specific select and highlight fground */
        }
        else {
            styleRec = NULL;
            gcValues.font = fs->fid;
            fground = textD->fgPixel;
        }
        /* Background color priority order is:
           1 Primary(Selection), 2 Highlight(Parens),
           3 Rangeset, 4 SyntaxHighlightStyle,
           5 Backlight (if NOT fill), 6 DefaultBackground */
        bground =
            style & PRIMARY_MASK   ? textD->selectBGPixel :
            style & HIGHLIGHT_MASK ? textD->highlightBGPixel :
            style & RANGESET_MASK  ?
                      getRangesetColor(textD,
                          (style&RANGESET_MASK)>>RANGESET_SHIFT,
                            bground) :
            styleRec && styleRec->bgColorName ? styleRec->bgColor :
            (style & BACKLIGHT_MASK) && !(style & FILL_MASK) ?
                      textD->bgClassPixel[(style>>BACKLIGHT_SHIFT) & 0xff] :
            textD->bgPixel;
        if (fground == bground) /* B&W kludge */
            fground = textD->bgPixel;
        /* set up gc for clearing using the foreground color entry */
        gcValues.foreground = gcValues.background = bground;
        XChangeGC(XtDisplay(textD->w), gc,
                GCFont | GCForeground | GCBackground, &gcValues);
    }

    /* Draw blank area rather than text, if that was the request */
    if (style & FILL_MASK) {
        /* wipes out to right hand edge of widget */
	if (toX >= textD->left)
	    clearRect(textD, bgGC, max(x, textD->left), y,
		    toX - max(x, textD->left), textD->ascent + textD->descent);
        return;
    }

    /* If any space around the character remains unfilled (due to use of
       different sized fonts for highlighting), fill in above or below
       to erase previously drawn characters */
    if (fs->ascent < textD->ascent)
    	clearRect(textD, bgGC, x, y, toX - x, textD->ascent - fs->ascent);
    if (fs->descent < textD->descent)
    	clearRect(textD, bgGC, x, y + textD->ascent + fs->descent, toX - x,
    		textD->descent - fs->descent);

    /* set up gc for writing text (set foreground properly) */
    if (bgGC == textD->styleGC) {
        gcValues.foreground = fground;
        XChangeGC(XtDisplay(textD->w), gc, GCForeground, &gcValues);
    }

    /* Draw the string using gc and font set above */
    XDrawImageString(XtDisplay(textD->w), XtWindow(textD->w), gc, x,
    	    y + textD->ascent, string, nChars);
    
    /* Underline if style is secondary selection */
    if (style & SECONDARY_MASK || underlineStyle)
    {
        /* restore foreground in GC (was set to background by clearRect()) */
        gcValues.foreground = fground;
        XChangeGC(XtDisplay(textD->w), gc,
                GCForeground, &gcValues);
        /* draw underline */
    	XDrawLine(XtDisplay(textD->w), XtWindow(textD->w), gc, x,
    	    	y + textD->ascent, toX - 1, y + textD->ascent);
    }
}

/*
** Clear a rectangle with the appropriate background color for "style"
*/
static void clearRect(textDisp *textD, GC gc, int x, int y, 
    	int width, int height)
{
    /* A width of zero means "clear to end of window" to XClearArea */
    if (width == 0 || XtWindow(textD->w) == 0)
    	return;
    
    if (gc == textD->gc) {
        XClearArea(XtDisplay(textD->w), XtWindow(textD->w), x, y,
                width, height, False);
    }
    else {
        XFillRectangle(XtDisplay(textD->w), XtWindow(textD->w),
                gc, x, y, width, height);
    }
}

/*
** Draw a cursor with top center at x, y.
*/
static void drawCursor(textDisp *textD, int x, int y)
{
    XSegment segs[5];
    int left, right, cursorWidth, midY;
    int fontWidth = textD->fontStruct->min_bounds.width, nSegs = 0;  
    int fontHeight = textD->ascent + textD->descent;
    int bot = y + fontHeight - 1;
    
    if (XtWindow(textD->w) == 0 || x < textD->left-1 ||
	    x > textD->left + textD->width)
    	return;
    
    /* For cursors other than the block, make them around 2/3 of a character
       width, rounded to an even number of pixels so that X will draw an
       odd number centered on the stem at x. */
    cursorWidth = (fontWidth/3) * 2;
    left = x - cursorWidth/2;
    right = left + cursorWidth;
    
    /* Create segments and draw cursor */
    if (textD->cursorStyle == CARET_CURSOR) {
    	midY = bot - fontHeight/5;
    	segs[0].x1 = left; segs[0].y1 = bot; segs[0].x2 = x; segs[0].y2 = midY;
    	segs[1].x1 = x; segs[1].y1 = midY; segs[1].x2 = right; segs[1].y2 = bot;
    	segs[2].x1 = left; segs[2].y1 = bot; segs[2].x2 = x; segs[2].y2=midY-1;
    	segs[3].x1 = x; segs[3].y1=midY-1; segs[3].x2 = right; segs[3].y2 = bot;
    	nSegs = 4;
    } else if (textD->cursorStyle == NORMAL_CURSOR) {
	segs[0].x1 = left; segs[0].y1 = y; segs[0].x2 = right; segs[0].y2 = y;
	segs[1].x1 = x; segs[1].y1 = y; segs[1].x2 = x; segs[1].y2 = bot;
	segs[2].x1 = left; segs[2].y1 = bot; segs[2].x2 = right; segs[2].y2=bot;
	nSegs = 3;
    } else if (textD->cursorStyle == HEAVY_CURSOR) {
	segs[0].x1 = x-1; segs[0].y1 = y; segs[0].x2 = x-1; segs[0].y2 = bot;
	segs[1].x1 = x; segs[1].y1 = y; segs[1].x2 = x; segs[1].y2 = bot;
	segs[2].x1 = x+1; segs[2].y1 = y; segs[2].x2 = x+1; segs[2].y2 = bot;
	segs[3].x1 = left; segs[3].y1 = y; segs[3].x2 = right; segs[3].y2 = y;
	segs[4].x1 = left; segs[4].y1 = bot; segs[4].x2 = right; segs[4].y2=bot;
	nSegs = 5;
    } else if (textD->cursorStyle == DIM_CURSOR) {
	midY = y + fontHeight/2;
	segs[0].x1 = x; segs[0].y1 = y; segs[0].x2 = x; segs[0].y2 = y;
	segs[1].x1 = x; segs[1].y1 = midY; segs[1].x2 = x; segs[1].y2 = midY;
	segs[2].x1 = x; segs[2].y1 = bot; segs[2].x2 = x; segs[2].y2 = bot;
	nSegs = 3;
    } else if (textD->cursorStyle == BLOCK_CURSOR) {
	right = x + fontWidth;
	segs[0].x1 = x; segs[0].y1 = y; segs[0].x2 = right; segs[0].y2 = y;
	segs[1].x1 = right; segs[1].y1 = y; segs[1].x2 = right; segs[1].y2=bot;
	segs[2].x1 = right; segs[2].y1 = bot; segs[2].x2 = x; segs[2].y2 = bot;
	segs[3].x1 = x; segs[3].y1 = bot; segs[3].x2 = x; segs[3].y2 = y;
	nSegs = 4;
    }
    XDrawSegments(XtDisplay(textD->w), XtWindow(textD->w),
    	    textD->cursorFGGC, segs, nSegs);
    
    /* Save the last position drawn */
    textD->cursorX = x;
    textD->cursorY = y;
}

/*
** Determine the drawing method to use to draw a specific character from "buf".
** "lineStartPos" gives the character index where the line begins, "lineIndex",
** the number of characters past the beginning of the line, and "dispIndex",
** the number of displayed characters past the beginning of the line.  Passing
** lineStartPos of -1 returns the drawing style for "no text".
**
** Why not just: styleOfPos(textD, pos)?  Because style applies to blank areas
** of the window beyond the text boundaries, and because this routine must also
** decide whether a position is inside of a rectangular selection, and do so
** efficiently, without re-counting character positions from the start of the
** line.
**
** Note that style is a somewhat incorrect name, drawing method would
** be more appropriate.
*/
static int styleOfPos(textDisp *textD, int lineStartPos,
    	int lineLen, int lineIndex, int dispIndex, int thisChar)
{
    textBuffer *buf = textD->buffer;
    textBuffer *styleBuf = textD->styleBuffer;
    int pos, style = 0;
    
    if (lineStartPos == -1 || buf == NULL)
    	return FILL_MASK;
    
    pos = lineStartPos + min(lineIndex, lineLen);
    
    if (lineIndex >= lineLen)
   	style = FILL_MASK;
    else if (styleBuf != NULL) {
    	style = (unsigned char)BufGetCharacter(styleBuf, pos);
    	if (style == textD->unfinishedStyle) {
    	    /* encountered "unfinished" style, trigger parsing */
    	    (textD->unfinishedHighlightCB)(textD, pos, textD->highlightCBArg);
    	    style = (unsigned char)BufGetCharacter(styleBuf, pos);
    	}
    }
    if (inSelection(&buf->primary, pos, lineStartPos, dispIndex))
    	style |= PRIMARY_MASK;
    if (inSelection(&buf->highlight, pos, lineStartPos, dispIndex))
    	style |= HIGHLIGHT_MASK;
    if (inSelection(&buf->secondary, pos, lineStartPos, dispIndex))
    	style |= SECONDARY_MASK;
    /* store in the RANGESET_MASK portion of style the rangeset index for pos */
    if (buf->rangesetTable) {
        int rangesetIndex = RangesetIndex1ofPos(buf->rangesetTable, pos, True);
        style |= ((rangesetIndex << RANGESET_SHIFT) & RANGESET_MASK);
    }
    /* store in the BACKLIGHT_MASK portion of style the background color class
       of the character thisChar */
    if (textD->bgClass)
    {
        style |= (textD->bgClass[(unsigned char)thisChar]<<BACKLIGHT_SHIFT);
    }
    return style;
}

/*
** Find the width of a string in the font of a particular style
*/
static int stringWidth(const textDisp* textD, const char *string,
        int length, int style)
{
    XFontStruct *fs;
    
    if (style & STYLE_LOOKUP_MASK)
    	fs = textD->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A].font;
    else 
    	fs = textD->fontStruct;
    return XTextWidth(fs, (char*) string, (int) length);
}

/*
** Return true if position "pos" with indentation "dispIndex" is in
** selection "sel"
*/
static int inSelection(selection *sel, int pos, int lineStartPos, int dispIndex)
{
    return sel->selected &&
    	 ((!sel->rectangular &&
    	   pos >= sel->start && pos < sel->end) ||
    	  (sel->rectangular &&
    	   pos >= sel->start && lineStartPos <= sel->end &&
     	   dispIndex >= sel->rectStart && dispIndex < sel->rectEnd));
}

/*
** Translate window coordinates to the nearest (insert cursor or character
** cell) text position.  The parameter posType specifies how to interpret the
** position: CURSOR_POS means translate the coordinates to the nearest cursor
** position, and CHARACTER_POS means return the position of the character
** closest to (x, y).
*/
static int xyToPos(textDisp *textD, int x, int y, int posType)
{
    int charIndex, lineStart, lineLen, fontHeight;
    int charWidth, charLen, charStyle, visLineNum, xStep, outIndex;
    char *lineStr, expandedChar[MAX_EXP_CHAR_LEN];

    /* Find the visible line number corresponding to the y coordinate */
    fontHeight = textD->ascent + textD->descent;
    visLineNum = (y - textD->top) / fontHeight;
    if (visLineNum < 0)
	return textD->firstChar;
    if (visLineNum >= textD->nVisibleLines)
	visLineNum = textD->nVisibleLines - 1;
    
    /* Find the position at the start of the line */
    lineStart = textD->lineStarts[visLineNum];
    
    /* If the line start was empty, return the last position in the buffer */
    if (lineStart == -1)
    	return textD->buffer->length;
    
    /* Get the line text and its length */
    lineLen = visLineLength(textD, visLineNum);
    lineStr = BufGetRange(textD->buffer, lineStart, lineStart + lineLen);
    
    /* Step through character positions from the beginning of the line
       to find the character position corresponding to the x coordinate */
    xStep = textD->left - textD->horizOffset;
    outIndex = 0;
    for(charIndex=0; charIndex<lineLen; charIndex++) {
    	charLen = BufExpandCharacter(lineStr[charIndex], outIndex, expandedChar,
    		textD->buffer->tabDist, textD->buffer->nullSubsChar);
   	charStyle = styleOfPos(textD, lineStart, lineLen, charIndex, outIndex,
				lineStr[charIndex]);
    	charWidth = stringWidth(textD, expandedChar, charLen, charStyle);
    	if (x < xStep + (posType == CURSOR_POS ? charWidth/2 : charWidth)) {
    	    NEditFree(lineStr);
    	    return lineStart + charIndex;
    	}
    	xStep += charWidth;
    	outIndex += charLen;
    }
    
    /* If the x position was beyond the end of the line, return the position
       of the newline at the end of the line */
    NEditFree(lineStr);
    return lineStart + lineLen;
}

/*
** Translate window coordinates to the nearest row and column number for
** positioning the cursor.  This, of course, makes no sense when the font is
** proportional, since there are no absolute columns.  The parameter posType
** specifies how to interpret the position: CURSOR_POS means translate the
** coordinates to the nearest position between characters, and CHARACTER_POS
** means translate the position to the nearest character cell.
*/
static void xyToUnconstrainedPos(textDisp *textD, int x, int y, int *row,
	int *column, int posType)
{
    int fontHeight = textD->ascent + textD->descent;
    int fontWidth = textD->fontStruct->max_bounds.width;

    /* Find the visible line number corresponding to the y coordinate */
    *row = (y - textD->top) / fontHeight;
    if (*row < 0) *row = 0;
    if (*row >= textD->nVisibleLines) *row = textD->nVisibleLines - 1;
    *column = ((x-textD->left) + textD->horizOffset +
    	    (posType == CURSOR_POS ? fontWidth/2 : 0)) / fontWidth;
    if (*column < 0) *column = 0;
}

/*
** Offset the line starts array, topLineNum, firstChar and lastChar, for a new
** vertical scroll position given by newTopLineNum.  If any currently displayed
** lines will still be visible, salvage the line starts values, otherwise,
** count lines from the nearest known line start (start or end of buffer, or
** the closest value in the lineStarts array)
*/
static void offsetLineStarts(textDisp *textD, int newTopLineNum)
{
    int oldTopLineNum = textD->topLineNum;
    int oldFirstChar = textD->firstChar;
    int lineDelta = newTopLineNum - oldTopLineNum;
    int nVisLines = textD->nVisibleLines;
    int *lineStarts = textD->lineStarts;
    int i, lastLineNum;
    textBuffer *buf = textD->buffer;
    
    /* If there was no offset, nothing needs to be changed */
    if (lineDelta == 0)
    	return;
    	
    /* {   int i;
    	printf("Scroll, lineDelta %d\n", lineDelta);
    	printf("lineStarts Before: ");
    	for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	printf("\n");
    } */
    
    /* Find the new value for firstChar by counting lines from the nearest
       known line start (start or end of buffer, or the closest value in the
       lineStarts array) */
    lastLineNum = oldTopLineNum + nVisLines - 1;
    if (newTopLineNum < oldTopLineNum && newTopLineNum < -lineDelta) {
    	textD->firstChar = TextDCountForwardNLines(textD, 0, newTopLineNum-1,
    	    	True);
    	/* printf("counting forward %d lines from start\n", newTopLineNum-1);*/
    } else if (newTopLineNum < oldTopLineNum) {
    	textD->firstChar = TextDCountBackwardNLines(textD, textD->firstChar,
    		-lineDelta);
    	/* printf("counting backward %d lines from firstChar\n", -lineDelta);*/
    } else if (newTopLineNum < lastLineNum) {
    	textD->firstChar = lineStarts[newTopLineNum - oldTopLineNum];
    	/* printf("taking new start from lineStarts[%d]\n",
    		newTopLineNum - oldTopLineNum); */
    } else if (newTopLineNum-lastLineNum < textD->nBufferLines-newTopLineNum) {
    	textD->firstChar = TextDCountForwardNLines(textD, 
                lineStarts[nVisLines-1], newTopLineNum - lastLineNum, True);
    	/* printf("counting forward %d lines from start of last line\n",
    		newTopLineNum - lastLineNum); */
    } else {
    	textD->firstChar = TextDCountBackwardNLines(textD, buf->length,
		textD->nBufferLines - newTopLineNum + 1);
	/* printf("counting backward %d lines from end\n",
    		textD->nBufferLines - newTopLineNum + 1); */
    }
    
    /* Fill in the line starts array */
    if (lineDelta < 0 && -lineDelta < nVisLines) {
    	for (i=nVisLines-1; i >= -lineDelta; i--)
    	    lineStarts[i] = lineStarts[i+lineDelta];
    	calcLineStarts(textD, 0, -lineDelta);
    } else if (lineDelta > 0 && lineDelta < nVisLines) {
    	for (i=0; i<nVisLines-lineDelta; i++)
    	    lineStarts[i] = lineStarts[i+lineDelta];
    	calcLineStarts(textD, nVisLines-lineDelta, nVisLines-1);
    } else
	calcLineStarts(textD, 0, nVisLines);
    
    /* Set lastChar and topLineNum */
    calcLastChar(textD);
    textD->topLineNum = newTopLineNum;
    
    /* If we're numbering lines or being asked to maintain an absolute line
       number, re-calculate the absolute line number */
    offsetAbsLineNum(textD, oldFirstChar);
    
    /* {   int i;
    	printf("lineStarts After: ");
    	for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	printf("\n");
    } */
}

/*
** Update the line starts array, topLineNum, firstChar and lastChar for text
** display "textD" after a modification to the text buffer, given by the
** position where the change began "pos", and the nmubers of characters
** and lines inserted and deleted.
*/
static void updateLineStarts(textDisp *textD, int pos, int charsInserted,
	int charsDeleted, int linesInserted, int linesDeleted, int *scrolled)
{
    int *lineStarts = textD->lineStarts;
    int i, lineOfPos, lineOfEnd, nVisLines = textD->nVisibleLines;
    int charDelta = charsInserted - charsDeleted;
    int lineDelta = linesInserted - linesDeleted;

    /* {   int i;
    	printf("linesDeleted %d, linesInserted %d, charsInserted %d, charsDeleted %d\n",
    	    	linesDeleted, linesInserted, charsInserted, charsDeleted);
    	printf("lineStarts Before: ");
    	for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	printf("\n");
    } */
    /* If all of the changes were before the displayed text, the display
       doesn't change, just update the top line num and offset the line
       start entries and first and last characters */
    if (pos + charsDeleted < textD->firstChar) {
    	textD->topLineNum += lineDelta;
    	for (i=0; i<nVisLines && lineStarts[i] != -1; i++)
    	    lineStarts[i] += charDelta;
    	/* {   int i;
    	    printf("lineStarts after delete doesn't touch: ");
    	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	    printf("\n");
    	} */
    	textD->firstChar += charDelta;
    	textD->lastChar += charDelta;
    	*scrolled = False;
    	return;
    }
    
    /* The change began before the beginning of the displayed text, but
       part or all of the displayed text was deleted */
    if (pos < textD->firstChar) {
    	/* If some text remains in the window, anchor on that  */
    	if (posToVisibleLineNum(textD, pos + charsDeleted, &lineOfEnd) &&
    		++lineOfEnd < nVisLines && lineStarts[lineOfEnd] != -1) {
    	    textD->topLineNum = max(1, textD->topLineNum + lineDelta);
    	    textD->firstChar = TextDCountBackwardNLines(textD,
    	    	    lineStarts[lineOfEnd] + charDelta, lineOfEnd);
    	/* Otherwise anchor on original line number and recount everything */
    	} else {
    	    if (textD->topLineNum > textD->nBufferLines + lineDelta) {
    	    	textD->topLineNum = 1;
    	    	textD->firstChar = 0;
    	    } else
    		textD->firstChar = TextDCountForwardNLines(textD, 0,
    	    		textD->topLineNum - 1, True);
    	}
    	calcLineStarts(textD, 0, nVisLines-1);
    	/* {   int i;
    	    printf("lineStarts after delete encroaches: ");
    	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	    printf("\n");
    	} */
    	/* calculate lastChar by finding the end of the last displayed line */
    	calcLastChar(textD);
    	*scrolled = True;
    	return;
    }
    
    /* If the change was in the middle of the displayed text (it usually is),
       salvage as much of the line starts array as possible by moving and
       offsetting the entries after the changed area, and re-counting the
       added lines or the lines beyond the salvaged part of the line starts
       array */
    if (pos <= textD->lastChar) {
    	/* find line on which the change began */
    	posToVisibleLineNum(textD, pos, &lineOfPos);
    	/* salvage line starts after the changed area */
    	if (lineDelta == 0) {
    	    for (i=lineOfPos+1; i<nVisLines && lineStarts[i]!= -1; i++)
    		lineStarts[i] += charDelta;
    	} else if (lineDelta > 0) {
    	    for (i=nVisLines-1; i>=lineOfPos+lineDelta+1; i--)
    		lineStarts[i] = lineStarts[i-lineDelta] +
    			(lineStarts[i-lineDelta] == -1 ? 0 : charDelta);
    	} else /* (lineDelta < 0) */ {
    	    for (i=max(0,lineOfPos+1); i<nVisLines+lineDelta; i++)
    	    	lineStarts[i] = lineStarts[i-lineDelta] +
    	    		(lineStarts[i-lineDelta] == -1 ? 0 : charDelta);
    	}
    	/* {   int i;
    	    printf("lineStarts after salvage: ");
    	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	    printf("\n");
    	} */
    	/* fill in the missing line starts */
    	if (linesInserted >= 0)
    	    calcLineStarts(textD, lineOfPos + 1, lineOfPos + linesInserted);
    	if (lineDelta < 0)
    	    calcLineStarts(textD, nVisLines+lineDelta, nVisLines);
    	/* {   int i;
    	    printf("lineStarts after recalculation: ");
    	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	    printf("\n");
    	} */
    	/* calculate lastChar by finding the end of the last displayed line */
    	calcLastChar(textD);
    	*scrolled = False;
    	return;
    }
    
    /* Change was past the end of the displayed text, but displayable by virtue
       of being an insert at the end of the buffer into visible blank lines */
    if (emptyLinesVisible(textD)) {
    	posToVisibleLineNum(textD, pos, &lineOfPos);
    	calcLineStarts(textD, lineOfPos, lineOfPos+linesInserted);
    	calcLastChar(textD);
    	/* {   int i;
    	    printf("lineStarts after insert at end: ");
    	    for(i=0; i<nVisLines; i++) printf("%d ", lineStarts[i]);
    	    printf("\n");
    	} */
    	*scrolled = False;
    	return;
    }
    
    /* Change was beyond the end of the buffer and not visible, do nothing */
    *scrolled = False;
}

/*
** Scan through the text in the "textD"'s buffer and recalculate the line
** starts array values beginning at index "startLine" and continuing through
** (including) "endLine".  It assumes that the line starts entry preceding
** "startLine" (or textD->firstChar if startLine is 0) is good, and re-counts
** newlines to fill in the requested entries.  Out of range values for
** "startLine" and "endLine" are acceptable.
*/
static void calcLineStarts(textDisp *textD, int startLine, int endLine)
{
    int startPos, bufLen = textD->buffer->length;
    int line, lineEnd, nextLineStart, nVis = textD->nVisibleLines;
    int *lineStarts = textD->lineStarts;
    
    /* Clean up (possibly) messy input parameters */
    if (nVis == 0) return;
    if (endLine < 0) endLine = 0;
    if (endLine >= nVis) endLine = nVis - 1;
    if (startLine < 0) startLine = 0;
    if (startLine >=nVis) startLine = nVis - 1;
    if (startLine > endLine)
    	return;
    
    /* Find the last known good line number -> position mapping */
    if (startLine == 0) {
    	lineStarts[0] = textD->firstChar;
    	startLine = 1;
    }
    startPos = lineStarts[startLine-1];
    
    /* If the starting position is already past the end of the text,
       fill in -1's (means no text on line) and return */
    if (startPos == -1) {
        for (line=startLine; line<=endLine; line++)
    	    lineStarts[line] = -1;
    	return;
    }
    
    /* Loop searching for ends of lines and storing the positions of the
       start of the next line in lineStarts */
    for (line=startLine; line<=endLine; line++) {
    	findLineEnd(textD, startPos, True, &lineEnd, &nextLineStart);
    	startPos = nextLineStart;
    	if (startPos >= bufLen) {
    	    /* If the buffer ends with a newline or line break, put
    	       buf->length in the next line start position (instead of
    	       a -1 which is the normal marker for an empty line) to
    	       indicate that the cursor may safely be displayed there */
    	    if (line == 0 || (lineStarts[line-1] != bufLen &&
    	    	    lineEnd != nextLineStart)) {
    	    	lineStarts[line] = bufLen;
    	    	line++;
    	    }
    	    break;
    	}
    	lineStarts[line] = startPos;
    }
    
    /* Set any entries beyond the end of the text to -1 */
    for (; line<=endLine; line++)
    	lineStarts[line] = -1;
}

/* 
** Given a textDisp with a complete, up-to-date lineStarts array, update
** the lastChar entry to point to the last buffer position displayed.
*/
static void calcLastChar(textDisp *textD)
{
    int i;
    
    for (i=textD->nVisibleLines-1; i>0 && textD->lineStarts[i]== -1; i--);
    textD->lastChar = i < 0 ? 0 :
    	    TextDEndOfLine(textD, textD->lineStarts[i], True);
}

void TextDImposeGraphicsExposeTranslation(textDisp *textD, int *xOffset, int *yOffset)
{
    if (textD->graphicsExposeQueue) {
        graphicExposeTranslationEntry *thisGEQEntry = textD->graphicsExposeQueue->next;
        if (thisGEQEntry) {
            *xOffset += thisGEQEntry->horizontal;
            *yOffset += thisGEQEntry->vertical;
        }
    }
}

Boolean TextDPopGraphicExposeQueueEntry(textDisp *textD)
{
    graphicExposeTranslationEntry *removedGEQEntry = textD->graphicsExposeQueue;

    if (removedGEQEntry) {
        textD->graphicsExposeQueue = removedGEQEntry->next;
        NEditFree(removedGEQEntry);
    }
    return(removedGEQEntry?True:False);
}

void TextDTranlateGraphicExposeQueue(textDisp *textD, int xOffset, int yOffset, Boolean appendEntry)
{
    graphicExposeTranslationEntry *newGEQEntry = NULL;
    if (appendEntry) {
        newGEQEntry = (graphicExposeTranslationEntry *)NEditMalloc(sizeof(graphicExposeTranslationEntry));
        newGEQEntry->next = NULL;
        newGEQEntry->horizontal = xOffset;
        newGEQEntry->vertical = yOffset;
    }
    if (textD->graphicsExposeQueue) {
        graphicExposeTranslationEntry *iter = textD->graphicsExposeQueue;
        while (iter->next) {
            iter->next->horizontal += xOffset;
            iter->next->vertical += yOffset;
            iter = iter->next;
        }
        if (appendEntry) {
            iter->next = (struct graphicExposeTranslationEntry *)newGEQEntry;
        }
    }
    else {
        if (appendEntry) {
            textD->graphicsExposeQueue = newGEQEntry;
        }
    }
}

static void setScroll(textDisp *textD, int topLineNum, int horizOffset,
        int updateVScrollBar, int updateHScrollBar)
{
    int fontHeight = textD->ascent + textD->descent;
    int origHOffset = textD->horizOffset;
    int lineDelta = textD->topLineNum - topLineNum;
    int xOffset, yOffset, srcX, srcY, dstX, dstY, width, height;
    int exactHeight = textD->height - textD->height %
            (textD->ascent + textD->descent);
    
    /* Do nothing if scroll position hasn't actually changed or there's no
       window to draw in yet */
    if (XtWindow(textD->w) == 0 ||  (textD->horizOffset == horizOffset &&
            textD->topLineNum == topLineNum))
        return;
    
    /* If part of the cursor is protruding beyond the text clipping region,
       clear it off */
    blankCursorProtrusions(textD);

    /* If the vertical scroll position has changed, update the line
       starts array and related counters in the text display */
    offsetLineStarts(textD, topLineNum);
    
    /* Just setting textD->horizOffset is enough information for redisplay */
    textD->horizOffset = horizOffset;
    
    /* Update the scroll bar positions if requested, note: updating the
       horizontal scroll bars can have the further side-effect of changing
       the horizontal scroll position, textD->horizOffset */
    if (updateVScrollBar && textD->vScrollBar != NULL) {
        updateVScrollBarRange(textD);
    }
    if (updateHScrollBar && textD->hScrollBar != NULL) {
        updateHScrollBarRange(textD);
    }
    
    /* Redisplay everything if the window is partially obscured (since
       it's too hard to tell what displayed areas are salvageable) or
       if there's nothing to recover because the scroll distance is large */
    xOffset = origHOffset - textD->horizOffset;
    yOffset = lineDelta * fontHeight;
    if (textD->visibility != VisibilityUnobscured ||
            abs(xOffset) > textD->width || abs(yOffset) > exactHeight) {
        TextDTranlateGraphicExposeQueue(textD, xOffset, yOffset, False);
        TextDRedisplayRect(textD, textD->left, textD->top, textD->width,
                textD->height);
    } else {
        /* If the window is not obscured, paint most of the window using XCopyArea
           from existing displayed text, and redraw only what's necessary */
        /* Recover the useable window areas by moving to the proper location */
        srcX = textD->left + (xOffset >= 0 ? 0 : -xOffset);
        dstX = textD->left + (xOffset >= 0 ? xOffset : 0);
        width = textD->width - abs(xOffset);
        srcY = textD->top + (yOffset >= 0 ? 0 : -yOffset);
        dstY = textD->top + (yOffset >= 0 ? yOffset : 0);
        height = exactHeight - abs(yOffset);
        resetClipRectangles(textD);
        TextDTranlateGraphicExposeQueue(textD, xOffset, yOffset, True);
        XCopyArea(XtDisplay(textD->w), XtWindow(textD->w), XtWindow(textD->w),
                textD->gc, srcX, srcY, width, height, dstX, dstY);
        /* redraw the un-recoverable parts */
        if (yOffset > 0) {
            TextDRedisplayRect(textD, textD->left, textD->top,
                    textD->width, yOffset);
        }
        else if (yOffset < 0) {
            TextDRedisplayRect(textD, textD->left, textD->top +
                    textD->height + yOffset, textD->width, -yOffset);
        }
        if (xOffset > 0) {
            TextDRedisplayRect(textD, textD->left, textD->top,
                    xOffset, textD->height);
        }
        else if (xOffset < 0) {
            TextDRedisplayRect(textD, textD->left + textD->width + xOffset,
                    textD->top, -xOffset, textD->height);
        }
        /* Restore protruding parts of the cursor */
        textDRedisplayRange(textD, textD->cursorPos-1, textD->cursorPos+1);
    }
    
    /* Refresh line number/calltip display if its up and we've scrolled 
        vertically */
    if (lineDelta != 0) {
        redrawLineNumbers(textD, False);
        TextDRedrawCalltip(textD, 0);
    }

    HandleAllPendingGraphicsExposeNoExposeEvents((TextWidget)textD->w, NULL);
}

/*
** Update the minimum, maximum, slider size, page increment, and value
** for vertical scroll bar.
*/
static void updateVScrollBarRange(textDisp *textD)
{
    int sliderSize, sliderMax, sliderValue;
    
    if (textD->vScrollBar == NULL)
        return;
    
    /* The Vert. scroll bar value and slider size directly represent the top
       line number, and the number of visible lines respectively.  The scroll
       bar maximum value is chosen to generally represent the size of the whole
       buffer, with minor adjustments to keep the scroll bar widget happy */
    sliderSize = max(textD->nVisibleLines, 1); /* Avoid X warning (size < 1) */
    sliderValue = textD->topLineNum;
    sliderMax = max(textD->nBufferLines + 2 + 
                    TEXT_OF_TEXTD(textD).cursorVPadding, 
                    sliderSize + sliderValue);
    XtVaSetValues(textD->vScrollBar,
            XmNmaximum, sliderMax,
            XmNsliderSize, sliderSize,
            XmNpageIncrement, max(1, textD->nVisibleLines - 1),
            XmNvalue, sliderValue, NULL);
}

/*
** Update the minimum, maximum, slider size, page increment, and value
** for the horizontal scroll bar.  If scroll position is such that there
** is blank space to the right of all lines of text, scroll back (adjust
** horizOffset but don't redraw) to take up the slack and position the
** right edge of the text at the right edge of the display.
** 
** Note, there is some cost to this routine, since it scans the whole range
** of displayed text, particularly since it's usually called for each typed
** character!
*/
static int updateHScrollBarRange(textDisp *textD)
{
    int i, maxWidth = 0, sliderMax, sliderWidth;
    int origHOffset = textD->horizOffset;
    
    if (textD->hScrollBar == NULL || !XtIsManaged(textD->hScrollBar))
    	return False;
    
    /* Scan all the displayed lines to find the width of the longest line */
    for (i=0; i<textD->nVisibleLines && textD->lineStarts[i]!= -1; i++)
    	maxWidth = max(measureVisLine(textD, i), maxWidth);
    
    /* If the scroll position is beyond what's necessary to keep all lines
       in view, scroll to the left to bring the end of the longest line to
       the right margin */
    if (maxWidth < textD->width + textD->horizOffset && textD->horizOffset > 0)
    	textD->horizOffset = max(0, maxWidth - textD->width);
    
    /* Readjust the scroll bar */
    sliderWidth = textD->width;
    sliderMax = max(maxWidth, sliderWidth + textD->horizOffset);
    XtVaSetValues(textD->hScrollBar,
    	    XmNmaximum, sliderMax,
    	    XmNsliderSize, sliderWidth,
    	    XmNpageIncrement, max(textD->width - 100, 10),
    	    XmNvalue, textD->horizOffset, NULL);
    
    /* Return True if scroll position was changed */
    return origHOffset != textD->horizOffset;
}

/*
** Define area for drawing line numbers.  A width of 0 disables line
** number drawing.
*/
void TextDSetLineNumberArea(textDisp *textD, int lineNumLeft, int lineNumWidth,
	int textLeft)
{
    int newWidth = textD->width + textD->left - textLeft;
    textD->lineNumLeft = lineNumLeft;
    textD->lineNumWidth = lineNumWidth;
    textD->left = textLeft;
    XClearWindow(XtDisplay(textD->w), XtWindow(textD->w));
    resetAbsLineNum(textD);
    TextDResize(textD, newWidth, textD->height);
    TextDRedisplayRect(textD, 0, textD->top, INT_MAX, textD->height);
}

/*
** Refresh the line number area.  If clearAll is False, writes only over
** the character cell areas.  Setting clearAll to True will clear out any
** stray marks outside of the character cell area, which might have been
** left from before a resize or font change.
*/
static void redrawLineNumbers(textDisp *textD, int clearAll)
{
    int y, line, visLine, nCols, lineStart;
    char lineNumString[12];
    int lineHeight = textD->ascent + textD->descent;
    int charWidth = textD->fontStruct->max_bounds.width;
    XRectangle clipRect;
    Display *display = XtDisplay(textD->w);
    
    /* Don't draw if lineNumWidth == 0 (line numbers are hidden), or widget is
       not yet realized */
    if (textD->lineNumWidth == 0 || XtWindow(textD->w) == 0)
        return;
    
    /* Make sure we reset the clipping range for the line numbers GC, because
       the GC may be shared (eg, if the line numbers and text have the same
       color) and therefore the clipping ranges may be invalid. */
    clipRect.x = textD->lineNumLeft;
    clipRect.y = textD->top;
    clipRect.width = textD->lineNumWidth;
    clipRect.height = textD->height;
    XSetClipRectangles(display, textD->lineNumGC, 0, 0,
    	    &clipRect, 1, Unsorted);
    
    /* Erase the previous contents of the line number area, if requested */
    if (clearAll)
        XClearArea(XtDisplay(textD->w), XtWindow(textD->w), textD->lineNumLeft,
                textD->top, textD->lineNumWidth, textD->height, False);
    
    /* Draw the line numbers, aligned to the text */
    nCols = min(11, textD->lineNumWidth / charWidth);
    y = textD->top;
    line = getAbsTopLineNum(textD);
    for (visLine=0; visLine < textD->nVisibleLines; visLine++) {
        lineStart = textD->lineStarts[visLine];
        if (lineStart != -1 && (lineStart==0 ||
                BufGetCharacter(textD->buffer, lineStart-1)=='\n')) {
            sprintf(lineNumString, "%*d", nCols, line);
            XDrawImageString(XtDisplay(textD->w), XtWindow(textD->w),
                    textD->lineNumGC, textD->lineNumLeft, y + textD->ascent,
                    lineNumString, strlen(lineNumString));
            line++;
        } else {
            XClearArea(XtDisplay(textD->w), XtWindow(textD->w),
                    textD->lineNumLeft, y, textD->lineNumWidth,
                    textD->ascent + textD->descent, False);
            if (visLine == 0)
                line++;
        }
        y += lineHeight;
    }
}

/*
** Callbacks for drag or valueChanged on scroll bars
*/
static void vScrollCB(Widget w, XtPointer clientData, XtPointer callData)
{
    textDisp *textD = (textDisp *)clientData;
    int newValue = ((XmScrollBarCallbackStruct *)callData)->value;
    int lineDelta = newValue - textD->topLineNum;
    
    if (lineDelta == 0)
        return;
    setScroll(textD, newValue, textD->horizOffset, False, True);
}
static void hScrollCB(Widget w, XtPointer clientData, XtPointer callData)
{
    textDisp *textD = (textDisp *)clientData;
    int newValue = ((XmScrollBarCallbackStruct *)callData)->value;
    
    if (newValue == textD->horizOffset)
        return;
    setScroll(textD, textD->topLineNum, newValue, False, False);
}

static void visibilityEH(Widget w, XtPointer data, XEvent *event,
        Boolean *continueDispatch)
{
    /* Record whether the window is fully visible or not.  This information
       is used for choosing the scrolling methodology for optimal performance,
       if the window is partially obscured, XCopyArea may not work */
    ((textDisp *)data)->visibility = ((XVisibilityEvent *)event)->state;
}

static int max(int i1, int i2)
{
    return i1 >= i2 ? i1 : i2;
}

static int min(int i1, int i2)
{
    return i1 <= i2 ? i1 : i2;
}

/*
** Count the number of newlines in a null-terminated text string;
*/
static int countLines(const char *string)
{
    const char *c;
    int lineCount = 0;
    
    if (string == NULL)
	return 0;
    for (c=string; *c!='\0'; c++)
    	if (*c == '\n') lineCount++;
    return lineCount;
}

/*
** Return the width in pixels of the displayed line pointed to by "visLineNum"
*/
static int measureVisLine(textDisp *textD, int visLineNum)
{
    int i, width = 0, len, style, lineLen = visLineLength(textD, visLineNum);
    int charCount = 0, lineStartPos = textD->lineStarts[visLineNum];
    char expandedChar[MAX_EXP_CHAR_LEN];
    
    if (textD->styleBuffer == NULL) {
	for (i=0; i<lineLen; i++) {
    	    len = BufGetExpandedChar(textD->buffer, lineStartPos + i,
    		    charCount, expandedChar);
    	    width += XTextWidth(textD->fontStruct, expandedChar, len);
    	    charCount += len;
	}
    } else {
    	for (i=0; i<lineLen; i++) {
    	    len = BufGetExpandedChar(textD->buffer, lineStartPos+i,
    		    charCount, expandedChar);
    	    style = (unsigned char)BufGetCharacter(textD->styleBuffer,
		    lineStartPos+i) - ASCII_A;
    	    width += XTextWidth(textD->styleTable[style].font, expandedChar,
    	    	    len);
    	    charCount += len;
	}
    }
    return width;
}

/*
** Return true if there are lines visible with no corresponding buffer text
*/
static int emptyLinesVisible(textDisp *textD)
{
    return textD->nVisibleLines > 0 &&
    	    textD->lineStarts[textD->nVisibleLines-1] == -1;
}

/*
** When the cursor is at the left or right edge of the text, part of it
** sticks off into the clipped region beyond the text.  Normal redrawing
** can not overwrite this protruding part of the cursor, so it must be
** erased independently by calling this routine.
*/
static void blankCursorProtrusions(textDisp *textD)
{
    int x, width, cursorX = textD->cursorX, cursorY = textD->cursorY;
    int fontWidth = textD->fontStruct->max_bounds.width;  
    int fontHeight = textD->ascent + textD->descent;
    int cursorWidth, left = textD->left, right = left + textD->width;
    
    cursorWidth = (fontWidth/3) * 2;
    if (cursorX >= left-1 && cursorX <= left + cursorWidth/2 - 1) {
        x = cursorX - cursorWidth/2;
        width = left - x;
    } else if (cursorX >= right - cursorWidth/2 && cursorX <= right) {
        x = right;
        width = cursorX + cursorWidth/2 + 2 - right;
    } else
        return;
        
    XClearArea(XtDisplay(textD->w), XtWindow(textD->w), x, cursorY,
            width, fontHeight, False);
}

/*
** Allocate shared graphics contexts used by the widget, which must be
** re-allocated on a font change.
*/
static void allocateFixedFontGCs(textDisp *textD, XFontStruct *fontStruct,
        Pixel bgPixel, Pixel fgPixel, Pixel selectFGPixel, Pixel selectBGPixel,
        Pixel highlightFGPixel, Pixel highlightBGPixel, Pixel lineNumFGPixel)
{
    textD->gc = allocateGC(textD->w, GCFont | GCForeground | GCBackground,
            fgPixel, bgPixel, fontStruct->fid, GCClipMask, GCArcMode); 
    textD->selectGC = allocateGC(textD->w, GCFont | GCForeground | GCBackground,
            selectFGPixel, selectBGPixel, fontStruct->fid, GCClipMask,
            GCArcMode);
    textD->selectBGGC = allocateGC(textD->w, GCForeground, selectBGPixel, 0,
            fontStruct->fid, GCClipMask, GCArcMode);
    textD->highlightGC = allocateGC(textD->w, GCFont|GCForeground|GCBackground,
            highlightFGPixel, highlightBGPixel, fontStruct->fid, GCClipMask,
            GCArcMode);
    textD->highlightBGGC = allocateGC(textD->w, GCForeground, highlightBGPixel,
            0, fontStruct->fid, GCClipMask, GCArcMode);
    textD->lineNumGC = allocateGC(textD->w, GCFont | GCForeground | 
            GCBackground, lineNumFGPixel, bgPixel, fontStruct->fid, 
            GCClipMask, GCArcMode);
}

/*
** X11R4 does not have the XtAllocateGC function for sharing graphics contexts
** with changeable fields.  Unfortunately the R4 call for creating shared
** graphics contexts (XtGetGC) is rarely useful because most widgets need
** to be able to set and change clipping, and that makes the GC unshareable.
**
** This function allocates and returns a gc, using XtAllocateGC if possible,
** or XCreateGC on X11R4 systems where XtAllocateGC is not available.
*/
static GC allocateGC(Widget w, unsigned long valueMask,
	unsigned long foreground, unsigned long background, Font font,
	unsigned long dynamicMask, unsigned long dontCareMask)
{
    XGCValues gcValues;

    gcValues.font = font;
    gcValues.background = background;
    gcValues.foreground = foreground;
#if defined(XlibSpecificationRelease) && XlibSpecificationRelease > 4
    return XtAllocateGC(w, 0, valueMask, &gcValues, dynamicMask,
    	    dontCareMask);
#else
    return XCreateGC(XtDisplay(w), RootWindowOfScreen(XtScreen(w)),
    	    valueMask, &gcValues);
#endif
}

/*
** Release a gc allocated with allocateGC above
*/
static void releaseGC(Widget w, GC gc)
{
#if defined(XlibSpecificationRelease) && XlibSpecificationRelease > 4
    XtReleaseGC(w, gc);
#else
    XFreeGC(XtDisplay(w), gc);
#endif
}

/*
** resetClipRectangles sets the clipping rectangles for GCs which clip
** at the text boundary (as opposed to the window boundary).  These GCs
** are shared such that the drawing styles are constant, but the clipping
** rectangles are allowed to change among different users of the GCs (the
** GCs were created with XtAllocGC).  This routine resets them so the clipping
** rectangles are correct for this text display.
*/
static void resetClipRectangles(textDisp *textD)
{
    XRectangle clipRect;
    Display *display = XtDisplay(textD->w);
    
    clipRect.x = textD->left;
    clipRect.y = textD->top;
    clipRect.width = textD->width;
    clipRect.height = textD->height - textD->height %
    	    (textD->ascent + textD->descent);
    
    XSetClipRectangles(display, textD->gc, 0, 0,
    	    &clipRect, 1, Unsorted);
    XSetClipRectangles(display, textD->selectGC, 0, 0,
            &clipRect, 1, Unsorted);
    XSetClipRectangles(display, textD->highlightGC, 0, 0,
            &clipRect, 1, Unsorted);
    XSetClipRectangles(display, textD->selectBGGC, 0, 0,
            &clipRect, 1, Unsorted);
    XSetClipRectangles(display, textD->highlightBGGC, 0, 0,
            &clipRect, 1, Unsorted);
    XSetClipRectangles(display, textD->styleGC, 0, 0,
            &clipRect, 1, Unsorted);
} 

/*
** Return the length of a line (number of displayable characters) by examining
** entries in the line starts array rather than by scanning for newlines
*/
static int visLineLength(textDisp *textD, int visLineNum)
{
    int nextLineStart, lineStartPos = textD->lineStarts[visLineNum];
    
    if (lineStartPos == -1)
    	return 0;
    if (visLineNum+1 >= textD->nVisibleLines)
    	return textD->lastChar - lineStartPos;
    nextLineStart = textD->lineStarts[visLineNum+1];
    if (nextLineStart == -1)
	return textD->lastChar - lineStartPos;
    if (wrapUsesCharacter(textD, nextLineStart-1))
    	return nextLineStart-1 - lineStartPos;
    return nextLineStart - lineStartPos;
}

/*
** When continuous wrap is on, and the user inserts or deletes characters,
** wrapping can happen before and beyond the changed position.  This routine
** finds the extent of the changes, and counts the deleted and inserted lines
** over that range.  It also attempts to minimize the size of the range to
** what has to be counted and re-displayed, so the results can be useful
** both for delimiting where the line starts need to be recalculated, and
** for deciding what part of the text to redisplay.
*/
static void findWrapRange(textDisp *textD, const char *deletedText, int pos,
    	int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd,
    	int *linesInserted, int *linesDeleted)
{
    int length, retPos, retLines, retLineStart, retLineEnd;
    textBuffer *deletedTextBuf, *buf = textD->buffer;
    int nVisLines = textD->nVisibleLines;
    int *lineStarts = textD->lineStarts;
    int countFrom, countTo, lineStart, adjLineStart, i;
    int visLineNum = 0, nLines = 0;
    
    /*
    ** Determine where to begin searching: either the previous newline, or
    ** if possible, limit to the start of the (original) previous displayed
    ** line, using information from the existing line starts array
    */
    if (pos >= textD->firstChar && pos <= textD->lastChar) {
    	for (i=nVisLines-1; i>0; i--)
    	    if (lineStarts[i] != -1 && pos >= lineStarts[i])
    		break;
    	if (i > 0) {
    	    countFrom = lineStarts[i-1];
    	    visLineNum = i-1;
    	} else
    	    countFrom = BufStartOfLine(buf, pos);
    } else
    	countFrom = BufStartOfLine(buf, pos);

    
    /*
    ** Move forward through the (new) text one line at a time, counting
    ** displayed lines, and looking for either a real newline, or for the
    ** line starts to re-sync with the original line starts array
    */
    lineStart = countFrom;
    *modRangeStart = countFrom;
    while (True) {
    	
    	/* advance to the next line.  If the line ended in a real newline
    	   or the end of the buffer, that's far enough */
    	wrappedLineCounter(textD, buf, lineStart, buf->length, 1, True, 0,
    	    	&retPos, &retLines, &retLineStart, &retLineEnd);
    	if (retPos >= buf->length) {
    	    countTo = buf->length;
    	    *modRangeEnd = countTo;
    	    if (retPos != retLineEnd)
    	    	nLines++;
    	    break;
    	} else
    	    lineStart = retPos;
    	nLines++;
    	if (lineStart > pos + nInserted &&
    	    	BufGetCharacter(buf, lineStart-1) == '\n') {
    	    countTo = lineStart;
    	    *modRangeEnd = lineStart;
    	    break;
    	}
        
	/* Don't try to resync in continuous wrap mode with non-fixed font
	   sizes; it would result in a chicken-and-egg dependency between
	   the calculations for the inserted and the deleted lines. 
           If we're in that mode, the number of deleted lines is calculated in
           advance, without resynchronization, so we shouldn't resynchronize
           for the inserted lines either. */
	if (textD->suppressResync)
	    continue;
    	
    	/* check for synchronization with the original line starts array
    	   before pos, if so, the modified range can begin later */
     	if (lineStart <= pos) {
    	    while (visLineNum<nVisLines && lineStarts[visLineNum] < lineStart)
    		visLineNum++;
     	    if (visLineNum < nVisLines && lineStarts[visLineNum] == lineStart) {
    		countFrom = lineStart;
    		nLines = 0;
    		if (visLineNum+1 < nVisLines && lineStarts[visLineNum+1] != -1)
    		    *modRangeStart = min(pos, lineStarts[visLineNum+1]-1);
    		else
    		    *modRangeStart = countFrom;
    	    } else
    	    	*modRangeStart = min(*modRangeStart, lineStart-1);
    	}
    	
   	/* check for synchronization with the original line starts array
    	   after pos, if so, the modified range can end early */
    	else if (lineStart > pos + nInserted) {
    	    adjLineStart = lineStart - nInserted + nDeleted;
    	    while (visLineNum<nVisLines && lineStarts[visLineNum]<adjLineStart)
    	    	visLineNum++;
    	    if (visLineNum < nVisLines && lineStarts[visLineNum] != -1 &&
    	    	    lineStarts[visLineNum] == adjLineStart) {
    	    	countTo = TextDEndOfLine(textD, lineStart, True);
    	    	*modRangeEnd = lineStart;
    	    	break;
    	    }
    	}
    }
    *linesInserted = nLines;


    /* Count deleted lines between countFrom and countTo as the text existed
       before the modification (that is, as if the text between pos and
       pos+nInserted were replaced by "deletedText").  This extra context is
       necessary because wrapping can occur outside of the modified region
       as a result of adding or deleting text in the region. This is done by
       creating a textBuffer containing the deleted text and the necessary
       additional context, and calling the wrappedLineCounter on it.
       
       NOTE: This must not be done in continuous wrap mode when the font
	     width is not fixed. In that case, the calculation would try
	     to access style information that is no longer available (deleted
	     text), or out of date (updated highlighting), possibly leading 
	     to completely wrong calculations and/or even crashes eventually.
	     (This is not theoretical; it really happened.)
	     
	     In that case, the calculation of the number of deleted lines
	     has happened before the buffer was modified (only in that case,
	     because resynchronization of the line starts is impossible
	     in that case, which makes the whole calculation less efficient).
    */
    if (textD->suppressResync) {
	*linesDeleted = textD->nLinesDeleted;
	textD->suppressResync = 0;
	return;
    }

    length = (pos-countFrom) + nDeleted +(countTo-(pos+nInserted));
    deletedTextBuf = BufCreatePreallocated(length);
    if (pos > countFrom)
        BufCopyFromBuf(textD->buffer, deletedTextBuf, countFrom, pos, 0);
    if (nDeleted != 0)
	BufInsert(deletedTextBuf, pos-countFrom, deletedText);
    if (countTo > pos+nInserted)    
	BufCopyFromBuf(textD->buffer, deletedTextBuf,
    	    pos+nInserted, countTo, pos-countFrom+nDeleted);
    /* Note that we need to take into account an offset for the style buffer:
       the deletedTextBuf can be out of sync with the style buffer. */
    wrappedLineCounter(textD, deletedTextBuf, 0, length, INT_MAX, True, 
	    countFrom, &retPos, &retLines, &retLineStart, &retLineEnd);
    BufFree(deletedTextBuf);
    *linesDeleted = retLines;
    textD->suppressResync = 0;
}

/*
** This is a stripped-down version of the findWrapRange() function above,
** intended to be used to calculate the number of "deleted" lines during
** a buffer modification. It is called _before_ the modification takes place.
** 
** This function should only be called in continuous wrap mode with a
** non-fixed font width. In that case, it is impossible to calculate
** the number of deleted lines, because the necessary style information 
** is no longer available _after_ the modification. In other cases, we
** can still perform the calculation afterwards (possibly even more
** efficiently).
*/
static void measureDeletedLines(textDisp *textD, int pos, int nDeleted)
{
    int retPos, retLines, retLineStart, retLineEnd;
    textBuffer *buf = textD->buffer;
    int nVisLines = textD->nVisibleLines;
    int *lineStarts = textD->lineStarts;
    int countFrom, lineStart;
    int nLines = 0, i;
    /*
    ** Determine where to begin searching: either the previous newline, or
    ** if possible, limit to the start of the (original) previous displayed
    ** line, using information from the existing line starts array
    */
    if (pos >= textD->firstChar && pos <= textD->lastChar) {
    	for (i=nVisLines-1; i>0; i--)
    	    if (lineStarts[i] != -1 && pos >= lineStarts[i])
    		break;
    	if (i > 0) {
    	    countFrom = lineStarts[i-1];
    	} else
    	    countFrom = BufStartOfLine(buf, pos);
    } else
    	countFrom = BufStartOfLine(buf, pos);
    
    /*
    ** Move forward through the (new) text one line at a time, counting
    ** displayed lines, and looking for either a real newline, or for the
    ** line starts to re-sync with the original line starts array
    */
    lineStart = countFrom;
    while (True) {
    	/* advance to the next line.  If the line ended in a real newline
    	   or the end of the buffer, that's far enough */
    	wrappedLineCounter(textD, buf, lineStart, buf->length, 1, True, 0,
    	    	&retPos, &retLines, &retLineStart, &retLineEnd);
    	if (retPos >= buf->length) {
    	    if (retPos != retLineEnd)
    	    	nLines++;
    	    break;
    	} else
    	    lineStart = retPos;
    	nLines++;
    	if (lineStart > pos + nDeleted &&
    	    	BufGetCharacter(buf, lineStart-1) == '\n') {
    	    break;
    	}
	
	/* Unlike in the findWrapRange() function above, we don't try to 
	   resync with the line starts, because we don't know the length 
	   of the inserted text yet, nor the updated style information. 
	   
	   Because of that, we also shouldn't resync with the line starts
	   after the modification either, because we must perform the
	   calculations for the deleted and inserted lines in the same way. 
	   
	   This can result in some unnecessary recalculation and redrawing
	   overhead, and therefore we should only use this two-phase mode
	   of calculation when it's really needed (continuous wrap + variable
	   font width). */
    }
    textD->nLinesDeleted = nLines;
    textD->suppressResync = 1;
}

/*
** Count forward from startPos to either maxPos or maxLines (whichever is
** reached first), and return all relevant positions and line count.
** The provided textBuffer may differ from the actual text buffer of the
** widget. In that case it must be a (partial) copy of the actual text buffer
** and the styleBufOffset argument must indicate the starting position of the
** copy, to take into account the correct style information.
**
** Returned values:
**
**   retPos:	    Position where counting ended.  When counting lines, the
**  	    	    position returned is the start of the line "maxLines"
**  	    	    lines beyond "startPos".
**   retLines:	    Number of line breaks counted
**   retLineStart:  Start of the line where counting ended
**   retLineEnd:    End position of the last line traversed
*/
static void wrappedLineCounter(const textDisp* textD, const textBuffer* buf,
        int startPos, int maxPos, int maxLines,
        Boolean startPosIsLineStart, int styleBufOffset,
        int* retPos, int* retLines, int* retLineStart, int* retLineEnd)
{
    int lineStart, newLineStart = 0, b, p, colNum, wrapMargin;
    int maxWidth, width, countPixels, i, foundBreak;
    int nLines = 0, tabDist = textD->buffer->tabDist;
    unsigned char c;
    char nullSubsChar = textD->buffer->nullSubsChar;
    
    /* If the font is fixed, or there's a wrap margin set, it's more efficient
       to measure in columns, than to count pixels.  Determine if we can count
       in columns (countPixels == False) or must count pixels (countPixels ==
       True), and set the wrap target for either pixels or columns */
    if (textD->fixedFontWidth != -1 || textD->wrapMargin != 0) {
    	countPixels = False;
	wrapMargin = textD->wrapMargin != 0 ? textD->wrapMargin :
            	textD->width / textD->fixedFontWidth;
        maxWidth = INT_MAX;
    } else {
    	countPixels = True;
    	wrapMargin = INT_MAX;
    	maxWidth = textD->width;
    }
    
    /* Find the start of the line if the start pos is not marked as a
       line start. */
    if (startPosIsLineStart)
	lineStart = startPos;
    else
	lineStart = TextDStartOfLine(textD, startPos);
    
    /*
    ** Loop until position exceeds maxPos or line count exceeds maxLines.
    ** (actually, contines beyond maxPos to end of line containing maxPos,
    ** in case later characters cause a word wrap back before maxPos)
    */
    colNum = 0;
    width = 0;
    for (p=lineStart; p<buf->length; p++) {
    	c = BufGetCharacter(buf, p);

    	/* If the character was a newline, count the line and start over,
    	   otherwise, add it to the width and column counts */
    	if (c == '\n') {
    	    if (p >= maxPos) {
    		*retPos = maxPos;
    		*retLines = nLines;
    		*retLineStart = lineStart;
    		*retLineEnd = maxPos;
    		return;
    	    }
    	    nLines++;
    	    if (nLines >= maxLines) {
    		*retPos = p + 1;
    		*retLines = nLines;
    		*retLineStart = p + 1;
    		*retLineEnd = p;
    		return;
    	    }
    	    lineStart = p + 1;
    	    colNum = 0;
    	    width = 0;
    	} else {
    	    colNum += BufCharWidth(c, colNum, tabDist, nullSubsChar);
    	    if (countPixels)
    	    	width += measurePropChar(textD, c, colNum, p+styleBufOffset);
    	}

    	/* If character exceeded wrap margin, find the break point
    	   and wrap there */
    	if (colNum > wrapMargin || width > maxWidth) {
    	    foundBreak = False;
    	    for (b=p; b>=lineStart; b--) {
    	    	c = BufGetCharacter(buf, b);
    	    	if (c == '\t' || c == ' ') {
    	    	    newLineStart = b + 1;
    	    	    if (countPixels) {
    	    	    	colNum = 0;
    	    	    	width = 0;
    	    	    	for (i=b+1; i<p+1; i++) {
    	    	    	    width += measurePropChar(textD,
				    BufGetCharacter(buf, i), colNum, 
				    i+styleBufOffset);
    	    	    	    colNum++;
    	    	    	}
    	    	    } else
    	    	    	colNum = BufCountDispChars(buf, b+1, p+1);
    	    	    foundBreak = True;
    	    	    break;
    	    	}
    	    }
    	    if (!foundBreak) { /* no whitespace, just break at margin */
    	    	newLineStart = max(p, lineStart+1);
    	    	colNum = BufCharWidth(c, colNum, tabDist, nullSubsChar);
    	    	if (countPixels)
   	    	    width = measurePropChar(textD, c, colNum, p+styleBufOffset);
    	    }
    	    if (p >= maxPos) {
    		*retPos = maxPos;
    		*retLines = maxPos < newLineStart ? nLines : nLines + 1;
    		*retLineStart = maxPos < newLineStart ? lineStart :
    		    	newLineStart;
    		*retLineEnd = maxPos;
    		return;
    	    }
    	    nLines++;
    	    if (nLines >= maxLines) {
    		*retPos = foundBreak ? b + 1 : max(p, lineStart+1);
    		*retLines = nLines;
    		*retLineStart = lineStart;
    		*retLineEnd = foundBreak ? b : p;
    		return;
    	    }
    	    lineStart = newLineStart;
    	}
    }

    /* reached end of buffer before reaching pos or line target */
    *retPos = buf->length;
    *retLines = nLines;
    *retLineStart = lineStart;
    *retLineEnd = buf->length;
}

/*
** Measure the width in pixels of a character "c" at a particular column
** "colNum" and buffer position "pos".  This is for measuring characters in
** proportional or mixed-width highlighting fonts.
**
** A note about proportional and mixed-width fonts: the mixed width and
** proportional font code in nedit does not get much use in general editing,
** because nedit doesn't allow per-language-mode fonts, and editing programs
** in a proportional font is usually a bad idea, so very few users would
** choose a proportional font as a default.  There are still probably mixed-
** width syntax highlighting cases where things don't redraw properly for
** insertion/deletion, though static display and wrapping and resizing
** should now be solid because they are now used for online help display.
*/
static int measurePropChar(const textDisp* textD, char c,
    int colNum, int pos)
{
    int charLen, style;
    char expChar[MAX_EXP_CHAR_LEN];
    textBuffer *styleBuf = textD->styleBuffer;
    
    charLen = BufExpandCharacter(c, colNum, expChar, 
	    textD->buffer->tabDist, textD->buffer->nullSubsChar);
    if (styleBuf == NULL) {
	style = 0;
    } else {
	style = (unsigned char)BufGetCharacter(styleBuf, pos);
	if (style == textD->unfinishedStyle) {
    	    /* encountered "unfinished" style, trigger parsing */
    	    (textD->unfinishedHighlightCB)(textD, pos, textD->highlightCBArg);
    	    style = (unsigned char)BufGetCharacter(styleBuf, pos);
	}
    }
    return stringWidth(textD, expChar, charLen, style);
}

/*
** Finds both the end of the current line and the start of the next line.  Why?
** In continuous wrap mode, if you need to know both, figuring out one from the
** other can be expensive or error prone.  The problem comes when there's a
** trailing space or tab just before the end of the buffer.  To translate an
** end of line value to or from the next lines start value, you need to know
** whether the trailing space or tab is being used as a line break or just a
** normal character, and to find that out would otherwise require counting all
** the way back to the beginning of the line.
*/
static void findLineEnd(textDisp *textD, int startPos, int startPosIsLineStart,
    	int *lineEnd, int *nextLineStart)
{
    int retLines, retLineStart;
    
    /* if we're not wrapping use more efficient BufEndOfLine */
    if (!textD->continuousWrap) {
    	*lineEnd = BufEndOfLine(textD->buffer, startPos);
    	*nextLineStart = min(textD->buffer->length, *lineEnd + 1);
    	return;
    }
    
    /* use the wrapped line counter routine to count forward one line */
    wrappedLineCounter(textD, textD->buffer, startPos, textD->buffer->length,
    	    1, startPosIsLineStart, 0, nextLineStart, &retLines,
    	    &retLineStart, lineEnd);
    return;
}

/*
** Line breaks in continuous wrap mode usually happen at newlines or
** whitespace.  This line-terminating character is not included in line
** width measurements and has a special status as a non-visible character.
** However, lines with no whitespace are wrapped without the benefit of a
** line terminating character, and this distinction causes endless trouble
** with all of the text display code which was originally written without
** continuous wrap mode and always expects to wrap at a newline character.
**
** Given the position of the end of the line, as returned by TextDEndOfLine
** or BufEndOfLine, this returns true if there is a line terminating
** character, and false if there's not.  On the last character in the
** buffer, this function can't tell for certain whether a trailing space was
** used as a wrap point, and just guesses that it wasn't.  So if an exact
** accounting is necessary, don't use this function.
*/ 
static int wrapUsesCharacter(textDisp *textD, int lineEndPos)
{
    char c;
    
    if (!textD->continuousWrap || lineEndPos == textD->buffer->length)
    	return True;
    
    c = BufGetCharacter(textD->buffer, lineEndPos);
    return c == '\n' || ((c == '\t' || c == ' ') &&
    	    lineEndPos + 1 != textD->buffer->length);
}

/*
** Decide whether the user needs (or may need) a horizontal scroll bar,
** and manage or unmanage the scroll bar widget accordingly.  The H.
** scroll bar is only hidden in continuous wrap mode when it's absolutely
** certain that the user will not need it: when wrapping is set
** to the window edge, or when the wrap margin is strictly less than
** the longest possible line.
*/
static void hideOrShowHScrollBar(textDisp *textD)
{
    if (textD->continuousWrap && (textD->wrapMargin == 0 || textD->wrapMargin *
    	    textD->fontStruct->max_bounds.width < textD->width))
    	XtUnmanageChild(textD->hScrollBar);
    else
    	XtManageChild(textD->hScrollBar);
}

/*
** Return true if the selection "sel" is rectangular, and touches a
** buffer position withing "rangeStart" to "rangeEnd"
*/
static int rangeTouchesRectSel(selection *sel, int rangeStart, int rangeEnd)
{
    return sel->selected && sel->rectangular && sel->end >= rangeStart &&
    	    sel->start <= rangeEnd;
}

/*
** Extend the range of a redraw request (from *start to *end) with additional
** redraw requests resulting from changes to the attached style buffer (which
** contains auxiliary information for coloring or styling text).
*/
static void extendRangeForStyleMods(textDisp *textD, int *start, int *end)
{
    selection *sel = &textD->styleBuffer->primary;
    int extended = False;
    
    /* The peculiar protocol used here is that modifications to the style
       buffer are marked by selecting them with the buffer's primary selection.
       The style buffer is usually modified in response to a modify callback on
       the text buffer BEFORE textDisp.c's modify callback, so that it can keep
       the style buffer in step with the text buffer.  The style-update
       callback can't just call for a redraw, because textDisp hasn't processed
       the original text changes yet.  Anyhow, to minimize redrawing and to
       avoid the complexity of scheduling redraws later, this simple protocol
       tells the text display's buffer modify callback to extend it's redraw
       range to show the text color/and font changes as well. */
    if (sel->selected) {
	if (sel->start < *start) {
	    *start = sel->start;
	    extended = True;
	}
	if (sel->end > *end) {
	    *end = sel->end;
	    extended = True;
	}
    }
    
    /* If the selection was extended due to a style change, and some of the
       fonts don't match in spacing, extend redraw area to end of line to
       redraw characters exposed by possible font size changes */
    if (textD->fixedFontWidth == -1 && extended)
    	*end = BufEndOfLine(textD->buffer, *end) + 1;
}

/**********************  Backlight Functions ******************************/
/*
** Allocate a read-only (shareable) colormap cell for a named color, from the
** the default colormap of the screen on which the widget (w) is displayed. If
** the colormap is full and there's no suitable substitute, print an error on
** stderr, and return the widget's background color as a backup.
*/
static Pixel allocBGColor(Widget w, char *colorName, int *ok)
{
    int r,g,b;
    *ok = 1;
    return AllocColor(w, colorName, &r, &g, &b);
}

static Pixel getRangesetColor(textDisp *textD, int ind, Pixel bground)
{
    textBuffer *buf;
    RangesetTable *tab;
    Pixel color;
    char *color_name;
    int valid;

    if (ind > 0) {
      ind--;
      buf = textD->buffer;
      tab = buf->rangesetTable;

      valid = RangesetTableGetColorValid(tab, ind, &color);
      if (valid == 0) {
          color_name = RangesetTableGetColorName(tab, ind);
          if (color_name)
              color = allocBGColor(textD->w, color_name, &valid);
          RangesetTableAssignColorPixel(tab, ind, color, valid);
      }
      if (valid > 0) {
          return color;
      }
    }
    return bground;
}

/*
** Read the background color class specification string in str, allocating the
** necessary colors, and allocating and setting up the character->class_no and
** class_no->pixel map arrays, returned via *pp_bgClass and *pp_bgClassPixel
** respectively.
** Note: the allocation of class numbers could be more intelligent: there can
** never be more than 256 of these (one per character); but I don't think
** there'll be a pressing need. I suppose the scanning of the specification
** could be better too, but then, who cares!
*/
void TextDSetupBGClasses(Widget w, XmString str, Pixel **pp_bgClassPixel,
      unsigned char **pp_bgClass, Pixel bgPixelDefault)
{
    unsigned char bgClass[256];
    Pixel bgClassPixel[256];
    int class_no = 0;
    char *semicol;
    char *s = (char *)str;
    size_t was_semicol;
    int lo, hi, dummy;
    char *pos;
    Boolean is_good = True;

    NEditFree(*pp_bgClass);
    NEditFree(*pp_bgClassPixel);

    *pp_bgClassPixel = NULL;
    *pp_bgClass = NULL;

    if (!s)
      return;

    /* default for all chars is class number zero, for standard background */
    memset(bgClassPixel, 0, sizeof bgClassPixel);
    memset(bgClass, 0, sizeof bgClass);
    bgClassPixel[0] = bgPixelDefault;
    /* since class no == 0 in a "style" has no set bits in BACKLIGHT_MASK
       (see styleOfPos()), when drawString() is called for text with a
       backlight class no of zero, bgClassPixel[0] is never consulted, and
       the default background color is chosen. */

    /* The format of the class string s is:
              low[-high]{,low[-high]}:color{;low-high{,low[-high]}:color}
          eg
              32-255:#f0f0f0;1-31,127:red;128-159:orange;9-13:#e5e5e5
       where low and high represent a character range between ordinal
       ASCII values. Using strtol() allows automatic octal, dec and hex
       reading of low and high. The example format sets backgrounds as follows:
              char   1 - 8    colored red     (control characters)
              char   9 - 13   colored #e5e5e5 (isspace() control characters)
              char  14 - 31   colored red     (control characters)
              char  32 - 126  colored #f0f0f0
              char 127        colored red     (delete character)
              char 128 - 159  colored orange  ("shifted" control characters)
              char 160 - 255  colored #f0f0f0
       Notice that some of the later ranges overwrite the class values defined
       for earlier ones (eg the first clause, 32-255:#f0f0f0 sets the DEL
       character background color to #f0f0f0; it is then set to red by the
       clause 1-31,127:red). */

    while (s && class_no < 255) {
        class_no++;                   /* simple class alloc scheme */
      was_semicol = 0;
      is_good = True;
      if ((semicol = (char *)strchr(s, ';'))) {
          *semicol = '\0';    /* null-terminate low[-high]:color clause */
          was_semicol = 1;
      }

      /* loop over ranges before the color spec, assigning the characters
         in the ranges to the current class number */
      for (lo = hi = strtol(s, &pos, 0);
           is_good;
           lo = hi = strtol(pos + 1, &pos, 0)) {
          if (pos && *pos == '-')
              hi = strtol(pos + 1, &pos, 0);  /* get end of range */
          is_good = (pos && 0 <= lo && lo <= hi && hi <= 255);
          if (is_good)
              while (lo <= hi)
                  bgClass[lo++] = (unsigned char)class_no;
          if (*pos != ',')
              break;
      }
      if ((is_good = (is_good && *pos == ':'))) {
          is_good = (*pos++ != '\0');         /* pos now points to color */
          bgClassPixel[class_no] = allocBGColor(w, pos, &dummy);
      }
      if (!is_good) {
          /* complain? this class spec clause (in string s) was faulty */
      }

      /* end of loop iterator clauses */
      if (was_semicol)
        *semicol = ';';       /* un-null-terminate low[-high]:color clause */
      s = semicol + was_semicol;
    }

    /* when we get here, we've set up our class table and class-to-pixel table
       in local variables: now put them into the "real thing" */
    class_no++;                     /* bigger than all valid class_nos */
    *pp_bgClass = (unsigned char *)NEditMalloc(256);
    *pp_bgClassPixel = (Pixel *)NEditMalloc(class_no * sizeof (Pixel));
    if (!*pp_bgClass || !*pp_bgClassPixel) {
        NEditFree(*pp_bgClass);
        NEditFree(*pp_bgClassPixel);
        return;
    }
    memcpy(*pp_bgClass, bgClass, 256);
    memcpy(*pp_bgClassPixel, bgClassPixel, class_no * sizeof (Pixel));
}
