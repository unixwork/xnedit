/*******************************************************************************
*									       *
* textDrag.c - Text Dragging routines for NEdit text widget		       *
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
* Dec. 15, 1995								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "textDrag.h"
#include "textBuf.h"
#include "textDisp.h"
#include "textP.h"
#include "../util/nedit_malloc.h"

#include <limits.h>

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>
#if XmVersion >= 1002
#include <Xm/PrimitiveP.h>
#endif

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

static void trackModifyRange(int *rangeStart, int *modRangeEnd,
    	int *unmodRangeEnd, int modPos, int nInserted, int nDeleted);
static void findTextMargins(textBuffer *buf, int start, int end, int *leftMargin,
    	int *rightMargin);
static int findRelativeLineStart(textBuffer *buf, int referencePos,
    	int referenceLineNum, int newLineNum);
static int min3(int i1, int i2, int i3);
static int max3(int i1, int i2, int i3);
static int max(int i1, int i2);

/*
** Start the process of dragging the current primary-selected text across
** the window (move by dragging, as opposed to dragging to create the
** selection)
*/
void BeginBlockDrag(TextWidget tw)
{
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    int fontHeight = textD->fontStruct->ascent + textD->fontStruct->descent;
    int fontWidth = textD->fontStruct->max_bounds.width;
    selection *sel = &buf->primary;
    int nLines, mousePos, lineStart;
    int x, y, lineEnd;
    
    char *text;
    
    /* Save a copy of the whole text buffer as a backup, and for
       deriving changes */
    tw->text.dragOrigBuf = BufCreate();
    BufSetTabDistance(tw->text.dragOrigBuf, buf->tabDist);
    tw->text.dragOrigBuf->useTabs = buf->useTabs;
    text = BufGetAll(buf);
    BufSetAll(tw->text.dragOrigBuf, text);
    NEditFree(text);
    if (sel->rectangular)
    	BufRectSelect(tw->text.dragOrigBuf, sel->start, sel->end, sel->rectStart,
    	    	sel->rectEnd);
    else
    	BufSelect(tw->text.dragOrigBuf, sel->start, sel->end);
 
    /* Record the mouse pointer offsets from the top left corner of the
       selection (the position where text will actually be inserted In dragging
       non-rectangular selections)  */
    if (sel->rectangular) {
    	tw->text.dragXOffset = tw->text.btnDownX + textD->horizOffset -
    	    	textD->left - sel->rectStart * fontWidth;
    } else {
        if (!TextDPositionToXY(textD, sel->start, &x, &y))
            x = BufCountDispChars(buf, TextDStartOfLine(textD, sel->start),
            	    sel->start) * fontWidth + textD->left -
            	    textD->horizOffset;
        tw->text.dragXOffset = tw->text.btnDownX - x;
    }
    mousePos = TextDXYToPosition(textD, tw->text.btnDownX, tw->text.btnDownY);
    nLines = BufCountLines(buf, sel->start, mousePos);
    tw->text.dragYOffset = nLines * fontHeight + (((tw->text.btnDownY -
    	    tw->text.marginHeight) % fontHeight) - fontHeight/2);
    tw->text.dragNLines = BufCountLines(buf, sel->start, sel->end);
    
    /* Record the current drag insert position and the information for
       undoing the fictional insert of the selection in its new position */
    tw->text.dragInsertPos = sel->start;
    tw->text.dragInserted = sel->end - sel->start;
    if (sel->rectangular) {
    	textBuffer *testBuf = BufCreate();
    	char *testText = BufGetRange(buf, sel->start, sel->end);
        BufSetTabDistance(testBuf, buf->tabDist);
        testBuf->useTabs = buf->useTabs;
    	BufSetAll(testBuf, testText);
    	NEditFree(testText);
    	BufRemoveRect(testBuf, 0, sel->end - sel->start, sel->rectStart,
    	    	sel->rectEnd);
    	tw->text.dragDeleted = testBuf->length;
    	BufFree(testBuf);
    	tw->text.dragRectStart = sel->rectStart;
    } else {
    	tw->text.dragDeleted = 0;
    	tw->text.dragRectStart = 0;
    }
    tw->text.dragType = DRAG_MOVE;
    tw->text.dragSourceDeletePos = sel->start;
    tw->text.dragSourceInserted = tw->text.dragDeleted;
    tw->text.dragSourceDeleted = tw->text.dragInserted;
    
    /* For non-rectangular selections, fill in the rectangular information in
       the selection for overlay mode drags which are done rectangularly */
    if (!sel->rectangular) {
    	lineStart = BufStartOfLine(buf, sel->start);
    	if (tw->text.dragNLines == 0) {
    	    tw->text.dragOrigBuf->primary.rectStart =
    	    	    BufCountDispChars(buf, lineStart, sel->start);
    	    tw->text.dragOrigBuf->primary.rectEnd =
    	    	    BufCountDispChars(buf, lineStart, sel->end);
    	} else {
    	    lineEnd = BufGetCharacter(buf, sel->end - 1) == '\n' ? 
    	    	    sel->end - 1 : sel->end;
    	    findTextMargins(buf, lineStart, lineEnd,
    	    	    &tw->text.dragOrigBuf->primary.rectStart,
    	    	    &tw->text.dragOrigBuf->primary.rectEnd);
    	}
    }
    
    /* Set the drag state to announce an ongoing block-drag */
    tw->text.dragState = PRIMARY_BLOCK_DRAG;
    
    /* Call the callback announcing the start of a block drag */
    XtCallCallbacks((Widget)tw, textNdragStartCallback, (XtPointer)NULL);
}

