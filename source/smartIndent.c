/*******************************************************************************
*									       *
* smartIndent.c -- Maintain, and allow user to edit, macros for smart indent   *
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
* July, 1997								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "smartIndent.h"
#include "textBuf.h"
#include "nedit.h"
#include "text.h"
#include "preferences.h"
#include "interpret.h"
#include "macro.h"
#include "window.h"
#include "parse.h"
#include "shift.h"
#include "help.h"
#include "../util/DialogF.h"
#include "../util/misc.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <Xm/Xm.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/SeparatoG.h>
#include <Xm/PanedW.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif


static char MacroEndBoundary[] = "--End-of-Macro--";

typedef struct {
    char *lmName;
    char *initMacro;
    char *newlineMacro;
    char *modMacro;
} smartIndentRec;

typedef struct {
    Program *newlineMacro;
    int inNewLineMacro;
    Program *modMacro;
    int inModMacro;
} windowSmartIndentData;

/* Smart indent macros dialog information */
static struct {
    Widget shell;
    Widget lmOptMenu;
    Widget lmPulldown;
    Widget initMacro;
    Widget newlineMacro;
    Widget modMacro;
    char *langModeName;
} SmartIndentDialog = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};

/* Common smart indent macros dialog information */
static struct {
    Widget shell;
    Widget text;
} CommonDialog = {NULL,NULL};

static int NSmartIndentSpecs = 0;
static smartIndentRec *SmartIndentSpecs[MAX_LANGUAGE_MODES];
static char *CommonMacros = NULL;

static void executeNewlineMacro(WindowInfo *window,smartIndentCBStruct *cbInfo);
static void executeModMacro(WindowInfo *window,smartIndentCBStruct *cbInfo);
static void insertShiftedMacro(textBuffer *buf, char *macro);
static int isDefaultIndentSpec(smartIndentRec *indentSpec);
static smartIndentRec *findIndentSpec(const char *modeName);
static char *ensureNewline(char *string);
static int loadDefaultIndentSpec(char *lmName);
static int siParseError(char *stringStart, char *stoppedAt, char *message);
static void destroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void langModeCB(Widget w, XtPointer clientData, XtPointer callData);
static void commonDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void okCB(Widget w, XtPointer clientData, XtPointer callData);
static void applyCB(Widget w, XtPointer clientData, XtPointer callData);
static void checkCB(Widget w, XtPointer clientData, XtPointer callData);
static void restoreCB(Widget w, XtPointer clientData, XtPointer callData);
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData);
static void closeCB(Widget w, XtPointer clientData, XtPointer callData);
static void helpCB(Widget w, XtPointer clientData, XtPointer callData);
static int checkSmartIndentDialogData(void);
static smartIndentRec *getSmartIndentDialogData(void);
static void setSmartIndentDialogData(smartIndentRec *is);
static void comDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void comOKCB(Widget w, XtPointer clientData, XtPointer callData);
static void comApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static void comCheckCB(Widget w, XtPointer clientData, XtPointer callData);
static void comRestoreCB(Widget w, XtPointer clientData, XtPointer callData);
static void comCloseCB(Widget w, XtPointer clientData, XtPointer callData);
static int updateSmartIndentCommonData(void);
static int checkSmartIndentCommonDialogData(void);
static int updateSmartIndentData(void);
static char *readSIMacro(char **inPtr);
static smartIndentRec *copyIndentSpec(smartIndentRec *is);
static void freeIndentSpec(smartIndentRec *is);
static int indentSpecsDiffer(smartIndentRec *is1, smartIndentRec *is2);

#define N_DEFAULT_INDENT_SPECS 4
static smartIndentRec DefaultIndentSpecs[N_DEFAULT_INDENT_SPECS] = {
{"C",
"# C Macros and tuning parameters are shared with C++, and are declared\n\
# in the common section.  Press Common / Shared Initialization above.\n",
"return cFindSmartIndentDist($1)\n",
"if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n\
    cBraceOrPound($1, $2)\n"},
{"C++",
"# C++ Macros and tuning parameters are shared with C, and are declared\n\
# in the common section.  Press Common / Shared Initialization above.\n",
"return cFindSmartIndentDist($1)\n",
"if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n\
    cBraceOrPound($1, $2)\n"},
{"Python",
"# Number of characters in a normal indent level.  May be a number, or the\n\
# string \"default\", meaning, guess the value from the current tab settings.\n\
$pyIndentDist = \"default\"\n",
"if (get_range($1-1, $1) != \":\")\n\
    return -1\n\
return measureIndent($1) + defaultIndent($pyIndentDist)\n", NULL},
{"Matlab",
"# Number of spaces to indent \"case\" statements\n\
$caseDepth = 2\n\
define matlabNewlineMacro\n\
{\n\
   if (!$em_tab_dist)\n\
        tabsize = $tab_dist\n\
   else\n\
        tabsize = $em_tab_dist\n\
   startLine = startOfLine($1)\n\
   indentLevel = measureIndent($1)\n\
\n\
   # If this line is continued on next, return default:\n\
   lastLine = get_range(startLine, $1)\n\
   if (search_string(lastLine, \"...\", 0) != -1) {\n\
      if ($n_args == 2)\n\
         return matlabNewlineMacro(startLine - 1, 1)\n\
      else {\n\
         return -1\n\
      }\n\
   }\n\
\n\
   # Correct the indentLevel if this was a continued line.\n\
   while (startLine > 1)\n\
   {\n\
      endLine = startLine - 1\n\
      startLine = startOfLine(endLine)\n\
      lastLine = get_range(startLine, endLine)\n\
      # No \"...\" means we've found the root\n\
      if (search_string(lastLine, \"...\", 0) == -1) {\n\
         startLine = endLine + 1\n\
         break\n\
      }\n\
   }\n\
   indentLevel = measureIndent(startLine)\n\
\n\
   # Get the first word of the indentLevel line\n\
   FWend = search(\">|\\n\", startLine + indentLevel, \"regex\")\n\
   # This search fails on EOF\n\
   if (FWend == -1)\n\
      FWend = $1\n\
\n\
   firstWord = get_range(startLine + indentLevel, FWend)\n\
\n\
   # How shall we change the indent level based on the first word?\n\
   if (search_string(firstWord, \\\n\
         \"<for>|<function>|<if>|<switch>|<try>|<while>\", 0, \"regex\") == 0) {\n\
      return indentLevel + tabsize\n\
   }\n\
   else if ((firstWord == \"end\") || (search_string(firstWord, \\\n\
            \"<case>|<catch>|<else>|<elseif>|<otherwise>\", 0, \"regex\") == 0)) {\n\
      # Get the last indent level \n\
      if (startLine > 0) # avoid infinite loop\n\
	 last_indent = matlabNewlineMacro(startLine - 1, 1)\n\
      else\n\
         last_indent = indentLevel\n\
\n\
      # Re-indent this line\n\
      if ($n_args == 1) {\n\
         if (firstWord == \"case\" || firstWord == \"otherwise\")\n\
            replace_range(startLine, startLine + indentLevel, \\\n\
                          makeIndentString(last_indent - tabsize + $caseDepth))\n\
         else\n\
            replace_range(startLine, startLine + indentLevel, \\\n\
                          makeIndentString(last_indent - tabsize))\n\
      }\n\
\n\
      if (firstWord == \"end\") {\n\
         return max(last_indent - tabsize, 0)\n\
      }\n\
      else {\n\
         return last_indent\n\
      }\n\
   } \n\
   else {\n\
      return indentLevel\n\
   }\n\
}\n\
", "return matlabNewlineMacro($1)\n", NULL}
};

