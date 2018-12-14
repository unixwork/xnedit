/*******************************************************************************
*                                                                              *
* textBuf.c - Manage source text for one or more text areas                    *
*                                                                              *
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
* June 15, 1995                                                                *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "textBuf.h"
#include "rangeset.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define PREFERRED_GAP_SIZE 80	/* Initial size for the buffer gap (empty space
                                   in the buffer where text might be inserted
                                   if the user is typing sequential chars) */

static void histogramCharacters(const char *string, int length, char hist[256],
	int init);
static void subsChars(char *string, int length, char fromChar, char toChar);
static char chooseNullSubsChar(char hist[256]);
static int insert(textBuffer *buf, int pos, const char *text);
static void delete(textBuffer *buf, int start, int end);
static void deleteRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd, int *replaceLen, int *endPos);
static void insertCol(textBuffer *buf, int column, int startPos, const char *insText,
	int *nDeleted, int *nInserted, int *endPos);
static void overlayRect(textBuffer *buf, int startPos, int rectStart,
    	int rectEnd, const char *insText, int *nDeleted, int *nInserted, int *endPos);
static void insertColInLine(const char *line, const char *insLine, int column, int insWidth,
	int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen,
	int *endOffset);
static void deleteRectFromLine(const char *line, int rectStart, int rectEnd,
	int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen,
	int *endOffset);
static void overlayRectInLine(const char *line, const char *insLine, int rectStart,
    	int rectEnd, int tabDist, int useTabs, char nullSubsChar, char *outStr,
    	int *outLen, int *endOffset);
static void callPreDeleteCBs(textBuffer *buf, int pos, int nDeleted);
static void callModifyCBs(textBuffer *buf, int pos, int nDeleted,
	int nInserted, int nRestyled, const char *deletedText);
static void redisplaySelection(textBuffer *buf, selection *oldSelection,
	selection *newSelection);
static void moveGap(textBuffer *buf, int pos);
static void reallocateBuf(textBuffer *buf, int newGapStart, int newGapLen);
static void setSelection(selection *sel, int start, int end);
static void setRectSelect(selection *sel, int start, int end,
	int rectStart, int rectEnd);
static void updateSelections(textBuffer *buf, int pos, int nDeleted,
	int nInserted);
static void updateSelection(selection *sel, int pos, int nDeleted,
	int nInserted);
static int getSelectionPos(selection *sel, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd);
static char *getSelectionText(textBuffer *buf, selection *sel);
static void removeSelected(textBuffer *buf, selection *sel);
static void replaceSelected(textBuffer *buf, selection *sel, const char *text);
static void addPadding(char *string, int startIndent, int toIndent,
	int tabDist, int useTabs, char nullSubsChar, int *charsAdded);
static int searchForward(textBuffer *buf, int startPos, char searchChar,
	int *foundPos);
static int searchBackward(textBuffer *buf, int startPos, char searchChar,
	int *foundPos);
static char *copyLine(const char *text, int *lineLen);
static int countLines(const char *string);
static int textWidth(const char *text, int tabDist, char nullSubsChar);
static void findRectSelBoundariesForCopy(textBuffer *buf, int lineStartPos,
	int rectStart, int rectEnd, int *selStart, int *selEnd);
static char *realignTabs(const char *text, int origIndent, int newIndent,
	int tabDist, int useTabs, char nullSubsChar, int *newLength);
static char *expandTabs(const char *text, int startIndent, int tabDist,
	char nullSubsChar, int *newLen);
static char *unexpandTabs(const char *text, int startIndent, int tabDist,
	char nullSubsChar, int *newLen);
static int max(int i1, int i2);
static int min(int i1, int i2);

#ifdef __MVS__
static const char *ControlCodeTable[64] = {
     "nul", "soh", "stx", "etx", "sel", "ht", "rnl", "del",
     "ge", "sps", "rpt", "vt", "ff", "cr", "so", "si",
     "dle", "dc1", "dc2", "dc3", "res", "nl", "bs", "poc",
     "can", "em", "ubs", "cu1", "ifs", "igs", "irs", "ius",
     "ds", "sos", "fs", "wus", "byp", "lf", "etb", "esc",
     "sa", "sfe", "sm", "csp", "mfa", "enq", "ack", "bel",
     "x30", "x31", "syn", "ir", "pp", "trn", "nbs", "eot",
     "sbs", "it", "rff", "cu3", "dc4", "nak", "x3e", "sub"};
#else
static const char *ControlCodeTable[32] = {
     "nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
     "bs", "ht", "nl", "vt", "np", "cr", "so", "si",
     "dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
     "can", "em", "sub", "esc", "fs", "gs", "rs", "us"};
#endif

/*
** Create an empty text buffer
*/
textBuffer *BufCreate(void)
{
    textBuffer *buf = BufCreatePreallocated(0);
    return buf;
}

/*
** Create an empty text buffer of a pre-determined size (use this to
** avoid unnecessary re-allocation if you know exactly how much the buffer
** will need to hold
*/
textBuffer *BufCreatePreallocated(int requestedSize)
{
    textBuffer *buf;
    
    buf = (textBuffer *)NEditMalloc(sizeof(textBuffer));
    buf->length = 0;
    buf->buf = (char*) NEditMalloc(requestedSize + PREFERRED_GAP_SIZE + 1);
    buf->buf[requestedSize + PREFERRED_GAP_SIZE] = '\0';
    buf->gapStart = 0;
    buf->gapEnd = PREFERRED_GAP_SIZE;
    buf->tabDist = 8;
    buf->useTabs = True;
    buf->primary.selected = False;
    buf->primary.zeroWidth = False;
    buf->primary.rectangular = False;
    buf->primary.start = buf->primary.end = 0;
    buf->secondary.selected = False;
    buf->secondary.zeroWidth = False;
    buf->secondary.start = buf->secondary.end = 0;
    buf->secondary.rectangular = False;
    buf->highlight.selected = False;
    buf->highlight.zeroWidth = False;
    buf->highlight.start = buf->highlight.end = 0;
    buf->highlight.rectangular = False;
    buf->modifyProcs = NULL;
    buf->cbArgs = NULL;
    buf->nModifyProcs = 0;
    buf->preDeleteProcs = NULL;
    buf->preDeleteCbArgs = NULL;
    buf->nPreDeleteProcs = 0;
    buf->nullSubsChar = '\0';
#ifdef PURIFY
    {int i; for (i=buf->gapStart; i<buf->gapEnd; i++) buf->buf[i] = '.';}
#endif
    buf->rangesetTable = NULL;
    return buf;
}

/*
** Free a text buffer
*/
void BufFree(textBuffer *buf)
{
    NEditFree(buf->buf);
    if (buf->nModifyProcs != 0) {
    	NEditFree(buf->modifyProcs);
    	NEditFree(buf->cbArgs);
    }
    if (buf->rangesetTable)
	RangesetTableFree(buf->rangesetTable);
    if (buf->nPreDeleteProcs != 0) {
    	NEditFree(buf->preDeleteProcs);
    	NEditFree(buf->preDeleteCbArgs);
    }
    NEditFree(buf);
}

/*
** Get the entire contents of a text buffer.  Memory is allocated to contain
** the returned string, which the caller must free with NEditFree.
*/
char *BufGetAll(textBuffer *buf)
{
    char *text;
    
    text = (char*)NEditMalloc(buf->length+1);
    memcpy(text, buf->buf, buf->gapStart);
    memcpy(&text[buf->gapStart], &buf->buf[buf->gapEnd],
            buf->length - buf->gapStart);
    text[buf->length] = '\0';
    return text;
}

/*
** Get the entire contents of a text buffer as a single string.  The gap is
** moved so that the buffer data can be accessed as a single contiguous
** character array.
** NB DO NOT ALTER THE TEXT THROUGH THE RETURNED POINTER!
** (we make an exception in BufSubstituteNullChars() however)
** This function is intended ONLY to provide a searchable string without copying
** into a temporary buffer.
*/
const char *BufAsString(textBuffer *buf)
{
    char *text;
    int bufLen = buf->length;
    int leftLen = buf->gapStart;
    int rightLen = bufLen - leftLen;

    /* find where best to put the gap to minimise memory movement */
    if (leftLen != 0 && rightLen != 0) {
        leftLen = (leftLen < rightLen) ? 0 : bufLen;
        moveGap(buf, leftLen);
    }
    /* get the start position of the actual data */
    text = &buf->buf[(leftLen == 0) ? buf->gapEnd : 0];
    /* make sure it's null-terminated */
    text[bufLen] = 0;

    return text;
}

/*
** Replace the entire contents of the text buffer
*/
void BufSetAll(textBuffer *buf, const char *text)
{
    int length, deletedLength;
    char *deletedText;
    length = strlen(text);

    callPreDeleteCBs(buf, 0, buf->length);
    
    /* Save information for redisplay, and get rid of the old buffer */
    deletedText = BufGetAll(buf);
    deletedLength = buf->length;
    NEditFree(buf->buf);
    
    /* Start a new buffer with a gap of PREFERRED_GAP_SIZE in the center */
    buf->buf = (char*)NEditMalloc(length + PREFERRED_GAP_SIZE + 1);
    buf->buf[length + PREFERRED_GAP_SIZE] = '\0';
    buf->length = length;
    buf->gapStart = length/2;
    buf->gapEnd = buf->gapStart + PREFERRED_GAP_SIZE;
    memcpy(buf->buf, text, buf->gapStart);
    memcpy(&buf->buf[buf->gapEnd], &text[buf->gapStart], length-buf->gapStart);
#ifdef PURIFY
    {int i; for (i=buf->gapStart; i<buf->gapEnd; i++) buf->buf[i] = '.';}
#endif
    
    /* Zero all of the existing selections */
    updateSelections(buf, 0, deletedLength, 0);
    
    /* Call the saved display routine(s) to update the screen */
    callModifyCBs(buf, 0, deletedLength, length, 0, deletedText);
    NEditFree(deletedText);
}

/*
** Return a copy of the text between "start" and "end" character positions
** from text buffer "buf".  Positions start at 0, and the range does not
** include the character pointed to by "end"
*/
char* BufGetRange(const textBuffer* buf, int start, int end)
{
    char *text;
    int length, part1Length;
    
    /* Make sure start and end are ok, and allocate memory for returned string.
       If start is bad, return "", if end is bad, adjust it. */
    if (start < 0 || start > buf->length) {
    	text = (char*)NEditMalloc(1);
	text[0] = '\0';
        return text;
    }
    if (end < start) {
    	int temp = start;
    	start = end;
    	end = temp;
    }
    if (end > buf->length)
        end = buf->length;
    length = end - start;
    text = (char*)NEditMalloc(length+1);
    
    /* Copy the text from the buffer to the returned string */
    if (end <= buf->gapStart) {
        memcpy(text, &buf->buf[start], length);
    } else if (start >= buf->gapStart) {
        memcpy(text, &buf->buf[start+(buf->gapEnd-buf->gapStart)], length);
    } else {
        part1Length = buf->gapStart - start;
        memcpy(text, &buf->buf[start], part1Length);
        memcpy(&text[part1Length], &buf->buf[buf->gapEnd], length-part1Length);
    }
    text[length] = '\0';
    return text;
}

