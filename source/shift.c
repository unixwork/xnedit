/*******************************************************************************
*									       *
* shift.c -- Nirvana Editor built-in filter commands			       *
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
* June 18, 1991								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "shift.h"
#include "textBuf.h"
#include "text.h"
#include "nedit.h"
#include "window.h"
#include "../util/nedit_malloc.h"

#include <string.h>
#include <limits.h>
#include <ctype.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#include <Xm/Xm.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif


static void shiftRect(WindowInfo *window, int direction, int byTab,
	int selStart, int selEnd, int rectStart, int rectEnd);
static void changeCase(WindowInfo *window, int makeUpper);
static char *shiftLineRight(char *line, int lineLen, int tabsAllowed,
	int tabDist, int nChars);
static char *shiftLineLeft(char *line, int lineLen, int tabDist, int nChars);
static int findLeftMargin(char *text, int length, int tabDist);
static char *fillParagraphs(char *text, int rightMargin, int tabDist,
	int useTabs, char nullSubsChar, int *filledLen, int alignWithFirst);
static char *fillParagraph(char *text, int leftMargin, int firstLineIndent,
	int rightMargin, int tabDist, int allowTabs, char nullSubsChar,
	int *filledLen);
static char *makeIndentString(int indent, int tabDist, int allowTabs, int *nChars);
static int atTabStop(int pos, int tabDist);
static int nextTab(int pos, int tabDist);
static int countLines(const char *text);
static int findParagraphStart(textBuffer *buf, int startPos);
static int findParagraphEnd(textBuffer *buf, int startPos);

/*
** Shift the selection left or right by a single character, or by one tab stop
** if "byTab" is true.  (The length of a tab stop is the size of an emulated
** tab if emulated tabs are turned on, or a hardware tab if not).
*/
void ShiftSelection(WindowInfo *window, int direction, int byTab)
{
    int selStart, selEnd, isRect, rectStart, rectEnd;
    int shiftedLen, newEndPos, cursorPos, origLength, emTabDist, shiftDist;
    char *text, *shiftedText;
    textBuffer *buf = window->buffer;

    /* get selection, if no text selected, use current insert position */
    if (!BufGetSelectionPos(buf, &selStart, &selEnd, &isRect,
    	    &rectStart, &rectEnd)) {
    	cursorPos = TextGetCursorPos(window->lastFocus);
    	selStart = BufStartOfLine(buf, cursorPos);
    	selEnd = BufEndOfLine(buf, cursorPos);
	if (selEnd < buf->length)
	    selEnd++;
	BufSelect(buf, selStart, selEnd);
    	isRect = False;
    	text = BufGetRange(buf, selStart, selEnd);
    } else if (isRect) {
	cursorPos = TextGetCursorPos(window->lastFocus);
	origLength = buf->length;
	shiftRect(window, direction, byTab, selStart, selEnd, rectStart,
		rectEnd);
	TextSetCursorPos(window->lastFocus, (cursorPos < (selEnd+selStart)/2) ?
		selStart : cursorPos + (buf->length - origLength));
	return;
    } else {
	selStart = BufStartOfLine(buf, selStart);
	if (selEnd != 0 && BufGetCharacter(buf, selEnd-1) != '\n') {
	    selEnd = BufEndOfLine(buf, selEnd);
	    if (selEnd < buf->length)
		selEnd++;
	}
	BufSelect(buf, selStart, selEnd);
	text = BufGetRange(buf, selStart, selEnd);
    }
    
    /* shift the text by the appropriate distance */
    if (byTab) {
    	XtVaGetValues(window->textArea, textNemulateTabs, &emTabDist, NULL);
    	shiftDist = emTabDist == 0 ? buf->tabDist : emTabDist;
    } else
    	shiftDist = 1;
    shiftedText = ShiftText(text, direction, buf->useTabs, buf->tabDist,
    	    shiftDist, &shiftedLen);
    NEditFree(text);
    BufReplaceSelected(buf, shiftedText);
    NEditFree(shiftedText);
    
    newEndPos = selStart + shiftedLen;
    BufSelect(buf, selStart, newEndPos);
}