static char DefaultCommonMacros[] = "#\n\
# C/C++ Style/tuning parameters\n\
#\n\
\n\
# Number of characters in a normal indent level.  May be a number, or the\n\
# string \"default\", meaning, guess the value from the current tab settings.\n\
$cIndentDist = \"default\"\n\
\n\
# Number of characters in a line continuation.  May be a number or the\n\
# string \"default\", meaning, guess the value from the current tab settings.\n\
$cContinuationIndent = \"default\"\n\
\n\
# How far back from the current position to search for an anchoring position\n\
# on which to base indent.  When no reliable indicators of proper indent level\n\
# can be found within the requested distance, reverts to plain auto indent.\n\
$cMaxSearchBackLines = 10\n\
\n\
#\n\
# Find the start of the line containing position $1\n\
#\n\
define startOfLine {\n\
\n\
    for (i=$1-1; ; i--) {\n\
	if (i <= 0)\n\
	    return 0\n\
	if (get_character(i) == \"\\n\")\n\
	    return i + 1\n\
    }\n\
}\n\
\n\
#\n\
# Find the indent level of the line containing character position $1\n\
#\n\
define measureIndent {\n\
    \n\
    # measure the indentation to the first non-white character on the line\n\
    indent = 0\n\
    for (i=startOfLine($1); i < $text_length; i++) {\n\
	c = get_character(i)\n\
	if (c != \" \" && c != \"\\t\")\n\
	    break\n\
	if (c == \"\\t\")\n\
	    indent += $tab_dist - (indent % $tab_dist)\n\
	else\n\
	    indent++\n\
    }\n\
    return indent\n\
}\n\
\n\
#\n\
# Make a string to produce an indent of $1 characters\n\
#\n\
define makeIndentString {\n\
\n\
    if ($use_tabs) {\n\
	nTabs = $1 / $tab_dist\n\
	nSpaces = $1 % $tab_dist\n\
    } else {\n\
	nTabs = 0\n\
	nSpaces = $1\n\
    }\n\
    indentString = \"\"\n\
    for (i=0; i<nTabs; i++)\n\
	indentString = indentString \"\\t\"\n\
    for (i=0; i<nSpaces; i++)\n\
	indentString = indentString \" \"\n\
    return indentString\n\
}\n\
\n\
#\n\
# If $1 is a number, just pass it on.  If it is the string \"default\",\n\
# figure out a reasonable indent distance for a structured languages\n\
# like C, based on how tabs are set.\n\
#\n\
define defaultIndent {\n\
\n\
    if ($1 != \"default\")\n\
    	return $1\n\
    if ($em_tab_dist)\n\
    	return $em_tab_dist\n\
    if ($tab_dist <= 8)\n\
    	return $tab_dist\n\
    return 4\n\
}\n\
   \n\
#\n\
# If $1 is a number, just pass it on.  If it is the string \"default\",\n\
# figure out a reasonable amount of indentation for continued lines\n\
# based on how tabs are set.\n\
#\n\
define defaultContIndent {\n\
\n\
    if ($1 != \"default\")\n\
    	return $1\n\
    if ($em_tab_dist)\n\
    	return $em_tab_dist * 2\n\
    if ($tab_dist <= 8)\n\
    	return $tab_dist * 2\n\
    return 8\n\
}\n\
\n\
#\n\
# Find the end of the conditional part of if/while/for, by looking for balanced\n\
# parenthesis between $1 and $2.  returns -1 if parens don't balance before\n\
# $2, or if no parens are found\n\
#\n\
define findBalancingParen {\n\
\n\
    openParens = 0\n\
    parensFound = 0\n\
    for (i=$1; i<$2; i++) {\n\
	c = get_character(i)\n\
	if (c == \"(\") {\n\
	    openParens++\n\
	    parensFound = 1\n\
	} else if (c == \")\")\n\
	    openParens--\n\
	else if (!parensFound && c != \" \" && c != \"\\t\")\n\
	    return -1\n\
	if (parensFound && openParens <=0)\n\
	    return i+1\n\
    }\n\
    return -1\n\
}\n\
\n\
#\n\
# Skip over blank space and comments and preprocessor directives from position\n\
# $1 to a maximum of $2.\n\
# if $3 is non-zero, newlines are considered blank space as well.  Return -1\n\
# if the maximum position ($2) is hit mid-comment or mid-directive\n\
#\n\
define cSkipBlankSpace {\n\
    \n\
    for (i=$1; i<$2; i++) {\n\
	c = get_character(i)\n\
	if (c == \"/\") {\n\
	    if (i+1 >= $2)\n\
		return i\n\
	    if (get_character(i+1) == \"*\") {\n\
		for (i=i+1; ; i++) {\n\
		    if (i+1 >= $2)\n\
			return -1\n\
		    if (get_character(i) == \"*\" && get_character(i+1) == \"/\") {\n\
			i++\n\
			break\n\
		    }\n\
		}\n\
	    } else if (get_character(i+1) == \"/\") {\n\
		for (i=i+1; i<$2; i++) {\n\
		    if (get_character(i) == \"\\n\") {\n\
			if (!$3)\n\
			    return i\n\
			break\n\
		    }\n\
		}\n\
	    }\n\
	} else if (c == \"#\" && $3) {\n\
	    for (i=i+1; ; i++) {\n\
		if (i >= $2) {\n\
		    if (get_character(i-1) == \"\\\\\")\n\
			return -1\n\
		    else\n\
			break\n\
		}\n\
		if (get_character(i) == \"\\n\" && get_character(i-1) != \"\\\\\")\n\
		    break\n\
	    }\n\
	} else if (!(c == \" \" || c == \"\\t\" || ($3 && c==\"\\n\")))\n\
	    return i\n\
    }\n\
    return $2\n\
}\n\
\n\
#\n\
# Search backward for an anchor point: a line ending brace, or semicolon\n\
# or case statement, followed (ignoring blank lines and comments) by what we\n\
# assume is a properly indented line, a brace on a line by itself, or a case\n\
# statement.  Returns the position of the first non-white, non comment\n\
# character on the line.  returns -1 if an anchor position can't be found\n\
# before $cMaxSearchBackLines.\n\
#\n\
define cFindIndentAnchorPoint {\n\
\n\
    nLines = 0\n\
    anchorPos = $1\n\
    for (i=$1-1; i>0; i--) {\n\
	c = get_character(i)\n\
	if (c == \";\" || c == \"{\" || c == \"}\" || c == \":\") {\n\
\n\
	    # Verify that it's line ending\n\
	    lineEnd = cSkipBlankSpace(i+1, $1, 0)\n\
	    if (lineEnd == -1 || \\\n\
	    	    (lineEnd != $text_length && get_character(lineEnd) != \"\\n\"))\n\
   		continue\n\
\n\
	    # if it's a colon, it's only meaningful if \"case\" begins the line\n\
	    if (c == \":\") {\n\
	    	lineStart = startOfLine(i)\n\
		caseStart = cSkipBlankSpace(lineStart, lineEnd, 0)\n\
		if (get_range(caseStart, caseStart+4) != \"case\")\n\
		    continue\n\
		delim = get_character(caseStart+4)\n\
		if (delim!=\" \" && delim!=\"\\t\" && delim!=\"(\" && delim!=\":\")\n\
		    continue\n\
		isCase = 1\n\
	    } else\n\
	    	isCase = 0\n\
\n\
	    # Move forward past blank lines and comment lines to find\n\
	    #    non-blank, non-comment line-start\n\
	    anchorPos = cSkipBlankSpace(lineEnd, $1, 1)\n\
\n\
	    # Accept if it's before the requested position, otherwise\n\
	    #    continue further back in the file and try again\n\
	    if (anchorPos != -1 && anchorPos < $1)\n\
		break\n\
\n\
	    # A case statement by itself is an acceptable anchor\n\
	    if (isCase)\n\
	    	return caseStart\n\
\n\
	    # A brace on a line by itself is an acceptable anchor, even\n\
	    #    if it doesn't follow a semicolon or another brace\n\
	    if (c == \"{\" || c == \"}\") {\n\
		for (j = i-1; ; j--) {\n\
		    if (j == 0)\n\
			return i\n\
		    ch = get_character(j)\n\
		    if (ch == \"\\n\")\n\
		       return i\n\
		    if (ch != \"\\t\" && ch != \" \")\n\
		       break\n\
		}\n\
	    }\n\
\n\
	} else if (c == \"\\n\")\n\
	    if (++nLines > $cMaxSearchBackLines)\n\
		return -1\n\
    }\n\
    if (i <= 0)\n\
	return -1\n\
    return anchorPos\n\
}\n\
\n\
#\n\
# adjust the indent on a line about to recive either a right or left brace\n\
# or pound (#) character ($2) following position $1\n\
#\n\
define cBraceOrPound {\n\
\n\
    # Find start of the line, and make sure there's nothing but white-space\n\
    #   before the character.  If there's anything before it, do nothing\n\
    for (i=$1-1; ; i--) {\n\
	if (i < 0) {\n\
	    lineStart = 0\n\
	    break\n\
	}\n\
	c = get_character(i)\n\
	if (c == \"\\n\") {\n\
	    lineStart = i + 1\n\
	    break\n\
	}\n\
	if (c != \" \" && c != \"\\t\")\n\
	    return\n\
    }\n\
\n\
    # If the character was a pound, drag it all the way to the left margin\n\
    if ($2 == \"#\") {\n\
	replace_range(lineStart, $1, \"\")\n\
	return\n\
    }\n\
\n\
    # Find the position on which to base the indent\n\
    indent = cFindSmartIndentDist($1 - 1, \"noContinue\")\n\
    if (indent == -1)\n\
	return\n\
    \n\
    # Adjust the indent if it's a right brace (left needs no adjustment)\n\
    if ($2 == \"}\") {\n\
	indent -= defaultIndent($cIndentDist)\n\
        if (indent < 0)\n\
	    indent = 0\n\
    }\n\
\n\
    # Replace the current indent with the new indent string\n\
    insertStr = makeIndentString(indent)\n\
    replace_range(lineStart, $1, insertStr)\n\
}\n\
\n\
#\n\
# Find Smart Indent Distance for a newline character inserted at $1,\n\
# or return -1 to give up.  Adding the optional argument \"noContinue\"\n\
# will stop the routine from inserting line continuation indents\n\
#\n\
define cFindSmartIndentDist {\n\
\n\
    # Find a known good indent to base the new indent upon\n\
    anchorPos = cFindIndentAnchorPoint($1)\n\
    if (anchorPos == -1)\n\
	return -1\n\
\n\
    # Find the indentation of that line\n\
    anchorIndent = measureIndent(anchorPos)\n\
\n\
    # Look for special keywords which affect indent (for, if, else while, do)\n\
    #    and modify the continuation indent distance to the normal indent\n\
    #    distance when a completed statement of this type occupies the line.\n\
    if ($n_args >= 2 && $2 == \"noContinue\") {\n\
	continueIndent = 0\n\
	$allowSemi = 0\n\
    } else\n\
	continueIndent = cCalcContinueIndent(anchorPos, $1)\n\
\n\
    # Move forward from anchor point, ignoring comments and blank lines,\n\
    #   remembering the last non-white, non-comment character.  If $1 is\n\
    #   in the middle of a comment, give up\n\
    lastChar = get_character(anchorPos)\n\
    if (anchorPos < $1) {\n\
	for (i=anchorPos;;) {\n\
   	    i = cSkipBlankSpace(i, $1, 1)\n\
	    if (i == -1)\n\
		return -1\n\
 	    if (i >= $1)\n\
 		break\n\
 	    lastChar = get_character(i++)\n\
	}\n\
    }\n\
\n\
    # Return the new indent based on the type of the last character.\n\
    #   In a for stmt, however, last character may be a semicolon and not\n\
    #   signal the end of the statement\n\
    if (lastChar == \"{\")\n\
	return anchorIndent + defaultIndent($cIndentDist)\n\
    else if (lastChar == \"}\")\n\
	return anchorIndent\n\
    else if (lastChar == \";\") {\n\
	if ($allowSemi)\n\
	    return anchorIndent + continueIndent\n\
	else\n\
	    return anchorIndent\n\
    } else if (lastChar == \":\" && get_range(anchorPos, anchorPos+4) == \"case\")\n\
    	return anchorIndent + defaultIndent($cIndentDist)\n\
    return anchorIndent + continueIndent\n\
}\n\
\n\
#\n\
# Calculate the continuation indent distance for statements not ending in\n\
# semicolons or braces.  This is not necessarily $continueIndent.  It may\n\
# be adjusted if the statement contains if, while, for, or else.\n\
#\n\
# As a side effect, also return $allowSemi to help distinguish statements\n\
# which might contain an embedded semicolon, which should not be interpreted\n\
# as an end of statement character.\n\
#\n\
define cCalcContinueIndent {\n\
\n\
    anchorPos = $1\n\
    maxPos = $2\n\
\n\
    # Figure out if the anchor is on a keyword which changes indent.  A special\n\
    #   case is made for elses nested in after braces\n\
    anchorIsFor = 0\n\
    $allowSemi = 0\n\
    if (get_character(anchorPos) == \"}\") {\n\
	for (i=anchorPos+1; i<maxPos; i++) {\n\
	    c = get_character(i)\n\
	    if (c != \" \" && c != \"\\t\")\n\
		break\n\
	}\n\
	if (get_range(i, i+4) == \"else\") {\n\
	    keywordEnd = i + 4\n\
	    needsBalancedParens = 0\n\
	} else\n\
	    return defaultContIndent($cContinuationIndent)\n\
    } else if (get_range(anchorPos, anchorPos + 4) == \"else\") {\n\
	keywordEnd = anchorPos + 4\n\
	needsBalancedParens = 0\n\
    } else if (get_range(anchorPos, anchorPos + 2) == \"do\") {\n\
	keywordEnd = anchorPos + 2\n\
	needsBalancedParens = 0\n\
    } else if (get_range(anchorPos, anchorPos + 3) == \"for\") {\n\
	keywordEnd = anchorPos + 3\n\
	anchorIsFor = 1\n\
	needsBalancedParens = 1\n\
    } else if (get_range(anchorPos, anchorPos + 2) == \"if\") {\n\
	keywordEnd = anchorPos + 2\n\
	needsBalancedParens = 1\n\
    } else if (get_range(anchorPos, anchorPos + 5) == \"while\") {\n\
	keywordEnd = anchorPos + 5\n\
	needsBalancedParens = 1\n\
    } else\n\
	return defaultContIndent($cContinuationIndent)\n\
\n\
    # If the keyword must be followed balanced parenthesis, find the end of\n\
    # the statement by following balanced parens.  If the parens aren't\n\
    # balanced by maxPos, continue the condition.  In the special case of\n\
    # the for keyword, a semicolon can end the line and the caller should be\n\
    # signaled to allow that\n\
    if (needsBalancedParens) {\n\
	stmtEnd = findBalancingParen(keywordEnd, maxPos)\n\
	if (stmtEnd == -1) {\n\
	    $allowSemi = anchorIsFor\n\
	    return defaultContIndent($cContinuationIndent)\n\
	}\n\
    } else\n\
	stmtEnd = keywordEnd\n\
\n\
    # check if the statement ends the line\n\
    lineEnd = cSkipBlankSpace(stmtEnd, maxPos, 0)\n\
    if (lineEnd == -1)		    	    # ends in comment or preproc\n\
	return -1\n\
    if (lineEnd == maxPos)  	    	    # maxPos happens at stmt end\n\
	return defaultIndent($cIndentDist)\n\
    c = get_character(lineEnd)\n\
    if (c != \"\\n\")   		    	    # something past last paren on line,\n\
	return defaultIndent($cIndentDist)  #   probably quoted or extra braces\n\
\n\
    # stmt contintinues beyond matching paren && newline, we're in\n\
    #   the conditional part, calculate the continue indent distance\n\
    #   recursively, based on the anchor point of the new line\n\
    newAnchor = cSkipBlankSpace(lineEnd+1, maxPos, 1)\n\
    if (newAnchor == -1)\n\
	return -1\n\
    if (newAnchor == maxPos)\n\
	return defaultIndent($cIndentDist)\n\
    return cCalcContinueIndent(newAnchor, maxPos) + defaultIndent($cIndentDist)\n\
}\n\
";