/*
** Return the character at buffer position "pos".  Positions start at 0.
*/
char BufGetCharacter(const textBuffer* buf, int pos)
{
    if (pos < 0 || pos >= buf->length)
        return '\0';
    if (pos < buf->gapStart)
        return buf->buf[pos];
    else
    	return buf->buf[pos + buf->gapEnd-buf->gapStart];
}

/*
** Insert null-terminated string "text" at position "pos" in "buf"
*/
void BufInsert(textBuffer *buf, int pos, const char *text)
{
    int nInserted;
    
    /* if pos is not contiguous to existing text, make it */
    if (pos > buf->length) pos = buf->length;
    if (pos < 0 ) pos = 0;

    /* Even if nothing is deleted, we must call these callbacks */
    callPreDeleteCBs(buf, pos, 0);

    /* insert and redisplay */
    nInserted = insert(buf, pos, text);
    buf->cursorPosHint = pos + nInserted;
    callModifyCBs(buf, pos, 0, nInserted, 0, NULL);
}

/*
** Delete the characters between "start" and "end", and insert the
** null-terminated string "text" in their place in in "buf"
*/
void BufReplace(textBuffer *buf, int start, int end, const char *text)
{
    char *deletedText;
    int nInserted = strlen(text);
    
    callPreDeleteCBs(buf, start, end-start);
    deletedText = BufGetRange(buf, start, end);
    delete(buf, start, end);
    insert(buf, start, text);
    buf->cursorPosHint = start + nInserted;
    callModifyCBs(buf, start, end-start, nInserted, 0, deletedText);
    NEditFree(deletedText);
}

void BufRemove(textBuffer *buf, int start, int end)
{
    char *deletedText;
    
    /* Make sure the arguments make sense */
    if (start > end) {
    	int temp = start;
	start = end;
	end = temp;
    }
    if (start > buf->length) start = buf->length;
    if (start < 0) start = 0;
    if (end > buf->length) end = buf->length;
    if (end < 0) end = 0;

    callPreDeleteCBs(buf, start, end-start);
    /* Remove and redisplay */
    deletedText = BufGetRange(buf, start, end);
    delete(buf, start, end);
    buf->cursorPosHint = start;
    callModifyCBs(buf, start, end-start, 0, 0, deletedText);
    NEditFree(deletedText);
}

void BufCopyFromBuf(textBuffer *fromBuf, textBuffer *toBuf, int fromStart,
    	int fromEnd, int toPos)
{
    int length = fromEnd - fromStart;
    int part1Length;

    /* Prepare the buffer to receive the new text.  If the new text fits in
       the current buffer, just move the gap (if necessary) to where
       the text should be inserted.  If the new text is too large, reallocate
       the buffer with a gap large enough to accomodate the new text and a
       gap of PREFERRED_GAP_SIZE */
    if (length > toBuf->gapEnd - toBuf->gapStart)
    	reallocateBuf(toBuf, toPos, length + PREFERRED_GAP_SIZE);
    else if (toPos != toBuf->gapStart)
	moveGap(toBuf, toPos);
    
    /* Insert the new text (toPos now corresponds to the start of the gap) */
    if (fromEnd <= fromBuf->gapStart) {
        memcpy(&toBuf->buf[toPos], &fromBuf->buf[fromStart], length);
    } else if (fromStart >= fromBuf->gapStart) {
        memcpy(&toBuf->buf[toPos],
            	&fromBuf->buf[fromStart+(fromBuf->gapEnd-fromBuf->gapStart)],
            	length);
    } else {
        part1Length = fromBuf->gapStart - fromStart;
        memcpy(&toBuf->buf[toPos], &fromBuf->buf[fromStart], part1Length);
        memcpy(&toBuf->buf[toPos+part1Length], &fromBuf->buf[fromBuf->gapEnd],
            	length-part1Length);
    }
    toBuf->gapStart += length;
    toBuf->length += length;
    updateSelections(toBuf, toPos, 0, length);
} 

/*
** Insert "text" columnwise into buffer starting at displayed character
** position "column" on the line beginning at "startPos".  Opens a rectangular
** space the width and height of "text", by moving all text to the right of
** "column" right.  If charsInserted and charsDeleted are not NULL, the
** number of characters inserted and deleted in the operation (beginning
** at startPos) are returned in these arguments
*/
void BufInsertCol(textBuffer *buf, int column, int startPos, const char *text,
    	int *charsInserted, int *charsDeleted)
{
    int nLines, lineStartPos, nDeleted, insertDeleted, nInserted;
    char *deletedText;
    
    nLines = countLines(text);
    lineStartPos = BufStartOfLine(buf, startPos);
    nDeleted = BufEndOfLine(buf, BufCountForwardNLines(buf, startPos, nLines)) -
    	    lineStartPos;
    callPreDeleteCBs(buf, lineStartPos, nDeleted);
    deletedText = BufGetRange(buf, lineStartPos, lineStartPos + nDeleted);
    insertCol(buf, column, lineStartPos, text, &insertDeleted, &nInserted,
    	    &buf->cursorPosHint);
    if (nDeleted != insertDeleted)
    	fprintf(stderr, "NEdit internal consistency check ins1 failed");
    callModifyCBs(buf, lineStartPos, nDeleted, nInserted, 0, deletedText);
    NEditFree(deletedText);
    if (charsInserted != NULL)
    	*charsInserted = nInserted;
    if (charsDeleted != NULL)
    	*charsDeleted = nDeleted;
}

/*
** Overlay "text" between displayed character positions "rectStart" and
** "rectEnd" on the line beginning at "startPos".  If charsInserted and
** charsDeleted are not NULL, the number of characters inserted and deleted
** in the operation (beginning at startPos) are returned in these arguments.
** If rectEnd equals -1, the width of the inserted text is measured first.
*/
void BufOverlayRect(textBuffer *buf, int startPos, int rectStart,
    	int rectEnd, const char *text, int *charsInserted, int *charsDeleted)
{
    int nLines, lineStartPos, nDeleted, insertDeleted, nInserted;
    char *deletedText;
    
    nLines = countLines(text);
    lineStartPos = BufStartOfLine(buf, startPos);
    if(rectEnd == -1)
        rectEnd = rectStart + textWidth(text, buf->tabDist, buf->nullSubsChar);
    lineStartPos = BufStartOfLine(buf, startPos);
    nDeleted = BufEndOfLine(buf, BufCountForwardNLines(buf, startPos, nLines)) -
    	    lineStartPos;
    callPreDeleteCBs(buf, lineStartPos, nDeleted);
    deletedText = BufGetRange(buf, lineStartPos, lineStartPos + nDeleted);
    overlayRect(buf, lineStartPos, rectStart, rectEnd, text, &insertDeleted,
    	    &nInserted, &buf->cursorPosHint);
    if (nDeleted != insertDeleted)
    	fprintf(stderr, "NEdit internal consistency check ovly1 failed");
    callModifyCBs(buf, lineStartPos, nDeleted, nInserted, 0, deletedText);
    NEditFree(deletedText);
    if (charsInserted != NULL)
    	*charsInserted = nInserted;
    if (charsDeleted != NULL)
    	*charsDeleted = nDeleted;
}

/*
** Replace a rectangular area in buf, given by "start", "end", "rectStart",
** and "rectEnd", with "text".  If "text" is vertically longer than the
** rectangle, add extra lines to make room for it.
*/
void BufReplaceRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd, const char *text)
{
    char *deletedText;
    char *insText=NULL;
    int i, nInsertedLines, nDeletedLines, insLen, hint;
    int insertDeleted, insertInserted, deleteInserted;
    int linesPadded = 0;
    
    /* Make sure start and end refer to complete lines, since the
       columnar delete and insert operations will replace whole lines */
    start = BufStartOfLine(buf, start);
    end = BufEndOfLine(buf, end);
    
    callPreDeleteCBs(buf, start, end-start);
    
    /* If more lines will be deleted than inserted, pad the inserted text
       with newlines to make it as long as the number of deleted lines.  This
       will indent all of the text to the right of the rectangle to the same
       column.  If more lines will be inserted than deleted, insert extra
       lines in the buffer at the end of the rectangle to make room for the
       additional lines in "text" */
    nInsertedLines = countLines(text);
    nDeletedLines = BufCountLines(buf, start, end);
    if (nInsertedLines < nDeletedLines) {
        char *insPtr;

    	insLen = strlen(text);
    	insText = (char*)NEditMalloc(insLen + nDeletedLines - nInsertedLines + 1);
    	strcpy(insText, text);
    	insPtr = insText + insLen;
    	for (i=0; i<nDeletedLines-nInsertedLines; i++)
    	    *insPtr++ = '\n';
    	*insPtr = '\0';
    } else if (nDeletedLines < nInsertedLines) {
    	linesPadded = nInsertedLines-nDeletedLines;
    	for (i=0; i<linesPadded; i++)
    	    insert(buf, end, "\n");
    } else /* nDeletedLines == nInsertedLines */ {
    }
    
    /* Save a copy of the text which will be modified for the modify CBs */
    deletedText = BufGetRange(buf, start, end);
    	  
    /* Delete then insert */
    deleteRect(buf, start, end, rectStart, rectEnd, &deleteInserted, &hint);
    if (insText) {
    	insertCol(buf, rectStart, start, insText, &insertDeleted, &insertInserted,
    		    &buf->cursorPosHint);
        NEditFree(insText);
    }
    else
    	insertCol(buf, rectStart, start, text, &insertDeleted, &insertInserted,
    		    &buf->cursorPosHint);
    
    /* Figure out how many chars were inserted and call modify callbacks */
    if (insertDeleted != deleteInserted + linesPadded)
    	fprintf(stderr, "NEdit: internal consistency check repl1 failed\n");
    callModifyCBs(buf, start, end-start, insertInserted, 0, deletedText);
    NEditFree(deletedText);
}

/*
** Remove a rectangular swath of characters between character positions start
** and end and horizontal displayed-character offsets rectStart and rectEnd.
*/
void BufRemoveRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd)
{
    char *deletedText;
    int nInserted;
    
    start = BufStartOfLine(buf, start);
    end = BufEndOfLine(buf, end);
    callPreDeleteCBs(buf, start, end-start);
    deletedText = BufGetRange(buf, start, end);
    deleteRect(buf, start, end, rectStart, rectEnd, &nInserted,
    	    &buf->cursorPosHint);
    callModifyCBs(buf, start, end-start, nInserted, 0, deletedText);
    NEditFree(deletedText);
}

/*
** Clear a rectangular "hole" out of the buffer between character positions
** start and end and horizontal displayed-character offsets rectStart and
** rectEnd.
*/
void BufClearRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd)
{
    int i, nLines;
    char *newlineString;
    
    nLines = BufCountLines(buf, start, end);
    newlineString = (char*)NEditMalloc(nLines+1);
    for (i=0; i<nLines; i++)
    	newlineString[i] = '\n';
    newlineString[i] = '\0';
    BufOverlayRect(buf, start, rectStart, rectEnd, newlineString,
    	    NULL, NULL);
    NEditFree(newlineString);
}

