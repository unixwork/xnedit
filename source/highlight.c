/*******************************************************************************
*									       *
* highlight.c -- Nirvana Editor syntax highlighting (text coloring and font    *
*   	    	 selected by file content				       *
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
* June 24, 1996								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "highlight.h"
#include "textBuf.h"
#include "textDisp.h"
#include "text.h"
#include "textP.h"
#include "nedit.h"
#include "regularExp.h"
#include "highlightData.h"
#include "preferences.h"
#include "window.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/
#include <inttypes.h>

#include <Xm/Xm.h>
#include <Xm/XmP.h>
#if XmVersion >= 1002
#include <Xm/PrimitiveP.h>
#endif

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* How much re-parsing to do when an unfinished style is encountered */
#define PASS_2_REPARSE_CHUNK_SIZE 1000

/* Initial forward expansion of parsing region in incremental reparsing,
   when style changes propagate forward beyond the original modification.
   This distance is increased by a factor of two for each subsequent step. */
#define REPARSE_CHUNK_SIZE 80

/* Meanings of style buffer characters (styles). Don't use plain 'A' or 'B';
   it causes problems with EBCDIC coding (possibly negative offsets when 
   subtracting 'A'). */
#define UNFINISHED_STYLE ASCII_A
#define PLAIN_STYLE     (ASCII_A+1)
#define IS_PLAIN(style) (style == PLAIN_STYLE || style == UNFINISHED_STYLE)
#define IS_STYLED(style) (style != PLAIN_STYLE && style != UNFINISHED_STYLE)

/* Compare two styles where one of the styles may not yet have been processed
   with pass2 patterns */
#define EQUIVALENT_STYLE(style1, style2, firstPass2Style) (style1 == style2 || \
    	(style1 == UNFINISHED_STYLE && \
    	 (style2 == PLAIN_STYLE || (unsigned char)style2 >= firstPass2Style)) || \
    	(style2 == UNFINISHED_STYLE && \
    	 (style1 == PLAIN_STYLE || (unsigned char)style1 >= firstPass2Style)))

/* Scanning context can be reduced (with big efficiency gains) if we
   know that patterns can't cross line boundaries, which is implied
   by a context requirement of 1 line and 0 characters */
#define CAN_CROSS_LINE_BOUNDARIES(contextRequirements) \
    	(contextRequirements->nLines != 1 || contextRequirements->nChars != 0)

/* "Compiled" version of pattern specification */
typedef struct _highlightDataRec {
    regexp *startRE;
    regexp *endRE;
    regexp *errorRE;
    regexp *subPatternRE;
    char style;
    int colorOnly;
    signed char startSubexprs[NSUBEXP+1];
    signed char endSubexprs[NSUBEXP+1];
    int flags;
    int nSubPatterns;
    int nSubBranches; /* Number of top-level branches of subPatternRE */
    int userStyleIndex;
    struct _highlightDataRec **subPatterns;
} highlightDataRec;

/* Context requirements for incremental reparsing of a pattern set */
typedef struct {
    int nLines;
    int nChars;
} reparseContext;

/* Data structure attached to window to hold all syntax highlighting
   information (for both drawing and incremental reparsing) */
typedef struct {
    highlightDataRec *pass1Patterns;
    highlightDataRec *pass2Patterns;
    char *parentStyles;
    reparseContext contextRequirements;
    styleTableEntry *styleTable;
    int nStyles;
    textBuffer *styleBuffer;
    patternSet *patternSetForWindow;
} windowHighlightData;

static windowHighlightData *createHighlightData(WindowInfo *window,
	patternSet *patSet);
static void freeHighlightData(windowHighlightData *hd);
static patternSet *findPatternsForWindow(WindowInfo *window, int warn);
static highlightDataRec *compilePatterns(Widget dialogParent,
    	highlightPattern *patternSrc, int nPatterns);
static void freePatterns(highlightDataRec *patterns);
static void handleUnparsedRegion(const WindowInfo* win, textBuffer* styleBuf,
        int pos);
static void handleUnparsedRegionCB(const textDisp* textD, int pos,
        const void* cbArg);
static void incrementalReparse(windowHighlightData *highlightData,
    	textBuffer *buf, int pos, int nInserted, const char *delimiters);
static int parseBufferRange(highlightDataRec *pass1Patterns,
    	highlightDataRec *pass2Patterns, textBuffer *buf, textBuffer *styleBuf,
        reparseContext *contextRequirements, int beginParse, int endParse,
        const char *delimiters);
static int parseString(highlightDataRec *pattern, const char **string,
    	char **styleString, int length, char *prevChar, int anchored,
    	const char *delimiters, const char* lookBehindTo, const char* match_till);
static void passTwoParseString(highlightDataRec *pattern, char *string,
        char *styleString, int length, char *prevChar, const char *delimiters,
        const char* lookBehindTo, const char* match_till);
static void fillStyleString(const char **stringPtr, char **stylePtr,
        const char *toPtr, char style, char *prevChar);
static void modifyStyleBuf(textBuffer *styleBuf, char *styleString,
    	int startPos, int endPos, int firstPass2Style);
static int lastModified(textBuffer *styleBuf);
static int max(int i1, int i2);
static int min(int i1, int i2);
static char getPrevChar(textBuffer *buf, int pos);
static regexp *compileREAndWarn(Widget parent, const char *re);
static int parentStyleOf(const char *parentStyles, int style);
static int isParentStyle(const char *parentStyles, int style1, int style2);
static int findSafeParseRestartPos(textBuffer *buf,
    	windowHighlightData *highlightData, int *pos);
static int backwardOneContext(textBuffer *buf, reparseContext *context,
    	int fromPos);
static int forwardOneContext(textBuffer *buf, reparseContext *context,
    	int fromPos);
static void recolorSubexpr(regexp *re, int subexpr, int style,
        const char *string, char *styleString);
static int indexOfNamedPattern(highlightPattern *patList, int nPats,
    	const char *patName);
static int findTopLevelParentIndex(highlightPattern *patList, int nPats,
    	int index);
static highlightDataRec *patternOfStyle(highlightDataRec *patterns, int style);
static void updateWindowHeight(WindowInfo *window, int oldFontHeight);
static int getFontHeight(WindowInfo *window);
static styleTableEntry *styleTableEntryOfCode(WindowInfo *window, int hCode);

/*
** Buffer modification callback for triggering re-parsing of modified
** text and keeping the style buffer synchronized with the text buffer.
** This must be attached to the the text buffer BEFORE any widget text
** display callbacks, so it can get the style buffer ready to be used
** by the text display routines.
**
** Update the style buffer for changes to the text, and mark any style
** changes by selecting the region in the style buffer.  This strange
** protocol of informing the text display to redraw style changes by
** making selections in the style buffer is used because this routine
** is intended to be called BEFORE the text display callback paints the
** text (to minimize redraws and, most importantly, to synchronize the
** style buffer with the text buffer).  If we redraw now, the text
** display hasn't yet processed the modification, redrawing later is
** not only complicated, it will double-draw almost everything typed.
** 
** Note: This routine must be kept efficient.  It is called for every
** character typed.
*/
void SyntaxHighlightModifyCB(int pos, int nInserted, int nDeleted,
    	int nRestyled, const char *deletedText, void *cbArg) 
{    
    WindowInfo *window = (WindowInfo *)cbArg;
    windowHighlightData 
    	    *highlightData = (windowHighlightData *)window->highlightData;
    
    if (highlightData == NULL)
    	return;
    	
    /* Restyling-only modifications (usually a primary or secondary  selection)
       don't require any processing, but clear out the style buffer selection
       so the widget doesn't think it has to keep redrawing the old area */
    if (nInserted == 0 && nDeleted == 0) {
    	BufUnselect(highlightData->styleBuffer);
    	return;
    }
    
    /* First and foremost, the style buffer must track the text buffer
       accurately and correctly */
    if (nInserted > 0) {
        char *insStyle;
        int i;

    	insStyle = (char*)NEditMalloc(nInserted + 1);
    	for (i=0; i<nInserted; i++)
    	    insStyle[i] = UNFINISHED_STYLE;
    	insStyle[i] = '\0';
    	BufReplace(highlightData->styleBuffer, pos, pos+nDeleted, insStyle);
    	NEditFree(insStyle);
    } else {
    	BufRemove(highlightData->styleBuffer, pos, pos+nDeleted);
    }

    /* Mark the changed region in the style buffer as requiring redraw.  This
       is not necessary for getting it redrawn, it will be redrawn anyhow by
       the text display callback, but it clears the previous selection and
       saves the modifyStyleBuf routine from unnecessary work in tracking
       changes that are already scheduled for redraw */
    BufSelect(highlightData->styleBuffer, pos, pos+nInserted);
    
    /* Re-parse around the changed region */
    if (highlightData->pass1Patterns)
    	incrementalReparse(highlightData, window->buffer, pos, nInserted,
    	    	GetWindowDelimiters(window));
}

/*
** Turn on syntax highlighting.  If "warn" is true, warn the user when it
** can't be done, otherwise, just return.
*/
void StartHighlighting(WindowInfo *window, int warn)
{
    patternSet *patterns;
    windowHighlightData *highlightData;
    char *stylePtr, *styleString;
    const char  *stringPtr, *bufString;
    char prevChar = '\0';
    int i, oldFontHeight;
    
    /* Find the pattern set matching the window's current
       language mode, tell the user if it can't be done */
    patterns = findPatternsForWindow(window, warn);
    if (patterns == NULL)
    	return;
    
    /* Compile the patterns */
    highlightData = createHighlightData(window, patterns);
    if (highlightData == NULL)
    	return;
    
    /* Prepare for a long delay, refresh display and put up a watch cursor */
    BeginWait(window->shell);
    XmUpdateDisplay(window->shell);
    
    /* Parse the buffer with pass 1 patterns.  If there are none, initialize
       the style buffer to all UNFINISHED_STYLE to trigger parsing later */
    stylePtr = styleString = (char*)NEditMalloc(window->buffer->length + 1);
    if (highlightData->pass1Patterns == NULL) {
    	for (i=0; i<window->buffer->length; i++)
    	    *stylePtr++ = UNFINISHED_STYLE;
    } else {
	stringPtr = bufString = BufAsString(window->buffer);
	parseString(highlightData->pass1Patterns, &stringPtr, &stylePtr,
    		window->buffer->length, &prevChar, False,
    		GetWindowDelimiters(window), bufString, NULL);
    }
    *stylePtr = '\0';
    BufSetAll(highlightData->styleBuffer, styleString);
    NEditFree(styleString);

    /* install highlight pattern data in the window data structure */
    window->highlightData = highlightData;
    	
    /* Get the height of the current font in the window, to be used after
       highlighting is turned on to resize the window to make room for
       additional highlight fonts which may be sized differently */
    oldFontHeight = getFontHeight(window);
    
    /* Attach highlight information to text widgets in each pane */
    AttachHighlightToWidget(window->textArea, window);
    for (i=0; i<window->nPanes; i++)
	AttachHighlightToWidget(window->textPanes[i], window);
        
    /* Re-size the window to fit the highlight fonts properly & tell the
       window manager about the potential line-height change as well */
    updateWindowHeight(window, oldFontHeight);
    UpdateWMSizeHints(window);
    UpdateMinPaneHeights(window);

    /* Make sure that if the window has grown, the additional area gets
       repainted. Otherwise, it is possible that the area gets moved before a
       repaint event is received and the area doesn't get repainted at all
       (eg. because of a -line command line argument that moves the text). */
    XmUpdateDisplay(window->shell);
    EndWait(window->shell);
}