static void shiftRect(WindowInfo *window, int direction, int byTab,
	int selStart, int selEnd, int rectStart, int rectEnd)
{
    int offset, emTabDist;
    textBuffer *tempBuf, *buf = window->buffer;
    char *text;
    
    /* Make sure selStart and SelEnd refer to whole lines */
    selStart = BufStartOfLine(buf, selStart);
    selEnd = BufEndOfLine(buf, selEnd);
    
    /* Calculate the the left/right offset for the new rectangle */
    if (byTab) {
    	XtVaGetValues(window->textArea, textNemulateTabs, &emTabDist, NULL);
    	offset = emTabDist == 0 ? buf->tabDist : emTabDist;
    } else
    	offset = 1;
    offset *= direction == SHIFT_LEFT ? -1 : 1;
    if (rectStart + offset < 0)
	offset = -rectStart;
    
    /* Create a temporary buffer for the lines containing the selection, to
       hide the intermediate steps from the display update routines */
    tempBuf = BufCreate();
    tempBuf->tabDist = buf->tabDist;
    tempBuf->useTabs = buf->useTabs;
    text = BufGetRange(buf, selStart, selEnd);
    BufSetAll(tempBuf, text);
    NEditFree(text);
    
    /* Do the shift in the temporary buffer */
    text = BufGetTextInRect(buf, selStart, selEnd, rectStart, rectEnd);
    BufRemoveRect(tempBuf, 0, selEnd-selStart, rectStart, rectEnd);
    BufInsertCol(tempBuf, rectStart+offset, 0, text, NULL, NULL);
    NEditFree(text);
    
    /* Make the change in the real buffer */
    BufReplace(buf, selStart, selEnd, BufAsString(tempBuf));
    BufRectSelect(buf, selStart, selStart + tempBuf->length,
	    rectStart+offset, rectEnd+offset);
    BufFree(tempBuf);
}

void UpcaseSelection(WindowInfo *window)
{
    changeCase(window, True);
}

void DowncaseSelection(WindowInfo *window)
{
    changeCase(window, False);
}

/*
** Capitalize or lowercase the contents of the selection (or of the character
** before the cursor if there is no selection).  If "makeUpper" is true,
** change to upper case, otherwise, change to lower case.
*/
static void changeCase(WindowInfo *window, int makeUpper)
{
    textBuffer *buf = window->buffer;
    char *text, *c;
    char oldChar;
    int cursorPos, start, end, isRect, rectStart, rectEnd;
    
    /* Get the selection.  Use character before cursor if no selection */
    if (!BufGetSelectionPos(buf, &start, &end, &isRect, &rectStart, &rectEnd)) {
    	char bufChar[2] = " ";
    	cursorPos = TextGetCursorPos(window->lastFocus);
    	if (cursorPos == 0) {
    	    XBell(TheDisplay, 0);
    	    return;
	}
    	*bufChar = BufGetCharacter(buf, cursorPos-1);
    	*bufChar = makeUpper ? toupper((unsigned char)*bufChar) :
		tolower((unsigned char)*bufChar);
    	BufReplace(buf, cursorPos-1, cursorPos, bufChar);
    } else {
        Boolean modified = False;

	text = BufGetSelectionText(buf);
        for (c = text; *c != '\0'; c++) {
            oldChar = *c;
    	    *c = makeUpper ? toupper((unsigned char)*c) :
		    tolower((unsigned char)*c);
            if (*c != oldChar) {
                modified = True;
            }
        }

        if (modified) {
            BufReplaceSelected(buf, text);
        }

	NEditFree(text);
	if (isRect)
	    BufRectSelect(buf, start, end, rectStart, rectEnd);
	else
	    BufSelect(buf, start, end);
    }
}