/*
** Turn on smart-indent (well almost).  Unfortunately, this doesn't do
** everything.  It requires that the smart indent callback (SmartIndentCB)
** is already attached to all of the text widgets in the window, and that the
** smartIndent resource must be turned on in the widget.  These are done
** separately, because they are required per-text widget, and therefore must
** be repeated whenever a new text widget is created within this window
** (a split-window command).
*/
void BeginSmartIndent(WindowInfo *window, int warn)
{
    windowSmartIndentData *winData;
    smartIndentRec *indentMacros;
    char *modeName, *stoppedAt, *errMsg;
    static int initialized;

    /* Find the window's language mode.  If none is set, warn the user */
    modeName = LanguageModeName(window->languageMode);
    if (modeName == NULL)
    {
        if (warn)
        {
            DialogF(DF_WARN, window->shell, 1, "Smart Indent",
                    "No language-specific mode has been set for this file.\n\n"
                    "To use smart indent in this window, please select a\n"
                    "language from the Preferences -> Language Modes menu.",
                    "OK");
        }
        return;
    }
    
    /* Look up the appropriate smart-indent macros for the language */
    indentMacros = findIndentSpec(modeName);
    if (indentMacros == NULL)
    {
        if (warn)
        {
            DialogF(DF_WARN, window->shell, 1, "Smart Indent",
                    "Smart indent is not available in languagemode\n%s.\n\n"
                    "You can create new smart indent macros in the\n"
                    "Preferences -> Default Settings -> Smart Indent\n"
                    "dialog, or choose a different language mode from:\n"
                    "Preferences -> Language Mode.", "OK", modeName);
        }
        return;
    }
    
    /* Make sure that the initial macro file is loaded before we execute 
       any of the smart-indent macros. Smart-indent macros may reference
       routines defined in that file. */
    ReadMacroInitFile(window);   
    
    /* Compile and run the common and language-specific initialization macros
       (Note that when these return, the immediate commands in the file have not
       necessarily been executed yet.  They are only SCHEDULED for execution) */
    if (!initialized) {
    	if (!ReadMacroString(window, CommonMacros,
	    	"smart indent common initialization macros"))
    	    return;
	initialized = True;
    }
    if (indentMacros->initMacro != NULL) {
	if (!ReadMacroString(window, indentMacros->initMacro,
    	    	"smart indent initialization macro"))
    	    return;
    }
    
    /* Compile the newline and modify macros and attach them to the window */
    winData = (windowSmartIndentData *)NEditMalloc(sizeof(windowSmartIndentData));
    winData->inNewLineMacro = 0;
    winData->inModMacro = 0;
    winData->newlineMacro = ParseMacro(indentMacros->newlineMacro, &errMsg,
    	    &stoppedAt);
    if (winData->newlineMacro == NULL) {
        NEditFree(winData);
    	ParseError(window->shell, indentMacros->newlineMacro, stoppedAt,
    	    	"newline macro", errMsg);
    	return;
    }
    if (indentMacros->modMacro == NULL)
    	winData->modMacro = NULL;
    else {
    	winData->modMacro = ParseMacro(indentMacros->modMacro, &errMsg,
    	    	&stoppedAt);
    	if (winData->modMacro == NULL) {
            FreeProgram(winData->newlineMacro);
            NEditFree(winData);
    	    ParseError(window->shell, indentMacros->modMacro, stoppedAt,
    	    	    "smart indent modify macro", errMsg);
    	    return;
    	}
    }
    window->smartIndentData = (void *)winData;
}

void EndSmartIndent(WindowInfo *window)
{
    windowSmartIndentData *winData =
    	    (windowSmartIndentData *)window->smartIndentData;
    
    if (winData == NULL)
    	return;

    /* Free programs and allocated data */
    if (winData->modMacro != NULL)
    	FreeProgram(winData->modMacro);
    FreeProgram(winData->newlineMacro);
    NEditFree(winData);
    window->smartIndentData = NULL;
}