/*
** Turn off syntax highlighting and free style buffer, compiled patterns, and
** related data.
*/
void StopHighlighting(WindowInfo *window)
{
    int i, oldFontHeight;

    if (window->highlightData==NULL)
    	return;
    	
    /* Get the line height being used by the highlight fonts in the window,
       to be used after highlighting is turned off to resize the window
       back to the line height of the primary font */
    oldFontHeight = getFontHeight(window);
    
    /* Free and remove the highlight data from the window */
    freeHighlightData((windowHighlightData *)window->highlightData);
    window->highlightData = NULL;
    
    /* Remove and detach style buffer and style table from all text
       display(s) of window, and redisplay without highlighting */
    RemoveWidgetHighlight(window->textArea);
    for (i=0; i<window->nPanes; i++)
	RemoveWidgetHighlight(window->textPanes[i]);
        
    /* Re-size the window to fit the primary font properly & tell the window
       manager about the potential line-height change as well */
    updateWindowHeight(window, oldFontHeight);
    UpdateWMSizeHints(window);
    UpdateMinPaneHeights(window);
}

/*
** Free highlighting data from a window destined for destruction, without
** redisplaying.
*/
void FreeHighlightingData(WindowInfo *window)
{
    int i;
    
    if (window->highlightData == NULL)
	return;
    
    /* Free and remove the highlight data from the window */
    freeHighlightData((windowHighlightData *)window->highlightData);
    window->highlightData = NULL;
    
    /* The text display may make a last desperate attempt to access highlight
       information when it is destroyed, which would be a disaster. */
    ((TextWidget)window->textArea)->text.textD->styleBuffer = NULL;
    for (i=0; i<window->nPanes; i++)
	((TextWidget)window->textPanes[i])->text.textD->styleBuffer = NULL;
}

/*
** Attach style information from a window's highlight data to a
** text widget and redisplay.
*/
void AttachHighlightToWidget(Widget widget, WindowInfo *window)
{
    windowHighlightData *highlightData =
    	    (windowHighlightData *)window->highlightData;
    
    TextDAttachHighlightData(((TextWidget)widget)->text.textD,
    	    highlightData->styleBuffer, highlightData->styleTable,
    	    highlightData->nStyles, UNFINISHED_STYLE, handleUnparsedRegionCB,
    	    window);
}

/*
** Remove style information from a text widget and redisplay it.
*/
void RemoveWidgetHighlight(Widget widget)
{
    TextDAttachHighlightData(((TextWidget)widget)->text.textD,
    	    NULL, NULL, 0, UNFINISHED_STYLE, NULL, NULL);
}

/*
** Change highlight fonts and/or styles in a highlighted window, without
** re-parsing.
*/
void UpdateHighlightStyles(WindowInfo *window)
{
    patternSet *patterns;
    windowHighlightData *highlightData;
    windowHighlightData *oldHighlightData =
    	    (windowHighlightData *)window->highlightData;
    textBuffer *styleBuffer;
    int i;
    
    /* Do nothing if window not highlighted */
    if (window->highlightData == NULL)
    	return;

    /* Find the pattern set for the window's current language mode */
    patterns = findPatternsForWindow(window, False);
    if (patterns == NULL) {
    	StopHighlighting(window);
    	return;
    }
    
    /* Build new patterns */
    highlightData = createHighlightData(window, patterns);
    if (highlightData == NULL) {
    	StopHighlighting(window);
    	return;
    }

    /* Update highlight pattern data in the window data structure, but
       preserve all of the effort that went in to parsing the buffer
       by swapping it with the empty one in highlightData (which is then
       freed in freeHighlightData) */
    styleBuffer = oldHighlightData->styleBuffer;
    oldHighlightData->styleBuffer = highlightData->styleBuffer;
    freeHighlightData(oldHighlightData);
    highlightData->styleBuffer = styleBuffer;
    window->highlightData = highlightData;
    
    /* Attach new highlight information to text widgets in each pane
       (and redraw) */
    AttachHighlightToWidget(window->textArea, window);
    for (i=0; i<window->nPanes; i++)
	AttachHighlightToWidget(window->textPanes[i], window);
}

/*
** Do a test compile of patterns in "patSet" and report problems to the
** user via dialog.  Returns True if patterns are ok.
**
** This is somewhat kludgy in that it uses createHighlightData, which
** requires a window to find the fonts to use, and just uses a random
** window from the window list.  Since the window is used to get the
** dialog parent as well, in non-popups-under-pointer mode, these dialogs
** will appear in odd places on the screen.
*/
int TestHighlightPatterns(patternSet *patSet)
{
    windowHighlightData *highlightData;
    
    /* Compile the patterns (passing a random window as a source for fonts, and
       parent for dialogs, since we really don't care what fonts are used) */
    highlightData = createHighlightData(WindowList, patSet);
    if (highlightData == NULL)
    	return False;
    freeHighlightData(highlightData);
    return True;
}

/*
** Returns the highlight style of the character at a given position of a 
** window. To avoid breaking encapsulation, the highlight style is converted 
** to a void* pointer (no other module has to know that characters are used
** to represent highlight styles; that would complicate future extensions).
** Returns NULL if the window has highlighting turned off.
** The only guarantee that this function offers, is that when the same
** pointer is returned for two positions, the corresponding characters have
** the same highlight style.
**/
void* GetHighlightInfo(WindowInfo *window, int pos)
{
    int style;
    highlightDataRec *pattern = NULL;
    windowHighlightData *highlightData = 
	(windowHighlightData *)window->highlightData; 
    if (!highlightData)
	return NULL;
    
    /* Be careful with signed/unsigned conversions. NO conversion here! */
    style = (int)BufGetCharacter(highlightData->styleBuffer, pos);
    
    /* Beware of unparsed regions. */
    if (style == UNFINISHED_STYLE) {
	handleUnparsedRegion(window, highlightData->styleBuffer, pos);
	style = (int)BufGetCharacter(highlightData->styleBuffer, pos);
    }
	    
    if (highlightData->pass1Patterns) {
       pattern = patternOfStyle(highlightData->pass1Patterns, style);
    }
    
    if (!pattern && highlightData->pass2Patterns) {
	pattern = patternOfStyle(highlightData->pass2Patterns, style);
    }
    
    if (!pattern) {
	return NULL;
    }
    return (void*)(intptr_t)pattern->userStyleIndex;    
}
    
/*
** Free allocated memory associated with highlight data, including compiled
** regular expressions, style buffer and style table.  Note: be sure to
** NULL out the widget references to the objects in this structure before
** calling this.  Because of the slow, multi-phase destruction of
** widgets, this data can be referenced even AFTER destroying the widget.
*/
static void freeHighlightData(windowHighlightData *hd)
{
    if (hd == NULL)
    	return;
    if (hd->pass1Patterns != NULL)
    	freePatterns(hd->pass1Patterns);
    if (hd->pass2Patterns != NULL)
    	freePatterns(hd->pass2Patterns);
    NEditFree(hd->parentStyles);
    BufFree(hd->styleBuffer);
    NEditFree(hd->styleTable);
    NEditFree(hd);
}

/*
** Find the pattern set matching the window's current language mode, or
** tell the user if it can't be done (if warn is True) and return NULL.
*/
static patternSet *findPatternsForWindow(WindowInfo *window, int warn)
{
    patternSet *patterns;
    char *modeName;
    
    /* Find the window's language mode.  If none is set, warn user */
    modeName = LanguageModeName(window->languageMode);
    if (modeName == NULL) {
    	if (warn)
    	    DialogF(DF_WARN, window->shell, 1, "Language Mode",
                    "No language-specific mode has been set for this file.\n\n"
                    "To use syntax highlighting in this window, please select a\n"
                    "language from the Preferences -> Language Modes menu.\n\n"
                    "New language modes and syntax highlighting patterns can be\n"
                    "added via Preferences -> Default Settings -> Language Modes,\n"
                    "and Preferences -> Default Settings -> Syntax Highlighting.",
                    "OK");
    	return NULL;
    }
    
    /* Look up the appropriate pattern for the language */
    patterns = FindPatternSet(modeName);
    if (patterns == NULL)
    {
        if (warn)
        {
            DialogF(DF_WARN, window->shell, 1, "Language Mode",
                    "Syntax highlighting is not available in language\n"
                    "mode %s.\n\n"
                    "You can create new syntax highlight patterns in the\n"
                    "Preferences -> Default Settings -> Syntax Highlighting\n"
                    "dialog, or choose a different language mode from:\n"
                    "Preferences -> Language Mode.", "OK", modeName);
            return NULL;
        }
    }
    
    return patterns;
}