void FillSelection(WindowInfo *window)
{
    textBuffer *buf = window->buffer;
    char *text, *filledText;
    int left, right, nCols, len, isRect, rectStart, rectEnd;
    int rightMargin, wrapMargin;
    int insertPos = TextGetCursorPos(window->lastFocus);
    int hasSelection = window->buffer->primary.selected;
    
    /* Find the range of characters and get the text to fill.  If there is a
       selection, use it but extend non-rectangular selections to encompass
       whole lines.  If there is no selection, find the paragraph containing
       the insertion cursor */
    if (!BufGetSelectionPos(buf, &left, &right, &isRect, &rectStart, &rectEnd)) {
	left = findParagraphStart(buf, insertPos);
	right = findParagraphEnd(buf, insertPos);
	if (left == right) {
    	    XBell(TheDisplay, 0);
    	    return;
	}
	text = BufGetRange(buf, left, right);
    } else if (isRect) {
    	left = BufStartOfLine(buf, left);
    	right = BufEndOfLine(buf, right);
    	text = BufGetTextInRect(buf, left, right, rectStart, INT_MAX);
    } else {
	left = BufStartOfLine(buf, left);
	if (right != 0 && BufGetCharacter(buf, right-1) != '\n') {
	    right = BufEndOfLine(buf, right);
	    if (right < buf->length)
		right++;
	}
    	BufSelect(buf, left, right);
    	text = BufGetRange(buf, left, right);
    }
    
    /* Find right margin either as specified in the rectangular selection, or
       by measuring the text and querying the window's wrap margin (or width) */
    if (hasSelection && isRect) {
    	rightMargin = rectEnd - rectStart;
    } else
    {
        XtVaGetValues(window->textArea,
                textNcolumns, &nCols,
                textNwrapMargin, &wrapMargin,
                NULL);
        rightMargin = (wrapMargin == 0 ? nCols : wrapMargin);
    }
    
    /* Fill the text */
    filledText = fillParagraphs(text, rightMargin, buf->tabDist, buf->useTabs,
	    buf->nullSubsChar, &len, False);
    NEditFree(text);
        
    /* Replace the text in the window */
    if (hasSelection && isRect) {
        BufReplaceRect(buf, left, right, rectStart, INT_MAX, filledText);
        BufRectSelect(buf, left,
        	BufEndOfLine(buf, BufCountForwardNLines(buf, left,
        	countLines(filledText)-1)), rectStart, rectEnd);
    } else {
	BufReplace(buf, left, right, filledText);
	if (hasSelection)
    	    BufSelect(buf, left, left + len);
    }
    NEditFree(filledText);
    
    /* Find a reasonable cursor position.  Usually insertPos is best, but
       if the text was indented, positions can shift */
    if (hasSelection && isRect)
    	TextSetCursorPos(window->lastFocus, buf->cursorPosHint);
    else
	TextSetCursorPos(window->lastFocus, insertPos < left ? left :
    		(insertPos > left + len ? left + len : insertPos));
}

/*
** shift lines left and right in a multi-line text string.  Returns the
** shifted text in memory that must be freed by the caller with NEditFree.
*/
char *ShiftText(char *text, int direction, int tabsAllowed, int tabDist,
	int nChars, int *newLen)
{
    char *shiftedText, *shiftedLine;
    char *textPtr, *lineStartPtr, *shiftedPtr;
    int bufLen;
    
    /*
    ** Allocate memory for shifted string.  Shift left adds a maximum of
    ** tabDist-2 characters per line (remove one tab, add tabDist-1 spaces).
    ** Shift right adds a maximum of nChars character per line.
    */
    if (direction == SHIFT_RIGHT)
        bufLen = strlen(text) + countLines(text) * nChars;
    else
        bufLen = strlen(text) + countLines(text) * tabDist;
    shiftedText = (char*)NEditMalloc(bufLen + 1);
    
    /*
    ** break into lines and call shiftLine(Left/Right) on each
    */
    lineStartPtr = text;
    textPtr = text;
    shiftedPtr = shiftedText;
    while (TRUE) {
	if (*textPtr=='\n' || *textPtr=='\0') {
	    shiftedLine = (direction == SHIFT_RIGHT) ?
		    shiftLineRight(lineStartPtr, textPtr-lineStartPtr,
		        tabsAllowed, tabDist, nChars) :
	    	    shiftLineLeft(lineStartPtr, textPtr-lineStartPtr, tabDist,
			nChars);
	    strcpy(shiftedPtr, shiftedLine);
	    shiftedPtr += strlen(shiftedLine);
	    NEditFree(shiftedLine);
	    if (*textPtr == '\0') {
	        /* terminate string & exit loop at end of text */
	    	*shiftedPtr = '\0';
		break;
	    } else {
	    	/* move the newline from text to shifted text */
		*shiftedPtr++ = *textPtr++;
	    }
	    /* start line over */
	    lineStartPtr = textPtr;
	} else
	    textPtr++;
    }
    *newLen = shiftedPtr - shiftedText;
    return shiftedText;
}