char *BufGetTextInRect(textBuffer *buf, int start, int end,
	int rectStart, int rectEnd)
{
    int lineStart, selLeft, selRight, len;
    char *textOut, *textIn, *outPtr, *retabbedStr;
   
    start = BufStartOfLine(buf, start);
    end = BufEndOfLine(buf, end);
    textOut = (char*)NEditMalloc((end - start) + 1);
    lineStart = start;
    outPtr = textOut;
    while (lineStart <= end) {
        findRectSelBoundariesForCopy(buf, lineStart, rectStart, rectEnd,
        	&selLeft, &selRight);
        textIn = BufGetRange(buf, selLeft, selRight);
        len = selRight - selLeft;
        memcpy(outPtr, textIn, len);
        NEditFree(textIn);
        outPtr += len;
        lineStart = BufEndOfLine(buf, selRight) + 1;
        *outPtr++ = '\n';
    }
    if (outPtr != textOut)
    	outPtr--;  /* don't leave trailing newline */
    *outPtr = '\0';
    
    /* If necessary, realign the tabs in the selection as if the text were
       positioned at the left margin */
    retabbedStr = realignTabs(textOut, rectStart, 0, buf->tabDist,
    	    buf->useTabs, buf->nullSubsChar, &len);
    NEditFree(textOut);
    return retabbedStr;
}

/*
** Get the hardware tab distance used by all displays for this buffer,
** and used in computing offsets for rectangular selection operations.
*/
int BufGetTabDistance(textBuffer *buf)
{
    return buf->tabDist;
}

/*
** Set the hardware tab distance used by all displays for this buffer,
** and used in computing offsets for rectangular selection operations.
*/
void BufSetTabDistance(textBuffer *buf, int tabDist)
{
    const char *deletedText;
    
    /* First call the pre-delete callbacks with the previous tab setting 
       still active. */
    callPreDeleteCBs(buf, 0, buf->length);
    
    /* Change the tab setting */
    buf->tabDist = tabDist;
    
    /* Force any display routines to redisplay everything */
    deletedText = BufAsString(buf);
    callModifyCBs(buf, 0, buf->length, buf->length, 0, deletedText);
}

void BufCheckDisplay(textBuffer *buf, int start, int end)
{
    /* just to make sure colors in the selected region are up to date */
    callModifyCBs(buf, start, 0, 0, end-start, NULL);
}

void BufSelect(textBuffer *buf, int start, int end)
{
    selection oldSelection = buf->primary;

    setSelection(&buf->primary, start, end);
    redisplaySelection(buf, &oldSelection, &buf->primary);
}

void BufUnselect(textBuffer *buf)
{
    selection oldSelection = buf->primary;

    buf->primary.selected = False;
    buf->primary.zeroWidth = False;
    redisplaySelection(buf, &oldSelection, &buf->primary);
}

void BufRectSelect(textBuffer *buf, int start, int end, int rectStart,
        int rectEnd)
{
    selection oldSelection = buf->primary;

    setRectSelect(&buf->primary, start, end, rectStart, rectEnd);
    redisplaySelection(buf, &oldSelection, &buf->primary);
}

int BufGetSelectionPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd)
{
    return getSelectionPos(&buf->primary, start, end, isRect, rectStart,
    	    rectEnd);
}

/* Same as above, but also returns TRUE for empty selections */
int BufGetEmptySelectionPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd)
{
    return getSelectionPos(&buf->primary, start, end, isRect, rectStart,
    	    rectEnd) || buf->primary.zeroWidth;
}

char *BufGetSelectionText(textBuffer *buf)
{
    return getSelectionText(buf, &buf->primary);
}

void BufRemoveSelected(textBuffer *buf)
{
    removeSelected(buf, &buf->primary);
}

void BufReplaceSelected(textBuffer *buf, const char *text)
{
    replaceSelected(buf, &buf->primary, text);
}

void BufSecondarySelect(textBuffer *buf, int start, int end)
{
    selection oldSelection = buf->secondary;

    setSelection(&buf->secondary, start, end);
    redisplaySelection(buf, &oldSelection, &buf->secondary);
}

void BufSecondaryUnselect(textBuffer *buf)
{
    selection oldSelection = buf->secondary;

    buf->secondary.selected = False;
    buf->secondary.zeroWidth = False;
    redisplaySelection(buf, &oldSelection, &buf->secondary);
}

void BufSecRectSelect(textBuffer *buf, int start, int end,
        int rectStart, int rectEnd)
{
    selection oldSelection = buf->secondary;

    setRectSelect(&buf->secondary, start, end, rectStart, rectEnd);
    redisplaySelection(buf, &oldSelection, &buf->secondary);
}

int BufGetSecSelectPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd)
{
    return getSelectionPos(&buf->secondary, start, end, isRect, rectStart,
    	    rectEnd);
}

char *BufGetSecSelectText(textBuffer *buf)
{
    return getSelectionText(buf, &buf->secondary);
}

void BufRemoveSecSelect(textBuffer *buf)
{
    removeSelected(buf, &buf->secondary);
}

void BufReplaceSecSelect(textBuffer *buf, const char *text)
{
    replaceSelected(buf, &buf->secondary, text);
}

void BufHighlight(textBuffer *buf, int start, int end)
{
    selection oldSelection = buf->highlight;

    setSelection(&buf->highlight, start, end);
    redisplaySelection(buf, &oldSelection, &buf->highlight);
}

void BufUnhighlight(textBuffer *buf)
{
    selection oldSelection = buf->highlight;

    buf->highlight.selected = False;
    buf->highlight.zeroWidth = False;
    redisplaySelection(buf, &oldSelection, &buf->highlight);
}

void BufRectHighlight(textBuffer *buf, int start, int end,
        int rectStart, int rectEnd)
{
    selection oldSelection = buf->highlight;

    setRectSelect(&buf->highlight, start, end, rectStart, rectEnd);
    redisplaySelection(buf, &oldSelection, &buf->highlight);
}

int BufGetHighlightPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd)
{
    return getSelectionPos(&buf->highlight, start, end, isRect, rectStart,
    	    rectEnd);
}

/*
** Add a callback routine to be called when the buffer is modified
*/
void BufAddModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB,
	void *cbArg)
{
    bufModifyCallbackProc *newModifyProcs;
    void **newCBArgs;
    int i;
    
    newModifyProcs = (bufModifyCallbackProc *)
    	    NEditMalloc(sizeof(bufModifyCallbackProc *) * (buf->nModifyProcs+1));
    newCBArgs = (void **)NEditMalloc(sizeof(void *) * (buf->nModifyProcs+1));
    for (i=0; i<buf->nModifyProcs; i++) {
    	newModifyProcs[i] = buf->modifyProcs[i];
    	newCBArgs[i] = buf->cbArgs[i];
    }
    if (buf->nModifyProcs != 0) {
	NEditFree(buf->modifyProcs);
	NEditFree(buf->cbArgs);
    }
    newModifyProcs[buf->nModifyProcs] = bufModifiedCB;
    newCBArgs[buf->nModifyProcs] = cbArg;
    buf->nModifyProcs++;
    buf->modifyProcs = newModifyProcs;
    buf->cbArgs = newCBArgs;
}

/*
** Similar to the above, but makes sure that the callback is called before
** normal priority callbacks.
*/
void BufAddHighPriorityModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB,
	void *cbArg)
{
    bufModifyCallbackProc *newModifyProcs;
    void **newCBArgs;
    int i;
    
    newModifyProcs = (bufModifyCallbackProc *)
    	    NEditMalloc(sizeof(bufModifyCallbackProc *) * (buf->nModifyProcs+1));
    newCBArgs = (void **)NEditMalloc(sizeof(void *) * (buf->nModifyProcs+1));
    for (i=0; i<buf->nModifyProcs; i++) {
    	newModifyProcs[i+1] = buf->modifyProcs[i];
    	newCBArgs[i+1] = buf->cbArgs[i];
    }
    if (buf->nModifyProcs != 0) {
	NEditFree(buf->modifyProcs);
	NEditFree(buf->cbArgs);
    }
    newModifyProcs[0] = bufModifiedCB;
    newCBArgs[0] = cbArg;
    buf->nModifyProcs++;
    buf->modifyProcs = newModifyProcs;
    buf->cbArgs = newCBArgs;
}

void BufRemoveModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB,
	void *cbArg)
{
    int i, toRemove = -1;
    bufModifyCallbackProc *newModifyProcs;
    void **newCBArgs;

    /* find the matching callback to remove */
    for (i=0; i<buf->nModifyProcs; i++) {
    	if (buf->modifyProcs[i] == bufModifiedCB && buf->cbArgs[i] == cbArg) {
    	    toRemove = i;
    	    break;
    	}
    }
    if (toRemove == -1) {
    	fprintf(stderr, "NEdit Internal Error: Can't find modify CB to remove\n");
    	return;
    }
    
    /* Allocate new lists for remaining callback procs and args (if
       any are left) */
    buf->nModifyProcs--;
    if (buf->nModifyProcs == 0) {
    	buf->nModifyProcs = 0;
    	NEditFree(buf->modifyProcs);
    	buf->modifyProcs = NULL;
	NEditFree(buf->cbArgs);
	buf->cbArgs = NULL;
	return;
    }
    newModifyProcs = (bufModifyCallbackProc *)
    	    NEditMalloc(sizeof(bufModifyCallbackProc *) * (buf->nModifyProcs));
    newCBArgs = (void **)NEditMalloc(sizeof(void *) * (buf->nModifyProcs));
    
    /* copy out the remaining members and free the old lists */
    for (i=0; i<toRemove; i++) {
    	newModifyProcs[i] = buf->modifyProcs[i];
    	newCBArgs[i] = buf->cbArgs[i];
    }
    for (; i<buf->nModifyProcs; i++) {
	newModifyProcs[i] = buf->modifyProcs[i+1];
    	newCBArgs[i] = buf->cbArgs[i+1];
    }
    NEditFree(buf->modifyProcs);
    NEditFree(buf->cbArgs);
    buf->modifyProcs = newModifyProcs;
    buf->cbArgs = newCBArgs;
}

/*
** Add a callback routine to be called before text is deleted from the buffer.
*/
void BufAddPreDeleteCB(textBuffer *buf, bufPreDeleteCallbackProc bufPreDeleteCB,
	void *cbArg)
{
    bufPreDeleteCallbackProc *newPreDeleteProcs;
    void **newCBArgs;
    int i;
    
    newPreDeleteProcs = (bufPreDeleteCallbackProc *)
    	    NEditMalloc(sizeof(bufPreDeleteCallbackProc *) * (buf->nPreDeleteProcs+1));
    newCBArgs = (void **)NEditMalloc(sizeof(void *) * (buf->nPreDeleteProcs+1));
    for (i=0; i<buf->nPreDeleteProcs; i++) {
    	newPreDeleteProcs[i] = buf->preDeleteProcs[i];
    	newCBArgs[i] = buf->preDeleteCbArgs[i];
    }
    if (buf->nPreDeleteProcs != 0) {
	NEditFree(buf->preDeleteProcs);
	NEditFree(buf->preDeleteCbArgs);
    }
    newPreDeleteProcs[buf->nPreDeleteProcs] =  bufPreDeleteCB;
    newCBArgs[buf->nPreDeleteProcs] = cbArg;
    buf->nPreDeleteProcs++;
    buf->preDeleteProcs = newPreDeleteProcs;
    buf->preDeleteCbArgs = newCBArgs;
}