/*
** Create complete syntax highlighting information from "patternSrc", using
** highlighting fonts from "window", includes pattern compilation.  If errors
** are encountered, warns user with a dialog and returns NULL.  To free the
** allocated components of the returned data structure, use freeHighlightData.
*/
static windowHighlightData *createHighlightData(WindowInfo *window,
    	patternSet *patSet)
{
    highlightPattern *patternSrc = patSet->patterns;
    int nPatterns = patSet->nPatterns;
    int contextLines = patSet->lineContext;
    int contextChars = patSet->charContext;
    int i, nPass1Patterns, nPass2Patterns;
    int noPass1, noPass2;
    char *parentStyles, *parentStylesPtr, *parentName;
    highlightPattern *pass1PatternSrc, *pass2PatternSrc, *p1Ptr, *p2Ptr;
    styleTableEntry *styleTable, *styleTablePtr;
    textBuffer *styleBuf;
    highlightDataRec *pass1Pats, *pass2Pats;
    windowHighlightData *highlightData;
    
    /* The highlighting code can't handle empty pattern sets, quietly say no */
    if (nPatterns == 0)
    {
        return NULL;
    }

    /* Check that the styles and parent pattern names actually exist */
    if (!NamedStyleExists("Plain"))
    {
        DialogF(DF_WARN, window->shell, 1, "Highlight Style",
                "Highlight style \"Plain\" is missing", "OK");
        return NULL;
    }

    for (i=0; i<nPatterns; i++)
    {
        if (patternSrc[i].subPatternOf != NULL
                && indexOfNamedPattern(patternSrc, nPatterns,
                        patternSrc[i].subPatternOf) == -1)
        {
            DialogF(DF_WARN, window->shell, 1, "Parent Pattern",
                    "Parent field \"%s\" in pattern \"%s\"\n"
                    "does not match any highlight patterns in this set",
                    "OK", patternSrc[i].subPatternOf, patternSrc[i].name);
            return NULL;
        }
    }

    for (i=0; i<nPatterns; i++)
    {
        if (!NamedStyleExists(patternSrc[i].style))
        {
            DialogF(DF_WARN, window->shell, 1, "Highlight Style",
                    "Style \"%s\" named in pattern \"%s\"\n"
                    "does not match any existing style", "OK",
                    patternSrc[i].style, patternSrc[i].name);
            return NULL;
        }
    }
    
    /* Make DEFER_PARSING flags agree with top level patterns (originally,
       individual flags had to be correct and were checked here, but dialog now
       shows this setting only on top patterns which is much less confusing) */
    for (i = 0; i < nPatterns; i++)
    {
        if (patternSrc[i].subPatternOf != NULL)
        {
            int parentindex;

            parentindex=findTopLevelParentIndex(patternSrc, nPatterns, i);
            if (parentindex==-1)
            {
                DialogF(DF_WARN, window->shell, 1, "Parent Pattern",
                        "Pattern \"%s\" does not have valid parent", "OK",
                        patternSrc[i].name);
                return NULL;
            }

            if (patternSrc[parentindex].flags & DEFER_PARSING)
            {
                patternSrc[i].flags |= DEFER_PARSING;
            } else
            {
                patternSrc[i].flags &= ~DEFER_PARSING;
            }
        }
    }

    /* Sort patterns into those to be used in pass 1 parsing, and those to
       be used in pass 2, and add default pattern (0) to each list */
    nPass1Patterns = 1;
    nPass2Patterns = 1;
    for (i=0; i<nPatterns; i++)
    	if (patternSrc[i].flags & DEFER_PARSING)
    	    nPass2Patterns++;
    	else
    	    nPass1Patterns++;
    p1Ptr = pass1PatternSrc = (highlightPattern *)NEditMalloc(
    	    sizeof(highlightPattern) * nPass1Patterns);
    p2Ptr = pass2PatternSrc = (highlightPattern *)NEditMalloc(
    	    sizeof(highlightPattern) * nPass2Patterns);
    p1Ptr->name = p2Ptr->name = "";
    p1Ptr->startRE = p2Ptr->startRE = NULL;
    p1Ptr->endRE = p2Ptr->endRE = NULL;
    p1Ptr->errorRE = p2Ptr->errorRE = NULL;
    p1Ptr->style = p2Ptr->style = "Plain";
    p1Ptr->subPatternOf = p2Ptr->subPatternOf = NULL;
    p1Ptr->flags = p2Ptr->flags = 0;
    p1Ptr++; p2Ptr++;
    for (i=0; i<nPatterns; i++) {
    	if (patternSrc[i].flags & DEFER_PARSING)
    	    *p2Ptr++ = patternSrc[i];
    	else
    	    *p1Ptr++ = patternSrc[i];
    }
    
    /* If a particular pass is empty except for the default pattern, don't
       bother compiling it or setting up styles */
    if (nPass1Patterns == 1)
    	nPass1Patterns = 0;
    if (nPass2Patterns == 1)
    	nPass2Patterns = 0;

    /* Compile patterns */
    if (nPass1Patterns == 0)
    	pass1Pats = NULL;
    else {
	pass1Pats = compilePatterns(window->shell, pass1PatternSrc,
    		nPass1Patterns);
	if (pass1Pats == NULL)
    	    return NULL;
    }
    if (nPass2Patterns == 0)
    	pass2Pats = NULL;
    else {
	pass2Pats = compilePatterns(window->shell, pass2PatternSrc,
    		nPass2Patterns);  
	if (pass2Pats == NULL)
    	    return NULL;
    }
    
    /* Set pattern styles.  If there are pass 2 patterns, pass 1 pattern
       0 should have a default style of UNFINISHED_STYLE.  With no pass 2
       patterns, unstyled areas of pass 1 patterns should be PLAIN_STYLE
       to avoid triggering re-parsing every time they are encountered */
    noPass1 = nPass1Patterns == 0;
    noPass2 = nPass2Patterns == 0;
    if (noPass2)
    	pass1Pats[0].style = PLAIN_STYLE;
    else if (noPass1)
    	pass2Pats[0].style = PLAIN_STYLE;
    else {
	pass1Pats[0].style = UNFINISHED_STYLE;
	pass2Pats[0].style = PLAIN_STYLE;
    }
    for (i=1; i<nPass1Patterns; i++)
       	pass1Pats[i].style = PLAIN_STYLE + i;
    for (i=1; i<nPass2Patterns; i++)
       	pass2Pats[i].style = PLAIN_STYLE + (noPass1 ? 0 : nPass1Patterns-1) + i;
    
    /* Create table for finding parent styles */
    parentStylesPtr = parentStyles = (char*)NEditMalloc(nPass1Patterns+nPass2Patterns+2);
    *parentStylesPtr++ = '\0';
    *parentStylesPtr++ = '\0';
    for (i=1; i<nPass1Patterns; i++) {
	parentName = pass1PatternSrc[i].subPatternOf;
	*parentStylesPtr++ = parentName == NULL ? PLAIN_STYLE :
		pass1Pats[indexOfNamedPattern(pass1PatternSrc,
		nPass1Patterns, parentName)].style;
    }
    for (i=1; i<nPass2Patterns; i++) {
	parentName = pass2PatternSrc[i].subPatternOf;
	*parentStylesPtr++ = parentName == NULL ? PLAIN_STYLE :
		pass2Pats[indexOfNamedPattern(pass2PatternSrc,
		nPass2Patterns, parentName)].style;
    }
    
    /* Set up table for mapping colors and fonts to syntax */
    styleTablePtr = styleTable = (styleTableEntry *)NEditMalloc(
    	    sizeof(styleTableEntry) * (nPass1Patterns + nPass2Patterns + 1));
#define setStyleTablePtr(styleTablePtr, patternSrc) \
    do { \
      styleTableEntry *p = styleTablePtr; \
      highlightPattern *pat = patternSrc; \
      int r, g, b; \
      \
      p->highlightName = pat->name; \
      p->styleName = pat->style; \
      p->colorName = ColorOfNamedStyle(pat->style); \
      p->bgColorName = BgColorOfNamedStyle(pat->style); \
      p->isBold = FontOfNamedStyleIsBold(pat->style); \
      p->isItalic = FontOfNamedStyleIsItalic(pat->style); \
      /* And now for the more physical stuff */ \
      p->color = AllocColor(window->textArea, p->colorName, &r, &g, &b); \
      p->red = r; \
      p->green = g; \
      p->blue = b; \
      if (p->bgColorName) { \
        p->bgColor = AllocColor(window->textArea, p->bgColorName, &r, &g, &b); \
        p->bgRed = r; \
        p->bgGreen = g; \
        p->bgBlue = b; \
      } \
      else { \
        p->bgColor = p->color; \
        p->bgRed = r; \
        p->bgGreen = g; \
        p->bgBlue = b; \
      } \
      p->font = FontOfNamedStyle(window, pat->style); \
    } while (0)

    /* PLAIN_STYLE (pass 1) */
    styleTablePtr->underline = FALSE;
    setStyleTablePtr(styleTablePtr++,
                   noPass1 ? &pass2PatternSrc[0] : &pass1PatternSrc[0]);
    /* PLAIN_STYLE (pass 2) */
    styleTablePtr->underline = FALSE;
    setStyleTablePtr(styleTablePtr++,
                   noPass2 ? &pass1PatternSrc[0] : &pass2PatternSrc[0]);
    /* explicit styles (pass 1) */
    for (i=1; i<nPass1Patterns; i++) {
    	styleTablePtr->underline = FALSE;
      setStyleTablePtr(styleTablePtr++, &pass1PatternSrc[i]);
    }
    /* explicit styles (pass 2) */
    for (i=1; i<nPass2Patterns; i++) {
    	styleTablePtr->underline = FALSE;
      setStyleTablePtr(styleTablePtr++, &pass2PatternSrc[i]);
    }

    /* Free the temporary sorted pattern source list */
    NEditFree(pass1PatternSrc);
    NEditFree(pass2PatternSrc);
    
    /* Create the style buffer */
    styleBuf = BufCreate();
    
    /* Collect all of the highlighting information in a single structure */
    highlightData =(windowHighlightData *)NEditMalloc(sizeof(windowHighlightData));
    highlightData->pass1Patterns = pass1Pats;
    highlightData->pass2Patterns = pass2Pats;
    highlightData->parentStyles = parentStyles;
    highlightData->styleTable = styleTable;
    highlightData->nStyles = styleTablePtr - styleTable;
    highlightData->styleBuffer = styleBuf;
    highlightData->contextRequirements.nLines = contextLines;
    highlightData->contextRequirements.nChars = contextChars;
    highlightData->patternSetForWindow = patSet;
    
    return highlightData;
}