/*
** Returns true if there are smart indent macros for a named language
*/
int SmartIndentMacrosAvailable(char *languageModeName)
{
    return findIndentSpec(languageModeName) != NULL;
}

/*
** Attaches to the text widget's smart-indent callback to invoke a user
** defined macro when the text widget requires an indent (not just when the
** user types a newline, but also when the widget does an auto-wrap with
** auto-indent on), or the user types some other character.
*/
void SmartIndentCB(Widget w, XtPointer clientData, XtPointer callData) 
{
    WindowInfo *window = WidgetToWindow(w);
    smartIndentCBStruct *cbInfo = (smartIndentCBStruct *)callData;
    
    if (window->smartIndentData == NULL)
    	return;
    if (cbInfo->reason == CHAR_TYPED)
	executeModMacro(window, cbInfo);
    else if (cbInfo->reason == NEWLINE_INDENT_NEEDED)
	executeNewlineMacro(window, cbInfo);
}

/*
** Run the newline macro with information from the smart-indent callback
** structure passed by the widget
*/
static void executeNewlineMacro(WindowInfo *window, smartIndentCBStruct *cbInfo)
{
    windowSmartIndentData *winData =
    	    (windowSmartIndentData *)window->smartIndentData;
    /* posValue probably shouldn't be static due to re-entrance issues <slobasso> */
    static DataValue posValue = {INT_TAG, {0}};
    DataValue result;
    RestartData *continuation;
    char *errMsg;
    int stat;
    
    /* Beware of recursion: the newline macro may insert a string which
       triggers the newline macro to be called again and so on. Newline
       macros shouldn't insert strings, but nedit must not crash either if
       they do. */
    if (winData->inNewLineMacro)
	return;
   
    /* Call newline macro with the position at which to add newline/indent */
    posValue.val.n = cbInfo->pos;
    ++(winData->inNewLineMacro);
    stat = ExecuteMacro(window, winData->newlineMacro, 1, &posValue, &result,
    	    &continuation, &errMsg);
    
    /* Don't allow preemption or time limit.  Must get return value */
    while (stat == MACRO_TIME_LIMIT)
    	stat = ContinueMacro(continuation, &result, &errMsg);
    
    --(winData->inNewLineMacro);
    /* Collect Garbage.  Note that the mod macro does not collect garbage,
       (because collecting per-line is more efficient than per-character)
       but GC now depends on the newline macro being mandatory */
    SafeGC();
    
    /* Process errors in macro execution */
    if (stat == MACRO_PREEMPT || stat == MACRO_ERROR)
    {
        DialogF(DF_ERR, window->shell, 1, "Smart Indent",
                "Error in smart indent macro:\n%s", "OK",
                stat == MACRO_ERROR
                        ? errMsg
                        : "dialogs and shell commands not permitted");
        EndSmartIndent(window);
        return;
    }

    /* Validate and return the result */
    if (result.tag != INT_TAG || result.val.n < -1 || result.val.n > 1000)
    {
        DialogF(DF_ERR, window->shell, 1, "Smart Indent",
                "Smart indent macros must return\ninteger indent distance",
                "OK");
        EndSmartIndent(window);
        return;
    }

    cbInfo->indentRequest = result.val.n;
}


Boolean InSmartIndentMacros(WindowInfo *window) {
    windowSmartIndentData *winData =
    	    (windowSmartIndentData *)window->smartIndentData;

	return((winData && (winData->inModMacro || winData->inNewLineMacro)));
}

/*
** Run the modification macro with information from the smart-indent callback
** structure passed by the widget
*/
static void executeModMacro(WindowInfo *window,smartIndentCBStruct *cbInfo)
{
    windowSmartIndentData *winData =
    	    (windowSmartIndentData *)window->smartIndentData;
    /* args probably shouldn't be static due to future re-entrance issues <slobasso> */
    static DataValue args[2] = {{INT_TAG, {0}}, {STRING_TAG, {0}}};
    /* after 5.2 release remove inModCB and use new winData->inModMacro value */
    static int inModCB = False;
    DataValue result;
    RestartData *continuation;
    char *errMsg;
    int stat;
    
    /* Check for inappropriate calls and prevent re-entering if the macro
       makes a buffer modification */
    if (winData == NULL || winData->modMacro == NULL || inModCB)
    	return;
	
    /* Call modification macro with the position of the modification,
       and the character(s) inserted.  Don't allow
       preemption or time limit.  Execution must not overlap or re-enter */
    args[0].val.n = cbInfo->pos;
    AllocNStringCpy(&args[1].val.str, cbInfo->charsTyped);

    inModCB = True;
    ++(winData->inModMacro);

    stat = ExecuteMacro(window, winData->modMacro, 2, args, &result,
        &continuation, &errMsg);
    while (stat == MACRO_TIME_LIMIT)
        stat = ContinueMacro(continuation, &result, &errMsg);

    --(winData->inModMacro);
    inModCB = False;
    
    /* Process errors in macro execution */
    if (stat == MACRO_PREEMPT || stat == MACRO_ERROR)
    {
        DialogF(DF_ERR, window->shell, 1, "Smart Indent",
                "Error in smart indent modification macro:\n%s", "OK",
                stat == MACRO_ERROR
                        ? errMsg
                        : "dialogs and shell commands not permitted");
        EndSmartIndent(window);
        return;
    }
}

