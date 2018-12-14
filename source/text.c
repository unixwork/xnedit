/*******************************************************************************
*									       *
* text.c - Display text from a text buffer				       *
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
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "text.h"
#include "textP.h"
#include "textBuf.h"
#include "textDisp.h"
#include "textSel.h"
#include "textDrag.h"
#include "nedit.h"
#include "calltips.h"
#include "../util/DialogF.h"
#include "window.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/
#include <limits.h>

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>
#if XmVersion >= 1002
#include <Xm/PrimitiveP.h>
#endif

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif


#ifdef UNICOS
#define XtOffset(p_type,field) ((size_t)__INTADDR__(&(((p_type)0)->field)))
#endif

/* Number of pixels of motion from the initial (grab-focus) button press
   required to begin recognizing a mouse drag for the purpose of making a
   selection */
#define SELECT_THRESHOLD 5

/* Length of delay in milliseconds for vertical autoscrolling */
#define VERTICAL_SCROLL_DELAY 50

static void initialize(TextWidget request, TextWidget new);
static void handleHidePointer(Widget w, XtPointer unused, 
        XEvent *event, Boolean *continue_to_dispatch);
static void handleShowPointer(Widget w, XtPointer unused, 
        XEvent *event, Boolean *continue_to_dispatch);
static void redisplay(TextWidget w, XEvent *event, Region region);
static void redisplayGE(TextWidget w, XtPointer client_data,
                    XEvent *event, Boolean *continue_to_dispatch_return);
static void destroy(TextWidget w);
static void resize(TextWidget w);
static Boolean setValues(TextWidget current, TextWidget request,
	TextWidget new);
static void realize(Widget w, XtValueMask *valueMask,
	XSetWindowAttributes *attributes);
static XtGeometryResult queryGeometry(Widget w, XtWidgetGeometry *proposed,
	XtWidgetGeometry *answer);
static void grabFocusAP(Widget w, XEvent *event, String *args,
	Cardinal *n_args);
static void moveDestinationAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void extendAdjustAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void extendStartAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void extendEndAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void processCancelAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void secondaryStartAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void secondaryOrDragStartAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void secondaryAdjustAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void secondaryOrDragAdjustAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void copyToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void copyToOrEndDragAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void copyPrimaryAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void cutPrimaryAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void moveToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void moveToOrEndDragAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void endDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void exchangeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void mousePanAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void pasteClipboardAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void copyClipboardAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void cutClipboardAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void insertStringAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void selfInsertAP(Widget w, XEvent *event, String *args,
	Cardinal *n_args);
static void newlineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void newlineAndIndentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void newlineNoIndentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void processTabAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void endOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void beginningOfLineAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deleteSelectionAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deletePreviousCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deleteNextCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deletePreviousWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deleteNextWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deleteToStartOfLineAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deleteToEndOfLineAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void forwardCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void backwardCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void forwardWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void backwardWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void forwardParagraphAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void backwardParagraphAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void keySelectAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processShiftUpAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void processDownAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void processShiftDownAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void beginningOfFileAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void endOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void nextPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void previousPageAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void pageLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void pageRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void toggleOverstrikeAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void scrollUpAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void scrollDownAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void scrollLeftAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void scrollRightAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void scrollToLineAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void selectAllAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void deselectAllAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void focusInAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void focusOutAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void checkMoveSelectionChange(Widget w, XEvent *event, int startPos,
	String *args, Cardinal *nArgs);
static void keyMoveExtendSelection(Widget w, XEvent *event, int startPos,
	int rectangular);
static void checkAutoShowInsertPos(Widget w);
static int checkReadOnly(Widget w);
static void simpleInsertAtCursor(Widget w, char *chars, XEvent *event,
    	int allowPendingDelete);
static int pendingSelection(Widget w);
static int deletePendingSelection(Widget w, XEvent *event);
static int deleteEmulatedTab(Widget w, XEvent *event);
static void selectWord(Widget w, int pointerX);
static int spanForward(textBuffer *buf, int startPos, char *searchChars,
	int ignoreSpace, int *foundPos);
static int spanBackward(textBuffer *buf, int startPos, char *searchChars, int
    	ignoreSpace, int *foundPos);
static void selectLine(Widget w);
static int startOfWord(TextWidget w, int pos);
static int endOfWord(TextWidget w, int pos);
static void checkAutoScroll(TextWidget w, int x, int y);
static void endDrag(Widget w);
static void cancelDrag(Widget w);
static void callCursorMovementCBs(Widget w, XEvent *event);
static void adjustSelection(TextWidget tw, int x, int y);
static void adjustSecondarySelection(TextWidget tw, int x, int y);
static void autoScrollTimerProc(XtPointer clientData, XtIntervalId *id);
static char *wrapText(TextWidget tw, char *startLine, char *text, int bufOffset,
    	int wrapMargin, int *breakBefore);
static int wrapLine(TextWidget tw, textBuffer *buf, int bufOffset,
    	int lineStartPos, int lineEndPos, int limitPos, int *breakAt,
	int *charsAdded);
static char *createIndentString(TextWidget tw, textBuffer *buf, int bufOffset,
    	int lineStartPos, int lineEndPos, int *length, int *column);
static void cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id);
static int hasKey(const char *key, const String *args, const Cardinal *nArgs);
static int max(int i1, int i2);
static int min(int i1, int i2);
static int strCaseCmp(const char *str1, const char *str2);
static void ringIfNecessary(Boolean silent, Widget w);

static char defaultTranslations[] = 
    /* Home */
    "~Shift ~Ctrl Alt<Key>osfBeginLine: last_document()\n"

    /* Backspace */
    "Ctrl<KeyPress>osfBackSpace: delete_previous_word()\n"
    "<KeyPress>osfBackSpace: delete_previous_character()\n"

    /* Delete */
    "Alt Shift Ctrl<KeyPress>osfDelete: cut_primary(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfDelete: cut_primary(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfDelete: cut_primary()\n"
    "Ctrl<KeyPress>osfDelete: delete_to_end_of_line()\n"
    "Shift<KeyPress>osfDelete: cut_clipboard()\n"
    "<KeyPress>osfDelete: delete_next_character()\n"

    /* Insert */
    "Alt Shift Ctrl<KeyPress>osfInsert: copy_primary(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfInsert: copy_primary(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfInsert: copy_primary()\n"
    "Shift<KeyPress>osfInsert: paste_clipboard()\n"
    "Ctrl<KeyPress>osfInsert: copy_clipboard()\n"
    "~Shift ~Ctrl<KeyPress>osfInsert: set_overtype_mode()\n"

    /* Cut/Copy/Paste */
    "Shift Ctrl<KeyPress>osfCut: cut_primary()\n"
    "<KeyPress>osfCut: cut_clipboard()\n"
    "<KeyPress>osfCopy: copy_clipboard()\n"
    "<KeyPress>osfPaste: paste_clipboard()\n"
    "<KeyPress>osfPrimaryPaste: copy_primary()\n"

    /* BeginLine */
    "Alt Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\" \"rect\")\n"
    "Alt Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\")\n"
    "Ctrl<KeyPress>osfBeginLine: beginning_of_file()\n"
    "Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfBeginLine: beginning_of_line()\n"

    /* EndLine */
    "Alt Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfEndLine: end_of_line(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfEndLine: end_of_line(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\")\n"
    "Ctrl<KeyPress>osfEndLine: end_of_file()\n"
    "Shift<KeyPress>osfEndLine: end_of_line(\"extend\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfEndLine: end_of_line()\n"

    /* Left */
    "Alt Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfLeft: key_select(\"left\", \"rect\")\n"
    "Meta Shift<KeyPress>osfLeft: key_select(\"left\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\")\n"
    "Ctrl<KeyPress>osfLeft: backward_word()\n"
    "Shift<KeyPress>osfLeft: key_select(\"left\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfLeft: backward_character()\n"

    /* Right */
    "Alt Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfRight: key_select(\"right\", \"rect\")\n"
    "Meta Shift<KeyPress>osfRight: key_select(\"right\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\")\n"
    "Ctrl<KeyPress>osfRight: forward_word()\n"
    "Shift<KeyPress>osfRight: key_select(\"right\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfRight: forward_character()\n"

    /* Up */
    "Alt Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfUp: process_shift_up(\"rect\")\n"
    "Meta Shift<KeyPress>osfUp: process_shift_up(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\")\n"
    "Ctrl<KeyPress>osfUp: backward_paragraph()\n"
    "Shift<KeyPress>osfUp: process_shift_up()\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfUp: process_up()\n"

    /* Down */
    "Alt Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfDown: process_shift_down(\"rect\")\n"
    "Meta Shift<KeyPress>osfDown: process_shift_down(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\")\n"
    "Ctrl<KeyPress>osfDown: forward_paragraph()\n"
    "Shift<KeyPress>osfDown: process_shift_down()\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfDown: process_down()\n"

    /* PageUp */
    "Alt Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfPageUp: previous_page(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageUp: previous_page(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\")\n"
    "Ctrl<KeyPress>osfPageUp: previous_document()\n"
    "Shift<KeyPress>osfPageUp: previous_page(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageUp: previous_page()\n"

    /* PageDown */
    "Alt Shift Ctrl<KeyPress>osfPageDown: page_right(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfPageDown: page_right(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfPageDown: next_page(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageDown: next_page(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfPageDown: page_right(\"extend\")\n"
    "Ctrl<KeyPress>osfPageDown: next_document()\n"
    "Shift<KeyPress>osfPageDown: next_page(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageDown: next_page()\n"

    /* PageLeft and PageRight are placed later than the PageUp/PageDown
       bindings.  Some systems map osfPageLeft to Ctrl-PageUp. 
       Overloading this single key gives problems, and we want to give
       priority to the normal version. */

    /* PageLeft */
    "Alt Shift<KeyPress>osfPageLeft: page_left(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageLeft: page_left(\"extend\", \"rect\")\n"
    "Shift<KeyPress>osfPageLeft: page_left(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageLeft: page_left()\n"

    /* PageRight */
    "Alt Shift<KeyPress>osfPageRight: page_right(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageRight: page_right(\"extend\", \"rect\")\n"
    "Shift<KeyPress>osfPageRight: page_right(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageRight: page_right()\n"

    "Shift<KeyPress>osfSelect: key_select()\n"
    "<KeyPress>osfCancel: process_cancel()\n"
    "Ctrl~Alt~Meta<KeyPress>v: paste_clipboard()\n"
    "Ctrl~Alt~Meta<KeyPress>c: copy_clipboard()\n"
    "Ctrl~Alt~Meta<KeyPress>x: cut_clipboard()\n"
    "Ctrl~Alt~Meta<KeyPress>u: delete_to_start_of_line()\n"
    "Ctrl<KeyPress>Return: newline_and_indent()\n"
    "Shift<KeyPress>Return: newline_no_indent()\n"
    "<KeyPress>Return: newline()\n"
    /* KP_Enter = osfActivate
       Note: Ctrl+KP_Enter is already bound to Execute Command Line... */
    "Shift<KeyPress>osfActivate: newline_no_indent()\n" 
    "<KeyPress>osfActivate: newline()\n"
    "Ctrl<KeyPress>Tab: self_insert()\n"
    "<KeyPress>Tab: process_tab()\n"
    "Alt Shift Ctrl<KeyPress>space: key_select(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>space: key_select(\"rect\")\n"
    "Shift Ctrl~Meta~Alt<KeyPress>space: key_select()\n"
    "Ctrl~Meta~Alt<KeyPress>slash: select_all()\n"
    "Ctrl~Meta~Alt<KeyPress>backslash: deselect_all()\n"
    "<KeyPress>: self_insert()\n"
    "Alt Ctrl<Btn1Down>: move_destination()\n"
    "Meta Ctrl<Btn1Down>: move_destination()\n"
    "Shift Ctrl<Btn1Down>: extend_start(\"rect\")\n"
    "Shift<Btn1Down>: extend_start()\n"
    "<Btn1Down>: grab_focus()\n"
    "Button1 Ctrl<MotionNotify>: extend_adjust(\"rect\")\n"
    "Button1~Ctrl<MotionNotify>: extend_adjust()\n"
    "<Btn1Up>: extend_end()\n"
    "<Btn2Down>: secondary_or_drag_start()\n"
    "Shift Ctrl Button2<MotionNotify>: secondary_or_drag_adjust(\"rect\", \"copy\", \"overlay\")\n"
    "Shift Button2<MotionNotify>: secondary_or_drag_adjust(\"copy\")\n"
    "Ctrl Button2<MotionNotify>: secondary_or_drag_adjust(\"rect\", \"overlay\")\n"
    "Button2<MotionNotify>: secondary_or_drag_adjust()\n"
    "Shift Ctrl<Btn2Up>: move_to_or_end_drag(\"copy\", \"overlay\")\n"
    "Shift <Btn2Up>: move_to_or_end_drag(\"copy\")\n"
    "Alt<Btn2Up>: exchange()\n"
    "Meta<Btn2Up>: exchange()\n"
    "Ctrl<Btn2Up>: copy_to_or_end_drag(\"overlay\")\n"
    "<Btn2Up>: copy_to_or_end_drag()\n"
    "Ctrl~Meta~Alt<Btn3Down>: mouse_pan()\n"
    "Ctrl~Meta~Alt Button3<MotionNotify>: mouse_pan()\n"
    "<Btn3Up>: end_drag()\n"
    "<FocusIn>: focusIn()\n"
    "<FocusOut>: focusOut()\n"
    /* Support for mouse wheel in XFree86 */
    "Shift<Btn4Down>,<Btn4Up>: scroll_up(1)\n"
    "Shift<Btn5Down>,<Btn5Up>: scroll_down(1)\n"
    "Ctrl<Btn4Down>,<Btn4Up>: scroll_up(1, pages)\n"
    "Ctrl<Btn5Down>,<Btn5Up>: scroll_down(1, pages)\n"
    "<Btn4Down>,<Btn4Up>: scroll_up(5)\n"
    "<Btn5Down>,<Btn5Up>: scroll_down(5)\n";
     /* some of the translations from the Motif text widget were not picked up:
	  :<KeyPress>osfSelect: set-anchor()\n\
	  :<KeyPress>osfActivate: activate()\n\
	 ~Shift Ctrl~Meta~Alt<KeyPress>Return: activate()\n\
	  ~Shift Ctrl~Meta~Alt<KeyPress>space: set-anchor()\n\
	   :<KeyPress>osfClear: clear-selection()\n\
	  ~Shift~Ctrl~Meta~Alt<KeyPress>Return: process-return()\n\
	  Shift~Meta~Alt<KeyPress>Tab: prev-tab-group()\n\
	  Ctrl~Meta~Alt<KeyPress>Tab: next-tab-group()\n\
	  <UnmapNotify>: unmap()\n\
	  <EnterNotify>: enter()\n\
	  <LeaveNotify>: leave()\n
     */


static XtActionsRec actionsList[] = {
    {"self-insert", selfInsertAP},
    {"self_insert", selfInsertAP},
    {"grab-focus", grabFocusAP},
    {"grab_focus", grabFocusAP},
    {"extend-adjust", extendAdjustAP},
    {"extend_adjust", extendAdjustAP},
    {"extend-start", extendStartAP},
    {"extend_start", extendStartAP},
    {"extend-end", extendEndAP},
    {"extend_end", extendEndAP},
    {"secondary-adjust", secondaryAdjustAP},
    {"secondary_adjust", secondaryAdjustAP},
    {"secondary-or-drag-adjust", secondaryOrDragAdjustAP},
    {"secondary_or_drag_adjust", secondaryOrDragAdjustAP},
    {"secondary-start", secondaryStartAP},
    {"secondary_start", secondaryStartAP},
    {"secondary-or-drag-start", secondaryOrDragStartAP},
    {"secondary_or_drag_start", secondaryOrDragStartAP},
    {"process-bdrag", secondaryOrDragStartAP},
    {"process_bdrag", secondaryOrDragStartAP},
    {"move-destination", moveDestinationAP},
    {"move_destination", moveDestinationAP},
    {"move-to", moveToAP},
    {"move_to", moveToAP},
    {"move-to-or-end-drag", moveToOrEndDragAP},
    {"move_to_or_end_drag", moveToOrEndDragAP},
    {"end_drag", endDragAP},
    {"copy-to", copyToAP},
    {"copy_to", copyToAP},
    {"copy-to-or-end-drag", copyToOrEndDragAP},
    {"copy_to_or_end_drag", copyToOrEndDragAP},
    {"exchange", exchangeAP},
    {"process-cancel", processCancelAP},
    {"process_cancel", processCancelAP},
    {"paste-clipboard", pasteClipboardAP},
    {"paste_clipboard", pasteClipboardAP},
    {"copy-clipboard", copyClipboardAP},
    {"copy_clipboard", copyClipboardAP},
    {"cut-clipboard", cutClipboardAP},
    {"cut_clipboard", cutClipboardAP},
    {"copy-primary", copyPrimaryAP},
    {"copy_primary", copyPrimaryAP},
    {"cut-primary", cutPrimaryAP},
    {"cut_primary", cutPrimaryAP},
    {"newline", newlineAP},
    {"newline-and-indent", newlineAndIndentAP},
    {"newline_and_indent", newlineAndIndentAP},
    {"newline-no-indent", newlineNoIndentAP},
    {"newline_no_indent", newlineNoIndentAP},
    {"delete-selection", deleteSelectionAP},
    {"delete_selection", deleteSelectionAP},
    {"delete-previous-character", deletePreviousCharacterAP},
    {"delete_previous_character", deletePreviousCharacterAP},
    {"delete-next-character", deleteNextCharacterAP},
    {"delete_next_character", deleteNextCharacterAP},
    {"delete-previous-word", deletePreviousWordAP},
    {"delete_previous_word", deletePreviousWordAP},
    {"delete-next-word", deleteNextWordAP},
    {"delete_next_word", deleteNextWordAP},
    {"delete-to-start-of-line", deleteToStartOfLineAP},
    {"delete_to_start_of_line", deleteToStartOfLineAP},
    {"delete-to-end-of-line", deleteToEndOfLineAP},
    {"delete_to_end_of_line", deleteToEndOfLineAP},
    {"forward-character", forwardCharacterAP},
    {"forward_character", forwardCharacterAP},
    {"backward-character", backwardCharacterAP},
    {"backward_character", backwardCharacterAP},
    {"key-select", keySelectAP},
    {"key_select", keySelectAP},
    {"process-up", processUpAP},
    {"process_up", processUpAP},
    {"process-down", processDownAP},
    {"process_down", processDownAP},
    {"process-shift-up", processShiftUpAP},
    {"process_shift_up", processShiftUpAP},
    {"process-shift-down", processShiftDownAP},
    {"process_shift_down", processShiftDownAP},
    {"process-home", beginningOfLineAP},
    {"process_home", beginningOfLineAP},
    {"forward-word", forwardWordAP},
    {"forward_word", forwardWordAP},
    {"backward-word", backwardWordAP},
    {"backward_word", backwardWordAP},
    {"forward-paragraph", forwardParagraphAP},
    {"forward_paragraph", forwardParagraphAP},
    {"backward-paragraph", backwardParagraphAP},
    {"backward_paragraph", backwardParagraphAP},
    {"beginning-of-line", beginningOfLineAP},
    {"beginning_of_line", beginningOfLineAP},
    {"end-of-line", endOfLineAP},
    {"end_of_line", endOfLineAP},
    {"beginning-of-file", beginningOfFileAP},
    {"beginning_of_file", beginningOfFileAP},
    {"end-of-file", endOfFileAP},
    {"end_of_file", endOfFileAP},
    {"next-page", nextPageAP},
    {"next_page", nextPageAP},
    {"previous-page", previousPageAP},
    {"previous_page", previousPageAP},
    {"page-left", pageLeftAP},
    {"page_left", pageLeftAP},
    {"page-right", pageRightAP},
    {"page_right", pageRightAP},
    {"toggle-overstrike", toggleOverstrikeAP},
    {"toggle_overstrike", toggleOverstrikeAP},
    {"scroll-up", scrollUpAP},
    {"scroll_up", scrollUpAP},
    {"scroll-down", scrollDownAP},
    {"scroll_down", scrollDownAP},
    {"scroll_left", scrollLeftAP},
    {"scroll_right", scrollRightAP},
    {"scroll-to-line", scrollToLineAP},
    {"scroll_to_line", scrollToLineAP},
    {"select-all", selectAllAP},
    {"select_all", selectAllAP},
    {"deselect-all", deselectAllAP},
    {"deselect_all", deselectAllAP},
    {"focusIn", focusInAP},
    {"focusOut", focusOutAP},
    {"process-return", selfInsertAP},
    {"process_return", selfInsertAP},
    {"process-tab", processTabAP},
    {"process_tab", processTabAP},
    {"insert-string", insertStringAP},
    {"insert_string", insertStringAP},
    {"mouse_pan", mousePanAP},
};