/*
** Reposition the primary-selected text that is being dragged as a block
** for a new mouse position of (x, y)
*/
void BlockDragSelection(TextWidget tw, int x, int y, int dragType)
{
    textDisp *textD = tw->text.textD;
    textBuffer *buf = textD->buffer;
    int fontHeight = textD->fontStruct->ascent + textD->fontStruct->descent;
    int fontWidth = textD->fontStruct->max_bounds.width;
    textBuffer *origBuf = tw->text.dragOrigBuf;
    int dragXOffset = tw->text.dragXOffset;
    textBuffer *tempBuf;
    selection *origSel = &origBuf->primary;
    int rectangular = origSel->rectangular;
    int overlay, oldDragType = tw->text.dragType;
    int nLines = tw->text.dragNLines;
    int insLineNum, insLineStart, insRectStart, insRectEnd, insStart;
    char *repText, *text, *insText;
    int modRangeStart = -1, tempModRangeEnd = -1, bufModRangeEnd = -1;
    int referenceLine, referencePos, tempStart, tempEnd, origSelLen;
    int insertInserted, insertDeleted, row, column;
    int origSelLineStart, origSelLineEnd;
    int sourceInserted, sourceDeleted, sourceDeletePos;
    
    if (tw->text.dragState != PRIMARY_BLOCK_DRAG)
    	return;

    /* The operation of block dragging is simple in theory, but not so simple
       in practice.  There is a backup buffer (tw->text.dragOrigBuf) which
       holds a copy of the buffer as it existed before the drag.  When the
       user drags the mouse to a new location, this routine is called, and
       a temporary buffer is created and loaded with the local part of the
       buffer (from the backup) which might be changed by the drag.  The
       changes are all made to this temporary buffer, and the parts of this
       buffer which then differ from the real (displayed) buffer are used to
       replace those parts, thus one replace operation serves as both undo
       and modify.  This double-buffering of the operation prevents excessive
       redrawing (though there is still plenty of needless redrawing due to
       re-selection and rectangular operations).
       
       The hard part is keeping track of the changes such that a single replace
       operation will do everyting.  This is done using a routine called
       trackModifyRange which tracks expanding ranges of changes in the two
       buffers in modRangeStart, tempModRangeEnd, and bufModRangeEnd. */

    /* Create a temporary buffer for accumulating changes which will
       eventually be replaced in the real buffer.  Load the buffer with the
       range of characters which might be modified in this drag step
       (this could be tighter, but hopefully it's not too slow) */
    tempBuf = BufCreate();
    tempBuf->tabDist = buf->tabDist;
    tempBuf->useTabs = buf->useTabs;
    tempStart = min3(tw->text.dragInsertPos, origSel->start,
    	 BufCountBackwardNLines(buf, textD->firstChar, nLines+2));
    tempEnd = BufCountForwardNLines(buf, max3(tw->text.dragInsertPos,
    	 origSel->start, textD->lastChar), nLines+2) +
    	 origSel->end - origSel->start;
    text = BufGetRange(origBuf, tempStart, tempEnd);
    BufSetAll(tempBuf, text);
    NEditFree(text);

    /* If the drag type is USE_LAST, use the last dragType applied */
    if (dragType == USE_LAST)
    	dragType = tw->text.dragType;
    overlay = dragType == DRAG_OVERLAY_MOVE || dragType == DRAG_OVERLAY_COPY;

    /* Overlay mode uses rectangular selections whether or not the original
       was rectangular.  To use a plain selection as if it were rectangular,
       the start and end positions need to be moved to the line boundaries
       and trailing newlines must be excluded */
    origSelLineStart = BufStartOfLine(origBuf, origSel->start);
    if (!rectangular && BufGetCharacter(origBuf, origSel->end - 1) == '\n')
    	origSelLineEnd = origSel->end - 1;
    else
    	origSelLineEnd = BufEndOfLine(origBuf, origSel->end);
    if (!rectangular && overlay && nLines != 0)
    	dragXOffset -= fontWidth * (origSel->rectStart -
    	    	(origSel->start - origSelLineStart));
    
    /* If the drag operation is of a different type than the last one, and the
       operation is a move, expand the modified-range to include undoing the
       text-removal at the site from which the text was dragged. */
    if (dragType != oldDragType && tw->text.dragSourceDeleted != 0)
    	trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd,
    	    	tw->text.dragSourceDeletePos, tw->text.dragSourceInserted,
    	    	tw->text.dragSourceDeleted);
    
    /* Do, or re-do the original text removal at the site where a move began.
       If this part has not changed from the last call, do it silently to
       bring the temporary buffer in sync with the real (displayed) 
       buffer.  If it's being re-done, track the changes to complete the
       redo operation begun above */
    if (dragType == DRAG_MOVE || dragType == DRAG_OVERLAY_MOVE) {
	if (rectangular || overlay) {
    	    int prevLen = tempBuf->length;
    	    origSelLen = origSelLineEnd - origSelLineStart;
    	    if (overlay)
    		BufClearRect(tempBuf, origSelLineStart-tempStart,
    		    	origSelLineEnd-tempStart, origSel->rectStart,
    		    	origSel->rectEnd);
    	    else
    		BufRemoveRect(tempBuf, origSelLineStart-tempStart,
    		    	origSelLineEnd-tempStart, origSel->rectStart,
    		    	origSel->rectEnd);
    	    sourceDeletePos = origSelLineStart;
    	    sourceInserted = origSelLen - prevLen + tempBuf->length;
    	    sourceDeleted = origSelLen;
	} else {
    	    BufRemove(tempBuf, origSel->start - tempStart,
    	    	    origSel->end - tempStart);
    	    sourceDeletePos = origSel->start;
    	    sourceInserted = 0;
    	    sourceDeleted = origSel->end - origSel->start;
	}
	if (dragType != oldDragType)
    	    trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd,
    	    	    sourceDeletePos, sourceInserted, sourceDeleted);
    } else {
    	sourceDeletePos = 0;
    	sourceInserted = 0;
    	sourceDeleted = 0;
    }
    
    /* Expand the modified-range to include undoing the insert from the last
       call. */
    trackModifyRange(&modRangeStart, &bufModRangeEnd, &tempModRangeEnd,
    	   tw->text.dragInsertPos, tw->text.dragInserted, tw->text.dragDeleted);
    
    /* Find the line number and column of the insert position.  Note that in
       continuous wrap mode, these must be calculated as if the text were
       not wrapped */
    TextDXYToUnconstrainedPosition(textD, max(0, x - dragXOffset),
    	    max(0, y - (tw->text.dragYOffset % fontHeight)), &row, &column);
    column = TextDOffsetWrappedColumn(textD, row, column);
    row = TextDOffsetWrappedRow(textD, row);
    insLineNum = row + textD->topLineNum - tw->text.dragYOffset / fontHeight;
    
    /* find a common point of reference between the two buffers, from which
       the insert position line number can be translated to a position */
    if (textD->firstChar > modRangeStart) {
    	referenceLine = textD->topLineNum -
    	    	BufCountLines(buf, modRangeStart, textD->firstChar);
    	referencePos = modRangeStart;
    } else {
    	referencePos = textD->firstChar;
    	referenceLine = textD->topLineNum;
    }

    /* find the position associated with the start of the new line in the
       temporary buffer */
    insLineStart = findRelativeLineStart(tempBuf, referencePos - tempStart,
    	    referenceLine, insLineNum) + tempStart;
    if (insLineStart - tempStart == tempBuf->length)
    	insLineStart = BufStartOfLine(tempBuf, insLineStart - tempStart) +
    	    	tempStart;
    
    /* Find the actual insert position */
    if (rectangular || overlay) {
    	insStart = insLineStart;
    	insRectStart = column;
    } else { /* note, this will fail with proportional fonts */
    	insStart = BufCountForwardDispChars(tempBuf, insLineStart - tempStart,
    	    	column) + tempStart;
    	insRectStart = 0;
    }
    
    /* If the position is the same as last time, don't bother drawing (it
       would be nice if this decision could be made earlier) */
    if (insStart == tw->text.dragInsertPos &&
    	    insRectStart == tw->text.dragRectStart && dragType == oldDragType) {
    	BufFree(tempBuf);
    	return;
    }

    /* Do the insert in the temporary buffer */
    if (rectangular || overlay) {
    	insText = BufGetTextInRect(origBuf, origSelLineStart, origSelLineEnd,
    	    	origSel->rectStart, origSel->rectEnd);
    	if (overlay)
    	    BufOverlayRect(tempBuf, insStart - tempStart, insRectStart,
    	    	    insRectStart + origSel->rectEnd - origSel->rectStart,
    	    	    insText, &insertInserted, &insertDeleted);
    	else
    	    BufInsertCol(tempBuf, insRectStart, insStart - tempStart, insText,
    	    	    &insertInserted, &insertDeleted);
    	trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd,
    	    	insStart, insertInserted, insertDeleted);
    	NEditFree(insText);
    } else {
    	insText = BufGetSelectionText(origBuf);
    	BufInsert(tempBuf, insStart - tempStart, insText);
    	trackModifyRange(&modRangeStart, &tempModRangeEnd, &bufModRangeEnd,
    	    	    insStart, origSel->end - origSel->start, 0);
    	insertInserted = origSel->end - origSel->start;
    	insertDeleted = 0;
    	NEditFree(insText);
    }
 
    /* Make the changes in the real buffer */
    repText = BufGetRange(tempBuf, modRangeStart - tempStart,
    	    tempModRangeEnd - tempStart);
    BufFree(tempBuf);
    TextDBlankCursor(textD);
    BufReplace(buf, modRangeStart, bufModRangeEnd, repText);
    NEditFree(repText);
    
    /* Store the necessary information for undoing this step */
    tw->text.dragInsertPos = insStart;
    tw->text.dragRectStart = insRectStart;
    tw->text.dragInserted = insertInserted;
    tw->text.dragDeleted = insertDeleted;
    tw->text.dragSourceDeletePos = sourceDeletePos;
    tw->text.dragSourceInserted = sourceInserted;
    tw->text.dragSourceDeleted = sourceDeleted;
    tw->text.dragType = dragType;
 
    /* Reset the selection and cursor position */
    if (rectangular || overlay) {
    	insRectEnd = insRectStart + origSel->rectEnd - origSel->rectStart;
    	BufRectSelect(buf, insStart, insStart + insertInserted, insRectStart,
    	    	insRectEnd);
    	TextDSetInsertPosition(textD, BufCountForwardDispChars(buf,
    	    	BufCountForwardNLines(buf, insStart, tw->text.dragNLines),
    	    	insRectEnd));
    } else {
    	BufSelect(buf, insStart, insStart + origSel->end - origSel->start);
    	TextDSetInsertPosition(textD, insStart + origSel->end - origSel->start);
    }
    TextDUnblankCursor(textD);
    XtCallCallbacks((Widget)tw, textNcursorMovementCallback, (XtPointer)NULL);
    tw->text.emTabsBeforeCursor = 0;
}

