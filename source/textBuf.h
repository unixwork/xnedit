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

#include <fontconfig/fontconfig.h>
#include <wchar.h>

/* Maximum length in characters of a tab or control character expansion
   of a single buffer character */
#define MAX_EXP_CHAR_LEN 120

typedef struct _RangesetTable RangesetTable;

typedef struct {
    char selected;          /* True if the selection is active */
    char rectangular;       /* True if the selection is rectangular */
    char zeroWidth;         /* Width 0 selections aren't "real" selections, but
                                they can be useful when creating rectangular
                                selections from the keyboard. */
    ssize_t start;              /* Pos. of start of selection, or if rectangular
                                 start of line containing it. */
    ssize_t end;                /* Pos. of end of selection, or if rectangular
                                 end of line containing it. */
    int rectStart;          /* Indent of left edge of rect. selection */
    int rectEnd;            /* Indent of right edge of rect. selection */
} selection;

typedef void (*bufModifyCallbackProc)(ssize_t pos, ssize_t nInserted, ssize_t nDeleted,
    ssize_t nRestyled, const char *deletedText, void *cbArg);
typedef void (*bufPreDeleteCallbackProc)(ssize_t pos, ssize_t nDeleted, void *cbArg);
typedef void (*bufBeginModifyCallbackProc)(void *cbArg);
typedef void (*bufEndModifyCallbackProc)(void *cbArg);

typedef struct _textBuffer {
    ssize_t length; 	        /* length of the text in the buffer (the length
                                   of the buffer itself must be calculated:
                                   gapEnd - gapStart + length) */
    char *buf;                  /* allocated memory where the text is stored */
    ssize_t gapStart;  	        /* points to the first character of the gap */
    ssize_t gapEnd;                 /* points to the first char after the gap */
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
    int nBeginModifyProcs;	/* number of begin-modify procs attached */
    bufBeginModifyCallbackProc	/* procedure to call before a batch of  */
	 *beginModifyProcs;	/* modifications is done. */
    void **beginModifyCbArgs;	/* caller args for begin-modify proc above */
    int nEndModifyProcs;	/* number of end-modify procs attached */
    bufEndModifyCallbackProc	/* procedure to call after a batch of  */
	 *endModifyProcs;	/* modifications is done. */
    void **endModifyCbArgs;	/* caller args for end-modify proc above */
    ssize_t cursorPosHint;		/* hint for reasonable cursor position after
    				   a buffer modification operation */
    char nullSubsChar;	    	/* NEdit is based on C null-terminated strings,
    	    	    	    	   so ascii-nul characters must be substituted
				   with something else.  This is the else, but
				   of course, things get quite messy when you
				   use it */
    RangesetTable *rangesetTable;
				/* current range sets */
    size_t *ansi_escpos;        /* indices of all ansi escape positions */
    size_t alloc_ansi_escpos;   /* ansi_escpos allocation size */
    size_t num_ansi_escpos;     /* number of ansi escape sequences */
} textBuffer;

typedef struct EscSeqStr {
    char *seq;
    size_t len;
    size_t off_orig;
    size_t off_trans;
} EscSeqStr;

typedef struct EscSeqArray {
    EscSeqStr *esc;
    size_t num_esc;
    char *text;
} EscSeqArray;

textBuffer *BufCreate(void);
textBuffer *BufCreatePreallocated(int requestedSize);
void BufFree(textBuffer *buf);
char *BufGetAll(textBuffer *buf);
const char *BufAsString(textBuffer *buf);
const char *BufAsStringCleaned(textBuffer *buf, EscSeqArray **esc);
void BufReintegrateEscSeq(textBuffer *buf, EscSeqArray *escseq);
void BufSetAll(textBuffer *buf, const char *text);
char* BufGetRange(const textBuffer* buf, ssize_t start, ssize_t end);
char BufGetCharacter(const textBuffer* buf, ssize_t pos);
wchar_t BufGetCharacterW(const textBuffer *buf, ssize_t pos);
FcChar32 BufGetCharacter32(const textBuffer* buf, ssize_t pos, int *charlen);
char *BufGetTextInRect(textBuffer *buf, ssize_t start, ssize_t end,
	int rectStart, int rectEnd);
void BufBeginModifyBatch(textBuffer *buf);
void BufEndModifyBatch(textBuffer *buf);
void BufInsert(textBuffer *buf, ssize_t pos, const char *text);
void BufRemove(textBuffer *buf, ssize_t start, ssize_t end);
void BufReplace(textBuffer *buf, ssize_t start, ssize_t end, const char *text);
void BufCopyFromBuf(textBuffer *fromBuf, textBuffer *toBuf, ssize_t fromStart,
        ssize_t fromEnd, ssize_t toPos);
void BufInsertCol(textBuffer *buf, int column, ssize_t startPos, const char *text,
        ssize_t *charsInserted, ssize_t *charsDeleted);
void BufReplaceRect(textBuffer *buf, ssize_t start, ssize_t end, int rectStart,
	int rectEnd, const char *text);
void BufRemoveRect(textBuffer *buf, ssize_t start, ssize_t end, int rectStart,
	int rectEnd);
void BufOverlayRect(textBuffer *buf, ssize_t startPos, int rectStart,
    	int rectEnd, const char *text, ssize_t *charsInserted, ssize_t *charsDeleted);