void BufRemovePreDeleteCB(textBuffer *buf, bufPreDeleteCallbackProc bufPreDeleteCB,
	void *cbArg)
{
    int i, toRemove = -1;
    bufPreDeleteCallbackProc *newPreDeleteProcs;
    void **newCBArgs;

    /* find the matching callback to remove */
    for (i=0; i<buf->nPreDeleteProcs; i++) {
    	if (buf->preDeleteProcs[i] == bufPreDeleteCB && 
	    buf->preDeleteCbArgs[i] == cbArg) {
    	    toRemove = i;
    	    break;
    	}
    }
    if (toRemove == -1) {
    	fprintf(stderr, "NEdit Internal Error: Can't find pre-delete CB to remove\n");
    	return;
    }
    
    /* Allocate new lists for remaining callback procs and args (if
       any are left) */
    buf->nPreDeleteProcs--;
    if (buf->nPreDeleteProcs == 0) {
    	buf->nPreDeleteProcs = 0;
    	NEditFree(buf->preDeleteProcs);
    	buf->preDeleteProcs = NULL;
	NEditFree(buf->preDeleteCbArgs);
	buf->preDeleteCbArgs = NULL;
	return;
    }
    newPreDeleteProcs = (bufPreDeleteCallbackProc *)
    	    NEditMalloc(sizeof(bufPreDeleteCallbackProc *) * (buf->nPreDeleteProcs));
    newCBArgs = (void **)NEditMalloc(sizeof(void *) * (buf->nPreDeleteProcs));
    
    /* copy out the remaining members and free the old lists */
    for (i=0; i<toRemove; i++) {
    	newPreDeleteProcs[i] = buf->preDeleteProcs[i];
    	newCBArgs[i] = buf->preDeleteCbArgs[i];
    }
    for (; i<buf->nPreDeleteProcs; i++) {
	newPreDeleteProcs[i] = buf->preDeleteProcs[i+1];
    	newCBArgs[i] = buf->preDeleteCbArgs[i+1];
    }
    NEditFree(buf->preDeleteProcs);
    NEditFree(buf->preDeleteCbArgs);
    buf->preDeleteProcs = newPreDeleteProcs;
    buf->preDeleteCbArgs = newCBArgs;
}

/*
** Find the position of the start of the line containing position "pos"
*/
int BufStartOfLine(textBuffer *buf, int pos)
{
    int startPos;
    
    if (!searchBackward(buf, pos, '\n', &startPos))
    	return 0;
    return startPos + 1;
}


/*
** Find the position of the end of the line containing position "pos"
** (which is either a pointer to the newline character ending the line,
** or a pointer to one character beyond the end of the buffer)
*/
int BufEndOfLine(textBuffer *buf, int pos)
{
    int endPos;
    
    if (!searchForward(buf, pos, '\n', &endPos))
    	endPos = buf->length;
    return endPos;
}

/*
** Get a character from the text buffer expanded into it's screen
** representation (which may be several characters for a tab or a
** control code).  Returns the number of characters written to "outStr".
** "indent" is the number of characters from the start of the line
** for figuring tabs.  Output string is guranteed to be shorter or
** equal in length to MAX_EXP_CHAR_LEN
*/
int BufGetExpandedChar(const textBuffer* buf, int pos, int indent,
        char* outStr)
{
    return BufExpandCharacter(BufGetCharacter(buf, pos), indent, outStr,
    	    buf->tabDist, buf->nullSubsChar);
}

/*
** Expand a single character from the text buffer into it's screen
** representation (which may be several characters for a tab or a
** control code).  Returns the number of characters added to "outStr".
** "indent" is the number of characters from the start of the line
** for figuring tabs.  Output string is guranteed to be shorter or
** equal in length to MAX_EXP_CHAR_LEN
*/
int BufExpandCharacter(char c, int indent, char *outStr,
        int tabDist, char nullSubsChar)
{
    int i, nSpaces;
    
    /* Convert tabs to spaces */
    if (c == '\t') {
	nSpaces = tabDist - (indent % tabDist);
	for (i=0; i<nSpaces; i++)
	    outStr[i] = ' ';
	return nSpaces;
    }
    
    /* Convert ASCII (and EBCDIC in the __MVS__ (OS/390) case) control
       codes to readable character sequences */
    if (c == nullSubsChar) {
	sprintf(outStr, "<nul>");
    	return 5;
    }
#ifdef __MVS__
    if (((unsigned char)c) <= 63) {
    	sprintf(outStr, "<%s>", ControlCodeTable[(unsigned char)c]);
    	return strlen(outStr);
    }
#else
    if (((unsigned char)c) <= 31) {
    	sprintf(outStr, "<%s>", ControlCodeTable[(unsigned char)c]);
    	return strlen(outStr);
    } else if (c == 127) {
    	sprintf(outStr, "<del>");
    	return 5;
    }
#endif
    
    /* Otherwise, just return the character */
    *outStr = c;
    return 1;
}

/*
** Return the length in displayed characters of character "c" expanded
** for display (as discussed above in BufGetExpandedChar).  If the
** buffer for which the character width is being measured is doing null
** substitution, nullSubsChar should be passed as that character (or nul
** to ignore).
*/
int BufCharWidth(char c, int indent, int tabDist, char nullSubsChar)
{
    /* Note, this code must parallel that in BufExpandCharacter */
    if (c == nullSubsChar)
    	return 5;
    else if (c == '\t')
	return tabDist - (indent % tabDist);
    else if (((unsigned char)c) <= 31)
    	return strlen(ControlCodeTable[(unsigned char)c]) + 2;
    else if (c == 127)
    	return 5;
    return 1;
}

/*
** Count the number of displayed characters between buffer position
** "lineStartPos" and "targetPos". (displayed characters are the characters
** shown on the screen to represent characters in the buffer, where tabs and
** control characters are expanded)
*/
int BufCountDispChars(const textBuffer* buf, int lineStartPos,
        int targetPos)
{
    int pos, charCount = 0;
    char expandedChar[MAX_EXP_CHAR_LEN];
    
    pos = lineStartPos;
    while (pos < targetPos && pos < buf->length)
    	charCount += BufGetExpandedChar(buf, pos++, charCount, expandedChar);
    return charCount;
}

/*
** Count forward from buffer position "startPos" in displayed characters
** (displayed characters are the characters shown on the screen to represent
** characters in the buffer, where tabs and control characters are expanded)
*/
int BufCountForwardDispChars(textBuffer *buf, int lineStartPos, int nChars)
{
    int pos, charCount = 0;
    char c;
    
    pos = lineStartPos;
    while (charCount < nChars && pos < buf->length) {
    	c = BufGetCharacter(buf, pos);
    	if (c == '\n')
    	    return pos;
    	charCount += BufCharWidth(c, charCount, buf->tabDist,buf->nullSubsChar);
    	pos++;
    }
    return pos;
}

/*
** Count the number of newlines between startPos and endPos in buffer "buf".
** The character at position "endPos" is not counted.
*/
int BufCountLines(textBuffer *buf, int startPos, int endPos)
{
    int pos, gapLen = buf->gapEnd - buf->gapStart;
    int lineCount = 0;
    
    pos = startPos;
    while (pos < buf->gapStart) {
        if (pos == endPos)
            return lineCount;
        if (buf->buf[pos++] == '\n')
            lineCount++;
    }
    while (pos < buf->length) {
        if (pos == endPos)
            return lineCount;
    	if (buf->buf[pos++ + gapLen] == '\n')
            lineCount++;
    }
    return lineCount;
}

/*
** Find the first character of the line "nLines" forward from "startPos"
** in "buf" and return its position
*/
int BufCountForwardNLines(const textBuffer* buf, int startPos,
        unsigned nLines)
{
    int pos, gapLen = buf->gapEnd - buf->gapStart;
    int lineCount = 0;
    
    if (nLines == 0)
    	return startPos;
    
    pos = startPos;
    while (pos < buf->gapStart) {
        if (buf->buf[pos++] == '\n') {
            lineCount++;
            if (lineCount == nLines)
            	return pos;
        }
    }
    while (pos < buf->length) {
    	if (buf->buf[pos++ + gapLen] == '\n') {
            lineCount++;
            if (lineCount >= nLines)
            	return pos;
        }
    }
    return pos;
}

/*
** Find the position of the first character of the line "nLines" backwards
** from "startPos" (not counting the character pointed to by "startpos" if
** that is a newline) in "buf".  nLines == 0 means find the beginning of
** the line
*/
int BufCountBackwardNLines(textBuffer *buf, int startPos, int nLines)
{
    int pos, gapLen = buf->gapEnd - buf->gapStart;
    int lineCount = -1;
    
    pos = startPos - 1;
    if (pos <= 0)
    	return 0;
    
    while (pos >= buf->gapStart) {
    	if (buf->buf[pos + gapLen] == '\n') {
            if (++lineCount >= nLines)
            	return pos + 1;
        }
        pos--;
    }
    while (pos >= 0) {
        if (buf->buf[pos] == '\n') {
            if (++lineCount >= nLines)
            	return pos + 1;
        }
        pos--;
    }
    return 0;
}

/*
** Search forwards in buffer "buf" for characters in "searchChars", starting
** with the character "startPos", and returning the result in "foundPos"
** returns True if found, False if not.
*/
int BufSearchForward(textBuffer *buf, int startPos, const char *searchChars,
	int *foundPos)
{
    int pos, gapLen = buf->gapEnd - buf->gapStart;
    const char *c;
    
    pos = startPos;
    while (pos < buf->gapStart) {
        for (c=searchChars; *c!='\0'; c++) {
	    if (buf->buf[pos] == *c) {
        	*foundPos = pos;
        	return True;
            }
        }
        pos++;
    }
    while (pos < buf->length) {
    	for (c=searchChars; *c!='\0'; c++) {
    	    if (buf->buf[pos + gapLen] == *c) {
        	*foundPos = pos;
        	return True;
            }
        }
        pos++;
    }
    *foundPos = buf->length;
    return False;
}

/*
** Search backwards in buffer "buf" for characters in "searchChars", starting
** with the character BEFORE "startPos", returning the result in "foundPos"
** returns True if found, False if not.
*/
int BufSearchBackward(textBuffer *buf, int startPos, const char *searchChars,
	int *foundPos)
{
    int pos, gapLen = buf->gapEnd - buf->gapStart;
    const char *c;
    
    if (startPos == 0) {
    	*foundPos = 0;
    	return False;
    }
    pos = startPos == 0 ? 0 : startPos - 1;
    while (pos >= buf->gapStart) {
    	for (c=searchChars; *c!='\0'; c++) {
    	    if (buf->buf[pos + gapLen] == *c) {
        	*foundPos = pos;
        	return True;
            }
        }
        pos--;
    }
    while (pos >= 0) {
    	for (c=searchChars; *c!='\0'; c++) {
            if (buf->buf[pos] == *c) {
        	*foundPos = pos;
        	return True;
            }
        }
        pos--;
    }
    *foundPos = 0;
    return False;
}