/* The motif text widget defined a bunch of actions which the nedit text
   widget as-of-yet does not support:
   
     Actions which were not bound to keys (for emacs emulation, some of
     them should probably be supported:
       
	kill-next-character()
	kill-next-word()
	kill-previous-character()
	kill-previous-word()
	kill-selection()
	kill-to-end-of-line()
	kill-to-start-of-line()
	unkill()
	next-line()
	newline-and-backup()
	beep()
	redraw-display()
	scroll-one-line-down()
	scroll-one-line-up()
	set-insertion-point()

    Actions which are not particularly useful:
    
	set-anchor()
	activate()
	clear-selection() -> this is a wierd one
	do-quick-action() -> don't think this ever worked
	Help()
	next-tab-group()
	select-adjust()
	select-start()
	select-end()
*/

static XtResource resources[] = {
    {XmNhighlightThickness, XmCHighlightThickness, XmRDimension,
      sizeof(Dimension), XtOffset(TextWidget, primitive.highlight_thickness),
      XmRInt, 0},
    {XmNshadowThickness, XmCShadowThickness, XmRDimension, sizeof(Dimension),
      XtOffset(TextWidget, primitive.shadow_thickness), XmRInt, 0},
    {textNfont, textCFont, XmRFontStruct, sizeof(XFontStruct *),
      XtOffset(TextWidget, text.fontStruct), XmRString, "fixed"},
    {textNselectForeground, textCSelectForeground, XmRPixel, sizeof(Pixel),
      XtOffset(TextWidget, text.selectFGPixel), XmRString, 
      NEDIT_DEFAULT_SEL_FG},
    {textNselectBackground, textCSelectBackground, XmRPixel, sizeof(Pixel),
      XtOffset(TextWidget, text.selectBGPixel), XmRString, 
      NEDIT_DEFAULT_SEL_BG},
    {textNhighlightForeground, textCHighlightForeground, XmRPixel,sizeof(Pixel),
      XtOffset(TextWidget, text.highlightFGPixel), XmRString, 
      NEDIT_DEFAULT_HI_FG},
    {textNhighlightBackground, textCHighlightBackground, XmRPixel,sizeof(Pixel),
      XtOffset(TextWidget, text.highlightBGPixel), XmRString, 
      NEDIT_DEFAULT_HI_BG},
    {textNlineNumForeground, textCLineNumForeground, XmRPixel,sizeof(Pixel),
      XtOffset(TextWidget, text.lineNumFGPixel), XmRString, 
      NEDIT_DEFAULT_LINENO_FG},
    {textNcursorForeground, textCCursorForeground, XmRPixel,sizeof(Pixel),
      XtOffset(TextWidget, text.cursorFGPixel), XmRString, 
      NEDIT_DEFAULT_CURSOR_FG},
    {textNcalltipForeground, textCcalltipForeground, XmRPixel,sizeof(Pixel),
      XtOffset(TextWidget, text.calltipFGPixel), XmRString, 
      NEDIT_DEFAULT_CALLTIP_FG},
    {textNcalltipBackground, textCcalltipBackground, XmRPixel,sizeof(Pixel),
      XtOffset(TextWidget, text.calltipBGPixel), XmRString, 
      NEDIT_DEFAULT_CALLTIP_BG},
    {textNbacklightCharTypes,textCBacklightCharTypes,XmRString,sizeof(XmString),
      XtOffset(TextWidget, text.backlightCharTypes), XmRString, NULL},
    {textNrows, textCRows, XmRInt,sizeof(int),
      XtOffset(TextWidget, text.rows), XmRString, "24"},
    {textNcolumns, textCColumns, XmRInt, sizeof(int),
      XtOffset(TextWidget, text.columns), XmRString, "80"},
    {textNmarginWidth, textCMarginWidth, XmRInt, sizeof(int),
      XtOffset(TextWidget, text.marginWidth), XmRString, "5"},
    {textNmarginHeight, textCMarginHeight, XmRInt, sizeof(int),
      XtOffset(TextWidget, text.marginHeight), XmRString, "5"},
    {textNpendingDelete, textCPendingDelete, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.pendingDelete), XmRString, "True"},
    {textNautoWrap, textCAutoWrap, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.autoWrap), XmRString, "True"},
    {textNcontinuousWrap, textCContinuousWrap, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.continuousWrap), XmRString, "True"},
    {textNautoIndent, textCAutoIndent, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.autoIndent), XmRString, "True"},
    {textNsmartIndent, textCSmartIndent, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.smartIndent), XmRString, "False"},
    {textNoverstrike, textCOverstrike, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.overstrike), XmRString, "False"},
    {textNheavyCursor, textCHeavyCursor, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.heavyCursor), XmRString, "False"},
    {textNreadOnly, textCReadOnly, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.readOnly), XmRString, "False"},
    {textNhidePointer, textCHidePointer, XmRBoolean, sizeof(Boolean),
      XtOffset(TextWidget, text.hidePointer), XmRString, "False"},
    {textNwrapMargin, textCWrapMargin, XmRInt, sizeof(int),
      XtOffset(TextWidget, text.wrapMargin), XmRString, "0"},
    {textNhScrollBar, textCHScrollBar, XmRWidget, sizeof(Widget),
      XtOffset(TextWidget, text.hScrollBar), XmRString, ""},
    {textNvScrollBar, textCVScrollBar, XmRWidget, sizeof(Widget),
      XtOffset(TextWidget, text.vScrollBar), XmRString, ""},
    {textNlineNumCols, textCLineNumCols, XmRInt, sizeof(int),
      XtOffset(TextWidget, text.lineNumCols), XmRString, "0"},
    {textNautoShowInsertPos, textCAutoShowInsertPos, XmRBoolean,
      sizeof(Boolean), XtOffset(TextWidget, text.autoShowInsertPos),
      XmRString, "True"},
    {textNautoWrapPastedText, textCAutoWrapPastedText, XmRBoolean,
      sizeof(Boolean), XtOffset(TextWidget, text.autoWrapPastedText),
      XmRString, "False"},
    {textNwordDelimiters, textCWordDelimiters, XmRString, sizeof(char *),
      XtOffset(TextWidget, text.delimiters), XmRString,
      ".,/\\`'!@#%^&*()-=+{}[]\":;<>?"},
    {textNblinkRate, textCBlinkRate, XmRInt, sizeof(int),
      XtOffset(TextWidget, text.cursorBlinkRate), XmRString, "500"},
    {textNemulateTabs, textCEmulateTabs, XmRInt, sizeof(int),
      XtOffset(TextWidget, text.emulateTabs), XmRString, "0"},
    {textNfocusCallback, textCFocusCallback, XmRCallback, sizeof(caddr_t),
      XtOffset(TextWidget, text.focusInCB), XtRCallback, NULL},
    {textNlosingFocusCallback, textCLosingFocusCallback, XmRCallback,
      sizeof(caddr_t), XtOffset(TextWidget, text.focusOutCB), XtRCallback,NULL},
    {textNcursorMovementCallback, textCCursorMovementCallback, XmRCallback,
      sizeof(caddr_t), XtOffset(TextWidget, text.cursorCB), XtRCallback, NULL},
    {textNdragStartCallback, textCDragStartCallback, XmRCallback,
      sizeof(caddr_t), XtOffset(TextWidget, text.dragStartCB), XtRCallback,
      NULL},
    {textNdragEndCallback, textCDragEndCallback, XmRCallback,
      sizeof(caddr_t), XtOffset(TextWidget, text.dragEndCB), XtRCallback, NULL},
    {textNsmartIndentCallback, textCSmartIndentCallback, XmRCallback,
      sizeof(caddr_t), XtOffset(TextWidget, text.smartIndentCB), XtRCallback,
      NULL},
    {textNcursorVPadding, textCCursorVPadding, XtRCardinal, sizeof(Cardinal),
      XtOffset(TextWidget, text.cursorVPadding), XmRString, "0"}
};

static TextClassRec textClassRec = {
     /* CoreClassPart */
  {
    (WidgetClass) &xmPrimitiveClassRec,  /* superclass       */
    "Text",                         /* class_name            */
    sizeof(TextRec),                /* widget_size           */
    NULL,                           /* class_initialize      */
    NULL,                           /* class_part_initialize */
    FALSE,                          /* class_inited          */
    (XtInitProc)initialize,         /* initialize            */
    NULL,                           /* initialize_hook       */
    realize,   		            /* realize               */
    actionsList,                    /* actions               */
    XtNumber(actionsList),          /* num_actions           */
    resources,                      /* resources             */
    XtNumber(resources),            /* num_resources         */
    NULLQUARK,                      /* xrm_class             */
    TRUE,                           /* compress_motion       */
    TRUE,                           /* compress_exposure     */
    TRUE,                           /* compress_enterleave   */
    FALSE,                          /* visible_interest      */
    (XtWidgetProc)destroy,          /* destroy               */
    (XtWidgetProc)resize,           /* resize                */
    (XtExposeProc)redisplay,        /* expose                */
    (XtSetValuesFunc)setValues,     /* set_values            */
    NULL,                           /* set_values_hook       */
    XtInheritSetValuesAlmost,       /* set_values_almost     */
    NULL,                           /* get_values_hook       */
    NULL,                           /* accept_focus          */
    XtVersion,                      /* version               */
    NULL,                           /* callback private      */
    defaultTranslations,            /* tm_table              */
    queryGeometry,                  /* query_geometry        */
    NULL,                           /* display_accelerator   */
    NULL,                           /* extension             */
  },
  /* Motif primitive class fields */
  {
     (XtWidgetProc)_XtInherit,   	/* Primitive border_highlight   */
     (XtWidgetProc)_XtInherit,   	/* Primitive border_unhighlight */
     NULL, /*XtInheritTranslations,*/	/* translations                 */
     NULL,				/* arm_and_activate             */
     NULL,				/* get resources      		*/
     0,					/* num get_resources  		*/
     NULL,         			/* extension                    */
  },
  /* Text class part */
  {
    0,                              	/* ignored	                */
  }
};

WidgetClass textWidgetClass = (WidgetClass)&textClassRec;
#define NEDIT_HIDE_CURSOR_MASK (KeyPressMask)
#define NEDIT_SHOW_CURSOR_MASK (FocusChangeMask | PointerMotionMask | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask)
static char empty_bits[] = {0x00, 0x00, 0x00, 0x00};
static Cursor empty_cursor = 0;

/*
** Widget initialize method
*/
static void initialize(TextWidget request, TextWidget new)
{
    XFontStruct *fs = new->text.fontStruct;
    char *delimiters;
    textBuffer *buf;
    Pixel white, black;
    int textLeft;
    int charWidth;
    int marginWidth;
    int lineNumCols;

    charWidth = fs->max_bounds.width;
    marginWidth = new->text.marginWidth;
    lineNumCols = new->text.lineNumCols;
    
    /* Set the initial window size based on the rows and columns resources */
    if (request->core.width == 0)
    	new->core.width = charWidth * new->text.columns + marginWidth*2 +
	       (lineNumCols == 0 ? 0 : marginWidth + charWidth * lineNumCols);
    if (request->core.height == 0)
   	new->core.height = (fs->ascent + fs->descent) * new->text.rows +
   		new->text.marginHeight * 2;
    
    /* The default colors work for B&W as well as color, except for
       selectFGPixel and selectBGPixel, where color highlighting looks
       much better without reverse video, so if we get here, and the
       selection is totally unreadable because of the bad default colors,
       swap the colors and make the selection reverse video */
    white = WhitePixelOfScreen(XtScreen((Widget)new));
    black = BlackPixelOfScreen(XtScreen((Widget)new));
    if (    new->text.selectBGPixel == white &&
    	    new->core.background_pixel == white &&
    	    new->text.selectFGPixel == black &&
    	    new->primitive.foreground == black) {
    	new->text.selectBGPixel = black;
    	new->text.selectFGPixel = white;
    }
    
    /* Create the initial text buffer for the widget to display (which can
       be replaced later with TextSetBuffer) */
    buf = BufCreate();
    
    /* Create and initialize the text-display part of the widget */
    textLeft = new->text.marginWidth +
	    (lineNumCols == 0 ? 0 : marginWidth + charWidth * lineNumCols);
    new->text.textD = TextDCreate((Widget)new, new->text.hScrollBar,
	    new->text.vScrollBar, textLeft, new->text.marginHeight,
	    new->core.width - marginWidth - textLeft,
	    new->core.height - new->text.marginHeight * 2,
	    lineNumCols == 0 ? 0 : marginWidth,
	    lineNumCols == 0 ? 0 : lineNumCols * charWidth,
	    buf, new->text.fontStruct, new->core.background_pixel,
	    new->primitive.foreground, new->text.selectFGPixel,
	    new->text.selectBGPixel, new->text.highlightFGPixel,
	    new->text.highlightBGPixel, new->text.cursorFGPixel,
	    new->text.lineNumFGPixel,
          new->text.continuousWrap, new->text.wrapMargin,
          new->text.backlightCharTypes, new->text.calltipFGPixel,
          new->text.calltipBGPixel);

    /* Add mandatory delimiters blank, tab, and newline to the list of
       delimiters.  The memory use scheme here is that new values are
       always copied, and can therefore be safely freed on subsequent
       set-values calls or destroy */
    delimiters = (char*)NEditMalloc(strlen(new->text.delimiters) + 4);
    sprintf(delimiters, "%s%s", " \t\n", new->text.delimiters);
    new->text.delimiters = delimiters;

    /* Start with the cursor blanked (widgets don't have focus on creation,
       the initial FocusIn event will unblank it and get blinking started) */
    new->text.textD->cursorOn = False;
    
    /* Initialize the widget variables */
    new->text.autoScrollProcID = 0;
    new->text.cursorBlinkProcID = 0;
    new->text.dragState = NOT_CLICKED;
    new->text.multiClickState = NO_CLICKS;
    new->text.lastBtnDown = 0;
    new->text.selectionOwner = False;
    new->text.motifDestOwner = False;
    new->text.emTabsBeforeCursor = 0;

#ifndef NO_XMIM
    /* Register the widget to the input manager */
    XmImRegister((Widget)new, 0);
    /* In case some Resources for the IC need to be set, add them below */
    XmImVaSetValues((Widget)new, NULL);
#endif

    XtAddEventHandler((Widget)new, GraphicsExpose, True,
            (XtEventHandler)redisplayGE, (Opaque)NULL);

    if (new->text.hidePointer) {
        Display *theDisplay;
        Pixmap empty_pixmap;
        XColor black_color;
        /* Set up the empty Cursor */
        if (empty_cursor == 0) {
            theDisplay = XtDisplay((Widget)new);
            empty_pixmap = XCreateBitmapFromData(theDisplay,
                    RootWindowOfScreen(XtScreen((Widget)new)), empty_bits, 1, 1);
            XParseColor(theDisplay, DefaultColormapOfScreen(XtScreen((Widget)new)), 
                    "black", &black_color);
            empty_cursor = XCreatePixmapCursor(theDisplay, empty_pixmap, 
                    empty_pixmap, &black_color, &black_color, 0, 0);
        }

        /* Add event handler to hide the pointer on keypresses */
        XtAddEventHandler((Widget)new, NEDIT_HIDE_CURSOR_MASK, False, 
                handleHidePointer, (Opaque)NULL);
    }
}

/* Hide the pointer while the user is typing */
static void handleHidePointer(Widget w, XtPointer unused, 
        XEvent *event, Boolean *continue_to_dispatch) {
    TextWidget tw = (TextWidget) w;
    ShowHidePointer(tw, True);
}

/* Restore the pointer if the mouse moves or focus changes */
static void handleShowPointer(Widget w, XtPointer unused, 
        XEvent *event, Boolean *continue_to_dispatch) {
    TextWidget tw = (TextWidget) w;
    ShowHidePointer(tw, False);
}

