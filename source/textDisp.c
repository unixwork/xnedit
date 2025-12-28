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
#include <ctype.h>
#ifndef __MVS__
#include <sys/param.h>
#endif

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

#define MCURSOR_ALLOC 8
#define MCURSOR_MAX 1024
#define MCURSOR_ALLOC_RESET 32

enum positionTypes {CURSOR_POS, CHARACTER_POS};

static void updateLineStarts(textDisp *textD, int pos, int charsInserted,
        int charsDeleted, int linesInserted, int linesDeleted, int *scrolled);
static void offsetLineStarts(textDisp *textD, int newTopLineNum);
static void calcLineStarts(textDisp *textD, int startLine, int endLine);
static void calcLastChar(textDisp *textD);
static int posToVisibleLineNum(textDisp *textD, int pos, int *lineNum);
static int getCharWidth(textDisp *textD, const char *src_orig, FcChar32 *dst, int len);
static FcChar32 getCharacter32(const textDisp *textD, const textBuffer* buf, int pos, int *charlen);
static void redisplayLine(textDisp *textD, int visLineNum, int leftClip,
        int rightClip, int leftCharIndex, int rightCharIndex);
static void drawString(textDisp *textD, int style, int rbIndex, int x, int y, int fromX,
        int toX, FcChar32 *string, int nChars, Boolean highlightLine, ansiStyle *ansi);
static void clearRect(textDisp *textD, XftColor *color, int x, int y, 
        int width, int height);
static void drawCursor(textDisp *textD, int x, int y);
static int styleOfPos(textDisp *textD, int lineStartPos,
        int lineLen, int lineIndex, int dispIndex, int thisChar);
static int charWidth4(const textDisp* textD, const FcChar32* string,
        int length, NFont *font);
static NFont* styleFontList(const textDisp* textD, int style);
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
static void redrawLineNumbers(textDisp *textD, int top, int height, int clearAll);
static void updateVScrollBarRange(textDisp *textD);
static int updateHScrollBarRange(textDisp *textD);
static int max(int i1, int i2);
static int min(int i1, int i2);
static int countLines(const char *string);
static int measureVisLine(textDisp *textD, int visLineNum);
static int emptyLinesVisible(textDisp *textD);
static void blankSingleCursorProtrusions(textDisp *textD);
static void blankCursorProtrusions(textDisp *textD);
static void allocateFixedFontGCs(textDisp *textD, Pixel bgPixel, Pixel fgPixel);
static GC allocateGC(Widget w, unsigned long valueMask,
        unsigned long foreground, unsigned long background, Font font,
        unsigned long dynamicMask, unsigned long dontCareMask);
static void releaseGC(Widget w, GC gc);
static void resetClipRectangles(textDisp *textD);
static int visLineLength(textDisp *textD, int visLineNum);
static void measureDeletedLines(textDisp *textD, int pos, int nDeleted);
static int  findWrapRange(textDisp *textD, const char *deletedText, int pos,
        int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd,
        int *linesInserted, int *linesDeleted);
static void wrappedLineCounter(const textDisp* textD, const textBuffer* buf,
        int startPos, int maxPos, int maxLines,
        Boolean startPosIsLineStart, int styleBufOffset,
        int* retPos, int* retLines, int* retLineStart, int* retLineEnd,
        Boolean *retWrap);
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
static int measurePropChar(const textDisp* textD, FcChar32 c,
        int colNum, int pos);
static XftColor allocBGColor(Widget w, char *colorName, int *ok);
static XftColor* getRangesetColor(textDisp *textD, int ind, XftColor *bground);
static void textDRedisplayRange(textDisp *textD, int start, int end);
static void findActiveAnsiStyle(textDisp *textD, ssize_t pos, ansiStyle *style);
static int parseEscapeSequence(textBuffer *buf, size_t pos, ansiStyle *style);
static void extendAnsiStyle(ansiStyle *style, ansiStyle *ext);
static void ansiFgToColorIndex(textDisp *textD, short fg, XftColor *color);
static void ansiBgToColorIndex(textDisp *textD, short bg, XftColor *color);

textDisp *TextDCreate(Widget widget, Widget hScrollBar, Widget vScrollBar,
        Position left, Position top, Position width, Position height,
        Position lineNumLeft, Position lineNumWidth, Position marginWidth,
        textBuffer *buffer, NFont *font, NFont *bold, NFont *italic,
        NFont *boldItalic, ColorProfile *colorProfile, int continuousWrap,
        int wrapMargin, int rightMargin, XmString bgClassString,
        Pixel calltipFGPixel, Pixel calltipBGPixel, Pixel lineHighlightBGPixel,
        Boolean indentRainbow, Boolean highlightCursorLine, Boolean ansiColors)
{
    textDisp *textD;
    XGCValues gcValues;
    int i;
    
    XftFont *xftFont = FontDefault(font);
    
    textD = (textDisp *)NEditMalloc(sizeof(textDisp));
    textD->w = widget;
    textD->d = NULL;
    textD->top = top;
    textD->left = left;
    textD->width = width;
    textD->height = height;
    textD->marginWidth = marginWidth;
    textD->cursorOn = True;
    /*
    textD->cursor->cursorPos = 0;
    textD->cursor->cursorPosCache = -1;
    textD->cursor->cursorPosCacheLeft = 0;
    textD->cursor->cursorPosCacheRight = 0;
    textD->cursor->x = -100;
    textD->cursor->y = -100;
    */
    textD->cursorToHint = NO_HINT;
    textD->cursorStyle = NORMAL_CURSOR;
    textD->xic_x = 0;
    textD->xic_y = 0;
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
    textD->font = FontRef(font);
    textD->boldFont = FontRef(bold);
    textD->italicFont = FontRef(italic);
    textD->boldItalicFont = FontRef(boldItalic);
    textD->ascent = xftFont->ascent;
    textD->descent = xftFont->descent;
    textD->fixedFontWidth = -1;
    textD->styleBuffer = NULL;
    textD->styleTable = NULL;
    textD->nStyles = 0;
    textD->colorProfile = colorProfile;
    textD->wrapMargin = wrapMargin;
    textD->continuousWrap = continuousWrap;
    allocateFixedFontGCs(
            textD, colorProfile->textBgColor.pixel, colorProfile->textFgColor.pixel);
    textD->lineNumLeft = lineNumLeft;
    textD->lineNumWidth = lineNumWidth;
    textD->nVisibleLines = (height - 1) / (textD->ascent + textD->descent) + 1;
    gcValues.foreground = colorProfile->cursorFgColor.pixel;
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
          &textD->bgClass, colorProfile->textBgColor);
    textD->suppressResync = 0;
    textD->nLinesDeleted = 0;
    textD->modifyingTabDist = 0;
    textD->pointerHidden = False;
    textD->disableRedisplay = False;
    textD->fixLeftClipAfterResize = False;
    textD->graphicsExposeQueue = NULL;
    textD->indentRainbow = indentRainbow;
    textD->highlightCursorLine = highlightCursorLine;
    textD->redrawCursorLine = False;
    
    textD->rightMargin = rightMargin;
    textD->rightMarginPos = rightMargin > 0 ? left + rightMargin * font->maxWidth : 0;  
    
    // Initialize multi cursor array
    textD->mcursorAlloc = MCURSOR_ALLOC;
    textD->mcursorSize = 1;
    textD->mcursorSizeReal = 1;
    textD->multicursor = NEditCalloc(MCURSOR_ALLOC, sizeof(textCursor));
    textD->mcursorOn = FALSE;
    
    // Initialize main cursor
    textD->multicursor[0].cursorPos = 0;
    textD->multicursor[0].cursorPosCache = -1;
    textD->multicursor[0].cursorPosCacheLeft = 0;
    textD->multicursor[0].cursorPosCacheRight = 0;
    textD->multicursor[0].cursorPreferredCol = -1;
    textD->multicursor[0].x = -100;
    textD->multicursor[0].y = -100;
    
    textD->cursor = textD->multicursor;
    textD->newcursor = textD->multicursor;
    
    textD->cacheNoWrappingWidth = 0;
    textD->cacheNoWrapping = False;
    
    TextDSetAnsiColors(textD, ansiColors);
    
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
    		XmNincrement, font->maxWidth, NULL);
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
 * Initialize the XftDraw object. This should be called after the widget
 * is realized.
 */
void TextDInitXft(textDisp *textD) {   
    XWindowAttributes attributes;
    XGetWindowAttributes(XtDisplay(textD->w), XtWindow(textD->w), &attributes); 
    
    Screen *screen = textD->w->core.screen;
    Visual *visual = screen->root_visual;
    for(int i=0;i<screen->ndepths;i++) {
        Depth d = screen->depths[i];
        if(d.depth == textD->w->core.depth) {
            visual = d.visuals;
            break;
        }
    }
    
    Display *dp = XtDisplay(textD->w);
    textD->d = XftDrawCreate(
            dp,
            XtWindow(textD->w),
            visual,
            textD->w->core.colormap);
    
}

/*
** Free a text display and release its associated memory.  Note, the text
** BUFFER that the text display displays is a separate entity and is not
** freed, nor are the style buffer or style table.
*/
void TextDFree(textDisp *textD)
{
    FontUnref(textD->font);
    FontUnref(textD->boldFont);
    FontUnref(textD->italicFont);
    FontUnref(textD->boldItalicFont);
    
    NEditFree(textD->multicursor);
    
    BufRemoveModifyCB(textD->buffer, bufModifiedCB, textD);
    BufRemovePreDeleteCB(textD->buffer, bufPreDeleteCB, textD);
    releaseGC(textD->w, textD->gc);
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
    TextDSetFont(textD, textD->font);
}


void TextDSetColorProfile(textDisp *textD, ColorProfile *profile)
{
    XGCValues values;
    Display *d = XtDisplay(textD->w);
    
    textD->colorProfile = profile;
    
    releaseGC(textD->w, textD->gc);
    allocateFixedFontGCs(textD, profile->textBgColor.pixel, profile->textFgColor.pixel);
    
    /* Change the cursor GC (the cursor GC is not shared). */
    values.foreground = profile->cursorFgColor.pixel;
    XChangeGC( d, textD->cursorFGGC, GCForeground, &values );
    
    /* Redisplay */
    TextDRedisplayRect(textD, textD->left, textD->top, textD->width,
                       textD->height);
    redrawLineNumbers(textD, textD->top, textD->height, True);
}

/*
** Change the (non highlight) font
*/
void TextDSetFont(textDisp *textD, NFont *font)
{   
    XftFont *fontStruct = FontDefault(font);
    int i, maxAscent = fontStruct->ascent, maxDescent = fontStruct->descent;
    int width, height;
    XftFont *styleFont;
    NFont *styleFontList;
    
    /* If font size changes, cursor will be redrawn in a new position */
    if(!textD->disableRedisplay) {
        blankCursorProtrusions(textD);
    }
    
    /* If there is a (syntax highlighting) style table in use, find the new
       maximum font height for this text display */
    for (i=0; i<textD->nStyles; i++) {
        styleFontList = textD->styleTable[i].font;
        styleFont = styleFontList ? FontDefault(styleFontList) : NULL;
        if (styleFont != NULL && styleFont->ascent > maxAscent)
            maxAscent = styleFont->ascent;
        if (styleFont != NULL && styleFont->descent > maxDescent)
            maxDescent = styleFont->descent;
    }
    textD->ascent = maxAscent;
    textD->descent = maxDescent;
    
    // force -1 fixedFontWidth, because the fixed font optimization
    // doesn't work with unicode
    textD->fixedFontWidth = -1;
    
    /* Don't let the height dip below one line, or bad things can happen */
    if (textD->height < maxAscent + maxDescent)
        textD->height = maxAscent + maxDescent;
 
    if(textD->font != font) {
        FontUnref(textD->font);
        textD->font = FontRef(font);
    }
    
    if(textD->disableRedisplay) {
        return;
    }
    
    /* Do a full resize to force recalculation of font related parameters */
    width = textD->width;
    height = textD->height;
    textD->width = textD->height = 0;
    TextDResize(textD, width, height);
    
    /* if the shell window doesn't get resized, and the new fonts are
       of smaller sizes, sometime we get some residual text on the
       blank space at the bottom part of text area. Clear it here. */
    clearRect(textD, &textD->colorProfile->textBgColor, textD->left, 
	    textD->top + textD->height - maxAscent - maxDescent, 
	    textD->width, maxAscent + maxDescent);

    /* Redisplay */
    TextDRedisplayRect(textD, textD->left, textD->top, textD->width,
            textD->height);
    
    /* Clean up line number area in case spacing has changed */
    redrawLineNumbers(textD, textD->top, textD->height, True);
}

