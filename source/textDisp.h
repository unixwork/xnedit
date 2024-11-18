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
#include "colorprofile.h"

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <Xm/Xm.h>

#include <X11/Xft/Xft.h>

enum cursorStyles {NORMAL_CURSOR, CARET_CURSOR, DIM_CURSOR, BLOCK_CURSOR,
	HEAVY_CURSOR};

#define NO_HINT -1

typedef struct _textDisp textDisp;

typedef struct NFont NFont;
typedef struct NFontList NFontList;
typedef struct NCharSetList NCharSetList;
struct NFontList {
    XftFont *font;
    NFontList *next;
};

struct NCharSetList {
    FcCharSet *charset;
    NCharSetList *next;
};

struct NFont {
    NFontList *fonts;
    NCharSetList *fail;
    Display *display;
    FcPattern *pattern;
    double size;
    int minWidth;
    int maxWidth;
    unsigned int ref;
};

typedef struct {
    char *highlightName;
    char *styleName;
    char *colorName;
    char isBold;
    char isItalic;
    XftColor color;
    Boolean underline;
    NFont *font;
    char *bgColorName;      /* background style coloring (name may be NULL) */
    XftColor bgColor;
} styleTableEntry;

typedef struct graphicExposeTranslationEntry {
    int horizontal;
    int vertical;
    struct graphicExposeTranslationEntry *next;
} graphicExposeTranslationEntry;

typedef struct ansiStyle {
    short fg;
    short bg;
    short bold;
    short italic;
    short fg_r;
    short fg_g;
    short fg_b;
    short bg_r;
    short bg_g;
    short bg_b;
} ansiStyle;

typedef void (*unfinishedStyleCBProc)(const textDisp *textD, int pos, const void *highlightCBArg);

typedef struct _calltipStruct {
    int ID;                 /* ID of displayed calltip.  Equals
                              zero if none is displayed. */
    Boolean anchored;       /* Is it anchored to a position */
    int pos;                /* Position tip is anchored to */
    int hAlign;             /* horizontal alignment */
    int vAlign;             /* vertical alignment */
    int alignMode;          /* Strict or sloppy alignment */
} calltipStruct;

typedef struct _textCursor {
    int cursorPos;
    int cursorPosCache;
    int cursorPosCacheLeft;
    int cursorPosCacheRight;
    int cursorPreferredCol;
    int x;                 /* X, Y pos. of last drawn cursor 
                              Note: these are used for *drawing*
                              and are not generally reliable
                              for finding the insert position's
                              x/y coordinates! */
    int y;
} textCursor;

struct _textDisp {
    Widget w;
    XftDraw *d;
    int top, left, width, height, lineNumLeft, lineNumWidth;
    textCursor *cursor;
    textCursor *newcursor;
    textCursor *multicursor;
    size_t mcursorAlloc;
    size_t mcursorSize;
    size_t mcursorSizeReal;
    int cursorOn;
    int cursorToHint;			/* Tells the buffer modified callback
    					   where to move the cursor, to reduce
    					   the number of redraw calls */
    int cursorStyle;			/* One of enum cursorStyles above */
    Position marginWidth;                    /* textWidget marginWidth */
    //int cursorPreferredCol;		/* Column for vert. cursor movement */
    int xic_x;                          /* input method x */
    int xic_y;                          /* input method y */
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
    NFont *font;                        /* primary font */
    NFont *boldFont;
    NFont *italicFont;
    NFont *boldItalicFont;
    int ascent, descent;		/* Composite ascent and descent for
    					   primary font + all-highlight fonts */
    int fixedFontWidth;			/* Font width if all current fonts are
    					   fixed and match in width, else -1 */
    Widget hScrollBar, vScrollBar;
    GC gc;
    GC cursorFGGC;			/* GC for drawing the cursor */
    
    XftColor styleGC;
    XftColor *bgClassPixel;		/* table of colors for each BG class */
    
    unsigned char *bgClass;		/* obtains index into bgClassPixel[] */
    
    Boolean indentRainbow;
    //XftColor *indentRainbowColors;
    //int numRainbowColors;
    
    Boolean ansiColors;
    //XftColor *ansiColorList;
    
    ColorProfile *colorProfile;
    
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
    Boolean mcursorOn;                  /* Is there more than one cursor */
    Boolean pointerHidden;              /* true if the mouse pointer is 
                                           hidden */
    Boolean disableRedisplay;
    Boolean highlightCursorLine;
    Boolean fixLeftClipAfterResize;     /* after resize, left clip could be
                                           in the middle of a glyph */
    Boolean redrawCursorLine;           /* If true, redraw the complete in
                                           line in some functions, when it
                                           contains the cursor */
    graphicExposeTranslationEntry *graphicsExposeQueue;
    
    size_t cacheNoWrappingWidth;        /* Min width with no line wrapping */
    Boolean cacheNoWrapping;            /* Currently no line wrapping */
};