void ShowHidePointer(TextWidget w, Boolean hidePointer)
{
    if (w->text.hidePointer) {
        if (hidePointer != w->text.textD->pointerHidden) {
            if (hidePointer) {
                /* Don't listen for keypresses any more */
                XtRemoveEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, False, 
                        handleHidePointer, (Opaque)NULL);
                /* Switch to empty cursor */
                XDefineCursor(XtDisplay(w), XtWindow(w), empty_cursor);

                w->text.textD->pointerHidden = True;

                /* Listen to mouse movement, focus change, and button presses */
                XtAddEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK,
                        False, handleShowPointer, (Opaque)NULL);
            }
            else {
                /* Don't listen to mouse/focus events any more */
                XtRemoveEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK,
                        False, handleShowPointer, (Opaque)NULL);
                /* Switch to regular cursor */
                XUndefineCursor(XtDisplay(w), XtWindow(w));

                w->text.textD->pointerHidden = False;

                /* Listen for keypresses now */
                XtAddEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, False, 
                        handleHidePointer, (Opaque)NULL);
            }
        }
    }
}

/*
** Widget destroy method
*/
static void destroy(TextWidget w)
{
    textBuffer *buf;
    
    /* Free the text display and possibly the attached buffer.  The buffer
       is freed only if after removing all of the modify procs (by calling
       StopHandlingXSelections and TextDFree) there are no modify procs
       left */
    StopHandlingXSelections((Widget)w);
    buf = w->text.textD->buffer;
    TextDFree(w->text.textD);
    if (buf->nModifyProcs == 0)
    	BufFree(buf);

    if (w->text.cursorBlinkProcID != 0)
    	XtRemoveTimeOut(w->text.cursorBlinkProcID);
    NEditFree(w->text.delimiters);
    XtRemoveAllCallbacks((Widget)w, textNfocusCallback);
    XtRemoveAllCallbacks((Widget)w, textNlosingFocusCallback);
    XtRemoveAllCallbacks((Widget)w, textNcursorMovementCallback);
    XtRemoveAllCallbacks((Widget)w, textNdragStartCallback);
    XtRemoveAllCallbacks((Widget)w, textNdragEndCallback);

#ifndef NO_XMIM
    /* Unregister the widget from the input manager */
    XmImUnregister((Widget)w);
#endif
}

/*
** Widget resize method.  Called when the size of the widget changes
*/
static void resize(TextWidget w)
{
    XFontStruct *fs = w->text.fontStruct;
    int height = w->core.height, width = w->core.width;
    int marginWidth = w->text.marginWidth, marginHeight = w->text.marginHeight;
    int lineNumAreaWidth = w->text.lineNumCols == 0 ? 0 : w->text.marginWidth +
		fs->max_bounds.width * w->text.lineNumCols;

    w->text.columns = (width - marginWidth*2 - lineNumAreaWidth) /
    	    fs->max_bounds.width;
    w->text.rows = (height - marginHeight*2) / (fs->ascent + fs->descent);
    
    /* Reject widths and heights less than a character, which the text
       display can't tolerate.  This is not strictly legal, but I've seen
       it done in other widgets and it seems to do no serious harm.  NEdit
       prevents panes from getting smaller than one line, but sometimes
       splitting windows on Linux 2.0 systems (same Motif, why the change in
       behavior?), causes one or two resize calls with < 1 line of height.
       Fixing it here is 100x easier than re-designing textDisp.c */
    if (w->text.columns < 1) {
    	w->text.columns = 1;
    	w->core.width = width = fs->max_bounds.width + marginWidth*2 +
		lineNumAreaWidth;
    }
    if (w->text.rows < 1) {
    	w->text.rows = 1;
    	w->core.height = height = fs->ascent + fs->descent + marginHeight*2;
    }
    
    /* Resize the text display that the widget uses to render text */
    TextDResize(w->text.textD, width - marginWidth*2 - lineNumAreaWidth,
	    height - marginHeight*2);
    
    /* if the window became shorter or narrower, there may be text left
       in the bottom or right margin area, which must be cleaned up */
    if (XtIsRealized((Widget)w)) {
	XClearArea(XtDisplay(w), XtWindow(w), 0, height-marginHeight,
    		width, marginHeight, False);
	XClearArea(XtDisplay(w), XtWindow(w),width-marginWidth,
		0, marginWidth, height, False);
    }
}

/*
** Widget redisplay method
*/
static void redisplay(TextWidget w, XEvent *event, Region region)
{
    XExposeEvent *e = &event->xexpose;
    
    TextDRedisplayRect(w->text.textD, e->x, e->y, e->width, e->height);
}

static Bool findGraphicsExposeOrNoExposeEvent(Display *theDisplay, XEvent *event, XPointer arg)
{
    if ((theDisplay == event->xany.display) && 
        (event->type == GraphicsExpose || event->type == NoExpose) &&
        ((Widget)arg == XtWindowToWidget(event->xany.display, event->xany.window))) {
        return(True);
    }
    else {
        return(False);
    }
}

static void adjustRectForGraphicsExposeOrNoExposeEvent(TextWidget w, XEvent *event,
                            Boolean *first, int *left, int *top, int *width, int *height)
{
    Boolean removeQueueEntry = False;
    
    if (event->type == GraphicsExpose) {
        XGraphicsExposeEvent *e = &event->xgraphicsexpose;
        int x = e->x, y = e->y;

        TextDImposeGraphicsExposeTranslation(w->text.textD, &x, &y);
        if (*first) {
            *left = x;
            *top = y;
            *width = e->width;
            *height = e->height;
            
            *first = False;
        }
        else {
            int prev_left = *left;
            int prev_top = *top;
            
            *left = min(*left, x);
            *top = min(*top, y);
            *width = max(prev_left + *width, x + e->width) - *left;
            *height = max(prev_top + *height, y + e->height) - *top;
        }
        if (e->count == 0) {
            removeQueueEntry = True;
        }
    }
    else if (event->type == NoExpose) {
        removeQueueEntry = True;
    }
    if (removeQueueEntry) {
        TextDPopGraphicExposeQueueEntry(w->text.textD);
    }
}

static void redisplayGE(TextWidget w, XtPointer client_data,
                    XEvent *event, Boolean *continue_to_dispatch_return)
{
    if (event->type == GraphicsExpose || event->type == NoExpose) {
        HandleAllPendingGraphicsExposeNoExposeEvents(w, event);
    }
}

void HandleAllPendingGraphicsExposeNoExposeEvents(TextWidget w, XEvent *event)
{
    XEvent foundEvent;
    int left;
    int top;
    int width;
    int height;
    Boolean invalidRect = True;

    if (event) {
        adjustRectForGraphicsExposeOrNoExposeEvent(w, event, &invalidRect, &left, &top, &width, &height);
    }
    while (XCheckIfEvent(XtDisplay(w), &foundEvent, findGraphicsExposeOrNoExposeEvent, (XPointer)w)) {
        adjustRectForGraphicsExposeOrNoExposeEvent(w, &foundEvent, &invalidRect, &left, &top, &width, &height);
    }
    if (!invalidRect) {
        TextDRedisplayRect(w->text.textD, left, top, width, height);
    }
}

/*
** Widget setValues method
*/
static Boolean setValues(TextWidget current, TextWidget request,
	TextWidget new)
{
    Boolean redraw = False, reconfigure = False;
    
    if (new->text.overstrike != current->text.overstrike) {
    	if (current->text.textD->cursorStyle == BLOCK_CURSOR)
    	    TextDSetCursorStyle(current->text.textD,
    	    	    current->text.heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
    	else if (current->text.textD->cursorStyle == NORMAL_CURSOR ||
    		current->text.textD->cursorStyle == HEAVY_CURSOR)
    	    TextDSetCursorStyle(current->text.textD, BLOCK_CURSOR);
    }
    
    if (new->text.fontStruct != current->text.fontStruct) {
	if (new->text.lineNumCols != 0)
	    reconfigure = True;
    	TextDSetFont(current->text.textD, new->text.fontStruct);
    }
    
    if (new->text.wrapMargin != current->text.wrapMargin ||
    	    new->text.continuousWrap != current->text.continuousWrap)
    	TextDSetWrapMode(current->text.textD, new->text.continuousWrap,
    	    	new->text.wrapMargin);
    
    /* When delimiters are changed, copy the memory, so that the caller
       doesn't have to manage it, and add mandatory delimiters blank,
       tab, and newline to the list */
    if (new->text.delimiters != current->text.delimiters) {
	char *delimiters = (char*)NEditMalloc(strlen(new->text.delimiters) + 4);
    	NEditFree(current->text.delimiters);
	sprintf(delimiters, "%s%s", " \t\n", new->text.delimiters);
	new->text.delimiters = delimiters;
    }
    
    /* Setting the lineNumCols resource tells the text widget to hide or
       show, or change the number of columns of the line number display,
       which requires re-organizing the x coordinates of both the line
       number display and the main text display */
    if (new->text.lineNumCols != current->text.lineNumCols || reconfigure)
    {
        int marginWidth = new->text.marginWidth;
        int charWidth = new->text.fontStruct->max_bounds.width;
        int lineNumCols = new->text.lineNumCols;
        if (lineNumCols == 0)
        {
            TextDSetLineNumberArea(new->text.textD, 0, 0, marginWidth);
            new->text.columns = (new->core.width - marginWidth*2) / charWidth;
        } else
        {
            TextDSetLineNumberArea(new->text.textD, marginWidth,
                    charWidth * lineNumCols,
                    2*marginWidth + charWidth * lineNumCols);
            new->text.columns = (new->core.width - marginWidth*3 - charWidth
                    * lineNumCols) / charWidth;
        }
    }

    if (new->text.backlightCharTypes != current->text.backlightCharTypes)
    {
        TextDSetupBGClasses((Widget)new, new->text.backlightCharTypes,
                &new->text.textD->bgClassPixel, &new->text.textD->bgClass,
                new->text.textD->bgPixel);
        redraw = True;
    }
    
    return redraw;
} 

/*
** Widget realize method
*/
static void realize(Widget w, XtValueMask *valueMask,
	XSetWindowAttributes *attributes)
{
    /* Set bit gravity window attribute.  This saves a full blank and redraw
       on window resizing */
    *valueMask |= CWBitGravity;
    attributes->bit_gravity = NorthWestGravity;
        
    /* Continue with realize method from superclass */
    (xmPrimitiveClassRec.core_class.realize)(w, valueMask, attributes);
}

/*
** Widget query geometry method ... unless asked to negotiate a different size simply return current size.
*/
static XtGeometryResult queryGeometry(Widget w, XtWidgetGeometry *proposed,
	XtWidgetGeometry *answer)
{
    TextWidget tw = (TextWidget)w;
    
    int curHeight = tw->core.height;
    int curWidth = tw->core.width;
    XFontStruct *fs = tw->text.textD->fontStruct;
    int fontWidth = fs->max_bounds.width;
    int fontHeight = fs->ascent + fs->descent;
    int marginHeight = tw->text.marginHeight;
    int propWidth = (proposed->request_mode & CWWidth) ? proposed->width : 0;
    int propHeight = (proposed->request_mode & CWHeight) ? proposed->height : 0;
    
    answer->request_mode = CWHeight | CWWidth;
    
    if(proposed->request_mode & CWWidth)
       /* Accept a width no smaller than 10 chars */
       answer->width = max(fontWidth * 10, proposed->width);
    else
       answer->width = curWidth;
    
    if(proposed->request_mode & CWHeight)
       /* Accept a height no smaller than an exact multiple of the line height
          and at least one line high */
       answer->height = max(1, ((propHeight - 2*marginHeight) / fontHeight)) *
    	    fontHeight + 2*marginHeight;
    else
       answer->height = curHeight;
    
    /*printf("propWidth %d, propHeight %d, ansWidth %d, ansHeight %d\n",
    	    propWidth, propHeight, answer->width, answer->height);*/
    if (propWidth == answer->width && propHeight == answer->height)
    	return XtGeometryYes;
    else if (answer->width == curWidth && answer->height == curHeight)
    	return XtGeometryNo;
    else
    	return XtGeometryAlmost;
}
    
/*
** Set the text buffer which this widget will display and interact with.
** The currently attached buffer is automatically freed, ONLY if it has
** no additional modify procs attached (as it would if it were being
** displayed by another text widget).
*/
void TextSetBuffer(Widget w, textBuffer *buffer)
{
    textBuffer *oldBuf = ((TextWidget)w)->text.textD->buffer;
    
    StopHandlingXSelections(w);
    TextDSetBuffer(((TextWidget)w)->text.textD, buffer);
    if (oldBuf->nModifyProcs == 0)
    	BufFree(oldBuf);
}

/*
** Get the buffer associated with this text widget.  Note that attaching
** additional modify callbacks to the buffer will prevent it from being
** automatically freed when the widget is destroyed.
*/
textBuffer *TextGetBuffer(Widget w)
{
    return ((TextWidget)w)->text.textD->buffer;
}

/*
** Translate a line number and column into a position
*/
int TextLineAndColToPos(Widget w, int lineNum, int column)
{
    return TextDLineAndColToPos(((TextWidget)w)->text.textD, lineNum, column );
}

/*
** Translate a position into a line number (if the position is visible,
** if it's not, return False
*/
int TextPosToLineAndCol(Widget w, int pos, int *lineNum, int *column)
{
    return TextDPosToLineAndCol(((TextWidget)w)->text.textD, pos, lineNum,
    	    column);
}

/*
** Translate a buffer text position to the XY location where the center
** of the cursor would be positioned to point to that character.  Returns
** False if the position is not displayed because it is VERTICALLY out
** of view.  If the position is horizontally out of view, returns the
** x coordinate where the position would be if it were visible.
*/
int TextPosToXY(Widget w, int pos, int *x, int *y)
{
    return TextDPositionToXY(((TextWidget)w)->text.textD, pos, x, y);
}

/*
** Return the cursor position
*/
int TextGetCursorPos(Widget w)
{
    return TextDGetInsertPosition(((TextWidget)w)->text.textD);
}

/*
** Set the cursor position
*/
void TextSetCursorPos(Widget w, int pos)
{
    TextDSetInsertPosition(((TextWidget)w)->text.textD, pos);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, NULL);

}

/*
** Return the horizontal and vertical scroll positions of the widget
*/
void TextGetScroll(Widget w, int *topLineNum, int *horizOffset)
{
    TextDGetScroll(((TextWidget)w)->text.textD, topLineNum, horizOffset);
}

/*
** Set the horizontal and vertical scroll positions of the widget
*/
void TextSetScroll(Widget w, int topLineNum, int horizOffset)
{
    TextDSetScroll(((TextWidget)w)->text.textD, topLineNum, horizOffset);
}

int TextGetMinFontWidth(Widget w, Boolean considerStyles)
{
    return(TextDMinFontWidth(((TextWidget)w)->text.textD, considerStyles));
}

int TextGetMaxFontWidth(Widget w, Boolean considerStyles)
{
    return(TextDMaxFontWidth(((TextWidget)w)->text.textD, considerStyles));
}

/*
** Set this widget to be the owner of selections made in it's attached
** buffer (text buffers may be shared among several text widgets).
*/
void TextHandleXSelections(Widget w)
{
    HandleXSelections(w);
}

void TextPasteClipboard(Widget w, Time time)
{
    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    TakeMotifDestination(w, time);
    InsertClipboard(w, False);
    callCursorMovementCBs(w, NULL);
}

void TextColPasteClipboard(Widget w, Time time)
{
    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    TakeMotifDestination(w, time);
    InsertClipboard(w, True);
    callCursorMovementCBs(w, NULL);
}

void TextCopyClipboard(Widget w, Time time)
{
    cancelDrag(w);
    if (!((TextWidget)w)->text.textD->buffer->primary.selected) {
    	XBell(XtDisplay(w), 0);
    	return;
    }
    CopyToClipboard(w, time);
}

void TextCutClipboard(Widget w, Time time)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    
    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    if (!textD->buffer->primary.selected) {
    	XBell(XtDisplay(w), 0);
    	return;
    }
    TakeMotifDestination(w, time);
    CopyToClipboard (w, time);
    BufRemoveSelected(textD->buffer);
    TextDSetInsertPosition(textD, textD->buffer->cursorPosHint);
    checkAutoShowInsertPos(w);
}

int TextFirstVisibleLine(Widget w)
{
    return(((TextWidget)w)->text.textD->topLineNum);
}

int TextNumVisibleLines(Widget w)
{
    return(((TextWidget)w)->text.textD->nVisibleLines);
}

int TextVisibleWidth(Widget w)
{
    return(((TextWidget)w)->text.textD->width);
}

int TextFirstVisiblePos(Widget w)
{
    return ((TextWidget)w)->text.textD->firstChar;
}

int TextLastVisiblePos(Widget w)
{
    return ((TextWidget)w)->text.textD->lastChar;
}
	