void TextDSetBoldFont(textDisp *textD, NFont *boldFont)
{
    if(textD->boldFont != boldFont) {
        FontUnref(textD->boldFont);
        textD->boldFont = FontRef(boldFont);
    }
}

void TextDSetItalicFont(textDisp *textD, NFont *italicFont)
{
    if(textD->italicFont != italicFont) {
        FontUnref(textD->italicFont);
        textD->italicFont = FontRef(italicFont);
    }
}

void TextDSetBoldItalicFont(textDisp *textD, NFont *boldItalicFont)
{
    if(textD->boldItalicFont != boldItalicFont) {
        FontUnref(textD->boldItalicFont);
        textD->boldItalicFont = FontRef(boldItalicFont);
    }
}

int TextDMinFontWidth(textDisp *textD, Boolean considerStyles)
{
    int fontWidth = textD->font->minWidth;
    int i;

    if (considerStyles) {
        for (i = 0; i < textD->nStyles; ++i) {
            int thisWidth = (textD->styleTable[i].font)->minWidth;
            if (thisWidth < fontWidth) {
                fontWidth = thisWidth;
            }
        }
    }
    return fontWidth;
}

int TextDMaxFontWidth(textDisp *textD, Boolean considerStyles)
{
    int fontWidth = textD->font->maxWidth;
    int i;

    if (considerStyles) {
        for (i = 0; i < textD->nStyles; ++i) {
            int thisWidth = textD->styleTable[i].font->maxWidth;
            if (thisWidth > fontWidth) {
                fontWidth = thisWidth;
            }
        }
    }
    return fontWidth;
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
    
    if(width > oldWidth) {
        textD->fixLeftClipAfterResize = True;
    }
    
    textD->width = width;
    textD->height = height;
    
    if(width < textD->cacheNoWrappingWidth) {
        textD->cacheNoWrapping = False;
    }
    
    /* In continuous wrap mode, a change in width affects the total number of
       lines in the buffer, and can leave the top line number incorrect, and
       the top character no longer pointing at a valid line start */
    if (textD->continuousWrap && textD->wrapMargin==0 && width!=oldWidth && !textD->cacheNoWrapping) {
        Boolean wrap0 = False;
        Boolean wrap1 = False;
        
        int oldFirstChar = textD->firstChar;
        
        textD->firstChar = TextDStartOfLine(textD, textD->firstChar);
        int toplinenum = TextDCountLinesW(textD, 0, textD->firstChar, True, &wrap0);  
        int count = TextDCountLinesW(textD, textD->firstChar, textD->buffer->length, True, &wrap1);

        
        textD->nBufferLines = toplinenum + count;
        
        if(!(wrap0 || wrap1)) {
            textD->cacheNoWrapping = True;
            textD->cacheNoWrappingWidth = width;
        }
        
        textD->topLineNum = toplinenum+1;
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
    redrawLineNumbers(textD, textD->top, textD->height, True);
    
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
    if(textD->fixLeftClipAfterResize) {
        // this call was directly after a window resize
        // the left clip could be in the middle of a glyph, that was previously
        // not rendered
        // as a result, only the right half of the glyph would be rendered now
        // to fix this, decrease the left a bit
        int changeLeftClip = textD->font->fonts->font->max_advance_width;
        if(left > changeLeftClip) {
            left -= changeLeftClip;
            width += changeLeftClip;
        }
        textD->fixLeftClipAfterResize = False;
    }
    
    /* find the line number range of the display */
    fontHeight = textD->ascent + textD->descent;
    firstLine = (top - textD->top - fontHeight + 1) / fontHeight;
    lastLine = (top + height - textD->top) / fontHeight;
    
    if(textD->d && textD->rightMarginPos > 0) {
        XRectangle clipRect;
        clipRect.x = left;
        clipRect.y = top;
        clipRect.width = width + 9000;
        clipRect.height = height;
        XftDrawSetClipRectangles(textD->d, 0, 0, &clipRect, 1);
        
        // draw right border line
        XftDrawRect(textD->d, &textD->colorProfile->rborderColor, textD->rightMarginPos, top, 1, height);
        
        // draw remaining right area using bg2
        /*
        int bg2width = left + width - textD->rightMarginPos;
        XftDrawRect(textD->d, &textD->colorProfile->textBgColor2, textD->rightMarginPos + 1, top, bg2width, height);
        */
    }
    
    /* If the graphics contexts are shared using XtAllocateGC, their
       clipping rectangles may have changed since the last use */
    resetClipRectangles(textD);
      
    /* draw the lines of text */
    for (line=firstLine; line<=lastLine; line++)
    	redisplayLine(textD, line, left-textD->marginWidth, left+width, 0, INT_MAX);
    
    /* draw the line numbers if exposed area includes them */
    if (textD->lineNumWidth != 0 && left <= textD->lineNumLeft + textD->lineNumWidth) {
        redrawLineNumbers(textD, top, height, True);
    }
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
    int oldLineStart, newLineStart, oldLineEnd, newLineEnd;
    Boolean hiline = False;
    if(textD->highlightCursorLine) {
        oldLineStart = BufStartOfLine(textD->buffer, textD->cursor->cursorPos);
        newLineStart = BufStartOfLine(textD->buffer, newPos);
        if(oldLineStart != newLineStart || textD->mcursorOn || textD->redrawCursorLine) {
            hiline = True;
            oldLineEnd = BufEndOfLine(textD->buffer, textD->cursor->cursorPos);
            newLineEnd = BufEndOfLine(textD->buffer, newPos);
            textD->cursor->cursorPos = -1;
        }
    }
    
    Boolean redrawTextWidget = 0;
    if(TextDClearMultiCursor(textD)) {
        redrawTextWidget = True;
        textD->cursor = textD->multicursor;
    } else if (newPos == textD->cursor->cursorPos) {
        /* do nothing if it hasn't changed */
        return;
    }
    
    /* make sure new position is ok */
    if (newPos < 0) newPos = 0;
    if (newPos > textD->buffer->length) newPos = textD->buffer->length;
    
    /* cursor movement cancels vertical cursor motion column */
    textD->cursor->cursorPreferredCol = -1;
   
    /* erase the cursor at it's previous position */
    TextDBlankCursor(textD);
    
    /* draw it at its new position */
    textD->cursor->cursorPos = newPos;
    textD->cursorOn = True;
    
    if(redrawTextWidget) {
        // redraw everything
        TextDRedisplayRect(textD, 0, textD->top, textD->width + textD->left, textD->height);
        return;
    }
    
    int left, right;
    TextDCursorLR(textD, &left, &right);
    textDRedisplayRange(textD, left, right);
    
    if(hiline) {
        int oldLine = -1, newLine = -1, oldLine2 = -1, newLine2 = -1;
        
        posToVisibleLineNum(textD, newLineStart, &newLine);
        posToVisibleLineNum(textD, newLineEnd, &newLine2);
        
        if(newLine == -1 && newLine2 >= 0) {
            newLine = 0;
        }
        if(newLine2 == -1 && newLine >= 0) {
            newLine2 = textD->nVisibleLines-1;
        }
        
        posToVisibleLineNum(textD, oldLineStart, &oldLine);
        posToVisibleLineNum(textD, oldLineEnd, &oldLine2);
        
        if(oldLine == -1 && oldLine2 >= 0) {
            oldLine = 0;
        }
        if(oldLine2 == -1 && oldLine >= 0) {
            oldLine2 = textD->nVisibleLines-1;
        }
        
        for(int i=oldLine;i<=oldLine2;i++) {
            redisplayLine(textD, i, 0, INT_MAX, 0, INT_MAX);
        }
        
        for(int i=newLine;i<=newLine2;i++) {
            redisplayLine(textD, i, 0, INT_MAX, 0, INT_MAX);
        }
    }
}

/*
 * Add diff to all cursors >= startPos
 */
void TextDChangeCursors(textDisp *textD, int startPos, int diff) {
    //int prevPos = -2;
    size_t newMCursorSize = textD->mcursorSize;
    for(int i=textD->mcursorSize-1;i>=0;i--) {
        if(textD->multicursor[i].cursorPos < startPos) {
            break;
        }
        textD->multicursor[i].cursorPos += diff;
        textD->multicursor[i].cursorPreferredCol = -1;
        
        // cursor out of bounds: remove cursor
        if(textD->multicursor[i].cursorPos > textD->buffer->length) {
            newMCursorSize = i;
        }
    }
    textD->mcursorSize = newMCursorSize > 0 ? newMCursorSize : 1;
    textD->mcursorSizeReal = textD->mcursorSize;
    if(textD->mcursorSize == 1) {
        textD->mcursorOn = False;
    }
}

int TextDAddCursor(textDisp *textD, int newMultiCursorPos) {
    int mcInsertPos = 0;
    
    // make sure, there is not already a cursor for the new position
    for(int i=0;i<textD->mcursorSize;i++) {
        if(textD->multicursor[i].cursorPos == newMultiCursorPos) {
            return i; // pos already in the cursor pos array
        } else if(textD->multicursor[i].cursorPos > newMultiCursorPos) {
            break;
        }
        mcInsertPos = i+1;
    }
    
    // check limit
    if(textD->mcursorSize == MCURSOR_MAX) {
        return -1;
    }
    
    // check array size, do we need to realloc the array?
    if(textD->mcursorSize == textD->mcursorAlloc) {
        textD->mcursorAlloc *= 2;
        textD->multicursor = NEditRealloc(textD->multicursor, textD->mcursorAlloc * sizeof(textCursor));
    }
    
    textD->mcursorOn = TRUE;
    
    // add cursor (sorted)
    textD->mcursorSize++;
    textD->mcursorSizeReal = textD->mcursorSize;
    if(mcInsertPos+1 < textD->mcursorSize) {
        // insert pos in the middle
        // move elements one position
        memmove(textD->multicursor+mcInsertPos+1, textD->multicursor+mcInsertPos, (textD->mcursorSize-mcInsertPos-1)*sizeof(textCursor));
    }
    textCursor newCursor = TextDPos2Cursor(textD, newMultiCursorPos);
    textD->multicursor[mcInsertPos] = newCursor;
    textD->newcursor = &textD->multicursor[mcInsertPos];
    
    textD->cursor = textD->multicursor;
    
    // render new cursor
    textD->cursorOn = False;
    if(textD->highlightCursorLine) {
        // redraw entire line
        //int newCursorLineStart = BufStartOfLine(textD->buffer, newMultiCursorPos);
        int newCursorLine;
        posToVisibleLineNum(textD, newMultiCursorPos, &newCursorLine);
        redisplayLine(textD, newCursorLine, 0, INT_MAX, 0, INT_MAX);
    }
    TextDUnblankCursor(textD);
    
    return -1;
}

void TextDRemoveCursor(textDisp *textD, int cursorIndex) { 
    if(textD->mcursorSize == 1) {
        return;
    } else if(textD->mcursorSize == 2) {
        textD->mcursorOn = FALSE;
    }
    
    int cursorLine;
    posToVisibleLineNum(textD, textD->multicursor[cursorIndex].cursorPos, &cursorLine);
    
    if(cursorIndex+1 != textD->mcursorSize) {
        memmove(textD->multicursor + cursorIndex, textD->multicursor + cursorIndex + 1, (textD->mcursorSize - cursorIndex - 1)*sizeof(textCursor));
    }
    textD->mcursorSize--;
    textD->mcursorSizeReal = textD->mcursorSize;
     
    if(textD->highlightCursorLine) {
        // redraw entire line
        redisplayLine(textD, cursorLine, 0, INT_MAX, 0, INT_MAX);
    }
    
    textD->newcursor = textD->cursor;
}

void TextDSetCursors(textDisp *textD, size_t *cursors, size_t ncursors) {
    TextDBlankCursor(textD);    
    textD->mcursorSize = 0;
    for(int i=0;i<ncursors;i++) {
        TextDAddCursor(textD, (int)cursors[i]);
    }
}

int TextDClearMultiCursor(textDisp *textD) {
    if(textD->mcursorSize > 1) {
        TextDBlankCursor(textD);     
        textD->mcursorOn = FALSE;
        textD->mcursorSize = 1;
        textD->mcursorSizeReal = 1;
        if(textD->mcursorAlloc > MCURSOR_ALLOC_RESET) {
            // reduce multicursor array, if it is too big
            textD->mcursorAlloc = MCURSOR_ALLOC;
            textD->multicursor = NEditRealloc(textD->multicursor, MCURSOR_ALLOC * sizeof(textCursor));
        }
        textD->cursor = textD->multicursor;
        textD->newcursor = textD->multicursor;
        return 1;
    }
    return 0;
}

void TextDCheckCursorDuplicates(textDisp *textD) {
    size_t mcursorSize = textD->mcursorSize;
    for(size_t i=1;i<textD->mcursorSize;i++) {
        if(textD->multicursor[i].cursorPos == textD->multicursor[i-1].cursorPos) {
            TextDRemoveCursor(textD, i);
            mcursorSize--;
            i--;
        }
    }
    textD->cursor = textD->multicursor;
    textD->newcursor = textD->multicursor;
    
    if(textD->highlightCursorLine) {
        size_t lastPos = textD->multicursor[textD->mcursorSize-1].cursorPos;
        int cursorLine;
        posToVisibleLineNum(textD, lastPos, &cursorLine);
        
        if(cursorLine + 1 < textD->nVisibleLines) {
            redisplayLine(textD, cursorLine + 1, 0, INT_MAX, 0, INT_MAX);
        }
    }
}

static void textDBlankCursorPos(textDisp *textD) {
    blankSingleCursorProtrusions(textD);
    textD->cursorOn = False;
    int left, right;
    TextDCursorLR(textD, &left, &right);
    textDRedisplayRange(textD, left, right);
}

void TextDBlankCursor(textDisp *textD)
{
    if (!textD->cursorOn)
    	return;
    
    if(textD->mcursorSize == 1) {
        textDBlankCursorPos(textD);
    } else {
        textCursor *origCursor = textD->cursor;
        for(int i=0;i<textD->mcursorSize;i++) {
            textD->cursor = textD->multicursor + i;
            textDBlankCursorPos(textD);
        }
        textD->cursor = origCursor;
    }
}

void textDUnblankCursorPos(textDisp *textD) {
    int left, right;
    TextDCursorLR(textD, &left, &right);
    textDRedisplayRange(textD, left, right);
}

void TextDUnblankCursor(textDisp *textD)
{
    if (!textD->cursorOn) {
    	textD->cursorOn = True;
        if(textD->mcursorSize == 1) {
            textDUnblankCursorPos(textD);
        } else {         
            textCursor *origCursor = textD->cursor;
            for(int i=0;i<textD->mcursorSize;i++) {
                textD->cursor = textD->multicursor + i;
                textDUnblankCursorPos(textD);
            }
            textD->cursor = origCursor;
        }
    }
}

void TextDSetCursorStyle(textDisp *textD, int style)
{
    textD->cursorStyle = style;
    blankCursorProtrusions(textD);
    if (textD->cursorOn) {
        int left, right;
        if(textD->mcursorSize == 1) {
            TextDCursorLR(textD, &left, &right);
            textDRedisplayRange(textD, left, right);
        } else {
            textCursor *origCursor = textD->cursor;
            for(int i=0;i<textD->mcursorSize;i++) {
                textD->cursor = textD->multicursor + i;
                TextDCursorLR(textD, &left, &right);
                textDRedisplayRange(textD, left, right);
            }
            textD->cursor = origCursor;
        } 
    }
}

Boolean TextDPosHasCursor(textDisp *textD, int pos, int *index) {
    *index = -1;
    if(textD->mcursorSizeReal == 1) {
        return pos == textD->cursor->cursorPos;
    } else {
        for(int i=0;i<textD->mcursorSizeReal;i++) {
            int cursor = textD->multicursor[i].cursorPos;
            if(pos == cursor) {
                *index = i;
                return True;
            }
        }
        return False;
    }
}

Boolean TextDRangeHasCursor(textDisp *textD, int start, int end) {
    if(textD->mcursorSizeReal == 1) {
        int cursor = textD->cursor->cursorPos;
        return cursor >= start && cursor <= end;
    } else {
        for(int i=0;i<textD->mcursorSizeReal;i++) {
            int cursor = textD->multicursor[i].cursorPos;
            if(cursor >= start && cursor <= end) {
                return True;
            }
        }
        return False;
    }
}

void TextDSetWrapMode(textDisp *textD, int wrap, int wrapMargin)
{
    textD->wrapMargin = wrapMargin;
    textD->continuousWrap = wrap;
    textD->cacheNoWrapping = False;
    
    /* wrapping can change change the total number of lines, re-count */
    Boolean retWrap;
    textD->nBufferLines = TextDCountLinesW(textD, 0, textD->buffer->length,
                                           True, &retWrap);
    if(!retWrap) {
        textD->cacheNoWrappingWidth = textD->width;
        textD->cacheNoWrapping = True;
    }
    
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
    return textD->cursor->cursorPos;
}

/*
** Insert "text" at the current cursor location.  This has the same
** effect as inserting the text into the buffer using BufInsert and
** then moving the insert position after the newly inserted text, except
** that it's optimized to do less redrawing.
*/
void TextDInsert(textDisp *textD, char *text)
{
    int pos = textD->cursor->cursorPos;
    
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
    int startPos = textD->cursor->cursorPos;
    textBuffer *buf = textD->buffer;
    int lineStart = BufStartOfLine(buf, startPos);
    int textLen = strlen(text);
    int i, p, endPos, indent, startIndent, endIndent, inc;
    char *c, ch, *paddedText = NULL;
    
    /* determine how many displayed character positions are covered */
    startIndent = BufCountDispChars(textD->buffer, lineStart, startPos);
    indent = startIndent;
    for (c=text; *c!='\0'; c+=inc) {
        inc = Utf8CharLen((unsigned char*)c);
        if(inc >= 1) {
            indent++;
        } else {
            indent += BufCharWidth(*c, indent, buf->tabDist, buf->nullSubsChar);
        }
    }
    endIndent = indent;
    
    /* find which characters to remove, and if necessary generate additional
       padding to make up for removed control characters at the end */
    indent=startIndent;
    for (p=startPos; ; p+=inc) {
    	if (p == buf->length)
    	    break;
    	ch = BufGetCharacter(buf, p);
        inc = Utf8CharLen((unsigned char*)&ch);
    	if (ch == '\n')
    	    break;
    	indent += BufCharWidth(ch, indent, buf->tabDist, buf->nullSubsChar);
    	if (indent == endIndent) {
    	    p += inc;
    	    break;
    	} else if (indent > endIndent) {
    	    if (ch != '\t') {
    	    	p += inc;
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


XftColor PixelToColor(Widget w, Pixel p)
{
    XColor xcolor;
    memset(&xcolor, 0, sizeof(XColor));
    xcolor.pixel = p;
    XQueryColor(XtDisplay(w), w->core.colormap, &xcolor);
    
    XftColor color;
    color.pixel = p;
    color.color.red = xcolor.red;
    color.color.green = xcolor.green;
    color.color.blue = xcolor.blue;
    color.color.alpha = 0xFFFF;
    return color;
}

/*
XftColor RGBToColor(short r, short g, short b)
{
    XftColor color;
    color.color.red = r;
    color.color.green = g;
    color.color.blue = b;
    color.color.alpha = 0xFFFF;
    return color;
}
*/

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
    int i, lineEnd, charIndex, outIndex, isMB;
    int lineStart=0, charLen=0;
    char expandedChar[MAX_EXP_CHAR_LEN];

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
      char *lineStrAlloc;
      const char *lineStr = BufGetRange2(textD->buffer, lineStart, lineEnd, &lineStrAlloc);
      outIndex = 0;
      for(i=lineStart; i<lineEnd; i++, charIndex++) {
          charLen = BufExpandCharacter(lineStr+charIndex, lineEnd-charIndex,
                  outIndex, expandedChar, textD->buffer->tabDist,
                  textD->buffer->nullSubsChar, &isMB);
          if ( outIndex+charLen >= column ) break;
          outIndex+=charLen;
      }
      NEditFree(lineStrAlloc);

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
    int visLineNum, charLen, outIndex, xStep, charStyle, inc;
    FcChar32 expandedChar[MAX_EXP_CHAR_LEN];
    FcChar32 uc;
    NFont *font;
    
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
    char *lineStrAlloc;
    const char *lineStr = BufGetRange2(textD->buffer, lineStartPos, lineStartPos + lineLen, &lineStrAlloc);
    
    /* Step through character positions from the beginning of the line
       to "pos" to calculate the x coordinate */
    xStep = textD->left - textD->horizOffset;
    outIndex = 0;
    for(charIndex=0; charIndex<pos-lineStartPos; charIndex+=inc) {
        inc = getCharWidth(textD, lineStr+charIndex, &uc, lineLen - charIndex);
        if(inc > 1) {
            charLen = 1;
            expandedChar[0] = uc;
        } else {
            charLen = BufExpandCharacter4(lineStr[charIndex],
                    outIndex,
                    expandedChar,
                    textD->buffer->tabDist, textD->buffer->nullSubsChar);
        }
        
   	charStyle = styleOfPos(textD, lineStartPos, lineLen, charIndex,
   	    	outIndex, lineStr[charIndex]);
        font = styleFontList(textD, charStyle);
    	xStep += charWidth4(textD, expandedChar, charLen, font);
    	outIndex += charLen;
    }
    *x = xStep;
    NEditFree(lineStrAlloc);
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
        int startPos = BufStartOfLine(buf, pos);
	*column = BufCountDispChars(buf, startPos, pos);
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
    int cursorPos = textD->cursor->cursorPos;
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
        fprintf(stderr, "xnedit: internal consistency check tl1 failed\n");
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
    if (posToVisibleLineNum(textD, textD->cursor->cursorPos, visLineNum)) {
        *lineStartPos = textD->lineStarts[*visLineNum];
    }
    else {
        *lineStartPos = TextDStartOfLine(textD, textD->cursor->cursorPos);
        *visLineNum = -1;
    }

    /* Decide what column to move to, if there's a preferred column use that */
    column = (textD->cursor->cursorPreferredCol >= 0)
            ? textD->cursor->cursorPreferredCol
            : BufCountDispChars(textD->buffer, *lineStartPos, textD->cursor->cursorPos);
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
    if (textD->cursor->cursorPos >= textD->buffer->length)
        return False;

    TextDSetInsertPosition(
            textD,
            BufRightPos(textD->buffer, textD->cursor->cursorPos));
    return True;
}

int TextDMoveLeft(textDisp *textD)
{
    if (textD->cursor->cursorPos <= 0)
        return False;

    TextDSetInsertPosition(textD, BufLeftPos(textD->buffer, textD->cursor->cursorPos)); 
    return True;
}

int TextDMoveUp(textDisp *textD, int absolute)
{
    int lineStartPos, column, prevLineStartPos, newPos, visLineNum;
    
    /* Find the position of the start of the line.  Use the line starts array
       if possible, to avoid unbounded line-counting in continuous wrap mode */
    if (absolute) {
        lineStartPos = BufStartOfLine(textD->buffer, textD->cursor->cursorPos);
        visLineNum = -1;
    } else if (posToVisibleLineNum(textD, textD->cursor->cursorPos, &visLineNum))
    	lineStartPos = textD->lineStarts[visLineNum];
    else {
    	lineStartPos = TextDStartOfLine(textD, textD->cursor->cursorPos);
    	visLineNum = -1;
    }
    if (lineStartPos == 0)
    	return False;
    
    /* Decide what column to move to, if there's a preferred column use that */
    column = textD->cursor->cursorPreferredCol >= 0
            ? textD->cursor->cursorPreferredCol
            : BufCountDispChars(textD->buffer, lineStartPos, textD->cursor->cursorPos);
    
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
    textD->cursor->cursorPreferredCol = column;
    
    return True;
}

int TextDMoveDown(textDisp *textD, int absolute)
{
    int lineStartPos, column, nextLineStartPos, newPos, visLineNum;

    if (textD->cursor->cursorPos == textD->buffer->length) {
        return False;
    }

    if (absolute) {
        lineStartPos = BufStartOfLine(textD->buffer, textD->cursor->cursorPos);
        visLineNum = -1;
    } else if (posToVisibleLineNum(textD, textD->cursor->cursorPos, &visLineNum)) {
        lineStartPos = textD->lineStarts[visLineNum];
    } else {
        lineStartPos = TextDStartOfLine(textD, textD->cursor->cursorPos);
        visLineNum = -1;
    }

    column = textD->cursor->cursorPreferredCol >= 0
            ? textD->cursor->cursorPreferredCol
            : BufCountDispChars(textD->buffer, lineStartPos, textD->cursor->cursorPos);

    if (absolute)
        nextLineStartPos = BufCountForwardNLines(textD->buffer, lineStartPos, 1);
    else
        nextLineStartPos = TextDCountForwardNLines(textD, lineStartPos, 1, True);

    newPos = BufCountForwardDispChars(textD->buffer, nextLineStartPos, column);

    if (textD->continuousWrap && !absolute) {
        newPos = min(newPos, TextDEndOfLine(textD, nextLineStartPos, True));
    }

    TextDSetInsertPosition(textD, newPos);
    textD->cursor->cursorPreferredCol = column;
    
    return True;
}

textCursor TextDPos2Cursor(textDisp *textD, int pos) {
    textBuffer *buf = textD->buffer;
    textCursor c;
    c.cursorPos = pos;
    c.cursorPosCache = pos;
    c.cursorPosCacheLeft = BufLeftPos(buf, pos);
    c.cursorPosCacheRight = BufRightPos(buf, pos);
    c.cursorPreferredCol = -1;
    c.x = -100;
    c.y = -100;
    if(textD->ansiColors) {
        while(BufGetCharacter(buf, c.cursorPosCacheLeft) == '\e') {
                c.cursorPosCacheLeft = BufLeftPos(buf, c.cursorPosCacheLeft);
        }
        while(BufGetCharacter(buf, pos) == '\e') {
            c.cursorPosCacheRight += BufCharLen(buf, c.cursorPosCacheRight);
            pos = c.cursorPosCacheRight;
        }
        c.cursorPosCacheRight += BufCharLen(buf, textD->cursor->cursorPosCacheRight);
    }
    return c;
}

void TextDCursorLR(textDisp *textD, int *left, int *right)
{
    if(textD->cursor->cursorPos != textD->cursor->cursorPosCache) {
        textCursor c = TextDPos2Cursor(textD, textD->cursor->cursorPos);
        textD->cursor->cursorPosCache = c.cursorPosCache;
        textD->cursor->cursorPosCacheLeft = c.cursorPosCacheLeft;
        textD->cursor->cursorPosCacheRight = c.cursorPosCacheRight;
    }
    *left = textD->cursor->cursorPosCacheLeft;
    *right = textD->cursor->cursorPosCacheRight;
    
}

void TextDSetAnsiColors(textDisp *textD, Boolean ansiColors)
{
    textD->ansiColors = ansiColors;
    if(ansiColors) {
        BufEnableAnsiEsc(textD->buffer);
        textD->cursor->cursorPosCache = -1;
    } else {
        BufDisableAnsiEsc(textD->buffer);
    }
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
    Boolean retWrap;
    int retLines = TextDCountLinesW(textD, startPos, endPos, startPosIsLineStart, &retWrap);
    if(retWrap) {
        textD->cacheNoWrapping = False;
    }
    return retLines;
}

int TextDCountLinesW(textDisp *textD, int startPos, int endPos,
    	int startPosIsLineStart, Boolean *retWrapped)
{
    int retLines, retPos, retLineStart, retLineEnd;
    
    /* If we're not wrapping use simple (and more efficient) BufCountLines */
    if (!textD->continuousWrap) {
        if(retWrapped) {
            *retWrapped = 0;
        }
        return BufCountLines(textD->buffer, startPos, endPos);
    }
    
    wrappedLineCounter(textD, textD->buffer, startPos, endPos, INT_MAX,
	    startPosIsLineStart, 0, &retPos, &retLines, &retLineStart,
	    &retLineEnd, retWrapped);
    
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
    	    &retLineEnd, NULL);
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
	    &retLineEnd, NULL);
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
	    &retLineEnd, NULL);
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
	    	True, 0, &retPos, &retLines, &retLineStart, &retLineEnd,
                NULL);
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
    int scrolled, origCursorPos = textD->cursor->cursorPos;
    int wrapModStart, wrapModEnd;
    int redrawLN = False;
    int diff = nInserted - nDeleted;
    
    /* buffer modification cancels vertical cursor motion column */
    if (nInserted != 0 || nDeleted != 0)
    	textD->cursor->cursorPreferredCol = -1;
    
    /* Count the number of lines inserted and deleted, and in the case
       of continuous wrap mode, how much has changed */
    if (textD->continuousWrap) {
    	redrawLN = findWrapRange(textD, deletedText, pos, nInserted, nDeleted,
    	    	&wrapModStart, &wrapModEnd, &linesInserted, &linesDeleted);
        if(!redrawLN && nDeleted > 0) {
            for(int i=0;i<nDeleted;i++) {
                if(deletedText[i] == '\n') {
                    redrawLN = 1;
                    break;
                }
            }
        }
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
    	textD->cursor->cursorPos = textD->cursorToHint;
    	textD->cursorToHint = NO_HINT;
    } else if (textD->cursor->cursorPos > pos) {
        if(textD->mcursorSize > 1) {
            // multi cursor update
            TextDChangeCursors(textD, pos, nInserted - nDeleted);
        } else {
            if (textD->cursor->cursorPos < pos + nDeleted)
                textD->cursor->cursorPos = pos;
            else
                textD->cursor->cursorPos += nInserted - nDeleted;
        }
    }
    
    /* If the changes caused scrolling, re-paint everything and we're done. */
    Bool isLastCursor = textD->cursor == textD->multicursor + textD->mcursorSize - 1;
    if(scrolled && isLastCursor) {
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
    // translated cursor pos
    int cpos = origCursorPos;
    if(cpos > pos) {
        cpos += diff;
    }
    if (textD->highlightCursorLine) {
        cpos = BufStartOfLine(buf, cpos);
    }
    if (origCursorPos == startDispPos && textD->cursor->cursorPos != startDispPos) {
        startDispPos = min(startDispPos, BufLeftPos(buf, cpos));
    }
    	
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
        // for some reason, a simple insert of one character always results
        // in linesInserted > 2
        if (linesInserted > 2 || redrawLN) redrawLineNumbers(textD, textD->top, textD->height, True);
    } else { /* linesInserted != linesDeleted */
    	endDispPos = textD->lastChar + 1;
    	if (origCursorPos >= pos) {
            blankCursorProtrusions(textD);
        }
    	    
	redrawLineNumbers(textD, textD->top, textD->height, True);
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
    		    fprintf(stderr, "xnedit: Consistency check ptvl failed\n");
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
 * Get the number of bytes for a character
 * If ANSI coloring is enabled and the the first character is <ESC>
 * return the number of bytes of the escape sequence
 */
static int getCharWidth(textDisp *textD, const char *src_orig, FcChar32 *dst, int len)
{
    if(textD->ansiColors && *src_orig == '\e') {
        // number of bytes for the complete escape sequence
        if(len < 3) return 1;
        if(src_orig[1] != '[') return 1;
        int i;
        for(i=2;i<len;i++) {
            char c = src_orig[i];
            if(c == 'm') {
                i++;
                break;
            }
            if(c < '0' || (c > '9' && c != ';')) break;
        }
        *dst = 0;
        return i;
    } else {
        // number of UTF-8 bytes for the Unicode Character
        return Utf8ToUcs4(src_orig, dst, len);
    }
}

static FcChar32 getCharacter32(const textDisp *textD, const textBuffer* buf, int pos, int *charlen)
{
    if(textD->ansiColors) {
        if(BufGetCharacter(buf, pos) == '\e') {
            if(BufGetCharacter(buf, pos+1) == '[') {
                int start = pos;
                pos += 2;
                char c;
                while((c = BufGetCharacter(buf, pos)) != '\0') {
                    if(c == 'm') {
                        pos++;
                        break;
                    }
                    if(c < '0' || (c > '9' && c != ';')) break;
                    pos++;
                }
                *charlen = pos - start;
                return 0;
            }
        }    
    }
    return BufGetCharacter32(buf, pos, charlen);
}

typedef struct _textCursorX {
    int cursorX;
    int index;
} textCursorX;

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
    int x, y, startX, charIndex, lineStartPos, lineLen, fontHeight, inc;
    int stdCharWidth, charWidth, startIndex, charStyle, style;
    int charLen, outStartIndex, outIndex, hasCursor = False;
    int dispIndexOffset, y_orig;
    int startOfLine = INT_MAX;
    int endOfLine = 0;
    int cursorLine = False;
    FcChar32 expandedChar[MAX_EXP_CHAR_LEN];
    FcChar32 outStr[MAX_DISP_LINE_LEN];
    FcChar32 *outPtr;
    const char *lineStr;
    char *lineStrFree;
    char baseChar;
    FcChar32 uc = 0;
    NFont *styleFL = textD->font;
    NFont *charFL;
    XftFont *styleFont;
    XftFont *charFont;
    int rbTabDist = TEXT_OF_TEXTD(textD).emulateTabs > 0 ?
            TEXT_OF_TEXTD(textD).emulateTabs : buf->tabDist;
    Boolean indentRainbow = textD->indentRainbow;
    
    textCursorX singleCursor = { textD->cursor->cursorPos, 0 };
    textCursorX *cursorX = textD->mcursorSizeReal == 1 ? &singleCursor : NEditCalloc(textD->mcursorSizeReal, sizeof(textCursorX));
    int cursorNum = 0;
    int cursorIndex;
     
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
        lineStrFree = NULL;
    } else {
	lineLen = visLineLength(textD, visLineNum);
	lineStr = BufGetRange2(buf, lineStartPos, lineStartPos + lineLen, &lineStrFree);
        endOfLine = BufEndOfLine(buf, lineStartPos);
        if(textD->highlightCursorLine) {
            startOfLine = BufStartOfLine(buf, lineStartPos);
        }
    }
    
    /* Get the active ANSI color/style */
    int ansiS = -1;
    int ansiCharS = -1;
    ansiStyle newAnsiStyle = {-1, -1, -1, -1};
    ansiStyle ansi = {-1, -1, -1, -1};
    if(textD->ansiColors && lineStartPos >= 0) {
        findActiveAnsiStyle(textD, lineStartPos, &ansi);
    }
     
    /* Space beyond the end of the line is still counted in units of characters
       of a standardized character width (this is done mostly because style
       changes based on character position can still occur in this region due
       to rectangular selections).  stdCharWidth must be non-zero to prevent a
       potential infinite loop if x does not advance */
    stdCharWidth = textD->font->maxWidth;
    if (stdCharWidth <= 0) {
    	fprintf(stderr, "xnedit: Internal Error, bad font measurement\n");
    	NEditFree(lineStrFree);
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
    
    int rbEnd = lineLen;
    int rbCharIndex = 0;
    int rbPixelIndex = 0;
    
    inc = 1;
    for (charIndex = 0; ; charIndex+=inc) { 
        if(charIndex >= lineLen) {
            baseChar = '\0';   
            charLen = 1;
            inc = 1;
        } else {
            baseChar = lineStr[charIndex];
            const char *line = lineStr + charIndex;
            int remainingLen = lineLen - charIndex;
                    
            inc = getCharWidth(textD, line, &uc, remainingLen);
            if(inc > 1) {
                charLen = 1;
                expandedChar[0] = uc;
            } else {
                charLen = BufExpandCharacter4(lineStr[charIndex],
                        outIndex,
                        expandedChar,
                        buf->tabDist, buf->nullSubsChar);
            }
        }
        
    	style = styleOfPos(textD, lineStartPos, lineLen, charIndex,
                outIndex + dispIndexOffset, baseChar); 
        charWidth = charIndex >= lineLen
                ? stdCharWidth
                : charWidth4(
                        textD,
                        expandedChar,
                        charLen,
                        styleFL = styleFontList(textD, style));

    	if (x + charWidth >= leftClip && charIndex >= leftCharIndex) {
    	    startIndex = charIndex;
    	    outStartIndex = outIndex;
    	    startX = x;
            styleFont = FindFont(styleFL, uc);
    	    break;
    	}
        
        if(textD->ansiColors && baseChar == '\e') {
            newAnsiStyle.fg = -1;
            newAnsiStyle.bg = -1;
            newAnsiStyle.bold = -1;
            newAnsiStyle.italic = -1;
            parseEscapeSequence(buf, lineStartPos + charIndex, &newAnsiStyle);
            extendAnsiStyle(&ansi, &newAnsiStyle);
            ansiS = charIndex;
        }
        
        if(indentRainbow) {
            if(isspace(baseChar)) {
                if(baseChar == '\t') {
                    rbCharIndex += buf->tabDist - rbCharIndex % buf->tabDist;
                } else {
                    rbCharIndex++;
                }
            } else {
                rbEnd = charIndex;
            }
        }
        
    	x += charWidth;
    	outIndex += charLen;
    }
    
    rbPixelIndex = rbCharIndex / rbTabDist;
    if(rbEnd < startIndex) {
        indentRainbow = 0;
    }
    
    
    /* Set Xrender clipping to prevent text rendering beyond the line borders.
     * Sometimes with anti aliasing transparent pixels are above the glyph
     */
    if(textD->d) {
        XRectangle rect;
        rect.x = 0;
        rect.y = 0;
        rect.width = rightClip - leftClip;
        rect.height = textD->ascent + textD->descent;
        XftDrawSetClipRectangles(textD->d, leftClip, y, &rect, 1);
    }
    
    int rbCurrentPixelIndex = 0;
    if(!indentRainbow) {
        rbCurrentPixelIndex = -1;
        rbPixelIndex = -1;
    }
    
    /* check if the line contains the cursor
     */
    if(textD->highlightCursorLine && TextDRangeHasCursor(textD, startOfLine, endOfLine)) {
        cursorLine = True;
    }
    
    /* Scan character positions from the beginning of the clipping range, and
       draw parts whenever the style changes (also note if the cursor is on
       this line, and where it should be drawn to take advantage of the x
       position which we've gone to so much trouble to calculate) */
    outPtr = outStr;
    outIndex = outStartIndex;
    x = startX;
    inc = 1;
    for (charIndex = startIndex; charIndex < rightCharIndex; charIndex += inc) {
    	if (TextDPosHasCursor(textD, lineStartPos+charIndex, &cursorIndex)) {
    	    if (charIndex < lineLen
                    || (charIndex == lineLen && (lineStartPos+charIndex) >= buf->length)) {
    		hasCursor = True;
                cursorX[cursorNum].cursorX = x - 1;
                cursorX[cursorNum].index = cursorIndex;
                cursorNum++;
    	    } else if (charIndex == lineLen) {
    	    	if (wrapUsesCharacter(textD, (lineStartPos+charIndex))) {
    	    	    hasCursor = True;
    	    	    cursorX[cursorNum].cursorX = x - 1;
                    cursorX[cursorNum].index = cursorIndex;
                    cursorNum++;
    	    	}
    	    }
    	}
        
        int cpCharLen;
        if(charIndex >= lineLen) {
            baseChar = '\0';   
            charLen = 1;
            rbCurrentPixelIndex = -1;
            cpCharLen = -1;
        } else {
            baseChar = lineStr[charIndex];
            
            if(indentRainbow) {
                if(isspace(baseChar)) {
                    if(baseChar == '\t') {
                        rbCharIndex += buf->tabDist - rbCharIndex % buf->tabDist;
                    } else {
                        rbCharIndex++;
                    }
                    rbCurrentPixelIndex = ((rbCharIndex+rbTabDist-1) / rbTabDist)+textD->colorProfile->numRainbowColors -1;
                } else {
                    rbCurrentPixelIndex = -1;
                    indentRainbow = 0;
                    if(rbCharIndex % rbTabDist > 0) {
                        // don't highlight partial indention
                        rbPixelIndex = -2; // set to negative value but not -1
                    }
                }
            }
            
            inc = getCharWidth(textD, lineStr+charIndex, &uc, lineLen - charIndex);
            if(inc > 1) {
                if(uc != 0) {
                    charLen = 1;
                    expandedChar[0] = uc;
                } else {
                    charLen = 0;
                }
            } else {
                charLen = BufExpandCharacter4(lineStr[charIndex],
                        outIndex,
                        expandedChar,
                        buf->tabDist, buf->nullSubsChar);
            }
            cpCharLen = charLen;
        }
        
        if(textD->ansiColors && baseChar == '\e') {
            newAnsiStyle.fg = -1;
            newAnsiStyle.bg = -1;
            newAnsiStyle.bold = -1;
            newAnsiStyle.italic = -1;
            parseEscapeSequence(buf, lineStartPos + charIndex, &newAnsiStyle);
            ansiCharS = charIndex;
        }
        
   	charStyle = styleOfPos(textD, lineStartPos, lineLen, charIndex,
                outIndex + dispIndexOffset, baseChar);
        charFL = styleFontList(textD, charStyle);
        charFont = FindFont(charFL, uc);
        
        if (charStyle != style || charFont != styleFont || rbPixelIndex != rbCurrentPixelIndex || ansiS != ansiCharS) {
            drawString(textD, style, rbPixelIndex, startX, y, max(startX, leftClip), min(x, rightClip), outStr, outPtr - outStr, cursorLine, &ansi);
            outPtr = outStr;
            startX = x;
            style = charStyle;
            styleFont = charFont;
            rbPixelIndex = rbCurrentPixelIndex;
            extendAnsiStyle(&ansi, &newAnsiStyle);
    	}
        ansiS = ansiCharS;
        
        if(cpCharLen == 1) {
            *outPtr = *expandedChar;
            charWidth = charWidth4(textD, expandedChar, charLen, charFL);
        } else if(charIndex < lineLen) {
            memcpy(outPtr, expandedChar, sizeof(FcChar32)*charLen);
            charWidth = charWidth4(textD, expandedChar, charLen, charFL);
        } else {
            charWidth = stdCharWidth;
        }
        
        outPtr += charLen;
        x += charWidth;
        outIndex += charLen;
               
        if (outPtr - outStr + MAX_EXP_CHAR_LEN >= MAX_DISP_LINE_LEN
                || x >= rightClip) {
            break;
        }
    }
    extendAnsiStyle(&ansi, &newAnsiStyle);
    
    /* Draw the remaining style segment */
    drawString(textD, style, rbCurrentPixelIndex, startX, y, max(startX, leftClip), min(x, rightClip), outStr, outPtr - outStr, cursorLine, &ansi);
    
    /* Draw the cursor if part of it appeared on the redisplayed part of
       this line.  Also check for the cases which are not caught as the
       line is scanned above: when the cursor appears at the very end
       of the redisplayed section. */
    y_orig = textD->cursor->y;
    if (textD->cursorOn) {
        if (hasCursor) {
            for(int i=0;i<cursorNum;i++) {
                drawCursor(textD, cursorX[i].cursorX, y);
                cursorIndex = cursorX[i].index;
                if(cursorIndex >= 0) {
                    textD->multicursor[cursorIndex].x = cursorX[i].cursorX;
                    textD->multicursor[cursorIndex].y = y;
                }
            }
        } else if (charIndex < lineLen
                && TextDPosHasCursor(textD, lineStartPos+charIndex+1, &cursorIndex)
	    	&& x == rightClip) {
            if ((lineStartPos+charIndex+1) >= buf->length) {
    	    	drawCursor(textD, x - 1, y);
                if(cursorIndex >= 0) {
                    textD->multicursor[cursorIndex].x = x - 1;
                    textD->multicursor[cursorIndex].y = y;
                }
            } else {
                if (wrapUsesCharacter(textD, lineStartPos+charIndex+1)) {
    	    	    drawCursor(textD, x - 1, y);
                    if(cursorIndex >= 0) {
                        textD->multicursor[cursorIndex].x = x - 1;
                        textD->multicursor[cursorIndex].y = y;
                    }
                }
    	    }
        } else if (TextDPosHasCursor(textD, lineStartPos + rightCharIndex, &cursorIndex)) {
            drawCursor(textD, x - 1, y);
            if(cursorIndex >= 0) {
                textD->multicursor[cursorIndex].x = x - 1;
                textD->multicursor[cursorIndex].y = y;
            }
        }
    }
    
    // set the position where the input method might pop up
    if(hasCursor && textD->mcursorSizeReal == 1) {
        int cx = cursorX[0].cursorX;
        // xic position should the the cursor
        if(cx != textD->xic_x || y != textD->xic_y) {
            TextWidget textwidget = (TextWidget)textD->w;
            if(textwidget->text.xic) {
                // set x to the left edge of the cursor
                XPoint ipos;
                ipos.x = cx - (FontDefault(textD->font)->max_advance_width / 3);
                ipos.y = y;
                XVaNestedList xicvals;
                xicvals = XVaCreateNestedList(0, XNSpotLocation, &ipos, NULL);
                XSetICValues(textwidget->text.xic, XNPreeditAttributes, xicvals, NULL);
                XFree(xicvals);
                textD->xic_x = cx;
                textD->xic_y = y;
            } 
        }
    } 
    
    /* If the y position of the cursor has changed, redraw the calltip */
    if (hasCursor && (y_orig != textD->cursor->y || y_orig != y))
        TextDRedrawCalltip(textD, 0);
    
    NEditFree(lineStrFree);
    if(textD->mcursorSizeReal > 1) NEditFree(cursorX);
}

/*
** Draw a string or blank area according to parameter "style", using the
** appropriate colors and drawing method for that style, with top left
** corner at x, y.  If style says to draw text, use "string" as source of
** characters, and draw "nChars", if style is FILL, erase
** rectangle where text would have drawn from x to toX and from y to
** the maximum y extent of the current font(s).
*/
static void drawString(textDisp *textD, int style, int rbIndex, int x, int y, int fromX,
	int toX, FcChar32 *string, int nChars, Boolean highlightLine, ansiStyle *ansi)
{
    if(toX < fromX || nChars == 0) return;
    
    XftColor *gc = &textD->colorProfile->textFgColor;
    NFont *fontList = textD->font;
    XftColor *bground = &textD->colorProfile->textBgColor;
    XftColor *fground = &textD->colorProfile->textFgColor;
    int underlineStyle = FALSE;
    XftColor color = textD->colorProfile->textFgColor;
    
    /* Don't draw if widget isn't realized */
    if (XtWindow(textD->w) == 0)
    	return;
    
    if(nChars == 0) rbIndex = -1;
    
    /* select a GC */
    if (rbIndex >= 0 || style & (STYLE_LOOKUP_MASK | BACKLIGHT_MASK | RANGESET_MASK)) {
        gc = &textD->styleGC;
    }
    else if (style & HIGHLIGHT_MASK) {
        bground = &textD->colorProfile->hiliteBgColor;
        color = textD->colorProfile->hiliteFgColor;
    }
    else if (style & PRIMARY_MASK) {
        bground = &textD->colorProfile->selectBgColor;
        color = textD->colorProfile->selectFgColor;
    }
    else if (highlightLine && textD->highlightCursorLine) {
        bground = &textD->colorProfile->lineHiBgColor;
    }
    else {
        gc = &textD->colorProfile->textFgColor;
    }

    if (gc == &textD->styleGC) {
        /* we have work to do */
        styleTableEntry *styleRec;
        /* Set font, color, and gc depending on style.  For normal text, GCs
           for normal drawing, or drawing within a selection or highlight are
           pre-allocated and pre-configured.  For syntax highlighting, GCs are
           configured here, on the fly. */
        if (style & STYLE_LOOKUP_MASK) {
            styleRec = &textD->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A];
            underlineStyle = styleRec->underline;
            fontList = styleRec->font;
            //fground = styleRec->color;
            color = styleRec->color;
            /* here you could pick up specific select and highlight fground */
        }
        else {
            styleRec = NULL;
            fground = &textD->colorProfile->textFgColor;
        }     
        /* Background color priority order is:
           1 Primary(Selection), 2 Highlight(Parens),
           3 Rangeset, 4 SyntaxHighlightStyle,
           5 Backlight (if NOT fill), 6 DefaultBackground */
        bground =
            style & PRIMARY_MASK   ? &textD->colorProfile->selectBgColor :
            style & HIGHLIGHT_MASK ? &textD->colorProfile->hiliteBgColor :
            style & RANGESET_MASK  ?
                      getRangesetColor(textD,
                          (style&RANGESET_MASK)>>RANGESET_SHIFT,
                            bground) :
            rbIndex >= 0 ? &textD->colorProfile->rainbowColors[rbIndex%textD->colorProfile->numRainbowColors] : 
            styleRec && styleRec->bgColorName ? &styleRec->bgColor :
            (style & BACKLIGHT_MASK) && !(style & FILL_MASK) ?
                      &textD->bgClassPixel[(style>>BACKLIGHT_SHIFT) & 0xff] :
            &textD->colorProfile->textBgColor;
        if (fground == bground) /* B&W kludge */
            fground = &textD->colorProfile->textBgColor;
        /* set up gc for clearing using the foreground color entry */
        
        if((bground == &textD->colorProfile->textBgColor || rbIndex >= 0) && highlightLine && textD->highlightCursorLine) {
            bground = &textD->colorProfile->lineHiBgColor;
        }
    }
     
    /* Set ANSI color */
    XftColor ansiBGColor;
    if(ansi->fg > 0) {
        ansiFgToColorIndex(textD, ansi->fg, &color);
    }
    if(ansi->bg > 0 && bground == &textD->colorProfile->textBgColor) {
        ansiBgToColorIndex(textD, ansi->bg, &ansiBGColor);
        bground = &ansiBGColor;
    }
    if(ansi->bold > 0 || ansi->italic > 0) {
        if(ansi->bold == ansi->italic) {
            fontList = textD->boldItalicFont;
        } else if(ansi->bold == 1) {
            fontList = textD->boldFont;
        } else {
            fontList = textD->italicFont;
        }
    }
    

    /* Always draw blank area, because Xft AA text rendering needs a clean
     * background */
       
    /* wipes out to right hand edge of widget */
    if (toX >= textD->left) {
        clearRect(
                textD,
                bground,
                fromX,
                y,
                toX - fromX,
                textD->ascent + textD->descent);
    }
    if(style & FILL_MASK) {
        return;
    }

    
    /* We assume the string should be rendered with just one font, because
     * redisplayLine breaks the strings when a different font is required.
     * The first character in the string determines the charset and FindFont
     * returns a Font for this.
     */
    XftFont *font = FindFont(fontList, string[0]);
    
    /* If any space around the character remains unfilled (due to use of
       different sized fonts for highlighting), fill in above or below
       to erase previously drawn characters */
    if (font->ascent < textD->ascent)
    	clearRect(textD, bground, fromX, y, toX - fromX, textD->ascent - font->ascent);
    if (font->descent < textD->descent)
    	clearRect(textD, bground, fromX, y + textD->ascent + font->descent, toX - fromX,
    		textD->descent - font->descent);
    
    
    

    /* Draw the string using color and font set above */  
    XftDrawString32(textD->d, &color, font, x, y + textD->ascent, string, nChars);
        
    /* Underline if style is secondary selection */
    if (style & SECONDARY_MASK || underlineStyle)
    {
        /* draw underline */
    	//XDrawLine(XtDisplay(textD->w), XtWindow(textD->w), gc, x,
    	//    	y + textD->ascent, toX - 1, y + textD->ascent);
        XftDrawRect(textD->d, fground, x, y + textD->ascent, toX - 1, 1);
    }
}