/*
** Transform pattern sources into the compiled highlight information
** actually used by the code.  Output is a tree of highlightDataRec structures
** containing compiled regular expressions and style information.
*/
static highlightDataRec *compilePatterns(Widget dialogParent,
    	highlightPattern *patternSrc, int nPatterns)
{
    int i, nSubExprs, patternNum, length, subPatIndex, subExprNum, charsRead;
    int parentIndex;
    char *ptr, *bigPattern, *compileMsg;
    highlightDataRec *compiledPats;
    
    /* Allocate memory for the compiled patterns.  The list is terminated
       by a record with style == 0. */
    compiledPats = (highlightDataRec *)NEditMalloc(sizeof(highlightDataRec) *
    	    (nPatterns + 1));
    compiledPats[nPatterns].style = 0;
    
    /* Build the tree of parse expressions */
    for (i=0; i<nPatterns; i++) {
    	compiledPats[i].nSubPatterns = 0;
    	compiledPats[i].nSubBranches = 0;
    }
    for (i=1; i<nPatterns; i++)
    	if (patternSrc[i].subPatternOf == NULL)
    	    compiledPats[0].nSubPatterns++;
    	else
    	    compiledPats[indexOfNamedPattern(patternSrc, nPatterns,
    	    	    patternSrc[i].subPatternOf)].nSubPatterns++;
    for (i=0; i<nPatterns; i++)
    	compiledPats[i].subPatterns = compiledPats[i].nSubPatterns == 0 ?
    	    	NULL : (highlightDataRec **)NEditMalloc(
    	    	    sizeof(highlightDataRec *) * compiledPats[i].nSubPatterns);
    for (i=0; i<nPatterns; i++)
    	compiledPats[i].nSubPatterns = 0;
    for (i=1; i<nPatterns; i++) {
    	if (patternSrc[i].subPatternOf == NULL) {
    	    compiledPats[0].subPatterns[compiledPats[0].nSubPatterns++] =
    	    		&compiledPats[i];
    	} else {
    	    parentIndex = indexOfNamedPattern(patternSrc,
    	    	    nPatterns, patternSrc[i].subPatternOf);
    	    compiledPats[parentIndex].subPatterns[compiledPats[parentIndex].
    	    	    nSubPatterns++] = &compiledPats[i];
    	}
    }
    
    /* Process color-only sub patterns (no regular expressions to match,
       just colors and fonts for sub-expressions of the parent pattern */
    for (i=0; i<nPatterns; i++) {
        compiledPats[i].colorOnly = patternSrc[i].flags & COLOR_ONLY;
        compiledPats[i].userStyleIndex = IndexOfNamedStyle(patternSrc[i].style);
        if (compiledPats[i].colorOnly && compiledPats[i].nSubPatterns != 0)
        {
            DialogF(DF_WARN, dialogParent, 1, "Color-only Pattern",
                    "Color-only pattern \"%s\" may not have subpatterns", 
                    "OK", patternSrc[i].name);
            return NULL;
        }
        nSubExprs = 0;
        if (patternSrc[i].startRE != NULL) {
            ptr = patternSrc[i].startRE;
            while(TRUE) {
		if (*ptr == '&') {
		    compiledPats[i].startSubexprs[nSubExprs++] = 0;
		    ptr++;
		} else if (sscanf(ptr, "\\%d%n", &subExprNum, &charsRead)==1) {
        	    compiledPats[i].startSubexprs[nSubExprs++] = subExprNum;
        	    ptr += charsRead;
		} else
		    break;
            }
        }
        compiledPats[i].startSubexprs[nSubExprs] = -1;
        nSubExprs = 0;
        if (patternSrc[i].endRE != NULL) {
            ptr = patternSrc[i].endRE;
            while(TRUE) {
		if (*ptr == '&') {
		    compiledPats[i].endSubexprs[nSubExprs++] = 0;
		    ptr++;
		} else if (sscanf(ptr, "\\%d%n", &subExprNum, &charsRead)==1) {
        	    compiledPats[i].endSubexprs[nSubExprs++] = subExprNum;
        	    ptr += charsRead;
		} else
		    break;
            }
        }
        compiledPats[i].endSubexprs[nSubExprs] = -1;
    }

    /* Compile regular expressions for all highlight patterns */
    for (i=0; i<nPatterns; i++) {
	if (patternSrc[i].startRE == NULL || compiledPats[i].colorOnly)
	    compiledPats[i].startRE = NULL;
	else {
	    if ((compiledPats[i].startRE = compileREAndWarn(dialogParent,
	    	    patternSrc[i].startRE)) == NULL)
   		return NULL;
    	}
    	if (patternSrc[i].endRE == NULL || compiledPats[i].colorOnly)
    	    compiledPats[i].endRE = NULL;
    	else {
    	    if ((compiledPats[i].endRE = compileREAndWarn(dialogParent,
    	    	    patternSrc[i].endRE)) == NULL)
 		return NULL;
    	}
    	if (patternSrc[i].errorRE == NULL)
    	    compiledPats[i].errorRE = NULL;
    	else {
    	    if ((compiledPats[i].errorRE = compileREAndWarn(dialogParent,
    	    	    patternSrc[i].errorRE)) == NULL)
 		return NULL;
    	}
    }
    
    /* Construct and compile the great hairy pattern to match the OR of the
       end pattern, the error pattern, and all of the start patterns of the
       sub-patterns */
    for (patternNum=0; patternNum<nPatterns; patternNum++) {
	if (patternSrc[patternNum].endRE == NULL &&
	    	patternSrc[patternNum].errorRE == NULL &&
	    	compiledPats[patternNum].nSubPatterns == 0) {
	    compiledPats[patternNum].subPatternRE = NULL;
	    continue;
	}
	length = (compiledPats[patternNum].colorOnly ||
	    	patternSrc[patternNum].endRE == NULL) ? 0 :
	    	strlen(patternSrc[patternNum].endRE) + 5;
	length += (compiledPats[patternNum].colorOnly ||
                   patternSrc[patternNum].errorRE == NULL) ? 0 :
	    	strlen(patternSrc[patternNum].errorRE) + 5;
	for (i=0; i<compiledPats[patternNum].nSubPatterns; i++) {
    	    subPatIndex = compiledPats[patternNum].subPatterns[i]-compiledPats;
    	    length += compiledPats[subPatIndex].colorOnly ? 0 :
    	    	    strlen(patternSrc[subPatIndex].startRE) + 5;
	}
	if (length == 0) {
	    compiledPats[patternNum].subPatternRE = NULL;
	    continue;
	}
	bigPattern = (char*)NEditMalloc(length+1);
	ptr=bigPattern;
	if (patternSrc[patternNum].endRE != NULL) {
	    *ptr++ = '('; *ptr++ = '?'; *ptr++ = ':';
    	    strcpy(ptr, patternSrc[patternNum].endRE);
    	    ptr += strlen(patternSrc[patternNum].endRE);
    	    *ptr++ = ')';
    	    *ptr++ = '|';
	    compiledPats[patternNum].nSubBranches++;
	}
	if (patternSrc[patternNum].errorRE != NULL) {
	    *ptr++ = '('; *ptr++ = '?'; *ptr++ = ':';
    	    strcpy(ptr, patternSrc[patternNum].errorRE);
    	    ptr += strlen(patternSrc[patternNum].errorRE);
    	    *ptr++ = ')';
    	    *ptr++ = '|';
	    compiledPats[patternNum].nSubBranches++;
	}
	for (i=0; i<compiledPats[patternNum].nSubPatterns; i++) {
    	    subPatIndex = compiledPats[patternNum].subPatterns[i]-compiledPats;
    	    if (compiledPats[subPatIndex].colorOnly)
    	    	continue;
	    *ptr++ = '('; *ptr++ = '?'; *ptr++ = ':';
    	    strcpy(ptr, patternSrc[subPatIndex].startRE);
    	    ptr += strlen(patternSrc[subPatIndex].startRE);
    	    *ptr++ = ')';
    	    *ptr++ = '|';
	    compiledPats[patternNum].nSubBranches++;
	}
	*(ptr-1) = '\0';
	compiledPats[patternNum].subPatternRE = CompileRE(bigPattern,
	    	&compileMsg, REDFLT_STANDARD);
	if (compiledPats[patternNum].subPatternRE == NULL) {
    	    fprintf(stderr, "Error compiling syntax highlight patterns:\n%s",
    	    	    compileMsg);
    	    return NULL;
	}
	NEditFree(bigPattern);
    }
    
    /* Copy remaining parameters from pattern template to compiled tree */
    for (i=0; i<nPatterns; i++)
        compiledPats[i].flags = patternSrc[i].flags;
        
    return compiledPats;
}

/*
** Free a pattern list and all of its allocated components
*/
static void freePatterns(highlightDataRec *patterns)
{
    int i;
    
    for (i=0; patterns[i].style!=0; i++) {
    	if (patterns[i].startRE != NULL)
    	    free((char *)patterns[i].startRE);
    	if (patterns[i].endRE != NULL)
    	    free((char *)patterns[i].endRE);
    	if (patterns[i].errorRE != NULL)
    	    free((char *)patterns[i].errorRE);
    	if (patterns[i].subPatternRE != NULL)
    	    free((char *)patterns[i].subPatternRE);
    }

    for (i=0; patterns[i].style!=0; i++) {
        NEditFree(patterns[i].subPatterns);
    }

    NEditFree(patterns);
}

/*
** Find the highlightPattern structure with a given name in the window.
*/
highlightPattern *FindPatternOfWindow(WindowInfo *window, char *name)
{
    windowHighlightData *hData = (windowHighlightData *)window->highlightData;
    patternSet *set;
    int i;

    if (hData && (set = hData->patternSetForWindow)) {
      for (i = 0; i < set->nPatterns; i++)
          if (strcmp(set->patterns[i].name, name) == 0)
              return &set->patterns[i];
    }
    return NULL;
}

/*
** Picks up the entry in the style buffer for the position (if any). Rather
** like styleOfPos() in textDisp.c. Returns the style code or zero.
*/
int HighlightCodeOfPos(WindowInfo *window, int pos)
{
    windowHighlightData *highlightData =
          (windowHighlightData *)window->highlightData;
    textBuffer *styleBuf =
          highlightData ? highlightData->styleBuffer : NULL;
    int hCode = 0;
    
    if (styleBuf != NULL) {
      hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
      if (hCode == UNFINISHED_STYLE) {
          /* encountered "unfinished" style, trigger parsing */
          handleUnparsedRegion(window, highlightData->styleBuffer, pos);
          hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
      }
    }
    return hCode;
}

/*
** Returns the length over which a particular highlight code applies, starting
** at pos. If the initial code value *checkCode is zero, the highlight code of
** pos is used.
*/
/* YOO: This is called form only one other function, which uses a constant
    for checkCode and never evaluates it after the call. */
int HighlightLengthOfCodeFromPos(WindowInfo *window, int pos, int *checkCode)
{
    windowHighlightData *highlightData =
          (windowHighlightData *)window->highlightData;
    textBuffer *styleBuf =
          highlightData ? highlightData->styleBuffer : NULL;
    int hCode = 0;
    int oldPos = pos;
    
    if (styleBuf != NULL) {
      hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
      if (!hCode)
          return 0;
      if (hCode == UNFINISHED_STYLE) {
          /* encountered "unfinished" style, trigger parsing */
          handleUnparsedRegion(window, highlightData->styleBuffer, pos);
          hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
      }
      if (*checkCode == 0)
          *checkCode = hCode;
      while (hCode == *checkCode || hCode == UNFINISHED_STYLE) {
          if (hCode == UNFINISHED_STYLE) {
              /* encountered "unfinished" style, trigger parsing, then loop */
              handleUnparsedRegion(window, highlightData->styleBuffer, pos);
              hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
          }
          else {
              /* advance the position and get the new code */
              hCode = (unsigned char)BufGetCharacter(styleBuf, ++pos);
          }
      }
    }
    return pos - oldPos;
}

/*
** Returns the length over which a particular style applies, starting at pos. 
** If the initial code value *checkCode is zero, the highlight code of pos 
** is used.
*/
int StyleLengthOfCodeFromPos(WindowInfo *window, int pos, 
                             const char **checkStyleName)
{
    windowHighlightData *highlightData =
          (windowHighlightData *)window->highlightData;
    textBuffer *styleBuf =
          highlightData ? highlightData->styleBuffer : NULL;
    int hCode = 0;
    int oldPos = pos;
    styleTableEntry *entry;
    
    if (styleBuf != NULL) {
      hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
      if (!hCode)
          return 0;
      if (hCode == UNFINISHED_STYLE) {
          /* encountered "unfinished" style, trigger parsing */
          handleUnparsedRegion(window, highlightData->styleBuffer, pos);
          hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
      }
      entry = styleTableEntryOfCode(window, hCode);
      if (entry == NULL) 
          return 0;
      if ((*checkStyleName) == NULL)
          (*checkStyleName) = entry->styleName;
      while (hCode == UNFINISHED_STYLE ||
             ((entry = styleTableEntryOfCode(window, hCode)) &&
                strcmp(entry->styleName, (*checkStyleName)) == 0)) {
          if (hCode == UNFINISHED_STYLE) {
              /* encountered "unfinished" style, trigger parsing, then loop */
              handleUnparsedRegion(window, highlightData->styleBuffer, pos);
              hCode = (unsigned char)BufGetCharacter(styleBuf, pos);
          }
          else {
              /* advance the position and get the new code */
              hCode = (unsigned char)BufGetCharacter(styleBuf, ++pos);
          }
      }
    }
    return pos - oldPos;
}

/*
** Returns a pointer to the entry in the style table for the entry of code
** hCode (if any).
*/
static styleTableEntry *styleTableEntryOfCode(WindowInfo *window, int hCode)
{
    windowHighlightData *highlightData =
          (windowHighlightData *)window->highlightData;

    hCode -= UNFINISHED_STYLE;       /* get the correct index value */
    if (!highlightData || hCode < 0 || hCode >= highlightData->nStyles)
      return NULL;
    return &highlightData->styleTable[hCode];
}

/*
** Functions to return style information from the highlighting style table.
*/

char *HighlightNameOfCode(WindowInfo *window, int hCode)
{
    styleTableEntry *entry = styleTableEntryOfCode(window, hCode);
    return entry ? entry->highlightName : "";
}

char *HighlightStyleOfCode(WindowInfo *window, int hCode)
{
    styleTableEntry *entry = styleTableEntryOfCode(window, hCode);
    return entry ? entry->styleName : "";
}

Pixel HighlightColorValueOfCode(WindowInfo *window, int hCode,
      int *r, int *g, int *b)
{
    styleTableEntry *entry = styleTableEntryOfCode(window, hCode);
    if (entry) {
        *r = entry->red;
        *g = entry->green;
        *b = entry->blue;
        return entry->color;
    }
    else
    {
        /* pick up foreground color of the (first) text widget of the window */
        XColor colorDef;
        Colormap cMap;
        Display *display = XtDisplay(window->textArea);
        *r = *g = *b = 0;
        XtVaGetValues(window->textArea,
                      XtNcolormap,   &cMap,
                      XtNforeground, &colorDef.pixel,
                      NULL);
        if (XQueryColor(display, cMap, &colorDef)) {
            *r = colorDef.red;
            *g = colorDef.green;
            *b = colorDef.blue;
        }
        return colorDef.pixel;
    }
}