/*
** Insert text "chars" at the cursor position, respecting pending delete
** selections, overstrike, and handling cursor repositioning as if the text
** had been typed.  If autoWrap is on wraps the text to fit within the wrap
** margin, auto-indenting where the line was wrapped (but nowhere else).
** "allowPendingDelete" controls whether primary selections in the widget are
** treated as pending delete selections (True), or ignored (False). "event"
** is optional and is just passed on to the cursor movement callbacks.
*/
void TextInsertAtCursor(Widget w, char *chars, XEvent *event,
    	int allowPendingDelete, int allowWrap)
{
    int wrapMargin, colNum, lineStartPos, cursorPos;
    char *c, *lineStartText, *wrappedText;
    TextWidget tw = (TextWidget)w;
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    int fontWidth = textD->fontStruct->max_bounds.width;
    int replaceSel, singleLine, breakAt = 0;

    /* Don't wrap if auto-wrap is off or suppressed, or it's just a newline */
    if (!allowWrap || !tw->text.autoWrap ||
	    (chars[0] == '\n' && chars[1] == '\0')) {
    	simpleInsertAtCursor(w, chars, event, allowPendingDelete);
        return;
    }

    /* If this is going to be a pending delete operation, the real insert
       position is the start of the selection.  This will make rectangular
       selections wrap strangely, but this routine should rarely be used for
       them, and even more rarely when they need to be wrapped. */
    replaceSel = allowPendingDelete && pendingSelection(w);
    cursorPos = replaceSel ? buf->primary.start : TextDGetInsertPosition(textD);
    
    /* If the text is only one line and doesn't need to be wrapped, just insert
       it and be done (for efficiency only, this routine is called for each
       character typed). (Of course, it may not be significantly more efficient
       than the more general code below it, so it may be a waste of time!) */
    wrapMargin = tw->text.wrapMargin != 0 ? tw->text.wrapMargin :
            textD->width / fontWidth;
    lineStartPos = BufStartOfLine(buf, cursorPos);
    colNum = BufCountDispChars(buf, lineStartPos, cursorPos);
    for (c=chars; *c!='\0' && *c!='\n'; c++)
        colNum += BufCharWidth(*c, colNum, buf->tabDist, buf->nullSubsChar);
    singleLine = *c == '\0';
    if (colNum < wrapMargin && singleLine) {
    	simpleInsertAtCursor(w, chars, event, True);
        return;
    }
    
    /* Wrap the text */
    lineStartText = BufGetRange(buf, lineStartPos, cursorPos);
    wrappedText = wrapText(tw, lineStartText, chars, lineStartPos, wrapMargin,
    	    replaceSel ? NULL : &breakAt);
    NEditFree(lineStartText);
    
    /* Insert the text.  Where possible, use TextDInsert which is optimized
       for less redraw. */
    if (replaceSel) {
    	BufReplaceSelected(buf, wrappedText);
   	TextDSetInsertPosition(textD, buf->cursorPosHint);
    } else if (tw->text.overstrike) {
    	if (breakAt == 0 && singleLine)
    	    TextDOverstrike(textD, wrappedText);
    	else {
	    BufReplace(buf, cursorPos-breakAt, cursorPos, wrappedText);
	    TextDSetInsertPosition(textD, buf->cursorPosHint);
    	}
    } else {
    	if (breakAt == 0) {
    	    TextDInsert(textD, wrappedText);
    	} else {
    	    BufReplace(buf, cursorPos-breakAt, cursorPos, wrappedText);
    	    TextDSetInsertPosition(textD, buf->cursorPosHint);
    	}
    }
    NEditFree(wrappedText);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

/*
** Fetch text from the widget's buffer, adding wrapping newlines to emulate
** effect acheived by wrapping in the text display in continuous wrap mode.
*/
char *TextGetWrapped(Widget w, int startPos, int endPos, int *outLen)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    textBuffer *outBuf;
    int fromPos, toPos, outPos;
    char c, *outString;
    
    if (!((TextWidget)w)->text.continuousWrap || startPos == endPos) {
    	*outLen = endPos - startPos;
    	return BufGetRange(buf, startPos, endPos);
    }
    
    /* Create a text buffer with a good estimate of the size that adding
       newlines will expand it to.  Since it's a text buffer, if we guess
       wrong, it will fail softly, and simply expand the size */
    outBuf = BufCreatePreallocated((endPos-startPos) + (endPos-startPos)/5);
    outPos = 0;
    
    /* Go (displayed) line by line through the buffer, adding newlines where
       the text is wrapped at some character other than an existing newline */
    fromPos = startPos;
    toPos = TextDCountForwardNLines(textD, startPos, 1, False);
    while (toPos < endPos) {
    	BufCopyFromBuf(buf, outBuf, fromPos, toPos, outPos);
    	outPos += toPos - fromPos;
    	c = BufGetCharacter(outBuf, outPos-1);
    	if (c == ' ' || c == '\t')
    	    BufReplace(outBuf, outPos-1, outPos, "\n");
    	else if (c != '\n') {
    	    BufInsert(outBuf, outPos, "\n");
    	    outPos++;
    	}
    	fromPos = toPos;
    	toPos = TextDCountForwardNLines(textD, fromPos, 1, True);
    }
    BufCopyFromBuf(buf, outBuf, fromPos, endPos, outPos);
    
    /* return the contents of the output buffer as a string */
    outString = BufGetAll(outBuf);
    *outLen = outBuf->length;
    BufFree(outBuf);
    return outString;
}

/*
** Return the (statically allocated) action table for menu item actions.
**
** Warning: This routine can only be used before the first text widget is
** created!  After that, apparently, Xt takes over the table and overwrites
** it with its own version.  XtGetActionList is preferable, but is not
** available before X11R5.
*/
XtActionsRec *TextGetActions(int *nActions)
{
    *nActions = XtNumber(actionsList);
    return actionsList;
}

static void grabFocusAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    XButtonEvent *e = &event->xbutton;
    TextWidget tw = (TextWidget)w;
    textDisp *textD = tw->text.textD;
    Time lastBtnDown = tw->text.lastBtnDown;
    int row, column;
    
    /* Indicate state for future events, PRIMARY_CLICKED indicates that
       the proper initialization has been done for primary dragging and/or
       multi-clicking.  Also record the timestamp for multi-click processing */
    tw->text.dragState = PRIMARY_CLICKED;
    tw->text.lastBtnDown = e->time;

    /* Become owner of the MOTIF_DESTINATION selection, making this widget
       the designated recipient of secondary quick actions in Motif XmText
       widgets and in other NEdit text widgets */
    TakeMotifDestination(w, e->time);
    
    /* Check for possible multi-click sequence in progress */
    if (tw->text.multiClickState != NO_CLICKS) {
	if (e->time < lastBtnDown + XtGetMultiClickTime(XtDisplay(w))) {
    	    if (tw->text.multiClickState == ONE_CLICK) {
    		selectWord(w, e->x);
    		callCursorMovementCBs(w, event);
		return;
    	    } else if (tw->text.multiClickState == TWO_CLICKS) {
    		selectLine(w);
    		callCursorMovementCBs(w, event);
    		return;
    	    } else if (tw->text.multiClickState == THREE_CLICKS) {
    		BufSelect(textD->buffer, 0, textD->buffer->length);
    		return;
    	    } else if (tw->text.multiClickState > THREE_CLICKS)
    		tw->text.multiClickState = NO_CLICKS;
	} else
    	    tw->text.multiClickState = NO_CLICKS;
    }
    
    /* Clear any existing selections */
    BufUnselect(textD->buffer);
    
    /* Move the cursor to the pointer location */
    moveDestinationAP(w, event, args, nArgs);
    
    /* Record the site of the initial button press and the initial character
       position so subsequent motion events and clicking can decide when and
       where to begin a primary selection */
    tw->text.btnDownX = e->x;
    tw->text.btnDownY = e->y;
    tw->text.anchor = TextDGetInsertPosition(textD);
    TextDXYToUnconstrainedPosition(textD, e->x, e->y, &row, &column);
    column = TextDOffsetWrappedColumn(textD, row, column);
    tw->text.rectAnchor = column;
}

static void moveDestinationAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XButtonEvent *e = &event->xbutton;
    textDisp *textD = ((TextWidget)w)->text.textD;
 
    /* Get input focus */
    XmProcessTraversal(w, XmTRAVERSE_CURRENT);

    /* Move the cursor */
    TextDSetInsertPosition(textD, TextDXYToPosition(textD, e->x, e->y));
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void extendAdjustAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    TextWidget tw = (TextWidget)w;
    XMotionEvent *e = &event->xmotion;
    int dragState = tw->text.dragState;
    int rectDrag = hasKey("rect", args, nArgs);

    /* Make sure the proper initialization was done on mouse down */
    if (dragState != PRIMARY_DRAG && dragState != PRIMARY_CLICKED &&
    	    dragState != PRIMARY_RECT_DRAG)
    	return;
    	
    /* If the selection hasn't begun, decide whether the mouse has moved
       far enough from the initial mouse down to be considered a drag */
    if (tw->text.dragState == PRIMARY_CLICKED) {
    	if (abs(e->x - tw->text.btnDownX) > SELECT_THRESHOLD ||
    	    	abs(e->y - tw->text.btnDownY) > SELECT_THRESHOLD)
    	    tw->text.dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;
    	else
    	    return;
    }
    
    /* If "rect" argument has appeared or disappeared, keep dragState up
       to date about which type of drag this is */
    tw->text.dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;
    
    /* Record the new position for the autoscrolling timer routine, and
       engage or disengage the timer if the mouse is in/out of the window */
    checkAutoScroll(tw, e->x, e->y);
    
    /* Adjust the selection and move the cursor */
    adjustSelection(tw, e->x, e->y);
}

static void extendStartAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{    
    XMotionEvent *e = &event->xmotion;
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    selection *sel = &buf->primary;
    int anchor, rectAnchor, anchorLineStart, newPos, row, column;

    /* Find the new anchor point for the rest of this drag operation */
    newPos = TextDXYToPosition(textD, e->x, e->y);
    TextDXYToUnconstrainedPosition(textD, e->x, e->y, &row, &column);
    column = TextDOffsetWrappedColumn(textD, row, column);
    if (sel->selected) {
    	if (sel->rectangular) {
    	    rectAnchor = column < (sel->rectEnd + sel->rectStart) / 2 ?
    	    	    sel->rectEnd : sel->rectStart;
    	    anchorLineStart = BufStartOfLine(buf, newPos <
    	    	    (sel->end + sel->start) / 2 ? sel->end : sel->start);
    	    anchor = BufCountForwardDispChars(buf, anchorLineStart, rectAnchor);
    	} else {
    	    if (abs(newPos - sel->start) < abs(newPos - sel->end))
    		anchor = sel->end;
    	    else
    		anchor = sel->start;
    	    anchorLineStart = BufStartOfLine(buf, anchor);
    	    rectAnchor = BufCountDispChars(buf, anchorLineStart, anchor);
    	}
    } else {
    	anchor = TextDGetInsertPosition(textD);
    	anchorLineStart = BufStartOfLine(buf, anchor);
    	rectAnchor = BufCountDispChars(buf, anchorLineStart, anchor);
    }
    ((TextWidget)w)->text.anchor = anchor;
    ((TextWidget)w)->text.rectAnchor = rectAnchor;

    /* Make the new selection */
    if (hasKey("rect", args, nArgs))
	BufRectSelect(buf, BufStartOfLine(buf, min(anchor, newPos)),
		BufEndOfLine(buf, max(anchor, newPos)),
		min(rectAnchor, column), max(rectAnchor, column));
    else
    	BufSelect(buf, min(anchor, newPos), max(anchor, newPos));
    
    /* Never mind the motion threshold, go right to dragging since
       extend-start is unambiguously the start of a selection */
    ((TextWidget)w)->text.dragState = PRIMARY_DRAG;
    
    /* Don't do by-word or by-line adjustment, just by character */
    ((TextWidget)w)->text.multiClickState = NO_CLICKS;

    /* Move the cursor */
    TextDSetInsertPosition(textD, newPos);
    callCursorMovementCBs(w, event);
}

static void extendEndAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XButtonEvent *e = &event->xbutton;
    TextWidget tw = (TextWidget)w;
    
    if (tw->text.dragState == PRIMARY_CLICKED &&
    	    tw->text.lastBtnDown <= e->time + XtGetMultiClickTime(XtDisplay(w)))
    	tw->text.multiClickState++;
    endDrag(w);
}

static void processCancelAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    int dragState = ((TextWidget)w)->text.dragState;
    textBuffer *buf = ((TextWidget)w)->text.textD->buffer;
    textDisp *textD = ((TextWidget)w)->text.textD;

    /* If there's a calltip displayed, kill it. */
    TextDKillCalltip(textD, 0);
    
    if (dragState == PRIMARY_DRAG || dragState == PRIMARY_RECT_DRAG)
    	BufUnselect(buf);
    cancelDrag(w);
}

static void secondaryStartAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{    
    XMotionEvent *e = &event->xmotion;
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    selection *sel = &buf->secondary;
    int anchor, pos, row, column;

    /* Find the new anchor point and make the new selection */
    pos = TextDXYToPosition(textD, e->x, e->y);
    if (sel->selected) {
    	if (abs(pos - sel->start) < abs(pos - sel->end))
    	    anchor = sel->end;
    	else
    	    anchor = sel->start;
    	BufSecondarySelect(buf, anchor, pos);
    } else
    	anchor = pos;

    /* Record the site of the initial button press and the initial character
       position so subsequent motion events can decide when to begin a
       selection, (and where the selection began) */
    ((TextWidget)w)->text.btnDownX = e->x;
    ((TextWidget)w)->text.btnDownY = e->y;
    ((TextWidget)w)->text.anchor = pos;
    TextDXYToUnconstrainedPosition(textD, e->x, e->y, &row, &column);
    column = TextDOffsetWrappedColumn(textD, row, column);
    ((TextWidget)w)->text.rectAnchor = column;
    ((TextWidget)w)->text.dragState = SECONDARY_CLICKED;
}

static void secondaryOrDragStartAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{    
    XMotionEvent *e = &event->xmotion;
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;

    /* If the click was outside of the primary selection, this is not
       a drag, start a secondary selection */
    if (!buf->primary.selected || !TextDInSelection(textD, e->x, e->y)) {
    	secondaryStartAP(w, event, args, nArgs);
    	return;
    }

    if (checkReadOnly(w))
	return;

    /* Record the site of the initial button press and the initial character
       position so subsequent motion events can decide when to begin a
       drag, and where to drag to */
    ((TextWidget)w)->text.btnDownX = e->x;
    ((TextWidget)w)->text.btnDownY = e->y;
    ((TextWidget)w)->text.dragState = CLICKED_IN_SELECTION;
}

static void secondaryAdjustAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    TextWidget tw = (TextWidget)w;
    XMotionEvent *e = &event->xmotion;
    int dragState = tw->text.dragState;
    int rectDrag = hasKey("rect", args, nArgs);

    /* Make sure the proper initialization was done on mouse down */
    if (dragState != SECONDARY_DRAG && dragState != SECONDARY_RECT_DRAG &&
    	    dragState != SECONDARY_CLICKED)
    	return;
    	
    /* If the selection hasn't begun, decide whether the mouse has moved
       far enough from the initial mouse down to be considered a drag */
    if (tw->text.dragState == SECONDARY_CLICKED) {
    	if (abs(e->x - tw->text.btnDownX) > SELECT_THRESHOLD ||
    	    	abs(e->y - tw->text.btnDownY) > SELECT_THRESHOLD)
    	    tw->text.dragState = rectDrag ? SECONDARY_RECT_DRAG: SECONDARY_DRAG;
    	else
    	    return;
    }
    
    /* If "rect" argument has appeared or disappeared, keep dragState up
       to date about which type of drag this is */
    tw->text.dragState = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;
    
    /* Record the new position for the autoscrolling timer routine, and
       engage or disengage the timer if the mouse is in/out of the window */
    checkAutoScroll(tw, e->x, e->y);
    
    /* Adjust the selection */
    adjustSecondarySelection(tw, e->x, e->y);
}

static void secondaryOrDragAdjustAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    TextWidget tw = (TextWidget)w;
    XMotionEvent *e = &event->xmotion;
    int dragState = tw->text.dragState;

    /* Only dragging of blocks of text is handled in this action proc.
       Otherwise, defer to secondaryAdjust to handle the rest */
    if (dragState != CLICKED_IN_SELECTION && dragState != PRIMARY_BLOCK_DRAG) {
     	secondaryAdjustAP(w, event, args, nArgs);
    	return;
    }
    
    /* Decide whether the mouse has moved far enough from the
       initial mouse down to be considered a drag */
    if (tw->text.dragState == CLICKED_IN_SELECTION) {
    	if (abs(e->x - tw->text.btnDownX) > SELECT_THRESHOLD ||
    	    	abs(e->y - tw->text.btnDownY) > SELECT_THRESHOLD)
    	    BeginBlockDrag(tw);
    	else
    	    return;
    }
    
    /* Record the new position for the autoscrolling timer routine, and
       engage or disengage the timer if the mouse is in/out of the window */
    checkAutoScroll(tw, e->x, e->y);
    
    /* Adjust the selection */
    BlockDragSelection(tw, e->x, e->y, hasKey("overlay", args, nArgs) ?
    	(hasKey("copy", args, nArgs) ? DRAG_OVERLAY_COPY : DRAG_OVERLAY_MOVE) :
    	(hasKey("copy", args, nArgs) ? DRAG_COPY : DRAG_MOVE));
}

static void copyToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    XButtonEvent *e = &event->xbutton;
    TextWidget tw = (TextWidget)w;
    textDisp *textD = tw->text.textD;
    int dragState = tw->text.dragState;
    textBuffer *buf = textD->buffer;
    selection *secondary = &buf->secondary, *primary = &buf->primary;
    int rectangular = secondary->rectangular;
    char *textToCopy;
    int insertPos, lineStart, column;

    endDrag(w);
    if (!((dragState == SECONDARY_DRAG && secondary->selected) ||
    	    (dragState == SECONDARY_RECT_DRAG && secondary->selected) ||
    	    dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED))
        return;
    if (!(secondary->selected && !((TextWidget)w)->text.motifDestOwner)) {
	if (checkReadOnly(w)) {
	    BufSecondaryUnselect(buf);
	    return;
	}
    }
    if (secondary->selected) {
    	if (tw->text.motifDestOwner) {
	    TextDBlankCursor(textD);
	    textToCopy = BufGetSecSelectText(buf);
	    if (primary->selected && rectangular) {
    		insertPos = TextDGetInsertPosition(textD);
    		BufReplaceSelected(buf, textToCopy);
    		TextDSetInsertPosition(textD, buf->cursorPosHint);
	    } else if (rectangular) {
    		insertPos = TextDGetInsertPosition(textD);
    		lineStart = BufStartOfLine(buf, insertPos);
    		column = BufCountDispChars(buf, lineStart, insertPos);
    		BufInsertCol(buf, column, lineStart, textToCopy, NULL, NULL);
    		TextDSetInsertPosition(textD, buf->cursorPosHint);
	    } else
	    	TextInsertAtCursor(w, textToCopy, event, True,
			tw->text.autoWrapPastedText);
	    NEditFree(textToCopy);
	    BufSecondaryUnselect(buf);
	    TextDUnblankCursor(textD);
	} else
	    SendSecondarySelection(w, e->time, False);
    } else if (primary->selected) {
	textToCopy = BufGetSelectionText(buf);
	TextDSetInsertPosition(textD, TextDXYToPosition(textD, e->x, e->y));
	TextInsertAtCursor(w, textToCopy, event, False,
			tw->text.autoWrapPastedText);
	NEditFree(textToCopy);
    } else {
    	TextDSetInsertPosition(textD, TextDXYToPosition(textD, e->x, e->y));
    	InsertPrimarySelection(w, e->time, False);
    }
}

static void copyToOrEndDragAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    int dragState = ((TextWidget)w)->text.dragState;

    if (dragState != PRIMARY_BLOCK_DRAG) {
    	copyToAP(w, event, args, nArgs);
    	return;
    }
    
    FinishBlockDrag((TextWidget)w);
}