textDisp *TextDCreate(Widget widget, Widget hScrollBar, Widget vScrollBar,
        Position left, Position top, Position width, Position height,
        Position lineNumLeft, Position lineNumWidth, Position marginWidth,
        textBuffer *buffer, NFont *font, NFont *bold, NFont *italic,
        NFont *boldItalic, ColorProfile *colorProfile, int continuousWrap,
        int wrapMargin, XmString bgClassString, Pixel calltipFGPixel,
        Pixel calltipBGPixel, Pixel lineHighlightBGPixel, Boolean indentRainbow,
        Boolean highlightCursorLine, Boolean ansiColors);
void TextDInitXft(textDisp *textD);
void TextDFree(textDisp *textD);
void TextDSetBuffer(textDisp *textD, textBuffer *buffer);
void TextDAttachHighlightData(textDisp *textD, textBuffer *styleBuffer,
    	styleTableEntry *styleTable, int nStyles, char unfinishedStyle,
    	unfinishedStyleCBProc unfinishedHighlightCB, void *cbArg);
void TextDSetColors_Deprecated(textDisp *textD, XftColor *textFgP, XftColor *textBgP,
        XftColor *selectFgP, XftColor *selectBgP, XftColor *hiliteFgP, XftColor *hiliteBgP, 
        XftColor *lineNoFgP, XftColor *lineNoBgP, XftColor *cursorFgP, XftColor *lineHiBgP);
void TextDSetColorProfile(textDisp *textD, ColorProfile *profile);
void TextDSetFont(textDisp *textD, NFont *fontStruct);
void TextDSetBoldFont(textDisp *textD, NFont *boldFont);
void TextDSetItalicFont(textDisp *textD, NFont *boldFont);
void TextDSetBoldItalicFont(textDisp *textD, NFont *boldFont);
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
void TextDChangeCursors(textDisp *textD, int startPos, int diff);
int  TextDAddCursor(textDisp *textD, int newMultiCursorPos);
void TextDRemoveCursor(textDisp *textD, int cursorIndex);
void TextDSetCursors(textDisp *textD, size_t *cursors, size_t ncursors);
int  TextDClearMultiCursor(textDisp *textD);
void TextDCheckCursorDuplicates(textDisp *textD);
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
Boolean TextDPosHasCursor(textDisp *textD, int pos, int *index);
Boolean TextDRangeHasCursor(textDisp *textD, int start, int end);
void TextDSetWrapMode(textDisp *textD, int wrap, int wrapMargin);
int TextDEndOfLine(const textDisp* textD, int pos,
    Boolean startPosIsLineStart);
int TextDStartOfLine(const textDisp* textD, int pos);
int TextDCountForwardNLines(const textDisp* textD, int startPos,
        unsigned nLines, Boolean startPosIsLineStart);
int TextDCountBackwardNLines(textDisp *textD, int startPos, int nLines);
int TextDCountLines(textDisp *textD, int startPos, int endPos,
    	int startPosIsLineStart);
int TextDCountLinesW(textDisp *textD, int startPos, int endPos,
    	int startPosIsLineStart, Boolean *retWrapped);
void TextDSetupBGClasses(Widget w, XmString str, XftColor **pp_bgClassPixel,
	unsigned char **pp_bgClass, XftColor bgPixelDefault);
void TextDSetLineNumberArea(textDisp *textD, int lineNumLeft, int lineNumWidth,
	int textLeft);
void TextDMaintainAbsLineNum(textDisp *textD, int state);
int TextDPosOfPreferredCol(textDisp *textD, int column, int lineStartPos);
int TextDPreferredColumn(textDisp *textD, int *visLineNum, int *lineStartPos);
void TextDSetHighlightCursorLine(textDisp *textD, Boolean state);
void TextDSetIndentRainbow(textDisp *textD, Boolean indentRainbow);
void TextDSetIndentRainbowColors_Deprecated(textDisp *textD, const char *colors);
void TextDCursorLR(textDisp *textD, int *left, int *right);
textCursor TextDPos2Cursor(textDisp *textD, int pos);
void TextDSetAnsiColors(textDisp *textD, Boolean ansiColors);
void TextDSetAnsiColorList_Deprecated(textDisp *textD, XftColor *colors);

NFont *FontCreate(Display *dp, FcPattern *pattern);
NFont *FontFromName(Display *dp, const char *name);
XftFont *FontListAddFontForChar(NFont *f, FcChar32 c);
XftFont *FontDefault(NFont *f);
void FontAddFail(NFont *f, FcCharSet *c);
XftFont *FindFont(NFont *f, FcChar32 c);
void FontDestroy(NFont *f);
NFont *FontRef(NFont *font);
void FontUnref(NFont *font);

XftColor PixelToColor(Widget w, Pixel p);
XftColor RGBToColor(short r, short g, short b);

void TextDImposeGraphicsExposeTranslation(textDisp *textD, int *xOffset, int *yOffset);
Boolean TextDPopGraphicExposeQueueEntry(textDisp *textD);
void TextDTranlateGraphicExposeQueue(textDisp *textD, int xOffset, int yOffset, Boolean appendEntry);

#endif /* NEDIT_TEXTDISP_H_INCLUDED */