Pixel GetHighlightBGColorOfCode(WindowInfo *window, int hCode,
      int *r, int *g, int *b)
{
    styleTableEntry *entry = styleTableEntryOfCode(window, hCode);
    if (entry && entry->bgColorName) {
        *r = entry->bgRed;
        *g = entry->bgGreen;
        *b = entry->bgBlue;
        return entry->bgColor;
    }
    else
    {
        /* pick up background color of the (first) text widget of the window */
        XColor colorDef;
        Colormap cMap;
        Display *display = XtDisplay(window->textArea);
        *r = *g = *b = 0;
        XtVaGetValues(window->textArea,
                      XtNcolormap,   &cMap,
                      XtNbackground, &colorDef.pixel,
                      NULL);
        if (XQueryColor(display, cMap, &colorDef)) {
            *r = colorDef.red;
            *g = colorDef.green;
            *b = colorDef.blue;
        }
        return colorDef.pixel;
    }
}

/*
** Callback to parse an "unfinished" region of the buffer.  "unfinished" means
** that the buffer has been parsed with pass 1 patterns, but this section has
** not yet been exposed, and thus never had pass 2 patterns applied.  This
** callback is invoked when the text widget's display routines encounter one
** of these unfinished regions.  "pos" is the first position encountered which
** needs re-parsing.  This routine applies pass 2 patterns to a chunk of
** the buffer of size PASS_2_REPARSE_CHUNK_SIZE beyond pos.
*/
static void handleUnparsedRegion(const WindowInfo* window, textBuffer* styleBuf,
        int pos)
{
    textBuffer *buf = window->buffer;
    int beginParse, endParse, beginSafety, endSafety, p;
    windowHighlightData *highlightData =
            (windowHighlightData *)window->highlightData;

    reparseContext *context = &highlightData->contextRequirements;
    highlightDataRec *pass2Patterns = highlightData->pass2Patterns;
    char *string, *styleString, *stylePtr, c, prevChar;
    const char *stringPtr;
    int firstPass2Style = (unsigned char)pass2Patterns[1].style;
    
    /* If there are no pass 2 patterns to process, do nothing (but this
       should never be triggered) */
    if (pass2Patterns == NULL)
    	return;
    
    /* Find the point at which to begin parsing to ensure that the character at
       pos is parsed correctly (beginSafety), at most one context distance back
       from pos, unless there is a pass 1 section from which to start */
    beginParse = pos;
    beginSafety = backwardOneContext(buf, context, beginParse);
    for (p=beginParse; p>=beginSafety; p--) {
    	c = BufGetCharacter(styleBuf, p);
    	if (c != UNFINISHED_STYLE && c != PLAIN_STYLE &&
		(unsigned char)c < firstPass2Style) {
    	    beginSafety = p + 1;
    	    break;
    	}
    }
    
    /* Decide where to stop (endParse), and the extra distance (endSafety)
       necessary to ensure that the changes at endParse are correct.  Stop at
       the end of the unfinished region, or a max. of PASS_2_REPARSE_CHUNK_SIZE
       characters forward from the requested position */
    endParse = min(buf->length, pos + PASS_2_REPARSE_CHUNK_SIZE);
    endSafety = forwardOneContext(buf, context, endParse);
    for (p=pos; p<endSafety; p++) {
    	c = BufGetCharacter(styleBuf, p);
    	if (c != UNFINISHED_STYLE && c != PLAIN_STYLE &&
		(unsigned char)c < firstPass2Style) {
    	    endParse = min(endParse, p);
    	    endSafety = p;
    	    break;
    	} else if (c != UNFINISHED_STYLE && p < endParse) {
    	    endParse = p;
    	    if ((unsigned char)c < firstPass2Style)
    	    	endSafety = p;
    	    else
    	    	endSafety = forwardOneContext(buf, context, endParse);
    	    break;
    	}
    }
    
    /* Copy the buffer range into a string */
    /* printf("callback pass2 parsing from %d thru %d w/ safety from %d thru %d\n",
    	    beginParse, endParse, beginSafety, endSafety); */
    stringPtr = string = BufGetRange(buf, beginSafety, endSafety);
    styleString = stylePtr = BufGetRange(styleBuf, beginSafety, endSafety);
    
    /* Parse it with pass 2 patterns */
    prevChar = getPrevChar(buf, beginSafety);
    parseString(pass2Patterns, &stringPtr, &stylePtr, endParse - beginSafety,
    	    &prevChar, False, GetWindowDelimiters(window), string, NULL);

    /* Update the style buffer the new style information, but only between
       beginParse and endParse.  Skip the safety region */
    styleString[endParse-beginSafety] = '\0';
    BufReplace(styleBuf, beginParse, endParse,
    	    &styleString[beginParse-beginSafety]);
    NEditFree(styleString);
    NEditFree(string);    
}

/*
** Callback wrapper around the above function.
*/
static void handleUnparsedRegionCB(const textDisp* textD, int pos,
        const void* cbArg)
{
    handleUnparsedRegion((WindowInfo*) cbArg, textD->styleBuffer, pos);
}

/*
** Re-parse the smallest region possible around a modification to buffer "buf"
** to gurantee that the promised context lines and characters have
** been presented to the patterns.  Changes the style buffer in "highlightData"
** with the parsing result.
*/
static void incrementalReparse(windowHighlightData *highlightData,
    	textBuffer *buf, int pos, int nInserted, const char *delimiters)
{
    int beginParse, endParse, endAt, lastMod, parseInStyle, nPasses;
    textBuffer *styleBuf = highlightData->styleBuffer;
    highlightDataRec *pass1Patterns = highlightData->pass1Patterns;
    highlightDataRec *pass2Patterns = highlightData->pass2Patterns;
    highlightDataRec *startPattern;
    reparseContext *context = &highlightData->contextRequirements;
    char *parentStyles = highlightData->parentStyles;

    /* Find the position "beginParse" at which to begin reparsing.  This is
       far enough back in the buffer such that the guranteed number of
       lines and characters of context are examined. */
    beginParse = pos;
    parseInStyle = findSafeParseRestartPos(buf, highlightData, &beginParse);

    /* Find the position "endParse" at which point it is safe to stop
       parsing, unless styles are getting changed beyond the last
       modification */
    lastMod = pos + nInserted;
    endParse = forwardOneContext(buf, context, lastMod);
    
    /*
    ** Parse the buffer from beginParse, until styles compare
    ** with originals for one full context distance.  Distance increases
    ** by powers of two until nothing changes from previous step.  If
    ** parsing ends before endParse, start again one level up in the
    ** pattern hierarchy
    */
    for (nPasses=0; ; nPasses++) {
	
	/* Parse forward from beginParse to one context beyond the end
	   of the last modification */
        startPattern = patternOfStyle(pass1Patterns, parseInStyle);
        /* If there is no pattern matching the style, it must be a pass-2
           style. It that case, it is (probably) safe to start parsing with
           the root pass-1 pattern again. Anyway, passing a NULL-pointer to
           the parse routine would result in a crash; restarting with pass-1 
           patterns is certainly preferable, even if there is a slight chance 
           of a faulty coloring. */
	if (!startPattern) {
	    startPattern = pass1Patterns;
	}
    	endAt = parseBufferRange(startPattern,
    	    	pass2Patterns, buf, styleBuf, context, beginParse, endParse,
		delimiters);
	
	/* If parse completed at this level, move one style up in the
	   hierarchy and start again from where the previous parse left off. */
	if (endAt < endParse) {
	    beginParse = endAt;
	    endParse = forwardOneContext(buf, context,
	    	    max(endAt, max(lastModified(styleBuf), lastMod)));
	    if (IS_PLAIN(parseInStyle)) {
		fprintf(stderr,
			"NEdit internal error: incr. reparse fell short\n");
		return;
	    }
	    parseInStyle = parentStyleOf(parentStyles, parseInStyle);
	    
	/* One context distance beyond last style changed means we're done */
	} else if (lastModified(styleBuf) <= lastMod) {
	    return;
	    
	/* Styles are changing beyond the modification, continue extending
	   the end of the parse range by powers of 2 * REPARSE_CHUNK_SIZE and
	   reparse until nothing changes */
	} else {
	    lastMod = lastModified(styleBuf);
    	    endParse = min(buf->length, forwardOneContext(buf, context, lastMod)
    	    	    + (REPARSE_CHUNK_SIZE << nPasses));
	}
    }	
}

/*
** Parse text in buffer "buf" between positions "beginParse" and "endParse"
** using pass 1 patterns over the entire range and pass 2 patterns where needed
** to determine whether re-parsed areas have changed and need to be redrawn.
** Deposits style information in "styleBuf" and expands the selection in
** styleBuf to show the additional areas which have changed and need
** redrawing.  beginParse must be a position from which pass 1 parsing may
** safely be started using the pass1Patterns given.  Internally, adds a
** "takeoff" safety region before beginParse, so that pass 2 patterns will be
** allowed to match properly if they begin before beginParse, and a "landing"
** safety region beyond endparse so that endParse is guranteed to be parsed
** correctly in both passes.  Returns the buffer position at which parsing
** finished (this will normally be endParse, unless the pass1Patterns is a
** pattern which does end and the end is reached).
*/
static int parseBufferRange(highlightDataRec *pass1Patterns,
    	highlightDataRec *pass2Patterns, textBuffer *buf, textBuffer *styleBuf,
        reparseContext *contextRequirements, int beginParse, int endParse,
        const char *delimiters)
{
    char *string, *styleString, *stylePtr, *temp, prevChar;
    const char *stringPtr;
    int endSafety, endPass2Safety, startPass2Safety, tempLen;
    int modStart, modEnd, beginSafety, beginStyle, p, style;
    int firstPass2Style = pass2Patterns == NULL ? INT_MAX :
	    (unsigned char)pass2Patterns[1].style;
    
    /* Begin parsing one context distance back (or to the last style change) */
    beginStyle = pass1Patterns->style;
    if (CAN_CROSS_LINE_BOUNDARIES(contextRequirements)) {
    	beginSafety = backwardOneContext(buf, contextRequirements, beginParse);
     	for (p=beginParse; p>=beginSafety; p--) {
    	    style = BufGetCharacter(styleBuf, p-1);
    	    if (!EQUIVALENT_STYLE(style, beginStyle, firstPass2Style)) {
    	    	beginSafety = p;
    	    	break;
    	    }
    	}
    } else {
    	for (beginSafety=max(0,beginParse-1); beginSafety>0; beginSafety--) {
    	    style = BufGetCharacter(styleBuf, beginSafety);
    	    if (!EQUIVALENT_STYLE(style, beginStyle, firstPass2Style) ||
    	    	    BufGetCharacter(buf, beginSafety) == '\n') {
    	    	beginSafety++;
    	    	break;
    	    }
    	}
    }
    
    /* Parse one parse context beyond requested end to gurantee that parsing
       at endParse is complete, unless patterns can't cross line boundaries,
       in which case the end of the line is fine */
    if (endParse == 0)
    	return 0;
    if (CAN_CROSS_LINE_BOUNDARIES(contextRequirements))
    	endSafety = forwardOneContext(buf, contextRequirements, endParse);
    else if (endParse>=buf->length || (BufGetCharacter(buf,endParse-1)=='\n'))
    	endSafety = endParse;
    else
    	endSafety = min(buf->length, BufEndOfLine(buf, endParse) + 1);
    
    /* copy the buffer range into a string */
    string = BufGetRange(buf, beginSafety, endSafety);
    styleString = BufGetRange(styleBuf, beginSafety, endSafety);
    
    /* Parse it with pass 1 patterns */
    /* printf("parsing from %d thru %d\n", beginSafety, endSafety); */
    prevChar = getPrevChar(buf, beginParse);
    stringPtr = &string[beginParse-beginSafety];
    stylePtr = &styleString[beginParse-beginSafety];
    parseString(pass1Patterns, &stringPtr, &stylePtr, endParse-beginParse,
    	    &prevChar, False, delimiters, string, NULL);

    /* On non top-level patterns, parsing can end early */
    endParse = min(endParse, stringPtr-string + beginSafety);
    
    /* If there are no pass 2 patterns, we're done */
    if (pass2Patterns == NULL)
    	goto parseDone;
    
    /* Parsing of pass 2 patterns is done only as necessary for determining
       where styles have changed.  Find the area to avoid, which is already
       marked as changed (all inserted text and previously modified areas) */
    if (styleBuf->primary.selected) {
	modStart = styleBuf->primary.start;
	modEnd = styleBuf->primary.end;
    } else
    	modStart = modEnd = 0;
    
    /* Re-parse the areas before the modification with pass 2 patterns, from
       beginSafety to far enough beyond modStart to gurantee that parsing at
       modStart is correct (pass 2 patterns must match entirely within one
       context distance, and only on the top level).  If the parse region
       ends entirely before the modification or at or beyond modEnd, parse
       the whole thing and take advantage of the safety region which will be
       thrown away below.  Otherwise save the contents of the safety region
       temporarily, and restore it after the parse. */
    if (beginSafety < modStart) {
	if (endSafety > modStart) {
	    endPass2Safety = forwardOneContext(buf, contextRequirements,
	    	    modStart);
	    if (endPass2Safety + PASS_2_REPARSE_CHUNK_SIZE >= modEnd)
	    	endPass2Safety = endSafety;
    	} else
    	    endPass2Safety = endSafety;
	prevChar = getPrevChar(buf, beginSafety);
	if (endPass2Safety == endSafety) {
	    passTwoParseString(pass2Patterns, string, styleString,
                    endParse - beginSafety, &prevChar, delimiters, string, NULL);
	    goto parseDone;
	} else {
	    tempLen = endPass2Safety - modStart;
	    temp = (char*)NEditMalloc(tempLen);
	    strncpy(temp, &styleString[modStart-beginSafety], tempLen);
	    passTwoParseString(pass2Patterns, string, styleString,
                    modStart - beginSafety, &prevChar, delimiters, string, NULL);
	    strncpy(&styleString[modStart-beginSafety], temp, tempLen);
	    NEditFree(temp);
	}
    }
    
    /* Re-parse the areas after the modification with pass 2 patterns, from
       modEnd to endSafety, with an additional safety region before modEnd
       to ensure that parsing at modEnd is correct. */
    if (endParse > modEnd) {
	if (beginSafety > modEnd) {
	    prevChar = getPrevChar(buf, beginSafety);
	    passTwoParseString(pass2Patterns, string, styleString,
                    endParse - beginSafety, &prevChar, delimiters, string, NULL);
	} else {
	    startPass2Safety = max(beginSafety,
	    	    backwardOneContext(buf, contextRequirements, modEnd));
	    tempLen = modEnd - startPass2Safety;
	    temp = (char*)NEditMalloc(tempLen);
	    strncpy(temp, &styleString[startPass2Safety-beginSafety], tempLen);
	    prevChar = getPrevChar(buf, startPass2Safety);
	    passTwoParseString(pass2Patterns,
                    &string[startPass2Safety-beginSafety],
                    &styleString[startPass2Safety-beginSafety],
                    endParse-startPass2Safety, &prevChar, delimiters, string,
                    NULL);
	    strncpy(&styleString[startPass2Safety-beginSafety], temp, tempLen);
	    NEditFree(temp);
	}
    }
    	
parseDone:

    /* Update the style buffer with the new style information, but only
       through endParse.  Skip the safety region at the end */
    styleString[endParse-beginSafety] = '\0';
    modifyStyleBuf(styleBuf, &styleString[beginParse-beginSafety],
    	    beginParse, endParse, firstPass2Style);
    NEditFree(styleString);
    NEditFree(string);
    
    return endParse;
}