static void moveToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    XButtonEvent *e = &event->xbutton;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int dragState = ((TextWidget)w)->text.dragState;
    textBuffer *buf = textD->buffer;
    selection *secondary = &buf->secondary, *primary = &buf->primary;
    int insertPos, rectangular = secondary->rectangular;
    int column, lineStart;
    char *textToCopy;

    endDrag(w);
    if (!((dragState == SECONDARY_DRAG && secondary->selected) ||
    	    (dragState == SECONDARY_RECT_DRAG && secondary->selected) ||
    	    dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED))
        return;
    if (checkReadOnly(w)) {
	BufSecondaryUnselect(buf);
	return;
    }
    
    if (secondary->selected) {
	if (((TextWidget)w)->text.motifDestOwner) {
	    textToCopy = BufGetSecSelectText(buf);
	    if (primary->selected && rectangular) {
    		insertPos = TextDGetInsertPosition(textD);
    		BufReplaceSelected(buf, textToCopy);
    		TextDSetInsertPosition(textD, buf->cursorPosHint);
	    } else if (rectangular) {
    		insertPos = TextDGetInsertPosition(textD);
    		lineStart = BufStartOfLine(buf, insertPos);
    		column = BufCountDispChars(buf, lineStart, insertPos);
    		BufInsertCol(buf, column, lineStart, textToCopy, NULL, NULL);
    		TextDSetInsertPosition(textD, buf->cursorPosHint);
	    } else
	    	TextInsertAtCursor(w, textToCopy, event, True,
			((TextWidget)w)->text.autoWrapPastedText);
	    NEditFree(textToCopy);
	    BufRemoveSecSelect(buf);
	    BufSecondaryUnselect(buf);
	} else
	    SendSecondarySelection(w, e->time, True);
    } else if (primary->selected) {
        textToCopy = BufGetRange(buf, primary->start, primary->end);
	TextDSetInsertPosition(textD, TextDXYToPosition(textD, e->x, e->y));
	TextInsertAtCursor(w, textToCopy, event, False,
			((TextWidget)w)->text.autoWrapPastedText);
	NEditFree(textToCopy);
	BufRemoveSelected(buf);
	BufUnselect(buf);
    } else {
    	TextDSetInsertPosition(textD, TextDXYToPosition(textD, e->x, e->y));
    	MovePrimarySelection(w, e->time, False);
    } 
}

static void moveToOrEndDragAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    int dragState = ((TextWidget)w)->text.dragState;

    if (dragState != PRIMARY_BLOCK_DRAG) {
    	moveToAP(w, event, args, nArgs);
    	return;
    }
    
    FinishBlockDrag((TextWidget)w);
}

static void endDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (((TextWidget)w)->text.dragState == PRIMARY_BLOCK_DRAG)
    	FinishBlockDrag((TextWidget)w);
    else
	endDrag(w);
}

static void exchangeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    XButtonEvent *e = &event->xbutton;
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    selection *sec = &buf->secondary, *primary = &buf->primary;
    char *primaryText, *secText;
    int newPrimaryStart, newPrimaryEnd, secWasRect;
    int dragState = ((TextWidget)w)->text.dragState; /* save before endDrag */
    int silent = hasKey("nobell", args, nArgs);
    
    endDrag(w);
    if (checkReadOnly(w))
	return;

    /* If there's no secondary selection here, or the primary and secondary
       selection overlap, just beep and return */
    if (!sec->selected || (primary->selected &&
    	    ((primary->start <= sec->start && primary->end > sec->start) ||
    	     (sec->start <= primary->start && sec->end > primary->start))))
    {
    	BufSecondaryUnselect(buf);
        ringIfNecessary(silent, w);
	/* If there's no secondary selection, but the primary selection is 
	   being dragged, we must not forget to finish the dragging.
	   Otherwise, modifications aren't recorded. */
	if (dragState == PRIMARY_BLOCK_DRAG)
	    FinishBlockDrag((TextWidget)w);
    	return;
    }
    
    /* if the primary selection is in another widget, use selection routines */
    if (!primary->selected) {
    	ExchangeSelections(w, e->time);
    	return;
    }
    
    /* Both primary and secondary are in this widget, do the exchange here */
    primaryText = BufGetSelectionText(buf);
    secText = BufGetSecSelectText(buf);
    secWasRect = sec->rectangular;
    BufReplaceSecSelect(buf, primaryText);
    newPrimaryStart = primary->start;
    BufReplaceSelected(buf, secText);
    newPrimaryEnd = newPrimaryStart + strlen(secText);
    NEditFree(primaryText);
    NEditFree(secText);
    BufSecondaryUnselect(buf);
    if (secWasRect) {
    	TextDSetInsertPosition(textD, buf->cursorPosHint);
    } else {
	BufSelect(buf, newPrimaryStart, newPrimaryEnd);
	TextDSetInsertPosition(textD, newPrimaryEnd);
    }
    checkAutoShowInsertPos(w);
}

static void copyPrimaryAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    TextWidget tw = (TextWidget)w;
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    selection *primary = &buf->primary;
    int rectangular = hasKey("rect", args, nArgs);
    char *textToCopy;
    int insertPos, col;

    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    if (primary->selected && rectangular) {
	textToCopy = BufGetSelectionText(buf);
	insertPos = TextDGetInsertPosition(textD);
	col = BufCountDispChars(buf, BufStartOfLine(buf, insertPos), insertPos);
	BufInsertCol(buf, col, insertPos, textToCopy, NULL, NULL);
	TextDSetInsertPosition(textD, buf->cursorPosHint);
	NEditFree(textToCopy);
	checkAutoShowInsertPos(w);
    } else if (primary->selected) {
	textToCopy = BufGetSelectionText(buf);
	insertPos = TextDGetInsertPosition(textD);
	BufInsert(buf, insertPos, textToCopy);
	TextDSetInsertPosition(textD, insertPos + strlen(textToCopy));
	NEditFree(textToCopy);
	checkAutoShowInsertPos(w);
    } else if (rectangular) {
    	if (!TextDPositionToXY(textD, TextDGetInsertPosition(textD),
    		&tw->text.btnDownX, &tw->text.btnDownY))
    	    return; /* shouldn't happen */
    	InsertPrimarySelection(w, e->time, True);
    } else
    	InsertPrimarySelection(w, e->time, False);
}

static void cutPrimaryAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    selection *primary = &buf->primary;
    char *textToCopy;
    int rectangular = hasKey("rect", args, nArgs);
    int insertPos, col;

    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    if (primary->selected && rectangular) {
	textToCopy = BufGetSelectionText(buf);
	insertPos = TextDGetInsertPosition(textD);
	col = BufCountDispChars(buf, BufStartOfLine(buf, insertPos), insertPos);
	BufInsertCol(buf, col, insertPos, textToCopy, NULL, NULL);
	TextDSetInsertPosition(textD, buf->cursorPosHint);
	NEditFree(textToCopy);
	BufRemoveSelected(buf);
	checkAutoShowInsertPos(w);
    } else if (primary->selected) {
	textToCopy = BufGetSelectionText(buf);
	insertPos = TextDGetInsertPosition(textD);
	BufInsert(buf, insertPos, textToCopy);
	TextDSetInsertPosition(textD, insertPos + strlen(textToCopy));
	NEditFree(textToCopy);
	BufRemoveSelected(buf);
	checkAutoShowInsertPos(w);
    } else if (rectangular) {
    	if (!TextDPositionToXY(textD, TextDGetInsertPosition(textD),
    	    	&((TextWidget)w)->text.btnDownX,
    	    	&((TextWidget)w)->text.btnDownY))
    	    return; /* shouldn't happen */
    	MovePrimarySelection(w, e->time, True);
    } else {
    	MovePrimarySelection(w, e->time, False);
    }
}

static void mousePanAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    XButtonEvent *e = &event->xbutton;
    TextWidget tw = (TextWidget)w;
    textDisp *textD = tw->text.textD;
    int lineHeight = textD->ascent + textD->descent;
    int topLineNum, horizOffset;
    static Cursor panCursor = 0;
    
    if (tw->text.dragState == MOUSE_PAN) {
    	TextDSetScroll(textD,
    		(tw->text.btnDownY - e->y + lineHeight/2) / lineHeight,
    		tw->text.btnDownX - e->x);
    } else if (tw->text.dragState == NOT_CLICKED) {
    	TextDGetScroll(textD, &topLineNum, &horizOffset);
    	tw->text.btnDownX = e->x + horizOffset;
    	tw->text.btnDownY = e->y + topLineNum * lineHeight;
    	tw->text.dragState = MOUSE_PAN;
    	if (!panCursor)
	    panCursor = XCreateFontCursor(XtDisplay(w), XC_fleur);
	XGrabPointer(XtDisplay(w), XtWindow(w), False,
	    	ButtonMotionMask | ButtonReleaseMask, GrabModeAsync,
	    	GrabModeAsync, None, panCursor, CurrentTime);
    } else
    	cancelDrag(w);
}

static void pasteClipboardAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    if (hasKey("rect", args, nArgs))
    	TextColPasteClipboard(w, event->xkey.time);
    else
	TextPasteClipboard(w, event->xkey.time);
}

static void copyClipboardAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    TextCopyClipboard(w, event->xkey.time);
}

static void cutClipboardAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    TextCutClipboard(w, event->xkey.time);
}

static void insertStringAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{   
    smartIndentCBStruct smartIndent;
    textDisp *textD = ((TextWidget)w)->text.textD;
    
    if (*nArgs == 0)
    	return;
    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    if (((TextWidget)w)->text.smartIndent) {
    	smartIndent.reason = CHAR_TYPED;
	smartIndent.pos = TextDGetInsertPosition(textD);
    	smartIndent.indentRequest = 0;
	smartIndent.charsTyped = args[0];
    	XtCallCallbacks(w, textNsmartIndentCallback, (XtPointer)&smartIndent);
    }
    TextInsertAtCursor(w, args[0], event, True, True);
    BufUnselect((((TextWidget)w)->text.textD)->buffer);
}

static void selfInsertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{   
    WindowInfo* window = WidgetToWindow(w);

#ifdef NO_XMIM
    static XComposeStatus compose = {NULL, 0};
#else
    int status;
#endif
    XKeyEvent *e = &event->xkey;
    char chars[20];
    KeySym keysym;
    int nChars;
    smartIndentCBStruct smartIndent;
    textDisp *textD = ((TextWidget)w)->text.textD;
    
#ifdef NO_XMIM
    nChars = XLookupString(&event->xkey, chars, 19, &keysym, &compose);
    if (nChars == 0)
    	return;
#else
    nChars = XmImMbLookupString(w, &event->xkey, chars, 19, &keysym,
     	   &status);
    if (nChars == 0 || status == XLookupNone ||
     	   status == XLookupKeySym || status == XBufferOverflow)
    	return;
#endif
    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    TakeMotifDestination(w, e->time);
    chars[nChars] = '\0';
    
    if (!BufSubstituteNullChars(chars, nChars, window->buffer)) {
        DialogF(DF_ERR, window->shell, 1, "Error", "Too much binary data", "OK");
        return;
    }

    /* If smart indent is on, call the smart indent callback to check the
       inserted character */
    if (((TextWidget)w)->text.smartIndent) {
    	smartIndent.reason = CHAR_TYPED;
	smartIndent.pos = TextDGetInsertPosition(textD);
    	smartIndent.indentRequest = 0;
	smartIndent.charsTyped = chars;
    	XtCallCallbacks(w, textNsmartIndentCallback, (XtPointer)&smartIndent);
    }
    TextInsertAtCursor(w, chars, event, True, True);
    BufUnselect(textD->buffer);
}

static void newlineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{   
    if (((TextWidget)w)->text.autoIndent || ((TextWidget)w)->text.smartIndent)
    	newlineAndIndentAP(w, event, args, nArgs);
    else
    	newlineNoIndentAP(w, event, args, nArgs);
}

static void newlineNoIndentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{   
    XKeyEvent *e = &event->xkey;

    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    TakeMotifDestination(w, e->time);
    simpleInsertAtCursor(w, "\n", event, True);
    BufUnselect((((TextWidget)w)->text.textD)->buffer);
}

static void newlineAndIndentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{   
    XKeyEvent *e = &event->xkey;
    TextWidget tw = (TextWidget)w;
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    char *indentStr;
    int cursorPos, lineStartPos, column;
    
    if (checkReadOnly(w))
	return;
    cancelDrag(w);
    TakeMotifDestination(w, e->time);
    
    /* Create a string containing a newline followed by auto or smart
       indent string */
    cursorPos = TextDGetInsertPosition(textD);
    lineStartPos = BufStartOfLine(buf, cursorPos);
    indentStr = createIndentString(tw, buf, 0, lineStartPos,
    	    cursorPos, NULL, &column);
    
    /* Insert it at the cursor */
    simpleInsertAtCursor(w, indentStr, event, True);
    NEditFree(indentStr);
    
    if (tw->text.emulateTabs > 0) {
        /*  If emulated tabs are on, make the inserted indent deletable by
            tab. Round this up by faking the column a bit to the right to
            let the user delete half-tabs with one keypress.  */
        column += tw->text.emulateTabs - 1;
        tw->text.emTabsBeforeCursor = column / tw->text.emulateTabs;
    }
    
    BufUnselect(buf);
}

static void processTabAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    selection *sel = &buf->primary;
    int emTabDist = ((TextWidget)w)->text.emulateTabs;
    int emTabsBeforeCursor = ((TextWidget)w)->text.emTabsBeforeCursor;
    int insertPos, indent, startIndent, toIndent, lineStart, tabWidth;
    char *outStr, *outPtr;
    
    if (checkReadOnly(w))
	return;
    cancelDrag(w);
    TakeMotifDestination(w, event->xkey.time);
    
    /* If emulated tabs are off, just insert a tab */
    if (emTabDist <= 0) {
	TextInsertAtCursor(w, "\t", event, True, True);
    	return;
    }

    /* Find the starting and ending indentation.  If the tab is to
       replace an existing selection, use the start of the selection
       instead of the cursor position as the indent.  When replacing
       rectangular selections, tabs are automatically recalculated as
       if the inserted text began at the start of the line */
    insertPos = pendingSelection(w) ?
    	    sel->start : TextDGetInsertPosition(textD);
    lineStart = BufStartOfLine(buf, insertPos);
    if (pendingSelection(w) && sel->rectangular)
    	insertPos = BufCountForwardDispChars(buf, lineStart, sel->rectStart);
    startIndent = BufCountDispChars(buf, lineStart, insertPos);
    toIndent = startIndent + emTabDist - (startIndent % emTabDist);
    if (pendingSelection(w) && sel->rectangular) {
    	toIndent -= startIndent;
    	startIndent = 0;
    }

    /* Allocate a buffer assuming all the inserted characters will be spaces */
    outStr = (char*)NEditMalloc(toIndent - startIndent + 1);

    /* Add spaces and tabs to outStr until it reaches toIndent */
    outPtr = outStr;
    indent = startIndent;
    while (indent < toIndent) {
    	tabWidth = BufCharWidth('\t', indent, buf->tabDist, buf->nullSubsChar);
    	if (buf->useTabs && tabWidth > 1 && indent + tabWidth <= toIndent) {
    	    *outPtr++ = '\t';
    	    indent += tabWidth;
    	} else {
    	    *outPtr++ = ' ';
    	    indent++;
    	}
    }
    *outPtr = '\0';
    
    /* Insert the emulated tab */
    TextInsertAtCursor(w, outStr, event, True, True);
    NEditFree(outStr);
    
    /* Restore and ++ emTabsBeforeCursor cleared by TextInsertAtCursor */
    ((TextWidget)w)->text.emTabsBeforeCursor = emTabsBeforeCursor + 1;

    BufUnselect(buf);
}

static void deleteSelectionAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;

    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    TakeMotifDestination(w, e->time);
    deletePendingSelection(w, event);
}