static char *shiftLineRight(char *line, int lineLen, int tabsAllowed,
	int tabDist, int nChars)
{
    char *lineOut;
    char *lineInPtr, *lineOutPtr;
    int whiteWidth, i;
    
    lineInPtr = line;
    lineOut = (char*)NEditMalloc(lineLen + nChars + 1);
    lineOutPtr = lineOut;
    whiteWidth = 0;
    while (TRUE) {
        if (*lineInPtr == '\0' || (lineInPtr - line) >= lineLen) {
	    /* nothing on line, wipe it out */
	    *lineOut = '\0';
	    return lineOut;
        } else if (*lineInPtr == ' ') {
	    /* white space continues with tab, advance to next tab stop */
	    whiteWidth++;
	    *lineOutPtr++ = *lineInPtr++;
	} else if (*lineInPtr == '\t') {
	    /* white space continues with tab, advance to next tab stop */
	    whiteWidth = nextTab(whiteWidth, tabDist);
	    *lineOutPtr++ = *lineInPtr++;
	} else {
	    /* end of white space, add nChars of space */
	    for (i=0; i<nChars; i++) {
		*lineOutPtr++ = ' ';
		whiteWidth++;
		/* if we're now at a tab stop, change last 8 spaces to a tab */
		if (tabsAllowed && atTabStop(whiteWidth, tabDist)) {
		    lineOutPtr -= tabDist;
		    *lineOutPtr++ = '\t';
		}
	    }
	    /* move remainder of line */
    	    while (*lineInPtr!='\0' && (lineInPtr - line) < lineLen)
		*lineOutPtr++ = *lineInPtr++;
	    *lineOutPtr = '\0';
	    return lineOut;
	}
    }
}

static char *shiftLineLeft(char *line, int lineLen, int tabDist, int nChars)
{
    char *lineOut;
    int i, whiteWidth, lastWhiteWidth, whiteGoal;
    char *lineInPtr, *lineOutPtr;
    
    lineInPtr = line;
    lineOut = (char*)NEditMalloc(lineLen + tabDist + 1);
    lineOutPtr = lineOut;
    whiteWidth = 0;
    lastWhiteWidth = 0;
    while (TRUE) {
        if (*lineInPtr == '\0' || (lineInPtr - line) >= lineLen) {
	    /* nothing on line, wipe it out */
	    *lineOut = '\0';
	    return lineOut;
        } else if (*lineInPtr == ' ') {
	    /* white space continues with space, advance one character */
	    whiteWidth++;
	    *lineOutPtr++ = *lineInPtr++;
	} else if (*lineInPtr == '\t') {
	    /* white space continues with tab, advance to next tab stop	    */
	    /* save the position, though, in case we need to remove the tab */
	    lastWhiteWidth = whiteWidth;
	    whiteWidth = nextTab(whiteWidth, tabDist);
	    *lineOutPtr++ = *lineInPtr++;
	} else {
	    /* end of white space, remove nChars characters */
	    for (i=1; i<=nChars; i++) {
		if (lineOutPtr > lineOut) {
		    if (*(lineOutPtr-1) == ' ') {
			/* end of white space is a space, just remove it */
			lineOutPtr--;
		    } else {
	    		/* end of white space is a tab, remove it and add
	    		   back spaces */
			lineOutPtr--;
			whiteGoal = whiteWidth - i;
			whiteWidth = lastWhiteWidth;
			while (whiteWidth < whiteGoal) {
			    *lineOutPtr++ = ' ';
			    whiteWidth++;
			}
		    }
		}
	    }
	    /* move remainder of line */
    	    while (*lineInPtr!='\0' && (lineInPtr - line) < lineLen)
		*lineOutPtr++ = *lineInPtr++;
	    /* add a null */
	    *lineOutPtr = '\0';
	    return lineOut;
	}
    }
}
       
static int atTabStop(int pos, int tabDist)
{
    return (pos%tabDist == 0);
}

static int nextTab(int pos, int tabDist)
{
    return (pos/tabDist)*tabDist + tabDist;
}

static int countLines(const char *text)
{
    int count = 1;
    
    while(*text != '\0') {
    	if (*text++ == '\n') {
	    count++;
	}
    }
    return count;
}

/*
** Find the implied left margin of a text string (the number of columns to the
** first non-whitespace character on any line) up to either the terminating
** null character at the end of the string, or "length" characters, whever
** comes first.
*/
static int findLeftMargin(char *text, int length, int tabDist)
{
    char *c;
    int col = 0, leftMargin = INT_MAX;
    int inMargin = True;
    
    for (c=text; *c!='\0' && c-text<length; c++) {
    	if (*c == '\t') {
    	    col += BufCharWidth('\t', col, tabDist, '\0');
    	} else if (*c == ' ') {
    	    col++;
    	} else if (*c == '\n') {
	    col = 0;
    	    inMargin = True;
    	} else {
    	    /* non-whitespace */
    	    if (col < leftMargin && inMargin)
    	    	leftMargin = col;
    	    inMargin = False;
    	}
    }
    
    /* if no non-white text is found, the leftMargin will never be set */
    if (leftMargin == INT_MAX)
    	return 0;
    
    return leftMargin;
}