void EditSmartIndentMacros(WindowInfo *window)
{
#define BORDER 4
    Widget form, lmOptMenu, lmForm, lmBtn;
    Widget okBtn, applyBtn, checkBtn, deleteBtn, commonBtn;
    Widget closeBtn, helpBtn, restoreBtn, pane;
    Widget initForm, newlineForm, modifyForm;
    Widget initLbl, newlineLbl, modifyLbl;
    XmString s1;
    char *lmName;
    Arg args[20];
    int n;

    /* if the dialog is already displayed, just pop it to the top and return */
    if (SmartIndentDialog.shell != NULL) {
    	RaiseDialogWindow(SmartIndentDialog.shell);
    	return;
    }
    
    if (LanguageModeName(0) == NULL)
    {
        DialogF(DF_WARN, window->shell, 1, "Language Mode",
                "No Language Modes defined", "OK");
        return;
    }
    
    /* Decide on an initial language mode */
    lmName = LanguageModeName(window->languageMode == PLAIN_LANGUAGE_MODE ? 0 :
    	    window->languageMode);
    SmartIndentDialog.langModeName = NEditStrdup(lmName);

    /* Create a form widget in an application shell */
    n = 0;
    XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); n++;
    XtSetArg(args[n], XmNiconName, "NEdit Smart Indent Macros"); n++;
    XtSetArg(args[n], XmNtitle, "Program Smart Indent Macros"); n++;
    SmartIndentDialog.shell = CreateWidget(TheAppShell, "smartIndent",
	    topLevelShellWidgetClass, args, n);
    AddSmallIcon(SmartIndentDialog.shell);
    form = XtVaCreateManagedWidget("editSmartIndentMacros", xmFormWidgetClass,
	    SmartIndentDialog.shell, XmNautoUnmanage, False,
	    XmNresizePolicy, XmRESIZE_NONE, NULL);
    XtAddCallback(form, XmNdestroyCallback, destroyCB, NULL);
    AddMotifCloseCallback(SmartIndentDialog.shell, closeCB, NULL);
       
    lmForm = XtVaCreateManagedWidget("lmForm", xmFormWidgetClass,
    	    form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 1,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
 
    SmartIndentDialog.lmPulldown = CreateLanguageModeMenu(lmForm, langModeCB,
    	    NULL);
    n = 0;
    XtSetArg(args[n], XmNspacing, 0); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 50); n++;
    XtSetArg(args[n], XmNsubMenuId, SmartIndentDialog.lmPulldown); n++;
    lmOptMenu = XmCreateOptionMenu(lmForm, "langModeOptMenu", args, n);
    XtManageChild(lmOptMenu);
    SmartIndentDialog.lmOptMenu = lmOptMenu;
    
    XtVaCreateManagedWidget("lmLbl", xmLabelGadgetClass, lmForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Language Mode:"),
    	    XmNmnemonic, 'L',
    	    XmNuserData, XtParent(SmartIndentDialog.lmOptMenu),
    	    XmNalignment, XmALIGNMENT_END,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 50,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNbottomWidget, lmOptMenu, NULL);
    XmStringFree(s1);
    
    lmBtn = XtVaCreateManagedWidget("lmBtn", xmPushButtonWidgetClass, lmForm,
    	    XmNlabelString, s1=MKSTRING("Add / Modify\nLanguage Mode..."),
    	    XmNmnemonic, 'A',
    	    XmNrightAttachment, XmATTACH_FORM,
    	    XmNtopAttachment, XmATTACH_FORM, NULL);
    XtAddCallback(lmBtn, XmNactivateCallback, lmDialogCB, NULL);
    XmStringFree(s1);
    
    commonBtn = XtVaCreateManagedWidget("commonBtn", xmPushButtonWidgetClass,
    	    lmForm,
    	    XmNlabelString, s1=MKSTRING("Common / Shared\nInitialization..."),
    	    XmNmnemonic, 'C',
    	    XmNleftAttachment, XmATTACH_FORM,
    	    XmNtopAttachment, XmATTACH_FORM, NULL);
    XtAddCallback(commonBtn, XmNactivateCallback, commonDialogCB, NULL);
    XmStringFree(s1);
    
    okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 1,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 13,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(okBtn, XmNactivateCallback, okCB, NULL);
    XmStringFree(s1);
    
    applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Apply"),
    	    XmNmnemonic, 'y',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 13,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 26,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, applyCB, NULL);
    XmStringFree(s1);
    
    checkBtn = XtVaCreateManagedWidget("check", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Check"),
    	    XmNmnemonic, 'k',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 26,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 39,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(checkBtn, XmNactivateCallback, checkCB, NULL);
    XmStringFree(s1);
    
    deleteBtn = XtVaCreateManagedWidget("delete", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Delete"),
    	    XmNmnemonic, 'D',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 39,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 52,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(deleteBtn, XmNactivateCallback, deleteCB, NULL);
    XmStringFree(s1);
    
    restoreBtn = XtVaCreateManagedWidget("restore", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Restore Defaults"),
    	    XmNmnemonic, 'f',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 52,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 73,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(restoreBtn, XmNactivateCallback, restoreCB, NULL);
    XmStringFree(s1);
    
    closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass,
    	    form,
            XmNlabelString, s1=XmStringCreateSimple("Close"),
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 73,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 86,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, closeCB, NULL);
    XmStringFree(s1);
    
    helpBtn = XtVaCreateManagedWidget("help", xmPushButtonWidgetClass,
    	    form,
    	    XmNlabelString, s1=XmStringCreateSimple("Help"),
    	    XmNmnemonic, 'H',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 86,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 99,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(helpBtn, XmNactivateCallback, helpCB, NULL);
    XmStringFree(s1);
    
    pane = XtVaCreateManagedWidget("pane", xmPanedWindowWidgetClass,  form,
   	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 1,
   	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 99,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, lmForm,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, okBtn, NULL);
     	    /* XmNmarginWidth, 0, XmNmarginHeight, 0, XmNseparatorOn, False,
    	    XmNspacing, 3, XmNsashIndent, -2, */

    initForm = XtVaCreateManagedWidget("initForm", xmFormWidgetClass,
	    pane, NULL);
    initLbl = XtVaCreateManagedWidget("initLbl", xmLabelGadgetClass, initForm,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	     "Language Specific Initialization Macro Commands and Definitions"),
    	    XmNmnemonic, 'I', NULL);
    XmStringFree(s1);
    n = 0;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNrows, 5); n++;
    XtSetArg(args[n], XmNcolumns, 80); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, initLbl); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    SmartIndentDialog.initMacro = XmCreateScrolledText(initForm,
    	    "initMacro", args, n);
    AddMouseWheelSupport(SmartIndentDialog.initMacro);
    XtManageChild(SmartIndentDialog.initMacro);
    RemapDeleteKey(SmartIndentDialog.initMacro);
    XtVaSetValues(initLbl, XmNuserData, SmartIndentDialog.initMacro, NULL);

    newlineForm = XtVaCreateManagedWidget("newlineForm", xmFormWidgetClass,
	    pane, NULL);
    newlineLbl = XtVaCreateManagedWidget("newlineLbl", xmLabelGadgetClass,
    	    newlineForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Newline Macro"),
    	    XmNmnemonic, 'N', NULL);
    XmStringFree(s1);
    XtVaCreateManagedWidget("newlineArgsLbl", xmLabelGadgetClass,
    	    newlineForm, XmNalignment, XmALIGNMENT_END,
    	    XmNlabelString, s1=XmStringCreateSimple(
	       "($1 is insert position, return indent request or -1)"),
	    XmNrightAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);
    n = 0;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNrows, 5); n++;
    XtSetArg(args[n], XmNcolumns, 80); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, newlineLbl); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    SmartIndentDialog.newlineMacro = XmCreateScrolledText(newlineForm,
    	    "newlineMacro", args, n);
    AddMouseWheelSupport(SmartIndentDialog.newlineMacro);
    XtManageChild(SmartIndentDialog.newlineMacro);
    RemapDeleteKey(SmartIndentDialog.newlineMacro);
    XtVaSetValues(newlineLbl, XmNuserData, SmartIndentDialog.newlineMacro,NULL);

    modifyForm = XtVaCreateManagedWidget("modifyForm", xmFormWidgetClass,
	    pane, NULL);
    modifyLbl = XtVaCreateManagedWidget("modifyLbl", xmLabelGadgetClass,
    	    modifyForm, XmNlabelString,s1=XmStringCreateSimple("Type-in Macro"),
    	    XmNmnemonic, 'M', NULL);
    XmStringFree(s1);
    XtVaCreateManagedWidget("modifyArgsLbl", xmLabelGadgetClass,
    	    modifyForm, XmNalignment, XmALIGNMENT_END,
    	    XmNlabelString, s1=XmStringCreateSimple(
	        "($1 is position, $2 is character to be inserted)"),
	    XmNrightAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);
    n = 0;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNrows, 5); n++;
    XtSetArg(args[n], XmNcolumns, 80); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, modifyLbl); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    SmartIndentDialog.modMacro = XmCreateScrolledText(modifyForm,
    	    "modifyMacro", args, n);
    AddMouseWheelSupport(SmartIndentDialog.modMacro);
    XtManageChild(SmartIndentDialog.modMacro);
    RemapDeleteKey(SmartIndentDialog.modMacro);
    XtVaSetValues(modifyLbl, XmNuserData, SmartIndentDialog.modMacro, NULL);

    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);
    
    /* Fill in the dialog information for the selected language mode */
    setSmartIndentDialogData(findIndentSpec(lmName));
    SetLangModeMenu(SmartIndentDialog.lmOptMenu,SmartIndentDialog.langModeName);
    
    /* Realize all of the widgets in the new dialog */
    RealizeWithoutForcingPosition(SmartIndentDialog.shell);
}

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    NEditFree(SmartIndentDialog.langModeName);
    SmartIndentDialog.shell = NULL;
}

static void langModeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    char *modeName;
    int i, resp;
    static smartIndentRec emptyIndentSpec = {NULL, NULL, NULL, NULL};
    smartIndentRec *oldMacros, *newMacros;
	    
    /* Get the newly selected mode name.  If it's the same, do nothing */
    XtVaGetValues(w, XmNuserData, &modeName, NULL);
    if (!strcmp(modeName, SmartIndentDialog.langModeName))
    	return;

    /* Find the original macros */
    for (i=0; i<NSmartIndentSpecs; i++)
    	if (!strcmp(SmartIndentDialog.langModeName,SmartIndentSpecs[i]->lmName))
	    break;
    oldMacros = i == NSmartIndentSpecs ? &emptyIndentSpec : SmartIndentSpecs[i];
    
    /* Check if the macros have changed, if so allow user to apply, discard,
       or cancel */
    newMacros = getSmartIndentDialogData();
    if (indentSpecsDiffer(oldMacros, newMacros))
    {
        resp = DialogF(DF_QUES, SmartIndentDialog.shell, 3, "Smart Indent",
                "Smart indent macros for language mode\n"
                "%s were changed.  Apply changes?", "Apply", "Discard",
                "Cancel", SmartIndentDialog.langModeName);

        if (resp == 3)
        {
            SetLangModeMenu(SmartIndentDialog.lmOptMenu,
            SmartIndentDialog.langModeName);
            return;
        } else if (resp == 1)
        {
            if (checkSmartIndentDialogData())
            {
                if (oldMacros == &emptyIndentSpec)
                {
                    SmartIndentSpecs[NSmartIndentSpecs++]
                            = copyIndentSpec(newMacros);
                } else
                {
                    freeIndentSpec(oldMacros);
                    SmartIndentSpecs[i] = copyIndentSpec(newMacros);
                }
            } else
            {
                SetLangModeMenu(SmartIndentDialog.lmOptMenu,
                SmartIndentDialog.langModeName);
                return;
            }
        }
    }
    freeIndentSpec(newMacros);
    
    /* Fill the dialog with the new language mode information */
    SmartIndentDialog.langModeName = NEditStrdup(modeName);
    setSmartIndentDialogData(findIndentSpec(modeName));
}

static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData)
{
    EditLanguageModes();
}

static void commonDialogCB(Widget w, XtPointer clientData, XtPointer callData)
{
    EditCommonSmartIndentMacro();
}

static void okCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* change the macro */
    if (!updateSmartIndentData())
    	return;
    
    /* pop down and destroy the dialog */
    CloseAllPopupsFor(SmartIndentDialog.shell);
    XtDestroyWidget(SmartIndentDialog.shell);
}

static void applyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* change the patterns */
    updateSmartIndentData();
}
	
static void checkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (checkSmartIndentDialogData())
        DialogF(DF_INF, SmartIndentDialog.shell, 1, "Macro compiled",
                "Macros compiled without error", "OK");
}
	
