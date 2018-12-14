/*******************************************************************************
*                                                                              *
* textDisp.h -- Nirvana Editor Text Diplay Header File                         *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef NEDIT_TEXTDISP_H_INCLUDED
#define NEDIT_TEXTDISP_H_INCLUDED

#include "textBuf.h"

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <Xm/Xm.h>

enum cursorStyles {NORMAL_CURSOR, CARET_CURSOR, DIM_CURSOR, BLOCK_CURSOR,
	HEAVY_CURSOR};

#define NO_HINT -1

typedef struct {
    char *highlightName;
    char *styleName;
    char *colorName;
    char isBold;
    char isItalic;
    unsigned short red;
    unsigned short green;
    unsigned short blue;
    Pixel color;
    Boolean underline;
    XFontStruct *font;
    char *bgColorName;      /* background style coloring (name may be NULL) */
    unsigned short bgRed;
    unsigned short bgGreen;
    unsigned short bgBlue;
    Pixel bgColor;
} styleTableEntry;

typedef struct graphicExposeTranslationEntry {
    int horizontal;
    int vertical;
    struct graphicExposeTranslationEntry *next;
} graphicExposeTranslationEntry;

typedef void (*unfinishedStyleCBProc)();

typedef struct _calltipStruct {
    int ID;                 /* ID of displayed calltip.  Equals
                              zero if none is displayed. */
    Boolean anchored;       /* Is it anchored to a position */
    int pos;                /* Position tip is anchored to */
    int hAlign;             /* horizontal alignment */
    int vAlign;             /* vertical alignment */
    int alignMode;          /* Strict or sloppy alignment */
} calltipStruct;

typedef struct _textDisp {
    Widget w;
    int top, left, width, height, lineNumLeft, lineNumWidth;
    int cursorPos;
    int cursorOn;
    int cursorX, cursorY;		/* X, Y pos. of last drawn cursor 
                                            Note: these are used for *drawing*
                                            and are not generally reliable
                                            for finding the insert position's
                                            x/y coordinates! */
    int cursorToHint;			/* Tells the buffer modified callback
    					   where to move the cursor, to reduce
    					   the number of redraw calls */
    int cursorStyle;			/* One of enum cursorStyles above */
    int cursorPreferredCol;		/* Column for vert. cursor movement */
    int nVisibleLines;			/* # of visible (displayed) lines */
    int nBufferLines;			/* # of newlines in the buffer */
    textBuffer *buffer;     	    	/* Contains text to be displayed */
    textBuffer *styleBuffer;   	    	/* Optional parallel buffer containing
    	    	    	    	    	   color and font information */
    int firstChar, lastChar;		/* Buffer positions of first and last
    					   displayed character (lastChar points
    					   either to a newline or one character
    					   beyond the end of the buffer) */
    int continuousWrap;     	    	/* Wrap long lines when displaying */
    int wrapMargin; 	    	    	/* Margin in # of char positions for
    	    	    	    	    	   wrapping in continuousWrap mode */
    int *lineStarts;
    int topLineNum;			/* Line number of top displayed line
    					   of file (first line of file is 1) */
    int absTopLineNum;			/* In continuous wrap mode, the line
    					   number of the top line if the text
					   were not wrapped (note that this is
					   only maintained as needed). */
    int needAbsTopLineNum;		/* Externally settable flag to continue
    					   maintaining absTopLineNum even if
					   it isn't needed for line # display */
    int horizOffset;			/* Horizontal scroll pos. in pixels */
    int visibility;        /* Window visibility (see XVisibility event) */
    int nStyles;			/* Number of entries in styleTable */
    styleTableEntry *styleTable;    	/* Table of fonts and colors for
    	    	    	    	    	   coloring/syntax-highlighting */
    char unfinishedStyle;   	    	/* Style buffer entry which triggers
    	    	    	    	    	   on-the-fly reparsing of region */
    unfinishedStyleCBProc		/* Callback to parse "unfinished" */
    	    unfinishedHighlightCB;  	/*     regions */
    void *highlightCBArg;   	    	/* Arg to unfinishedHighlightCB */
    XFontStruct *fontStruct;		/* Font structure for primary font */
    int ascent, descent;		/* Composite ascent and descent for
    					   primary font + all-highlight fonts */
    int fixedFontWidth;			/* Font width if all current fonts are
    					   fixed and match in width, else -1 */
    Widget hScrollBar, vScrollBar;
    GC gc, selectGC, highlightGC;	/* GCs for drawing text */
    GC selectBGGC, highlightBGGC;	/* GCs for erasing text */
    GC cursorFGGC;			/* GC for drawing the cursor */
    GC lineNumGC;   	    	    	/* GC for drawing line numbers */
    GC styleGC;     	    	    	/* GC with color and font unspecified
    	    	    	    	    	   for drawing colored/styled text */
    Pixel fgPixel, bgPixel;		/* Foreground/Background colors */
    Pixel selectFGPixel,		/* Foreground select color */
          selectBGPixel;   		/* Background select color */
    Pixel highlightFGPixel,             /* Highlight colors are used when */
          highlightBGPixel;             /*    flashing matching parens    */
    Pixel lineNumFGPixel;   	    	/* Color for drawing line numbers */
    Pixel cursorFGPixel;
    Pixel *bgClassPixel;		/* table of colors for each BG class */
    unsigned char *bgClass;		/* obtains index into bgClassPixel[] */
    
    Widget calltipW;                    /* The Label widget for the calltip */
    Widget calltipShell;                /* The Shell that holds the calltip */
    calltipStruct calltip;              /* The info for the calltip itself */
    Pixel calltipFGPixel;
    Pixel calltipBGPixel;
    int suppressResync;			/* Suppress resynchronization of line
                                           starts during buffer updates */
    int nLinesDeleted;			/* Number of lines deleted during
					   buffer modification (only used
				           when resynchronization is 
                                           suppressed) */
    int modifyingTabDist;		/* Whether tab distance is being
    					   modified */
    Boolean pointerHidden;              /* true if the mouse pointer is 
                                           hidden */
    graphicExposeTranslationEntry *graphicsExposeQueue;
} textDisp;