/*
** Parses "string" according to compiled regular expressions in "pattern"
** until endRE is or errorRE are matched, or end of string is reached.
** Advances "string", "styleString" pointers to the next character past
** the end of the parsed section, and updates "prevChar" to reflect
** the new character before "string".
** If "anchored" is true, just scan the sub-pattern starting at the beginning
** of the string.  "length" is how much of the string must be parsed, but
** "string" must still be null terminated, the termination indicating how
** far the string should be searched, and "length" the part which is actually
** required (the string may or may not be parsed beyond "length").
**
** "lookBehindTo" indicates the boundary till where look-behind patterns may
** look back. If NULL, the start of the string is assumed to be the boundary.
**
** "match_till" indicates the boundary till where matches may extend. If NULL,
** it is assumed that the terminating \0 indicates the boundary. Note that
** look-ahead patterns can peek beyond the boundary, if supplied.
**
** Returns True if parsing was done and the parse succeeded.  Returns False if
** the error pattern matched, if the end of the string was reached without
** matching the end expression, or in the unlikely event of an internal error.
*/
static int parseString(highlightDataRec *pattern, const char **string,
    	char **styleString, int length, char *prevChar, int anchored,
    	const char *delimiters, const char* lookBehindTo, 
    	const char* match_till)
{
    int i, subExecuted, subIndex;
    char *stylePtr;
    const char *stringPtr, *savedStartPtr, *startingStringPtr;
    signed char *subExpr;
    char savedPrevChar;
    char succChar = match_till ? (*match_till) : '\0';
    highlightDataRec *subPat = NULL, *subSubPat;
    
    if (length <= 0)
    	return False;

    stringPtr = *string;
    stylePtr = *styleString;
    
    while (ExecRE(pattern->subPatternRE, stringPtr, anchored ? *string+1 :
	    *string+length+1, False, *prevChar, succChar, delimiters,
            lookBehindTo, match_till)) {
	/* Beware of the case where only one real branch exists, but that 
	   branch has sub-branches itself. In that case the top_branch refers 
	   to the matching sub-branch and must be ignored. */
	subIndex = (pattern->nSubBranches > 1) ? 
	   pattern->subPatternRE->top_branch : 0;
    	/* Combination of all sub-patterns and end pattern matched */
    	/* printf("combined patterns RE matched at %d\n",
    	    	pattern->subPatternRE->startp[0] - *string); */
	startingStringPtr = stringPtr;
	
	/* Fill in the pattern style for the text that was skipped over before
	   the match, and advance the pointers to the start of the pattern */
	fillStyleString(&stringPtr, &stylePtr, pattern->subPatternRE->startp[0],
	    	pattern->style, prevChar);
    	
    	/* If the combined pattern matched this pattern's end pattern, we're
    	   done.  Fill in the style string, update the pointers, color the
	   end expression if there were coloring sub-patterns, and return */
	savedStartPtr = stringPtr;
	savedPrevChar = *prevChar;
	if (pattern->endRE != NULL) {
	    if (subIndex == 0) {
		fillStyleString(&stringPtr, &stylePtr, 
		    pattern->subPatternRE->endp[0], pattern->style, prevChar);
		subExecuted = False;
		for (i=0;i<pattern->nSubPatterns; i++) {
		    subPat = pattern->subPatterns[i];
		    if (subPat->colorOnly) {
			if (!subExecuted) { 
                            if (!ExecRE(pattern->endRE, savedStartPtr,
				savedStartPtr+1, False, savedPrevChar, 
				succChar, delimiters, lookBehindTo, match_till)) {
				fprintf(stderr, "Internal error, failed to "
					"recover end match in parseString\n");
				return False;
			    }
			    subExecuted = True;
			}
			for (subExpr=subPat->endSubexprs; *subExpr!=-1; subExpr++)
		    	    recolorSubexpr(pattern->endRE, *subExpr, 
				subPat->style, *string, *styleString); 
    	    	    }
		}
    		*string = stringPtr;
		*styleString = stylePtr;
		return True;
	    }
	    --subIndex;
    	}
    	
    	/* If the combined pattern matched this pattern's error pattern, we're
    	   done.  Fill in the style string, update the pointers, and return */
    	if (pattern->errorRE != NULL) {
	    if (subIndex == 0) {
		fillStyleString(&stringPtr, &stylePtr, 
		    pattern->subPatternRE->startp[0], pattern->style, prevChar);
    		    *string = stringPtr;
		*styleString = stylePtr;
		return False;
	    }
	    --subIndex;
    	}
    	
    	/* Figure out which sub-pattern matched */
    	for (i=0; i<pattern->nSubPatterns; i++) {
	    subPat = pattern->subPatterns[i];
	    if (subPat->colorOnly) ++subIndex;
	    else if (i == subIndex) break;
	}
    	if (i == pattern->nSubPatterns) {
    	    fprintf(stderr, "Internal error, failed to match in parseString\n");
    	    return False;
    	}
    	
    	/* the sub-pattern is a simple match, just color it */
    	if (subPat->subPatternRE == NULL) {
    	    fillStyleString(&stringPtr, &stylePtr, pattern->subPatternRE->endp[0], /* subPat->startRE->endp[0],*/
    	    	    subPat->style, prevChar);
    	
    	/* Parse the remainder of the sub-pattern */	    
    	} else if (subPat->endRE != NULL) {
            /* The pattern is a starting/ending type of pattern, proceed with
               the regular hierarchical parsing. */
           
    	    /* If parsing should start after the start pattern, advance
    	       to that point (this is currently always the case) */
    	    if (!(subPat->flags & PARSE_SUBPATS_FROM_START))
    		fillStyleString(&stringPtr, &stylePtr, pattern->subPatternRE->endp[0], /* subPat->startRE->endp[0],*/
			subPat->style, prevChar);

   	    /* Parse to the end of the subPattern */
   	    parseString(subPat, &stringPtr, &stylePtr, length -
   	    	    (stringPtr - *string), prevChar, False, delimiters,
                    lookBehindTo, match_till);
    	} else {
    	    /* If the parent pattern is not a start/end pattern, the
               sub-pattern can between the boundaries of the parent's 
               match. Note that we must limit the recursive matches such
               that they do not exceed the parent's ending boundary. 
               Without that restriction, matching becomes unstable. */
           
   	    /* Parse to the end of the subPattern */
   	    parseString(subPat, &stringPtr, &stylePtr, 
                pattern->subPatternRE->endp[0]-stringPtr, prevChar, False, 
                delimiters, lookBehindTo, pattern->subPatternRE->endp[0]);
    	}
    	
    	/* If the sub-pattern has color-only sub-sub-patterns, add color
	   based on the coloring sub-expression references */
	subExecuted = False;
    	for (i=0; i<subPat->nSubPatterns; i++) {
	    subSubPat = subPat->subPatterns[i];
	    if (subSubPat->colorOnly) {
		if (!subExecuted) { 
                   if (!ExecRE(subPat->startRE, savedStartPtr, 
			savedStartPtr+1, False, savedPrevChar, succChar,
			delimiters, lookBehindTo, match_till)) {
			fprintf(stderr, "Internal error, failed to recover "
					"start match in parseString\n");
			return False;
		    }
		    subExecuted = True; 
		}
	    	for (subExpr=subSubPat->startSubexprs; *subExpr!=-1; subExpr++)
	    	    recolorSubexpr(subPat->startRE, *subExpr, subSubPat->style,
	    	    	    *string, *styleString);
	    }
	}
	
	/* Make sure parsing progresses.  If patterns match the empty string,
	   they can get stuck and hang the process */
	if (stringPtr == startingStringPtr) {
	    /* Avoid stepping over the end of the string (possible for
               zero-length matches at end of the string) */
	    if (*stringPtr == '\0')
		break;
	    fillStyleString(&stringPtr, &stylePtr, stringPtr+1,
			pattern->style, prevChar);
	}
    }
    
    /* If this is an anchored match (must match on first character), and
       nothing matched, return False */
    if (anchored && stringPtr == *string)
    	return False;
    
    /* Reached end of string, fill in the remaining text with pattern style
       (unless this was an anchored match) */
    if (!anchored)
        fillStyleString(&stringPtr, &stylePtr, *string+length, pattern->style,
                prevChar);
    
    /* Advance the string and style pointers to the end of the parsed text */
    *string = stringPtr;
    *styleString = stylePtr;
    return pattern->endRE == NULL;
}