/*
** A horrible design flaw in NEdit (from the very start, before we knew that
** NEdit would become so popular), is that it uses C NULL terminated strings
** to hold text.  This means editing text containing NUL characters is not
** possible without special consideration.  Here is the special consideration.
** The routines below maintain a special substitution-character which stands
** in for a null, and translates strings an buffers back and forth from/to
** the substituted form, figure out what to substitute, and figure out
** when we're in over our heads and no translation is possible.
*/

/*
** The primary routine for integrating new text into a text buffer with
** substitution of another character for ascii nuls.  This substitutes null
** characters in the string in preparation for being copied or replaced
** into the buffer, and if neccessary, adjusts the buffer as well, in the
** event that the string contains the character it is currently using for
** substitution.  Returns False, if substitution is no longer possible
** because all non-printable characters are already in use.
*/
int BufSubstituteNullChars(char *string, int length, textBuffer *buf)
{
    char histogram[256];

    /* Find out what characters the string contains */
    histogramCharacters(string, length, histogram, True);
    
    /* Does the string contain the null-substitute character?  If so, re-
       histogram the buffer text to find a character which is ok in both the
       string and the buffer, and change the buffer's null-substitution
       character.  If none can be found, give up and return False */
    if (histogram[(unsigned char)buf->nullSubsChar] != 0) {
        char *bufString, newSubsChar;
        /* here we know we can modify the file buffer directly,
           so we cast away constness */
        bufString = (char *)BufAsString(buf);
        histogramCharacters(bufString, buf->length, histogram, False);
        newSubsChar = chooseNullSubsChar(histogram);
        if (newSubsChar == '\0') {
            return False;
        }
        /* bufString points to the buffer's data, so we substitute in situ */
        subsChars(bufString, buf->length, buf->nullSubsChar, newSubsChar);
        buf->nullSubsChar = newSubsChar;
    }

    /* If the string contains null characters, substitute them with the
       buffer's null substitution character */
    if (histogram[0] != 0)
	subsChars(string, length, '\0', buf->nullSubsChar);
    return True;
}

/*
** Convert strings obtained from buffers which contain null characters, which
** have been substituted for by a special substitution character, back to
** a null-containing string.  There is no time penalty for calling this
** routine if no substitution has been done.
*/
void BufUnsubstituteNullChars(char *string, textBuffer *buf)
{
    register char *c, subsChar = buf->nullSubsChar;
    
    if (subsChar == '\0')
	return;
    for (c=string; *c != '\0'; c++)
    	if (*c == subsChar)
	    *c = '\0';
}

/* 
** Compares len Bytes contained in buf starting at Position pos with
** the contens of cmpText. Returns 0 if there are no differences, 
** != 0 otherwise.
**
*/
int BufCmp(textBuffer * buf, int pos, int len, const char *cmpText)
{
    int     posEnd;
    int     part1Length;
    int     result;

    posEnd = pos + len;
    if (posEnd > buf->length) {
        return (1);
    }
    if (pos < 0) {
        return (-1);
    }

    if (posEnd <= buf->gapStart) {
        return (strncmp(&(buf->buf[pos]), cmpText, len));
    } else if (pos >= buf->gapStart) {
        return (strncmp (&buf->buf[pos + (buf->gapEnd - buf->gapStart)], 
	    	    	cmpText, len));
    } else {
        part1Length = buf->gapStart - pos;
        result = strncmp(&buf->buf[pos], cmpText, part1Length);
        if (result) {
            return (result);
	}
        return (strncmp(&buf->buf[buf->gapEnd], &cmpText[part1Length], 
	    	    	len - part1Length));
    }
}

/*
** Create a pseudo-histogram of the characters in a string (don't actually
** count, because we don't want overflow, just mark the character's presence
** with a 1).  If init is true, initialize the histogram before acumulating.
** if not, add the new data to an existing histogram.
*/
static void histogramCharacters(const char *string, int length, char hist[256],
	int init)
{
    int i;
    const char *c;

    if (init)
	for (i=0; i<256; i++)
	    hist[i] = 0;
    for (c=string; c < &string[length]; c++)
        hist[*((unsigned char *)c)] |= 1;
}

/*
** Substitute fromChar with toChar in string.
*/
static void subsChars(char *string, int length, char fromChar, char toChar)
{
    char *c;
    
    for (c=string; c < &string[length]; c++)
	if (*c == fromChar) *c = toChar;
}

/*
** Search through ascii control characters in histogram in order of least
** likelihood of use, find an unused character to use as a stand-in for a
** null.  If the character set is full (no available characters outside of
** the printable set, return the null character.
*/
static char chooseNullSubsChar(char hist[256])
{
#define N_REPLACEMENTS 25
    static char replacements[N_REPLACEMENTS] = {1,2,3,4,5,6,14,15,16,17,18,19,
	    20,21,22,23,24,25,26,28,29,30,31,11,7};
    int i;
    for (i = 0; i < N_REPLACEMENTS; i++)
	if (hist[(unsigned char)replacements[i]] == 0)
	    return replacements[i];
    return '\0';
}
	    
/*
** Internal (non-redisplaying) version of BufInsert.  Returns the length of
** text inserted (this is just strlen(text), however this calculation can be
** expensive and the length will be required by any caller who will continue
** on to call redisplay).  pos must be contiguous with the existing text in
** the buffer (i.e. not past the end).
*/
static int insert(textBuffer *buf, int pos, const char *text)
{
    int length = strlen(text);

    /* Prepare the buffer to receive the new text.  If the new text fits in
       the current buffer, just move the gap (if necessary) to where
       the text should be inserted.  If the new text is too large, reallocate
       the buffer with a gap large enough to accomodate the new text and a
       gap of PREFERRED_GAP_SIZE */
    if (length > buf->gapEnd - buf->gapStart)
    	reallocateBuf(buf, pos, length + PREFERRED_GAP_SIZE);
    else if (pos != buf->gapStart)
	moveGap(buf, pos);
    
    /* Insert the new text (pos now corresponds to the start of the gap) */
    memcpy(&buf->buf[pos], text, length);
    buf->gapStart += length;
    buf->length += length;
    updateSelections(buf, pos, 0, length);
    
    return length;
}

/*
** Internal (non-redisplaying) version of BufRemove.  Removes the contents
** of the buffer between start and end (and moves the gap to the site of
** the delete).
*/
static void delete(textBuffer *buf, int start, int end)
{
    /* if the gap is not contiguous to the area to remove, move it there */
    if (start > buf->gapStart)
    	moveGap(buf, start);
    else if (end < buf->gapStart)
    	moveGap(buf, end);

    /* expand the gap to encompass the deleted characters */
    buf->gapEnd += end - buf->gapStart;
    buf->gapStart -= buf->gapStart - start;
    
    /* update the length */
    buf->length -= end - start;
    
    /* fix up any selections which might be affected by the change */
    updateSelections(buf, start, end-start, 0);
}

/*
** Insert a column of text without calling the modify callbacks.  Note that
** in some pathological cases, inserting can actually decrease the size of
** the buffer because of spaces being coalesced into tabs.  "nDeleted" and
** "nInserted" return the number of characters deleted and inserted beginning
** at the start of the line containing "startPos".  "endPos" returns buffer
** position of the lower left edge of the inserted column (as a hint for
** routines which need to set a cursor position).
*/
static void insertCol(textBuffer *buf, int column, int startPos,
        const char *insText, int *nDeleted, int *nInserted, int *endPos)
{
    int nLines, start, end, insWidth, lineStart, lineEnd;
    int expReplLen, expInsLen, len, endOffset;
    char *outStr, *outPtr, *line, *replText, *expText, *insLine;
    const char *insPtr;

    if (column < 0)
    	column = 0;
    	
    /* Allocate a buffer for the replacement string large enough to hold 
       possibly expanded tabs in both the inserted text and the replaced
       area, as well as per line: 1) an additional 2*MAX_EXP_CHAR_LEN
       characters for padding where tabs and control characters cross the
       column of the selection, 2) up to "column" additional spaces per
       line for padding out to the position of "column", 3) padding up
       to the width of the inserted text if that must be padded to align
       the text beyond the inserted column.  (Space for additional
       newlines if the inserted text extends beyond the end of the buffer
       is counted with the length of insText) */
    start = BufStartOfLine(buf, startPos);
    nLines = countLines(insText) + 1;
    insWidth = textWidth(insText, buf->tabDist, buf->nullSubsChar);
    end = BufEndOfLine(buf, BufCountForwardNLines(buf, start, nLines-1));
    replText = BufGetRange(buf, start, end);
    expText = expandTabs(replText, 0, buf->tabDist, buf->nullSubsChar,
	    &expReplLen);
    NEditFree(replText);
    NEditFree(expText);
    expText = expandTabs(insText, 0, buf->tabDist, buf->nullSubsChar,
	    &expInsLen);
    NEditFree(expText);
    outStr = (char*) NEditMalloc(expReplLen + expInsLen +
    	    nLines * (column + insWidth + MAX_EXP_CHAR_LEN) + 1);
    
    /* Loop over all lines in the buffer between start and end inserting
       text at column, splitting tabs and adding padding appropriately */
    outPtr = outStr;
    lineStart = start;
    insPtr = insText;
    while (True) {
    	lineEnd = BufEndOfLine(buf, lineStart);
    	line = BufGetRange(buf, lineStart, lineEnd);
    	insLine = copyLine(insPtr, &len);
    	insPtr += len;
    	insertColInLine(line, insLine, column, insWidth, buf->tabDist,
    		buf->useTabs, buf->nullSubsChar, outPtr, &len, &endOffset);
    	NEditFree(line);
    	NEditFree(insLine);
#if 0   /* Earlier comments claimed that trailing whitespace could multiply on
        the ends of lines, but insertColInLine looks like it should never
        add space unnecessarily, and this trimming interfered with
        paragraph filling, so lets see if it works without it. MWE */
        {
            char *c;
    	    for (c=outPtr+len-1; c>outPtr && (*c == ' ' || *c == '\t'); c--)
                len--;
        }
#endif
	outPtr += len;
	*outPtr++ = '\n';
    	lineStart = lineEnd < buf->length ? lineEnd + 1 : buf->length;
    	if (*insPtr == '\0')
    	    break;
    	insPtr++;
    }
    if (outPtr != outStr)
    	outPtr--; /* trim back off extra newline */
    *outPtr = '\0';
    
    /* replace the text between start and end with the new stuff */
    delete(buf, start, end);
    insert(buf, start, outStr);
    *nInserted = outPtr - outStr;
    *nDeleted = end - start;
    *endPos = start + (outPtr - outStr) - len + endOffset;
    NEditFree(outStr);
}