static void restoreCB(Widget w, XtPointer clientData, XtPointer callData)
{
   int i;
   smartIndentRec *defaultIS;
    
    /* Find the default indent spec */
    for (i=0; i<N_DEFAULT_INDENT_SPECS; i++)
    {
        if (!strcmp(SmartIndentDialog.langModeName,
                DefaultIndentSpecs[i].lmName))
        {
            break;
        }
    }

    if (i == N_DEFAULT_INDENT_SPECS)
    {
        DialogF(DF_WARN, SmartIndentDialog.shell, 1, "Smart Indent",
                "There are no default indent macros\nfor language mode %s",
                "OK", SmartIndentDialog.langModeName);
        return;
    }
    defaultIS = &DefaultIndentSpecs[i];
    
    if (DialogF(DF_WARN, SmartIndentDialog.shell, 2, "Discard Changes",
            "Are you sure you want to discard\n"
            "all changes to smart indent macros\n"
            "for language mode %s?", "Discard", "Cancel",
            SmartIndentDialog.langModeName) == 2)
    {
        return;
    }
    
    /* if a stored version of the indent macros exist, replace them, if not,
       add a new one */
    for (i=0; i<NSmartIndentSpecs; i++)
    	if (!strcmp(SmartIndentDialog.langModeName,SmartIndentSpecs[i]->lmName))
	    break;
    if (i < NSmartIndentSpecs) {
     	freeIndentSpec(SmartIndentSpecs[i]);
   	SmartIndentSpecs[i] = copyIndentSpec(defaultIS);
    } else
    	SmartIndentSpecs[NSmartIndentSpecs++] = copyIndentSpec(defaultIS);
   
    /* Update the dialog */
    setSmartIndentDialogData(defaultIS);
}
	
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int i;
    
    if (DialogF(DF_WARN, SmartIndentDialog.shell, 2, "Delete Macros",
            "Are you sure you want to delete smart indent\n"
            "macros for language mode %s?", "Yes, Delete", "Cancel",
            SmartIndentDialog.langModeName) == 2)
    {
        return;
    }

    /* if a stored version of the pattern set exists, delete it from the list */
    for (i=0; i<NSmartIndentSpecs; i++)
    	if (!strcmp(SmartIndentDialog.langModeName,SmartIndentSpecs[i]->lmName))
	    break;
    if (i < NSmartIndentSpecs) {
     	freeIndentSpec(SmartIndentSpecs[i]);
   	memmove(&SmartIndentSpecs[i], &SmartIndentSpecs[i+1],
   	    	(NSmartIndentSpecs-1 - i) * sizeof(smartIndentRec *));
    	NSmartIndentSpecs--;
    }
    
    /* Clear out the dialog */
    setSmartIndentDialogData(NULL);
}

static void closeCB(Widget widget, XtPointer clientData, XtPointer callData)
{
    /* pop down and destroy the dialog */
    CloseAllPopupsFor(SmartIndentDialog.shell);
    XtDestroyWidget(SmartIndentDialog.shell);
}

static void helpCB(Widget w, XtPointer clientData, XtPointer callData)
{
    Help(HELP_SMART_INDENT);
}

static int checkSmartIndentDialogData(void)
{
    char *widgetText, *errMsg, *stoppedAt;
    Program *prog;
    
    /* Check the initialization macro */
    if (!TextWidgetIsBlank(SmartIndentDialog.initMacro)) {
	widgetText =ensureNewline(XmTextGetString(SmartIndentDialog.initMacro));
	if (!CheckMacroString(SmartIndentDialog.shell, widgetText,
		"initialization macro", &stoppedAt)) {
    	    XmTextSetInsertionPosition(SmartIndentDialog.initMacro,
		    stoppedAt - widgetText);
	    XmProcessTraversal(SmartIndentDialog.initMacro, XmTRAVERSE_CURRENT);
	    NEditFree(widgetText);
	    return False;
	}
	NEditFree(widgetText);
    }
    
    /* Test compile the newline macro */
    if (TextWidgetIsBlank(SmartIndentDialog.newlineMacro))
    {
        DialogF(DF_WARN, SmartIndentDialog.shell, 1, "Smart Indent",
                "Newline macro required", "OK");
        return False;
    }

    widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.newlineMacro));
    prog = ParseMacro(widgetText, &errMsg, &stoppedAt);
    if (prog == NULL) {
 	ParseError(SmartIndentDialog.shell, widgetText, stoppedAt,
    	    	"newline macro", errMsg);
     	XmTextSetInsertionPosition(SmartIndentDialog.newlineMacro,
		stoppedAt - widgetText);
	XmProcessTraversal(SmartIndentDialog.newlineMacro, XmTRAVERSE_CURRENT);
	NEditFree(widgetText);
    	return False;
    }
    NEditFree(widgetText);
    FreeProgram(prog);
    
    /* Test compile the modify macro */
    if (!TextWidgetIsBlank(SmartIndentDialog.modMacro)) {
    	widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.modMacro));
    	prog = ParseMacro(widgetText, &errMsg, &stoppedAt);
	if (prog == NULL) {
    	    ParseError(SmartIndentDialog.shell, widgetText, stoppedAt,
    	    	    "modify macro", errMsg);
     	    XmTextSetInsertionPosition(SmartIndentDialog.modMacro,
		    stoppedAt - widgetText);
	    XmProcessTraversal(SmartIndentDialog.modMacro, XmTRAVERSE_CURRENT);
	    NEditFree(widgetText);
    	    return False;
    	}
	NEditFree(widgetText);
	FreeProgram(prog);
    }
    return True;
}

static smartIndentRec *getSmartIndentDialogData(void)
{
    smartIndentRec *is;
    
    is = (smartIndentRec *)NEditMalloc(sizeof(smartIndentRec));
    is->lmName = NEditStrdup(SmartIndentDialog.langModeName);
    is->initMacro = TextWidgetIsBlank(SmartIndentDialog.initMacro) ? NULL :
	    ensureNewline(XmTextGetString(SmartIndentDialog.initMacro));
    is->newlineMacro = TextWidgetIsBlank(SmartIndentDialog.newlineMacro) ? NULL:
	    ensureNewline(XmTextGetString(SmartIndentDialog.newlineMacro));
    is->modMacro = TextWidgetIsBlank(SmartIndentDialog.modMacro) ? NULL :
	    ensureNewline(XmTextGetString(SmartIndentDialog.modMacro));
    return is;
}

static void setSmartIndentDialogData(smartIndentRec *is)
{
    if (is == NULL) {
	XmTextSetString(SmartIndentDialog.initMacro, "");
	XmTextSetString(SmartIndentDialog.newlineMacro, "");
	XmTextSetString(SmartIndentDialog.modMacro, "");
    } else {
	if (is->initMacro == NULL)
	    XmTextSetString(SmartIndentDialog.initMacro, "");
	else
	    XmTextSetString(SmartIndentDialog.initMacro, is->initMacro);
	XmTextSetString(SmartIndentDialog.newlineMacro, is->newlineMacro);
	if (is->modMacro == NULL)
	    XmTextSetString(SmartIndentDialog.modMacro, "");
	else
	    XmTextSetString(SmartIndentDialog.modMacro, is->modMacro);
    }
}

void EditCommonSmartIndentMacro(void)
{
#define VERT_BORDER 4
    Widget form, topLbl;
    Widget okBtn, applyBtn, checkBtn;
    Widget closeBtn, restoreBtn;
    XmString s1;
    Arg args[20];
    int n;

    /* if the dialog is already displayed, just pop it to the top and return */
    if (CommonDialog.shell != NULL) {
    	RaiseDialogWindow(CommonDialog.shell);
    	return;
    }

    /* Create a form widget in an application shell */
    n = 0;
    XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); n++;
    XtSetArg(args[n], XmNiconName, "NEdit Common Smart Indent Macros"); n++;
    XtSetArg(args[n], XmNtitle, "Common Smart Indent Macros"); n++;
    CommonDialog.shell = CreateWidget(TheAppShell, "smartIndent",
	    topLevelShellWidgetClass, args, n);
    AddSmallIcon(CommonDialog.shell);
    form = XtVaCreateManagedWidget("editCommonSIMacros", xmFormWidgetClass,
	    CommonDialog.shell, XmNautoUnmanage, False,
	    XmNresizePolicy, XmRESIZE_NONE, NULL);
    XtAddCallback(form, XmNdestroyCallback, comDestroyCB, NULL);
    AddMotifCloseCallback(CommonDialog.shell, comCloseCB, NULL);
    
    topLbl = XtVaCreateManagedWidget("topLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple(
	        "Common Definitions for Smart Indent Macros"),
    	    XmNmnemonic, 'C',
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNtopOffset, VERT_BORDER,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);

    okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 6,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 18,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, VERT_BORDER, NULL);
    XtAddCallback(okBtn, XmNactivateCallback, comOKCB, NULL);
    XmStringFree(s1);
    
    applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Apply"),
    	    XmNmnemonic, 'y',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 22,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 35,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, VERT_BORDER, NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, comApplyCB, NULL);
    XmStringFree(s1);
    
    checkBtn = XtVaCreateManagedWidget("check", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Check"),
    	    XmNmnemonic, 'k',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 39,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 52,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, VERT_BORDER, NULL);
    XtAddCallback(checkBtn, XmNactivateCallback, comCheckCB, NULL);
    XmStringFree(s1);
    
    restoreBtn = XtVaCreateManagedWidget("restore", xmPushButtonWidgetClass,
    form,
    	    XmNlabelString, s1=XmStringCreateSimple("Restore Default"),
    	    XmNmnemonic, 'f',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 56,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 77,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, VERT_BORDER, NULL);
    XtAddCallback(restoreBtn, XmNactivateCallback, comRestoreCB, NULL);
    XmStringFree(s1);
    
    closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass,
    	    form,
    	    XmNlabelString, s1=XmStringCreateSimple("Close"),
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 81,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 94,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, VERT_BORDER, NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, comCloseCB, NULL);
    XmStringFree(s1);
    
    n = 0;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNrows, 24); n++;
    XtSetArg(args[n], XmNcolumns, 80); n++;
    XtSetArg(args[n], XmNvalue, CommonMacros); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, topLbl); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 99); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, okBtn); n++;
    XtSetArg(args[n], XmNbottomOffset, VERT_BORDER); n++;
    CommonDialog.text = XmCreateScrolledText(form, "commonText", args, n);
    AddMouseWheelSupport(CommonDialog.text);
    XtManageChild(CommonDialog.text);
    RemapDeleteKey(CommonDialog.text);
    XtVaSetValues(topLbl, XmNuserData, CommonDialog.text, NULL);

    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);
    
    /* Realize all of the widgets in the new dialog */
    RealizeWithoutForcingPosition(CommonDialog.shell);
}