/*
** Complete a block text drag operation
*/
void FinishBlockDrag(TextWidget tw)
{
    dragEndCBStruct endStruct;
    int modRangeStart = -1, origModRangeEnd, bufModRangeEnd;
    char *deletedText;
    
    /* Find the changed region of the buffer, covering both the deletion
       of the selected text at the drag start position, and insertion at
       the drag destination */
    trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd,
    	    	tw->text.dragSourceDeletePos, tw->text.dragSourceInserted,
    	    	tw->text.dragSourceDeleted);
    trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd,
    	    	tw->text.dragInsertPos, tw->text.dragInserted,
    	    	tw->text.dragDeleted);

    /* Get the original (pre-modified) range of text from saved backup buffer */
    deletedText = BufGetRange(tw->text.dragOrigBuf, modRangeStart,
    	    origModRangeEnd);

    /* Free the backup buffer */
    BufFree(tw->text.dragOrigBuf);
    
    /* Return to normal drag state */
    tw->text.dragState = NOT_CLICKED;
    
    /* Call finish-drag calback */
    endStruct.startPos = modRangeStart;
    endStruct.nCharsDeleted = origModRangeEnd - modRangeStart;
    endStruct.nCharsInserted = bufModRangeEnd - modRangeStart;
    endStruct.deletedText = deletedText;
    XtCallCallbacks((Widget)tw, textNdragEndCallback, (XtPointer)&endStruct);
    NEditFree(deletedText);
}