/*
** Takes a string which has already been parsed through pass1 parsing and
** re-parses the areas where pass two patterns are applicable.  Parameters
** have the same meaning as in parseString, except that strings aren't doubly
** indirect and string pointers are not updated.
*/
static void passTwoParseString(highlightDataRec *pattern, char *string,
        char *styleString, int length, char *prevChar, const char *delimiters,
        const char *lookBehindTo, const char *match_till)
{
    int inParseRegion = False;
    char *stylePtr, temp, *parseStart = NULL, *parseEnd, *s, *c;
    const char *stringPtr;
    int firstPass2Style = (unsigned char)pattern[1].style;
    
    for (c = string, s = styleString; ; c++, s++) {
    	if (!inParseRegion && *c != '\0' && (*s == UNFINISHED_STYLE ||
    	    	*s == PLAIN_STYLE || (unsigned char)*s >= firstPass2Style)) {
    	    parseStart = c;
    	    inParseRegion = True;
    	}
    	if (inParseRegion && (*c == '\0' || !(*s == UNFINISHED_STYLE ||
    	    	*s == PLAIN_STYLE || (unsigned char)*s >= firstPass2Style))) {
    	    parseEnd = c;
	    if (parseStart != string)
	    	*prevChar = *(parseStart-1);
	    stringPtr = parseStart;
	    stylePtr = &styleString[parseStart - string];
    	    temp = *parseEnd;
    	    *parseEnd = '\0';
    	    /* printf("pass2 parsing %d chars\n", strlen(stringPtr)); */
    	    parseString(pattern, &stringPtr, &stylePtr,
    	    	    min(parseEnd - parseStart, length - (parseStart - string)),
    	    	    prevChar, False, delimiters, lookBehindTo, match_till);
    	    *parseEnd = temp;
    	    inParseRegion = False;
    	}
    	if (*c == '\0' || (!inParseRegion && c - string >= length))
    	    break;
    }
}

/*
** Advance "stringPtr" and "stylePtr" until "stringPtr" == "toPtr", filling
** "stylePtr" with style "style".  Can also optionally update the pre-string
** character, prevChar, which is fed to regular the expression matching
** routines for determining word and line boundaries at the start of the string.
*/
static void fillStyleString(const char **stringPtr, char **stylePtr,
    	const char *toPtr, char style, char *prevChar)
{
    int i, len = toPtr-*stringPtr;
    
    if (*stringPtr >= toPtr)
    	return;
    	
    for (i=0; i<len; i++)
    	*(*stylePtr)++ = style;
    if (prevChar != NULL) *prevChar = *(toPtr-1);
    *stringPtr = toPtr;
}

/*
** Incorporate changes from styleString into styleBuf, tracking changes
** in need of redisplay, and marking them for redisplay by the text
** modification callback in textDisp.c.  "firstPass2Style" is necessary
** for distinguishing pass 2 styles which compare as equal to the unfinished
** style in the original buffer, from pass1 styles which signal a change.
*/
static void modifyStyleBuf(textBuffer *styleBuf, char *styleString,
    	int startPos, int endPos, int firstPass2Style)
{
    char *c, bufChar;
    int pos, modStart, modEnd, minPos = INT_MAX, maxPos = 0;
    selection *sel = &styleBuf->primary;
    
    /* Skip the range already marked for redraw */
    if (sel->selected) {
	modStart = sel->start;
	modEnd = sel->end;
    } else
    	modStart = modEnd = startPos;
    
    /* Compare the original style buffer (outside of the modified range) with
       the new string with which it will be updated, to find the extent of
       the modifications.  Unfinished styles in the original match any
       pass 2 style */
    for (c=styleString, pos=startPos; pos<modStart && pos<endPos; c++, pos++) {
    	bufChar = BufGetCharacter(styleBuf, pos);
    	if (*c != bufChar && !(bufChar == UNFINISHED_STYLE &&
    	    	(*c == PLAIN_STYLE || (unsigned char)*c >= firstPass2Style))) {
    	    if (pos < minPos) minPos = pos;
    	    if (pos > maxPos) maxPos = pos;
    	}
    }
    for (c=&styleString[max(0, modEnd-startPos)], pos=max(modEnd, startPos);
    	    pos<endPos; c++, pos++) {
    	bufChar = BufGetCharacter(styleBuf, pos);
    	if (*c != bufChar && !(bufChar == UNFINISHED_STYLE &&
    	    	(*c == PLAIN_STYLE || (unsigned char)*c >= firstPass2Style))) {
    	    if (pos < minPos) minPos = pos;
    	    if (pos+1 > maxPos) maxPos = pos+1;
    	}
    }
    
    /* Make the modification */
    BufReplace(styleBuf, startPos, endPos, styleString);
    
    /* Mark or extend the range that needs to be redrawn.  Even if no
       change was made, it's important to re-establish the selection,
       because it can get damaged by the BufReplace above */
    BufSelect(styleBuf, min(modStart, minPos), max(modEnd, maxPos));
}

/*
** Return the last modified position in styleBuf (as marked by modifyStyleBuf
** by the convention used for conveying modification information to the
** text widget, which is selecting the text)
*/
static int lastModified(textBuffer *styleBuf)
{
    if (styleBuf->primary.selected)
    	return max(0, styleBuf->primary.end);
    return 0;
}

/*
** Compute the distance between two colors.
*/

static double colorDistance(const XColor *c1, const XColor *c2)
{
    /* This is done in RGB space, which is close, but not optimal.  It's
       probably better to do it in HSV or YIQ space, however, that means
       a whole lot of extra conversions.  This would allow us to weight
       the coordinates differently, e.g, prefer to match hue over
       brightness. */

    static const double scale = 65535;

    double tred   = c1->red   / scale - c2->red   / scale;
    double tgreen = c1->green / scale - c2->green / scale;
    double tblue  = c1->blue  / scale - c2->blue  / scale;

    /* use square Euclidian distance */
    return tred * tred + tgreen * tgreen + tblue * tblue;
}

/*
** use this canned function to call AllocColor() when
** the r, g & b components is not needed, thus saving
** the little hassle of creating the dummy variable.
*/
Pixel AllocateColor(Widget w, const char *colorName)
{
    int dummy;
    
    return AllocColor(w, colorName, &dummy, &dummy, &dummy);
}

/*
** Allocate a read-only (shareable) colormap cell for a named color, from the
** the default colormap of the screen on which the widget (w) is displayed. If
** the colormap is full and there's no suitable substitute, print an error on
** stderr, and return the widget's foreground color as a backup.
*/

Pixel AllocColor(Widget w, const char *colorName, int *r, int *g, int *b)
{
    XColor       colorDef;
    XColor      *allColorDefs;
    Display     *display = XtDisplay(w);
    Colormap     cMap;
    Pixel        foreground, bestPixel;
    double       small = 1.0e9;
    int          depth;
    unsigned int ncolors;
    unsigned long i, best = 0;    /* pixel value */
    
    /* Get the correct colormap for compatability with the "best" visual
       feature in 5.2.  Default visual of screen is no good here. */

    XtVaGetValues(w,
                  XtNcolormap,   &cMap,
                  XtNdepth,      &depth,
                  XtNforeground, &foreground,
                  NULL);

    bestPixel = foreground; /* Our last fallback */

    /* First, check for valid syntax */        
    if (! XParseColor(display, cMap, colorName, &colorDef)) {
        fprintf(stderr, "NEdit: Color name %s not in database\n",  colorName);
        colorDef.pixel = foreground;
        if (XQueryColor(display, cMap, &colorDef)) {
            *r = colorDef.red;
	    *g = colorDef.green;
	    *b = colorDef.blue;
        }
        return foreground;
    }

    /* Attempt allocation of the exact color. */
    if (XAllocColor(display, cMap, &colorDef)) {
	*r = colorDef.red;
	*g = colorDef.green;
	*b = colorDef.blue;
        return colorDef.pixel;
    }

    /* ---------- Allocation failed, the colormap may be full. ---------- */

#if 0
    printf("Couldn't allocate %d %d %d\n", colorDef.red, colorDef.green, colorDef.blue);
#endif
 
    /* We can't do the nearest-match on other than 8 bit visuals because
       it just takes too long.  */

    if (depth > 8) {             /* Oh no! */
        colorDef.pixel = foreground;
        if (XQueryColor(display, cMap, &colorDef)) {
	    *r = colorDef.red;
	    *g = colorDef.green;
	    *b = colorDef.blue;
        }
        return foreground;
    }

    /* Get the entire colormap so we can find the closest one. */
    ncolors = (1 << depth);
    allColorDefs = malloc(ncolors * sizeof(XColor));
    memset(allColorDefs, 0, ncolors * sizeof(XColor));
    
    for (i = 0; i < ncolors; i++)
        allColorDefs[i].pixel = i;

    XQueryColors(display, cMap, allColorDefs, ncolors);

    /* Scan through each color, looking for the closest one. */
    for (i = 0; i < ncolors; i++)
    {
        double dist = colorDistance(&allColorDefs[i], &colorDef);

        if (dist < small)
        {
            best = i;
            small = dist;
        }
    }

    /* Legally try to acquire the shared color- we should loop through
       the shortest distances here.  We could sort the map in order
       of decreasing distances and loop through it until one works. */

    if (XAllocColor(display, cMap, &allColorDefs[best]))
        bestPixel = allColorDefs[best].pixel;

#if 0
    printf("Got %d %d %d, ", allColorDefs[best].red,
                             allColorDefs[best].green,
                             allColorDefs[best].blue);
    printf("That's %f off\n", small);
#endif

    *r = allColorDefs[best].red;
    *g = allColorDefs[best].green;
    *b = allColorDefs[best].blue;
    free(allColorDefs);
    return bestPixel;
}

/*
** Get the character before position "pos" in buffer "buf"
*/
static char getPrevChar(textBuffer *buf, int pos)
{
    return pos == 0 ? '\0' : BufGetCharacter(buf, pos-1);
}

/*
** compile a regular expression and present a user friendly dialog on failure.
*/
static regexp *compileREAndWarn(Widget parent, const char *re)
{
    regexp *compiledRE;
    char *compileMsg;
    
    compiledRE = CompileRE(re, &compileMsg, REDFLT_STANDARD);
    if (compiledRE == NULL)
    {
        char *boundedRe = NEditStrdup(re);
        size_t maxLength = DF_MAX_MSG_LENGTH - strlen(compileMsg) - 60;

        /* Prevent buffer overflow in DialogF. If the re is too long, 
        truncate it and append ... */
        if (strlen(boundedRe) > maxLength)
        {
            strcpy(&boundedRe[maxLength-3], "...");
        }

        DialogF(DF_WARN, parent, 1, "Error in Regex",
                "Error in syntax highlighting regular expression:\n%s\n%s",
                "OK", boundedRe, compileMsg);
        NEditFree(boundedRe);
        return NULL;
    }
    return compiledRE;
}

static int parentStyleOf(const char *parentStyles, int style)
{
    return parentStyles[(unsigned char)style-UNFINISHED_STYLE];
}

static int isParentStyle(const char *parentStyles, int style1, int style2)
{
    int p;
    
    for (p = parentStyleOf(parentStyles, style2); p != '\0';
	    p = parentStyleOf(parentStyles, p))
	if (style1 == p)
	    return TRUE;
    return FALSE;
}