/*
** Fill multiple paragraphs between rightMargin and an implied left margin
** and first line indent determined by analyzing the text.  alignWithFirst
** aligns subsequent paragraphs with the margins of the first paragraph (a
** capability not currently used in NEdit, but carried over from code for
** previous versions which did all paragraphs together).
*/
static char *fillParagraphs(char *text, int rightMargin, int tabDist,
	int useTabs, char nullSubsChar, int *filledLen, int alignWithFirst)
{
    int paraStart, paraEnd, fillEnd;
    char *c, ch, *secondLineStart, *paraText, *filledText;
    int firstLineLen, firstLineIndent, leftMargin, len;
    textBuffer *buf;
    
    /* Create a buffer to accumulate the filled paragraphs */
    buf = BufCreate();
    BufSetAll(buf, text);
    
    /*
    ** Loop over paragraphs, filling each one, and accumulating the results
    ** in buf
    */
    paraStart = 0;
    for (;;) {
	
	/* Skip over white space */
	while (paraStart < buf->length) {
	    ch = BufGetCharacter(buf, paraStart);
	    if (ch != ' ' && ch != '\t' && ch != '\n')
	    	break;
	    paraStart++;
	}
	if (paraStart >= buf->length)
	    break;
	paraStart = BufStartOfLine(buf, paraStart);
	
	/* Find the end of the paragraph */
	paraEnd = findParagraphEnd(buf, paraStart);
	
	/* Operate on either the one paragraph, or to make them all identical,
	   do all of them together (fill paragraph can format all the paragraphs
	   it finds with identical specs if it gets passed more than one) */
	fillEnd = alignWithFirst ? buf->length :  paraEnd;

	/* Get the paragraph in a text string (or all of the paragraphs if
	   we're making them all the same) */
	paraText = BufGetRange(buf, paraStart, fillEnd);
	
	/* Find separate left margins for the first and for the first line of
	   the paragraph, and for rest of the remainder of the paragraph */
	for (c=paraText ; *c!='\0' && *c!='\n'; c++);
	firstLineLen = c - paraText;
	secondLineStart = *c == '\0' ? paraText : c + 1;
	firstLineIndent = findLeftMargin(paraText, firstLineLen, tabDist);
	leftMargin = findLeftMargin(secondLineStart, paraEnd - paraStart -
		(secondLineStart - paraText), tabDist);

	/* Fill the paragraph */
	filledText = fillParagraph(paraText, leftMargin, firstLineIndent,
		rightMargin, tabDist, useTabs, nullSubsChar, &len);
	NEditFree(paraText);
	
	/* Replace it in the buffer */
	BufReplace(buf, paraStart, fillEnd, filledText);
	NEditFree(filledText);
	
	/* move on to the next paragraph */
	paraStart += len;
    }
    
    /* Free the buffer and return its contents */
    filledText = BufGetAll(buf);
    *filledLen = buf->length;
    BufFree(buf);
    return filledText;
}