/*
** Cancel a block drag operation
*/
void CancelBlockDrag(TextWidget tw)
{
    textBuffer *buf = tw->text.textD->buffer;
    textBuffer *origBuf = tw->text.dragOrigBuf;
    selection *origSel = &origBuf->primary;
    int modRangeStart = -1, origModRangeEnd, bufModRangeEnd;
    char *repText;
    dragEndCBStruct endStruct;

    /* If the operation was a move, make the modify range reflect the
       removal of the text from the starting position */
    if (tw->text.dragSourceDeleted != 0)
    	trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd,
    	    	tw->text.dragSourceDeletePos, tw->text.dragSourceInserted,
    	    	tw->text.dragSourceDeleted);
    
    /* Include the insert being undone from the last step in the modified
       range. */
    trackModifyRange(&modRangeStart, &bufModRangeEnd, &origModRangeEnd,
    	   tw->text.dragInsertPos, tw->text.dragInserted, tw->text.dragDeleted);
 
    /* Make the changes in the buffer */
    repText = BufGetRange(origBuf, modRangeStart, origModRangeEnd);
    BufReplace(buf, modRangeStart, bufModRangeEnd, repText);
    NEditFree(repText);
    
    /* Reset the selection and cursor position */
    if (origSel->rectangular)
    	BufRectSelect(buf, origSel->start, origSel->end, origSel->rectStart,
    	    	origSel->rectEnd);
    else
    	BufSelect(buf, origSel->start, origSel->end);
    TextDSetInsertPosition(tw->text.textD, buf->cursorPosHint);
    XtCallCallbacks((Widget)tw, textNcursorMovementCallback, NULL);
    tw->text.emTabsBeforeCursor = 0;
    
    /* Free the backup buffer */
    BufFree(origBuf);
    
    /* Indicate end of drag */
    tw->text.dragState = DRAG_CANCELED;
    
    /* Call finish-drag calback */
    endStruct.startPos = 0;
    endStruct.nCharsDeleted = 0;
    endStruct.nCharsInserted = 0;
    endStruct.deletedText = NULL;
    XtCallCallbacks((Widget)tw, textNdragEndCallback, (XtPointer)&endStruct);
}