static void comDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    CommonDialog.shell = NULL;
}

static void comOKCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* change the macro */
    if (!updateSmartIndentCommonData())
    	return;
    
    /* pop down and destroy the dialog */
    XtDestroyWidget(CommonDialog.shell);
}

static void comApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* change the macro */
    updateSmartIndentCommonData();
}

static void comCheckCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (checkSmartIndentCommonDialogData())
    {
        DialogF(DF_INF, CommonDialog.shell, 1, "Macro compiled",
                "Macros compiled without error", "OK");
    }
}

static void comRestoreCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (DialogF(DF_WARN, CommonDialog.shell, 2, "Discard Changes",
            "Are you sure you want to discard all\n"
            "changes to common smart indent macros", "Discard", "Cancel") == 2)
    {
        return;
    }
    
    /* replace common macros with default */
    NEditFree(CommonMacros);
    CommonMacros = NEditStrdup(DefaultCommonMacros);
   
    /* Update the dialog */
    XmTextSetString(CommonDialog.text, CommonMacros);
}

static void comCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* pop down and destroy the dialog */
    XtDestroyWidget(CommonDialog.shell);
}

/*
** Update the smart indent macros being edited in the dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the macros.
*/
static int updateSmartIndentCommonData(void)
{
    WindowInfo *window;
    	
    /* Make sure the patterns are valid and compile */
    if (!checkSmartIndentCommonDialogData())
    	return False;
    
    /* Get the current data */
    CommonMacros = ensureNewline(XmTextGetString(CommonDialog.text));
    
    /* Re-execute initialization macros (macros require a window to function,
       since user could theoretically execute an action routine, but it
       probably won't be referenced in a smart indent initialization) */
    if (!ReadMacroString(WindowList, CommonMacros, "common macros"))
    	return False;

    /* Find windows that are currently using smart indent and
       re-initialize the smart indent macros (in case they have initialization
       data which depends on common data) */
    for (window=WindowList; window!=NULL; window=window->next) {
    	if (window->indentStyle == SMART_INDENT &&
    		window->languageMode != PLAIN_LANGUAGE_MODE) {
    	    EndSmartIndent(window);
    	    BeginSmartIndent(window, False);
    	}
    }
    
    /* Note that preferences have been changed */
    MarkPrefsChanged();

    return True;
}

static int checkSmartIndentCommonDialogData(void)
{
    char *widgetText, *stoppedAt;
    
    if (!TextWidgetIsBlank(CommonDialog.text)) {
	widgetText = ensureNewline(XmTextGetString(CommonDialog.text));
	if (!CheckMacroString(CommonDialog.shell, widgetText,
		"macros", &stoppedAt)) {
    	    XmTextSetInsertionPosition(CommonDialog.text, stoppedAt-widgetText);
	    XmProcessTraversal(CommonDialog.text, XmTRAVERSE_CURRENT);
	    NEditFree(widgetText);
	    return False;
	}
	NEditFree(widgetText);
    }
    return True;
}

/*
** Update the smart indent macros being edited in the dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the macros.
*/
static int updateSmartIndentData(void)
{
    smartIndentRec *newMacros;
    WindowInfo *window;
    char *lmName;
    int i;
    	
    /* Make sure the patterns are valid and compile */
    if (!checkSmartIndentDialogData())
    	return False;
    
    /* Get the current data */
    newMacros = getSmartIndentDialogData();
    
    /* Find the original macros */
    for (i=0; i<NSmartIndentSpecs; i++)
    	if (!strcmp(SmartIndentDialog.langModeName,SmartIndentSpecs[i]->lmName))
	    break;
    
    /* If it's a new language, add it at the end, otherwise free the
       existing macros and replace it */
    if (i == NSmartIndentSpecs) {
    	SmartIndentSpecs[NSmartIndentSpecs++] = newMacros;
    } else {
	freeIndentSpec(SmartIndentSpecs[i]);
	SmartIndentSpecs[i] = newMacros;
    }
    
    /* Find windows that are currently using this indent specification and
       re-do the smart indent macros */
    for (window=WindowList; window!=NULL; window=window->next) {
    	lmName = LanguageModeName(window->languageMode);
	if (lmName != NULL && !strcmp(lmName, newMacros->lmName)) {
	    SetSensitive(window, window->smartIndentItem, True);
    	    if (window->indentStyle == SMART_INDENT &&
    		    window->languageMode != PLAIN_LANGUAGE_MODE) {
    	    	EndSmartIndent(window);
    	    	BeginSmartIndent(window, False);
    	    }
    	}
    }
    
    /* Note that preferences have been changed */
    MarkPrefsChanged();

    return True;
}

static int loadDefaultIndentSpec(char *lmName)
{
    int i;
    
    for (i=0; i<N_DEFAULT_INDENT_SPECS; i++) {
    	if (!strcmp(lmName, DefaultIndentSpecs[i].lmName)) {
    	    SmartIndentSpecs[NSmartIndentSpecs++] =
		    copyIndentSpec(&DefaultIndentSpecs[i]);
    	    return True;
    	}
    }
    return False;
}

int LoadSmartIndentString(char *inString)
{
   char *errMsg, *inPtr = inString;
   smartIndentRec is, *isCopy;
   int i;

   for (;;) {
   	
	/* skip over blank space */
	inPtr += strspn(inPtr, " \t\n");
	
	/* finished */
	if (*inPtr == '\0')
	    return True;

	/* read language mode name */
	is.lmName = ReadSymbolicField(&inPtr);
	if (is.lmName == NULL)
    	    return siParseError(inString, inPtr, "language mode name required");
	if (!SkipDelimiter(&inPtr, &errMsg)) {
	    NEditFree(is.lmName);
    	    return siParseError(inString, inPtr, errMsg);
    	}
    	
	/* look for "Default" keyword, and if it's there, return the default
	   smart indent macros */
	if (!strncmp(inPtr, "Default", 7)) {
    	    inPtr += 7;
    	    if (!loadDefaultIndentSpec(is.lmName)) {
    		NEditFree(is.lmName);
    		return siParseError(inString, inPtr,
    	    		"no default smart indent macros");
    	    }
	    NEditFree(is.lmName);
    	    continue;
	}

	/* read the initialization macro (arbitrary text terminated by the
	   macro end boundary string) */
	is.initMacro = readSIMacro(&inPtr);
	if (is.initMacro == NULL) {
    	    NEditFree(is.lmName);
    	    return siParseError(inString, inPtr,
    	    	    "no end boundary to initialization macro");
	}
	
	/* read the newline macro */
	is.newlineMacro = readSIMacro(&inPtr);
	if (is.newlineMacro == NULL) {
    	    NEditFree(is.lmName);
    	    NEditFree(is.initMacro);
    	    return siParseError(inString, inPtr,
    	    	    "no end boundary to newline macro");
	}
	
	/* read the modify macro */
	is.modMacro = readSIMacro(&inPtr);
	if (is.modMacro == NULL) {
    	    NEditFree(is.lmName);
    	    NEditFree(is.initMacro);
    	    NEditFree(is.newlineMacro);
    	    return siParseError(inString, inPtr,
    	    	    "no end boundary to modify macro");
	}
	
	/* if there's no mod macro, make it null so it won't be executed */
	if (is.modMacro[0] == '\0') {
	    NEditFree(is.modMacro);
            is.modMacro = NULL;
    	}
    	
    	/* create a new data structure and add/change it in the list */
	isCopy = (smartIndentRec *)NEditMalloc(sizeof(smartIndentRec));
	*isCopy = is;
	for (i=0; i<NSmartIndentSpecs; i++) {
	    if (!strcmp(SmartIndentSpecs[i]->lmName, is.lmName)) {
		freeIndentSpec(SmartIndentSpecs[i]);
		SmartIndentSpecs[i] = isCopy;
		break;
	    }
	}
	if (i == NSmartIndentSpecs)
	    SmartIndentSpecs[NSmartIndentSpecs++] = isCopy;
    }
}

