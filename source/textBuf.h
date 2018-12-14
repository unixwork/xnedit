/*******************************************************************************
*                                                                              *
* textBuf.h -- Nirvana Editor Text Buffer Header File                          *
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

#ifndef NEDIT_TEXTBUF_H_INCLUDED
#define NEDIT_TEXTBUF_H_INCLUDED

/* Maximum length in characters of a tab or control character expansion
   of a single buffer character */
#define MAX_EXP_CHAR_LEN 20

typedef struct _RangesetTable RangesetTable;

typedef struct {
    char selected;          /* True if the selection is active */
    char rectangular;       /* True if the selection is rectangular */
    char zeroWidth;         /* Width 0 selections aren't "real" selections, but
                                they can be useful when creating rectangular
                                selections from the keyboard. */
    int start;              /* Pos. of start of selection, or if rectangular
                                 start of line containing it. */
    int end;                /* Pos. of end of selection, or if rectangular
                                 end of line containing it. */
    int rectStart;          /* Indent of left edge of rect. selection */
    int rectEnd;            /* Indent of right edge of rect. selection */
} selection;

typedef void (*bufModifyCallbackProc)(int pos, int nInserted, int nDeleted,
	int nRestyled, const char *deletedText, void *cbArg);
typedef void (*bufPreDeleteCallbackProc)(int pos, int nDeleted, void *cbArg);

typedef struct _textBuffer {
    int length; 	        /* length of the text in the buffer (the length
                                   of the buffer itself must be calculated:
                                   gapEnd - gapStart + length) */
    char *buf;                  /* allocated memory where the text is stored */
    int gapStart;  	        /* points to the first character of the gap */
    int gapEnd;                 /* points to the first char after the gap */
    selection primary;		/* highlighted areas */
    selection secondary;
    selection highlight;
    int tabDist;		/* equiv. number of characters in a tab */
    int useTabs;		/* True if buffer routines are allowed to use
    				   tabs for padding in rectangular operations */
    int nModifyProcs;		/* number of modify-redisplay procs attached */
    bufModifyCallbackProc	/* procedures to call when buffer is */
    	    *modifyProcs;	/*    modified to redisplay contents */
    void **cbArgs;		/* caller arguments for modifyProcs above */
    int nPreDeleteProcs;	/* number of pre-delete procs attached */
    bufPreDeleteCallbackProc	/* procedure to call before text is deleted */
	 *preDeleteProcs;	/* from the buffer; at most one is supported. */
    void **preDeleteCbArgs;	/* caller argument for pre-delete proc above */
    int cursorPosHint;		/* hint for reasonable cursor position after
    				   a buffer modification operation */
    char nullSubsChar;	    	/* NEdit is based on C null-terminated strings,
    	    	    	    	   so ascii-nul characters must be substituted
				   with something else.  This is the else, but
				   of course, things get quite messy when you
				   use it */
    RangesetTable *rangesetTable;
				/* current range sets */
} textBuffer;

textBuffer *BufCreate(void);
textBuffer *BufCreatePreallocated(int requestedSize);
void BufFree(textBuffer *buf);
char *BufGetAll(textBuffer *buf);
const char *BufAsString(textBuffer *buf);
void BufSetAll(textBuffer *buf, const char *text);
char* BufGetRange(const textBuffer* buf, int start, int end);
char BufGetCharacter(const textBuffer* buf, int pos);
char *BufGetTextInRect(textBuffer *buf, int start, int end,
	int rectStart, int rectEnd);
void BufInsert(textBuffer *buf, int pos, const char *text);
void BufRemove(textBuffer *buf, int start, int end);
void BufReplace(textBuffer *buf, int start, int end, const char *text);
void BufCopyFromBuf(textBuffer *fromBuf, textBuffer *toBuf, int fromStart,
    	int fromEnd, int toPos);
void BufInsertCol(textBuffer *buf, int column, int startPos, const char *text,
    	int *charsInserted, int *charsDeleted);
void BufReplaceRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd, const char *text);
void BufRemoveRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd);
void BufOverlayRect(textBuffer *buf, int startPos, int rectStart,
    	int rectEnd, const char *text, int *charsInserted, int *charsDeleted);
void BufClearRect(textBuffer *buf, int start, int end, int rectStart,
	int rectEnd);
int BufGetTabDistance(textBuffer *buf);
void BufSetTabDistance(textBuffer *buf, int tabDist);
void BufCheckDisplay(textBuffer *buf, int start, int end);
void BufSelect(textBuffer *buf, int start, int end);
void BufUnselect(textBuffer *buf);
void BufRectSelect(textBuffer *buf, int start, int end, int rectStart,
        int rectEnd);
int BufGetSelectionPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd);
int BufGetEmptySelectionPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd);
char *BufGetSelectionText(textBuffer *buf);
void BufRemoveSelected(textBuffer *buf);
void BufReplaceSelected(textBuffer *buf, const char *text);
void BufSecondarySelect(textBuffer *buf, int start, int end);
void BufSecondaryUnselect(textBuffer *buf);
void BufSecRectSelect(textBuffer *buf, int start, int end,
        int rectStart, int rectEnd);
int BufGetSecSelectPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd);
char *BufGetSecSelectText(textBuffer *buf);
void BufRemoveSecSelect(textBuffer *buf);
void BufReplaceSecSelect(textBuffer *buf, const char *text);
void BufHighlight(textBuffer *buf, int start, int end);
void BufUnhighlight(textBuffer *buf);
void BufRectHighlight(textBuffer *buf, int start, int end,
        int rectStart, int rectEnd);
int BufGetHighlightPos(textBuffer *buf, int *start, int *end,
        int *isRect, int *rectStart, int *rectEnd);
void BufAddModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB,
	void *cbArg);
void BufAddHighPriorityModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB,
	void *cbArg);
void BufRemoveModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB,
	void *cbArg);
void BufAddPreDeleteCB(textBuffer *buf, bufPreDeleteCallbackProc bufPreDeleteCB,
	void *cbArg);
void BufRemovePreDeleteCB(textBuffer *buf, bufPreDeleteCallbackProc 
	bufPreDeleteCB,	void *cbArg);
int BufStartOfLine(textBuffer *buf, int pos);
int BufEndOfLine(textBuffer *buf, int pos);
int BufGetExpandedChar(const textBuffer* buf, int pos, int indent,
        char* outStr);
int BufExpandCharacter(char c, int indent, char *outStr, int tabDist,
	char nullSubsChar);
int BufCharWidth(char c, int indent, int tabDist, char nullSubsChar);
int BufCountDispChars(const textBuffer* buf, int lineStartPos,
        int targetPos);
int BufCountForwardDispChars(textBuffer *buf, int lineStartPos, int nChars);
int BufCountLines(textBuffer *buf, int startPos, int endPos);
int BufCountForwardNLines(const textBuffer* buf, int startPos,
        unsigned nLines);
int BufCountBackwardNLines(textBuffer *buf, int startPos, int nLines);
int BufSearchForward(textBuffer *buf, int startPos, const char *searchChars,
	int *foundPos);
int BufSearchBackward(textBuffer *buf, int startPos, const char *searchChars,
	int *foundPos);
int BufSubstituteNullChars(char *string, int length, textBuffer *buf);
void BufUnsubstituteNullChars(char *string, textBuffer *buf);
int BufCmp(textBuffer * buf, int pos, int len, const char *cmpText);

#endif /* NEDIT_TEXTBUF_H_INCLUDED */