/*
** Maintain boundaries of changed region between two buffers which
** start out with identical contents, but diverge through insertion,
** deletion, and replacement, such that the buffers can be reconciled
** by replacing the changed region of either buffer with the changed
** region of the other.
**
** rangeStart is the beginning of the modification region in the shared
** coordinates of both buffers (which are identical up to rangeStart).
** modRangeEnd is the end of the changed region for the buffer being
** modified, unmodRangeEnd is the end of the region for the buffer NOT
** being modified.  A value of -1 in rangeStart indicates that there
** have been no modifications so far.
*/
static void trackModifyRange(int *rangeStart, int *modRangeEnd,
    	int *unmodRangeEnd, int modPos, int nInserted, int nDeleted)
{
    if (*rangeStart == -1) {
    	*rangeStart = modPos;
    	*modRangeEnd = modPos + nInserted;
    	*unmodRangeEnd = modPos + nDeleted;
    } else {
    	if (modPos < *rangeStart)
    	    *rangeStart = modPos;
    	if (modPos + nDeleted > *modRangeEnd) {
    	    *unmodRangeEnd += modPos + nDeleted - *modRangeEnd;
    	    *modRangeEnd = modPos + nInserted;
    	} else
    	    *modRangeEnd += nInserted - nDeleted;
    }
}