void BufClearRect(textBuffer *buf, ssize_t start, ssize_t end, int rectStart,
	int rectEnd);
int BufGetTabDistance(textBuffer *buf);
void BufSetTabDistance(textBuffer *buf, int tabDist);
void BufCheckDisplay(textBuffer *buf, ssize_t start, ssize_t end);
void BufSelect(textBuffer *buf, ssize_t start, ssize_t end);
void BufUnselect(textBuffer *buf);
void BufRectSelect(textBuffer *buf, ssize_t start, ssize_t end, int rectStart,
        int rectEnd);
ssize_t BufGetSelectionPos(textBuffer *buf, ssize_t *start, ssize_t *end,
        int *isRect, int *rectStart, int *rectEnd);
ssize_t BufGetEmptySelectionPos(textBuffer *buf, ssize_t *start, ssize_t *end,
        int *isRect, int *rectStart, int *rectEnd);
char *BufGetSelectionText(textBuffer *buf);
void BufRemoveSelected(textBuffer *buf);
void BufReplaceSelected(textBuffer *buf, const char *text);
void BufSecondarySelect(textBuffer *buf, ssize_t start, ssize_t end);
void BufSecondaryUnselect(textBuffer *buf);
void BufSecRectSelect(textBuffer *buf, ssize_t start, ssize_t end,
        int rectStart, int rectEnd);
ssize_t BufGetSecSelectPos(textBuffer *buf, ssize_t *start, ssize_t *end,
        int *isRect, int *rectStart, int *rectEnd);
char *BufGetSecSelectText(textBuffer *buf);
void BufRemoveSecSelect(textBuffer *buf);
void BufReplaceSecSelect(textBuffer *buf, const char *text);
void BufHighlight(textBuffer *buf, ssize_t start, ssize_t end);
void BufUnhighlight(textBuffer *buf);
void BufRectHighlight(textBuffer *buf, ssize_t start, ssize_t end,
        int rectStart, int rectEnd);
int BufGetHighlightPos(textBuffer *buf, ssize_t *start, ssize_t *end,
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
void BufAddBeginModifyCB(textBuffer *buf, bufBeginModifyCallbackProc bufBeginModifyCB,
	void *cbArg);
void BufRemoveBeginModifyCB(textBuffer *buf, bufBeginModifyCallbackProc 
	bufBeginModifyCB,	void *cbArg);
void BufAddEndModifyCB(textBuffer *buf, bufEndModifyCallbackProc bufEndModifyCB,
	void *cbArg);
void BufRemoveEndModifyCB(textBuffer *buf, bufEndModifyCallbackProc 
	bufEndModifyCB,	void *cbArg);
ssize_t BufStartOfLine(textBuffer *buf, ssize_t pos);
ssize_t BufEndOfLine(textBuffer *buf, ssize_t pos);
ssize_t BufGetExpandedChar(const textBuffer* buf, ssize_t pos, int indent,
        char* outStr);
ssize_t BufExpandCharacter(const char *c, int clen, int indent, char *outStr, int tabDist,
	char nullSubsChar, int *isMB);
int BufExpandCharacter4(char c, int indent, FcChar32 *outStr,
        int tabDist, char nullSubsChar);
int BufCharWidth(char c, int indent, int tabDist, char nullSubsChar);
ssize_t BufCountDispChars(const textBuffer* buf, ssize_t lineStartPos,
        ssize_t targetPos);
ssize_t BufCountForwardDispChars(textBuffer *buf, ssize_t lineStartPos, ssize_t nChars);
ssize_t BufCountLines(textBuffer *buf, ssize_t startPos, ssize_t endPos);
ssize_t BufCountForwardNLines(const textBuffer* buf, ssize_t startPos,
        ssize_t nLines);
ssize_t BufCountBackwardNLines(textBuffer *buf, ssize_t startPos, ssize_t nLines);
int BufSearchForward(textBuffer *buf, ssize_t startPos, const char *searchChars,
        ssize_t *foundPos);
int BufSearchBackward(textBuffer *buf, ssize_t startPos, const char *searchChars,
        ssize_t *foundPos);
int BufSubstituteNullChars(char *string, ssize_t length, textBuffer *buf);
void BufUnsubstituteNullChars(char *string, textBuffer *buf);
int BufCmp(textBuffer * buf, ssize_t pos, ssize_t len, const char *cmpText);

void BufEnableAnsiEsc(textBuffer *buf);
void BufDisableAnsiEsc(textBuffer *buf);
void BufParseEscSeq(textBuffer *buf, size_t pos, size_t nInserted, size_t nDeleted);
int BufEscPos2Index(
        const textBuffer *buf,
        size_t startIndex,
        size_t startValue,
        size_t pos,
        ssize_t *index,
        size_t *value);

int BufCharLen(const textBuffer *buf, ssize_t pos);
int BufLeftPos(textBuffer *buf, ssize_t pos);
int BufRightPos(textBuffer *buf, ssize_t pos);

int Utf8ToUcs4(const char *src_orig, FcChar32 *dst, int len);
int Ucs4ToUtf8(FcChar32 ucs4, char *dst);
int Utf8CharLen(const unsigned char *u);

#endif /* NEDIT_TEXTBUF_H_INCLUDED */