/*
** Delete a rectangle of text without calling the modify callbacks.  Returns
** the number of characters replacing those between start and end.  Note that
** in some pathological cases, deleting can actually increase the size of
** the buffer because of tab expansions.  "endPos" returns the buffer position
** of the point in the last line where the text was removed (as a hint for
** routines which need to position the cursor after a delete operation)
*/
static void deleteRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd, int *replaceLen, int *endPos)
{
    int nLines, lineStart, lineEnd, len, endOffset;
    char *outStr, *outPtr, *line, *text, *expText;
    
    /* allocate a buffer for the replacement string large enough to hold 
       possibly expanded tabs as well as an additional  MAX_EXP_CHAR_LEN * 2
       characters per line for padding where tabs and control characters cross
       the edges of the selection */
    start = BufStartOfLine(buf, start);
    end = BufEndOfLine(buf, end);
    nLines = BufCountLines(buf, start, end) + 1;
    text = BufGetRange(buf, start, end);
    expText = expandTabs(text, 0, buf->tabDist, buf->nullSubsChar, &len);
    NEditFree(text);
    NEditFree(expText);
    outStr = (char*)NEditMalloc(len + nLines * MAX_EXP_CHAR_LEN * 2 + 1);
    
    /* loop over all lines in the buffer between start and end removing
       the text between rectStart and rectEnd and padding appropriately */
    lineStart = start;
    outPtr = outStr;
    while (lineStart <= buf->length && lineStart <= end) {
    	lineEnd = BufEndOfLine(buf, lineStart);
    	line = BufGetRange(buf, lineStart, lineEnd);
    	deleteRectFromLine(line, rectStart, rectEnd, buf->tabDist,
    		buf->useTabs, buf->nullSubsChar, outPtr, &len, &endOffset);
    	NEditFree(line);
	outPtr += len;
	*outPtr++ = '\n';
    	lineStart = lineEnd + 1;
    }
    if (outPtr != outStr)
    	outPtr--; /* trim back off extra newline */
    *outPtr = '\0';
    
    /* replace the text between start and end with the newly created string */
    delete(buf, start, end);
    insert(buf, start, outStr);
    *replaceLen = outPtr - outStr;
    *endPos = start + (outPtr - outStr) - len + endOffset;
    NEditFree(outStr);
}

/*
** Overlay a rectangular area of text without calling the modify callbacks.
** "nDeleted" and "nInserted" return the number of characters deleted and
** inserted beginning at the start of the line containing "startPos".
** "endPos" returns buffer position of the lower left edge of the inserted
** column (as a hint for routines which need to set a cursor position).
*/
static void overlayRect(textBuffer *buf, int startPos, int rectStart,
    	int rectEnd, const char *insText,
	int *nDeleted, int *nInserted, int *endPos)
{
    int nLines, start, end, lineStart, lineEnd;
    int expInsLen, len, endOffset;
    char *c, *outStr, *outPtr, *line, *expText, *insLine;
    const char *insPtr;

    /* Allocate a buffer for the replacement string large enough to hold
       possibly expanded tabs in the inserted text, as well as per line: 1)
       an additional 2*MAX_EXP_CHAR_LEN characters for padding where tabs
       and control characters cross the column of the selection, 2) up to
       "column" additional spaces per line for padding out to the position
       of "column", 3) padding up to the width of the inserted text if that
       must be padded to align the text beyond the inserted column.  (Space
       for additional newlines if the inserted text extends beyond the end
       of the buffer is counted with the length of insText) */
    start = BufStartOfLine(buf, startPos);
    nLines = countLines(insText) + 1;
    end = BufEndOfLine(buf, BufCountForwardNLines(buf, start, nLines-1));
    expText = expandTabs(insText, 0, buf->tabDist, buf->nullSubsChar,
	    &expInsLen);
    NEditFree(expText);
    outStr = (char*)NEditMalloc(end-start + expInsLen +
    	    nLines * (rectEnd + MAX_EXP_CHAR_LEN) + 1);
    
    /* Loop over all lines in the buffer between start and end overlaying the
       text between rectStart and rectEnd and padding appropriately.  Trim
       trailing space from line (whitespace at the ends of lines otherwise
       tends to multiply, since additional padding is added to maintain it */
    outPtr = outStr;
    lineStart = start;
    insPtr = insText;
    while (True) {
    	lineEnd = BufEndOfLine(buf, lineStart);
    	line = BufGetRange(buf, lineStart, lineEnd);
    	insLine = copyLine(insPtr, &len);
    	insPtr += len;
    	overlayRectInLine(line, insLine, rectStart, rectEnd, buf->tabDist,
		buf->useTabs, buf->nullSubsChar, outPtr, &len, &endOffset);
    	NEditFree(line);
    	NEditFree(insLine);
    	for (c=outPtr+len-1; c>outPtr && (*c == ' ' || *c == '\t'); c--)
    	    len--;
	outPtr += len;
	*outPtr++ = '\n';
    	lineStart = lineEnd < buf->length ? lineEnd + 1 : buf->length;
    	if (*insPtr == '\0')
    	    break;
    	insPtr++;
    }
    if (outPtr != outStr)
    	outPtr--; /* trim back off extra newline */
    *outPtr = '\0';
    
    /* replace the text between start and end with the new stuff */
    delete(buf, start, end);
    insert(buf, start, outStr);
    *nInserted = outPtr - outStr;
    *nDeleted = end - start;
    *endPos = start + (outPtr - outStr) - len + endOffset;
    NEditFree(outStr);
}

/*
** Insert characters from single-line string "insLine" in single-line string
** "line" at "column", leaving "insWidth" space before continuing line.
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
*/
static void insertColInLine(const char *line, const char *insLine,
        int column, int insWidth, int tabDist, int useTabs, char nullSubsChar,
	char *outStr, int *outLen, int *endOffset)
{
    char *c, *outPtr, *retabbedStr;
    const char *linePtr;
    int indent, toIndent, len, postColIndent;
        
    /* copy the line up to "column" */ 
    outPtr = outStr;
    indent = 0;
    for (linePtr=line; *linePtr!='\0'; linePtr++) {
	len = BufCharWidth(*linePtr, indent, tabDist, nullSubsChar);
	if (indent + len > column)
    	    break;
    	indent += len;
	*outPtr++ = *linePtr;
    }
    
    /* If "column" falls in the middle of a character, and the character is a
       tab, leave it off and leave the indent short and it will get padded
       later.  If it's a control character, insert it and adjust indent
       accordingly. */
    if (indent < column && *linePtr != '\0') {
    	postColIndent = indent + len;
    	if (*linePtr == '\t')
    	    linePtr++;
    	else {
    	    *outPtr++ = *linePtr++;
    	    indent += len;
    	}
    } else
    	postColIndent = indent;
    
    /* If there's no text after the column and no text to insert, that's all */
    if (*insLine == '\0' && *linePtr == '\0') {
    	*outLen = *endOffset = outPtr - outStr;
    	return;
    }
    
    /* pad out to column if text is too short */
    if (indent < column) {
	addPadding(outPtr, indent, column, tabDist, useTabs, nullSubsChar,&len);
	outPtr += len;
	indent = column;
    }
    
    /* Copy the text from "insLine" (if any), recalculating the tabs as if
       the inserted string began at column 0 to its new column destination */
    if (*insLine != '\0') {
	retabbedStr = realignTabs(insLine, 0, indent, tabDist, useTabs,
		nullSubsChar, &len);
	for (c=retabbedStr; *c!='\0'; c++) {
    	    *outPtr++ = *c;
    	    len = BufCharWidth(*c, indent, tabDist, nullSubsChar);
    	    indent += len;
	}
	NEditFree(retabbedStr);
    }
    
    /* If the original line did not extend past "column", that's all */
    if (*linePtr == '\0') {
    	*outLen = *endOffset = outPtr - outStr;
    	return;
    }
    
    /* Pad out to column + width of inserted text + (additional original
       offset due to non-breaking character at column) */
    toIndent = column + insWidth + postColIndent-column;
    addPadding(outPtr, indent, toIndent, tabDist, useTabs, nullSubsChar, &len);
    outPtr += len;
    indent = toIndent;
    
    /* realign tabs for text beyond "column" and write it out */
    retabbedStr = realignTabs(linePtr, postColIndent, indent, tabDist,
    	useTabs, nullSubsChar, &len);
    strcpy(outPtr, retabbedStr);
    NEditFree(retabbedStr);
    *endOffset = outPtr - outStr;
    *outLen = (outPtr - outStr) + len;
}

/*
** Remove characters in single-line string "line" between displayed positions
** "rectStart" and "rectEnd", and write the result to "outStr", which is
** assumed to be large enough to hold the returned string.  Note that in
** certain cases, it is possible for the string to get longer due to
** expansion of tabs.  "endOffset" returns the number of characters from
** the beginning of the string to the point where the characters were
** deleted (as a hint for routines which need to position the cursor).
*/
static void deleteRectFromLine(const char *line, int rectStart, int rectEnd,
	int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen,
	int *endOffset)
{
    int indent, preRectIndent, postRectIndent, len;
    const char *c;
    char *outPtr;
    char *retabbedStr;
    
    /* copy the line up to rectStart */
    outPtr = outStr;
    indent = 0;
    for (c=line; *c!='\0'; c++) {
	if (indent > rectStart)
	    break;
	len = BufCharWidth(*c, indent, tabDist, nullSubsChar);
	if (indent + len > rectStart && (indent == rectStart || *c == '\t'))
    	    break;
    	indent += len;
	*outPtr++ = *c;
    }
    preRectIndent = indent;
    
    /* skip the characters between rectStart and rectEnd */
    for(; *c!='\0' && indent<rectEnd; c++)
	indent += BufCharWidth(*c, indent, tabDist, nullSubsChar);
    postRectIndent = indent;
    
    /* If the line ended before rectEnd, there's nothing more to do */
    if (*c == '\0') {
    	*outPtr = '\0';
    	*outLen = *endOffset = outPtr - outStr;
    	return;
    }
    
    /* fill in any space left by removed tabs or control characters
       which straddled the boundaries */
    indent = max(rectStart + postRectIndent-rectEnd, preRectIndent);
    addPadding(outPtr, preRectIndent, indent, tabDist, useTabs, nullSubsChar,
	    &len);
    outPtr += len;

    /* Copy the rest of the line.  If the indentation has changed, preserve
       the position of non-whitespace characters by converting tabs to
       spaces, then back to tabs with the correct offset */
    retabbedStr = realignTabs(c, postRectIndent, indent, tabDist, useTabs,
    	    nullSubsChar, &len);
    strcpy(outPtr, retabbedStr);
    NEditFree(retabbedStr);
    *endOffset = outPtr - outStr;
    *outLen = (outPtr - outStr) + len;
}