int LoadSmartIndentCommonString(char *inString)
{
    int shiftedLen;
    char *inPtr = inString;
    
    /* If called from -import, can replace existing ones */
    NEditFree(CommonMacros);
    
    /* skip over blank space */
    inPtr += strspn(inPtr, " \t\n");

    /* look for "Default" keyword, and if it's there, return the default
       smart common macro */
    if (!strncmp(inPtr, "Default", 7)) {
    	CommonMacros = NEditStrdup(DefaultCommonMacros);
	return True;
    }
        
    /* Remove leading tabs added by writer routine */
    CommonMacros = ShiftText(inPtr, SHIFT_LEFT, True, 8, 8, &shiftedLen);
    return True;
}

/*
** Read a macro (arbitrary text terminated by the macro end boundary string)
** from the position pointed to by *inPtr, trim off added tabs and return an
** allocated copy of the string, and advance *inPtr to the end of the macro.
** Returns NULL if the macro end boundary string is not found.
*/
static char *readSIMacro(char **inPtr)
{
    char *retStr, *macroStr, *macroEnd;
    int shiftedLen;
    
    /* Strip leading newline */
    if (**inPtr == '\n')
    	(*inPtr)++;
    
    /* Find the end of the macro */
    macroEnd = strstr(*inPtr, MacroEndBoundary);
    if (macroEnd == NULL)
	return NULL;
    
    /* Copy the macro */
    macroStr = (char*)NEditMalloc(macroEnd - *inPtr + 1);
    strncpy(macroStr, *inPtr, macroEnd - *inPtr);
    macroStr[macroEnd - *inPtr] = '\0';
    
    /* Remove leading tabs added by writer routine */
    *inPtr = macroEnd + strlen(MacroEndBoundary);
    retStr = ShiftText(macroStr, SHIFT_LEFT, True, 8, 8, &shiftedLen);
    NEditFree(macroStr);
    return retStr;
}

static smartIndentRec *copyIndentSpec(smartIndentRec *is)
{
    smartIndentRec *ris = (smartIndentRec *)NEditMalloc(sizeof(smartIndentRec));
    ris->lmName = NEditStrdup(is->lmName);
    ris->initMacro = NEditStrdup(is->initMacro);
    ris->newlineMacro = NEditStrdup(is->newlineMacro);
    ris->modMacro = NEditStrdup(is->modMacro);
    return ris;
}

static void freeIndentSpec(smartIndentRec *is)
{
    NEditFree(is->lmName);
    NEditFree(is->initMacro);
    NEditFree(is->newlineMacro);
    NEditFree(is->modMacro);
}

static int indentSpecsDiffer(smartIndentRec *is1, smartIndentRec *is2)
{
    return AllocatedStringsDiffer(is1->initMacro, is2->initMacro) ||
	    AllocatedStringsDiffer(is1->newlineMacro, is2->newlineMacro) ||
	    AllocatedStringsDiffer(is1->modMacro, is2->modMacro);
}

static int siParseError(char *stringStart, char *stoppedAt, char *message)
{
    return ParseError(NULL, stringStart, stoppedAt,
    	    "smart indent specification", message);
}

char *WriteSmartIndentString(void)
{
    int i;
    smartIndentRec *sis;
    textBuffer *outBuf;
    char *outStr, *escapedStr;
    
    outBuf = BufCreate();
    for (i=0; i<NSmartIndentSpecs; i++) {
    	sis = SmartIndentSpecs[i];
    	BufInsert(outBuf, outBuf->length, "\t");
    	BufInsert(outBuf, outBuf->length, sis->lmName);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (isDefaultIndentSpec(sis))
    	    BufInsert(outBuf, outBuf->length, "Default\n");
    	else {
    	    insertShiftedMacro(outBuf, sis->initMacro);
    	    insertShiftedMacro(outBuf, sis->newlineMacro);
    	    insertShiftedMacro(outBuf, sis->modMacro);
    	}
    }
    
    /* Get the output string, and lop off the trailing newline */
    outStr = BufGetRange(outBuf, 0, outBuf->length > 0 ? outBuf->length-1 : 0);
    BufFree(outBuf);
    
    /* Protect newlines and backslashes from translation by the resource
       reader */
    escapedStr = EscapeSensitiveChars(outStr);
    NEditFree(outStr);
    return escapedStr;
}

char *WriteSmartIndentCommonString(void)
{
    int len;
    char *outStr, *escapedStr;
    
    if (!strcmp(CommonMacros, DefaultCommonMacros))
    	return NEditStrdup("Default");
    if (CommonMacros == NULL)
    	return NEditStrdup("");
    
    /* Shift the macro over by a tab to keep .nedit file bright and clean */
    outStr = ShiftText(CommonMacros, SHIFT_RIGHT, True, 8, 8, &len);
	
    /* Protect newlines and backslashes from translation by the resource
       reader */
    escapedStr = EscapeSensitiveChars(outStr);
    NEditFree(outStr);
    
    /* If there's a trailing escaped newline, remove it */
    len = strlen(escapedStr);
    if (len > 1 && escapedStr[len-1] == '\n' && escapedStr[len-2] == '\\')
    	escapedStr[len-2] = '\0';
    return escapedStr;
}

/*
** Insert macro text "macro" into buffer "buf" shifted right by 8 characters
** (so it looks nice in the .nedit file), and terminated with a macro-end-
** boundary string.
*/
static void insertShiftedMacro(textBuffer *buf, char  *macro)
{
    char *shiftedMacro;
    int shiftedLen;
    
    if (macro != NULL) {
	shiftedMacro = ShiftText(macro, SHIFT_RIGHT, True, 8, 8, &shiftedLen);
	BufInsert(buf, buf->length, shiftedMacro);
	NEditFree(shiftedMacro);
    }
    BufInsert(buf, buf->length, "\t");
    BufInsert(buf, buf->length, MacroEndBoundary);
    BufInsert(buf, buf->length, "\n");
}

static int isDefaultIndentSpec(smartIndentRec *indentSpec)
{
    int i;
   
    for (i=0; i<N_DEFAULT_INDENT_SPECS; i++)
    	if (!strcmp(indentSpec->lmName, DefaultIndentSpecs[i].lmName))
    	    return !indentSpecsDiffer(indentSpec, &DefaultIndentSpecs[i]);
    return False;
}
    
static smartIndentRec *findIndentSpec(const char *modeName)
{
    int i;

    if (modeName == NULL)
    	return NULL;
    
    for (i=0; i<NSmartIndentSpecs; i++)
    	if (!strcmp(modeName, SmartIndentSpecs[i]->lmName))
    	    return SmartIndentSpecs[i];
    return NULL;
}

/*
** If "string" is not terminated with a newline character,  return a
** reallocated string which does end in a newline (otherwise, just pass on
** string as function value).  (The macro language requires newline terminators
** for statements, but the text widget doesn't force it like the NEdit text
** buffer does, so this might avoid some confusion.)
*/
static char *ensureNewline(char *string)
{
    char *newString;
    int length;
    
    if (string == NULL)
	return NULL;
    length = strlen(string);
    if (length == 0 || string[length-1] == '\n')
	return string;
    newString = (char*)NEditMalloc(length + 2);
    strcpy(newString, string);
    newString[length] = '\n';
    newString[length+1] = '\0';
    NEditFree(string);
    return newString;
}

/*
** Returns True if there are smart indent macros, or potential macros
** not yet committed in the smart indent dialog for a language mode,
*/
int LMHasSmartIndentMacros(const char *languageMode)
{
    if (findIndentSpec(languageMode) != NULL)
    	return True;
    return SmartIndentDialog.shell!=NULL && !strcmp(SmartIndentDialog.langModeName,
    	    languageMode);
}

/*
** Change the language mode name of smart indent macro sets for language 
** "oldName" to "newName" in both the stored macro sets, and the pattern set
** currently being edited in the dialog.
*/
void RenameSmartIndentMacros(const char *oldName, const char *newName)
{
    int i;

    for (i=0; i<NSmartIndentSpecs; i++) {
    	if (!strcmp(oldName, SmartIndentSpecs[i]->lmName)) {
    	    NEditFree(SmartIndentSpecs[i]->lmName);
    	    SmartIndentSpecs[i]->lmName = NEditStrdup(newName);
    	}
    }
    if (SmartIndentDialog.shell != NULL) {
    	if (!strcmp(SmartIndentDialog.langModeName, oldName)) {
    	    NEditFree(SmartIndentDialog.langModeName);
    	    SmartIndentDialog.langModeName = NEditStrdup(newName);
    	}
    }
}

/*
** If a smart indent dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLangModeMenuSmartIndent(void)
{
    Widget oldMenu;

    if (SmartIndentDialog.shell == NULL)
    	return;

    oldMenu = SmartIndentDialog.lmPulldown;
    SmartIndentDialog.lmPulldown = CreateLanguageModeMenu(
    	    XtParent(XtParent(oldMenu)), langModeCB, NULL);
    XtVaSetValues(XmOptionButtonGadget(SmartIndentDialog.lmOptMenu),
    	    XmNsubMenuId, SmartIndentDialog.lmPulldown, NULL);
    SetLangModeMenu(SmartIndentDialog.lmOptMenu, SmartIndentDialog.langModeName);

    XtDestroyWidget(oldMenu);
}