static void deletePreviousCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    char c;
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (checkReadOnly(w))
	return;

    TakeMotifDestination(w, e->time);
    if (deletePendingSelection(w, event))
    	return;

    if (insertPos == 0) {
        ringIfNecessary(silent, w);
    	return;
    }

    if (deleteEmulatedTab(w, event))
    	return;

    if (((TextWidget)w)->text.overstrike) {
    	c = BufGetCharacter(textD->buffer, insertPos - 1);
    	if (c == '\n')
    	    BufRemove(textD->buffer, insertPos - 1, insertPos);
    	else if (c != '\t')
    	    BufReplace(textD->buffer, insertPos - 1, insertPos, " ");
    } else {
    	BufRemove(textD->buffer, insertPos - 1, insertPos);
    }

    TextDSetInsertPosition(textD, insertPos - 1);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void deleteNextCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (checkReadOnly(w))
	return;
    TakeMotifDestination(w, e->time);
    if (deletePendingSelection(w, event))
    	return;
    if (insertPos == textD->buffer->length) {
        ringIfNecessary(silent, w);
    	return;
    }
    BufRemove(textD->buffer, insertPos , insertPos + 1);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void deletePreviousWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int pos, lineStart = BufStartOfLine(textD->buffer, insertPos);
    char *delimiters = ((TextWidget)w)->text.delimiters;
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (checkReadOnly(w)) {
        return;
    }

    TakeMotifDestination(w, e->time);
    if (deletePendingSelection(w, event)) {
        return;
    }

    if (insertPos == lineStart) {
        ringIfNecessary(silent, w);
        return;
    }

    pos = max(insertPos - 1, 0);
    while (strchr(delimiters, BufGetCharacter(textD->buffer, pos)) != NULL &&
            pos != lineStart) {
        pos--;
    }

    pos = startOfWord((TextWidget)w, pos);
    BufRemove(textD->buffer, pos, insertPos);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void deleteNextWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int pos, lineEnd = BufEndOfLine(textD->buffer, insertPos);
    char *delimiters = ((TextWidget)w)->text.delimiters;
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (checkReadOnly(w)) {
        return;
    }

    TakeMotifDestination(w, e->time);
    if (deletePendingSelection(w, event)) {
        return;
    }

    if (insertPos == lineEnd) {
        ringIfNecessary(silent, w);
        return;
    }

    pos = insertPos;
    while (strchr(delimiters, BufGetCharacter(textD->buffer, pos)) != NULL &&
            pos != lineEnd) {
        pos++;
    }

    pos = endOfWord((TextWidget)w, pos);
    BufRemove(textD->buffer, insertPos, pos);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void deleteToEndOfLineAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int endOfLine;
    int silent = 0;

    silent = hasKey("nobell", args, nArgs);
    if (hasKey("absolute", args, nArgs))
        endOfLine = BufEndOfLine(textD->buffer, insertPos);
    else
        endOfLine = TextDEndOfLine(textD, insertPos, False);
    cancelDrag(w);
    if (checkReadOnly(w))
        return;
    TakeMotifDestination(w, e->time);
    if (deletePendingSelection(w, event))
        return;
    if (insertPos == endOfLine) {
        ringIfNecessary(silent, w);
    	return;
    }
    BufRemove(textD->buffer, insertPos, endOfLine);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void deleteToStartOfLineAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    XKeyEvent *e = &event->xkey;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int startOfLine;
    int silent = 0;

    silent = hasKey("nobell", args, nArgs);
    if (hasKey("wrap", args, nArgs))
        startOfLine = TextDStartOfLine(textD, insertPos);
    else
        startOfLine = BufStartOfLine(textD->buffer, insertPos);
    cancelDrag(w);
    if (checkReadOnly(w))
        return;
    TakeMotifDestination(w, e->time);
    if (deletePendingSelection(w, event))
        return;
    if (insertPos == startOfLine) {
        ringIfNecessary(silent, w);
    	return;
    }
    BufRemove(textD->buffer, startOfLine, insertPos);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void forwardCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    int insertPos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (!TextDMoveRight(((TextWidget)w)->text.textD)) {
        ringIfNecessary(silent, w);
    }
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void backwardCharacterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    int insertPos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (!TextDMoveLeft(((TextWidget)w)->text.textD)) {
        ringIfNecessary(silent, w);
    }
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void forwardWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    int pos, insertPos = TextDGetInsertPosition(textD);
    char *delimiters = ((TextWidget)w)->text.delimiters;
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (insertPos == buf->length) {
        ringIfNecessary(silent, w);
    	return;
    }
    pos = insertPos;

    if (hasKey("tail", args, nArgs)) {
        for (; pos<buf->length; pos++) {
            if (NULL == strchr(delimiters, BufGetCharacter(buf, pos))) {
                break;
            }
        }
        if (NULL == strchr(delimiters, BufGetCharacter(buf, pos))) {
            pos = endOfWord((TextWidget)w, pos);
        }
    } else {
        if (NULL == strchr(delimiters, BufGetCharacter(buf, pos))) {
            pos = endOfWord((TextWidget)w, pos);
        }
        for (; pos<buf->length; pos++) {
            if (NULL == strchr(delimiters, BufGetCharacter(buf, pos))) {
                break;
            }
        }
    }

    TextDSetInsertPosition(textD, pos);
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void backwardWordAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    int pos, insertPos = TextDGetInsertPosition(textD);
    char *delimiters = ((TextWidget)w)->text.delimiters;
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (insertPos == 0) {
        ringIfNecessary(silent, w);
    	return;
    }
    pos = max(insertPos - 1, 0);
    while (strchr(delimiters, BufGetCharacter(buf, pos)) != NULL && pos > 0)
    	pos--;
    pos = startOfWord((TextWidget)w, pos);
    	
    TextDSetInsertPosition(textD, pos);
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void forwardParagraphAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int pos, insertPos = TextDGetInsertPosition(textD);
    textBuffer *buf = textD->buffer;
    char c;
    static char whiteChars[] = " \t";
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (insertPos == buf->length) {
        ringIfNecessary(silent, w);
    	return;
    }
    pos = min(BufEndOfLine(buf, insertPos)+1, buf->length);
    while (pos < buf->length) {
    	c = BufGetCharacter(buf, pos);
    	if (c == '\n')
    	    break;
    	if (strchr(whiteChars, c) != NULL)
    	    pos++;
    	else
    	    pos = min(BufEndOfLine(buf, pos)+1, buf->length);
    }
    TextDSetInsertPosition(textD, min(pos+1, buf->length));
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void backwardParagraphAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int parStart, pos, insertPos = TextDGetInsertPosition(textD);
    textBuffer *buf = textD->buffer;
    char c;
    static char whiteChars[] = " \t";
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (insertPos == 0) {
        ringIfNecessary(silent, w);
    	return;
    }
    parStart = BufStartOfLine(buf, max(insertPos-1, 0));
    pos = max(parStart - 2, 0);
    while (pos > 0) {
    	c = BufGetCharacter(buf, pos);
    	if (c == '\n')
    	    break;
    	if (strchr(whiteChars, c) != NULL)
    	    pos--;
    	else {
    	    parStart = BufStartOfLine(buf, pos);
    	    pos = max(parStart - 2, 0);
    	}
    }
    TextDSetInsertPosition(textD, parStart);
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void keySelectAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int stat, insertPos = TextDGetInsertPosition(textD);
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (hasKey("left", args, nArgs)) stat = TextDMoveLeft(textD);
    else if (hasKey("right", args, nArgs)) stat = TextDMoveRight(textD);
    else if (hasKey("up", args, nArgs)) stat = TextDMoveUp(textD, 0);
    else if (hasKey("down", args, nArgs)) stat = TextDMoveDown(textD, 0);
    else {
    	keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args,nArgs));
    	return;
    }
    if (!stat) {
        ringIfNecessary(silent, w);
    }
    else {
	keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args,nArgs));
	checkAutoShowInsertPos(w);
    	callCursorMovementCBs(w, event);
    }
}

static void processUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    int insertPos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
    int silent = hasKey("nobell", args, nArgs);
    int abs = hasKey("absolute", args, nArgs);

    cancelDrag(w);
    if (!TextDMoveUp(((TextWidget)w)->text.textD, abs))
        ringIfNecessary(silent, w);
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void processShiftUpAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    int insertPos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
    int silent = hasKey("nobell", args, nArgs);
    int abs = hasKey("absolute", args, nArgs);

    cancelDrag(w);
    if (!TextDMoveUp(((TextWidget)w)->text.textD, abs))
        ringIfNecessary(silent, w);
    keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void processDownAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    int insertPos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
    int silent = hasKey("nobell", args, nArgs);
    int abs = hasKey("absolute", args, nArgs);

    cancelDrag(w);
    if (!TextDMoveDown(((TextWidget)w)->text.textD, abs))
        ringIfNecessary(silent, w);
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void processShiftDownAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    int insertPos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
    int silent = hasKey("nobell", args, nArgs);
    int abs = hasKey("absolute", args, nArgs);

    cancelDrag(w);
    if (!TextDMoveDown(((TextWidget)w)->text.textD, abs))
        ringIfNecessary(silent, w);
    keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

static void beginningOfLineAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);

    cancelDrag(w);
    if (hasKey("absolute", args, nArgs))
        TextDSetInsertPosition(textD, BufStartOfLine(textD->buffer, insertPos));
    else
        TextDSetInsertPosition(textD, TextDStartOfLine(textD, insertPos));
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
    textD->cursorPreferredCol = 0;
}

static void endOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);

    cancelDrag(w);
    if (hasKey("absolute", args, nArgs))
        TextDSetInsertPosition(textD, BufEndOfLine(textD->buffer, insertPos));
    else
        TextDSetInsertPosition(textD, TextDEndOfLine(textD, insertPos, False));
    checkMoveSelectionChange(w, event, insertPos, args, nArgs);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
    textD->cursorPreferredCol = -1;
}

static void beginningOfFileAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    int insertPos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
	textDisp *textD = ((TextWidget)w)->text.textD;

    cancelDrag(w);
    if (hasKey("scrollbar", args, nArgs)) {
	if (textD->topLineNum != 1) {
	    TextDSetScroll(textD, 1, textD->horizOffset);
	}
    }
    else {
        TextDSetInsertPosition(((TextWidget)w)->text.textD, 0);
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
    }
}

static void endOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int lastTopLine;

    cancelDrag(w);
    if (hasKey("scrollbar", args, nArgs)) {
        lastTopLine = max(1, 
                textD->nBufferLines - (textD->nVisibleLines - 2) +
                ((TextWidget)w)->text.cursorVPadding);
        if (lastTopLine != textD->topLineNum) {
            TextDSetScroll(textD, lastTopLine, textD->horizOffset);
        }
    }
    else {
        TextDSetInsertPosition(textD, textD->buffer->length);
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
    }
}

static void nextPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    int lastTopLine = max(1, 
            textD->nBufferLines - (textD->nVisibleLines - 2) + 
            ((TextWidget)w)->text.cursorVPadding );
    int insertPos = TextDGetInsertPosition(textD);
    int column = 0, visLineNum, lineStartPos;
    int pos, targetLine;
    int pageForwardCount = max(1, textD->nVisibleLines - 1);
    int maintainColumn = 0;
    int silent = hasKey("nobell", args, nArgs);
    
    maintainColumn = hasKey("column", args, nArgs);
    cancelDrag(w);
    if (hasKey("scrollbar", args, nArgs)) { /* scrollbar only */
        targetLine = min(textD->topLineNum + pageForwardCount, lastTopLine);

        if (targetLine == textD->topLineNum) {
            ringIfNecessary(silent, w);
            return;
        }
        TextDSetScroll(textD, targetLine, textD->horizOffset);
    }
    else if (hasKey("stutter", args, nArgs)) { /* Mac style */
        /* move to bottom line of visible area */
        /* if already there, page down maintaining preferrred column */
        targetLine = max(min(textD->nVisibleLines - 1, textD->nBufferLines), 0);
        column = TextDPreferredColumn(textD, &visLineNum, &lineStartPos);
        if (lineStartPos == textD->lineStarts[targetLine]) {
            if (insertPos >= buf->length || textD->topLineNum == lastTopLine) {
                ringIfNecessary(silent, w);
                return;
            }
            targetLine = min(textD->topLineNum + pageForwardCount, lastTopLine);
            pos = TextDCountForwardNLines(textD, insertPos, pageForwardCount, False);
            if (maintainColumn) {
                pos = TextDPosOfPreferredCol(textD, column, pos);
            }
            TextDSetInsertPosition(textD, pos);
            TextDSetScroll(textD, targetLine, textD->horizOffset);
        }
        else {
            pos = textD->lineStarts[targetLine];
            while (targetLine > 0 && pos == -1) {
                --targetLine;
                pos = textD->lineStarts[targetLine];
            }
            if (lineStartPos == pos) {
                ringIfNecessary(silent, w);
                return;
            }
            if (maintainColumn) {
                pos = TextDPosOfPreferredCol(textD, column, pos);
            }
            TextDSetInsertPosition(textD, pos);
        }
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
        if (maintainColumn) {
            textD->cursorPreferredCol = column;
        }
        else {
            textD->cursorPreferredCol = -1;
        }
    }
    else { /* "standard" */
        if (insertPos >= buf->length && textD->topLineNum == lastTopLine) {
            ringIfNecessary(silent, w);
            return;
        }
        if (maintainColumn) {
            column = TextDPreferredColumn(textD, &visLineNum, &lineStartPos);
        }
        targetLine = textD->topLineNum + textD->nVisibleLines - 1;
        if (targetLine < 1) targetLine = 1;
        if (targetLine > lastTopLine) targetLine = lastTopLine;
        pos = TextDCountForwardNLines(textD, insertPos, textD->nVisibleLines-1, False);
        if (maintainColumn) {
            pos = TextDPosOfPreferredCol(textD, column, pos);
        }
        TextDSetInsertPosition(textD, pos);
        TextDSetScroll(textD, targetLine, textD->horizOffset);
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
        if (maintainColumn) {
            textD->cursorPreferredCol = column;
        }
        else {
            textD->cursorPreferredCol = -1;
        }
    }
}

static void previousPageAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int column = 0, visLineNum, lineStartPos;
    int pos, targetLine;
    int pageBackwardCount = max(1, textD->nVisibleLines - 1);
    int maintainColumn = 0;
    int silent = hasKey("nobell", args, nArgs);
    
    maintainColumn = hasKey("column", args, nArgs);
    cancelDrag(w);
    if (hasKey("scrollbar", args, nArgs)) { /* scrollbar only */
        targetLine = max(textD->topLineNum - pageBackwardCount, 1);

        if (targetLine == textD->topLineNum) {
            ringIfNecessary(silent, w);
            return;
        }
        TextDSetScroll(textD, targetLine, textD->horizOffset);
    }
    else if (hasKey("stutter", args, nArgs)) { /* Mac style */
        /* move to top line of visible area */
        /* if already there, page up maintaining preferrred column if required */
        targetLine = 0;
        column = TextDPreferredColumn(textD, &visLineNum, &lineStartPos);
        if (lineStartPos == textD->lineStarts[targetLine]) {
            if (textD->topLineNum == 1 && (maintainColumn || column == 0)) {
                ringIfNecessary(silent, w);
                return;
            }
            targetLine = max(textD->topLineNum - pageBackwardCount, 1);
            pos = TextDCountBackwardNLines(textD, insertPos, pageBackwardCount);
            if (maintainColumn) {
                pos = TextDPosOfPreferredCol(textD, column, pos);
            }
            TextDSetInsertPosition(textD, pos);
            TextDSetScroll(textD, targetLine, textD->horizOffset);
        }
        else {
            pos = textD->lineStarts[targetLine];
            if (maintainColumn) {
                pos = TextDPosOfPreferredCol(textD, column, pos);
            }
            TextDSetInsertPosition(textD, pos);
        }
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
        if (maintainColumn) {
            textD->cursorPreferredCol = column;
        }
        else {
            textD->cursorPreferredCol = -1;
        }
    }
    else { /* "standard" */
        if (insertPos <= 0 && textD->topLineNum == 1) {
            ringIfNecessary(silent, w);
            return;
        }
        if (maintainColumn) {
            column = TextDPreferredColumn(textD, &visLineNum, &lineStartPos);
        }
        targetLine = textD->topLineNum - (textD->nVisibleLines - 1);
        if (targetLine < 1) targetLine = 1;
        pos = TextDCountBackwardNLines(textD, insertPos, textD->nVisibleLines-1);
        if (maintainColumn) {
            pos = TextDPosOfPreferredCol(textD, column, pos);
        }
        TextDSetInsertPosition(textD, pos);
        TextDSetScroll(textD, targetLine, textD->horizOffset);
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
        if (maintainColumn) {
            textD->cursorPreferredCol = column;
        }
        else {
            textD->cursorPreferredCol = -1;
        }
    }
}

static void pageLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    int insertPos = TextDGetInsertPosition(textD);
    int maxCharWidth = textD->fontStruct->max_bounds.width;
    int lineStartPos, indent, pos;
    int horizOffset;
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (hasKey("scrollbar", args, nArgs)) {
	if (textD->horizOffset == 0) {
            ringIfNecessary(silent, w);
    	    return;
	}
	horizOffset = max(0, textD->horizOffset - textD->width);
	TextDSetScroll(textD, textD->topLineNum, horizOffset);
    }
    else {
        lineStartPos = BufStartOfLine(buf, insertPos);
        if (insertPos == lineStartPos && textD->horizOffset == 0) {
            ringIfNecessary(silent, w);
    	    return;
        }
        indent = BufCountDispChars(buf, lineStartPos, insertPos);
        pos = BufCountForwardDispChars(buf, lineStartPos,
    	        max(0, indent - textD->width / maxCharWidth));
        TextDSetInsertPosition(textD, pos);
        TextDSetScroll(textD, textD->topLineNum,
    	        max(0, textD->horizOffset - textD->width));
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
    }
}

static void pageRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    int insertPos = TextDGetInsertPosition(textD);
    int maxCharWidth = textD->fontStruct->max_bounds.width;
    int oldHorizOffset = textD->horizOffset;
    int lineStartPos, indent, pos;
    int horizOffset, sliderSize, sliderMax;
    int silent = hasKey("nobell", args, nArgs);
    
    cancelDrag(w);
    if (hasKey("scrollbar", args, nArgs)) {
        XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, 
    	    XmNsliderSize, &sliderSize, NULL);
	horizOffset = min(textD->horizOffset + textD->width, sliderMax - sliderSize);
	if (textD->horizOffset == horizOffset) {
            ringIfNecessary(silent, w);
    	    return;
	}
	TextDSetScroll(textD, textD->topLineNum, horizOffset);
    }
    else {
        lineStartPos = BufStartOfLine(buf, insertPos);
        indent = BufCountDispChars(buf, lineStartPos, insertPos);
        pos = BufCountForwardDispChars(buf, lineStartPos,
    	        indent + textD->width / maxCharWidth);
        TextDSetInsertPosition(textD, pos);
        TextDSetScroll(textD, textD->topLineNum, textD->horizOffset + textD->width);
        if (textD->horizOffset == oldHorizOffset && insertPos == pos)
            ringIfNecessary(silent, w);
        checkMoveSelectionChange(w, event, insertPos, args, nArgs);
        checkAutoShowInsertPos(w);
        callCursorMovementCBs(w, event);
    }
}

static void toggleOverstrikeAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    TextWidget tw = (TextWidget)w;
    
    if (tw->text.overstrike) {
    	tw->text.overstrike = False;
    	TextDSetCursorStyle(tw->text.textD,
    	    	tw->text.heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
    } else {
    	tw->text.overstrike = True;
    	if (    tw->text.textD->cursorStyle == NORMAL_CURSOR ||
    		tw->text.textD->cursorStyle == HEAVY_CURSOR)
    	    TextDSetCursorStyle(tw->text.textD, BLOCK_CURSOR);
    }
}

static void scrollUpAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int topLineNum, horizOffset, nLines;

    if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
    	return;
    if (*nArgs == 2) {
        /* Allow both 'page' and 'pages' */
        if (strncmp(args[1], "page", 4) == 0) 
            nLines *= textD->nVisibleLines;
            
        /* 'line' or 'lines' is the only other valid possibility */
        else if (strncmp(args[1], "line", 4) != 0)
            return;
    }
    TextDGetScroll(textD, &topLineNum, &horizOffset);
    TextDSetScroll(textD, topLineNum-nLines, horizOffset);
}

static void scrollDownAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int topLineNum, horizOffset, nLines;
    
    if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
    	return;
    if (*nArgs == 2) {
        /* Allow both 'page' and 'pages' */
        if (strncmp(args[1], "page", 4) == 0) 
            nLines *= textD->nVisibleLines;
            
        /* 'line' or 'lines' is the only other valid possibility */
        else if (strncmp(args[1], "line", 4) != 0)
            return;
    }
    TextDGetScroll(textD, &topLineNum, &horizOffset);
    TextDSetScroll(textD, topLineNum+nLines, horizOffset);
}

static void scrollLeftAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int horizOffset, nPixels;
    int sliderMax, sliderSize;
    
    if (*nArgs == 0 || sscanf(args[0], "%d", &nPixels) != 1)
    	return;
    XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, 
   	XmNsliderSize, &sliderSize, NULL);
    horizOffset = min(max(0, textD->horizOffset - nPixels), sliderMax - sliderSize);
    if (textD->horizOffset != horizOffset) {
	TextDSetScroll(textD, textD->topLineNum, horizOffset);
    }
}