/*
** Clear a rectangle with the appropriate background color for "style"
*/
static void clearRect(textDisp *textD, XftColor *color, int x, int y, 
        int width, int height)
{
    /* A width of zero means "clear to end of window" to XClearArea */
    if (width == 0 || XtWindow(textD->w) == 0)
    	return;
    
    if (color == &textD->colorProfile->textBgColor) {
        XClearArea(XtDisplay(textD->w), XtWindow(textD->w), x, y,
                width, height, False);
    }
    else {
        XftDrawRect(textD->d, color, x, y, width, height);
    }

    if(textD->rightMarginPos > 0 && textD->rightMarginPos >= x) {
        XftDrawRect(textD->d, &textD->colorProfile->rborderColor, textD->rightMarginPos, y, 1, height);
        
        // draw remaining right area using bg2
        /*
        int bg2width = x + width - textD->rightMarginPos;
        XftDrawRect(textD->d, &textD->colorProfile->textBg2Color, textD->rightMarginPos + 1, y, bg2width, height);
        */
    }
}

/*
** Draw a cursor with top center at x, y.
*/
static void drawCursor(textDisp *textD, int x, int y)
{
    XSegment segs[5];
    int left, right, cursorWidth, midY;
    int fontWidth = textD->font->minWidth;
    int fontHeight = textD->ascent + textD->descent;
    int nSegs = 0;
    int bot = y + fontHeight - 1;
    
    if (XtWindow(textD->w) == 0 || x < textD->left-1 ||
	    x > textD->left + textD->width)
    	return;
    
    /* For cursors other than the block, make them around 2/3 of a character
       width, rounded to an even number of pixels so that X will draw an
       odd number centered on the stem at x. */
    cursorWidth = (fontWidth/3) * 2;
    
    // simplify the cursor in multicursor mode
    if(textD->mcursorOn && textD->cursorStyle != CARET_CURSOR && textD->cursorStyle != BLOCK_CURSOR) {
        cursorWidth = 0;
    }
    
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
    textD->cursor->x = x;
    textD->cursor->y = y;
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
static int charWidth4(const textDisp* textD, const FcChar32* string,
        int length, NFont *fontList)
{ 
    if(string[0] == 0) return 0; // special case for ansi coloring
    
    int strWidth = 0;
    if(fontList->minWidth == fontList->maxWidth && string[0] < 128) {
        strWidth += fontList->minWidth * length;
    } else {
        // main font is not a monospace font or character is not ascii
        XftFont *font = FindFont(fontList, string[0]);
        XGlyphInfo extents;
        XftTextExtents32(XtDisplay(textD->w), font, string, length, &extents);
        strWidth = extents.xOff;
    }
    
    return strWidth;
}

static NFont* styleFontList(const textDisp* textD, int style)
{
    NFont *font;
    if (style & STYLE_LOOKUP_MASK)
    	font = textD->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A].font;
    else 
    	font = textD->font;
    return font;
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
    int charWidth, charLen, charStyle, visLineNum, xStep, outIndex, inc;
    FcChar32 expandedChar[MAX_EXP_CHAR_LEN];
    FcChar32 uc = 0;
    NFont *font;
    
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
    char *lineStrAlloc;
    const char *lineStr = BufGetRange2(textD->buffer, lineStart, lineStart + lineLen, &lineStrAlloc);
    
    /* Step through character positions from the beginning of the line
       to find the character position corresponding to the x coordinate */
    xStep = textD->left - textD->horizOffset;
    outIndex = 0;
    inc = 1;
    for(charIndex=0; charIndex<lineLen; charIndex+=inc) {
        inc = getCharWidth(textD, lineStr+charIndex, &uc, lineLen-charIndex);
        if(inc > 1) {
            /* not ascii */
            charLen = 1;
            expandedChar[0] = uc;
        } else {
            charLen = BufExpandCharacter4(lineStr[charIndex],
                        outIndex,
                        expandedChar,
                        textD->buffer->tabDist, textD->buffer->nullSubsChar);
        }
        
   	charStyle = styleOfPos(textD, lineStart, lineLen, charIndex, outIndex,
				lineStr[charIndex]);
        font = styleFontList(textD, charStyle);
    	charWidth = charWidth4(textD, expandedChar, charLen, font);
    	if (x < xStep + (posType == CURSOR_POS ? charWidth/2 : charWidth)) {
    	    NEditFree(lineStrAlloc);
    	    return lineStart + charIndex;
    	}
    	xStep += charWidth;
    	outIndex += charLen;
    }
    
    /* If the x position was beyond the end of the line, return the position
       of the newline at the end of the line */
    NEditFree(lineStrAlloc);
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
    int fontWidth = textD->font->maxWidth;

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
    //if (textD->visibility != VisibilityUnobscured ||
    //        abs(xOffset) > textD->width || abs(yOffset) > exactHeight) {
    if(1) {
        // WORKAROUND for hscroll bug with new xft rendering
        // the code with XCopyArea doesn't work, so we always redisplay
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
        int left, right;
        TextDCursorLR(textD, &left, &right);
        textDRedisplayRange(textD, left, right);
    }
    
    /* Refresh line number/calltip display if its up and we've scrolled 
        vertically */
    if (lineDelta != 0) {
        redrawLineNumbers(textD, textD->top, textD->height, True);
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
static void redrawLineNumbers(textDisp *textD, int top, int height, int clearAll)
{
    int y, line, visLine, nCols, lineStart;
    char lineNumString[12];
    int lineHeight = textD->ascent + textD->descent;
    int charWidth = textD->font->maxWidth;
    XRectangle clipRect;
    
    /* Don't draw if lineNumWidth == 0 (line numbers are hidden), or widget is
       not yet realized */
    if (textD->lineNumWidth == 0 || XtWindow(textD->w) == 0)
        return;
    
    /* Make sure we reset the clipping range for the line numbers GC, because
       the GC may be shared (eg, if the line numbers and text have the same
       color) and therefore the clipping ranges may be invalid. */
    clipRect.x = 0; //textD->lineNumLeft;
    clipRect.y = 0; //textD->top;
    clipRect.width = textD->lineNumWidth + textD->lineNumLeft;
    clipRect.height = textD->height + 2*textD->top;
    //XSetClipRectangles(display, textD->lineNumGC, 0, 0,
    //	    &clipRect, 1, Unsorted);
    if(textD->d) {
        XftDrawSetClipRectangles(textD->d, 0, 0, &clipRect, 1);
    }
    
    /* Erase the previous contents of the line number area, if requested */
    if (clearAll) {
        //XClearArea(XtDisplay(textD->w), XtWindow(textD->w), textD->lineNumLeft,
        //        textD->top, textD->lineNumWidth, textD->height, False);
        XftDrawRect(
                textD->d,
                &textD->colorProfile->lineNoBgColor,
                0,
                0,
                textD->lineNumLeft + textD->lineNumWidth,
                2*top + height);
    }
    
    /* Draw the line numbers, aligned to the text */
    nCols = min(11, textD->lineNumWidth / charWidth);
    y = textD->top;
    line = getAbsTopLineNum(textD);
    for (visLine=0; visLine < textD->nVisibleLines; visLine++) {
        lineStart = textD->lineStarts[visLine];
        if (lineStart != -1 && (lineStart==0 ||
                BufGetCharacter(textD->buffer, lineStart-1)=='\n')) {
            snprintf(lineNumString, 12, "%*d", nCols, line);
            XftDrawString8(
                    textD->d,
                    &textD->colorProfile->lineNoFgColor,
                    FontDefault(textD->font),
                    textD->lineNumLeft,
                    y + textD->ascent,
                    (FcChar8*)lineNumString,
                    strlen(lineNumString));           
            line++;
        } else {
            /*
            XClearArea(XtDisplay(textD->w), XtWindow(textD->w),
                    textD->lineNumLeft, y, textD->lineNumWidth,
                    textD->ascent + textD->descent, False);
            */
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
    textBuffer *buf = textD->buffer;
    int i, width = 0, style, lineLen = visLineLength(textD, visLineNum);
    int lineStartPos = textD->lineStarts[visLineNum];
    char *free_lineStr;
    const char *lineStr = BufGetRange2(buf, lineStartPos, lineStartPos + lineLen, &free_lineStr);
    FcChar32 expandedChar[MAX_EXP_CHAR_LEN];
    FcChar32 uc;
    int inc;
    int charLen;
    unsigned short indent = 0;
    NFont *font;
    
    for(i=0;i<lineLen;i+=inc) {
        inc = getCharWidth(textD, lineStr+i, &uc, lineLen - i);
        if(inc > 1) {
            charLen = 1;
            expandedChar[0] = uc;
            indent += inc;
        } else {
            charLen = BufExpandCharacter4(lineStr[i],
                    indent,
                    expandedChar,
                    buf->tabDist, buf->nullSubsChar);
            indent += charLen;
        }
        
        if (textD->styleBuffer) {
            style = (unsigned char)BufGetCharacter(textD->styleBuffer,
		    lineStartPos+i) - ASCII_A;
            font = textD->styleTable[style].font;
        } else {
            font = textD->font;
        }
        
        width += charWidth4(textD, expandedChar, charLen, font);
    }
    NEditFree(free_lineStr);
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
static void blankSingleCursorProtrusions(textDisp *textD)
{
    int x, width, cursorX = textD->cursor->x, cursorY = textD->cursor->y;
    int fontWidth = textD->font->maxWidth; //FontDefault(textD->font)->max_advance_width;
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

static void blankCursorProtrusions(textDisp *textD) {
    if(textD->mcursorSize == 1) {
        blankSingleCursorProtrusions(textD);
    } else {
        textCursor *origCursor = textD->cursor;
        for(int i=0;i<textD->mcursorSize;i++) {
            textD->cursor = textD->multicursor + i;
            blankSingleCursorProtrusions(textD);
        }
        textD->cursor = origCursor;
    }
}

/*
** Allocate shared graphics contexts used by the widget, which must be
** re-allocated on a font change.
*/
static void allocateFixedFontGCs(textDisp *textD, Pixel bgPixel, Pixel fgPixel)
{
    textD->gc = allocateGC(textD->w, GCForeground | GCBackground,
            fgPixel, bgPixel, 0, GCClipMask, GCArcMode); 
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
    
    if(textD->d) {
        XftDrawSetClipRectangles(textD->d, 0, 0, &clipRect, 1);
    }
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
static int findWrapRange(textDisp *textD, const char *deletedText, int pos,
    	int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd,
    	int *linesInserted, int *linesDeleted)
{
    int length, retPos, retLines, retLineStart, retLineEnd;
    textBuffer *deletedTextBuf, *buf = textD->buffer;
    int nVisLines = textD->nVisibleLines;
    int *lineStarts = textD->lineStarts;
    int countFrom, countTo, lineStart, adjLineStart, i;
    int visLineNum = 0, nLines = 0;
    int nl = 0;
    
    /*
    ** Determine where to begin searching: either the previous newline, or
    ** if possible, limit to the start of the (original) previous displayed
    ** line, using information from the existing line starts array
    */
    if (pos >= textD->firstChar && pos <= textD->lastChar) {
    	for (i=nVisLines-1; i>0; i--) { 
            if (lineStarts[i] != -1 && pos >= lineStarts[i])
    		break;
        }   
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
    	    	&retPos, &retLines, &retLineStart, &retLineEnd, NULL);
        if(pos == retLineEnd) {
            nl = 1;
        }
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
        return nl;
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
	    countFrom, &retPos, &retLines, &retLineStart, &retLineEnd,
            NULL);
    BufFree(deletedTextBuf);
    *linesDeleted = retLines;
    textD->suppressResync = 0;
    
    return nl;
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
    	    	&retPos, &retLines, &retLineStart, &retLineEnd, NULL);
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
**   retWrap        Was any line wrapped
*/
static void wrappedLineCounter(const textDisp* textD, const textBuffer* buf,
        int startPos, int maxPos, int maxLines,
        Boolean startPosIsLineStart, int styleBufOffset,
        int* retPos, int* retLines, int* retLineStart, int* retLineEnd,
        Boolean *retWrap)
{
    int lineStart, newLineStart = 0, b, p, colNum, wrapMargin;
    int maxWidth, width, countPixels, i, foundBreak;
    int nLines = 0, tabDist = textD->buffer->tabDist;
    FcChar32 c;
    char nullSubsChar = textD->buffer->nullSubsChar;
    if(retWrap) {
        *retWrap = False;
    }
    
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
    int inc = 1;
    for (p=lineStart; p<buf->length; p+=inc) {
        c = getCharacter32(textD, buf, p, &inc);
        
    	/* If the character was a newline, count the line and start over,
    	   otherwise, add it to the width and column counts */
    	if ((char)c == '\n') {
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
    	} else if(c != 0) {
    	    colNum += BufCharWidth((char)c, colNum, tabDist, nullSubsChar);
    	    if (countPixels)
    	    	width += measurePropChar(textD, c, colNum, p+styleBufOffset);
    	} // else: c == 0 => invisible escape sequence

    	/* If character exceeded wrap margin, find the break point
    	   and wrap there */
    	if (colNum > wrapMargin || width > maxWidth) {
            if(retWrap) {
                *retWrap = True;
                retWrap = NULL;
            }
            
    	    foundBreak = False;
            /* TODO: implement unicode word boundary */
    	    for (b=p; b>=lineStart; b--) {
                c = BufGetCharacter(buf, b);
    	    	if ((char)c == '\t' || (char)c == ' ') {
    	    	    newLineStart = b + 1;
    	    	    if (countPixels) {
    	    	    	colNum = 0;
    	    	    	width = 0;
                        int charLen;
    	    	    	for (i=b+1; i<p+1; i+=charLen) {
    	    	    	    width += measurePropChar(textD,
				    getCharacter32(textD, buf, i, &charLen), colNum, 
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
    	    	colNum = BufCharWidth((char)c, colNum, tabDist, nullSubsChar);
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
    return;
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
static int measurePropChar(const textDisp* textD, FcChar32 c,
    int colNum, int pos)
{
    int style;
    textBuffer *styleBuf = textD->styleBuffer;
    
    NFont *font = NULL;
    if (styleBuf) {
	style = (unsigned char)BufGetCharacter(styleBuf, pos);
	if (style == textD->unfinishedStyle) {
    	    /* encountered "unfinished" style, trigger parsing */
    	    (textD->unfinishedHighlightCB)(textD, pos, textD->highlightCBArg);
    	    style = (unsigned char)BufGetCharacter(styleBuf, pos);
	}
        if (style & STYLE_LOOKUP_MASK) {
            font = textD->styleTable[(style & STYLE_LOOKUP_MASK) - ASCII_A].font;
        }
    }
    if(!font) {
        font = textD->font;
    }
    
    int charLen;
    FcChar32 expChar[MAX_EXP_CHAR_LEN];
    if(c < 128) {
        charLen = BufExpandCharacter4(c, colNum, expChar, 
	    textD->buffer->tabDist, textD->buffer->nullSubsChar);
        
        if(font->minWidth == font->maxWidth) {
            return font->minWidth;
        } else {
            XGlyphInfo extents;
            XftTextExtents32(XtDisplay(textD->w), font->fonts->font, expChar, charLen, &extents);
            return extents.xOff;
        }
    } else {
        charLen = 1;
        expChar[0] = c;
    }
    
    return charWidth4(textD, expChar, charLen, font);
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
    	    &retLineStart, lineEnd, NULL);
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
    	    FontDefault(textD->font)->max_advance_width < textD->width))
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
static XftColor allocBGColor(Widget w, char *colorName, int *ok)
{
    int r,g,b;
    *ok = 1;
    XftColor color;
    color.pixel = AllocColor(w, colorName, &r, &g, &b);
    color.color.red = r;
    color.color.green = g;
    color.color.blue = b;
    color.color.alpha = 0xFFFF;
    return color;
}

static XftColor* getRangesetColor(textDisp *textD, int ind, XftColor *bground)
{
    textBuffer *buf;
    RangesetTable *tab;
    XftColor color;
    char *color_name;
    int valid;

    if (ind > 0) {
      ind--;
      buf = textD->buffer;
      tab = buf->rangesetTable;
      
      Rangeset *rangeset = NULL;
      valid = RangesetTableGetColorValid(tab, ind, &color, &rangeset);
      if (valid == 0) {
          color_name = RangesetTableGetColorName(tab, ind);
          if (color_name)
              color = allocBGColor(textD->w, color_name, &valid);
          rangeset = RangesetTableAssignColorPixel(tab, ind, color, valid);
      }
      if (valid > 0) {
          return RangesetGetColor(rangeset);
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
void TextDSetupBGClasses(Widget w, XmString str, XftColor **pp_bgClassPixel,
      unsigned char **pp_bgClass, XftColor bgPixelDefault)
{
    unsigned char bgClass[256];
    XftColor bgClassPixel[256];
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
    *pp_bgClassPixel = (XftColor *)NEditMalloc(class_no * sizeof (XftColor));
    if (!*pp_bgClass || !*pp_bgClassPixel) {
        NEditFree(*pp_bgClass);
        NEditFree(*pp_bgClassPixel);
        return;
    }
    memcpy(*pp_bgClass, bgClass, 256);
    memcpy(*pp_bgClassPixel, bgClassPixel, class_no * sizeof (XftColor));
}


void TextDSetHighlightCursorLine(textDisp *textD, Boolean state)
{
    textD->highlightCursorLine = state;
}

void TextDSetIndentRainbow(textDisp *textD, Boolean indentRainbow)
{
    textD->indentRainbow = indentRainbow;
}


#define ANSI_ESC_MAX_PARAM 16
#define ANSI_ESC_MAX_PARAM_LEN 4

#define ANSI_ESC_RESET        0
#define ANSI_ESC_BOLD         1
#define ANSI_ESC_ITALIC       3
#define ANSI_ESC_RESET_BOLD   22
#define ANSI_ESC_RESET_ITALIC 23

#define ANSI_STYLE_SET(style, value) if(style < 0) style = value 

/* Parses an ANSI Escape Sequence and sets the style in the ansiStyle object
 * Returns 1 if all possible settings are set
 */
static int parseEscapeSequence(textBuffer *buf, size_t pos, ansiStyle *style)
{
    char c1 = BufGetCharacter(buf, pos);
    char c2 = BufGetCharacter(buf, pos+1);
    if(c1 != '\e' || c2 != '[') return 0;
    
    int param[ANSI_ESC_MAX_PARAM];
    int nparam = 0;
    
    // parse the escape sequence into an array of integer parameters
    char paramDig[ANSI_ESC_MAX_PARAM_LEN];
    int paramDigLen = 0;
    
    size_t i = pos+2;
    while(nparam < ANSI_ESC_MAX_PARAM) {
        char c = BufGetCharacter(buf, i++);
        if(c >= '0' && c <= '9') {
            if(paramDigLen >= ANSI_ESC_MAX_PARAM_LEN) break;
            paramDig[paramDigLen++] = c - '0';
        } else {
            int iParam = 0;
            int mul = 1;
            for(int j=paramDigLen-1;j>=0;j--) {
                iParam += paramDig[j] * mul;
                mul *= 10;
            }

            param[nparam++] = iParam;
            
            paramDigLen = 0;
            if(c != ';') break;
        }
    }
    
    // evaluate parameter
    for(int k=0;k<nparam;k++) {
        int p = param[k];
        
        switch(p) {
            case ANSI_ESC_RESET: {
                ANSI_STYLE_SET(style->fg, 0);
                ANSI_STYLE_SET(style->bg, 0);
                ANSI_STYLE_SET(style->bold, 0);
                ANSI_STYLE_SET(style->italic, 0);
                k = nparam; // terminate loop
                break;
            }
            case ANSI_ESC_BOLD: {
                ANSI_STYLE_SET(style->bold, 1);
                break;
            }
            case ANSI_ESC_ITALIC: {
                ANSI_STYLE_SET(style->italic, 1);
                break;
            }
            case ANSI_ESC_RESET_BOLD: {
                ANSI_STYLE_SET(style->bold, 0);
                break;
            }
            case ANSI_ESC_RESET_ITALIC: {
                ANSI_STYLE_SET(style->bold, 0);
                break;
            }
            default: {
                if((p >= 30 && p <= 39) || (p >= 90 && p <= 97)) {
                    ANSI_STYLE_SET(style->fg, p);
                } else if((p >= 40 && p <= 49) || (p >= 100 && p <= 107)) {
                    ANSI_STYLE_SET(style->bg, p);
                }
                break;
            }
        }
    }
    
    // check if all style options are set
    if(style->fg != -1 && style->bg != -1 && style->bold != -1 && style->italic) {
        return 1;
    }
    
    
    return 0;
}

static void extendAnsiStyle(ansiStyle *style, ansiStyle *ext)
{
    if(ext->fg >= 0) {
        style->fg = ext->fg;
    }
    if(ext->bg >= 0) {
        style->bg = ext->bg;
    }
    if(ext->bold >= 0) style->bold = ext->bold;
    if(ext->italic >= 0) style->italic = ext->italic;
}

static void findActiveAnsiStyle(textDisp *textD, ssize_t pos, ansiStyle *style)
{
    textBuffer *buf = textD->buffer;
    ssize_t prev_esc;       // index of previous escape sequence
    size_t prev_esc_pos;    // absolute position of previous esc
    BufEscPos2Index(buf, 0, 0, pos, &prev_esc, &prev_esc_pos);
    if(prev_esc < 0) {
        return;
    }
    
    for(ssize_t i=prev_esc;i>=0;i--) {
        if(parseEscapeSequence(buf, prev_esc_pos, style)) {
            break;
        }
        prev_esc_pos -= buf->ansi_escpos[i]; // subtract offset to previous escape sequence
    }
}

static void ansiFgToColorIndex(textDisp *textD, short fg, XftColor *color)
{
    if(fg >= 30 && fg <= 37) {
        *color = textD->colorProfile->ansiColors[fg-30];
    } else if(fg >= 90 && fg <= 97) {
        *color = textD->colorProfile->ansiColors[fg-90];
    }
}

static void ansiBgToColorIndex(textDisp *textD, short bg, XftColor *color)
{
     if(bg >= 40 && bg <= 47) {
        *color = textD->colorProfile->ansiColors[bg-40];
    } else if(bg >= 100 && bg <= 107) {
        *color = textD->colorProfile->ansiColors[bg-100];
    }
}

/* font list functions */

#ifdef EXCLUDE_FONTS
static char **font_excludes;
static size_t num_font_excludes = 0;

void FontInitExcludes(const char *list) {
    size_t num = 1;
    size_t len = strlen(list);
    if(len == 0) {
        return;
    }
    // how many elements in the fonts list
    for(int i=0;i<len;i++) {
        if(list[i] == ',') {
            num++;
        }
    }
    
    font_excludes = calloc(num, sizeof(char*));
    num_font_excludes = num;
    
    char *str = strdup(list);
    font_excludes[0] = str;
    
    int e = 0;
    for(int i=0;i<len;i++) {
        if(list[i] == ',') {
            str[i] = '\0';
            font_excludes[++e] = str+i+1;
        }
    }
    
    // trim font names
    for(int i=0;i<num_font_excludes;i++) {
        char *font = font_excludes[i];
        while(*font > 0 && isspace(*font)) {
            font++;
        }
        size_t font_len = strlen(font);
        while(font_len > 0 && isspace(font[font_len-1])) {
            font_len--;
            font[font_len] = '\0';
        }
        font_excludes[i] = font;
        //printf("Exclude Font: %s\n", font);
    }
}


#endif

static void getFontMinMax(NFont *font, int *min, int *max) {
    XftFont *xftFont = FontDefault(font);
    int fontMin = xftFont->max_advance_width;
    int fontMax = 0;
    
    for(int i=32;i<127;i++) {
        XGlyphInfo extents;
        FcChar8 c = i;
        XftTextExtents8(font->display, xftFont, &c, 1, &extents);
        if(extents.xOff < fontMin) {
            fontMin = extents.xOff;
        }
        if(extents.xOff > fontMax) {
            fontMax = extents.xOff;
        }
    }
    
    *min = fontMin;
    *max = fontMax;
}

NFont *FontCreate(Display *dp, FcPattern *pattern)
{
    if(!pattern) {
        return NULL;
    }
    FcResult result;
    pattern = FcPatternDuplicate(pattern);
    FcPattern *match = XftFontMatch(dp, DefaultScreen(dp), pattern, &result);
    
    double sz = 0;
    result = FcPatternGetDouble (pattern, FC_SIZE, 0, &sz);
    if(result != FcResultMatch) {
        FcPatternGetDouble (match, FC_SIZE, 0, &sz);
    }
    
    XftFont *defaultFont = XftFontOpenPattern(dp, match);
    if(!defaultFont) {
        FcPatternDestroy(match);
        return NULL;
    }
    
    NFont *font = NEditMalloc(sizeof(NFont));
    font->display = dp;
    font->pattern = pattern;
    font->fail = NULL;
    font->size = sz;
    font->ref = 1;

    NFontList *list = NEditMalloc(sizeof(NFontList));
    list->font = defaultFont;
    list->next = NULL;
    font->fonts = list;
    
    getFontMinMax(font, &font->minWidth, &font->maxWidth);
    
    return font;
}

NFont *FontFromName(Display *dp, const char *name)
{
    FcPattern *pattern = FcNameParse((FcChar8*)name);
    if(!pattern) {
        return NULL;
    }
    NFont *font = FontCreate(dp, pattern);
    FcPatternDestroy(pattern);
    return font;
}

XftFont *FontListAddFontForChar(NFont *f, FcChar32 c)
{
    /* charset for char c */
    FcCharSet *charset = FcCharSetCreate();
    FcValue value;
    value.type = FcTypeCharSet;
    value.u.c = charset;
    FcCharSetAddChar(charset, c);
    if(!FcCharSetHasChar(charset, c)) {
        FcCharSetDestroy(charset);
        return f->fonts->font;
    }
    
    /* font lookup based on the NFont pattern */ 
    FcPattern *pattern = FcPatternDuplicate(f->pattern);
    FcPatternAdd(pattern, FC_CHARSET, value, 0);
    FcResult result;
    FcPattern *match = XftFontMatch (
            f->display, DefaultScreen(f->display), pattern, &result);
    if(!match) {
        FcPatternDestroy(pattern);
        FontAddFail(f, charset);
        return f->fonts->font;
    }
    
#ifdef EXCLUDE_FONTS
    char *name = NULL;
    FcPatternGetString(match, FC_FULLNAME, 0, (FcChar8**)&name);
    if(name) {
        for(int i=0;i<num_font_excludes;i++) {
            if(!strcmp(name, font_excludes[i])) {
                return f->fonts->font;
            }
        }
    }
#endif
    
    XftFont *newFont = XftFontOpenPattern(f->display, match);   
    if(!newFont || !FcCharSetHasChar(newFont->charset, c)) {
        FcPatternDestroy(pattern);
        FcPatternDestroy(match);
        if(newFont) {
            XftFontClose(f->display, newFont);
        }
        FontAddFail(f, charset);
        return f->fonts->font;
    }
    
    FcCharSetDestroy(charset);
    
    NFontList *newElm = NEditMalloc(sizeof(NFontList));
    newElm->font = newFont;
    newElm->next = NULL;
    
    NFontList *elm = f->fonts;
    NFontList *last = NULL;
    while(elm) {
        last = elm;
        elm = elm->next;
    }
    last->next = newElm;
    
    
    return newFont;
}

XftFont *FindFont(NFont *f, FcChar32 c)
{
    if(c < 128) {
        return f->fonts->font;
    }
    
    /* make sure the char is not in the fail list, because we don't
     * want to retry font lookups */
    NCharSetList *fail = f->fail;
    while(fail) {
        if(FcCharSetHasChar(fail->charset, c)) {
            return f->fonts->font;
        }
        fail = fail->next;
        
    }
    
    /* find a font that has this char */
    NFontList *elm = f->fonts;
    while(elm) {
        if(FcCharSetHasChar(elm->font->charset, c)) {
            return elm->font;
        }
        elm = elm->next;
    }
    
    /* open a new font for this char */
    return FontListAddFontForChar(f, c);
}

XftFont *FontDefault(NFont *f) {
    return f->fonts->font;
}

void FontAddFail(NFont *f, FcCharSet *c)
{
    NCharSetList *elm = f->fail;
    NCharSetList *last = elm;
    while(elm) {
        last = elm;
        elm = elm->next;
    }
    
    NCharSetList *newElm = NEditMalloc(sizeof(NCharSetList));
    newElm->charset = c;
    newElm->next = NULL;
    if(last) {
        last->next = newElm;
    } else {
        f->fail = newElm;
    }
}

void FontDestroy(NFont *f)
{
    NCharSetList *c = f->fail;
    NCharSetList *nc;
    while(c) {
        FcCharSetDestroy(c->charset);
        nc = c->next;
        NEditFree(c);
        c = nc;
    }
    
    NFontList *l = f->fonts;
    NFontList *nl;
    while(l) {
        XftFontClose(f->display, l->font);
        nl = l->next;
        NEditFree(l);
        l = nl;
    }
    
    FcPatternDestroy(f->pattern);
    NEditFree(f);
}

NFont *FontRef(NFont *font)  {
    font->ref++;
    return font;
}

void FontUnref(NFont *font) {
    if(--font->ref == 0) {
        FontDestroy(font);
    }
}