textDisp *TextDCreate(Widget widget, Widget hScrollBar, Widget vScrollBar,
	Position left, Position top, Position width, Position height,
	Position lineNumLeft, Position lineNumWidth, textBuffer *buffer,
	XFontStruct *fontStruct, Pixel bgPixel, Pixel fgPixel,
	Pixel selectFGPixel, Pixel selectBGPixel, Pixel highlightFGPixel,
	Pixel highlightBGPixel, Pixel cursorFGPixel, Pixel lineNumFGPixel,
        int continuousWrap, int wrapMargin, XmString bgClassString, 
        Pixel calltipFGPixel, Pixel calltipBGPixel);
void TextDFree(textDisp *textD);
void TextDSetBuffer(textDisp *textD, textBuffer *buffer);
void TextDAttachHighlightData(textDisp *textD, textBuffer *styleBuffer,
    	styleTableEntry *styleTable, int nStyles, char unfinishedStyle,
    	unfinishedStyleCBProc unfinishedHighlightCB, void *cbArg);
void TextDSetColors(textDisp *textD, Pixel textFgP, Pixel textBgP,
        Pixel selectFgP, Pixel selectBgP, Pixel hiliteFgP, Pixel hiliteBgP, 
        Pixel lineNoFgP, Pixel cursorFgP);
void TextDSetFont(textDisp *textD, XFontStruct *fontStruct);
int TextDMinFontWidth(textDisp *textD, Boolean considerStyles);
int TextDMaxFontWidth(textDisp *textD, Boolean considerStyles);
void TextDResize(textDisp *textD, int width, int height);
void TextDRedisplayRect(textDisp *textD, int left, int top, int width,
	int height);
void TextDSetScroll(textDisp *textD, int topLineNum, int horizOffset);
void TextDGetScroll(textDisp *textD, int *topLineNum, int *horizOffset);
void TextDInsert(textDisp *textD, char *text);
void TextDOverstrike(textDisp *textD, char *text);
void TextDSetInsertPosition(textDisp *textD, int newPos);
int TextDGetInsertPosition(textDisp *textD);
int TextDXYToPosition(textDisp *textD, int x, int y);
int TextDXYToCharPos(textDisp *textD, int x, int y);
void TextDXYToUnconstrainedPosition(textDisp *textD, int x, int y, int *row,
	int *column);
int TextDLineAndColToPos(textDisp *textD, int lineNum, int column);
int TextDOffsetWrappedColumn(textDisp *textD, int row, int column);
int TextDOffsetWrappedRow(textDisp *textD, int row);
int TextDPositionToXY(textDisp *textD, int pos, int *x, int *y);
int TextDPosToLineAndCol(textDisp *textD, int pos, int *lineNum, int *column);
int TextDInSelection(textDisp *textD, int x, int y);
void TextDMakeInsertPosVisible(textDisp *textD);
int TextDMoveRight(textDisp *textD);
int TextDMoveLeft(textDisp *textD);
int TextDMoveUp(textDisp *textD, int absolute);
int TextDMoveDown(textDisp *textD, int absolute);
void TextDBlankCursor(textDisp *textD);
void TextDUnblankCursor(textDisp *textD);
void TextDSetCursorStyle(textDisp *textD, int style);
void TextDSetWrapMode(textDisp *textD, int wrap, int wrapMargin);
int TextDEndOfLine(const textDisp* textD, int pos,
    Boolean startPosIsLineStart);
int TextDStartOfLine(const textDisp* textD, int pos);
int TextDCountForwardNLines(const textDisp* textD, int startPos,
        unsigned nLines, Boolean startPosIsLineStart);
int TextDCountBackwardNLines(textDisp *textD, int startPos, int nLines);
int TextDCountLines(textDisp *textD, int startPos, int endPos,
    	int startPosIsLineStart);
void TextDSetupBGClasses(Widget w, XmString str, Pixel **pp_bgClassPixel,
	unsigned char **pp_bgClass, Pixel bgPixelDefault);
void TextDSetLineNumberArea(textDisp *textD, int lineNumLeft, int lineNumWidth,
	int textLeft);
void TextDMaintainAbsLineNum(textDisp *textD, int state);
int TextDPosOfPreferredCol(textDisp *textD, int column, int lineStartPos);
int TextDPreferredColumn(textDisp *textD, int *visLineNum, int *lineStartPos);

#ifdef VMS /* VMS linker doesn't like long names (>31 chars) */
#define TextDImposeGraphicsExposeTranslation TextDGraphicsExposeTranslation
#endif /* VMS */

void TextDImposeGraphicsExposeTranslation(textDisp *textD, int *xOffset, int *yOffset);
Boolean TextDPopGraphicExposeQueueEntry(textDisp *textD);
void TextDTranlateGraphicExposeQueue(textDisp *textD, int xOffset, int yOffset, Boolean appendEntry);

#endif /* NEDIT_TEXTDISP_H_INCLUDED */