/*
** Trim leading space, and arrange text to fill between leftMargin and
** rightMargin (except for the first line which fills from firstLineIndent),
** re-creating whitespace to the left of the text using tabs (if allowTabs is
** True) calculated using tabDist, and spaces.  Returns a newly allocated
** string as the function result, and the length of the new string in filledLen.
*/
static char *fillParagraph(char *text, int leftMargin, int firstLineIndent,
	int rightMargin, int tabDist, int allowTabs, char nullSubsChar,
	int *filledLen)
{
    char *cleanedText, *outText, *indentString, *leadIndentStr, *outPtr, *c, *b;
    int col, cleanedLen, indentLen, leadIndentLen, nLines = 1;
    int inWhitespace, inMargin;
    
    /* remove leading spaces, convert newlines to spaces */
    cleanedText = (char*)NEditMalloc(strlen(text)+1);
    outPtr = cleanedText;
    inMargin = True;
    for (c=text; *c!='\0'; c++) {
    	if (*c == '\t' || *c == ' ') {
    	    if (!inMargin)
    	    	*outPtr++ = *c;
    	} else if (*c == '\n') {
    	    if (inMargin) {
    	    	/* a newline before any text separates paragraphs, so leave
    	    	   it in, back up, and convert the previous space back to \n */
    	    	if (outPtr > cleanedText && *(outPtr-1) == ' ')
    	    	    *(outPtr-1) = '\n';
    	    	*outPtr++ = '\n';
    	    	nLines +=2;
    	    } else
    	    	*outPtr++ = ' ';
    	    inMargin = True;
    	} else {
    	    *outPtr++ = *c;
    	    inMargin = False;
    	}
    }
    cleanedLen = outPtr - cleanedText;
    *outPtr = '\0';
    
    /* Put back newlines breaking text at word boundaries within the margins.
       Algorithm: scan through characters, counting columns, and when the
       margin width is exceeded, search backward for beginning of the word
       and convert the last whitespace character into a newline */
    col = firstLineIndent;
    for (c=cleanedText; *c!='\0'; c++) {
    	if (*c == '\n')
    	    col = leftMargin;
    	else
    	    col += BufCharWidth(*c, col, tabDist, nullSubsChar);
    	if (col-1 > rightMargin) {
    	    inWhitespace = True;
    	    for (b=c; b>=cleanedText && *b!='\n'; b--) {
    	    	if (*b == '\t' || *b == ' ') {
    	    	    if (!inWhitespace) {
    	    		*b = '\n';
    	    		c = b;
    	    		col = leftMargin;
     	    		nLines++;
   	    		break;
    	    	    }
    	    	} else 
    	    	    inWhitespace = False;
    	    }
    	}
    }
    nLines++;

    /* produce a string to prepend to lines to indent them to the left margin */
    leadIndentStr = makeIndentString(firstLineIndent, tabDist,
	    allowTabs, &leadIndentLen);
    indentString = makeIndentString(leftMargin, tabDist, allowTabs, &indentLen);
        
    /* allocate memory for the finished string */
    outText = (char*)NEditMalloc(sizeof(char) * (cleanedLen + leadIndentLen +
	    indentLen * (nLines-1) + 1));
    outPtr = outText;
    
    /* prepend the indent string to each line of the filled text */
    strncpy(outPtr, leadIndentStr, leadIndentLen);
    outPtr += leadIndentLen;
    for (c=cleanedText; *c!='\0'; c++) {
    	*outPtr++ = *c;
    	if (*c == '\n') {
    	    strncpy(outPtr, indentString, indentLen);
    	    outPtr += indentLen;
    	}
    }
    
    /* convert any trailing space to newline.  Add terminating null */
    if (*(outPtr-1) == ' ')
    	*(outPtr-1) = '\n';
    *outPtr = '\0';
    
    /* clean up, return result */
    NEditFree(cleanedText);
    NEditFree(leadIndentStr);
    NEditFree(indentString);
    *filledLen = outPtr - outText;
    return outText;
}

static char *makeIndentString(int indent, int tabDist, int allowTabs, int *nChars)
{
    char *indentString, *outPtr;
    int i;
    
    outPtr = indentString = (char*)NEditMalloc(sizeof(char) * indent + 1);
    if (allowTabs) {
	for (i=0; i<indent/tabDist; i++)
    	    *outPtr++ = '\t';
	for (i=0; i<indent%tabDist; i++)
    	    *outPtr++ = ' ';
    } else {
    	for (i=0; i<indent; i++)
    	    *outPtr++ = ' ';
    }
    *outPtr = '\0';
    *nChars = outPtr - indentString;
    return indentString;
}

/*
** Find the boundaries of the paragraph containing pos
*/
static int findParagraphEnd(textBuffer *buf, int startPos)
{
    char c;
    int pos;
    static char whiteChars[] = " \t";

    pos = BufEndOfLine(buf, startPos)+1;
    while (pos < buf->length) {
    	c = BufGetCharacter(buf, pos);
    	if (c == '\n')
    	    break;
    	if (strchr(whiteChars, c) != NULL)
    	    pos++;
    	else
    	    pos = BufEndOfLine(buf, pos)+1;
    }
    return pos < buf->length ? pos : buf->length;
}
static int findParagraphStart(textBuffer *buf, int startPos)
{
    char c;
    int pos, parStart;
    static char whiteChars[] = " \t";

    if (startPos == 0)
    	return 0;
    parStart = BufStartOfLine(buf, startPos);
    pos = parStart - 2;
    while (pos > 0) {
    	c = BufGetCharacter(buf, pos);
    	if (c == '\n')
    	    break;
    	if (strchr(whiteChars, c) != NULL)
    	    pos--;
    	else {
    	    parStart = BufStartOfLine(buf, pos);
    	    pos = parStart - 2;
    	}
    }
    return parStart > 0 ? parStart : 0;
}