/*
** Overlay characters from single-line string "insLine" on single-line string
** "line" between displayed character offsets "rectStart" and "rectEnd".
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
**
** This code does not handle control characters very well, but oh well.
*/
static void overlayRectInLine(const char *line, const char *insLine,
        int rectStart, int rectEnd, int tabDist, int useTabs,
	char nullSubsChar, char *outStr, int *outLen, int *endOffset)
{
    char *c, *outPtr, *retabbedStr;
    const char *linePtr;
    int inIndent, outIndent, len, postRectIndent;
        
    /* copy the line up to "rectStart" or just before the char that 
        contains it*/ 
    outPtr = outStr;
    inIndent = outIndent = 0;
    for (linePtr=line; *linePtr!='\0'; linePtr++) {
	len = BufCharWidth(*linePtr, inIndent, tabDist, nullSubsChar);
	if (inIndent + len > rectStart)
    	    break;
    	inIndent += len;
    	outIndent += len;
	*outPtr++ = *linePtr;
    }
    
    /* If "rectStart" falls in the middle of a character, and the character
       is a tab, leave it off and leave the outIndent short and it will get
       padded later.  If it's a control character, insert it and adjust
       outIndent accordingly. */
    if (inIndent < rectStart && *linePtr != '\0') {
    	if (*linePtr == '\t') {
            /* Skip past the tab */
    	    linePtr++;
    	    inIndent += len;
    	} else {
    	    *outPtr++ = *linePtr++;
    	    outIndent += len;
    	    inIndent += len;
    	}
    }
    
    /* skip the characters between rectStart and rectEnd */
    for(; *linePtr!='\0' && inIndent < rectEnd; linePtr++)
	inIndent += BufCharWidth(*linePtr, inIndent, tabDist, nullSubsChar);
    postRectIndent = inIndent;
    
    /* After this inIndent is dead and linePtr is supposed to point at the
        character just past the last character that will be altered by
        the overlay, whether that's a \t or otherwise.  postRectIndent is
        the position at which that character is supposed to appear */
    
    /* If there's no text after rectStart and no text to insert, that's all */
    if (*insLine == '\0' && *linePtr == '\0') {
    	*outLen = *endOffset = outPtr - outStr;
    	return;
    }

    /* pad out to rectStart if text is too short */
    if (outIndent < rectStart) {
	addPadding(outPtr, outIndent, rectStart, tabDist, useTabs, nullSubsChar,
		&len);
	outPtr += len;
    }
    outIndent = rectStart;
    
    /* Copy the text from "insLine" (if any), recalculating the tabs as if
       the inserted string began at column 0 to its new column destination */
    if (*insLine != '\0') {
	retabbedStr = realignTabs(insLine, 0, rectStart, tabDist, useTabs,
		nullSubsChar, &len);
	for (c=retabbedStr; *c!='\0'; c++) {
    	    *outPtr++ = *c;
    	    len = BufCharWidth(*c, outIndent, tabDist, nullSubsChar);
    	    outIndent += len;
	}
	NEditFree(retabbedStr);
    }
    
    /* If the original line did not extend past "rectStart", that's all */
    if (*linePtr == '\0') {
    	*outLen = *endOffset = outPtr - outStr;
    	return;
    }
    
    /* Pad out to rectEnd + (additional original offset
       due to non-breaking character at right boundary) */
    addPadding(outPtr, outIndent, postRectIndent, tabDist, useTabs,
	    nullSubsChar, &len);
    outPtr += len;
    outIndent = postRectIndent;
    
    /* copy the text beyond "rectEnd" */
    strcpy(outPtr, linePtr);
    *endOffset = outPtr - outStr;
    *outLen = (outPtr - outStr) + strlen(linePtr);
}

static void setSelection(selection *sel, int start, int end)
{
    sel->selected = start != end;
    sel->zeroWidth = (start == end) ? 1 : 0;
    sel->rectangular = False;
    sel->start = min(start, end);
    sel->end = max(start, end);
}

static void setRectSelect(selection *sel, int start, int end,
	int rectStart, int rectEnd)
{
    sel->selected = rectStart < rectEnd;
    sel->zeroWidth = (rectStart == rectEnd) ? 1 : 0;
    sel->rectangular = True;
    sel->start = start;
    sel->end = end;
    sel->rectStart = rectStart;
    sel->rectEnd = rectEnd;
}

static int getSelectionPos(selection *sel, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd)
{
    /* Always fill in the parameters (zero-width can be requested too). */
    *isRect = sel->rectangular;
    *start = sel->start;
    *end = sel->end;
    if (sel->rectangular) {
	*rectStart = sel->rectStart;
	*rectEnd = sel->rectEnd;
    }
    return sel->selected;
}

static char *getSelectionText(textBuffer *buf, selection *sel)
{
    int start, end, isRect, rectStart, rectEnd;
    char *text;
    
    /* If there's no selection, return an allocated empty string */
    if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd)) {
    	text = (char*)NEditMalloc(1);
    	*text = '\0';
    	return text;
    }
    
    /* If the selection is not rectangular, return the selected range */
    if (isRect)
    	return BufGetTextInRect(buf, start, end, rectStart, rectEnd);
    else
    	return BufGetRange(buf, start, end);
}

static void removeSelected(textBuffer *buf, selection *sel)
{
    int start, end;
    int isRect, rectStart, rectEnd;
    
    if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd))
    	return;
    if (isRect)
        BufRemoveRect(buf, start, end, rectStart, rectEnd);
    else
        BufRemove(buf, start, end);
}

static void replaceSelected(textBuffer *buf, selection *sel, const char *text)
{
    int start, end, isRect, rectStart, rectEnd;
    selection oldSelection = *sel;
    
    /* If there's no selection, return */
    if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd))
    	return;
    
    /* Do the appropriate type of replace */
    if (isRect)
    	BufReplaceRect(buf, start, end, rectStart, rectEnd, text);
    else
    	BufReplace(buf, start, end, text);
    
    /* Unselect (happens automatically in BufReplace, but BufReplaceRect
       can't detect when the contents of a selection goes away) */
    sel->selected = False;
    redisplaySelection(buf, &oldSelection, sel);
}

static void addPadding(char *string, int startIndent, int toIndent,
	int tabDist, int useTabs, char nullSubsChar, int *charsAdded)
{
    char *outPtr;
    int len, indent;
    
    indent = startIndent;
    outPtr = string;
    if (useTabs) {
	while (indent < toIndent) {
	    len = BufCharWidth('\t', indent, tabDist, nullSubsChar);
	    if (len > 1 && indent + len <= toIndent) {
		*outPtr++ = '\t';
		indent += len;
	    } else {
		*outPtr++ = ' ';
		indent++;
	    }
	}
    } else {
    	while (indent < toIndent) {
	    *outPtr++ = ' ';
	    indent++;
	}
    }
    *charsAdded = outPtr - string;
}

/*
** Call the stored modify callback procedure(s) for this buffer to update the
** changed area(s) on the screen and any other listeners.
*/
static void callModifyCBs(textBuffer *buf, int pos, int nDeleted,
	int nInserted, int nRestyled, const char *deletedText)
{
    int i;
    
    for (i=0; i<buf->nModifyProcs; i++)
    	(*buf->modifyProcs[i])(pos, nInserted, nDeleted, nRestyled,
    		deletedText, buf->cbArgs[i]);
}

/*
** Call the stored pre-delete callback procedure(s) for this buffer to update 
** the changed area(s) on the screen and any other listeners.
*/
static void callPreDeleteCBs(textBuffer *buf, int pos, int nDeleted)
{
    int i;
    
    for (i=0; i<buf->nPreDeleteProcs; i++)
    	(*buf->preDeleteProcs[i])(pos, nDeleted, buf->preDeleteCbArgs[i]);
}

/*
** Call the stored redisplay procedure(s) for this buffer to update the
** screen for a change in a selection.
*/
static void redisplaySelection(textBuffer *buf, selection *oldSelection,
	selection *newSelection)
{
    int oldStart, oldEnd, newStart, newEnd, ch1Start, ch1End, ch2Start, ch2End;
    
    /* If either selection is rectangular, add an additional character to
       the end of the selection to request the redraw routines to wipe out
       the parts of the selection beyond the end of the line */
    oldStart = oldSelection->start;
    newStart = newSelection->start;
    oldEnd = oldSelection->end;
    newEnd = newSelection->end;
    if (oldSelection->rectangular)
    	oldEnd++;
    if (newSelection->rectangular)
    	newEnd++;

    /* If the old or new selection is unselected, just redisplay the
       single area that is (was) selected and return */
    if (!oldSelection->selected && !newSelection->selected)
    	return;
    if (!oldSelection->selected) {
    	callModifyCBs(buf, newStart, 0, 0, newEnd-newStart, NULL);
    	return;
    }
    if (!newSelection->selected) {
    	callModifyCBs(buf, oldStart, 0, 0, oldEnd-oldStart, NULL);
    	return;
    }

    /* If the selection changed from normal to rectangular or visa versa, or
       if a rectangular selection changed boundaries, redisplay everything */
    if ((oldSelection->rectangular && !newSelection->rectangular) ||
    	    (!oldSelection->rectangular && newSelection->rectangular) ||
    	    (oldSelection->rectangular && (
    	    	(oldSelection->rectStart != newSelection->rectStart) ||
    	    	(oldSelection->rectEnd != newSelection->rectEnd)))) {
    	callModifyCBs(buf, min(oldStart, newStart), 0, 0,
    		max(oldEnd, newEnd) - min(oldStart, newStart), NULL);
    	return;
    }
    
    /* If the selections are non-contiguous, do two separate updates
       and return */
    if (oldEnd < newStart || newEnd < oldStart) {
	callModifyCBs(buf, oldStart, 0, 0, oldEnd-oldStart, NULL);
	callModifyCBs(buf, newStart, 0, 0, newEnd-newStart, NULL);
	return;
    }
    
    /* Otherwise, separate into 3 separate regions: ch1, and ch2 (the two
       changed areas), and the unchanged area of their intersection,
       and update only the changed area(s) */
    ch1Start = min(oldStart, newStart);
    ch2End = max(oldEnd, newEnd);
    ch1End = max(oldStart, newStart);
    ch2Start = min(oldEnd, newEnd);
    if (ch1Start != ch1End)
    	callModifyCBs(buf, ch1Start, 0, 0, ch1End-ch1Start, NULL);
    if (ch2Start != ch2End)
    	callModifyCBs(buf, ch2Start, 0, 0, ch2End-ch2Start, NULL);
}

static void moveGap(textBuffer *buf, int pos)
{
    int gapLen = buf->gapEnd - buf->gapStart;
    
    if (pos > buf->gapStart)
    	memmove(&buf->buf[buf->gapStart], &buf->buf[buf->gapEnd],
		pos - buf->gapStart);
    else
    	memmove(&buf->buf[pos + gapLen], &buf->buf[pos], buf->gapStart - pos);
    buf->gapEnd += pos - buf->gapStart;
    buf->gapStart += pos - buf->gapStart;
}

/*
** reallocate the text storage in "buf" to have a gap starting at "newGapStart"
** and a gap size of "newGapLen", preserving the buffer's current contents.
*/
static void reallocateBuf(textBuffer *buf, int newGapStart, int newGapLen)
{
    char *newBuf;
    int newGapEnd;

    newBuf = (char*)NEditMalloc(buf->length + newGapLen + 1);
    newBuf[buf->length + PREFERRED_GAP_SIZE] = '\0';
    newGapEnd = newGapStart + newGapLen;
    if (newGapStart <= buf->gapStart) {
	memcpy(newBuf, buf->buf, newGapStart);
	memcpy(&newBuf[newGapEnd], &buf->buf[newGapStart],
		buf->gapStart - newGapStart);
	memcpy(&newBuf[newGapEnd + buf->gapStart - newGapStart],
		&buf->buf[buf->gapEnd], buf->length - buf->gapStart);
    } else { /* newGapStart > buf->gapStart */
	memcpy(newBuf, buf->buf, buf->gapStart);
	memcpy(&newBuf[buf->gapStart], &buf->buf[buf->gapEnd],
		newGapStart - buf->gapStart);
	memcpy(&newBuf[newGapEnd],
		&buf->buf[buf->gapEnd + newGapStart - buf->gapStart],
		buf->length - newGapStart);
    }
    NEditFree(buf->buf);
    buf->buf = newBuf;
    buf->gapStart = newGapStart;
    buf->gapEnd = newGapEnd;
#ifdef PURIFY
    {int i; for (i=buf->gapStart; i<buf->gapEnd; i++) buf->buf[i] = '.';}
#endif
}