/*
** Find the left and right margins of text between "start" and "end" in
** buffer "buf".  Note that "start is assumed to be at the start of a line. 
*/
static void findTextMargins(textBuffer *buf, int start, int end, int *leftMargin,
    	int *rightMargin)
{
    char c;
    int pos, width = 0, maxWidth = 0, minWhite = INT_MAX, inWhite = True;
    
    for (pos=start; pos<end; pos++) {
    	c = BufGetCharacter(buf, pos);
    	if (inWhite && c != ' ' && c != '\t') {
    	    inWhite = False;
    	    if (width < minWhite)
    	    	minWhite = width;
    	}
    	if (c == '\n') {
    	    if (width > maxWidth)
    	    	maxWidth = width;
    	    width = 0;
    	    inWhite = True;
    	} else
    	    width += BufCharWidth(c, width, buf->tabDist, buf->nullSubsChar);
    }
    if (width > maxWidth)
    	maxWidth = width;
    *leftMargin = minWhite == INT_MAX ? 0 : minWhite;
    *rightMargin = maxWidth;
}

/*
** Find a text position in buffer "buf" by counting forward or backward
** from a reference position with known line number
*/
static int findRelativeLineStart(textBuffer *buf, int referencePos,
    	int referenceLineNum, int newLineNum)
{
    if (newLineNum < referenceLineNum)
    	return BufCountBackwardNLines(buf, referencePos,
    	    	referenceLineNum - newLineNum);
    else if (newLineNum > referenceLineNum)
    	return BufCountForwardNLines(buf, referencePos,
    	    	newLineNum - referenceLineNum);
    return BufStartOfLine(buf, referencePos);
}

static int min3(int i1, int i2, int i3)
{
    if (i1 <= i2 && i1 <= i3)
    	return i1;
    return i2 <= i3 ? i2 : i3;
}

static int max3(int i1, int i2, int i3)
{
    if (i1 >= i2 && i1 >= i3)
    	return i1;
    return i2 >= i3 ? i2 : i3;
}

static int max(int i1, int i2)
{
    return i1 >= i2 ? i1 : i2;
}