static void scrollRightAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int horizOffset, nPixels;
    int sliderMax, sliderSize;
    
    if (*nArgs == 0 || sscanf(args[0], "%d", &nPixels) != 1)
    	return;
    XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, 
    	    XmNsliderSize, &sliderSize, NULL);
    horizOffset = min(max(0, textD->horizOffset + nPixels), sliderMax - sliderSize);
    if (textD->horizOffset != horizOffset) {
	TextDSetScroll(textD, textD->topLineNum, horizOffset);
    }
}

static void scrollToLineAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int topLineNum, horizOffset, lineNum;
    
    if (*nArgs == 0 || sscanf(args[0], "%d", &lineNum) != 1)
    	return;
    TextDGetScroll(textD, &topLineNum, &horizOffset);
    TextDSetScroll(textD, lineNum, horizOffset);
}
  
static void selectAllAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textBuffer *buf = ((TextWidget)w)->text.textD->buffer;

    cancelDrag(w);
    BufSelect(buf, 0, buf->length);
}

static void deselectAllAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    cancelDrag(w);
    BufUnselect(((TextWidget)w)->text.textD->buffer);
}

/*
**  Called on the Intrinsic FocusIn event.
**
**  Note that the widget has no internal state about the focus, ie. it does
**  not know whether it has the focus or not.
*/
static void focusInAP(Widget widget, XEvent* event, String* unused1,
        Cardinal* unused2)
{
    TextWidget textwidget = (TextWidget) widget;
    textDisp* textD = textwidget->text.textD;

    /* I don't entirely understand the traversal mechanism in Motif widgets,
       particularly, what leads to this widget getting a focus-in event when
       it does not actually have the input focus.  The temporary solution is
       to do the comparison below, and not show the cursor when Motif says
       we don't have focus, but keep looking for the real answer */
#if XmVersion >= 1002
    if (widget != XmGetFocusWidget(widget))
    	return;
#endif

    /* If the timer is not already started, start it */
    if (textwidget->text.cursorBlinkRate != 0
            && textwidget->text.cursorBlinkProcID == 0) {
        textwidget->text.cursorBlinkProcID
                = XtAppAddTimeOut(XtWidgetToApplicationContext(widget),
                    textwidget->text.cursorBlinkRate, cursorBlinkTimerProc,
                    widget);
    }

    /* Change the cursor to active style */
    if (textwidget->text.overstrike)
    	TextDSetCursorStyle(textD, BLOCK_CURSOR);
    else
        TextDSetCursorStyle(textD, (textwidget->text.heavyCursor
                ? HEAVY_CURSOR
                : NORMAL_CURSOR));
    TextDUnblankCursor(textD);

#ifndef NO_XMIM
    /* Notify Motif input manager that widget has focus */
    XmImVaSetFocusValues(widget, NULL);
#endif

    /* Call any registered focus-in callbacks */
    XtCallCallbacks((Widget) widget, textNfocusCallback, (XtPointer) event);
}

static void focusOutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    
    /* Remove the cursor blinking timer procedure */
    if (((TextWidget)w)->text.cursorBlinkProcID != 0)
    	XtRemoveTimeOut(((TextWidget)w)->text.cursorBlinkProcID);
    ((TextWidget)w)->text.cursorBlinkProcID = 0;

    /* Leave a dim or destination cursor */
    TextDSetCursorStyle(textD, ((TextWidget)w)->text.motifDestOwner ?
    	    CARET_CURSOR : DIM_CURSOR);
    TextDUnblankCursor(textD);
    
    /* If there's a calltip displayed, kill it. */
    TextDKillCalltip(textD, 0);

    /* Call any registered focus-out callbacks */
    XtCallCallbacks((Widget)w, textNlosingFocusCallback, (XtPointer)event);
}

/*
** For actions involving cursor movement, "extend" keyword means incorporate
** the new cursor position in the selection, and lack of an "extend" keyword
** means cancel the existing selection
*/
static void checkMoveSelectionChange(Widget w, XEvent *event, int startPos,
	String *args, Cardinal *nArgs)
{
    if (hasKey("extend", args, nArgs))
    	keyMoveExtendSelection(w, event, startPos, hasKey("rect", args, nArgs));
    else
    	BufUnselect((((TextWidget)w)->text.textD)->buffer);
}

/*
** If a selection change was requested via a keyboard command for moving
** the insertion cursor (usually with the "extend" keyword), adjust the
** selection to include the new cursor position, or begin a new selection
** between startPos and the new cursor position with anchor at startPos.
*/
static void keyMoveExtendSelection(Widget w, XEvent *event, int origPos,
	int rectangular)
{
    XKeyEvent *e = &event->xkey;
    TextWidget tw = (TextWidget)w;
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    selection *sel = &buf->primary;
    int newPos = TextDGetInsertPosition(textD);
    int startPos, endPos, startCol, endCol, newCol, origCol;
    int anchor, rectAnchor, anchorLineStart;

    /* Moving the cursor does not take the Motif destination, but as soon as
       the user selects something, grab it (I'm not sure if this distinction
       actually makes sense, but it's what Motif was doing, back when their
       secondary selections actually worked correctly) */
    TakeMotifDestination(w, e->time);
    
    if ((sel->selected || sel->zeroWidth) && sel->rectangular && rectangular) { 
        /* rect -> rect */
        newCol = BufCountDispChars(buf, BufStartOfLine(buf, newPos), newPos);
        startCol = min(tw->text.rectAnchor, newCol);
        endCol   = max(tw->text.rectAnchor, newCol);
        startPos = BufStartOfLine(buf, min(tw->text.anchor, newPos));
        endPos = BufEndOfLine(buf, max(tw->text.anchor, newPos));
	BufRectSelect(buf, startPos, endPos, startCol, endCol);
    } else if (sel->selected && rectangular) { /* plain -> rect */
        newCol = BufCountDispChars(buf, BufStartOfLine(buf, newPos), newPos);
        if (abs(newPos - sel->start) < abs(newPos - sel->end))
            anchor = sel->end;
        else
            anchor = sel->start;
        anchorLineStart = BufStartOfLine(buf, anchor);
        rectAnchor = BufCountDispChars(buf, anchorLineStart, anchor);
        tw->text.anchor = anchor;
        tw->text.rectAnchor = rectAnchor;
        BufRectSelect(buf, BufStartOfLine(buf, min(anchor, newPos)),
                BufEndOfLine(buf, max(anchor, newPos)),
                min(rectAnchor, newCol), max(rectAnchor, newCol));
    } else if (sel->selected && sel->rectangular) { /* rect -> plain */
    	startPos = BufCountForwardDispChars(buf,
    		BufStartOfLine(buf, sel->start), sel->rectStart);
    	endPos = BufCountForwardDispChars(buf,
    		BufStartOfLine(buf, sel->end), sel->rectEnd);
    	if (abs(origPos - startPos) < abs(origPos - endPos))
    	    anchor = endPos;
    	else
    	    anchor = startPos;
    	BufSelect(buf, anchor, newPos);
    } else if (sel->selected) { /* plain -> plain */
        if (abs(origPos - sel->start) < abs(origPos - sel->end))
    	    anchor = sel->end;
    	else
    	    anchor = sel->start;
     	BufSelect(buf, anchor, newPos);
    } else if (rectangular) { /* no sel -> rect */
	origCol = BufCountDispChars(buf, BufStartOfLine(buf, origPos), origPos);
	newCol = BufCountDispChars(buf, BufStartOfLine(buf, newPos), newPos);
	startCol = min(newCol, origCol);
	endCol = max(newCol, origCol);
	startPos = BufStartOfLine(buf, min(origPos, newPos));
	endPos = BufEndOfLine(buf, max(origPos, newPos));
        tw->text.anchor = origPos;
        tw->text.rectAnchor = origCol;
	BufRectSelect(buf, startPos, endPos, startCol, endCol);
    } else { /* no sel -> plain */
        tw->text.anchor = origPos;
        tw->text.rectAnchor = BufCountDispChars(buf, 
                BufStartOfLine(buf, origPos), origPos);
        BufSelect(buf, tw->text.anchor, newPos);
    }
}

static void checkAutoShowInsertPos(Widget w)
{
    if (((TextWidget)w)->text.autoShowInsertPos)
    	TextDMakeInsertPosVisible(((TextWidget)w)->text.textD);
}

static int checkReadOnly(Widget w)
{
    if (((TextWidget)w)->text.readOnly) {
    	XBell(XtDisplay(w), 0);
	return True;
    }
    return False;
}

/*
** Insert text "chars" at the cursor position, as if the text had been
** typed.  Same as TextInsertAtCursor, but without the complicated auto-wrap
** scanning and re-formatting.
*/
static void simpleInsertAtCursor(Widget w, char *chars, XEvent *event,
    	int allowPendingDelete)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;
    char *c;

    if (allowPendingDelete && pendingSelection(w)) {
    	BufReplaceSelected(buf, chars);
    	TextDSetInsertPosition(textD, buf->cursorPosHint);
    } else if (((TextWidget)w)->text.overstrike) {
    	for (c=chars; *c!='\0' && *c!='\n'; c++);
    	if (*c == '\n')
    	    TextDInsert(textD, chars);
    	else
    	    TextDOverstrike(textD, chars);
    } else
    	TextDInsert(textD, chars);
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);
}

/*
** If there's a selection, delete it and position the cursor where the
** selection was deleted.  (Called by routines which do deletion to check
** first for and do possible selection delete)
*/
static int deletePendingSelection(Widget w, XEvent *event)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = textD->buffer;

    if (((TextWidget)w)->text.textD->buffer->primary.selected) {
	BufRemoveSelected(buf);
	TextDSetInsertPosition(textD, buf->cursorPosHint);
    	checkAutoShowInsertPos(w);
    	callCursorMovementCBs(w, event);
	return True;
    } else
	return False;
}

/*
** Return true if pending delete is on and there's a selection contiguous
** with the cursor ready to be deleted.  These criteria are used to decide
** if typing a character or inserting something should delete the selection
** first.
*/
static int pendingSelection(Widget w)
{
    selection *sel = &((TextWidget)w)->text.textD->buffer->primary;
    int pos = TextDGetInsertPosition(((TextWidget)w)->text.textD);
    
    return ((TextWidget)w)->text.pendingDelete && sel->selected &&
    	    pos >= sel->start && pos <= sel->end;
}

/*
** Check if tab emulation is on and if there are emulated tabs before the
** cursor, and if so, delete an emulated tab as a unit.  Also finishes up
** by calling checkAutoShowInsertPos and callCursorMovementCBs, so the
** calling action proc can just return (this is necessary to preserve
** emTabsBeforeCursor which is otherwise cleared by callCursorMovementCBs).
*/
static int deleteEmulatedTab(Widget w, XEvent *event)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    textBuffer *buf = ((TextWidget)w)->text.textD->buffer;
    int emTabDist = ((TextWidget)w)->text.emulateTabs;
    int emTabsBeforeCursor = ((TextWidget)w)->text.emTabsBeforeCursor;
    int startIndent, toIndent, insertPos, startPos, lineStart;
    int pos, indent, startPosIndent;
    char c, *spaceString;
    
    if (emTabDist <= 0 || emTabsBeforeCursor <= 0)
    	return False;
    
    /* Find the position of the previous tab stop */
    insertPos = TextDGetInsertPosition(textD);
    lineStart = BufStartOfLine(buf, insertPos);
    startIndent = BufCountDispChars(buf, lineStart, insertPos);
    toIndent = (startIndent-1) - ((startIndent-1) % emTabDist);
    
    /* Find the position at which to begin deleting (stop at non-whitespace
       characters) */
    startPosIndent = indent = 0;
    startPos = lineStart;
    for (pos=lineStart; pos < insertPos; pos++) {
    	c = BufGetCharacter(buf, pos);
    	indent += BufCharWidth(c, indent, buf->tabDist, buf->nullSubsChar);
    	if (indent > toIndent)
    	    break;
    	startPosIndent = indent;
    	startPos = pos + 1;
    }  	
    
    /* Just to make sure, check that we're not deleting any non-white chars */
    for (pos=insertPos-1; pos>=startPos; pos--) {
    	c = BufGetCharacter(buf, pos);
    	if (c != ' ' && c != '\t') {
    	    startPos = pos + 1;
    	    break;
    	}
    }
    
    /* Do the text replacement and reposition the cursor.  If any spaces need
       to be inserted to make up for a deleted tab, do a BufReplace, otherwise,
       do a BufRemove. */
    if (startPosIndent < toIndent) {
    	spaceString = (char*)NEditMalloc(toIndent - startPosIndent + 1);
    	memset(spaceString, ' ', toIndent-startPosIndent);
    	spaceString[toIndent - startPosIndent] = '\0';
    	BufReplace(buf, startPos, insertPos, spaceString);
    	TextDSetInsertPosition(textD, startPos + toIndent - startPosIndent);
    	NEditFree(spaceString);
    } else {
	BufRemove(buf, startPos, insertPos);
	TextDSetInsertPosition(textD, startPos);
    }
    
    /* The normal cursor movement stuff would usually be called by the action
       routine, but this wraps around it to restore emTabsBeforeCursor */
    checkAutoShowInsertPos(w);
    callCursorMovementCBs(w, event);

    /* Decrement and restore the marker for consecutive emulated tabs, which
       would otherwise have been zeroed by callCursorMovementCBs */
    ((TextWidget)w)->text.emTabsBeforeCursor = emTabsBeforeCursor - 1;
    return True;
}

/*
** Select the word or whitespace adjacent to the cursor, and move the cursor
** to its end.  pointerX is used as a tie-breaker, when the cursor is at the
** boundary between a word and some white-space.  If the cursor is on the
** left, the word or space on the left is used.  If it's on the right, that
** is used instead.
*/
static void selectWord(Widget w, int pointerX)
{
    TextWidget tw = (TextWidget)w;
    textBuffer *buf = tw->text.textD->buffer;
    int x, y, insertPos = TextDGetInsertPosition(tw->text.textD);
    
    TextPosToXY(w, insertPos, &x, &y);
    if (pointerX < x && insertPos > 0 && BufGetCharacter(buf, insertPos-1) != '\n')
	insertPos--;
    BufSelect(buf, startOfWord(tw, insertPos), endOfWord(tw, insertPos));
}

static int startOfWord(TextWidget w, int pos)
{
    int startPos;
    textBuffer *buf = w->text.textD->buffer;
    char *delimiters=w->text.delimiters;
    char c = BufGetCharacter(buf, pos);
                
    if (c == ' ' || c== '\t') {
        if (!spanBackward(buf, pos, " \t", False, &startPos))
	    return 0;
    } else if (strchr(delimiters, c)) {
        if (!spanBackward(buf, pos, delimiters, True, &startPos))
	    return 0;
    } else {
        if (!BufSearchBackward(buf, pos, delimiters, &startPos))
	    return 0;
    }
    return min(pos, startPos+1);
                
}

static int endOfWord(TextWidget w, int pos)
{
    int endPos;
    textBuffer *buf = w->text.textD->buffer;
    char *delimiters=w->text.delimiters;
    char c = BufGetCharacter(buf, pos);
    
    if (c == ' ' || c== '\t') { 
        if (!spanForward(buf, pos, " \t", False, &endPos))
	    return buf->length;
    } else if (strchr(delimiters, c)) {
        if (!spanForward(buf, pos, delimiters, True, &endPos))
	    return buf->length;
    } else { 
        if (!BufSearchForward(buf, pos, delimiters, &endPos))
	    return buf->length;
    }
    return endPos;
}

/*
** Search forwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character "startPos", and returning the
** result in "foundPos" returns True if found, False if not. If ignoreSpace
** is set, then Space, Tab, and Newlines are ignored in searchChars.
*/
static int spanForward(textBuffer *buf, int startPos, char *searchChars,
	int ignoreSpace, int *foundPos)
{
    int pos;
    char *c;
    
    pos = startPos;
    while (pos < buf->length) {
	for (c=searchChars; *c!='\0'; c++)
            if(!(ignoreSpace && (*c==' ' || *c=='\t' || *c=='\n')))
                if (BufGetCharacter(buf, pos) == *c)
		    break; 
        if(*c == 0) { 
            *foundPos = pos;
            return True;
        }
        pos++;
    }
    *foundPos = buf->length;
    return False;
}

/* 
** Search backwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character BEFORE "startPos", returning the
** result in "foundPos" returns True if found, False if not. If ignoreSpace is
** set, then Space, Tab, and Newlines are ignored in searchChars. 
*/
static int spanBackward(textBuffer *buf, int startPos, char *searchChars, int
    	ignoreSpace, int *foundPos)
{
    int pos;
    char *c;
    
    if (startPos == 0) {
        *foundPos = 0;
        return False;
    }
    pos = startPos == 0 ? 0 : startPos - 1;
    while (pos >= 0) {
        for (c=searchChars; *c!='\0'; c++)
            if(!(ignoreSpace && (*c==' ' || *c=='\t' || *c=='\n')))
                if (BufGetCharacter(buf, pos) == *c)
		    break;
        if(*c == 0) {
            *foundPos = pos;
            return True;
        }     
        pos--;
    }
    *foundPos = 0;
    return False;
}

/*
** Select the line containing the cursor, including the terminating newline,
** and move the cursor to its end.
*/
static void selectLine(Widget w)
{
    textDisp *textD = ((TextWidget)w)->text.textD;
    int insertPos = TextDGetInsertPosition(textD);
    int endPos, startPos;
   
    endPos = BufEndOfLine(textD->buffer, insertPos);
    startPos = BufStartOfLine(textD->buffer, insertPos);
    BufSelect(textD->buffer, startPos, min(endPos + 1, textD->buffer->length));
    TextDSetInsertPosition(textD, endPos);
}

/*
** Given a new mouse pointer location, pass the position on to the 
** autoscroll timer routine, and make sure the timer is on when it's
** needed and off when it's not.
*/
static void checkAutoScroll(TextWidget w, int x, int y)
{
    int inWindow;
    
    /* Is the pointer in or out of the window? */
    inWindow = x >= w->text.textD->left &&
    	    x < w->core.width - w->text.marginWidth &&
    	    y >= w->text.marginHeight &&
    	    y < w->core.height - w->text.marginHeight;
    
    /* If it's in the window, cancel the timer procedure */
    if (inWindow) {
    	if (w->text.autoScrollProcID != 0)
    	    XtRemoveTimeOut(w->text.autoScrollProcID);;
    	w->text.autoScrollProcID = 0;
    	return;
    }

    /* If the timer is not already started, start it */
    if (w->text.autoScrollProcID == 0) {
    	w->text.autoScrollProcID = XtAppAddTimeOut(
    	    	XtWidgetToApplicationContext((Widget)w),
    		0, autoScrollTimerProc, w);
    }
    
    /* Pass on the newest mouse location to the autoscroll routine */
    w->text.mouseX = x;
    w->text.mouseY = y;
}