/*
** Discriminates patterns which can be used with parseString from those which
** can't.  Leaf patterns are not suitable for parsing, because patterns
** contain the expressions used for parsing within the context of their own
** operation, i.e. the parent pattern initiates, and leaf patterns merely
** confirm and color.  Returns TRUE if the pattern is suitable for parsing.
*/
static int patternIsParsable(highlightDataRec *pattern)
{
    return pattern != NULL && pattern->subPatternRE != NULL;
}

/*
** Back up position pointed to by "pos" enough that parsing from that point
** on will satisfy context gurantees for pattern matching for modifications
** at pos.  Returns the style with which to begin parsing.  The caller is
** guranteed that parsing may safely BEGIN with that style, but not that it
** will continue at that level.
**
** This routine can be fooled if a continuous style run of more than one
** context distance in length is produced by multiple pattern matches which
** abut, rather than by a single continuous match.  In this  case the
** position returned by this routine may be a bad starting point which will
** result in an incorrect re-parse.  However this will happen very rarely,
** and, if it does, is unlikely to result in incorrect highlighting.
*/
static int findSafeParseRestartPos(textBuffer *buf,
    	windowHighlightData *highlightData, int *pos)
{
    int style, startStyle, runningStyle, checkBackTo, safeParseStart, i;
    char *parentStyles = highlightData->parentStyles;
    highlightDataRec *pass1Patterns = highlightData->pass1Patterns;
    reparseContext *context = &highlightData->contextRequirements;
    
    /* We must begin at least one context distance back from the change */
    *pos = backwardOneContext(buf, context, *pos);
    
    /* If the new position is outside of any styles or at the beginning of
       the buffer, this is a safe place to begin parsing, and we're done */
    if (*pos == 0)
    	return PLAIN_STYLE;
    startStyle = BufGetCharacter(highlightData->styleBuffer, *pos);
    if (IS_PLAIN(startStyle))
    	return PLAIN_STYLE;
    
    /*
    ** The new position is inside of a styled region, meaning, its pattern
    ** could potentially be affected by the modification.
    **
    ** Follow the style back by enough context to ensure that if we don't find
    ** its beginning, at least we've found a safe place to begin parsing
    ** within the styled region.
    **
    ** A safe starting position within a style is either at a style
    ** boundary, or far enough from the beginning and end of the style run
    ** to ensure that it's not within the start or end expression match
    ** (unfortunately, abutting styles can produce false runs so we're not
    ** really ensuring it, just making it likely).
    */ 
    if (patternIsParsable(patternOfStyle(pass1Patterns, startStyle))) {
    	safeParseStart = backwardOneContext(buf, context, *pos);
    	checkBackTo = backwardOneContext(buf, context, safeParseStart);
    } else {
	safeParseStart = 0;
	checkBackTo = 0;
    }
    runningStyle = startStyle;
    for (i = *pos-1; ; i--) {
    	
    	/* The start of the buffer is certainly a safe place to parse from */
    	if (i == 0) {
    	    *pos = 0;
    	    return PLAIN_STYLE;
    	}
    	
    	/* If the style is preceded by a parent style, it's safe to parse
	   with the parent style, provided that the parent is parsable. */
    	style = BufGetCharacter(highlightData->styleBuffer, i);
	if (isParentStyle(parentStyles, style, runningStyle)) {
	    if (patternIsParsable(patternOfStyle(pass1Patterns, style))) {
		*pos = i + 1;
		return style;
	    } else {
		/* The parent is not parsable, so well have to continue
		   searching. The parent now becomes the running style. */
	    	runningStyle = style;
	    }
    	}
	
	/* If the style is preceded by a child style, it's safe to resume
	   parsing with the running style, provided that the running
	   style is parsable. */
    	else if (isParentStyle(parentStyles, runningStyle, style)) {
	    if (patternIsParsable
		   (patternOfStyle(pass1Patterns, runningStyle))) {
		*pos = i + 1;
		return runningStyle;
	    }
	    /* Else: keep searching; it's no use switching to the child style
	       because even the running one is not parsable. */
    	}
	
	/* If the style is preceded by a sibling style, it's safe to resume
	   parsing with the common ancestor style, provided that the ancestor
	   is parsable. Checking for siblings is very hard; checking whether
	   the style has the same parent will probably catch 99% of the cases
	   in practice. */
	else if (runningStyle != style &&
	    isParentStyle(parentStyles,
			  parentStyleOf(parentStyles, runningStyle), style)) {
	    int parentStyle = parentStyleOf(parentStyles, runningStyle);
	    if (patternIsParsable(patternOfStyle(pass1Patterns, parentStyle))) {
		*pos = i + 1;
		return parentStyle;
	    } else {
		/* Switch to the new style */
		runningStyle = style;
	    }
    	}

	/* If the style is preceded by an unrelated style, it's safe to
	   resume parsing with PLAIN_STYLE. (Actually, it isn't, because
	   we didn't really check for all possible sibling relations; but
	   it will be ok in practice.) */
	else if (runningStyle != style) {
	    *pos = i + 1;
    	    return PLAIN_STYLE;
    	}
	
	/* If the style is parsable and didn't change for one whole context
	   distance on either side of safeParseStart, safeParseStart is a
	   reasonable guess at a place to start parsing.
	   Note: No 'else' here! We may come from one of the 'fall-through
	   cases' above. */
    	if (i == checkBackTo) {
    	    *pos = safeParseStart;

	    /* We should never return a non-parsable style, because it will
	       result in an internal error. If the current style is not
	       parsable, the pattern set most probably contains a context
	       distance violation. In that case we can only avoid internal
	       errors (by climbing the pattern hierarchy till we find a
	       parsable ancestor) and hope that the highlighting errors are
	       minor. */
	    while (!patternIsParsable
			(patternOfStyle(pass1Patterns, runningStyle)))
	       runningStyle = parentStyleOf(parentStyles, runningStyle);

    	    return runningStyle;
    	}
    }
}

/*
** Return a position far enough back in "buf" from "fromPos" to give patterns
** their guranteed amount of context for matching (from "context").  If
** backing up by lines yields the greater distance, the returned position will
** be to the newline character before the start of the line, rather than to
** the first character of the line.  (I did this because earlier prototypes of
** the syntax highlighting code, which were based on single-line context, used
** this to ensure that line-spanning expressions would be detected.  I think
** it may reduce some 2 line context requirements to one line, at a cost of
** only one extra character, but I'm not sure, and my brain hurts from
** thinking about it).
*/
static int backwardOneContext(textBuffer *buf, reparseContext *context,
    	int fromPos)
{
    if (context->nLines == 0)
    	return max(0, fromPos - context->nChars);
    else if (context->nChars == 0)
    	return max(0,
    	    	BufCountBackwardNLines(buf, fromPos, context->nLines-1) - 1);
    else
    	return max(0, min(max(0, BufCountBackwardNLines(buf, fromPos,
    	    context->nLines-1) -1), fromPos - context->nChars));
}

/*
** Return a position far enough forward in "buf" from "fromPos" to ensure
** that patterns are given their required amount of context for matching
** (from "context").  If moving forward by lines yields the greater
** distance, the returned position will be the first character of of the
** next line, rather than the newline character at the end (see notes in
** backwardOneContext).
*/
static int forwardOneContext(textBuffer *buf, reparseContext *context,
    	int fromPos)
{
    if (context->nLines == 0)
    	return min(buf->length, fromPos + context->nChars);
    else if (context->nChars == 0)
    	return min(buf->length,
    	    	BufCountForwardNLines(buf, fromPos, context->nLines));
    else
    	return min(buf->length, max(BufCountForwardNLines(buf, fromPos,
    	    	context->nLines), fromPos + context->nChars));
}

/*
** Change styles in the portion of "styleString" to "style" where a particular
** sub-expression, "subExpr", of regular expression "re" applies to the
** corresponding portion of "string".
*/
static void recolorSubexpr(regexp *re, int subexpr, int style,
    	const char *string, char *styleString)
{
    const char *stringPtr;
    char *stylePtr;
    	
    stringPtr = re->startp[subexpr];
    stylePtr = &styleString[stringPtr - string];
    fillStyleString(&stringPtr, &stylePtr, re->endp[subexpr], style, NULL);
}

/*
** Search for a pattern in pattern list "patterns" with style "style"
*/
static highlightDataRec *patternOfStyle(highlightDataRec *patterns, int style)
{
    int i;
    for (i=0; patterns[i].style!=0; i++)
    	if (patterns[i].style == style)
    	    return &patterns[i];
    if (style == PLAIN_STYLE || style == UNFINISHED_STYLE)
	return &patterns[0];
    return NULL;
}

static int max(int i1, int i2)
{
    return i1 >= i2 ? i1 : i2;
}

static int min(int i1, int i2)
{
    return i1 <= i2 ? i1 : i2;
}

static int indexOfNamedPattern(highlightPattern *patList, int nPats,
    	const char *patName)
{
    int i;
    
    if (patName == NULL)
    	return -1;
    for (i=0; i<nPats; i++)
    	if (!strcmp(patList[i].name, patName))
    	    return i;
    return -1;
}

static int findTopLevelParentIndex(highlightPattern *patList, int nPats,
    	int index)
{
    int topIndex;
    
    topIndex = index;
    while (patList[topIndex].subPatternOf != NULL) {
    	topIndex = indexOfNamedPattern(patList, nPats,
    	    	patList[topIndex].subPatternOf);
	if (index==topIndex)
	   return -1; /* amai: circular dependency ?! */
    }
    return topIndex;
}

/*
** Re-size (or re-height, anyhow) a window after adding or removing
** highlight fonts has changed the required vertical spacing (horizontal
** spacing is determined by the primary font, which doesn't change).
**
** Note that this messes up the window manager's height increment hint,
** which must be subsequently reset by UpdateWMSizeHints.
*/
static void updateWindowHeight(WindowInfo *window, int oldFontHeight)
{
    int i, borderHeight, marginHeight;
    Dimension windowHeight, textAreaHeight, textHeight, newWindowHeight;
    
    /* resize if there's only _one_ document in the window, to avoid
       the growing-window bug */
    if (NDocuments(window) > 1)
    	return;
	
    /* Decompose the window height into the part devoted to displaying
       text (textHeight) and the non-text part (boderHeight) */
    XtVaGetValues(window->shell, XmNheight, &windowHeight, NULL);
    XtVaGetValues(window->textArea, XmNheight, &textAreaHeight,
    	    textNmarginHeight, &marginHeight, NULL);
    textHeight = textAreaHeight - 2*marginHeight;
    for (i=0; i<window->nPanes; i++) {
    	XtVaGetValues(window->textPanes[i], XmNheight, &textAreaHeight, NULL);
    	textHeight += textAreaHeight - 2*marginHeight;
    }
    borderHeight = windowHeight - textHeight;
        
    /* Calculate a new window height appropriate for the new font */
    newWindowHeight = (textHeight*getFontHeight(window)) / oldFontHeight +
    	    borderHeight;
    
    /* Many window managers enforce window size increments even on client resize
       requests.  Our height increment is probably wrong because it is still
       set for the previous font.  Set the new height in advance, before
       attempting to resize. */
    XtVaSetValues(window->shell, XmNheightInc, getFontHeight(window), NULL);
    
    /* Re-size the window */
    XtVaSetValues(window->shell, XmNheight, newWindowHeight, NULL);
}

/*
** Find the height currently being used to display text, which is
** a composite of all of the active highlighting fonts as determined by the
** text display component
*/
static int getFontHeight(WindowInfo *window)
{
    textDisp *textD = ((TextWidget)window->textArea)->text.textD;

    return textD->ascent + textD->descent;
}