/*
** Update all of the selections in "buf" for changes in the buffer's text
*/
static void updateSelections(textBuffer *buf, int pos, int nDeleted,
	int nInserted)
{
    updateSelection(&buf->primary, pos, nDeleted, nInserted);
    updateSelection(&buf->secondary, pos, nDeleted, nInserted);
    updateSelection(&buf->highlight, pos, nDeleted, nInserted);
}

/*
** Update an individual selection for changes in the corresponding text
*/
static void updateSelection(selection *sel, int pos, int nDeleted,
	int nInserted)
{
    if ((!sel->selected && !sel->zeroWidth) || pos > sel->end)
    	return;
    if (pos+nDeleted <= sel->start) {
    	sel->start += nInserted - nDeleted;
	sel->end += nInserted - nDeleted;
    } else if (pos <= sel->start && pos+nDeleted >= sel->end) {
    	sel->start = pos;
    	sel->end = pos;
    	sel->selected = False;
        sel->zeroWidth = False;
    } else if (pos <= sel->start && pos+nDeleted < sel->end) {
    	sel->start = pos;
    	sel->end = nInserted + sel->end - nDeleted;
    } else if (pos < sel->end) {
    	sel->end += nInserted - nDeleted;
	if (sel->end <= sel->start)
	    sel->selected = False;
    }
}

/*
** Search forwards in buffer "buf" for character "searchChar", starting
** with the character "startPos", and returning the result in "foundPos"
** returns True if found, False if not.  (The difference between this and
** BufSearchForward is that it's optimized for single characters.  The
** overall performance of the text widget is dependent on its ability to
** count lines quickly, hence searching for a single character: newline)
*/
static int searchForward(textBuffer *buf, int startPos, char searchChar,
	int *foundPos)
{
    int pos, gapLen = buf->gapEnd - buf->gapStart;
    
    pos = startPos;
    while (pos < buf->gapStart) {
        if (buf->buf[pos] == searchChar) {
            *foundPos = pos;
            return True;
        }
        pos++;
    }
    while (pos < buf->length) {
    	if (buf->buf[pos + gapLen] == searchChar) {
            *foundPos = pos;
            return True;
        }
        pos++;
    }
    *foundPos = buf->length;
    return False;
}

/*
** Search backwards in buffer "buf" for character "searchChar", starting
** with the character BEFORE "startPos", returning the result in "foundPos"
** returns True if found, False if not.  (The difference between this and
** BufSearchBackward is that it's optimized for single characters.  The
** overall performance of the text widget is dependent on its ability to
** count lines quickly, hence searching for a single character: newline)
*/
static int searchBackward(textBuffer *buf, int startPos, char searchChar,
	int *foundPos)
{
    int pos, gapLen = buf->gapEnd - buf->gapStart;
    
    if (startPos == 0) {
    	*foundPos = 0;
    	return False;
    }
    pos = startPos == 0 ? 0 : startPos - 1;
    while (pos >= buf->gapStart) {
    	if (buf->buf[pos + gapLen] == searchChar) {
            *foundPos = pos;
            return True;
        }
        pos--;
    }
    while (pos >= 0) {
        if (buf->buf[pos] == searchChar) {
            *foundPos = pos;
            return True;
        }
        pos--;
    }
    *foundPos = 0;
    return False;
}

/*
** Copy from "text" to end up to but not including newline (or end of "text")
** and return the copy as the function value, and the length of the line in
** "lineLen"
*/
static char *copyLine(const char *text, int *lineLen)
{
    int len = 0;
    const char *c;
    char *outStr;
    
    for (c=text; *c!='\0' && *c!='\n'; c++)
    	len++;
    outStr = (char*)NEditMalloc(len + 1);
    strncpy(outStr, text, len);
    outStr[len] = '\0';
    *lineLen = len;
    return outStr;
}

/*
** Count the number of newlines in a null-terminated text string;
*/
static int countLines(const char *string)
{
    const char *c;
    int lineCount = 0;
    
    for (c=string; *c!='\0'; c++)
    	if (*c == '\n') lineCount++;
    return lineCount;
}

/*
** Measure the width in displayed characters of string "text"
*/
static int textWidth(const char *text, int tabDist, char nullSubsChar)
{
    int width = 0, maxWidth = 0;
    const char *c;
    
    for (c=text; *c!='\0'; c++) {
    	if (*c == '\n') {
    	    if (width > maxWidth)
    	    	maxWidth = width;
    	    width = 0;
    	} else
    	    width += BufCharWidth(*c, width, tabDist, nullSubsChar);
    }
    if (width > maxWidth)
    	return width;
    return maxWidth;
}

/*
** Find the first and last character position in a line withing a rectangular
** selection (for copying).  Includes tabs which cross rectStart, but not
** control characters which do so.  Leaves off tabs which cross rectEnd.
**
** Technically, the calling routine should convert tab characters which
** cross the right boundary of the selection to spaces which line up with
** the edge of the selection.  Unfortunately, the additional memory
** management required in the parent routine to allow for the changes
** in string size is not worth all the extra work just for a couple of
** shifted characters, so if a tab protrudes, just lop it off and hope
** that there are other characters in the selection to establish the right
** margin for subsequent columnar pastes of this data.
*/
static void findRectSelBoundariesForCopy(textBuffer *buf, int lineStartPos,
	int rectStart, int rectEnd, int *selStart, int *selEnd)
{
    int pos, width, indent = 0;
    char c;
    
    /* find the start of the selection */
    for (pos=lineStartPos; pos<buf->length; pos++) {
    	c = BufGetCharacter(buf, pos);
    	if (c == '\n')
    	    break;
    	width = BufCharWidth(c, indent, buf->tabDist, buf->nullSubsChar);
    	if (indent + width > rectStart) {
    	    if (indent != rectStart && c != '\t') {
    	    	pos++;
    	    	indent += width;
    	    }
    	    break;
    	}
    	indent += width;
    }
    *selStart = pos;
    
    /* find the end */
    for (; pos<buf->length; pos++) {
    	c = BufGetCharacter(buf, pos);
    	if (c == '\n')
    	    break;
    	width = BufCharWidth(c, indent, buf->tabDist, buf->nullSubsChar);
    	indent += width;
    	if (indent > rectEnd) {
    	    if (indent-width != rectEnd && c != '\t')
    	    	pos++;
    	    break;
    	}
    }
    *selEnd = pos;
}

/*
** Adjust the space and tab characters from string "text" so that non-white
** characters remain stationary when the text is shifted from starting at
** "origIndent" to starting at "newIndent".  Returns an allocated string
** which must be freed by the caller with NEditFree.
*/
static char *realignTabs(const char *text, int origIndent, int newIndent,
	int tabDist, int useTabs, char nullSubsChar, int *newLength)
{
    char *expStr, *outStr;
    int len;
    
    /* If the tabs settings are the same, retain original tabs */
    if (origIndent % tabDist == newIndent %tabDist) {
    	len = strlen(text);
    	outStr = (char*)NEditMalloc(len + 1);
    	strcpy(outStr, text);
    	*newLength = len;
    	return outStr;
    }
    
    /* If the tab settings are not the same, brutally convert tabs to
       spaces, then back to tabs in the new position */
    expStr = expandTabs(text, origIndent, tabDist, nullSubsChar, &len);
    if (!useTabs) {
    	*newLength = len;
    	return expStr;
    }
    outStr = unexpandTabs(expStr, newIndent, tabDist, nullSubsChar, newLength);
    NEditFree(expStr);
    return outStr;
}    

/*
** Expand tabs to spaces for a block of text.  The additional parameter
** "startIndent" if nonzero, indicates that the text is a rectangular selection
** beginning at column "startIndent"
*/
static char *expandTabs(const char *text, int startIndent, int tabDist,
	char nullSubsChar, int *newLen)
{
    char *outStr, *outPtr;
    const char *c;
    int indent, len, outLen = 0;

    /* rehearse the expansion to figure out length for output string */
    indent = startIndent;
    for (c=text; *c!='\0'; c++) {
    	if (*c == '\t') {
    	    len = BufCharWidth(*c, indent, tabDist, nullSubsChar);
    	    outLen += len;
    	    indent += len;
    	} else if (*c == '\n') {
    	    indent = startIndent;
    	    outLen++;
    	} else {
    	    indent += BufCharWidth(*c, indent, tabDist, nullSubsChar);
    	    outLen++;
    	}
    }
    
    /* do the expansion */
    outStr = (char*)NEditMalloc(outLen+1);
    outPtr = outStr;
    indent = startIndent;
    for (c=text; *c!= '\0'; c++) {
    	if (*c == '\t') {
    	    len = BufExpandCharacter(*c, indent, outPtr, tabDist, nullSubsChar);
    	    outPtr += len;
    	    indent += len;
    	} else if (*c == '\n') {
    	    indent = startIndent;
    	    *outPtr++ = *c;
    	} else {
    	    indent += BufCharWidth(*c, indent, tabDist, nullSubsChar);
    	    *outPtr++ = *c;
    	}
    }
    outStr[outLen] = '\0';
    *newLen = outLen;
    return outStr;
}

/*
** Convert sequences of spaces into tabs.  The threshold for conversion is
** when 3 or more spaces can be converted into a single tab, this avoids
** converting double spaces after a period withing a block of text.
*/
static char *unexpandTabs(const char *text, int startIndent, int tabDist,
	char nullSubsChar, int *newLen)
{
    char *outStr, *outPtr, expandedChar[MAX_EXP_CHAR_LEN];
    const char *c;
    int indent, len;
    
    outStr = (char*)NEditMalloc(strlen(text)+1);
    outPtr = outStr;
    indent = startIndent;
    for (c=text; *c!='\0';) {
    	if (*c == ' ') {
    	    len = BufExpandCharacter('\t', indent, expandedChar, tabDist,
		    nullSubsChar);
    	    if (len >= 3 && !strncmp(c, expandedChar, len)) {
    	    	c += len;
    	    	*outPtr++ = '\t';
    	    	indent += len;
    	    } else {
    	    	*outPtr++ = *c++;
    	    	indent++;
    	    }
    	} else if (*c == '\n') {
    	    indent = startIndent;
    	    *outPtr++ = *c++;
    	} else {
    	    *outPtr++ = *c++;
    	    indent++;
    	}
    }
    *outPtr = '\0';
    *newLen = outPtr - outStr;
    return outStr;
}

static int max(int i1, int i2)
{
    return i1 >= i2 ? i1 : i2;
}

static int min(int i1, int i2)
{
    return i1 <= i2 ? i1 : i2;
}