/*
** Reset drag state and cancel the auto-scroll timer
*/
static void endDrag(Widget w)
{
    if (((TextWidget)w)->text.autoScrollProcID != 0)
    	XtRemoveTimeOut(((TextWidget)w)->text.autoScrollProcID);
    ((TextWidget)w)->text.autoScrollProcID = 0;
    if (((TextWidget)w)->text.dragState == MOUSE_PAN)
    	XUngrabPointer(XtDisplay(w), CurrentTime);
    ((TextWidget)w)->text.dragState = NOT_CLICKED;
}

/*
** Cancel any drag operation that might be in progress.  Should be included
** in nearly every key event to cleanly end any dragging before edits are made
** which might change the insert position or the content of the buffer during
** a drag operation)
*/
static void cancelDrag(Widget w)
{
    int dragState = ((TextWidget)w)->text.dragState;
    
    if (((TextWidget)w)->text.autoScrollProcID != 0)
    	XtRemoveTimeOut(((TextWidget)w)->text.autoScrollProcID);
    if (dragState == SECONDARY_DRAG || dragState == SECONDARY_RECT_DRAG)
    	BufSecondaryUnselect(((TextWidget)w)->text.textD->buffer);
    if (dragState == PRIMARY_BLOCK_DRAG)
    	CancelBlockDrag((TextWidget)w);
    if (dragState == MOUSE_PAN)
    	XUngrabPointer(XtDisplay(w), CurrentTime);
    if (dragState != NOT_CLICKED)
    	((TextWidget)w)->text.dragState = DRAG_CANCELED;
}

/*
** Do operations triggered by cursor movement: Call cursor movement callback
** procedure(s), and cancel marker indicating that the cursor is after one or
** more just-entered emulated tabs (spaces to be deleted as a unit).
*/
static void callCursorMovementCBs(Widget w, XEvent *event)
{
    ((TextWidget)w)->text.emTabsBeforeCursor = 0;
    XtCallCallbacks((Widget)w, textNcursorMovementCallback, (XtPointer)event);
}

/*
** Adjust the selection as the mouse is dragged to position: (x, y).
*/
static void adjustSelection(TextWidget tw, int x, int y)
{
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    int row, col, startCol, endCol, startPos, endPos;
    int newPos = TextDXYToPosition(textD, x, y);
    
    /* Adjust the selection */
    if (tw->text.dragState == PRIMARY_RECT_DRAG) {
	TextDXYToUnconstrainedPosition(textD, x, y, &row, &col);
    	col = TextDOffsetWrappedColumn(textD, row, col);
	startCol = min(tw->text.rectAnchor, col);
	endCol = max(tw->text.rectAnchor, col);
	startPos = BufStartOfLine(buf, min(tw->text.anchor, newPos));
	endPos = BufEndOfLine(buf, max(tw->text.anchor, newPos));
	BufRectSelect(buf, startPos, endPos, startCol, endCol);
    } else if (tw->text.multiClickState == ONE_CLICK) {
    	startPos = startOfWord(tw, min(tw->text.anchor, newPos));
    	endPos = endOfWord(tw, max(tw->text.anchor, newPos));
    	BufSelect(buf, startPos, endPos);
    	newPos = newPos < tw->text.anchor ? startPos : endPos;
    } else if (tw->text.multiClickState == TWO_CLICKS) {
        startPos = BufStartOfLine(buf, min(tw->text.anchor, newPos));
        endPos = BufEndOfLine(buf, max(tw->text.anchor, newPos));
        BufSelect(buf, startPos, min(endPos+1, buf->length));
        newPos = newPos < tw->text.anchor ? startPos : endPos;
    } else
    	BufSelect(buf, tw->text.anchor, newPos);
    
    /* Move the cursor */
    TextDSetInsertPosition(textD, newPos);
    callCursorMovementCBs((Widget)tw, NULL);
}

static void adjustSecondarySelection(TextWidget tw, int x, int y)
{
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    int row, col, startCol, endCol, startPos, endPos;
    int newPos = TextDXYToPosition(textD, x, y);

    if (tw->text.dragState == SECONDARY_RECT_DRAG) {
	TextDXYToUnconstrainedPosition(textD, x, y, &row, &col);
    	col = TextDOffsetWrappedColumn(textD, row, col);
	startCol = min(tw->text.rectAnchor, col);
	endCol = max(tw->text.rectAnchor, col);
	startPos = BufStartOfLine(buf, min(tw->text.anchor, newPos));
	endPos = BufEndOfLine(buf, max(tw->text.anchor, newPos));
	BufSecRectSelect(buf, startPos, endPos, startCol, endCol);
    } else
    	BufSecondarySelect(textD->buffer, tw->text.anchor, newPos);
}

/*
** Wrap multi-line text in argument "text" to be inserted at the end of the
** text on line "startLine" and return the result.  If "breakBefore" is
** non-NULL, allow wrapping to extend back into "startLine", in which case
** the returned text will include the wrapped part of "startLine", and
** "breakBefore" will return the number of characters at the end of
** "startLine" that were absorbed into the returned string.  "breakBefore"
** will return zero if no characters were absorbed into the returned string.
** The buffer offset of text in the widget's text buffer is needed so that
** smart indent (which can be triggered by wrapping) can search back farther
** in the buffer than just the text in startLine.
*/
static char *wrapText(TextWidget tw, char *startLine, char *text, int bufOffset,
    	int wrapMargin, int *breakBefore)
{
    textBuffer *wrapBuf, *buf = tw->text.textD->buffer;
    int startLineLen = strlen(startLine);
    int colNum, pos, lineStartPos, limitPos, breakAt, charsAdded;
    int firstBreak = -1, tabDist = buf->tabDist;
    char c, *wrappedText;
    
    /* Create a temporary text buffer and load it with the strings */
    wrapBuf = BufCreate();
    BufInsert(wrapBuf, 0, startLine);
    BufInsert(wrapBuf, wrapBuf->length, text);
    
    /* Scan the buffer for long lines and apply wrapLine when wrapMargin is
       exceeded.  limitPos enforces no breaks in the "startLine" part of the
       string (if requested), and prevents re-scanning of long unbreakable
       lines for each character beyond the margin */
    colNum = 0;
    pos = 0;
    lineStartPos = 0;
    limitPos = breakBefore == NULL ? startLineLen : 0;
    while (pos < wrapBuf->length) {
    	c = BufGetCharacter(wrapBuf, pos);
    	if (c == '\n') {
    	    lineStartPos = limitPos = pos + 1;
    	    colNum = 0;
    	} else {
    	    colNum += BufCharWidth(c, colNum, tabDist, buf->nullSubsChar);
    	    if (colNum > wrapMargin) {
    		if (!wrapLine(tw, wrapBuf, bufOffset, lineStartPos, pos,
    		    	limitPos, &breakAt, &charsAdded)) {
    	    	    limitPos = max(pos, limitPos);
    		} else {
    	    	    lineStartPos = limitPos = breakAt+1;
    	    	    pos += charsAdded;
    	    	    colNum = BufCountDispChars(wrapBuf, lineStartPos, pos+1);
    	    	    if (firstBreak == -1)
    	    		firstBreak = breakAt;
    		}
    	    }
    	}
    	pos++;
    }
    
    /* Return the wrapped text, possibly including part of startLine */
    if (breakBefore == NULL)
    	wrappedText = BufGetRange(wrapBuf, startLineLen, wrapBuf->length);
    else {
    	*breakBefore = firstBreak != -1 && firstBreak < startLineLen ?
    	    	startLineLen - firstBreak : 0;
    	wrappedText = BufGetRange(wrapBuf, startLineLen - *breakBefore,
    	    	wrapBuf->length);
    }
    BufFree(wrapBuf);
    return wrappedText;
}

/*
** Wraps the end of a line beginning at lineStartPos and ending at lineEndPos
** in "buf", at the last white-space on the line >= limitPos.  (The implicit
** assumption is that just the last character of the line exceeds the wrap
** margin, and anywhere on the line we can wrap is correct).  Returns False if
** unable to wrap the line.  "breakAt", returns the character position at
** which the line was broken,
**
** Auto-wrapping can also trigger auto-indent.  The additional parameter
** bufOffset is needed when auto-indent is set to smart indent and the smart
** indent routines need to scan far back in the buffer.  "charsAdded" returns
** the number of characters added to acheive the auto-indent.  wrapMargin is
** used to decide whether auto-indent should be skipped because the indent
** string itself would exceed the wrap margin.
*/
static int wrapLine(TextWidget tw, textBuffer *buf, int bufOffset,
    	int lineStartPos, int lineEndPos, int limitPos, int *breakAt,
	int *charsAdded)
{
    int p, length, column;
    char c, *indentStr;
    
    /* Scan backward for whitespace or BOL.  If BOL, return False, no
       whitespace in line at which to wrap */
    for (p=lineEndPos; ; p--) {
	if (p < lineStartPos || p < limitPos)
     	    return False;
     	c = BufGetCharacter(buf, p);
     	if (c == '\t' || c == ' ')
     	    break;
    }
    
    /* Create an auto-indent string to insert to do wrap.  If the auto
       indent string reaches the wrap position, slice the auto-indent
       back off and return to the left margin */
    if (tw->text.autoIndent || tw->text.smartIndent) {
	indentStr = createIndentString(tw, buf, bufOffset, lineStartPos,
	    	lineEndPos, &length, &column);
	if (column >= p-lineStartPos)
	    indentStr[1] = '\0';
    } else {
    	indentStr = "\n";
    	length = 1;
    }
    
    /* Replace the whitespace character with the auto-indent string
       and return the stats */
    BufReplace(buf, p, p+1, indentStr);
    if (tw->text.autoIndent || tw->text.smartIndent)
    	NEditFree(indentStr);
    *breakAt = p;
    *charsAdded = length-1;
    return True;
}

/*
** Create and return an auto-indent string to add a newline at lineEndPos to a
** line starting at lineStartPos in buf.  "buf" may or may not be the real
** text buffer for the widget.  If it is not the widget's text buffer it's
** offset position from the real buffer must be specified in "bufOffset" to
** allow the smart-indent routines to scan back as far as necessary. The
** string length is returned in "length" (or "length" can be passed as NULL,
** and the indent column is returned in "column" (if non NULL).
*/
static char *createIndentString(TextWidget tw, textBuffer *buf, int bufOffset,
    	int lineStartPos, int lineEndPos, int *length, int *column)
{
    textDisp *textD = tw->text.textD;
    int pos, indent = -1, tabDist = textD->buffer->tabDist;
    int i, useTabs = textD->buffer->useTabs;
    char *indentPtr, *indentStr, c;
    smartIndentCBStruct smartIndent;
    
    /* If smart indent is on, call the smart indent callback.  It is not
       called when multi-line changes are being made (lineStartPos != 0),
       because smart indent needs to search back an indeterminate distance
       through the buffer, and reconciling that with wrapping changes made,
       but not yet committed in the buffer, would make programming smart
       indent more difficult for users and make everything more complicated */
    if (tw->text.smartIndent && (lineStartPos == 0 || buf == textD->buffer)) {
    	smartIndent.reason = NEWLINE_INDENT_NEEDED;
	smartIndent.pos = lineEndPos + bufOffset;
    	smartIndent.indentRequest = 0;
	smartIndent.charsTyped = NULL;
    	XtCallCallbacks((Widget)tw, textNsmartIndentCallback,
    	    	(XtPointer)&smartIndent);
    	indent = smartIndent.indentRequest;
    }
    
    /* If smart indent wasn't used, measure the indent distance of the line */
    if (indent == -1) {
	indent = 0;
	for (pos=lineStartPos; pos<lineEndPos; pos++) {
	    c =  BufGetCharacter(buf, pos);
	    if (c != ' ' && c != '\t')
		break;
	    if (c == '\t')
		indent += tabDist - (indent % tabDist);
	    else
		indent++;
	}
    }
    
    /* Allocate and create a string of tabs and spaces to achieve the indent */
    indentPtr = indentStr = (char*)NEditMalloc(indent + 2);
    *indentPtr++ = '\n';
    if (useTabs) {
	for (i=0; i < indent / tabDist; i++)
	    *indentPtr++ = '\t';
	for (i=0; i < indent % tabDist; i++)
	    *indentPtr++ = ' ';
    } else {
	for (i=0; i < indent; i++)
	    *indentPtr++ = ' ';
    }
    *indentPtr = '\0';
    	
    /* Return any requested stats */
    if (length != NULL)
    	*length = indentPtr - indentStr;
    if (column != NULL)
    	*column = indent;
    
    return indentStr;
}

/*
** Xt timer procedure for autoscrolling
*/
static void autoScrollTimerProc(XtPointer clientData, XtIntervalId *id)
{
    TextWidget w = (TextWidget)clientData;
    textDisp *textD = w->text.textD;
    int topLineNum, horizOffset, newPos, cursorX, y;
    int fontWidth = textD->fontStruct->max_bounds.width;
    int fontHeight = textD->fontStruct->ascent + textD->fontStruct->descent;

    /* For vertical autoscrolling just dragging the mouse outside of the top
       or bottom of the window is sufficient, for horizontal (non-rectangular)
       scrolling, see if the position where the CURSOR would go is outside */
    newPos = TextDXYToPosition(textD, w->text.mouseX, w->text.mouseY);
    if (w->text.dragState == PRIMARY_RECT_DRAG)
    	cursorX = w->text.mouseX;
    else if (!TextDPositionToXY(textD, newPos, &cursorX, &y))
    	cursorX = w->text.mouseX;
    
    /* Scroll away from the pointer, 1 character (horizontal), or 1 character
       for each fontHeight distance from the mouse to the text (vertical) */
    TextDGetScroll(textD, &topLineNum, &horizOffset);
    if (cursorX >= (int)w->core.width - w->text.marginWidth)
    	horizOffset += fontWidth;
    else if (w->text.mouseX < textD->left)
    	horizOffset -= fontWidth;
    if (w->text.mouseY >= (int)w->core.height - w->text.marginHeight)
    	topLineNum += 1 + ((w->text.mouseY - (int)w->core.height -
    	    	w->text.marginHeight) / fontHeight) + 1;
    else if (w->text.mouseY < w->text.marginHeight)
    	topLineNum -= 1 + ((w->text.marginHeight-w->text.mouseY) / fontHeight);
    TextDSetScroll(textD, topLineNum, horizOffset);
    
    /* Continue the drag operation in progress.  If none is in progress
       (safety check) don't continue to re-establish the timer proc */
    if (w->text.dragState == PRIMARY_DRAG) {
	adjustSelection(w, w->text.mouseX, w->text.mouseY);
    } else if (w->text.dragState == PRIMARY_RECT_DRAG) {
	adjustSelection(w, w->text.mouseX, w->text.mouseY);
    } else if (w->text.dragState == SECONDARY_DRAG) {
	adjustSecondarySelection(w, w->text.mouseX, w->text.mouseY);
    } else if (w->text.dragState == SECONDARY_RECT_DRAG) {
	adjustSecondarySelection(w, w->text.mouseX, w->text.mouseY);
    } else if (w->text.dragState == PRIMARY_BLOCK_DRAG) {
    	BlockDragSelection(w, w->text.mouseX, w->text.mouseY, USE_LAST);
    } else {
    	w->text.autoScrollProcID = 0;
    	return;
    }
    
    /* re-establish the timer proc (this routine) to continue processing */
    w->text.autoScrollProcID = XtAppAddTimeOut(
    	    XtWidgetToApplicationContext((Widget)w),
    	    w->text.mouseY >= w->text.marginHeight &&
    	    w->text.mouseY < w->core.height - w->text.marginHeight ?
    	    (VERTICAL_SCROLL_DELAY*fontWidth) / fontHeight :
    	    VERTICAL_SCROLL_DELAY, autoScrollTimerProc, w);
}

/*
** Xt timer procedure for cursor blinking
*/
static void cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id)
{
    TextWidget w = (TextWidget)clientData;
    textDisp *textD = w->text.textD;
    
    /* Blink the cursor */
    if (textD->cursorOn)
    	TextDBlankCursor(textD);
    else
    	TextDUnblankCursor(textD);
    	
    /* re-establish the timer proc (this routine) to continue processing */
    w->text.cursorBlinkProcID = XtAppAddTimeOut(
    	    XtWidgetToApplicationContext((Widget)w),
    	    w->text.cursorBlinkRate, cursorBlinkTimerProc, w);
}

/*
**  Sets the caret to on or off and restart the caret blink timer.
**  This could be used by other modules to modify the caret's blinking.
*/
void ResetCursorBlink(TextWidget textWidget, Boolean startsBlanked)
{
    if (0 != textWidget->text.cursorBlinkRate)
    {
        if (0 != textWidget->text.cursorBlinkProcID)
        {
            XtRemoveTimeOut(textWidget->text.cursorBlinkProcID);
        }

        if (startsBlanked)
        {
            TextDBlankCursor(textWidget->text.textD);
        } else
        {
            TextDUnblankCursor(textWidget->text.textD);
        }

        textWidget->text.cursorBlinkProcID
                = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget) textWidget),
                    textWidget->text.cursorBlinkRate, cursorBlinkTimerProc,
                    textWidget);
    }
}

/*
** look at an action procedure's arguments to see if argument "key" has been
** specified in the argument list
*/
static int hasKey(const char *key, const String *args, const Cardinal *nArgs)
{
    int i;
    
    for (i=0; i<(int)*nArgs; i++)
    	if (!strCaseCmp(args[i], key))
    	    return True;
    return False;
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
** strCaseCmp compares its arguments and returns 0 if the two strings
** are equal IGNORING case differences.  Otherwise returns 1.
*/

static int strCaseCmp(const char *str1, const char *str2)
{
    unsigned const char *c1 = (unsigned const char*) str1;
    unsigned const char *c2 = (unsigned const char*) str2;
    
    for (; *c1!='\0' && *c2!='\0'; c1++, c2++)
    	if (toupper(*c1) != toupper(*c2))
    	    return 1;
    if (*c1 == *c2) {
        return(0);
    }
    else {
        return(1);
    }
}

static void ringIfNecessary(Boolean silent, Widget w)
{
    if (!silent)
        XBell(XtDisplay(w), 0);
}
