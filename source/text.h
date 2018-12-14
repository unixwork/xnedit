/*******************************************************************************
*                                                                              *
* text.h -- Nirvana Editor Text Widget Header File                            *
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

#ifndef NEDIT_TEXT_H_INCLUDED
#define NEDIT_TEXT_H_INCLUDED

#include "textBuf.h"

#include <X11/Intrinsic.h>
#include <X11/X.h>
#include <X11/Xlib.h>

/* Resource strings */
#define textNfont "font"
#define textCFont "Font"
#define textNrows "rows"
#define textCRows "Rows"
#define textNcolumns "columns"
#define textCColumns "Columns"
#define textNmarginWidth "marginWidth"
#define textCMarginWidth "MarginWidth"
#define textNmarginHeight "marginHeight"
#define textCMarginHeight "MarginHeight"
#define textNselectForeground "selectForeground"
#define textCSelectForeground "SelectForeground"
#define textNselectBackground "selectBackground"
#define textCSelectBackground "SelectBackground"
#define textNhighlightForeground "highlightForeground"
#define textCHighlightForeground "HighlightForeground"
#define textNhighlightBackground "highlightBackground"
#define textCHighlightBackground "HighlightBackground"
#define textNcursorForeground "cursorForeground"
#define textCCursorForeground "CursorForeground"
#define textNlineNumForeground "lineNumForeground"
#define textCLineNumForeground "LineNumForeground"
#define textNcalltipForeground "calltipForeground"
#define textCcalltipForeground "CalltipForeground"
#define textNcalltipBackground "calltipBackground"
#define textCcalltipBackground "CalltipBackground"
#define textNpendingDelete "pendingDelete"
#define textCPendingDelete "PendingDelete"
#define textNhScrollBar "hScrollBar"
#define textCHScrollBar "HScrollBar"
#define textNvScrollBar "vScrollBar"
#define textCVScrollBar "VScrollBar"
#define textNlineNumCols "lineNumCols"
#define textCLineNumCols "LineNumCols"
#define textNautoShowInsertPos "autoShowInsertPos"
#define textCAutoShowInsertPos "AutoShowInsertPos"
#define textNautoWrapPastedText "autoWrapPastedText"
#define textCAutoWrapPastedText "AutoWrapPastedText"
#define textNwordDelimiters "wordDelimiters"
#define textCWordDelimiters "WordDelimiters"
#define textNblinkRate "blinkRate"
#define textCBlinkRate "BlinkRate"
#define textNfocusCallback "focusCallback"
#define textCFocusCallback "FocusCallback"
#define textNlosingFocusCallback "losingFocusCallback"
#define textCLosingFocusCallback "LosingFocusCallback"
#define textNcursorMovementCallback "cursorMovementCallback"
#define textCCursorMovementCallback "CursorMovementCallback"
#define textNdragStartCallback "dragStartCallback"
#define textCDragStartCallback "DragStartCallback"
#define textNdragEndCallback "dragEndCallback"
#define textCDragEndCallback "DragEndCallback"
#define textNsmartIndentCallback "smartIndentCallback"
#define textCSmartIndentCallback "SmartIndentCallback"
#define textNautoWrap "autoWrap"
#define textCAutoWrap "AutoWrap"
#define textNcontinuousWrap "continuousWrap"
#define textCContinuousWrap "ContinuousWrap"
#define textNwrapMargin "wrapMargin"
#define textCWrapMargin "WrapMargin"
#define textNautoIndent "autoIndent"
#define textCAutoIndent "AutoIndent"
#define textNsmartIndent "smartIndent"
#define textCSmartIndent "SmartIndent"
#define textNoverstrike "overstrike"
#define textCOverstrike "Overstrike"
#define textNheavyCursor "heavyCursor"
#define textCHeavyCursor "HeavyCursor"
#define textNreadOnly "readOnly"
#define textCReadOnly "ReadOnly"
#define textNhidePointer "hidePointer"
#define textCHidePointer "HidePointer"
#define textNemulateTabs "emulateTabs"
#define textCEmulateTabs "EmulateTabs"
#define textNcursorVPadding "cursorVPadding"
#define textCCursorVPadding "CursorVPadding"
#define textNbacklightCharTypes "backlightCharTypes"
#define textCBacklightCharTypes "BacklightCharTypes"


extern WidgetClass textWidgetClass;

struct _TextClassRec;
struct _TextRec;

typedef struct _TextRec *TextWidget;

typedef struct {
    int startPos;
    int nCharsDeleted;
    int nCharsInserted;
    char *deletedText;
} dragEndCBStruct;

enum smartIndentCallbackReasons {NEWLINE_INDENT_NEEDED, CHAR_TYPED};
typedef struct {
    int reason;
    int pos;
    int indentRequest;
    char *charsTyped;
} smartIndentCBStruct;

/* User callable routines */
void TextSetBuffer(Widget w, textBuffer *buffer);
textBuffer *TextGetBuffer(Widget w);
int TextLineAndColToPos(Widget w, int lineNum, int column);
int TextPosToLineAndCol(Widget w, int pos, int *lineNum, int *column);
int TextPosToXY(Widget w, int pos, int *x, int *y);
int TextGetCursorPos(Widget w);
void TextSetCursorPos(Widget w, int pos);
void TextGetScroll(Widget w, int *topLineNum, int *horizOffset);
void TextSetScroll(Widget w, int topLineNum, int horizOffset);
int TextGetMinFontWidth(Widget w, Boolean considerStyles);
int TextGetMaxFontWidth(Widget w, Boolean considerStyles);
void TextHandleXSelections(Widget w);
void TextPasteClipboard(Widget w, Time time);
void TextColPasteClipboard(Widget w, Time time);
void TextCopyClipboard(Widget w, Time time);
void TextCutClipboard(Widget w, Time time);
int TextFirstVisibleLine(Widget w);
int TextNumVisibleLines(Widget w);
int TextVisibleWidth(Widget w);
void TextInsertAtCursor(Widget w, char *chars, XEvent *event,
    	int allowPendingDelete, int allowWrap);
int TextFirstVisiblePos(Widget w);
int TextLastVisiblePos(Widget w);
char *TextGetWrapped(Widget w, int startPos, int endPos, int *length);
XtActionsRec *TextGetActions(int *nActions);
void ShowHidePointer(TextWidget w, Boolean hidePointer);
void ResetCursorBlink(TextWidget textWidget, Boolean startsBlanked);

#ifdef VMS /* VMS linker doesn't like long names (>31 chars) */
#define HandleAllPendingGraphicsExposeNoExposeEvents HandlePendingExpNoExpEvents
#endif /* VMS */

void HandleAllPendingGraphicsExposeNoExposeEvents(TextWidget w, XEvent *event);

#endif /* NEDIT_TEXT_H_INCLUDED */
