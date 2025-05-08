/*******************************************************************************
*									       *
* preferences.c -- Nirvana Editor preferences processing		       *
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
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License *
* for more details.							       *
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* April 20, 1993							       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "preferences.h"
#include "textBuf.h"
#include "nedit.h"
#include "menu.h"
#include "text.h"
#include "search.h"
#include "window.h"
#include "userCmds.h"
#include "highlight.h"
#include "highlightData.h"
#include "help.h"
#include "regularExp.h"
#include "smartIndent.h"
#include "windowTitle.h"
#include "server.h"
#include "tags.h"
#include "filter.h"
#include "../util/prefFile.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/managedList.h"
#include "../util/fontsel.h"
#include "../util/fileUtils.h"
#include "../util/utils.h"
#include "../util/nedit_malloc.h"
#include "../util/colorchooser.h"
#include "../util/filedialog.h"
#include "../util/textfield.h"

#include <ctype.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <langinfo.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#include "../util/clearcase.h"

#include <Xm/Xm.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/SeparatoG.h>
#include <Xm/LabelG.h>
#include <Xm/Label.h>
#include <Xm/PushBG.h>
#include <Xm/PushB.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeBG.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/Separator.h>
#include <Xm/ScrolledW.h>
#include <Xm/ArrowB.h>
#include <Xm/ComboBox.h>
#include <Xm/DrawingA.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#if XmVersion >= 1002
#define MENU_WIDGET(w) (XmGetPostedFromWidget(XtParent(w)))
#else
#define MENU_WIDGET(w) (w)
#endif

#define PREF_FILE_VERSION "6.3"

/* New styles added in 5.2 for auto-upgrade */
#define ADD_5_2_STYLES " Pointer:#660000:Bold\nRegex:#009944:Bold\nWarning:brown2:Italic\n"

/* New Styles added in 6.1 */
#define ADD_6_1_STYLES " Emphasis:black:Italic\nStrong:black:Bold\nHeader:brown:Bold\n"

/* maximum number of word delimiters allowed (256 allows whole character set) */
#define MAX_WORD_DELIMITERS 256

/* maximum number of file extensions allowed in a language mode */
#define MAX_FILE_EXTENSIONS 20

/* Return values for checkFontStatus */
enum fontStatus {GOOD_FONT, BAD_PRIMARY, BAD_FONT, BAD_SIZE, BAD_SPACING};

/* enumerated type preference strings 
** The order of the elements in this array must be exactly the same
** as the order of the corresponding integers of the enum SearchType
** defined in search.h (!!)
*/
static char *SearchMethodStrings[] = {
  	"Literal", "CaseSense", "RegExp", 
	"LiteralWord", "CaseSenseWord", "RegExpNoCase", 
	NULL
};

static ColorProfile *colorProfiles;
static ColorProfile *defaultColorProfile;

#ifdef REPLACE_SCOPE
/* enumerated default scope for replace dialog if a selection exists when
** the dialog is popped up.
*/
static char *ReplaceDefScopeStrings[] = {
	"Window", "Selection", "Smart", NULL
};
#endif

#define N_WRAP_STYLES 3
static char *AutoWrapTypes[N_WRAP_STYLES+3] = {"None", "Newline", "Continuous",
    	"True", "False", NULL};
#define N_INDENT_STYLES 3
static char *AutoIndentTypes[N_INDENT_STYLES+3] = {"None", "Auto",
    	"Smart", "True", "False", NULL};
#define N_VIRTKEY_OVERRIDE_MODES 3
static char *VirtKeyOverrideModes[N_VIRTKEY_OVERRIDE_MODES+1] = { "Never",
	"Auto", "Always", NULL};

#define N_SHOW_MATCHING_STYLES 3
/* For backward compatibility, "False" and "True" are still accepted.
   They are internally converted to "Off" and "Delimiter" respectively. 
   NOTE: N_SHOW_MATCHING_STYLES must correspond to the number of 
         _real_ matching styles, not counting False & True. 
         False and True should also be the last ones in the list. */
static char *ShowMatchingTypes[] = {"Off", "Delimiter", "Range", 
	"False", "True", NULL};

/*  This array must be kept in parallel to the enum truncSubstitution
    in nedit.h  */
static char* TruncSubstitutionModes[] = {"Silent", "Fail", "Warn", "Ignore", NULL};

/* suplement wrap and indent styles w/ a value meaning "use default" for
   the override fields in the language modes dialog */
#define DEFAULT_WRAP -1
#define DEFAULT_INDENT -1
#define DEFAULT_TAB_DIST -1
#define DEFAULT_EM_TAB_DIST -1

/* list of available language modes and language specific preferences */
static int NLanguageModes = 0;
typedef struct {
    char *name;
    int nExtensions;
    char **extensions;
    char *recognitionExpr;
    char *defTipsFile;
    char *delimiters;
    int wrapStyle;	
    int indentStyle;	
    int tabDist;	
    int emTabDist;	
} languageModeRec;
static languageModeRec *LanguageModes[MAX_LANGUAGE_MODES];

/* Language mode dialog information */
static struct {
    Widget shell;
    Widget nameW;
    Widget extW;
    Widget recogW;
    Widget defTipsW;
    Widget delimitW;
    Widget managedListW;
    Widget tabW;
    Widget emTabW;
    Widget defaultIndentW;
    Widget noIndentW;
    Widget autoIndentW;
    Widget smartIndentW;
    Widget defaultWrapW;
    Widget noWrapW;
    Widget newlineWrapW;
    Widget contWrapW;
    languageModeRec **languageModeList;
    int nLanguageModes;
} LMDialog = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
              NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0};

/* Font dialog information */
typedef struct {
    Widget shell;
    Widget primaryW;
    Widget fillW;
    Widget italicW;
    Widget italicErrW;
    Widget boldW;
    Widget boldErrW;
    Widget boldItalicW;
    Widget boldItalicErrW;
    WindowInfo *window;
    int forWindow;
} fontDialog;

typedef struct {
    Widget remove;
    Widget up;
    Widget down;
    Widget textfield;
    Widget colorchooser;
} indentRainbowColor;

/* Color dialog information */
typedef struct {
    Widget shell;
    WindowInfo *window;
    
    Widget profileDropDown;
    
    Widget tabs[4];
    Widget tabForms[4];
    
    // general colors
    Widget textFgW;
    Widget textFgErrW;
    Widget textBgW;
    Widget textBgErrW;
    Widget selectFgW;
    Widget selectFgErrW;
    Widget selectBgW;
    Widget selectBgErrW;
    Widget hiliteFgW;
    Widget hiliteFgErrW;
    Widget hiliteBgW;
    Widget hiliteBgErrW;
    Widget lineNoFgW;
    Widget lineNoFgErrW;
    Widget lineNoBgW;
    Widget lineNoBgErrW;
    Widget cursorFgW;
    Widget cursorFgErrW;
    Widget cursorLineBgW;
    Widget cursorLineBgErrW;
    
    // indent rainbow
    Widget scrollW;
    Widget btnForm;
    Widget addBtn;
    indentRainbowColor *indentRainbowColors;
    size_t allocIndentRainbowColors;
    size_t numIndentRainbowColors;
    
    // ansi
    Widget ansiBlackW;
    Widget ansiBlackErrW;
    Widget ansiRedW;
    Widget ansiRedErrW;
    Widget ansiGreenW;
    Widget ansiGreenErrW;
    Widget ansiYellowW;
    Widget ansiYellowErrW;
    Widget ansiBlueW;
    Widget ansiBlueErrW;
    Widget ansiMagentaW;
    Widget ansiMagentaErrW;
    Widget ansiCyanW;
    Widget ansiCyanErrW;
    Widget ansiWhiteW;
    Widget ansiWhiteErrW;
    Widget ansiBrightBlackW;
    Widget ansiBrightBlackErrW;
    Widget ansiBrightRedW;
    Widget ansiBrightRedErrW;
    Widget ansiBrightGreenW;
    Widget ansiBrightGreenErrW;
    Widget ansiBrightYellowW;
    Widget ansiBrightYellowErrW;
    Widget ansiBrightBlueW;
    Widget ansiBrightBlueErrW;
    Widget ansiBrightMagentaW;
    Widget ansiBrightMagentaErrW;
    Widget ansiBrightCyanW;
    Widget ansiBrightCyanErrW;
    Widget ansiBrightWhiteW;
    Widget ansiBrightWhiteErrW;

    // styles
    Widget styleDefaultW;
    Widget styleLightenW;
    Widget styleCustomW;
    Widget styleXResLabelW;
    Widget styleXResW;
    //Widget styleWMDefaultW;
    //Widget styleWMLightW;
    //Widget styleWMDarkW;
    
    ColorProfile *colorProfiles;
    size_t numColorProfiles;
    int selectedProfile;
} colorDialog;

/* Repository for simple preferences settings */
static struct prefData {
    int openInTab;		/* open files in new tabs  */
    int wrapStyle;		/* what kind of wrapping to do */
    int wrapMargin;		/* 0=wrap at window width, other=wrap margin */
    int autoIndent;		/* style for auto-indent */
    int autoSave;		/* whether automatic backup feature is on */
    int saveOldVersion;		/* whether to preserve a copy of last version */
    int saveSearchHistory;      /* whether to store search/replace history */
    int searchDlogs;		/* whether to show explanatory search dialogs */
    int searchWrapBeep;     	/* 1=beep when search restarts at begin/end */
    int keepSearchDlogs;	/* whether to retain find and replace dialogs */
    int searchWraps;	/* whether to attempt search again if reach bof or eof */
    int statsLine;		/* whether to show the statistics line */
    int iSearchLine;	    	/* whether to show the incremental search line*/
    int tabBar;			/* whether to show the tab bar */
    int tabBarHideOne;		/* hide tab bar if only one document in window */
    int globalTabNavigate;  	/* prev/next document across windows */
    int toolTips;	    	/* whether to show the tooltips */
    int lineNums;   	    	/* whether to show line numbers */
    int pathInWindowsMenu;   	/* whether to show path in windows menu */
    int warnFileMods;	    	/* warn user if files externally modified */
    int warnRealFileMods;	/* only warn if file contents modified */
    int warnExit;	    	/* whether to warn on exit */
    int searchMethod;		/* initial search method as a text string */
#ifdef REPLACE_SCOPE
    int replaceDefScope;	/* default replace scope if selection exists */
#endif
    int textRows;		/* initial window height in characters */
    int textCols;		/* initial window width in characters */
    int tabDist;		/* number of characters between tab stops */
    int emTabDist;		/* non-zero tab dist. if emulated tabs are on */
    int insertTabs;		/* whether to use tabs for padding */
    int showMatchingStyle;	/* how to flash matching parenthesis */
    int matchSyntaxBased;	/* use syntax info to match parenthesis */
    int highlightSyntax;    	/* whether to highlight syntax by default */
    int smartTags;  	    	/* look for tag in current window first */
    int alwaysCheckRelativeTagsSpecs; /* for every new opened file of session */
    int stickyCaseSenseBtn;     /* whether Case Word Btn is sticky to Regex Btn */
    int prefFileRead;	    	/* detects whether a .nedit existed */
    int highlightCursorLine;    /* highlight line of cursor */
    int indentRainbow;          /* highlight indentation level */
    char *indentRainbowColors;  /* indent rainbow colors */
    char *ansiColorList;        /* ANSI color table (16 colors) */
    int ansiColors;             /* coloring from ANSI escape sequences  */
    int backlightChars;		/* whether to apply character "backlighting" */
    char *backlightCharTypes;	/* the backlighting color definitions */
#ifdef SGI_CUSTOM
    int shortMenus; 	    	/* short menu mode */
#endif
    char fontString[MAX_FONT_LEN]; /* names of fonts for text widget */
    char boldFontString[MAX_FONT_LEN];
    char italicFontString[MAX_FONT_LEN];
    char boldItalicFontString[MAX_FONT_LEN];
    
    NFont *font;
    NFont *boldFont;
    NFont *italicFont;
    NFont *boldItalicFont;
    
    char *defaultColorProfile;
    
    char *iconSize;
    int closeIconSize;
    int isrcFindIconSize;
    int isrcClearIconSize;
    
    
    /*  The accumulated list of undo operations can potentially consume huge
        amounts of memory.  These tuning parameters determine how much undo infor-
        mation is retained.  Normally, the list is kept between UNDO_OP_LIMIT and
        UNDO_OP_TRIMTO in length (when the list reaches UNDO_OP_LIMIT, it is
        trimmed to UNDO_OP_TRIMTO then allowed to grow back to UNDO_OP_LIMIT).
        When there are very large amounts of saved text held in the list,
        UNDO_WORRY_LIMIT and UNDO_PURGE_LIMIT take over and cause the list to
        be trimmed back further to keep its size down. */
    int undoPurgeLimit;         /* If undo list gets this large (in bytes),
				   trim it to length of UNDO_PURGE_TRIMTO */
    int undoPurgeTrimTo;        /* Amount to trim the undo list in a purge */
    int undoWorryLimit;         /* If undo list gets this large (in bytes),
				   trim it to length of UNDO_WORRY_TRIMTO */
    int undoWorryTrimTo;        /* Amount to trim the undo list when memory
				   use begins to get serious */
    int undoOpLimit;            /* normal limit for length of undo list */
    int undoOpTrimTo;           /* size undo list is normally trimmed to
				   when it exceeds UNDO_OP_TRIMTO in length */
    
    int zoomStep;
    int zoomCtrlMouseWheel;     /* 0 = page with ctrl+mousewheel, change font size with shift+ctrl+mousewheel, 1 = actions swapped */
    int sortTabs;		/* sort tabs alphabetically */
    int repositionDialogs;	/* w. to reposition dialogs under the pointer */
    int autoScroll;             /* w. to autoscroll near top/bottom of screen */
    int autoScrollVPadding;     /* how close to get before autoscrolling */
    int sortOpenPrevMenu;   	/* whether to sort the "Open Previous" menu */
    int appendLF;       /* Whether to append LF at the end of each file */
    int mapDelete;		/* whether to map delete to backspace */
    int stdOpenDialog;		/* w. to retain redundant text field in Open */
    char tagFile[MAXPATHLEN];	/* name of tags file to look for at startup */
    int maxPrevOpenFiles;   	/* limit to size of Open Previous menu */
    int typingHidesPointer;     /* hide mouse pointer when typing */
    char delimiters[MAX_WORD_DELIMITERS]; /* punctuation characters */
    char shell[MAXPATHLEN + 1]; /* shell to use for executing commands */
    char geometry[MAX_GEOM_STRING_LEN];	/* per-application geometry string,
    	    	    	    	    	   only for the clueless */
    char serverName[MAXPATHLEN];/* server name for multiple servers per disp. */
    char bgMenuBtn[MAX_ACCEL_LEN]; /* X event description for triggering
    	    	    	    	      posting of background menu */
    char fileVersion[16]; 	/* Version of nedit which wrote the .nedit
    				   file we're reading */
    int findReplaceUsesSelection; /* whether the find replace dialog is automatically
                                     loaded with the primary selection */
    int virtKeyOverride;	/* Override Motif default virtual key bindings
				   never, if invalid, or always */
    char titleFormat[MAX_TITLE_FORMAT_LEN];
    char helpFontNames[NUM_HELP_FONTS][MAX_FONT_LEN];/* fonts for help system */
    char helpLinkColor[MAX_COLOR_LEN]; 	/* Color for hyperlinks in the help system */
    char colorNames[NUM_COLORS][MAX_COLOR_LEN];
    char tooltipBgColor[MAX_COLOR_LEN];
    int  undoModifiesSelection;
    int  focusOnRaise;
    int honorSymlinks;
    int truncSubstitution;
    int forceOSConversion;
    int autoEnableXattr;
    int windowDarkTheme;
    int fsbView;
    int fsbShowHidden;
    int fsbFileCmp;
    int editorConfig;
    int lockEncodingError;
    char defaultCharset[MAX_ENCODING_LENGTH];
    char fallbackCharset[MAX_ENCODING_LENGTH];
#ifdef EXCLUDE_FONTS
    char *fontExcludes;
#endif
} PrefData;

/* Temporary storage for preferences strings which are discarded after being
   read */
static struct {
    char *shellCmds;
    char *macroCmds;
    char *bgMenuCmds;
    char *highlight;
    char *language;
    char *styles;
    char *colorProfileStyles;
    char *smartIndent;
    char *smartIndentCommon;
    char *shell;
    char *colorProfiles;
    char *filter;
} TempStringPrefs;

/* preference descriptions for SavePreferences and RestorePreferences. */
static PrefDescripRec PrefDescrip[] = {
    {"fileVersion", "FileVersion" , PREF_STRING, "", PrefData.fileVersion,
      (void *)sizeof(PrefData.fileVersion), True},
#ifdef linux
    {"shellCommands", "ShellCommands", PREF_ALLOC_STRING, "spell:Alt+B:s:EX:\n\
    cat>spellTmp; xterm -e ispell -x spellTmp; cat spellTmp; rm spellTmp\n\
    wc::w:ED:\nwc | awk '{print $1 \" lines, \" $2 \" words, \" $3 \" characters\"}'\n\
    sort::o:EX:\nsort\nnumber lines::n:AW:\nnl -ba\nmake:Alt+Z:m:W:\nmake\n\
    expand::p:EX:\nexpand\nunexpand::u:EX:\nunexpand\n",
    &TempStringPrefs.shellCmds, NULL, True},
#elif __FreeBSD__
    {"shellCommands", "ShellCommands", PREF_ALLOC_STRING, "spell:Alt+B:s:EX:\n\
    cat>spellTmp; xterm -e ispell -x spellTmp; cat spellTmp; rm spellTmp\n\
    wc::w:ED:\nwc | awk '{print $2 \" lines, \" $1 \" words, \" $3 \" characters\"}'\n\
    sort::o:EX:\nsort\nnumber lines::n:AW:\npr -tn\nmake:Alt+Z:m:W:\nmake\n\
    expand::p:EX:\nexpand\nunexpand::u:EX:\nunexpand\n",
    &TempStringPrefs.shellCmds, NULL, True},
#else
    {"shellCommands", "ShellCommands", PREF_ALLOC_STRING, "spell:Alt+B:s:ED:\n\
    (cat;echo \"\") | spell\nwc::w:ED:\nwc | awk '{print $1 \" lines, \" $2 \" words, \" $3 \" characters\"}'\n\
    \nsort::o:EX:\nsort\nnumber lines::n:AW:\nnl -ba\nmake:Alt+Z:m:W:\nmake\n\
    expand::p:EX:\nexpand\nunexpand::u:EX:\nunexpand\n",
    &TempStringPrefs.shellCmds, NULL, True},
#endif /* linux, __FreeBSD__ */
    {"macroCommands", "MacroCommands", PREF_ALLOC_STRING,
	"Complete Word:Alt+D::: {\n\
		# This macro attempts to complete the current word by\n\
		# finding another word in the same document that has\n\
		# the same prefix; repeated invocations of the macro\n\
		# (by repeated typing of its accelerator, say) cycles\n\
		# through the alternatives found.\n\
		# \n\
		# Make sure $compWord contains something (a dummy index)\n\
		$compWord[\"\"] = \"\"\n\
		\n\
		# Test whether the rest of $compWord has been initialized:\n\
		# this avoids having to initialize the global variable\n\
		# $compWord in an external macro file\n\
		if (!(\"wordEnd\" in $compWord)) {\n\
		    # we need to initialize it\n\
		    $compWord[\"wordEnd\"] = 0\n\
		    $compWord[\"repeat\"] = 0\n\
		    $compWord[\"init\"] = 0\n\
		    $compWord[\"wordStart\"] = 0\n\
		}\n\
		\n\
		if ($compWord[\"wordEnd\"] == $cursor) {\n\
		        $compWord[\"repeat\"] += 1\n\
		}\n\
		else {\n\
		   $compWord[\"repeat\"] = 1\n\
		   $compWord[\"init\"] = $cursor\n\
		\n\
		   # search back to a word boundary to find the word to complete\n\
		   # (we use \\w here to allow for programming \"words\" that can include\n\
		   # digits and underscores; use \\l for letters only)\n\
		   $compWord[\"wordStart\"] = search(\"<\\\\w+\", $cursor, \"backward\", \"regex\", \"wrap\")\n\
		\n\
		   if ($compWord[\"wordStart\"] == -1)\n\
		      return\n\
		\n\
		    if ($search_end == $cursor)\n\
		       $compWord[\"word\"] = get_range($compWord[\"wordStart\"], $cursor)\n\
		    else\n\
		        return\n\
		}\n\
		s = $cursor\n\
		for (i=0; i <= $compWord[\"repeat\"]; i++)\n\
		    s = search($compWord[\"word\"], s - 1, \"backward\", \"regex\", \"wrap\")\n\
		\n\
		if (s == $compWord[\"wordStart\"]) {\n\
		   beep()\n\
		   $compWord[\"repeat\"] = 0\n\
		   s = $compWord[\"wordStart\"]\n\
		   se = $compWord[\"init\"]\n\
		}\n\
		else\n\
		   se = search(\">\", s, \"regex\")\n\
		\n\
		replace_range($compWord[\"wordStart\"], $cursor, get_range(s, se))\n\
		\n\
		$compWord[\"wordEnd\"] = $cursor\n\
	}\n\
	Fill Sel. w/Char:::R: {\n\
		# This macro replaces each character position in\n\
		# the selection with the string typed into the dialog\n\
		# it displays.\n\
		if ($selection_start == -1) {\n\
		    beep()\n\
		    return\n\
		}\n\
		\n\
		# Ask the user what character to fill with\n\
		fillChar = string_dialog(\"Fill selection with what character?\", \\\n\
		                         \"OK\", \"Cancel\")\n\
		if ($string_dialog_button == 2 || $string_dialog_button == 0)\n\
		    return\n\
		\n\
		# Count the number of lines (NL characters) in the selection\n\
		# (by removing all non-NLs in selection and counting the remainder)\n\
		nLines = length(replace_in_string(get_selection(), \\\n\
		                                  \"^.*$\", \"\", \"regex\"))\n\
		\n\
		rectangular = $selection_left != -1\n\
		\n\
		# work out the pieces of required of the replacement text\n\
		# this will be top mid bot where top is empty or ends in NL,\n\
		# mid is 0 or more lines of repeats ending with NL, and\n\
		# bot is 0 or more repeats of the fillChar\n\
		\n\
		toplen = -1 # top piece by default empty (no NL)\n\
		midlen = 0\n\
		botlen = 0\n\
		\n\
		if (rectangular) {\n\
		    # just fill the rectangle:  mid\\n \\ nLines\n\
		    #                           mid\\n /\n\
		    #                           bot   - last line with no nl\n\
		    midlen = $selection_right -  $selection_left\n\
		    botlen = $selection_right -  $selection_left\n\
		} else {\n\
		    #                  |col[0]\n\
		    #         .........toptoptop\\n                      |col[0]\n\
		    # either  midmidmidmidmidmid\\n \\ nLines - 1   or ...botbot...\n\
		    #         midmidmidmidmidmid\\n /                          |col[1]\n\
		    #         botbot...         |\n\
		    #                 |col[1]   |wrap margin\n\
		    # we need column positions col[0], col[1] of selection start and\n\
		    # end (use a loop and arrays to do the two positions)\n\
		    sel[0] = $selection_start\n\
		    sel[1] = $selection_end\n\
		\n\
		    # col[0] = pos_to_column($selection_start)\n\
		    # col[1] = pos_to_column($selection_end)\n\
		\n\
		    for (i = 0; i < 2; ++i) {\n\
		        end = sel[i]\n\
		        pos = search(\"^\", end, \"regex\", \"backward\")\n\
		        thisCol = 0\n\
		        while (pos < end) {\n\
		            nexttab = search(\"\\t\", pos)\n\
		            if (nexttab < 0 || nexttab >= end) {\n\
		                thisCol += end - pos # count remaining non-tabs\n\
		                nexttab = end\n\
		            } else {\n\
		                thisCol += nexttab - pos + $tab_dist\n\
		                thisCol -= (thisCol % $tab_dist)\n\
		            }\n\
		            pos = nexttab + 1 # skip past the tab or end\n\
		        }\n\
		        col[i] = thisCol\n\
		    }\n\
		    toplen = max($wrap_margin - col[0], 0)\n\
		    botlen = min(col[1], $wrap_margin)\n\
		\n\
		    if (nLines == 0) {\n\
		        toplen = -1\n\
		        botlen = max(botlen - col[0], 0)\n\
		    } else {\n\
		        midlen = $wrap_margin\n\
		        if (toplen < 0)\n\
		            toplen = 0\n\
		        nLines-- # top piece will end in a NL\n\
		    }\n\
		}\n\
		\n\
		# Create the fill text\n\
		# which is the longest piece? make a line of that length\n\
		# (use string doubling - this allows the piece to be\n\
		# appended to double in size at each iteration)\n\
		\n\
		len = max(toplen, midlen, botlen)\n\
		charlen = length(fillChar) # maybe more than one char given!\n\
		\n\
		line = \"\"\n\
		while (len > 0) {\n\
		    if (len % 2)\n\
		        line = line fillChar\n\
		    len /= 2\n\
		    if (len > 0)\n\
		        fillChar = fillChar fillChar\n\
		}\n\
		# assemble our pieces\n\
		toppiece = \"\"\n\
		midpiece = \"\"\n\
		botpiece = \"\"\n\
		if (toplen >= 0)\n\
		    toppiece = substring(line, 0, toplen * charlen) \"\\n\"\n\
		if (botlen > 0)\n\
		    botpiece = substring(line, 0, botlen * charlen)\n\
		\n\
		# assemble midpiece (use doubling again)\n\
		line = substring(line, 0, midlen * charlen) \"\\n\"\n\
		while (nLines > 0) {\n\
		    if (nLines % 2)\n\
		        midpiece = midpiece line\n\
		    nLines /= 2\n\
		    if (nLines > 0)\n\
		        line = line line\n\
		}\n\
		# Replace the selection with the complete fill text\n\
		replace_selection(toppiece midpiece botpiece)\n\
	}\n\
	Quote Mail Reply:::: {\n\
		if ($selection_start == -1)\n\
		    replace_all(\"^.*$\", \"\\\\> &\", \"regex\")\n\
		else\n\
		    replace_in_selection(\"^.*$\", \"\\\\> &\", \"regex\")\n\
	}\n\
	Unquote Mail Reply:::: {\n\
		if ($selection_start == -1)\n\
		    replace_all(\"(^\\\\> )(.*)$\", \"\\\\2\", \"regex\")\n\
		else\n\
		    replace_in_selection(\"(^\\\\> )(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>/* Comment */@C@C++@Java@CSS@JavaScript@Lex:::R: {\n\
		selStart = $selection_start\n\
		selEnd = $selection_end\n\
		replace_range(selStart, selEnd, \"/* \" get_selection() \" */\")\n\
		select(selStart, selEnd + 6)\n\
	}\n\
	Comments>/* Uncomment */@C@C++@Java@CSS@JavaScript@Lex:::R: {\n\
		pos = search(\"(?n\\\\s*/\\\\*\\\\s*)\", $selection_start, \"regex\")\n\
		start = $search_end\n\
		end = search(\"(?n\\\\*/\\\\s*)\", $selection_end, \"regex\", \"backward\")\n\
		if (pos != $selection_start || end == -1 )\n\
		    return\n\
		replace_selection(get_range(start, end))\n\
		select(pos, $cursor)\n\
	}\n\
	Comments>// Comment@C@C++@Java@JavaScript:::R: {\n\
		replace_in_selection(\"^.*$\", \"// &\", \"regex\")\n\
	}\n\
	Comments>// Uncomment@C@C++@Java@JavaScript:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*// ?)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments># Comment@Perl@Sh Ksh Bash@NEdit Macro@Makefile@Awk@Csh@Python@Tcl:::R: {\n\
		replace_in_selection(\"^.*$\", \"#&\", \"regex\")\n\
	}\n\
	Comments># Uncomment@Perl@Sh Ksh Bash@NEdit Macro@Makefile@Awk@Csh@Python@Tcl:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*#)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>-- Comment@SQL:::R: {\n\
		replace_in_selection(\"^.*$\", \"--&\", \"regex\")\n\
	}\n\
	Comments>-- Uncomment@SQL:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*--)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>! Comment@X Resources:::R: {\n\
		replace_in_selection(\"^.*$\", \"!&\", \"regex\")\n\
	}\n\
	Comments>! Uncomment@X Resources:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*!)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>% Comment@LaTeX:::R: {\n\
		replace_in_selection(\"^.*$\", \"%&\", \"regex\")\n\
		}\n\
	Comments>% Uncomment@LaTeX:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*%)(.*)$\", \"\\\\2\", \"regex\")\n\
		}\n\
	Comments>Bar Comment@C:::R: {\n\
		if ($selection_left != -1) {\n\
		    dialog(\"Selection must not be rectangular\")\n\
		    return\n\
		}\n\
		start = $selection_start\n\
		end = $selection_end-1\n\
		origText = get_range($selection_start, $selection_end-1)\n\
		newText = \"/*\\n\" replace_in_string(get_range(start, end), \\\n\
		    \"^\", \" * \", \"regex\") \"\\n */\\n\"\n\
		replace_selection(newText)\n\
		select(start, start + length(newText))\n\
	}\n\
	Comments>Bar Uncomment@C:::R: {\n\
		selStart = $selection_start\n\
		selEnd = $selection_end\n\
		pos = search(\"/\\\\*\\\\s*\\\\n\", selStart, \"regex\")\n\
		if (pos != selStart) return\n\
		start = $search_end\n\
		end = search(\"\\\\n\\\\s*\\\\*/\\\\s*\\\\n?\", selEnd, \"regex\", \"backward\")\n\
		if (end == -1 || $search_end < selEnd) return\n\
		newText = get_range(start, end)\n\
		newText = replace_in_string(newText,\"^ *\\\\* ?\", \"\", \"regex\", \"copy\")\n\
		if (get_range(selEnd, selEnd - 1) == \"\\n\") selEnd -= 1\n\
		replace_range(selStart, selEnd, newText)\n\
		select(selStart, selStart + length(newText))\n\
	}\n\
	Make C Prototypes@C@C++:::: {\n\
		# simplistic extraction of C function prototypes, usually good enough\n\
		if ($selection_start == -1) {\n\
		    start = 0\n\
		    end = $text_length\n\
		} else {\n\
		    start = $selection_start\n\
		    end = $selection_end\n\
		}\n\
		string = get_range(start, end)\n\
		# remove all C++ and C comments, then all blank lines in the extracted range\n\
		string = replace_in_string(string, \"//.*$\", \"\", \"regex\", \"copy\")\n\
		string = replace_in_string(string, \"(?n/\\\\*.*?\\\\*/)\", \"\", \"regex\", \"copy\")\n\
		string = replace_in_string(string, \"^\\\\s*\\n\", \"\", \"regex\", \"copy\")\n\
		nDefs = 0\n\
		searchPos = 0\n\
		prototypes = \"\"\n\
		staticPrototypes = \"\"\n\
		for (;;) {\n\
		    headerStart = search_string(string, \\\n\
		        \"^[a-zA-Z]([^;#\\\"'{}=><!/]|\\n)*\\\\)[ \\t]*\\n?[ \\t]*\\\\{\", \\\n\
		        searchPos, \"regex\")\n\
		    if (headerStart == -1)\n\
		        break\n\
		    headerEnd = search_string(string, \")\", $search_end,\"backward\") + 1\n\
		    prototype = substring(string, headerStart, headerEnd) \";\\n\"\n\
		    if (substring(string, headerStart, headerStart+6) == \"static\")\n\
		        staticPrototypes = staticPrototypes prototype\n\
		    else\n\
		        prototypes = prototypes prototype\n\
		    searchPos = headerEnd\n\
		    nDefs++\n\
		}\n\
		if (nDefs == 0) {\n\
		    dialog(\"No function declarations found\")\n\
		    return\n\
		}\n\
		new()\n\
		focus_window(\"last\")\n\
		replace_range(0, 0, prototypes staticPrototypes)\n\
	}", &TempStringPrefs.macroCmds, NULL, True},
    {"bgMenuCommands", "BGMenuCommands", PREF_ALLOC_STRING,
       "Undo:::: {\nundo()\n}\n\
	Redo:::: {\nredo()\n}\n\
	Cut:::R: {\ncut_clipboard()\n}\n\
	Copy:::R: {\ncopy_clipboard()\n}\n\
	Paste:::: {\npaste_clipboard()\n}", &TempStringPrefs.bgMenuCmds,
	NULL, True},
    {"highlightPatterns", "HighlightPatterns", PREF_ALLOC_STRING,
       "Ada:Default\n\
        Awk:Default\n\
        C++:Default\n\
        C:Default\n\
        CSS:Default\n\
        Csh:Default\n\
        Dockerfile:Default\n\
        Fortran:Default\n\
        Java:Default\n\
        JavaScript:Default\n\
        LaTeX:Default\n\
        Lex:Default\n\
	Lua:Default\n\
        Makefile:Default\n\
        Markdown:Default\n\
        Matlab:Default\n\
        NEdit Macro:Default\n\
        OCaml:Default\n\
        Pascal:Default\n\
        Perl:Default\n\
        PostScript:Default\n\
        Python:Default\n\
        Regex:Default\n\
        SGML HTML:Default\n\
        SQL:Default\n\
        Sh Ksh Bash:Default\n\
        Tcl:Default\n\
        VHDL:Default\n\
        Verilog:Default\n\
        XML:Default\n\
        X Resources:Default\n\
        Yacc:Default",
        &TempStringPrefs.highlight, NULL, True},
    {"languageModes", "LanguageModes", PREF_ALLOC_STRING,
       "Ada:.ada .ad .ads .adb .a:::::::\n\
        Awk:.awk:::::::\n\
        C++:.cc .hh .C .H .i .cxx .hxx .cpp .c++::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":\n\
        C:.c .h::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":\n\
        CSS:css::Auto:None:::\".,/\\`'!|@#%^&*()=+{}[]\"\":;<>?~\":\n\
        Csh:.csh .cshrc .tcshrc .login .logout:\"^[ \\t]*#[ \\t]*![ \\t]*/bin/t?csh\"::::::\n\
        Dockerfile::::::::\n\
        Fortran:.f .f77 .for:::::::\n\
        Java:.java:::::::\n\
        JavaScript:.js:::::::\n\
        LaTeX:.tex .sty .cls .ltx .ins .clo .fd:::::::\n\
        Lex:.lex:::::::\n\
	Lua:.lua:::::::\n\
        Makefile:Makefile makefile .gmk:::None:8:8::\n\
        Markdown:.md .markdown .mdtxt .mdtext:::::::\n\
        Matlab:.m .oct .sci:::::::\n\
        NEdit Macro:.nm .neditmacro:::::::\n\
        OCaml:.ml::Auto:None:8:2:\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":\n\
        Pascal:.pas .p .int:::::::\n\
        Perl:.pl .pm .p5 .PL:\"^[ \\t]*#[ \\t]*!.*perl\":Auto:None:::\".,/\\\\`'!$@#%^&*()-=+{}[]\"\":;<>?~|\":\n\
        PostScript:.ps .eps .epsf .epsi:\"^%!\":::::\"/%(){}[]<>\":\n\
        Python:.py:\"^#!.*python\":Auto:None:::\"!\"\"#$%&'()*+,-./:;<=>?@[\\\\]^`{|}~\":\n\
        Regex:.reg .regex:\"\\(\\?[:#=!iInN].+\\)\":None:Continuous::::\n\
        SGML HTML:.sgml .sgm .html .htm:\"\\<[Hh][Tt][Mm][Ll]\\>\"::::::\n\
        SQL:.sql:::::::\n\
        Sh Ksh Bash:.sh .bash .ksh .profile .bashrc .bash_logout .bash_login .bash_profile:\"^[ \\t]*#[ \\t]*![ \\t]*/.*bin/(bash|ksh|sh|zsh)\"::::::\n\
        Tcl:.tcl .tk .itcl .itk::Smart:None::::\n\
        VHDL:.vhd .vhdl .vdl:::::::\n\
        Verilog:.v:::::::\n\
        XML:.xml .xsl .dtd:\"\\<(?i\\?xml|!doctype)\"::None:::\"<>/=\"\"'()+*?|\":\n\
        X Resources:.Xresources .Xdefaults .nedit .pats nedit.rc:\"^[!#].*([Aa]pp|[Xx]).*[Dd]efaults\"::::::\n\
        Yacc:.y::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":",
	&TempStringPrefs.language, NULL, True},
    {"styles", "Styles", PREF_ALLOC_STRING, "Plain:black:Plain\n\
    	Comment:gray20:Italic\n\
    	Keyword:black:Bold\n\
        Operator:dark blue:Bold\n\
        Bracket:dark blue:Bold\n\
    	Storage Type:brown:Bold\n\
    	Storage Type1:saddle brown:Bold\n\
    	String:darkGreen:Plain\n\
    	String1:SeaGreen:Plain\n\
    	String2:darkGreen:Bold\n\
    	Preprocessor:RoyalBlue4:Plain\n\
    	Preprocessor1:blue:Plain\n\
        Preprocessor2:rebecca purple:Bold\n\
    	Character Const:darkGreen:Plain\n\
    	Numeric Const:darkGreen:Plain\n\
    	Identifier:brown:Plain\n\
    	Identifier1:RoyalBlue4:Plain\n\
        Identifier2:SteelBlue:Plain\n\
 	Subroutine:brown:Plain\n\
	Subroutine1:chocolate:Plain\n\
   	Ada Attributes:plum:Bold\n\
	Label:red:Italic\n\
	Flag:red:Bold\n\
    	Text Comment:SteelBlue4:Italic\n\
    	Text Key:VioletRed4:Bold\n\
	Text Key1:VioletRed4:Plain\n\
    	Text Arg:RoyalBlue4:Bold\n\
    	Text Arg1:SteelBlue4:Bold\n\
	Text Arg2:RoyalBlue4:Plain\n\
    	Text Escape:gray30:Bold\n\
	LaTeX Math:darkGreen:Plain\n"
        ADD_5_2_STYLES
        ADD_6_1_STYLES,
	&TempStringPrefs.styles, NULL, True},
#ifndef DISABLE_COLORPROFILES
    {"colorProfileStyles", "colorProfileStyles", PREF_ALLOC_STRING, "",
        &TempStringPrefs.colorProfileStyles, NULL, False},
#endif
    {"smartIndentInit", "SmartIndentInit", PREF_ALLOC_STRING,
        "C:Default\n\
	C++:Default\n\
	Python:Default\n\
	Matlab:Default", &TempStringPrefs.smartIndent, NULL, True},
    {"smartIndentInitCommon", "SmartIndentInitCommon", PREF_ALLOC_STRING,
        "Default", &TempStringPrefs.smartIndentCommon, NULL, True},
    {"autoWrap", "AutoWrap", PREF_ENUM, "Continuous",
    	&PrefData.wrapStyle, AutoWrapTypes, True},
    {"wrapMargin", "WrapMargin", PREF_INT, "0",
    	&PrefData.wrapMargin, NULL, True},
    {"autoIndent", "AutoIndent", PREF_ENUM, "Auto",
    	&PrefData.autoIndent, AutoIndentTypes, True},
    {"autoSave", "AutoSave", PREF_BOOLEAN, "True",
    	&PrefData.autoSave, NULL, True},
    {"openInTab", "OpenInTab", PREF_BOOLEAN, "True",
    	&PrefData.openInTab, NULL, True},
    {"saveOldVersion", "SaveOldVersion", PREF_BOOLEAN, "False",
    	&PrefData.saveOldVersion, NULL, True},
    {"showMatching", "ShowMatching", PREF_ENUM, "Delimiter",
 	&PrefData.showMatchingStyle, ShowMatchingTypes, True},
    {"matchSyntaxBased", "MatchSyntaxBased", PREF_BOOLEAN, "True",
 	&PrefData.matchSyntaxBased, NULL, True},
    {"highlightSyntax", "HighlightSyntax", PREF_BOOLEAN, "True",
    	&PrefData.highlightSyntax, NULL, True},
    {"highlightCursorLine", "HighlightCursorLine", PREF_BOOLEAN, "False",
      &PrefData.highlightCursorLine, NULL, True},
    {"indentRainbow", "IndentRainbow", PREF_BOOLEAN, "False",
      &PrefData.indentRainbow, NULL, True},
    {"indentRainbowColors", "IndentRainbowColors", PREF_ALLOC_STRING,
      "#f0f8ff;#f0fff6;#f8fff0;#fff6f0;#fef0ff;#f0f1ff",
      &PrefData.indentRainbowColors, NULL, True},
    {"ansiColorList", "AnsiColorList", PREF_ALLOC_STRING,
      "#000000;#800000;#008000;#808000;#000080;#800080;#008080;#c0c0c0;#808080;#ff0000;#00ff00;#ffff00;#0000ff;#ff00ff;#00ffff;#ffffff",
      &PrefData.ansiColorList, NULL, True},
    {"ansiColors", "AnsiColors", PREF_BOOLEAN, "False",
      &PrefData.ansiColors, NULL, True},
#ifndef DISABLE_COLORPROFILES
    {"colorProfiles", "ColorProfiles", PREF_ALLOC_STRING, 
            "light:colors:{black;#f5f5f5;black;rgb:cc/cc/cc;white;red;black;#eff0f1;black;rgb:ee/ee/ee},ansi:{#000000;#800000;#008000;#808000;#000080;#800080;#008080;#c0c0c0;#808080;#ff0000;#00ff00;#ffff00;#0000ff;#ff00ff;#00ffff;#ffffff},rainbow:{#f0f8ff;#f0fff6;#f8fff0;#fff6f0;#fef0ff;#f0f1ff},styles:{0},wm:{0},res:{light}\n\
            dark:colors:{white;#161616;white;rgb:cc/cc/cc;black;red;#a8a8a8;#313131;white;#4f4f4f},ansi:{#000000;#800000;#008000;#808000;#000080;#800080;#008080;#c0c0c0;#808080;#ff0000;#00ff00;#ffff00;#0000ff;#ff00ff;#00ffff;#ffffff},rainbow:{#06182a;#02210e;#192709;#2d1505;#210423;#070a2d},styles:{1},wm:{0},res:{dark}",
        &TempStringPrefs.colorProfiles, NULL, True},
    {"defaultColorProfile", "DefaultColorProfile", PREF_ALLOC_STRING, "default",
        &PrefData.defaultColorProfile, NULL, True},
#endif
    {"backlightChars", "BacklightChars", PREF_BOOLEAN, "False",
      &PrefData.backlightChars, NULL, True},
    {"backlightCharTypes", "BacklightCharTypes", PREF_ALLOC_STRING,
      "0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange",
    /*                     gray87                 gray94                 */
      &PrefData.backlightCharTypes, NULL, False},
    {"saveSearchHistory", "SaveSearchHistory", PREF_BOOLEAN, "False",
      &PrefData.saveSearchHistory, NULL, True},
    {"searchDialogs", "SearchDialogs", PREF_BOOLEAN, "False",
    	&PrefData.searchDlogs, NULL, True},
    {"beepOnSearchWrap", "BeepOnSearchWrap", PREF_BOOLEAN, "False",
      &PrefData.searchWrapBeep, NULL, True},
    {"retainSearchDialogs", "RetainSearchDialogs", PREF_BOOLEAN, "False",
    	&PrefData.keepSearchDlogs, NULL, True},
    {"searchWraps", "SearchWraps", PREF_BOOLEAN, "True",
    	&PrefData.searchWraps, NULL, True},
    {"stickyCaseSenseButton", "StickyCaseSenseButton", PREF_BOOLEAN, "True",
    	&PrefData.stickyCaseSenseBtn, NULL, True},
#if XmVersion < 1002 /* Flashing is annoying in 1.1 versions */
    {"repositionDialogs", "RepositionDialogs", PREF_BOOLEAN, "False",
    	&PrefData.repositionDialogs, NULL, True},
#else
    {"repositionDialogs", "RepositionDialogs", PREF_BOOLEAN, "True",
    	&PrefData.repositionDialogs, NULL, True},
#endif
    {"autoScroll", "AutoScroll", PREF_BOOLEAN, "False",
    	&PrefData.autoScroll, NULL, True},
    {"autoScrollVPadding", "AutoScrollVPadding", PREF_INT, "4",
    	&PrefData.autoScrollVPadding, NULL, False},
    {"appendLF", "AppendLF", PREF_BOOLEAN, "True",
        &PrefData.appendLF, NULL, True},
    {"sortOpenPrevMenu", "SortOpenPrevMenu", PREF_BOOLEAN, "True",
    	&PrefData.sortOpenPrevMenu, NULL, True},
    {"statisticsLine", "StatisticsLine", PREF_BOOLEAN, "False",
    	&PrefData.statsLine, NULL, True},
    {"iSearchLine", "ISearchLine", PREF_BOOLEAN, "False",
    	&PrefData.iSearchLine, NULL, True},
    {"zoomStep", "ZoomStep", PREF_INT, "1",
    	&PrefData.zoomStep, NULL, True},
    {"zoomCtrlMouseWheel", "ZoomCtrlMouseWheel", PREF_INT, "0",
    	&PrefData.zoomCtrlMouseWheel, NULL, True},
    {"undoPurgeLimit", "UndoPurgeLimit", PREF_INT, "50000000",
    	&PrefData.undoPurgeLimit, NULL, True},
    {"undoPurgeTrimTo", "UndoPurgeTrimTo", PREF_INT, "1",
    	&PrefData.undoPurgeTrimTo, NULL, True},
    {"undoWorryLimit", "UndoWorryLimit", PREF_INT, "8000000",
    	&PrefData.undoWorryLimit, NULL, True},
    {"undoWorryTrimTo", "UndoWorryTrimTo", PREF_INT, "5",
    	&PrefData.undoWorryTrimTo, NULL, True},
    {"undoOpLimit", "UndoOpLimit", PREF_INT, "600",
    	&PrefData.undoOpLimit, NULL, True},
    {"undoOpTrimTo", "UndoOpTrimTo", PREF_INT, "200",
    	&PrefData.undoOpTrimTo, NULL, True},
    {"sortTabs", "SortTabs", PREF_BOOLEAN, "False",
    	&PrefData.sortTabs, NULL, True},
    {"tabBar", "TabBar", PREF_BOOLEAN, "True",
    	&PrefData.tabBar, NULL, True},
    {"tabBarHideOne", "TabBarHideOne", PREF_BOOLEAN, "True",
    	&PrefData.tabBarHideOne, NULL, True},
    {"toolTips", "ToolTips", PREF_BOOLEAN, "True",
    	&PrefData.toolTips, NULL, True},
    {"globalTabNavigate", "GlobalTabNavigate", PREF_BOOLEAN, "False",
    	&PrefData.globalTabNavigate, NULL, True},
    {"lineNumbers", "LineNumbers", PREF_BOOLEAN, "False",
    	&PrefData.lineNums, NULL, True},
    {"pathInWindowsMenu", "PathInWindowsMenu", PREF_BOOLEAN, "True",
    	&PrefData.pathInWindowsMenu, NULL, True},
    {"warnFileMods", "WarnFileMods", PREF_BOOLEAN, "True",
    	&PrefData.warnFileMods, NULL, True},
    {"warnRealFileMods", "WarnRealFileMods", PREF_BOOLEAN, "True",
    	&PrefData.warnRealFileMods, NULL, True},
    {"warnExit", "WarnExit", PREF_BOOLEAN, "True",
    	&PrefData.warnExit, NULL, True},
    {"searchMethod", "SearchMethod", PREF_ENUM, "Literal",
    	&PrefData.searchMethod, SearchMethodStrings, True},
#ifdef REPLACE_SCOPE
    {"replaceDefaultScope", "ReplaceDefaultScope", PREF_ENUM, "Smart",
    	&PrefData.replaceDefScope, ReplaceDefScopeStrings, True},
#endif
    {"textRows", "TextRows", PREF_INT, "24",
    	&PrefData.textRows, NULL, True},
    {"textCols", "TextCols", PREF_INT, "80",
    	&PrefData.textCols, NULL, True},
    {"tabDistance", "TabDistance", PREF_INT, "8",
    	&PrefData.tabDist, NULL, True},
    {"emulateTabs", "EmulateTabs", PREF_INT, "0",
    	&PrefData.emTabDist, NULL, True},
    {"insertTabs", "InsertTabs", PREF_BOOLEAN, "True",
    	&PrefData.insertTabs, NULL, True},
    {"textFont", "TextFont", PREF_STRING,
    	"Monospace:size=11",
    	PrefData.fontString, (void *)sizeof(PrefData.fontString), True},
    {"boldHighlightFont", "BoldHighlightFont", PREF_STRING,
    	"Monospace:size=11:weight=bold",
    	PrefData.boldFontString, (void *)sizeof(PrefData.boldFontString), True},
    {"italicHighlightFont", "ItalicHighlightFont", PREF_STRING,
    	"Monospace:size=11:slant=italic",
    	PrefData.italicFontString,
    	(void *)sizeof(PrefData.italicFontString), True},
    {"boldItalicHighlightFont", "BoldItalicHighlightFont", PREF_STRING,
    	"Monospace:size=11:weight=bold:slant=italic",
    	PrefData.boldItalicFontString,
    	(void *)sizeof(PrefData.boldItalicFontString), True},
    {"helpFont", "HelpFont", PREF_STRING,
    	"Sans",
	PrefData.helpFontNames[HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[HELP_FONT]), False},
    {"boldHelpFont", "BoldHelpFont", PREF_STRING,
    	"Sans",
	PrefData.helpFontNames[BOLD_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[BOLD_HELP_FONT]), False},
    {"italicHelpFont", "ItalicHelpFont", PREF_STRING,
    	"Sans",
	PrefData.helpFontNames[ITALIC_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[ITALIC_HELP_FONT]), False},
    {"boldItalicHelpFont", "BoldItalicHelpFont", PREF_STRING,
    	"Sans",
	PrefData.helpFontNames[BOLD_ITALIC_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[BOLD_ITALIC_HELP_FONT]), False},
    {"fixedHelpFont", "FixedHelpFont", PREF_STRING,
    	"Monospace",
	PrefData.helpFontNames[FIXED_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[FIXED_HELP_FONT]), False},
    {"boldFixedHelpFont", "BoldFixedHelpFont", PREF_STRING,
    	"Monospace",
	PrefData.helpFontNames[BOLD_FIXED_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[BOLD_FIXED_HELP_FONT]), False},
    {"italicFixedHelpFont", "ItalicFixedHelpFont", PREF_STRING,
    	"Monospace",
	PrefData.helpFontNames[ITALIC_FIXED_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[ITALIC_FIXED_HELP_FONT]), False},
    {"boldItalicFixedHelpFont", "BoldItalicFixedHelpFont", PREF_STRING,
    	"Monospace",
	PrefData.helpFontNames[BOLD_ITALIC_FIXED_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[BOLD_ITALIC_FIXED_HELP_FONT]), False},
    {"helpLinkFont", "HelpLinkFont", PREF_STRING,
    	"Sans",
	PrefData.helpFontNames[HELP_LINK_FONT],
	(void *)sizeof(PrefData.helpFontNames[HELP_LINK_FONT]), False},
    {"h1HelpFont", "H1HelpFont", PREF_STRING,
    	"Sans",
	PrefData.helpFontNames[H1_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[H1_HELP_FONT]), False},
    {"h2HelpFont", "H2HelpFont", PREF_STRING,
    	"Sans",
	PrefData.helpFontNames[H2_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[H2_HELP_FONT]), False},
    {"h3HelpFont", "H3HelpFont", PREF_STRING,
    	"Monospace",
	PrefData.helpFontNames[H3_HELP_FONT],
	(void *)sizeof(PrefData.helpFontNames[H3_HELP_FONT]), False},
    {"helpLinkColor", "HelpLinkColor", PREF_STRING, "#009900",
	PrefData.helpLinkColor,
	(void *)sizeof(PrefData.helpLinkColor), False},
        
    {"textFgColor", "TextFgColor", PREF_STRING, NEDIT_DEFAULT_FG,
        PrefData.colorNames[TEXT_FG_COLOR],
        (void *)sizeof(PrefData.colorNames[TEXT_FG_COLOR]), True},
    {"textBgColor", "TextBgColor", PREF_STRING, NEDIT_DEFAULT_TEXT_BG,
        PrefData.colorNames[TEXT_BG_COLOR],
        (void *)sizeof(PrefData.colorNames[TEXT_BG_COLOR]), True},
    {"selectFgColor", "SelectFgColor", PREF_STRING, NEDIT_DEFAULT_SEL_FG,
        PrefData.colorNames[SELECT_FG_COLOR],
        (void *)sizeof(PrefData.colorNames[SELECT_FG_COLOR]), True},
    {"selectBgColor", "SelectBgColor", PREF_STRING, NEDIT_DEFAULT_SEL_BG,
        PrefData.colorNames[SELECT_BG_COLOR],
        (void *)sizeof(PrefData.colorNames[SELECT_BG_COLOR]), True},
    {"hiliteFgColor", "HiliteFgColor", PREF_STRING, NEDIT_DEFAULT_HI_FG,
        PrefData.colorNames[HILITE_FG_COLOR],
        (void *)sizeof(PrefData.colorNames[HILITE_FG_COLOR]), True},
    {"hiliteBgColor", "HiliteBgColor", PREF_STRING, NEDIT_DEFAULT_HI_BG,
        PrefData.colorNames[HILITE_BG_COLOR],
        (void *)sizeof(PrefData.colorNames[HILITE_BG_COLOR]), True},
    {"lineNoFgColor", "LineNoFgColor", PREF_STRING, NEDIT_DEFAULT_LINENO_FG,
        PrefData.colorNames[LINENO_FG_COLOR],
        (void *)sizeof(PrefData.colorNames[LINENO_FG_COLOR]), True},
    {"lineNoBgColor", "LineNoBgColor", PREF_STRING, NEDIT_DEFAULT_LINENO_BG,
        PrefData.colorNames[LINENO_BG_COLOR],
        (void *)sizeof(PrefData.colorNames[LINENO_BG_COLOR]), True},
    {"cursorFgColor", "CursorFgColor", PREF_STRING, NEDIT_DEFAULT_CURSOR_FG,
        PrefData.colorNames[CURSOR_FG_COLOR],
        (void *)sizeof(PrefData.colorNames[CURSOR_FG_COLOR]), True},
    {"cursorLineBgColor", "CursorLineBgColor", PREF_STRING, NEDIT_DEFAULT_CURSOR_LINE_BG,
        PrefData.colorNames[CURSOR_LINE_BG_COLOR],
        (void *)sizeof(PrefData.colorNames[CURSOR_LINE_BG_COLOR]), True},
    {"tooltipBgColor", "TooltipBgColor", PREF_STRING, "LemonChiffon1",
        PrefData.tooltipBgColor,
        (void *)sizeof(PrefData.tooltipBgColor), False},
    {"shell", "Shell", PREF_STRING, "DEFAULT", PrefData.shell,
        (void*) sizeof(PrefData.shell), True},
    {"geometry", "Geometry", PREF_STRING, "",
    	PrefData.geometry, (void *)sizeof(PrefData.geometry), False},
    {"remapDeleteKey", "RemapDeleteKey", PREF_BOOLEAN, "False",
    	&PrefData.mapDelete, NULL, False},
    {"stdOpenDialog", "StdOpenDialog", PREF_BOOLEAN, "False",
    	&PrefData.stdOpenDialog, NULL, False},
    {"tagFile", "TagFile", PREF_STRING,
    	"", PrefData.tagFile, (void *)sizeof(PrefData.tagFile), False},
    {"wordDelimiters", "WordDelimiters", PREF_STRING,
    	".,/\\`'!|@#%^&*()-=+{}[]\":;<>?",
    	PrefData.delimiters, (void *)sizeof(PrefData.delimiters), False},
    {"serverName", "ServerName", PREF_STRING, "", PrefData.serverName,
      (void *)sizeof(PrefData.serverName), False},
    {"maxPrevOpenFiles", "MaxPrevOpenFiles", PREF_INT, "30",
    	&PrefData.maxPrevOpenFiles, NULL, False},
    {"bgMenuButton", "BGMenuButton" , PREF_STRING,
	"~Shift~Ctrl~Meta~Alt<Btn3Down>", PrefData.bgMenuBtn,
      (void *)sizeof(PrefData.bgMenuBtn), False},
    {"smartTags", "SmartTags", PREF_BOOLEAN, "True",
    	&PrefData.smartTags, NULL, True},
    {"typingHidesPointer", "TypingHidesPointer", PREF_BOOLEAN, "False",
        &PrefData.typingHidesPointer, NULL, False},
    {"alwaysCheckRelativeTagsSpecs", "AlwaysCheckRelativeTagsSpecs", 
      	PREF_BOOLEAN, "True", &PrefData.alwaysCheckRelativeTagsSpecs, NULL, False},
    {"prefFileRead", "PrefFileRead", PREF_BOOLEAN, "False",
    	&PrefData.prefFileRead, NULL, True},
#ifdef SGI_CUSTOM
    {"shortMenus", "ShortMenus", PREF_BOOLEAN, "False", &PrefData.shortMenus,
      NULL, True},
#endif
    {"findReplaceUsesSelection", "FindReplaceUsesSelection", PREF_BOOLEAN, "False",
    	&PrefData.findReplaceUsesSelection, NULL, False},
    {"overrideDefaultVirtualKeyBindings", "OverrideDefaultVirtualKeyBindings", 
      PREF_ENUM, "Auto", &PrefData.virtKeyOverride, VirtKeyOverrideModes, False},
    {"titleFormat", "TitleFormat", PREF_STRING, "{%c} [%s] %f (%S) - %d",
	PrefData.titleFormat, (void *)sizeof(PrefData.titleFormat), True},
    {"undoModifiesSelection", "UndoModifiesSelection", PREF_BOOLEAN,
        "True", &PrefData.undoModifiesSelection, NULL, False},
    {"focusOnRaise", "FocusOnRaise", PREF_BOOLEAN,
            "False", &PrefData.focusOnRaise, NULL, False},
    {"forceOSConversion", "ForceOSConversion", PREF_BOOLEAN, "True",
            &PrefData.forceOSConversion, NULL, False},
    {"truncSubstitution", "TruncSubstitution", PREF_ENUM, "Fail",
            &PrefData.truncSubstitution, TruncSubstitutionModes, False},
    {"honorSymlinks", "HonorSymlinks", PREF_BOOLEAN, "True",
            &PrefData.honorSymlinks, NULL, False},
    {"autoEnableXattr", "AutoEnableXattr", PREF_BOOLEAN, "True",
            &PrefData.autoEnableXattr, NULL, False},
    {"windowDarkTheme", "WindowDarkTheme", PREF_BOOLEAN, "False",
            &PrefData.windowDarkTheme, NULL, False},
    {"fsbView", "FsbView", PREF_INT, "1",
            &PrefData.fsbView, NULL, True},
    {"fsbShowHidden", "FsbShowHidden", PREF_BOOLEAN, "False",
            &PrefData.fsbShowHidden, NULL, True},
    {"fsbFileCmp", "FsbFileCmp", PREF_INT, "0",
            &PrefData.fsbFileCmp, NULL, True},
    {"editorConfig", "EditorConfig", PREF_BOOLEAN, "True",
            &PrefData.editorConfig, NULL, True},
    {"defaultCharset", "DefaultCharset", PREF_STRING, "locale",
        PrefData.defaultCharset,
        (void *)sizeof(PrefData.defaultCharset), True},
    {"fallbackCharset", "FallbackCharset", PREF_STRING, "locale",
        PrefData.fallbackCharset,
        (void *)sizeof(PrefData.fallbackCharset), True},
    {"iconSize", "IconSize", PREF_ALLOC_STRING,
      "small",
      &PrefData.iconSize, NULL, True} ,
    {"lockEncodingError", "LockEncodingError", PREF_BOOLEAN, "True",
            &PrefData.lockEncodingError, NULL, True},
    {"filter", "Filter", PREF_ALLOC_STRING,
      "gzip;*.gz;.gz;gzip -d;gzip\n"
      "bzip2;*.bz;.bz;bzip2 -d;bzip2",
      &TempStringPrefs.filter, NULL, True} ,
#ifdef EXCLUDE_FONTS
    {"excludeFonts", "ExcludeFonts", PREF_ALLOC_STRING,
      "",
      &PrefData.fontExcludes, NULL, True} ,
#endif
};

static XrmOptionDescRec OpTable[] = {
    {"-wrap", ".autoWrap", XrmoptionNoArg, (caddr_t)"Continuous"},
    {"-nowrap", ".autoWrap", XrmoptionNoArg, (caddr_t)"None"},
    {"-autowrap", ".autoWrap", XrmoptionNoArg, (caddr_t)"Newline"},
    {"-noautowrap", ".autoWrap", XrmoptionNoArg, (caddr_t)"None"},
    {"-autoindent", ".autoIndent", XrmoptionNoArg, (caddr_t)"Auto"},
    {"-noautoindent", ".autoIndent", XrmoptionNoArg, (caddr_t)"False"},
    {"-autosave", ".autoSave", XrmoptionNoArg, (caddr_t)"True"},
    {"-noautosave", ".autoSave", XrmoptionNoArg, (caddr_t)"False"},
    {"-rows", ".textRows", XrmoptionSepArg, (caddr_t)NULL},
    {"-columns", ".textCols", XrmoptionSepArg, (caddr_t)NULL},
    {"-tabs", ".tabDistance", XrmoptionSepArg, (caddr_t)NULL},
    {"-font", ".textFont", XrmoptionSepArg, (caddr_t)NULL},
    {"-fn", ".textFont", XrmoptionSepArg, (caddr_t)NULL},
    {"-svrname", ".serverName", XrmoptionSepArg, (caddr_t)NULL},
};

static const char HeaderText[] = "\
! Preferences file for NEdit\n\
! (User settings in X \"application defaults\" format)\n\
!\n\
! This file is overwritten by the \"Save Defaults...\" command in NEdit\n\
! and serves only the interactively settable options presented in the NEdit\n\
! \"Preferences\" menu.  To modify other options, such as key bindings, use\n\
! the .Xdefaults file in your home directory (or the X resource\n\
! specification method appropriate to your system).  The contents of this\n\
! file can be moved into an X resource file, but since resources in this file\n\
! override their corresponding X resources, either this file should be \n\
! deleted or individual resource lines in the file should be deleted for the\n\
! moved lines to take effect.\n";

/* Module-global variable set when any preference changes (for asking the
   user about re-saving on exit) */
static int PrefsHaveChanged = False;

/* Module-global variable set when user uses -import to load additional
   preferences on top of the defaults.  Contains name of file loaded */
static char *ImportedFile = NULL;

/* Module-global variables to support Initial Window Size... dialog */
static int DoneWithSizeDialog;
static Widget RowText, ColText;

/* Module-global variables for Tabs dialog */
static int DoneWithTabsDialog;
static WindowInfo *TabsDialogForWindow;
static Widget TabDistText, EmTabText, EmTabToggle, UseTabsToggle, EmTabLabel;

/* Module-global variables for Wrap Margin dialog */
static int DoneWithWrapDialog;
static WindowInfo *WrapDialogForWindow;
static Widget WrapText, WrapTextLabel, WrapWindowToggle;

/*  Module-global variables for shell selection dialog  */
static int DoneWithShellSelDialog = False;

static void translatePrefFormats(int convertOld, int fileVer);
static void setIntPref(int *prefDataField, int newValue);
static void setStringPref(char *prefDataField, const char *newValue);
static void sizeOKCB(Widget w, XtPointer clientData, XtPointer callData);
static void sizeCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void tabsOKCB(Widget w, XtPointer clientData, XtPointer callData);
static void tabsCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void tabsHelpCB(Widget w, XtPointer clientData, XtPointer callData);
static void emTabsCB(Widget w, XtPointer clientData, XtPointer callData);
static void wrapOKCB(Widget w, XtPointer clientData, XtPointer callData);
static void wrapCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void wrapWindowCB(Widget w, XtPointer clientData, XtPointer callData);
static void shellSelOKCB(Widget widget, XtPointer clientData,
        XtPointer callData);
static void shellSelCancelCB(Widget widgget, XtPointer clientData,
        XtPointer callData);
static void reapplyLanguageMode(WindowInfo *window, int mode,int forceDefaults);

static void fillFromPrimaryCB(Widget w, XtPointer clientData,
    	XtPointer callData);
static int checkFontStatus(fontDialog *fd, Widget fontTextFieldW);
static int showFontStatus(fontDialog *fd, Widget fontTextFieldW,
    	Widget errorLabelW);
static void primaryModifiedCB(Widget w, XtPointer clientData,
	XtPointer callData);
static void italicModifiedCB(Widget w, XtPointer clientData,
    	XtPointer callData);
static void boldModifiedCB(Widget w, XtPointer clientData, XtPointer callData);
static void boldItalicModifiedCB(Widget w, XtPointer clientData,
    	XtPointer callData);
static void primaryBrowseCB(Widget w, XtPointer clientData, XtPointer callData);
static void italicBrowseCB(Widget w, XtPointer clientData, XtPointer callData);
static void boldBrowseCB(Widget w, XtPointer clientData, XtPointer callData);
static void boldItalicBrowseCB(Widget w, XtPointer clientData,
    	XtPointer callData);
static void browseFont(Widget parent, Widget fontTextW);
static void fontDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void fontOkCB(Widget w, XtPointer clientData, XtPointer callData);
static void fontApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static void fontCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void updateFonts(fontDialog *fd);

static Boolean checkColorStatus(colorDialog *cd, Widget colorFieldW);
static int verifyAllColors (colorDialog *cd);
static void showColorStatus (colorDialog *cd, Widget colorFieldW,
      Widget errorLabelW);
static void updateColors(colorDialog *cd);
static void colorDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void colorOkCB     (Widget w, XtPointer clientData, XtPointer callData);
static void colorApplyCB  (Widget w, XtPointer clientData, XtPointer callData);
static void colorCloseCB(Widget w, XtPointer clientData, XtPointer callData);
static void indentRainbowDialogLoadColors(colorDialog *cd);
static void loadAnsiColors(colorDialog *cd);
static void loadColors(colorDialog *cd);
static void loadColorProfileStyleSettings(colorDialog *cd);
static void clearRainbowColors(colorDialog *cd);
static void saveColorProfileSettings(colorDialog *cd);

static int matchLanguageMode(WindowInfo *window);
static int loadLanguageModesString(char *inString, int fileVer);
static char *writeLanguageModesString(void);
static char *createExtString(char **extensions, int nExtensions);
static char **readExtensionList(char **inPtr, int *nExtensions);
static void updateLanguageModeSubmenu(WindowInfo *window);
static void setLangModeCB(Widget w, XtPointer clientData, XtPointer callData);
#ifdef ENABLE_COLORPROFILES
static void updateColorProfilesMenu(WindowInfo *window);
#else
#define updateColorProfilesMenu(w)
#endif
static int modeError(languageModeRec *lm, const char *stringStart,
	const char *stoppedAt, const char *message);
static void lmDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void lmOkCB(Widget w, XtPointer clientData, XtPointer callData);
static void lmApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static void lmCloseCB(Widget w, XtPointer clientData, XtPointer callData);
static int lmDeleteConfirmCB(int itemIndex, void *cbArg);
static int updateLMList(void);
static languageModeRec *copyLanguageModeRec(languageModeRec *lm);
static void *lmGetDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg);
static void lmSetDisplayedCB(void *item, void *cbArg);
static languageModeRec *readLMDialogFields(int silent);
static void lmFreeItemCB(void *item);
static void freeLanguageModeRec(languageModeRec *lm);
static int lmDialogEmpty(void);
static void updatePatternsTo5dot1(void);
static void updatePatternsTo6dot1(void);
static void updatePatternsTo6dot2(void);
static void updatePatternsTo6dot3(void);
static void migrateColorResources(XrmDatabase prefDB, XrmDatabase appDB);
static void spliceString(char **intoString, const char *insertString, const char *atExpr);
static int regexFind(const char *inString, const char *expr);
static int regexReplace(char **inString, const char *expr,
                        const char *replaceWith);
static int caseFind(const char *inString, const char *expr);
static int caseReplace(char **inString, const char *expr,
                       const char *replaceWith, int replaceLen);
static int stringReplace(char **inString, const char *expr, 
                         const char *replaceWith, int searchType,
                         int replaceLen);
static int replaceMacroIfUnchanged(const char* oldText, const char* newStart, 
                                    const char* newEnd);
static const char* getDefaultShell(void);
static void parseIconSize(char *iconSize);


#ifdef SGI_CUSTOM
static int shortPrefToDefault(Widget parent, const char *settingName, int *setDefault);
#endif

XrmDatabase CreateNEditPrefDB(int *argcInOut, char **argvInOut)
{
    return CreatePreferencesDatabase(GetRCFileName(NEDIT_RC), APP_NAME,
            OpTable, XtNumber(OpTable), (unsigned int *)argcInOut, argvInOut);
}
    
void RestoreNEditPrefs(XrmDatabase prefDB, XrmDatabase appDB)
{
    int requiresConversion;
    int major;              /* The integral part of version number */
    int minor;              /* fractional part of version number */
    int fileVer = 0;        /* Both combined into an integer */
    int nparsed;
    
    /* Load preferences */
    RestorePreferences(prefDB, appDB, APP_NAME,
    	    APP_CLASS, PrefDescrip, XtNumber(PrefDescrip));

    /* If the preferences file was written by an older version of NEdit,
       warn the user that it will be converted. */
    requiresConversion = PrefData.prefFileRead &&
    	    PrefData.fileVersion[0] == '\0';
    if (requiresConversion) {
	updatePatternsTo5dot1();
    }

    if (PrefData.prefFileRead) {
        if (PrefData.fileVersion[0] == '\0') {
            fileVer = 0;    /* Pre-5.1 */
        }
        else {
            /* Note: do not change the format of this.  Older executables
               need to read this field for forward compatability. */
            nparsed = sscanf(PrefData.fileVersion, "%d.%d", &major, &minor);
            if (nparsed >= 2) {
                /* Use OSF-style numbering scheme */
                fileVer = major * 1000 + minor;
            }
        }
    }
    
    if (PrefData.prefFileRead && fileVer < 6001) {
        updatePatternsTo6dot1();
    }
    if (PrefData.prefFileRead && fileVer < 6002) {
        updatePatternsTo6dot2();
    }
    if (PrefData.prefFileRead && fileVer < 6003) {
        updatePatternsTo6dot3();
    }

    /* Note that we don't care about unreleased file versions.  Anyone
       who is running a CVS or alpha version of NEdit is resposnbile
       for managing the preferences file themselves.  Otherwise, it
       gets impossible to track the number of "in-between" file formats.
       We only do auto-upgrading for a real release. */

    
    /* Migrate colors if there's no config file yet */
    if (!PrefData.prefFileRead) {
        migrateColorResources(prefDB, appDB);
    }
   
    /* Do further parsing on resource types which RestorePreferences does
       not understand and reads as strings, to put them in the final form
       in which nedit stores and uses.  If the preferences file was
       written by an older version of NEdit, update regular expressions in
       highlight patterns to quote braces and use & instead of \0 */
    translatePrefFormats(requiresConversion, fileVer);
    
#ifdef EXCLUDE_FONTS
    FontInitExcludes(PrefData.fontExcludes);
#endif
}

/*
** Many of of NEdit's preferences are much more complicated than just simple
** integers or strings.  These are read as strings, but must be parsed and
** translated into something meaningful.  This routine does the translation,
** and, in most cases, frees the original string, which is no longer useful.
**
** In addition this function covers settings that, while simple, require
** additional steps before they can be published.
**
** The argument convertOld attempts a conversion from pre 5.1 format .nedit
** files (which means patterns and macros may contain regular expressions
** which are of the older syntax where braces were not quoted, and \0 was a
** legal substitution character).  Macros, so far can not be automatically
** converted, unfortunately.
*/
static void translatePrefFormats(int convertOld, int fileVer)
{
    /* Parse the strings which represent types which are not decoded by
       the standard resource manager routines */
    if (TempStringPrefs.shellCmds != NULL) {
	LoadShellCmdsString(TempStringPrefs.shellCmds);
	NEditFree(TempStringPrefs.shellCmds);
	TempStringPrefs.shellCmds = NULL;
    }
    if (TempStringPrefs.macroCmds != NULL) {
	LoadMacroCmdsString(TempStringPrefs.macroCmds);
    	NEditFree(TempStringPrefs.macroCmds);
	TempStringPrefs.macroCmds = NULL;
    }
    if (TempStringPrefs.bgMenuCmds != NULL) {
	LoadBGMenuCmdsString(TempStringPrefs.bgMenuCmds);
    	NEditFree(TempStringPrefs.bgMenuCmds);
	TempStringPrefs.bgMenuCmds = NULL;
    }
    if (TempStringPrefs.highlight != NULL) {
	LoadHighlightString(TempStringPrefs.highlight, convertOld);
    	NEditFree(TempStringPrefs.highlight);
	TempStringPrefs.highlight = NULL;
    }
    if (TempStringPrefs.colorProfiles != NULL) {
        ParseColorProfiles(TempStringPrefs.colorProfiles);
        NEditFree(TempStringPrefs.colorProfiles);
        TempStringPrefs.colorProfiles = NULL;
    } else {
        ParseColorProfiles("");
    }
    if (TempStringPrefs.styles != NULL) {
        LoadStylesString(TempStringPrefs.styles, False);
        NEditFree(TempStringPrefs.styles);
        TempStringPrefs.styles = NULL;
    }
    if (TempStringPrefs.colorProfileStyles != NULL) {
        if(strlen(TempStringPrefs.colorProfileStyles) > 0) {
            LoadStylesString(TempStringPrefs.colorProfileStyles, True);
            PrefDescrip[7].save = True;
        }
        NEditFree(TempStringPrefs.colorProfileStyles);
        TempStringPrefs.colorProfileStyles = NULL;
    }
    if (TempStringPrefs.language != NULL) {
	loadLanguageModesString(TempStringPrefs.language, fileVer);
    	NEditFree(TempStringPrefs.language);
	TempStringPrefs.language = NULL;
    }
    if (TempStringPrefs.smartIndent != NULL) {
	LoadSmartIndentString(TempStringPrefs.smartIndent);
    	NEditFree(TempStringPrefs.smartIndent);
	TempStringPrefs.smartIndent = NULL;
    }
    if (TempStringPrefs.smartIndentCommon != NULL) {
	LoadSmartIndentCommonString(TempStringPrefs.smartIndentCommon);
	NEditFree(TempStringPrefs.smartIndentCommon);
	TempStringPrefs.smartIndentCommon = NULL;
    }
    if(TempStringPrefs.filter) {
        ParseFilterSettings(TempStringPrefs.filter);
        NEditFree(TempStringPrefs.filter);
        TempStringPrefs.filter = NULL;
    }
    if (PrefData.iconSize) {
        parseIconSize(PrefData.iconSize);
    }
     
    PrefData.font = FontFromName(TheDisplay, PrefData.fontString);
    PrefData.boldFont = FontFromName(TheDisplay, PrefData.boldFontString);
    PrefData.italicFont = FontFromName(TheDisplay, PrefData.italicFontString);
    PrefData.boldItalicFont = FontFromName(TheDisplay, PrefData.boldItalicFontString);
    
    /*
    **  The default set for the comand shell in PrefDescrip ("DEFAULT") is
    **  only a place-holder, the actual default is the user's login shell
    **  (or whatever is implemented in getDefaultShell()). We put the login
    **  shell's name in PrefData here.
    */
    if (0 == strcmp(PrefData.shell, "DEFAULT")) {
        strncpy(PrefData.shell, getDefaultShell(), MAXPATHLEN);
        PrefData.shell[MAXPATHLEN] = '\0';
    }

    /* For compatability with older (4.0.3 and before) versions, the autoWrap
       and autoIndent resources can accept values of True and False.  Translate
       them into acceptable wrap and indent styles */
    if (PrefData.wrapStyle == 3) PrefData.wrapStyle = CONTINUOUS_WRAP;
    if (PrefData.wrapStyle == 4) PrefData.wrapStyle = NO_WRAP;
    if (PrefData.autoIndent == 3) PrefData.autoIndent = AUTO_INDENT;
    if (PrefData.autoIndent == 4) PrefData.autoIndent = NO_AUTO_INDENT;

    /* setup language mode dependent info of user menus (to increase
       performance when switching between documents of different
       language modes) */
    SetupUserMenuInfo();
}

void SaveNEditPrefs(Widget parent, int quietly)
{
    const char* prefFileName = GetRCFileName(NEDIT_RC);
    if (prefFileName == NULL)
    {
        /*  GetRCFileName() might return NULL if an error occurs during
            creation of the preference file directory. */
        DialogF(DF_WARN, parent, 1, "Error saving Preferences",
                "Unable to save preferences: Cannot determine filename.",
                "OK");
        return;
    }

    if (!quietly) {
        if (DialogF(DF_INF, parent, 2, "Save Preferences",
                ImportedFile == NULL ?
                "Default preferences will be saved in the file:\n"
                "%s\n"
                "XNEdit automatically loads this file\n"
                "each time it is started." :
                "Default preferences will be saved in the file:\n"
                "%s\n"
                "SAVING WILL INCORPORATE SETTINGS\n"
                "FROM FILE: %s", "OK", "Cancel",
                prefFileName, ImportedFile) == 2) {
            return;
        }
    }

    /*  Write the more dynamic settings into TempStringPrefs.
        These locations are set in PrefDescrip, so this is where
        SavePreferences() will look for them.  */
    TempStringPrefs.shellCmds = WriteShellCmdsString();
    TempStringPrefs.macroCmds = WriteMacroCmdsString();
    TempStringPrefs.bgMenuCmds = WriteBGMenuCmdsString();
    TempStringPrefs.highlight = WriteHighlightString();
    TempStringPrefs.language = writeLanguageModesString();
    TempStringPrefs.styles = WriteStylesString();
    TempStringPrefs.colorProfileStyles = WriteColorProfileStylesString();
    TempStringPrefs.smartIndent = WriteSmartIndentString();
    TempStringPrefs.smartIndentCommon = WriteSmartIndentCommonString();
    TempStringPrefs.colorProfiles = WriteColorProfilesString();
    TempStringPrefs.filter = WriteFilterString();
    strcpy(PrefData.fileVersion, PREF_FILE_VERSION);

    if (!SavePreferences(XtDisplay(parent), prefFileName, HeaderText,
            PrefDescrip, XtNumber(PrefDescrip)))
    {
        DialogF(DF_WARN, parent, 1, "Save Preferences",
                "Unable to save preferences in %s", "OK", prefFileName);
    }

    NEditFree(TempStringPrefs.shellCmds);
    NEditFree(TempStringPrefs.macroCmds);
    NEditFree(TempStringPrefs.bgMenuCmds);
    NEditFree(TempStringPrefs.highlight);
    NEditFree(TempStringPrefs.language);
    NEditFree(TempStringPrefs.styles);
    NEditFree(TempStringPrefs.smartIndent);
    NEditFree(TempStringPrefs.smartIndentCommon);
    NEditFree(TempStringPrefs.colorProfiles);
    
    TempStringPrefs.shellCmds = NULL;
    TempStringPrefs.macroCmds = NULL;
    TempStringPrefs.bgMenuCmds = NULL;
    TempStringPrefs.highlight = NULL;
    TempStringPrefs.language = NULL;
    TempStringPrefs.styles = NULL;
    TempStringPrefs.smartIndent = NULL;
    TempStringPrefs.smartIndentCommon = NULL;
    TempStringPrefs.colorProfiles = NULL;
    NEditFree(TempStringPrefs.filter);
    
    PrefsHaveChanged = False;
}

/*
** Load an additional preferences file on top of the existing preferences
** derived from defaults, the .nedit file, and X resources.
*/
void ImportPrefFile(const char *filename, int convertOld)
{
    XrmDatabase db;
    char *fileString;
    
    fileString = ReadAnyTextFile(filename, False);
    if (fileString != NULL){
        db = XrmGetStringDatabase(fileString);
        NEditFree(fileString);
        OverlayPreferences(db, APP_NAME, APP_CLASS, PrefDescrip,
        XtNumber(PrefDescrip));
        translatePrefFormats(convertOld, -1);
        ImportedFile = NEditStrdup(filename);
    } else
    {
        fprintf(stderr, "Could not read additional preferences file: %s\n",
                filename);
    }
}

void SetPrefOpenInTab(int state)
{
    WindowInfo *w = WindowList;
    setIntPref(&PrefData.openInTab, state);
    for(; w != NULL; w = w->next)
        UpdateNewOppositeMenu(w, state);
}

int GetPrefOpenInTab(void)
{
    return PrefData.openInTab;
}

void SetPrefWrap(WrapStyle state)
{
    setIntPref(&PrefData.wrapStyle, (int)state);
}

WrapStyle GetPrefWrap(int langMode)
{
    if (langMode == PLAIN_LANGUAGE_MODE ||
	    LanguageModes[langMode]->wrapStyle == DEFAULT_WRAP)
    	return (WrapStyle)(PrefData.wrapStyle);
    return LanguageModes[langMode]->wrapStyle;
}

void SetPrefWrapMargin(int margin)
{
    setIntPref(&PrefData.wrapMargin, margin);
}

int GetPrefWrapMargin(void)
{
    return PrefData.wrapMargin;
}

void SetPrefSearch(int searchType)
{
    setIntPref(&PrefData.searchMethod, searchType);
}

int GetPrefSearch(void)
{
    return PrefData.searchMethod;
}

#ifdef REPLACE_SCOPE
void SetPrefReplaceDefScope(int scope)
{
    setIntPref(&PrefData.replaceDefScope, scope);
}

int GetPrefReplaceDefScope(void)
{
    return PrefData.replaceDefScope;
}
#endif

void SetPrefAutoIndent(IndentStyle state)
{
    setIntPref(&PrefData.autoIndent, (int)state);
}

IndentStyle GetPrefAutoIndent(int langMode)
{
    if (langMode == PLAIN_LANGUAGE_MODE ||
	    LanguageModes[langMode]->indentStyle == DEFAULT_INDENT)
    	return (IndentStyle)(PrefData.autoIndent);
    return LanguageModes[langMode]->indentStyle;
}

void SetPrefAutoSave(int state)
{
    setIntPref(&PrefData.autoSave, state);
}

int GetPrefAutoSave(void)
{
    return PrefData.autoSave;
}

void SetPrefSaveOldVersion(int state)
{
    setIntPref(&PrefData.saveOldVersion, state);
}

int GetPrefSaveOldVersion(void)
{
    return PrefData.saveOldVersion;
}

void SetPrefSaveSearchHistory(int state)
{
    setIntPref(&PrefData.saveSearchHistory, state);
}

int GetPrefSaveSearchHistory(void)
{
    return PrefData.saveSearchHistory;
}

void SetPrefSearchDlogs(int state)
{
    setIntPref(&PrefData.searchDlogs, state);
}

int GetPrefSearchDlogs(void)
{
    return PrefData.searchDlogs;
}

void SetPrefBeepOnSearchWrap(int state)
{
    setIntPref(&PrefData.searchWrapBeep, state);
}

int GetPrefBeepOnSearchWrap(void)
{
    return PrefData.searchWrapBeep;
}

void SetPrefKeepSearchDlogs(int state)
{
    setIntPref(&PrefData.keepSearchDlogs, state);
}

int GetPrefKeepSearchDlogs(void)
{
    return PrefData.keepSearchDlogs;
}

void SetPrefSearchWraps(int state)
{
    setIntPref(&PrefData.searchWraps, state);
}

int GetPrefStickyCaseSenseBtn(void)
{
    return PrefData.stickyCaseSenseBtn;
}

int GetPrefSearchWraps(void)
{
    return PrefData.searchWraps;
}

void SetPrefStatsLine(int state)
{
    setIntPref(&PrefData.statsLine, state);
}

int GetPrefStatsLine(void)
{
    return PrefData.statsLine;
}

void SetPrefISearchLine(int state)
{
    setIntPref(&PrefData.iSearchLine, state);
}

int GetPrefISearchLine(void)
{
    return PrefData.iSearchLine;
}

int GetPrefZoomStep(void)
{
    return PrefData.zoomStep;
}

void SetPrefZoomStep(int step)
{
    setIntPref(&PrefData.zoomStep, step);
}

int GetPrefZoomCtrlMouseWheel(void)
{
    return PrefData.zoomCtrlMouseWheel;
}

void SetPrefZoomCtrlMouseWheel(Boolean s)
{
    setIntPref(&PrefData.zoomCtrlMouseWheel, s);
}

void SetPrefSortTabs(int state)
{
    setIntPref(&PrefData.sortTabs, state);
}

int GetPrefSortTabs(void)
{
    return PrefData.sortTabs;
}

void SetPrefTabBar(int state)
{
    setIntPref(&PrefData.tabBar, state);
}

int GetPrefTabBar(void)
{
    return PrefData.tabBar;
}

void SetPrefTabBarHideOne(int state)
{
    setIntPref(&PrefData.tabBarHideOne, state);
}

int GetPrefTabBarHideOne(void)
{
    return PrefData.tabBarHideOne;
}

void SetPrefGlobalTabNavigate(int state)
{
    setIntPref(&PrefData.globalTabNavigate, state);
}

int GetPrefGlobalTabNavigate(void)
{
    return PrefData.globalTabNavigate;
}

void SetPrefToolTips(int state)
{
    setIntPref(&PrefData.toolTips, state);
}

int GetPrefToolTips(void)
{
    return PrefData.toolTips;
}

void SetPrefLineNums(int state)
{
    setIntPref(&PrefData.lineNums, state);
}

int GetPrefLineNums(void)
{
    return PrefData.lineNums;
}

void SetPrefShowPathInWindowsMenu(int state)
{
    setIntPref(&PrefData.pathInWindowsMenu, state);
}

int GetPrefShowPathInWindowsMenu(void)
{
    return PrefData.pathInWindowsMenu;
}

void SetPrefWarnFileMods(int state)
{
    setIntPref(&PrefData.warnFileMods, state);
}

int GetPrefWarnFileMods(void)
{
    return PrefData.warnFileMods;
}

void SetPrefWarnRealFileMods(int state)
{
    setIntPref(&PrefData.warnRealFileMods, state);
}

int GetPrefWarnRealFileMods(void)
{
    return PrefData.warnRealFileMods;
}

void SetPrefWarnExit(int state)
{
    setIntPref(&PrefData.warnExit, state);
}

int GetPrefWarnExit(void)
{
    return PrefData.warnExit;
}

void SetPrefv(int state)
{
    setIntPref(&PrefData.findReplaceUsesSelection, state);
}

int GetPrefFindReplaceUsesSelection(void)
{
    return PrefData.findReplaceUsesSelection;
}

int GetPrefMapDelete(void)
{
    return PrefData.mapDelete;
}

int GetPrefStdOpenDialog(void)
{
    return PrefData.stdOpenDialog;
}

void SetPrefRows(int nRows)
{
    setIntPref(&PrefData.textRows, nRows);
}

int GetPrefRows(void)
{
    return PrefData.textRows;
}

void SetPrefCols(int nCols)
{
   setIntPref(&PrefData.textCols, nCols);
}

int GetPrefCols(void)
{
    return PrefData.textCols;
}

void SetPrefTabDist(int tabDist)
{
    setIntPref(&PrefData.tabDist, tabDist);
}

int GetPrefTabDist(int langMode)
{
    int tabDist;
    if (langMode == PLAIN_LANGUAGE_MODE ||
	    LanguageModes[langMode]->tabDist == DEFAULT_TAB_DIST) {
	tabDist = PrefData.tabDist; 
    } else {
	tabDist = LanguageModes[langMode]->tabDist;
    }
    /* Make sure that the tab distance is in range (garbage may have 
       been entered via the command line or the X resources, causing
       errors later on, like division by zero). */
    if (tabDist <= 0) return 1;
    if (tabDist > MAX_EXP_CHAR_LEN) return MAX_EXP_CHAR_LEN;
    return tabDist;
}

void SetPrefEmTabDist(int tabDist)
{
    setIntPref(&PrefData.emTabDist, tabDist);
}

int GetPrefEmTabDist(int langMode)
{
    if (langMode == PLAIN_LANGUAGE_MODE ||
	    LanguageModes[langMode]->emTabDist == DEFAULT_EM_TAB_DIST)
	return PrefData.emTabDist;
    return LanguageModes[langMode]->emTabDist;
}

void SetPrefInsertTabs(int state)
{
    setIntPref(&PrefData.insertTabs, state);
}

int GetPrefInsertTabs(void)
{
    return PrefData.insertTabs;
}

void SetPrefShowMatching(ShowMatchingStyle state)
{
    setIntPref(&PrefData.showMatchingStyle, (int)state);
}

ShowMatchingStyle GetPrefShowMatching(void)
{
    /*
     * For backwards compatibility with pre-5.2 versions, the boolean 
     * False/True matching behavior is converted to NO_FLASH/FLASH_DELIMIT. 
     */
    if (PrefData.showMatchingStyle >= N_SHOW_MATCHING_STYLES) 
	PrefData.showMatchingStyle -= N_SHOW_MATCHING_STYLES;
    return (ShowMatchingStyle)(PrefData.showMatchingStyle);
}

void SetPrefMatchSyntaxBased(int state)
{
    setIntPref(&PrefData.matchSyntaxBased, state);
}

int GetPrefMatchSyntaxBased(void)
{
    return PrefData.matchSyntaxBased;
}

void SetPrefHighlightSyntax(Boolean state)
{
    setIntPref(&PrefData.highlightSyntax, state);
}

Boolean GetPrefHighlightSyntax(void)
{
    return PrefData.highlightSyntax;
}

void SetPrefHighlightCursorLine(int state)
{
    setIntPref(&PrefData.highlightCursorLine, state);
}

int GetPrefHighlightCursorLine(void)
{
    return PrefData.highlightCursorLine;
}

void SetPrefIndentRainbow(int state)
{
    setIntPref(&PrefData.indentRainbow, state);
}

int GetPrefIndentRainbow(void)
{
    return PrefData.indentRainbow;
}

void SetPrefIndentRainbowColors(const char *colorList)
{
    if(PrefData.indentRainbowColors) NEditFree(PrefData.indentRainbowColors);
    PrefData.indentRainbowColors = NEditStrdup(colorList);
    PrefsHaveChanged = True;
}

char *GetPrefIndentRainbowColors(void)
{
    return PrefData.indentRainbowColors;
}

void SetPrefAnsiColorList(const char *colorList)
{
    if(PrefData.ansiColorList) NEditFree(PrefData.ansiColorList);
    PrefData.ansiColorList = NEditStrdup(colorList);
    PrefsHaveChanged = True;
}

char *GetPrefAnsiColorList(void)
{
    return PrefData.ansiColorList;
}

void SetPrefAnsiColors(int state)
{
    setIntPref(&PrefData.ansiColors, state);
}

Boolean GetPrefAnsiColors(void)
{
    return PrefData.ansiColors;
}

void SetPrefBacklightChars(int state)
{
    setIntPref(&PrefData.backlightChars, state);
}

int GetPrefBacklightChars(void)
{
    return PrefData.backlightChars;
}

char *GetPrefBacklightCharTypes(void)
{
    return PrefData.backlightCharTypes;
}

void SetPrefRepositionDialogs(int state)
{
    setIntPref(&PrefData.repositionDialogs, state);
}

int GetPrefRepositionDialogs(void)
{
    return PrefData.repositionDialogs;
}

void SetPrefAutoScroll(int state)
{
    WindowInfo *w;
    int margin = state ? PrefData.autoScrollVPadding : 0;
    
    setIntPref(&PrefData.autoScroll, state);
    for(w = WindowList; w != NULL; w = w->next)
        SetAutoScroll(w, margin);
}

int GetPrefAutoScroll(void)
{
    return PrefData.autoScroll;
}

int GetVerticalAutoScroll(void)
{
    return PrefData.autoScroll ? PrefData.autoScrollVPadding : 0;
}

void SetPrefEditorConfig(int state)
{
    setIntPref(&PrefData.editorConfig, state);
}

int GetPrefEditorConfig(void)
{
    return PrefData.editorConfig;
}

void SetPrefAppendLF(int state)
{
    setIntPref(&PrefData.appendLF, state);
}

int GetPrefAppendLF(void)
{
    return PrefData.appendLF;
}

void SetPrefSortOpenPrevMenu(int state)
{
    setIntPref(&PrefData.sortOpenPrevMenu, state);
}

int GetPrefSortOpenPrevMenu(void)
{
    return PrefData.sortOpenPrevMenu;
}

char *GetPrefTagFile(void)
{
    return PrefData.tagFile;
}

void SetPrefSmartTags(int state)
{
    setIntPref(&PrefData.smartTags, state);
}

int GetPrefSmartTags(void)
{
    return PrefData.smartTags;
}

int GetPrefAlwaysCheckRelTagsSpecs(void)
{
    return PrefData.alwaysCheckRelativeTagsSpecs;
}

char *GetPrefDelimiters(void)
{
    return PrefData.delimiters;
}

char *GetPrefColorName(int index)
{
    return PrefData.colorNames[index];
}

void SetPrefColorName(int index, const char *name)
{
    setStringPref(PrefData.colorNames[index], name);
}

/*
** Set the font preferences using the font name (the fontList is generated
** in this call).  Note that this leaks memory and server resources each
** time the default font is re-set.  See note on SetFontByName in window.c
** for more information.
*/
void SetPrefFont(char *fontName)
{
    setStringPref(PrefData.fontString, fontName);
    FontUnref(PrefData.font);
    PrefData.font = FontFromName(TheDisplay, fontName);
}

void SetPrefBoldFont(char *fontName)
{
    
    setStringPref(PrefData.boldFontString, fontName);
    FontUnref(PrefData.boldFont);
    PrefData.boldFont = FontFromName(TheDisplay, fontName);
}

void SetPrefItalicFont(char *fontName)
{
    setStringPref(PrefData.italicFontString, fontName);
    FontUnref(PrefData.italicFont);
    PrefData.italicFont = FontFromName(TheDisplay, fontName);
}
void SetPrefBoldItalicFont(char *fontName)
{
    setStringPref(PrefData.boldItalicFontString, fontName);
    FontUnref(PrefData.boldItalicFont);
    PrefData.boldItalicFont = FontFromName(TheDisplay, fontName);
}

char *GetPrefFontName(void)
{
    return PrefData.fontString;
}

char *GetPrefBoldFontName(void)
{
    return PrefData.boldFontString;
}

char *GetPrefItalicFontName(void)
{
    return PrefData.italicFontString;
}

char *GetPrefBoldItalicFontName(void)
{
    return PrefData.boldItalicFontString;
}

NFont *GetPrefFont(void)
{
    return PrefData.font;
}

NFont *GetPrefBoldFont(void)
{
    return PrefData.boldFont;
}

NFont *GetPrefItalicFont(void)
{
    return PrefData.italicFont;
}

NFont *GetPrefBoldItalicFont(void)
{
    return PrefData.boldItalicFont;
}

char *GetPrefHelpFontName(int index)
{
    return PrefData.helpFontNames[index];
}

char *GetPrefHelpLinkColor(void)
{
    return PrefData.helpLinkColor;
}

char *GetPrefTooltipBgColor(void)
{
    return PrefData.tooltipBgColor;
}

void SetPrefShell(const char *shell)
{
    setStringPref(PrefData.shell, shell);
}

const char* GetPrefShell(void)
{
    return PrefData.shell;
}

char *GetPrefGeometry(void)
{
    return PrefData.geometry;
}

char *GetPrefServerName(void)
{
    return PrefData.serverName;
}

char *GetPrefBGMenuBtn(void)
{
    return PrefData.bgMenuBtn;
}

int GetPrefMaxPrevOpenFiles(void)
{
    return PrefData.maxPrevOpenFiles;
}

int GetPrefTypingHidesPointer(void)
{
    return(PrefData.typingHidesPointer);
}

int GetEditorConfig(void)
{
    return PrefData.editorConfig;
}

#ifdef SGI_CUSTOM
void SetPrefShortMenus(int state)
{
    setIntPref(&PrefData.shortMenus, state);
}

int GetPrefShortMenus(void)
{
    return PrefData.shortMenus;
}
#endif

void SetPrefTitleFormat(const char* format)
{
    const WindowInfo* window;

    setStringPref(PrefData.titleFormat, format);

    /* update all windows */
    for (window=WindowList; window!=NULL; window=window->next) {
        UpdateWindowTitle(window);
    }
}
const char* GetPrefTitleFormat(void)
{
    return PrefData.titleFormat;
}

Boolean GetPrefUndoModifiesSelection(void)
{
    return (Boolean)PrefData.undoModifiesSelection;
}

Boolean GetPrefFocusOnRaise(void)
{
    return (Boolean)PrefData.focusOnRaise;
}

Boolean GetPrefForceOSConversion(void)
{
    return (Boolean) PrefData.forceOSConversion;
}

Boolean GetPrefHonorSymlinks(void)
{
    return (Boolean)PrefData.honorSymlinks;
}

Boolean GetAutoEnableXattr(void)
{
    return (Boolean)PrefData.autoEnableXattr;
}

Boolean GetWindowDarkTheme(void)
{
    return (Boolean)PrefData.windowDarkTheme;
}

int GetPrefFsbView(void)
{
    return PrefData.fsbView;
}

void SetPrefFsbView(int view) {
    setIntPref(&PrefData.fsbView, view);
}

int GetPrefFsbFileCmp(void)
{
    return PrefData.fsbFileCmp;
}

void SetPrefFsbFileCmp(int cmp) {
    setIntPref(&PrefData.fsbFileCmp, cmp);
}

Boolean GetPrefFsbShowHidden(void)
{
    return (Boolean)PrefData.fsbShowHidden;
}

void SetPrefFsbShowHidden(Boolean v) {
    setIntPref(&PrefData.fsbShowHidden, v);
}

int GetPrefOverrideVirtKeyBindings(void)
{
    return PrefData.virtKeyOverride;
}

int GetPrefTruncSubstitution(void)
{
    return PrefData.truncSubstitution;
}

const char* GetPrefDefaultCharset(void)
{
    if(!strcmp(PrefData.defaultCharset, "locale")) {
        char *l = nl_langinfo(CODESET);
        if(!l) l = "UTF-8";
        return l;
    }
    
    return PrefData.defaultCharset;
}

const char* GetPrefFallbackCharset(void) {
    return PrefData.fallbackCharset;
}

int GetPrefCloseIconSize(void)
{
    return PrefData.closeIconSize;
}

int GetPrefISrcFindIconSize(void)
{
    return PrefData.isrcFindIconSize;
}

int GetPrefISrcClearIconSize(void)
{
    return PrefData.isrcClearIconSize;
}

void SetPrefIconSize(const char *sizeStr)
{
    NEditFree(PrefData.iconSize);
    PrefData.iconSize = NEditStrdup(sizeStr);
    PrefsHaveChanged = True;
}

void SetPrefLockEncodingError(int state)
{
    setIntPref(&PrefData.lockEncodingError, state);
}

int GetPrefLockEncodingError(void)
{
    return PrefData.lockEncodingError;
}

ColorProfile* GetDefaultColorProfile(void) {
    return defaultColorProfile;
}

ColorProfile* GetColorProfiles(void) 
{
    return colorProfiles;
}

ColorProfile* GetColorProfile(const char *name)
{
    ColorProfile *cp = colorProfiles;
    while(cp) {
        if(!strcmp(cp->name, name)) {
            return cp;
        }
        cp = cp->next;
    }
    return NULL;
}

static Boolean defResDBEnable = True;

void EnableDefaultColorProfileResourceDB(Display *dp)
{
    if(defResDBEnable && defaultColorProfile->db) {
        XrmSetDatabase(dp, defaultColorProfile->db);
    }
}

void SetDefResDBEnable(Boolean set) 
{
    defResDBEnable = set;
}

char* GetPrefDefaultColorProfileName(void) {
    return PrefData.defaultColorProfile;
}

void SetPrefDefaultColorProfileName(const char *str) {
    if(PrefData.defaultColorProfile) NEditFree(PrefData.defaultColorProfile);
    PrefData.defaultColorProfile = NEditStrdup(str);
    PrefsHaveChanged = True;
}


void ColorProfileCopySettings(ColorProfile *from, ColorProfile *to)
{
    to->name = NEditStrdup(from->name);
    to->textFg = NEditStrdup(from->textFg);
    to->textBg = NEditStrdup(from->textBg);
    to->selectFg = NEditStrdup(from->selectFg);
    to->selectBg = NEditStrdup(from->selectBg);
    to->hiliteFg = NEditStrdup(from->hiliteFg);
    to->hiliteBg = NEditStrdup(from->hiliteBg);
    to->lineNoFg = NEditStrdup(from->lineNoFg);
    to->lineNoBg = NEditStrdup(from->lineNoBg);
    to->cursorFg = NEditStrdup(from->cursorFg);
    to->lineHiBg = NEditStrdup(from->lineHiBg);
    to->ansiColorList = NEditStrdup(from->ansiColorList);
    to->rainbowColorList = NEditStrdup(from->rainbowColorList);
    to->resourceFile = from->resourceFile ? NEditStrdup(from->resourceFile) : NULL;
    to->styleType = from->styleType;
    to->windowThemeVariant = from->windowThemeVariant;
    to->orig = from;
}

int ColorProfileResourceDBEqual(ColorProfile *c1, ColorProfile *c2) {
    char *s1 = c1->resourceFile ? c1->resourceFile : "";
    char *s2 = c2->resourceFile ? c2->resourceFile : "";
    return !strcmp(s1, s2);
}

int GetNumColorProfiles(void)
{
    int n = 0;
    ColorProfile *p = colorProfiles;
    while(p) {
        n++;
        p = p->next;
    }
    return n;
}

char* WriteColorProfilesString(void)
{
    // skip default profile
    ColorProfile *profile = colorProfiles->next;
    
    size_t alloc = GetNumColorProfiles() * 512;
    size_t size = 0;
    char *out = NEditMalloc(alloc);
    
    while(profile) {
        if(!profile->removed) {
            char buf[4096];
            int len = snprintf(
                    buf,
                    4096,
                    "%s:colors:{%s;%s;%s;%s;%s;%s;%s;%s;%s;%s},"
                    "ansi:{%s},rainbow:{%s},styles:{%d},wm:{%d}%s%s%s%s",
                    profile->name,
                    profile->textFg,
                    profile->textBg,
                    profile->selectFg,
                    profile->selectBg,
                    profile->hiliteFg,
                    profile->hiliteBg,
                    profile->lineNoFg,
                    profile->lineNoBg,
                    profile->cursorFg,
                    profile->lineHiBg,
                    profile->ansiColorList,
                    profile->rainbowColorList,
                    profile->styleType,
                    profile->windowThemeVariant,
                    profile->resourceFile ? ",res:{" : "",
                    profile->resourceFile ? profile->resourceFile : "",
                    profile->resourceFile ? "}" : "",
                    profile->next ? "\\n\\\n" : ""
                    );
            if(len < 1024) {
                if(size + len >= alloc) {
                    alloc += len + 256;
                    out = NEditRealloc(out, alloc);
                }
                memcpy(out+size, buf, len);
                size += len;
            }
        }
        
        profile = profile->next;
    }
    
    if(size >= alloc) {
        alloc++;
        out = NEditRealloc(out, alloc);
    }
    out[size] = 0;
    
    return out;
}

void SetPrefUndoPurgeLimit(int limit)
{
    setIntPref(&PrefData.undoPurgeLimit, limit);
}

int GetPrefUndoPurgeLimit(void) 
{
    return PrefData.undoPurgeLimit;
}

void SetPrefUndoPurgeTrimTo(int limit)
{
    setIntPref(&PrefData.undoPurgeTrimTo, limit);
}

int GetPrefUndoPurgeTrimTo(void)
{
    return PrefData.undoPurgeTrimTo;
}

void SetPrefUndoWorryLimit(int limit)
{
    setIntPref(&PrefData.undoWorryLimit, limit);
}

int GetPrefUndoWorryLimit(void)
{
    return PrefData.undoWorryLimit;
}

void SetPrefUndoWorryTrimTo(int limit)
{
    setIntPref(&PrefData.undoWorryTrimTo, limit);
}

int GetPrefUndoWorryTrimTo(void)
{
    return PrefData.undoWorryTrimTo;
}

void SetPrefUndoOpLimit(int limit)
{
    setIntPref(&PrefData.undoOpLimit, limit);
}

int GetPrefUndoOpLimit(void)
{
    return PrefData.undoOpLimit;
}

void SetPrefUndoOpTrimTo(int limit)
{
    setIntPref(&PrefData.undoOpTrimTo, limit);
}

int GetPrefUndoOpTrimTo(void)
{
    return PrefData.undoOpTrimTo;
}


/*
** If preferences don't get saved, ask the user on exit whether to save
*/
void MarkPrefsChanged(void)
{
    PrefsHaveChanged = True;
}

/*
** Check if preferences have changed, and if so, ask the user if he wants
** to re-save.  Returns False if user requests cancelation of Exit (or whatever
** operation triggered this call to be made).
*/
int CheckPrefsChangesSaved(Widget dialogParent)
{
    int resp;
    
    if (!PrefsHaveChanged)
        return True;
    
    resp = DialogF(DF_WARN, dialogParent, 3, "Default Preferences",
            ImportedFile == NULL ?
            "Default Preferences have changed.\n"
            "Save changes to XNEdit preference file?" :
            "Default Preferences have changed.  SAVING \n"
            "CHANGES WILL INCORPORATE ADDITIONAL\nSETTINGS FROM FILE: %s",
            "Save", "Don't Save", "Cancel", ImportedFile);
    if (resp == 2)
        return True;
    if (resp == 3)
        return False;
    
    SaveNEditPrefs(dialogParent, True);
    return True;
}

/*
** set *prefDataField to newValue, but first check if they're different
** and update PrefsHaveChanged if a preference setting has now changed.
*/
static void setIntPref(int *prefDataField, int newValue)
{
    if (newValue != *prefDataField)
	PrefsHaveChanged = True;
    *prefDataField = newValue;
}

static void setStringPref(char *prefDataField, const char *newValue)
{
    if (strcmp(prefDataField, newValue))
	PrefsHaveChanged = True;
    strcpy(prefDataField, newValue);
}

/*
** Set the language mode for the window, update the menu and trigger language
** mode specific actions (turn on/off highlighting).  If forceNewDefaults is
** true, re-establish default settings for language-specific preferences
** regardless of whether they were previously set by the user.
*/
void SetLanguageMode(WindowInfo *window, int mode, int forceNewDefaults)
{
    Widget menu;
    WidgetList items;
    int n;
    Cardinal nItems;
    void *userData;
    
    /* Do mode-specific actions */
    reapplyLanguageMode(window, mode, forceNewDefaults);
    
    /* Select the correct language mode in the sub-menu */
    if (IsTopDocument(window)) {
	XtVaGetValues(window->langModeCascade, XmNsubMenuId, &menu, NULL);
	XtVaGetValues(menu, XmNchildren, &items, XmNnumChildren, &nItems, NULL);
	for (n=0; n<(int)nItems; n++) {
    	    XtVaGetValues(items[n], XmNuserData, &userData, NULL);
    	    XmToggleButtonSetState(items[n], (int)(intptr_t)userData == mode, False);
	}
    }
}

/*
** Lookup a language mode by name, returning the index of the language
** mode or PLAIN_LANGUAGE_MODE if the name is not found
*/
int FindLanguageMode(const char *languageName)
{
    int i;
 
    /* Compare each language mode to the one we were presented */
    for (i=0; i<NLanguageModes; i++)
	if (!strcmp(languageName, LanguageModes[i]->name))
	    return i;

    return PLAIN_LANGUAGE_MODE;
}


/*
** Apply language mode matching criteria and set window->languageMode to
** the appropriate mode for the current file, trigger language mode
** specific actions (turn on/off highlighting), and update the language
** mode menu item.  If forceNewDefaults is true, re-establish default
** settings for language-specific preferences regardless of whether
** they were previously set by the user.
*/
void DetermineLanguageMode(WindowInfo *window, int forceNewDefaults)
{
    SetLanguageMode(window, matchLanguageMode(window), forceNewDefaults);
}

/*
** Return the name of the current language mode set in "window", or NULL
** if the current mode is "Plain".
*/
char *LanguageModeName(int mode)
{
    if (mode == PLAIN_LANGUAGE_MODE)
    	return NULL;
    else
    	return LanguageModes[mode]->name;
}

/*
** Get the set of word delimiters for the language mode set in the current
** window.  Returns NULL when no language mode is set (it would be easy to
** return the default delimiter set when the current language mode is "Plain",
** or the mode doesn't have its own delimiters, but this is usually used
** to supply delimiters for RE searching, and ExecRE can skip compiling a
** delimiter table when delimiters is NULL).
*/
char *GetWindowDelimiters(const WindowInfo *window)
{
    if (window->languageMode == PLAIN_LANGUAGE_MODE)
    	return NULL;
    else
    	return LanguageModes[window->languageMode]->delimiters;
}

/*
** Put up a dialog for selecting a custom initial window size
*/
void RowColumnPrefDialog(Widget parent)
{
    Widget form, selBox, topLabel;
    Arg selBoxArgs[2];
    XmString s1;
    
    // get current size in cols/rows
    WindowInfo *window = WidgetToWindow(parent);
    Dimension twWidth, twHeight;
    NFont *font;
    int marginWidth, marginHeight, lineNumCols;
    XtVaGetValues(
            window->textArea,
            XmNwidth, &twWidth,
            XmNheight, &twHeight,
            textNXftFont, &font,
            textNmarginWidth, &marginWidth,
            textNmarginHeight, &marginHeight,
            textNlineNumCols, &lineNumCols,
            NULL);
    
    if(window->nPanes > 0) {
        int lastPaneHeight = 0;
        
        // get position of first text widget
        Position transX, transY;
        XtTranslateCoords(window->textArea, 0, 0, &transX, &transY);
        int minY = transY;
        int maxY = transY; 
        
        // get position of first text widget and position/height of the
        // last text widget, to calculate the overall height
        for(int i=0;i<window->nPanes;i++) {
            int paneHeight;
            XtVaGetValues(window->textPanes[i], XmNheight, &paneHeight, NULL);
            
            XtTranslateCoords(window->textPanes[i], 0, 0, &transX, &transY);
            
            if(transY < minY) {
                minY = transY;
            }
            if(transY > maxY) {
                maxY = transY;
                lastPaneHeight = paneHeight;
            }
        }
        
        twHeight = maxY + lastPaneHeight - minY;
    }
    
    
    int charWidth = font->maxWidth;
    XftFont *xfont = FontDefault(font);
    
    twWidth -= marginWidth*2 + (lineNumCols == 0 ? 0 : marginWidth + charWidth * lineNumCols);
    twHeight -= marginHeight * 2;
    
    int currentCols = twWidth / charWidth;
    int currentRows = twHeight / (xfont->ascent + xfont->descent);
    


    XtSetArg(selBoxArgs[0], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
    XtSetArg(selBoxArgs[1], XmNautoUnmanage, False);
    selBox = CreatePromptDialog(parent, "customSize", selBoxArgs, 2);
    XtAddCallback(selBox, XmNokCallback, (XtCallbackProc)sizeOKCB, NULL);
    XtAddCallback(selBox, XmNcancelCallback, (XtCallbackProc)sizeCancelCB,NULL);
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_SELECTION_LABEL));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_HELP_BUTTON));
    XtVaSetValues(XtParent(selBox), XmNtitle, "Initial Window Size", NULL);
    
    form = XtVaCreateManagedWidget("form", xmFormWidgetClass, selBox, NULL);

    topLabel = XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=MKSTRING(
    	       "Enter desired size in rows\nand columns of characters:"), NULL);
    XmStringFree(s1);
 
    RowText = XtVaCreateManagedWidget("rows", xmTextWidgetClass, form,
    	    XmNcolumns, 3,
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNtopWidget, topLabel,
    	    XmNleftPosition, 5,
    	    XmNrightPosition, 45, NULL);
    RemapDeleteKey(RowText);
 
    XtVaCreateManagedWidget("xLabel", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=MKSTRING("x"),
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
    	    XmNtopWidget, topLabel,
    	    XmNbottomWidget, RowText,
    	    XmNleftPosition, 45,
    	    XmNrightPosition, 55, NULL);
    XmStringFree(s1);

    ColText = XtVaCreateManagedWidget("cols", xmTextWidgetClass, form,
    	    XmNcolumns, 3,
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNtopWidget, topLabel,
    	    XmNleftPosition, 55,
    	    XmNrightPosition, 95, NULL);
    RemapDeleteKey(ColText);
    
    // set default values
    char strbuf[20];
    snprintf(strbuf, 20, "%d", currentCols);
    XmTextSetString(ColText, strbuf);
    snprintf(strbuf, 20, "%d", currentRows);
    XmTextSetString(RowText, strbuf);

    /* put up dialog and wait for user to press ok or cancel */
    DoneWithSizeDialog = False;
    ManageDialogCenteredOnPointer(selBox);
    while (!DoneWithSizeDialog)
    {
       	XEvent event;
       	XtAppNextEvent(XtWidgetToApplicationContext(parent), &event);
       	ServerDispatchEvent(&event);
    }
    
    XtDestroyWidget(selBox);
}

static void sizeOKCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int rowValue, colValue, stat;
    
    /* get the values that the user entered and make sure they're ok */
    stat = GetIntTextWarn(RowText, &rowValue, "number of rows", True);
    if (stat != TEXT_READ_OK)
    	return;
    stat = GetIntTextWarn(ColText, &colValue, "number of columns", True);
    if (stat != TEXT_READ_OK)
    	return;
    
    /* set the corresponding preferences and dismiss the dialog */
    SetPrefRows(rowValue);
    SetPrefCols(colValue);
    DoneWithSizeDialog = True;
}

static void sizeCancelCB(Widget w, XtPointer clientData, XtPointer callData)
{
    DoneWithSizeDialog = True;
}

/*
** Present the user a dialog for setting tab related preferences, either as
** defaults, or for a specific window (pass "forWindow" as NULL to set default
** preference, or a window to set preferences for the specific window.
*/
void TabsPrefDialog(Widget parent, WindowInfo *forWindow)
{
    Widget form, selBox;
    Arg selBoxArgs[2];
    XmString s1;
    int emulate, emTabDist, useTabs, tabDist;

    XtSetArg(selBoxArgs[0], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
    XtSetArg(selBoxArgs[1], XmNautoUnmanage, False);
    selBox = CreatePromptDialog(parent, "customSize", selBoxArgs, 2);
    XtAddCallback(selBox, XmNokCallback, (XtCallbackProc)tabsOKCB, NULL);
    XtAddCallback(selBox, XmNcancelCallback, (XtCallbackProc)tabsCancelCB,NULL);
    XtAddCallback(selBox, XmNhelpCallback, (XtCallbackProc)tabsHelpCB,NULL);
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_SELECTION_LABEL));
    XtVaSetValues(XtParent(selBox), XmNtitle, "Tabs", NULL);
    
    form = XtVaCreateManagedWidget("form", xmFormWidgetClass, selBox, NULL);

    TabDistText = XtVaCreateManagedWidget("tabDistText", xmTextWidgetClass,
    	    form, XmNcolumns, 7,
    	    XmNtopAttachment, XmATTACH_FORM,
    	    XmNrightAttachment, XmATTACH_FORM, NULL);
    RemapDeleteKey(TabDistText);
    XtVaCreateManagedWidget("tabDistLabel", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Tab spacing (for hardware tab characters)"),
	    XmNmnemonic, 'T',
    	    XmNuserData, TabDistText,
    	    XmNtopAttachment, XmATTACH_FORM,
    	    XmNleftAttachment, XmATTACH_FORM,
    	    XmNrightAttachment, XmATTACH_WIDGET,
    	    XmNrightWidget, TabDistText,
	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNbottomWidget, TabDistText, NULL);
    XmStringFree(s1);
 
    EmTabText = XtVaCreateManagedWidget("emTabText", xmTextWidgetClass, form,
    	    XmNcolumns, 7,
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNtopWidget, TabDistText,
    	    XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
    	    XmNrightWidget, TabDistText, NULL);
    RemapDeleteKey(EmTabText);
    EmTabLabel = XtVaCreateManagedWidget("emTabLabel", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Emulated tab spacing"),
	    XmNmnemonic, 's',
    	    XmNuserData, EmTabText,
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNtopWidget, TabDistText,
    	    XmNrightAttachment, XmATTACH_WIDGET,
    	    XmNrightWidget, EmTabText,
    	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNbottomWidget, EmTabText, NULL);
    XmStringFree(s1);
    EmTabToggle = XtVaCreateManagedWidget("emTabToggle",
    	    xmToggleButtonWidgetClass, form, XmNlabelString,
    	    	s1=XmStringCreateSimple("Emulate tabs"),
	    XmNmnemonic, 'E',
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNtopWidget, TabDistText,
    	    XmNleftAttachment, XmATTACH_FORM,
    	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNbottomWidget, EmTabText, NULL);
    XmStringFree(s1);
    XtAddCallback(EmTabToggle, XmNvalueChangedCallback, emTabsCB, NULL);
    UseTabsToggle = XtVaCreateManagedWidget("useTabsToggle",
    	    xmToggleButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Use tab characters in padding and emulated tabs"),
	    XmNmnemonic, 'U',
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNtopWidget, EmTabText,
    	    XmNtopOffset, 5,
    	    XmNleftAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);

    /* Set default values */
    if (forWindow == NULL) {
    	emTabDist = GetPrefEmTabDist(PLAIN_LANGUAGE_MODE);
    	useTabs = GetPrefInsertTabs();
    	tabDist = GetPrefTabDist(PLAIN_LANGUAGE_MODE);
    } else {
    	XtVaGetValues(forWindow->textArea, textNemulateTabs, &emTabDist, NULL);
    	useTabs = forWindow->buffer->useTabs;
    	tabDist = BufGetTabDistance(forWindow->buffer);
    }
    emulate = emTabDist != 0;
    SetIntText(TabDistText, tabDist);
    XmToggleButtonSetState(EmTabToggle, emulate, True);
    if (emulate)
    	SetIntText(EmTabText, emTabDist);
    XmToggleButtonSetState(UseTabsToggle, useTabs, False);
    XtSetSensitive(EmTabText, emulate);
    XtSetSensitive(EmTabLabel, emulate);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);

    /* Set the widget to get focus */
#if XmVersion >= 1002
    XtVaSetValues(form, XmNinitialFocus, TabDistText, NULL);
#endif
    
    /* put up dialog and wait for user to press ok or cancel */
    TabsDialogForWindow = forWindow;
    DoneWithTabsDialog = False;
    ManageDialogCenteredOnPointer(selBox);
    while (!DoneWithTabsDialog)
    {
       	XEvent event;
       	XtAppNextEvent(XtWidgetToApplicationContext(parent), &event);
       	ServerDispatchEvent(&event);
    }
    
    XtDestroyWidget(selBox);
}

static void tabsOKCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int emulate, useTabs, stat, tabDist, emTabDist;
    WindowInfo *window = TabsDialogForWindow;
    
    /* get the values that the user entered and make sure they're ok */
    emulate = XmToggleButtonGetState(EmTabToggle);
    useTabs = XmToggleButtonGetState(UseTabsToggle);
    stat = GetIntTextWarn(TabDistText, &tabDist, "tab spacing", True);
    if (stat != TEXT_READ_OK)
    	return;

    if (tabDist <= 0 || tabDist > MAX_EXP_CHAR_LEN)
    {
        DialogF(DF_WARN, TabDistText, 1, "Tab Spacing",
                "Tab spacing out of range", "OK");
        return;
    }

    if (emulate) {
	stat = GetIntTextWarn(EmTabText, &emTabDist, "emulated tab spacing",True);
	if (stat != TEXT_READ_OK)
	    return;

        if (emTabDist <= 0 || emTabDist >= 1000)
        {
            DialogF(DF_WARN, EmTabText, 1, "Tab Spacing",
                    "Emulated tab spacing out of range", "OK");
            return;
        }
    } else
    	emTabDist = 0;
    
#ifdef SGI_CUSTOM
    /* Ask the user about saving as a default preference */
    if (TabsDialogForWindow != NULL) {
	int setDefault;
	if (!shortPrefToDefault(window->shell, "Tab Settings", &setDefault)) {
	    DoneWithTabsDialog = True;
    	    return;
	}
	if (setDefault) {
    	    SetPrefTabDist(tabDist);
    	    SetPrefEmTabDist(emTabDist);
    	    SetPrefInsertTabs(useTabs);
	    SaveNEditPrefs(window->shell, GetPrefShortMenus());
	}
    }
#endif

    /* Set the value in either the requested window or default preferences */
    if (TabsDialogForWindow == NULL) {
    	SetPrefTabDist(tabDist);
    	SetPrefEmTabDist(emTabDist);
    	SetPrefInsertTabs(useTabs);
    } else {
        char *params[1];
        char numStr[25];

        params[0] = numStr;
        sprintf(numStr, "%d", tabDist);
        XtCallActionProc(window->textArea, "set_tab_dist", NULL, params, 1);
        params[0] = numStr;
        sprintf(numStr, "%d", emTabDist);
        XtCallActionProc(window->textArea, "set_em_tab_dist", NULL, params, 1);
        params[0] = numStr;
        sprintf(numStr, "%d", useTabs);
        XtCallActionProc(window->textArea, "set_use_tabs", NULL, params, 1);
/*
    	setTabDist(window, tabDist);
    	setEmTabDist(window, emTabDist);
       	window->buffer->useTabs = useTabs;
*/
    }
    DoneWithTabsDialog = True;
}

static void tabsCancelCB(Widget w, XtPointer clientData, XtPointer callData)
{
    DoneWithTabsDialog = True;
}

static void tabsHelpCB(Widget w, XtPointer clientData, XtPointer callData)
{
    Help(HELP_TABS_DIALOG);
}

static void emTabsCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int state = XmToggleButtonGetState(w);
    
    XtSetSensitive(EmTabLabel, state);
    XtSetSensitive(EmTabText, state);
}

/*
** Present the user a dialog for setting wrap margin.
*/
void WrapMarginDialog(Widget parent, WindowInfo *forWindow)
{
    Widget form, selBox;
    Arg selBoxArgs[2];
    XmString s1;
    int margin;

    XtSetArg(selBoxArgs[0], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
    XtSetArg(selBoxArgs[1], XmNautoUnmanage, False);
    selBox = CreatePromptDialog(parent, "wrapMargin", selBoxArgs, 2);
    XtAddCallback(selBox, XmNokCallback, (XtCallbackProc)wrapOKCB, NULL);
    XtAddCallback(selBox, XmNcancelCallback, (XtCallbackProc)wrapCancelCB,NULL);
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_SELECTION_LABEL));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_HELP_BUTTON));
    XtVaSetValues(XtParent(selBox), XmNtitle, "Wrap Margin", NULL);
    
    form = XtVaCreateManagedWidget("form", xmFormWidgetClass, selBox, NULL);

    WrapWindowToggle = XtVaCreateManagedWidget("wrapWindowToggle",
    	    xmToggleButtonWidgetClass, form, XmNlabelString,
    	    	s1=XmStringCreateSimple("Wrap and Fill at width of window"),
	    XmNmnemonic, 'W',
    	    XmNtopAttachment, XmATTACH_FORM,
    	    XmNleftAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);
    XtAddCallback(WrapWindowToggle, XmNvalueChangedCallback, wrapWindowCB,NULL);
    WrapText = XtVaCreateManagedWidget("wrapText", xmTextWidgetClass, form,
    	    XmNcolumns, 5,
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNtopWidget, WrapWindowToggle,
    	    XmNrightAttachment, XmATTACH_FORM, NULL);
    RemapDeleteKey(WrapText);
    WrapTextLabel = XtVaCreateManagedWidget("wrapMarginLabel",
    	    xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Margin for Wrap and Fill"),
	    XmNmnemonic, 'M',
    	    XmNuserData, WrapText,
    	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNtopWidget, WrapWindowToggle,
    	    XmNleftAttachment, XmATTACH_FORM,
    	    XmNrightAttachment, XmATTACH_WIDGET,
    	    XmNrightWidget, WrapText,
	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNbottomWidget, WrapText, NULL);
    XmStringFree(s1);

    /* Set default value */
    if (forWindow == NULL)
    	margin = GetPrefWrapMargin();
    else
    	XtVaGetValues(forWindow->textArea, textNwrapMargin, &margin, NULL);
    XmToggleButtonSetState(WrapWindowToggle, margin==0, True);
    if (margin != 0)
    	SetIntText(WrapText, margin);
    XtSetSensitive(WrapText, margin!=0);
    XtSetSensitive(WrapTextLabel, margin!=0);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);

    /* put up dialog and wait for user to press ok or cancel */
    WrapDialogForWindow = forWindow;
    DoneWithWrapDialog = False;
    ManageDialogCenteredOnPointer(selBox);
    while (!DoneWithWrapDialog)
    {
       	XEvent event;
       	XtAppNextEvent(XtWidgetToApplicationContext(parent), &event);
       	ServerDispatchEvent(&event);
    }
    
    XtDestroyWidget(selBox);
}

static void wrapOKCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int wrapAtWindow, margin, stat;
    WindowInfo *window = WrapDialogForWindow;
    
    /* get the values that the user entered and make sure they're ok */
    wrapAtWindow = XmToggleButtonGetState(WrapWindowToggle);
    if (wrapAtWindow)
    	margin = 0;
    else {
	stat = GetIntTextWarn(WrapText, &margin, "wrap Margin", True);
	if (stat != TEXT_READ_OK)
    	    return;

       if (margin <= 0 || margin >= 1000)
       {
           DialogF(DF_WARN, WrapText, 1, "Wrap Margin", 
                   "Wrap margin out of range", "OK");
           return;
       }

    }

#ifdef SGI_CUSTOM
    /* Ask the user about saving as a default preference */
    if (WrapDialogForWindow != NULL) {
	int setDefault;
	if (!shortPrefToDefault(window->shell, "Wrap Margin Settings",
	    	&setDefault)) {
	    DoneWithWrapDialog = True;
    	    return;
	}
	if (setDefault) {
    	    SetPrefWrapMargin(margin);
	    SaveNEditPrefs(window->shell, GetPrefShortMenus());
	}
    }
#endif

    /* Set the value in either the requested window or default preferences */
    if (WrapDialogForWindow == NULL)
    	SetPrefWrapMargin(margin);
    else {
        char *params[1];
        char marginStr[25];
        sprintf(marginStr, "%d", margin);
        params[0] = marginStr;
        XtCallActionProc(window->textArea, "set_wrap_margin", NULL, params, 1);
    }
    DoneWithWrapDialog = True;
}

static void wrapCancelCB(Widget w, XtPointer clientData, XtPointer callData)
{
    DoneWithWrapDialog = True;
}

static void wrapWindowCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int wrapAtWindow = XmToggleButtonGetState(w);
    
    XtSetSensitive(WrapTextLabel, !wrapAtWindow);
    XtSetSensitive(WrapText, !wrapAtWindow);
}

/*
**  Create and show a dialog for selecting the shell
*/
void SelectShellDialog(Widget parent, WindowInfo* forWindow)
{
    Widget shellSelDialog;
    Arg shellSelDialogArgs[2];
    XmString label;

    /*  Set up the dialog.  */
    XtSetArg(shellSelDialogArgs[0],
            XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
    XtSetArg(shellSelDialogArgs[1], XmNautoUnmanage, False);
    shellSelDialog = CreatePromptDialog(parent, "shellSelDialog",
            shellSelDialogArgs, 2);

    /*  Fix dialog to our liking.  */
    XtVaSetValues(XtParent(shellSelDialog), XmNtitle, "Command Shell", NULL);
    XtAddCallback(shellSelDialog, XmNokCallback, (XtCallbackProc) shellSelOKCB,
            shellSelDialog);
    XtAddCallback(shellSelDialog, XmNcancelCallback,
            (XtCallbackProc) shellSelCancelCB, NULL);
    XtUnmanageChild(XmSelectionBoxGetChild(shellSelDialog, XmDIALOG_HELP_BUTTON));
    label = XmStringCreateLocalized("Enter shell path:");
    XtVaSetValues(shellSelDialog, XmNselectionLabelString, label, NULL);
    XmStringFree(label);

    /*  Set dialog's text to the current setting.  */
    XmTextSetString(XmSelectionBoxGetChild(shellSelDialog, XmDIALOG_TEXT),
            (char*) GetPrefShell());

    DoneWithShellSelDialog = False;

    /*  Show dialog and wait until the user made her choice.  */
    ManageDialogCenteredOnPointer(shellSelDialog);
    while (!DoneWithShellSelDialog) {
        XEvent event;
        XtAppNextEvent(XtWidgetToApplicationContext(parent), &event);
        ServerDispatchEvent(&event);
    }

    XtDestroyWidget(shellSelDialog);
}

static void shellSelOKCB(Widget widget, XtPointer clientData,
        XtPointer callData)
{
    Widget shellSelDialog = (Widget) clientData;
    String shellName = (char*)NEditMalloc(MAXPATHLEN);
    struct stat attribute;
    unsigned dlgResult;

    /*  Leave with a warning if the dialog is not up.  */
    if (!XtIsRealized(shellSelDialog)) {
        fprintf(stderr, "xnedit: Callback shellSelOKCB() illegally called.\n");
        return;
    }

    /*  Get the string that the user entered and make sure it's ok.  */
    shellName = XmTextGetString(XmSelectionBoxGetChild(shellSelDialog,
            XmDIALOG_TEXT));

    if (-1 == stat(shellName, &attribute)) {
        dlgResult = DialogF(DF_WARN, shellSelDialog, 2, "Command Shell",
                "The selected shell is not available.\nDo you want to use it anyway?",
                "OK", "Cancel");
        if (1 != dlgResult) {
            return;
        }
    }

    SetPrefShell(shellName);
    NEditFree(shellName);

    DoneWithShellSelDialog = True;
}

static void shellSelCancelCB(Widget widgget, XtPointer clientData,
        XtPointer callData)
{
    DoneWithShellSelDialog = True;
}

/*
** Present a dialog for editing language mode information
*/
void EditLanguageModes(void)
{
#define LIST_RIGHT 40
#define LEFT_MARGIN_POS 1
#define RIGHT_MARGIN_POS 99
#define H_MARGIN 5
    Widget form, nameLbl, topLbl, extLbl, recogLbl, delimitLbl, defTipsLbl;
    Widget okBtn, applyBtn, closeBtn;
    Widget overrideFrame, overrideForm, delimitForm;
    Widget tabForm, tabLbl, indentBox, wrapBox;
    XmString s1;
    int i, ac;
    Arg args[20];

    /* if the dialog is already displayed, just pop it to the top and return */
    if (LMDialog.shell != NULL) {
    	RaiseDialogWindow(LMDialog.shell);
    	return;
    }
    
    LMDialog.languageModeList = (languageModeRec **)NEditMalloc(
    	    sizeof(languageModeRec *) * MAX_LANGUAGE_MODES);
    for (i=0; i<NLanguageModes; i++)
    	LMDialog.languageModeList[i] = copyLanguageModeRec(LanguageModes[i]);
    LMDialog.nLanguageModes = NLanguageModes;

    /* Create a form widget in an application shell */
    ac = 0;
    XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
    XtSetArg(args[ac], XmNiconName, "XNEdit Language Modes"); ac++;
    XtSetArg(args[ac], XmNtitle, "Language Modes"); ac++;
    LMDialog.shell = CreateWidget(TheAppShell, "langModes",
	    topLevelShellWidgetClass, args, ac);
    AddSmallIcon(LMDialog.shell);
    form = XtVaCreateManagedWidget("editLanguageModes", xmFormWidgetClass,
	    LMDialog.shell, XmNautoUnmanage, False,
	    XmNresizePolicy, XmRESIZE_NONE, NULL);
    XtAddCallback(form, XmNdestroyCallback, lmDestroyCB, NULL);
    AddMotifCloseCallback(LMDialog.shell, lmCloseCB, NULL);
    
    topLbl = XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=MKSTRING(
"To modify the properties of an existing language mode, select the name from\n\
the list on the left.  To add a new language, select \"New\" from the list."),
	    XmNmnemonic, 'N',
	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 2,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LEFT_MARGIN_POS,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS, NULL);
    XmStringFree(s1);
    
    nameLbl = XtVaCreateManagedWidget("nameLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Name"),
	    XmNmnemonic, 'm',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, H_MARGIN,
	    XmNtopWidget, topLbl, NULL);
    XmStringFree(s1);
 
    LMDialog.nameW = XtVaCreateManagedWidget("name", xmTextWidgetClass, form,
    	    XmNcolumns, 15,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, nameLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, (RIGHT_MARGIN_POS + LIST_RIGHT)/2, NULL);
    RemapDeleteKey(LMDialog.nameW);
    XtVaSetValues(nameLbl, XmNuserData, LMDialog.nameW, NULL);
    
    extLbl = XtVaCreateManagedWidget("extLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, 	    	
    	    	s1=XmStringCreateSimple("File extensions (separate w/ space)"),
    	    XmNmnemonic, 'F',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, H_MARGIN,
	    XmNtopWidget, LMDialog.nameW, NULL);
    XmStringFree(s1);
 
    LMDialog.extW = XtVaCreateManagedWidget("ext", xmTextWidgetClass, form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, extLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS, NULL);
    RemapDeleteKey(LMDialog.extW);
    XtVaSetValues(extLbl, XmNuserData, LMDialog.extW, NULL);
    
    recogLbl = XtVaCreateManagedWidget("recogLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=MKSTRING(
"Recognition regular expression (applied to first 200\n\
characters of file to determine type from content)"),
    	    XmNalignment, XmALIGNMENT_BEGINNING,
    	    XmNmnemonic, 'R',
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, H_MARGIN,
	    XmNtopWidget, LMDialog.extW, NULL);
    XmStringFree(s1);
 
    LMDialog.recogW = XtVaCreateManagedWidget("recog", xmTextWidgetClass, form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, recogLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS, NULL);
    RemapDeleteKey(LMDialog.recogW);
    XtVaSetValues(recogLbl, XmNuserData, LMDialog.recogW, NULL);
	    
    defTipsLbl = XtVaCreateManagedWidget("defTipsLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=MKSTRING(
"Default calltips file(s) (separate w/colons)"),
    	    XmNalignment, XmALIGNMENT_BEGINNING,
    	    XmNmnemonic, 'c',
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, H_MARGIN,
	    XmNtopWidget, LMDialog.recogW, NULL);
    XmStringFree(s1);
 
    LMDialog.defTipsW = XtVaCreateManagedWidget("defTips", xmTextWidgetClass, 
            form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, defTipsLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS, NULL);
    RemapDeleteKey(LMDialog.defTipsW);
    XtVaSetValues(defTipsLbl, XmNuserData, LMDialog.defTipsW, NULL);
	    
    okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 10,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 30,
    	    XmNbottomAttachment, XmATTACH_POSITION,
    	    XmNbottomPosition, 99, NULL);
    XtAddCallback(okBtn, XmNactivateCallback, lmOkCB, NULL);
    XmStringFree(s1);

    applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Apply"),
    	    XmNmnemonic, 'A',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 40,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 60,
    	    XmNbottomAttachment, XmATTACH_POSITION,
    	    XmNbottomPosition, 99, NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, lmApplyCB, NULL);
    XmStringFree(s1);

    closeBtn = XtVaCreateManagedWidget("close",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("Close"),
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 70,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 90,
    	    XmNbottomAttachment, XmATTACH_POSITION,
    	    XmNbottomPosition, 99, NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, lmCloseCB, NULL);
    XmStringFree(s1);

    overrideFrame = XtVaCreateManagedWidget("overrideFrame",
    	    xmFrameWidgetClass, form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LEFT_MARGIN_POS,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS,
	    XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, closeBtn,
	    XmNbottomOffset, H_MARGIN, NULL);
    overrideForm = XtVaCreateManagedWidget("overrideForm", xmFormWidgetClass,
	    overrideFrame, NULL);
    XtVaCreateManagedWidget("overrideLbl", xmLabelGadgetClass, overrideFrame,
    	    XmNlabelString, s1=XmStringCreateSimple("Override Defaults"),
	    XmNchildType, XmFRAME_TITLE_CHILD,
	    XmNchildHorizontalAlignment, XmALIGNMENT_CENTER, NULL);
    XmStringFree(s1);
 
    delimitForm = XtVaCreateManagedWidget("delimitForm", xmFormWidgetClass,
	    overrideForm,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LEFT_MARGIN_POS,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNtopOffset, H_MARGIN,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS, NULL);
    delimitLbl = XtVaCreateManagedWidget("delimitLbl", xmLabelGadgetClass,
    	    delimitForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Word delimiters"),
    	    XmNmnemonic, 'W',
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);
    LMDialog.delimitW = XtVaCreateManagedWidget("delimit", xmTextWidgetClass,
    	    delimitForm,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, delimitLbl,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM, NULL);
    RemapDeleteKey(LMDialog.delimitW);
    XtVaSetValues(delimitLbl, XmNuserData, LMDialog.delimitW, NULL);

    tabForm = XtVaCreateManagedWidget("tabForm", xmFormWidgetClass,
	    overrideForm,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LEFT_MARGIN_POS,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, delimitForm,
	    XmNtopOffset, H_MARGIN,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS, NULL);
    tabLbl = XtVaCreateManagedWidget("tabLbl", xmLabelGadgetClass, tabForm,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Alternative hardware tab spacing"),
    	    XmNmnemonic, 't',
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);
    LMDialog.tabW = XtVaCreateManagedWidget("delimit", xmTextWidgetClass,
    	    tabForm,
    	    XmNcolumns, 3,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, tabLbl,
	    XmNbottomAttachment, XmATTACH_FORM, NULL);
    RemapDeleteKey(LMDialog.tabW);
    XtVaSetValues(tabLbl, XmNuserData, LMDialog.tabW, NULL);
    LMDialog.emTabW = XtVaCreateManagedWidget("delimit", xmTextWidgetClass,
    	    tabForm,
    	    XmNcolumns, 3,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM, NULL);
    RemapDeleteKey(LMDialog.emTabW);
    XtVaCreateManagedWidget("emTabLbl", xmLabelGadgetClass, tabForm,
    	    XmNlabelString,
    	    s1=XmStringCreateSimple("Alternative emulated tab spacing"),
    	    XmNalignment, XmALIGNMENT_END, 
    	    XmNmnemonic, 'e',
	    XmNuserData, LMDialog.emTabW,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, LMDialog.tabW,
	    XmNrightAttachment, XmATTACH_WIDGET,
	    XmNrightWidget, LMDialog.emTabW,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);

    indentBox = XtVaCreateManagedWidget("indentBox", xmRowColumnWidgetClass,
    	    overrideForm,
    	    XmNorientation, XmHORIZONTAL,
    	    XmNpacking, XmPACK_TIGHT,
    	    XmNradioBehavior, True,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LEFT_MARGIN_POS,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, tabForm,
	    XmNtopOffset, H_MARGIN, NULL);
    LMDialog.defaultIndentW = XtVaCreateManagedWidget("defaultIndent", 
    	    xmToggleButtonWidgetClass, indentBox,
    	    XmNset, True,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("Default indent style"),
    	    XmNmnemonic, 'D', NULL);
    XmStringFree(s1);
    LMDialog.noIndentW = XtVaCreateManagedWidget("noIndent", 
    	    xmToggleButtonWidgetClass, indentBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("No automatic indent"),
    	    XmNmnemonic, 'N', NULL);
    XmStringFree(s1);
    LMDialog.autoIndentW = XtVaCreateManagedWidget("autoIndent", 
    	    xmToggleButtonWidgetClass, indentBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("Auto-indent"),
    	    XmNmnemonic, 'A', NULL);
    XmStringFree(s1);
    LMDialog.smartIndentW = XtVaCreateManagedWidget("smartIndent", 
    	    xmToggleButtonWidgetClass, indentBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("Smart-indent"),
    	    XmNmnemonic, 'S', NULL);
    XmStringFree(s1);

    wrapBox = XtVaCreateManagedWidget("wrapBox", xmRowColumnWidgetClass,
    	    overrideForm,
    	    XmNorientation, XmHORIZONTAL,
    	    XmNpacking, XmPACK_TIGHT,
    	    XmNradioBehavior, True,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LEFT_MARGIN_POS,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, indentBox,
	    XmNtopOffset, H_MARGIN,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNbottomOffset, H_MARGIN, NULL);
    LMDialog.defaultWrapW = XtVaCreateManagedWidget("defaultWrap", 
    	    xmToggleButtonWidgetClass, wrapBox,
    	    XmNset, True,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("Default wrap style"),
    	    XmNmnemonic, 'D', NULL);
    XmStringFree(s1);
    LMDialog.noWrapW = XtVaCreateManagedWidget("noWrap", 
    	    xmToggleButtonWidgetClass, wrapBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("No wrapping"),
    	    XmNmnemonic, 'N', NULL);
    XmStringFree(s1);
    LMDialog.newlineWrapW = XtVaCreateManagedWidget("newlineWrap", 
    	    xmToggleButtonWidgetClass, wrapBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("Auto newline wrap"),
    	    XmNmnemonic, 'A', NULL);
    XmStringFree(s1);
    LMDialog.contWrapW = XtVaCreateManagedWidget("contWrap", 
    	    xmToggleButtonWidgetClass, wrapBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple("Continuous wrap"),
    	    XmNmnemonic, 'C', NULL);
    XmStringFree(s1);

    XtVaCreateManagedWidget("stretchForm", xmFormWidgetClass, form,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, LMDialog.defTipsW,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LIST_RIGHT,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, RIGHT_MARGIN_POS,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, overrideFrame,
	    XmNbottomOffset, H_MARGIN*2, NULL);
    
    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopOffset, H_MARGIN); ac++;
    XtSetArg(args[ac], XmNtopWidget, topLbl); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNleftPosition, LEFT_MARGIN_POS); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNrightPosition, LIST_RIGHT-1); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNbottomWidget, overrideFrame); ac++;
    XtSetArg(args[ac], XmNbottomOffset, H_MARGIN*2); ac++;
    LMDialog.managedListW = CreateManagedList(form, "list", args, ac,
    	    (void **)LMDialog.languageModeList, &LMDialog.nLanguageModes,
    	    MAX_LANGUAGE_MODES, 15, lmGetDisplayedCB, NULL, lmSetDisplayedCB,
    	    NULL, lmFreeItemCB);
    AddDeleteConfirmCB(LMDialog.managedListW, lmDeleteConfirmCB, NULL);
    XtVaSetValues(topLbl, XmNuserData, LMDialog.managedListW, NULL);
    	
    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);

    /* Realize all of the widgets in the new dialog */
    RealizeWithoutForcingPosition(LMDialog.shell);
}

static void lmDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int i;
    
    for (i=0; i<LMDialog.nLanguageModes; i++)
    	freeLanguageModeRec(LMDialog.languageModeList[i]);
    NEditFree(LMDialog.languageModeList);
}

static void lmOkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (!updateLMList())
    	return;

    /* pop down and destroy the dialog */
    XtDestroyWidget(LMDialog.shell);
    LMDialog.shell = NULL;
}

static void lmApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    updateLMList();
}

static void lmCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* pop down and destroy the dialog */
    XtDestroyWidget(LMDialog.shell);
    LMDialog.shell = NULL;
}

static int lmDeleteConfirmCB(int itemIndex, void *cbArg)
{
    int i;
    
    /* Allow duplicate names to be deleted regardless of dependencies */
    for (i=0; i<LMDialog.nLanguageModes; i++)
	if (i != itemIndex && !strcmp(LMDialog.languageModeList[i]->name,
		LMDialog.languageModeList[itemIndex]->name))
	    return True;
    
    /* don't allow deletion if data will be lost */
    if (LMHasHighlightPatterns(LMDialog.languageModeList[itemIndex]->name))
    {
        DialogF(DF_WARN, LMDialog.shell, 1, "Patterns exist",
                "This language mode has syntax highlighting\n"
                "patterns defined.  Please delete the patterns\n"
                "first, in Preferences -> Default Settings ->\n"
                "Syntax Highlighting, before proceeding here.", "OK");
        return False;
    }

    /* don't allow deletion if data will be lost */
    if (LMHasSmartIndentMacros(LMDialog.languageModeList[itemIndex]->name))
    {
        DialogF(DF_WARN, LMDialog.shell, 1, "Smart Indent Macros exist",
                "This language mode has smart indent macros\n"
                "defined.  Please delete the macros first,\n"
                "in Preferences -> Default Settings ->\n"
                "Auto Indent -> Program Smart Indent,\n"
                "before proceeding here.", "OK");
        return False;
    }

    return True;
}

/*
** Apply the changes that the user has made in the language modes dialog to the
** stored language mode information for this NEdit session (the data array
** LanguageModes)
*/
static int updateLMList(void)
{
    WindowInfo *window;
    int oldLanguageMode;
    char *oldModeName, *newDelimiters;
    int i, j;
    
    /* Get the current contents of the dialog fields */
    if (!UpdateManagedList(LMDialog.managedListW, True))
    	return False;

    /* Fix up language mode indices in all open windows (which may change
       if the currently selected mode is deleted or has changed position),
       and update word delimiters */
    for (window=WindowList; window!=NULL; window=window->next) {
	if (window->languageMode != PLAIN_LANGUAGE_MODE) {
            oldLanguageMode = window->languageMode;
    	    oldModeName = LanguageModes[window->languageMode]->name;
    	    window->languageMode = PLAIN_LANGUAGE_MODE;
    	    for (i=0; i<LMDialog.nLanguageModes; i++) {
    		if (!strcmp(oldModeName, LMDialog.languageModeList[i]->name)) {
    	    	    newDelimiters = LMDialog.languageModeList[i]->delimiters;
    	    	    if (newDelimiters == NULL)
    	    	    	newDelimiters = GetPrefDelimiters();
    	    	    XtVaSetValues(window->textArea, textNwordDelimiters,
    	    	    	    newDelimiters, NULL);
    	    	    for (j=0; j<window->nPanes; j++)
    	    	    	XtVaSetValues(window->textPanes[j],
    	    	    	    	textNwordDelimiters, newDelimiters, NULL);
                    /* don't forget to adapt the LM stored within the user menu cache */
                    if (window->userMenuCache->umcLanguageMode == oldLanguageMode)
                        window->userMenuCache->umcLanguageMode = i;
                    if (window->userBGMenuCache.ubmcLanguageMode == oldLanguageMode)
                        window->userBGMenuCache.ubmcLanguageMode = i;
                    /* update the language mode of this window (document) */
    	    	    window->languageMode = i;
    	    	    break;
    		}
    	    }
	}
    }
    
    /* If there were any name changes, re-name dependent highlight patterns
       and smart-indent macros and fix up the weird rename-format names */
    for (i=0; i<LMDialog.nLanguageModes; i++) {
    	if (strchr(LMDialog.languageModeList[i]->name, ':') != NULL) {
    	    char *newName = strrchr(LMDialog.languageModeList[i]->name, ':')+1;
    	    *strchr(LMDialog.languageModeList[i]->name, ':') = '\0';
    	    RenameHighlightPattern(LMDialog.languageModeList[i]->name, newName);
    	    RenameSmartIndentMacros(LMDialog.languageModeList[i]->name, newName);
    	    memmove(LMDialog.languageModeList[i]->name, newName,
    	    	    strlen(newName) + 1);
    	    ChangeManagedListData(LMDialog.managedListW);
    	}
    }
    
    /* Replace the old language mode list with the new one from the dialog */
    for (i=0; i<NLanguageModes; i++)
    	freeLanguageModeRec(LanguageModes[i]);
    for (i=0; i<LMDialog.nLanguageModes; i++)
    	LanguageModes[i] = copyLanguageModeRec(LMDialog.languageModeList[i]);
    NLanguageModes = LMDialog.nLanguageModes;
    
    /* Update user menu info to update language mode dependencies of
       user menu items */
    UpdateUserMenuInfo();

    /* Update the menus in the window menu bars and load any needed
        calltips files */
    for (window=WindowList; window!=NULL; window=window->next) {
    	updateLanguageModeSubmenu(window);
        if (window->languageMode != PLAIN_LANGUAGE_MODE &&
                LanguageModes[window->languageMode]->defTipsFile != NULL)
            AddTagsFile(LanguageModes[window->languageMode]->defTipsFile, TIP);
        /* cache user menus: Rebuild all user menus of this window */
        RebuildAllMenus(window);
    }
    
    /* If a syntax highlighting dialog is up, update its menu */
    UpdateLanguageModeMenu();
    /* The same for the smart indent macro dialog */
    UpdateLangModeMenuSmartIndent(); 
    /* Note that preferences have been changed */
    MarkPrefsChanged();

    return True;
}

static void *lmGetDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg)
{
    languageModeRec *lm, *oldLM = (languageModeRec *)oldItem;
    char *tempName;
    int i, nCopies, oldLen;
    
    /* If the dialog is currently displaying the "new" entry and the
       fields are empty, that's just fine */
    if (oldItem == NULL && lmDialogEmpty())
    	return NULL;
    
    /* Read the data the user has entered in the dialog fields */
    lm = readLMDialogFields(True);

    /* If there was a name change of a non-duplicate language mode, modify the
       name to the weird format of: ":old name:new name".  This signals that a
       name change is necessary in lm dependent data such as highlight
       patterns.  Duplicate language modes may be re-named at will, since no
       data will be lost due to the name change. */
    if (lm != NULL && oldLM != NULL && strcmp(oldLM->name, lm->name)) {
    	nCopies = 0;
	for (i=0; i<LMDialog.nLanguageModes; i++)
	    if (!strcmp(oldLM->name, LMDialog.languageModeList[i]->name))
		nCopies++;
	if (nCopies <= 1) {
    	    oldLen = strchr(oldLM->name, ':') == NULL ? strlen(oldLM->name) :
    	    	    strchr(oldLM->name, ':') - oldLM->name;
    	    tempName = (char*)NEditMalloc(oldLen + strlen(lm->name) + 2);
    	    strncpy(tempName, oldLM->name, oldLen);
    	    sprintf(&tempName[oldLen], ":%s", lm->name);
    	    NEditFree(lm->name);
    	    lm->name = tempName;
	}
    }
    
    /* If there are no problems reading the data, just return it */
    if (lm != NULL)
    	return (void *)lm;
    
    /* If there are problems, and the user didn't ask for the fields to be
       read, give more warning */
    if (!explicitRequest)
    {
        if (DialogF(DF_WARN, LMDialog.shell, 2, "Discard Language Mode",
                "Discard incomplete entry\nfor current language mode?", "Keep",
                "Discard") == 2)
        {
            return oldItem == NULL
                    ? NULL
                    : (void *)copyLanguageModeRec((languageModeRec *)oldItem);
        }
    }

    /* Do readLMDialogFields again without "silent" mode to display warning */
    lm = readLMDialogFields(False);
    *abort = True;
    return NULL;
}

static void lmSetDisplayedCB(void *item, void *cbArg)
{
    languageModeRec *lm = (languageModeRec *)item;
    char *extStr;

    if (item == NULL) {
    	XmTextSetString(LMDialog.nameW, "");
    	XmTextSetString(LMDialog.extW, "");
    	XmTextSetString(LMDialog.recogW, "");
        XmTextSetString(LMDialog.defTipsW, "");
    	XmTextSetString(LMDialog.delimitW, "");
    	XmTextSetString(LMDialog.tabW, "");
    	XmTextSetString(LMDialog.emTabW, "");
    	RadioButtonChangeState(LMDialog.defaultIndentW, True, True);
    	RadioButtonChangeState(LMDialog.defaultWrapW, True, True);
    } else {
    	XmTextSetString(LMDialog.nameW, strchr(lm->name, ':') == NULL ?
    	    	lm->name : strchr(lm->name, ':')+1);
    	extStr = createExtString(lm->extensions, lm->nExtensions);
    	XmTextSetString(LMDialog.extW, extStr);
    	NEditFree(extStr);
    	XmTextSetString(LMDialog.recogW, lm->recognitionExpr);
        XmTextSetString(LMDialog.defTipsW, lm->defTipsFile);
    	XmTextSetString(LMDialog.delimitW, lm->delimiters);
    	if (lm->tabDist == DEFAULT_TAB_DIST)
    	    XmTextSetString(LMDialog.tabW, "");
    	else
    	    SetIntText(LMDialog.tabW, lm->tabDist);
    	if (lm->emTabDist == DEFAULT_EM_TAB_DIST)
    	    XmTextSetString(LMDialog.emTabW, "");
    	else
    	    SetIntText(LMDialog.emTabW, lm->emTabDist);
    	RadioButtonChangeState(LMDialog.defaultIndentW,
    	    	lm->indentStyle == DEFAULT_INDENT, False);
    	RadioButtonChangeState(LMDialog.noIndentW,
    	    	lm->indentStyle == NO_AUTO_INDENT, False);
    	RadioButtonChangeState(LMDialog.autoIndentW,
    	    	lm->indentStyle == AUTO_INDENT, False);
    	RadioButtonChangeState(LMDialog.smartIndentW,
    	    	lm->indentStyle == SMART_INDENT, False);
    	RadioButtonChangeState(LMDialog.defaultWrapW,
    	    	lm->wrapStyle == DEFAULT_WRAP, False);
    	RadioButtonChangeState(LMDialog.noWrapW,
    	    	lm->wrapStyle == NO_WRAP, False);
    	RadioButtonChangeState(LMDialog.newlineWrapW,
    	    	lm->wrapStyle == NEWLINE_WRAP, False);
    	RadioButtonChangeState(LMDialog.contWrapW,
    	    	lm->wrapStyle == CONTINUOUS_WRAP, False);
    }
}

static void lmFreeItemCB(void *item)
{
    freeLanguageModeRec((languageModeRec *)item);
}

static void freeLanguageModeRec(languageModeRec *lm)
{
    int i;
    
    NEditFree(lm->name);
    NEditFree(lm->recognitionExpr);
    NEditFree(lm->defTipsFile);
    NEditFree(lm->delimiters);
    for (i=0; i<lm->nExtensions; i++)
    	NEditFree(lm->extensions[i]);
    NEditFree(lm->extensions);
    NEditFree(lm);
}

/*
** Copy a languageModeRec data structure and all of the allocated data it contains
*/
static languageModeRec *copyLanguageModeRec(languageModeRec *lm)
{
    languageModeRec *newLM;
    int i;
    
    newLM = (languageModeRec *)NEditMalloc(sizeof(languageModeRec));
    newLM->name = NEditStrdup(lm->name);
    newLM->nExtensions = lm->nExtensions;
    newLM->extensions = (char **)NEditMalloc(sizeof(char *) * lm->nExtensions);
    for (i=0; i<lm->nExtensions; i++) {
    	newLM->extensions[i] = NEditStrdup(lm->extensions[i]);
    }
    if (lm->recognitionExpr == NULL)
    	newLM->recognitionExpr = NULL;
    else {
	newLM->recognitionExpr = NEditStrdup(lm->recognitionExpr);
    }
    if (lm->defTipsFile == NULL)
    	newLM->defTipsFile = NULL;
    else {
	newLM->defTipsFile = NEditStrdup(lm->defTipsFile);
    }
    if (lm->delimiters == NULL)
    	newLM->delimiters = NULL;
    else {
	newLM->delimiters = NEditStrdup(lm->delimiters);
    }
    newLM->wrapStyle = lm->wrapStyle;
    newLM->indentStyle = lm->indentStyle;
    newLM->tabDist = lm->tabDist;
    newLM->emTabDist = lm->emTabDist;
    return newLM;
}

/*
** Read the fields in the language modes dialog and create a languageModeRec data
** structure reflecting the current state of the selected language mode in the dialog.
** If any of the information is incorrect or missing, display a warning dialog and
** return NULL.  Passing "silent" as True, suppresses the warning dialogs.
*/
static languageModeRec *readLMDialogFields(int silent)
{
    languageModeRec *lm;
    regexp *compiledRE;
    char *compileMsg, *extStr, *extPtr;

    /* Allocate a language mode structure to return, set unread fields to
       empty so everything can be freed on errors by freeLanguageModeRec */
    lm = (languageModeRec *)NEditMalloc(sizeof(languageModeRec));
    lm->nExtensions = 0;
    lm->recognitionExpr = NULL;
    lm->defTipsFile = NULL;
    lm->delimiters = NULL;

    /* read the name field */
    lm->name = ReadSymbolicFieldTextWidget(LMDialog.nameW,
    	    "language mode name", silent);
    if (lm->name == NULL) {
    	NEditFree(lm);
    	return NULL;
    }

    if (*lm->name == '\0')
    {
        if (!silent)
        {
            DialogF(DF_WARN, LMDialog.shell, 1, "Language Mode Name",
                    "Please specify a name\nfor the language mode", "OK");
            XmProcessTraversal(LMDialog.nameW, XmTRAVERSE_CURRENT);
        }
        freeLanguageModeRec(lm);
        return NULL;
    }
    
    /* read the extension list field */
    extStr = extPtr = XmTextGetString(LMDialog.extW);
    lm->extensions = readExtensionList(&extPtr, &lm->nExtensions);
    NEditFree(extStr);
    
    /* read recognition expression */
    lm->recognitionExpr = XmTextGetString(LMDialog.recogW);
    if (*lm->recognitionExpr == '\0') {
    	NEditFree(lm->recognitionExpr);
    	lm->recognitionExpr = NULL;
    } else
    {
        compiledRE = CompileRE(lm->recognitionExpr, &compileMsg, REDFLT_STANDARD);

        if (compiledRE == NULL)
        {
            if (!silent)
            {
                DialogF(DF_WARN, LMDialog.shell, 1, "Regex",
                        "Recognition expression:\n%s", "OK", compileMsg);
                XmProcessTraversal(LMDialog.recogW, XmTRAVERSE_CURRENT);
            }
            NEditFree(compiledRE);
            freeLanguageModeRec(lm);
            return NULL;    
        }

        NEditFree(compiledRE);
    }
    
    /* Read the default calltips file for the language mode */
    lm->defTipsFile = XmTextGetString(LMDialog.defTipsW);
    if (*lm->defTipsFile == '\0') {
        /* Empty string */
    	NEditFree(lm->defTipsFile);
    	lm->defTipsFile = NULL;
    } else {
        /* Ensure that AddTagsFile will work */
        if (AddTagsFile(lm->defTipsFile, TIP) == FALSE) {
            if (!silent)
            {
                DialogF(DF_WARN, LMDialog.shell, 1, "Error reading Calltips",
                        "Can't read default calltips file(s):\n  \"%s\"\n",
                        "OK", lm->defTipsFile);
                XmProcessTraversal(LMDialog.recogW, XmTRAVERSE_CURRENT);
            }
            freeLanguageModeRec(lm);
            return NULL;
        } else
            if (DeleteTagsFile(lm->defTipsFile, TIP, False) == FALSE)
                fprintf(stderr, "xnedit: Internal error: Trouble deleting " 
                        "calltips file(s):\n  \"%s\"\n", lm->defTipsFile);
    }
    
    /* read tab spacing field */
    if (TextWidgetIsBlank(LMDialog.tabW))
    	lm->tabDist = DEFAULT_TAB_DIST;
    else {
    	if (GetIntTextWarn(LMDialog.tabW, &lm->tabDist, "tab spacing", False)
    	    	!= TEXT_READ_OK) {
   	    freeLanguageModeRec(lm);
    	    return NULL;
	}

        if (lm->tabDist <= 0 || lm->tabDist > 100)
        {
            if (!silent)
            {
                DialogF(DF_WARN, LMDialog.shell, 1, "Invalid Tab Spacing",
                        "Invalid tab spacing: %d", "OK", lm->tabDist);
                XmProcessTraversal(LMDialog.tabW, XmTRAVERSE_CURRENT);
            }
            freeLanguageModeRec(lm);
            return NULL;
        }
    }
    
    /* read emulated tab field */
    if (TextWidgetIsBlank(LMDialog.emTabW))
    {
        lm->emTabDist = DEFAULT_EM_TAB_DIST;
    } else
    {
        if (GetIntTextWarn(LMDialog.emTabW, &lm->emTabDist,
                "emulated tab spacing", False) != TEXT_READ_OK)
        {
            freeLanguageModeRec(lm);
            return NULL;
        }

        if (lm->emTabDist < 0 || lm->emTabDist > 100)
        {
            if (!silent)
            {
                DialogF(DF_WARN, LMDialog.shell, 1, "Invalid Tab Spacing",
                        "Invalid emulated tab spacing: %d", "OK",
                        lm->emTabDist);
                XmProcessTraversal(LMDialog.emTabW, XmTRAVERSE_CURRENT);
            }
            freeLanguageModeRec(lm);
            return NULL;
        }
    }
    
    /* read delimiters string */
    lm->delimiters = XmTextGetString(LMDialog.delimitW);
    if (*lm->delimiters == '\0') {
    	NEditFree(lm->delimiters);
    	lm->delimiters = NULL;
    }
    
    /* read indent style */
    if (XmToggleButtonGetState(LMDialog.noIndentW))
    	 lm->indentStyle = NO_AUTO_INDENT;
    else if (XmToggleButtonGetState(LMDialog.autoIndentW))
    	 lm->indentStyle = AUTO_INDENT;
    else if (XmToggleButtonGetState(LMDialog.smartIndentW))
    	 lm->indentStyle = SMART_INDENT;
    else
    	 lm->indentStyle = DEFAULT_INDENT;
    
    /* read wrap style */
    if (XmToggleButtonGetState(LMDialog.noWrapW))
    	 lm->wrapStyle = NO_WRAP;
    else if (XmToggleButtonGetState(LMDialog.newlineWrapW))
    	 lm->wrapStyle = NEWLINE_WRAP;
    else if (XmToggleButtonGetState(LMDialog.contWrapW))
    	 lm->wrapStyle = CONTINUOUS_WRAP;
    else
    	 lm->wrapStyle = DEFAULT_WRAP;
    
    return lm;
}

/*
** Return True if the language mode dialog fields are blank (unchanged from the "New"
** language mode state).
*/
static int lmDialogEmpty(void)
{
    return TextWidgetIsBlank(LMDialog.nameW) &&
 	    TextWidgetIsBlank(LMDialog.extW) &&
	    TextWidgetIsBlank(LMDialog.recogW) &&
	    TextWidgetIsBlank(LMDialog.delimitW) &&
	    TextWidgetIsBlank(LMDialog.tabW) &&
	    TextWidgetIsBlank(LMDialog.emTabW) &&
	    XmToggleButtonGetState(LMDialog.defaultIndentW) &&
	    XmToggleButtonGetState(LMDialog.defaultWrapW);
}   	

/*
** Present a dialog for changing fonts (primary, and for highlighting).
*/
void ChooseFonts(WindowInfo *window, int forWindow)
{
#define MARGIN_SPACING 10
#define BTN_TEXT_OFFSET 3
    Widget form, primaryLbl, primaryBtn, italicLbl, italicBtn;
    Widget boldLbl, boldBtn, boldItalicLbl, boldItalicBtn;
    Widget primaryFrame, primaryForm, highlightFrame, highlightForm;
    Widget okBtn, applyBtn, cancelBtn;
    fontDialog *fd;
    XmString s1;
    int ac;
    Arg args[20];

    /* if the dialog is already displayed, just pop it to the top and return */
    if (window->fontDialog != NULL) {
    	RaiseDialogWindow(((fontDialog *)window->fontDialog)->shell);
    	return;
    }
    
    /* Create a structure for keeping track of dialog state */
    fd = (fontDialog *)NEditMalloc(sizeof(fontDialog));
    fd->window = window;
    fd->forWindow = forWindow;
    window->fontDialog = (void*)fd;
    
    /* Create a form widget in a dialog shell */
    ac = 0;
    XtSetArg(args[ac], XmNautoUnmanage, False); ac++;
    XtSetArg(args[ac], XmNresizePolicy, XmRESIZE_NONE); ac++;
    form = CreateFormDialog(window->shell, "choose Fonts", args, ac);
    XtVaSetValues(form, XmNshadowThickness, 0, NULL);
    fd->shell = XtParent(form);
    XtVaSetValues(fd->shell, XmNtitle, "Text Fonts", NULL);
    AddMotifCloseCallback(XtParent(form), fontCancelCB, fd);
    XtAddCallback(form, XmNdestroyCallback, fontDestroyCB, fd);

    primaryFrame = XtVaCreateManagedWidget("primaryFrame", xmFrameWidgetClass,
    	    form, XmNmarginHeight, 3,
	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 2,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    primaryForm = XtVaCreateManagedWidget("primaryForm", xmFormWidgetClass,
	    primaryFrame, NULL);
    primaryLbl = XtVaCreateManagedWidget("primaryFont", xmLabelGadgetClass,
    	    primaryFrame,
    	    XmNlabelString, s1=XmStringCreateSimple("Primary Font"),
    	    XmNmnemonic, 'P',
	    XmNchildType, XmFRAME_TITLE_CHILD,
	    XmNchildHorizontalAlignment, XmALIGNMENT_CENTER, NULL);
    XmStringFree(s1);

    primaryBtn = XtVaCreateManagedWidget("primaryBtn",
    	    xmPushButtonWidgetClass, primaryForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Browse..."),
    	    XmNmnemonic, 'r',
	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 2,
	    XmNtopOffset, BTN_TEXT_OFFSET,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);
    XtAddCallback(primaryBtn, XmNactivateCallback, primaryBrowseCB, fd);

    fd->primaryW = XtVaCreateManagedWidget("primary", xmTextWidgetClass,
    	    primaryForm,
    	    XmNcolumns, 30,
    	    XmNmaxLength, MAX_FONT_LEN,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, primaryBtn,
	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 2,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    RemapDeleteKey(fd->primaryW);
    XtAddCallback(fd->primaryW, XmNvalueChangedCallback,
    	    primaryModifiedCB, fd);
    XtVaSetValues(primaryLbl, XmNuserData, fd->primaryW, NULL);

    highlightFrame = XtVaCreateManagedWidget("highlightFrame",
    	    xmFrameWidgetClass, form,
	    XmNmarginHeight, 3,
	    XmNnavigationType, XmTAB_GROUP,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, primaryFrame,
	    XmNtopOffset, 20,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    highlightForm = XtVaCreateManagedWidget("highlightForm", xmFormWidgetClass,
    	    highlightFrame, NULL);
    XtVaCreateManagedWidget("highlightFonts", xmLabelGadgetClass,
    	    highlightFrame,
    	    XmNlabelString,
    	    	s1=XmStringCreateSimple("Fonts for Syntax Highlighting"),
	    XmNchildType, XmFRAME_TITLE_CHILD,
	    XmNchildHorizontalAlignment, XmALIGNMENT_CENTER, NULL);
    XmStringFree(s1);

    fd->fillW = XtVaCreateManagedWidget("fillBtn",
    	    xmPushButtonWidgetClass, highlightForm,
    	    XmNlabelString,
    	    	s1=XmStringCreateSimple("Fill Highlight Fonts from Primary"),
    	    XmNmnemonic, 'F',
    	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 2,
	    XmNtopOffset, BTN_TEXT_OFFSET,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);
    XtAddCallback(fd->fillW, XmNactivateCallback, fillFromPrimaryCB, fd);

    italicLbl = XtVaCreateManagedWidget("italicLbl", xmLabelGadgetClass,
    	    highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Italic Font"),
    	    XmNmnemonic, 'I',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, fd->fillW,
	    XmNtopOffset, MARGIN_SPACING,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);

    fd->italicErrW = XtVaCreateManagedWidget("italicErrLbl",
    	    xmLabelGadgetClass, highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"(vvv  spacing is inconsistent with primary font  vvv)"),
    	    XmNalignment, XmALIGNMENT_END,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, fd->fillW,
	    XmNtopOffset, MARGIN_SPACING,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, italicLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    XmStringFree(s1);

    italicBtn = XtVaCreateManagedWidget("italicBtn",
    	    xmPushButtonWidgetClass, highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Browse..."),
    	    XmNmnemonic, 'o',
    	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, italicLbl,
	    XmNtopOffset, BTN_TEXT_OFFSET,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);
    XtAddCallback(italicBtn, XmNactivateCallback, italicBrowseCB, fd);

    fd->italicW = XtVaCreateManagedWidget("italic", xmTextWidgetClass,
    	    highlightForm,
    	    XmNmaxLength, MAX_FONT_LEN,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, italicBtn,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, italicLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    RemapDeleteKey(fd->italicW);
    XtAddCallback(fd->italicW, XmNvalueChangedCallback,
    	    italicModifiedCB, fd);
    XtVaSetValues(italicLbl, XmNuserData, fd->italicW, NULL);

    boldLbl = XtVaCreateManagedWidget("boldLbl", xmLabelGadgetClass,
    	    highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Bold Font"),
    	    XmNmnemonic, 'B',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, italicBtn,
	    XmNtopOffset, MARGIN_SPACING,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);

    fd->boldErrW = XtVaCreateManagedWidget("boldErrLbl",
    	    xmLabelGadgetClass, highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple(""),
    	    XmNalignment, XmALIGNMENT_END,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, italicBtn,
	    XmNtopOffset, MARGIN_SPACING,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, boldLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    XmStringFree(s1);

    boldBtn = XtVaCreateManagedWidget("boldBtn",
    	    xmPushButtonWidgetClass, highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Browse..."),
    	    XmNmnemonic, 'w',
    	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, boldLbl,
	    XmNtopOffset, BTN_TEXT_OFFSET,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);
    XtAddCallback(boldBtn, XmNactivateCallback, boldBrowseCB, fd);

    fd->boldW = XtVaCreateManagedWidget("bold", xmTextWidgetClass,
    	    highlightForm,
    	    XmNmaxLength, MAX_FONT_LEN,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, boldBtn,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, boldLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    RemapDeleteKey(fd->boldW);
    XtAddCallback(fd->boldW, XmNvalueChangedCallback,
    	    boldModifiedCB, fd);
    XtVaSetValues(boldLbl, XmNuserData, fd->boldW, NULL);

    boldItalicLbl = XtVaCreateManagedWidget("boldItalicLbl", xmLabelGadgetClass,
    	    highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Bold Italic Font"),
    	    XmNmnemonic, 'l',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, boldBtn,
	    XmNtopOffset, MARGIN_SPACING,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);

    fd->boldItalicErrW = XtVaCreateManagedWidget("boldItalicErrLbl",
    	    xmLabelGadgetClass, highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple(""),
    	    XmNalignment, XmALIGNMENT_END,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, boldBtn,
	    XmNtopOffset, MARGIN_SPACING,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, boldItalicLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    XmStringFree(s1);

    boldItalicBtn = XtVaCreateManagedWidget("boldItalicBtn",
    	    xmPushButtonWidgetClass, highlightForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Browse..."),
    	    XmNmnemonic, 's',
    	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, boldItalicLbl,
	    XmNtopOffset, BTN_TEXT_OFFSET,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1, NULL);
    XmStringFree(s1);
    XtAddCallback(boldItalicBtn, XmNactivateCallback, boldItalicBrowseCB, fd);

    fd->boldItalicW = XtVaCreateManagedWidget("boldItalic",
    	    xmTextWidgetClass, highlightForm,
    	    XmNmaxLength, MAX_FONT_LEN,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, boldItalicBtn,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, boldItalicLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    RemapDeleteKey(fd->boldItalicW);
    XtAddCallback(fd->boldItalicW, XmNvalueChangedCallback,
    	    boldItalicModifiedCB, fd);
    XtVaSetValues(boldItalicLbl, XmNuserData, fd->boldItalicW, NULL);    

    okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
    	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, highlightFrame,
	    XmNtopOffset, MARGIN_SPACING,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, forWindow ? 13 : 26,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, forWindow ? 27 : 40, NULL);
    XtAddCallback(okBtn, XmNactivateCallback, fontOkCB, fd);
    XmStringFree(s1);

    if (forWindow) {
	applyBtn = XtVaCreateManagedWidget("apply",xmPushButtonWidgetClass,form,
    		XmNlabelString, s1=XmStringCreateSimple("Apply"),
    		XmNmnemonic, 'A',
    		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, highlightFrame,
		XmNtopOffset, MARGIN_SPACING,
    		XmNleftAttachment, XmATTACH_POSITION,
    		XmNleftPosition, 43,
    		XmNrightAttachment, XmATTACH_POSITION,
    		XmNrightPosition, 57, NULL);
	XtAddCallback(applyBtn, XmNactivateCallback, fontApplyCB, fd);
	XmStringFree(s1);
    }
    
    cancelBtn = XtVaCreateManagedWidget("cancel",
            xmPushButtonWidgetClass, form,
            XmNlabelString,
                    s1 = XmStringCreateSimple(forWindow ? "Close" : "Cancel"),
    	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, highlightFrame,
	    XmNtopOffset, MARGIN_SPACING,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, forWindow ? 73 : 59,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, forWindow ? 87 : 73,
            NULL);
    XtAddCallback(cancelBtn, XmNactivateCallback, fontCancelCB, fd);
    XmStringFree(s1);
 
    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, cancelBtn, NULL);
    
    /* Set initial values */
    if (forWindow) {
	XmTextSetString(fd->primaryW, window->fontName);
	XmTextSetString(fd->boldW, window->boldFontName);
	XmTextSetString(fd->italicW, window->italicFontName);
	XmTextSetString(fd->boldItalicW, window->boldItalicFontName);
    } else {
    	XmTextSetString(fd->primaryW, GetPrefFontName());
	XmTextSetString(fd->boldW, GetPrefBoldFontName());
	XmTextSetString(fd->italicW, GetPrefItalicFontName());
	XmTextSetString(fd->boldItalicW, GetPrefBoldItalicFontName());
    }
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);
    
    /* put up dialog */
    ManageDialogCenteredOnPointer(form);
}

static void fillFromPrimaryCB(Widget w, XtPointer clientData,
    	XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;
    char *primaryName;
    size_t primaryLen;
    
    char *italic;
    char *bold;
    char *bolditalic;
    
    primaryName = XmTextGetString(fd->primaryW);
    primaryLen = strlen(primaryName);
    
    if(primaryLen > 0) {
        italic = FontNameAddAttribute(primaryName, primaryLen, "slant", "italic");
        bold = FontNameAddAttribute(primaryName, primaryLen, "weight", "bold");
        bolditalic = FontNameAddAttribute(bold, strlen(bold), "slant", "italic");
        
        XmTextSetString(fd->boldW, bold);
        XmTextSetString(fd->italicW, italic);
        XmTextSetString(fd->boldItalicW, bolditalic);
        
        NEditFree(bold);
        NEditFree(italic);
        NEditFree(bolditalic);
    }
    
    NEditFree(primaryName);
}

static void primaryModifiedCB(Widget w, XtPointer clientData,
	XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    showFontStatus(fd, fd->italicW, fd->italicErrW);
    showFontStatus(fd, fd->boldW, fd->boldErrW);
    showFontStatus(fd, fd->boldItalicW, fd->boldItalicErrW);
}
static void italicModifiedCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    showFontStatus(fd, fd->italicW, fd->italicErrW);
}
static void boldModifiedCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    showFontStatus(fd, fd->boldW, fd->boldErrW);
}
static void boldItalicModifiedCB(Widget w, XtPointer clientData,
    	XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    showFontStatus(fd, fd->boldItalicW, fd->boldItalicErrW);
}

static void primaryBrowseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    browseFont(fd->shell, fd->primaryW);
}
static void italicBrowseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    browseFont(fd->shell, fd->italicW);
}
static void boldBrowseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    browseFont(fd->shell, fd->boldW);
}
static void boldItalicBrowseCB(Widget w, XtPointer clientData,
    	XtPointer callData)
{
   fontDialog *fd = (fontDialog *)clientData;

   browseFont(fd->shell, fd->boldItalicW);
}

static void fontDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;
    
    fd->window->fontDialog = NULL;
    NEditFree(fd);
}

static void fontOkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;
    
    updateFonts(fd);

    /* pop down and destroy the dialog */
    XtDestroyWidget(fd->shell);
}

static void fontApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;
    
    updateFonts(fd);
}

static void fontCancelCB(Widget w, XtPointer clientData, XtPointer callData)
{
    fontDialog *fd = (fontDialog *)clientData;

    /* pop down and destroy the dialog */
    XtDestroyWidget(fd->shell);
}

/*
** Check over a font name in a text field to make sure it agrees with the
** primary font in height and spacing.
*/
static int checkFontStatus(fontDialog *fd, Widget fontTextFieldW)
{
    char *primaryName, *testName;
    NFont *primaryFont, *testFont;
    XftFont *primary, *test;
    Display *display = XtDisplay(fontTextFieldW);
    int primaryHeight, testHeight;
    
    /* Get width and height of the font to check.  Note the test for empty
       name: X11R6 clients freak out X11R5 servers if they ask them to load
       an empty font name, and kill the whole application! */
    testName = XmTextGetString(fontTextFieldW);
    if (testName[0] == '\0') {
	NEditFree(testName);
    	return BAD_FONT;
    }
    //testFont = XLoadQueryFont(display, testName);
    testFont = FontFromName(display, testName);
    if (testFont == NULL) {
    	NEditFree(testName);
    	return BAD_FONT;
    }
    test = FontDefault(testFont);
    NEditFree(testName);
    testHeight = test->ascent + test->descent;
    FontDestroy(testFont);
    
    /* Get width and height of the primary font */
    primaryName = XmTextGetString(fd->primaryW);
    if (primaryName[0] == '\0') {
	NEditFree(primaryName);
    	return BAD_FONT;
    }
    primaryFont = FontFromName(display, primaryName);
    if (primaryFont == NULL) {
    	NEditFree(primaryName);
    	return BAD_PRIMARY;
    }
    primary = FontDefault(primaryFont);
    NEditFree(primaryName);
    primaryHeight = primary->ascent + primary->descent;
    FontDestroy(primaryFont);
    
    /* Compare font information */
    if (testHeight != primaryHeight)
    	return BAD_SIZE;
    return GOOD_FONT;
}

/*
** Update the error label for a font text field to reflect its validity and degree
** of agreement with the currently selected primary font
*/
static int showFontStatus(fontDialog *fd, Widget fontTextFieldW,
    	Widget errorLabelW)
{
    int status;
    XmString s;
    char *msg;
    
    status = checkFontStatus(fd, fontTextFieldW);
    if (status == BAD_PRIMARY)
    	msg = "(font below may not match primary font)";
    else if (status == BAD_FONT)
    	msg = "(xxx font below is invalid xxx)";
    else if (status == BAD_SIZE)
    	msg = "(height of font below does not match primary)";
    else if (status == BAD_SPACING)
    	msg = "(spacing of font below does not match primary)";
    else
    	msg = "";
    
    XtVaSetValues(errorLabelW, XmNlabelString, s=XmStringCreateSimple(msg),
	    NULL);
    XmStringFree(s);
    return status;
}

/*
** Put up a font selector panel to set the font name in the text widget "fontTextW"
*/
static void browseFont(Widget parent, Widget fontTextW)
{
    char *origFontName, *newFontName;
    
    origFontName = XmTextGetString(fontTextW);

    newFontName = FontSel(parent, origFontName);
    NEditFree(origFontName);
    if (newFontName == NULL)
    	return;
    XmTextSetString(fontTextW, newFontName);
    NEditFree(newFontName);
}

/*
** Accept the changes in the dialog and set the fonts regardless of errors
*/
static void updateFonts(fontDialog *fd)
{
    char *fontName, *italicName, *boldName, *boldItalicName;
    
    fontName = XmTextGetString(fd->primaryW);
    italicName = XmTextGetString(fd->italicW);
    boldName = XmTextGetString(fd->boldW);
    boldItalicName = XmTextGetString(fd->boldItalicW);
    
    if (fd->forWindow) {
        char *params[4];
        params[0] = fontName;
        params[1] = italicName;
        params[2] = boldName;
        params[3] = boldItalicName;
        XtCallActionProc(fd->window->textArea, "set_fonts", NULL, params, 4);
/*
    	SetFonts(fd->window, fontName, italicName, boldName, boldItalicName);
*/
    }
    else {
    	SetPrefFont(fontName);
    	SetPrefItalicFont(italicName);
    	SetPrefBoldFont(boldName);
    	SetPrefBoldItalicFont(boldItalicName);
    }
    NEditFree(fontName);
    NEditFree(italicName);
    NEditFree(boldName);
    NEditFree(boldItalicName);
}

/*
** Change the language mode to the one indexed by "mode", reseting word
** delimiters, syntax highlighting and other mode specific parameters
*/
static void reapplyLanguageMode(WindowInfo *window, int mode, int forceDefaults)
{
    char *delimiters;
    int i, wrapMode, indentStyle, tabDist, emTabDist, highlight, oldEmTabDist;
    int wrapModeIsDef, tabDistIsDef, emTabDistIsDef, indentStyleIsDef;
    int highlightIsDef, haveHighlightPatterns, haveSmartIndentMacros;
    int oldMode = window->languageMode;
    
    /* If the mode is the same, and changes aren't being forced (as might
       happen with Save As...), don't mess with already correct settings */
    if (window->languageMode == mode && !forceDefaults)
	return;
    
    /* Change the mode name stored in the window */
    window->languageMode = mode;
    
    /* Decref oldMode's default calltips file if needed */
    if (oldMode != PLAIN_LANGUAGE_MODE && LanguageModes[oldMode]->defTipsFile) {
        DeleteTagsFile( LanguageModes[oldMode]->defTipsFile, TIP, False );
    }
    
    /* Set delimiters for all text widgets */
    if (mode == PLAIN_LANGUAGE_MODE || LanguageModes[mode]->delimiters == NULL)
    	delimiters = GetPrefDelimiters();
    else
    	delimiters = LanguageModes[mode]->delimiters;
    XtVaSetValues(window->textArea, textNwordDelimiters, delimiters, NULL);
    for (i=0; i<window->nPanes; i++)
    	XtVaSetValues(window->textPanes[i], textNautoIndent, delimiters, NULL);
    
    /* Decide on desired values for language-specific parameters.  If a
       parameter was set to its default value, set it to the new default,
       otherwise, leave it alone */
    wrapModeIsDef = window->wrapMode == GetPrefWrap(oldMode);
    tabDistIsDef = BufGetTabDistance(window->buffer) == GetPrefTabDist(oldMode);
    XtVaGetValues(window->textArea, textNemulateTabs, &oldEmTabDist, NULL);
    emTabDistIsDef = oldEmTabDist == GetPrefEmTabDist(oldMode);
    indentStyleIsDef = window->indentStyle == GetPrefAutoIndent(oldMode) ||
	    (GetPrefAutoIndent(oldMode) == SMART_INDENT &&
	     window->indentStyle == AUTO_INDENT &&
	     !SmartIndentMacrosAvailable(LanguageModeName(oldMode)));
    highlightIsDef = window->highlightSyntax == GetPrefHighlightSyntax()
	    || (GetPrefHighlightSyntax() &&
		 FindPatternSet(LanguageModeName(oldMode)) == NULL);
    wrapMode = wrapModeIsDef || forceDefaults ?
    	    GetPrefWrap(mode) : window->wrapMode;
    tabDist = tabDistIsDef || forceDefaults ?
	    GetPrefTabDist(mode) : BufGetTabDistance(window->buffer);
    emTabDist = emTabDistIsDef || forceDefaults ?
	    GetPrefEmTabDist(mode) : oldEmTabDist;
    indentStyle = indentStyleIsDef || forceDefaults ?
    	    GetPrefAutoIndent(mode) : window->indentStyle;
    highlight = highlightIsDef || forceDefaults ? 
	    GetPrefHighlightSyntax() : window->highlightSyntax;
	     
    /* Dim/undim smart-indent and highlighting menu items depending on
       whether patterns/macros are available */
    haveHighlightPatterns = FindPatternSet(LanguageModeName(mode)) != NULL;
    haveSmartIndentMacros = SmartIndentMacrosAvailable(LanguageModeName(mode));
    if (IsTopDocument(window)) {
	XtSetSensitive(window->highlightItem, haveHighlightPatterns);
	XtSetSensitive(window->smartIndentItem, haveSmartIndentMacros);
    }
        
    /* Turn off requested options which are not available */
    highlight = haveHighlightPatterns && highlight;
    if (indentStyle == SMART_INDENT && !haveSmartIndentMacros)
	indentStyle = AUTO_INDENT;

    /* Change highlighting */
    window->highlightSyntax = highlight;
    SetToggleButtonState(window, window->highlightItem, highlight, False);
    StopHighlighting(window);

    /* we defer highlighting to RaiseDocument() if doc is hidden */
    if (IsTopDocument(window) && highlight)
    	StartHighlighting(window, False);

    /* Force a change of smart indent macros (SetAutoIndent will re-start) */
    if (window->indentStyle == SMART_INDENT) {
	EndSmartIndent(window);
	window->indentStyle = AUTO_INDENT;
    }
    
    /* set requested wrap, indent, and tabs */
    SetAutoWrap(window, wrapMode);
    SetAutoIndent(window, indentStyle);
    SetTabDist(window, tabDist);
    SetEmTabDist(window, emTabDist);
    
    /* Load calltips files for new mode */
    if (mode != PLAIN_LANGUAGE_MODE && LanguageModes[mode]->defTipsFile) {
        AddTagsFile( LanguageModes[mode]->defTipsFile, TIP );
    }

    /* Add/remove language specific menu items */
    UpdateUserMenus(window);
}

/*
** Find and return the name of the appropriate languange mode for
** the file in "window".  Returns a pointer to a string, which will
** remain valid until a change is made to the language modes list.
*/
static int matchLanguageMode(WindowInfo *window)
{
    char *ext, *first200;
    int i, j, fileNameLen, extLen, beginPos, endPos, start;
    const char *versionExtendedPath;

    /*... look for an explicit mode statement first */
    
    /* Do a regular expression search on for recognition pattern */
    first200 = BufGetRange(window->buffer, 0, 200);
    for (i=0; i<NLanguageModes; i++) {
    	if (LanguageModes[i]->recognitionExpr != NULL) {
    	    if (SearchString(first200, LanguageModes[i]->recognitionExpr,
    	    	    SEARCH_FORWARD, SEARCH_REGEX, False, 0, &beginPos,
    	    	    &endPos, NULL, NULL, NULL))
            {
		NEditFree(first200);
    	    	return i;
	    }
    	}
    }
    NEditFree(first200);
    
    /* Look at file extension ("@@/" starts a ClearCase version extended path,
       which gets appended after the file extension, and therefore must be
       stripped off to recognize the extension to make ClearCase users happy) */
    fileNameLen = strlen(window->filename);
    if ((versionExtendedPath = GetClearCaseVersionExtendedPath(window->filename)) != NULL)
        fileNameLen = versionExtendedPath - window->filename;
    for (i=0; i<NLanguageModes; i++) {
    	for (j=0; j<LanguageModes[i]->nExtensions; j++) {
    	    ext = LanguageModes[i]->extensions[j];
    	    extLen = strlen(ext);
    	    start = fileNameLen - extLen;
            if (start >= 0 && !strncmp(&window->filename[start], ext, extLen))  
                return i;
    	}
    }

    /* no appropriate mode was found */
    return PLAIN_LANGUAGE_MODE;
}

static int loadLanguageModesString(char *inString, int fileVer)
{    
    char *errMsg, *styleName, *inPtr = inString;
    languageModeRec *lm;
    int i;

    for (;;) {
   	
	/* skip over blank space */
	inPtr += strspn(inPtr, " \t\n");
    	
	/* Allocate a language mode structure to return, set unread fields to
	   empty so everything can be freed on errors by freeLanguageModeRec */
	lm = (languageModeRec *)NEditMalloc(sizeof(languageModeRec));
	lm->nExtensions = 0;
	lm->recognitionExpr = NULL;
        lm->defTipsFile = NULL;
	lm->delimiters = NULL;

	/* read language mode name */
	lm->name = ReadSymbolicField(&inPtr);
	if (lm->name == NULL) {
    	    NEditFree(lm);
    	    return modeError(NULL,inString,inPtr,"language mode name required");
	}
	if (!SkipDelimiter(&inPtr, &errMsg))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
    	/* read list of extensions */
    	lm->extensions = readExtensionList(&inPtr,
    	    	&lm->nExtensions);
	if (!SkipDelimiter(&inPtr, &errMsg))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
	/* read the recognition regular expression */
	if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
    	    lm->recognitionExpr = NULL;
    	else if (!ReadQuotedString(&inPtr, &errMsg, &lm->recognitionExpr))
    	    return modeError(lm, inString,inPtr, errMsg);
	if (!SkipDelimiter(&inPtr, &errMsg))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
    	/* read the indent style */
    	styleName = ReadSymbolicField(&inPtr);
	if (styleName == NULL)
    	    lm->indentStyle = DEFAULT_INDENT;
    	else {
	    for (i=0; i<N_INDENT_STYLES; i++) {
	    	if (!strcmp(styleName, AutoIndentTypes[i])) {
	    	    lm->indentStyle = i;
	    	    break;
	    	}
	    }
	    NEditFree(styleName);
	    if (i == N_INDENT_STYLES)
	    	return modeError(lm,inString,inPtr,"unrecognized indent style");
	}
	if (!SkipDelimiter(&inPtr, &errMsg))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
    	/* read the wrap style */
    	styleName = ReadSymbolicField(&inPtr);
	if (styleName == NULL)
    	    lm->wrapStyle = DEFAULT_WRAP;
    	else {
	    for (i=0; i<N_WRAP_STYLES; i++) {
	    	if (!strcmp(styleName, AutoWrapTypes[i])) {
	    	    lm->wrapStyle = i;
	    	    break;
	    	}
	    }
	    NEditFree(styleName);
	    if (i == N_WRAP_STYLES)
	    	return modeError(lm, inString, inPtr,"unrecognized wrap style");
	}
	if (!SkipDelimiter(&inPtr, &errMsg))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
    	/* read the tab distance */
	if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
    	    lm->tabDist = DEFAULT_TAB_DIST;
    	else if (!ReadNumericField(&inPtr, &lm->tabDist))
    	    return modeError(lm, inString, inPtr, "bad tab spacing");
	if (!SkipDelimiter(&inPtr, &errMsg))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
    	/* read emulated tab distance */
    	if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
    	    lm->emTabDist = DEFAULT_EM_TAB_DIST;
    	else if (!ReadNumericField(&inPtr, &lm->emTabDist))
    	    return modeError(lm, inString, inPtr, "bad emulated tab spacing");
	if (!SkipDelimiter(&inPtr, &errMsg))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
	/* read the delimiters string */
	if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
    	    lm->delimiters = NULL;
    	else if (!ReadQuotedString(&inPtr, &errMsg, &lm->delimiters))
    	    return modeError(lm, inString, inPtr, errMsg);
        
        /* After 5.3 all language modes need a default tips file field */
	if (!SkipDelimiter(&inPtr, &errMsg))
            if (fileVer > 5003)
    	        return modeError(lm, inString, inPtr, errMsg);

        /* read the default tips file */
	if (*inPtr == '\n' || *inPtr == '\0')
    	    lm->defTipsFile = NULL;
    	else if (!ReadQuotedString(&inPtr, &errMsg, &lm->defTipsFile))
    	    return modeError(lm, inString, inPtr, errMsg);
    	
   	/* pattern set was read correctly, add/replace it in the list */
   	for (i=0; i<NLanguageModes; i++) {
	    if (!strcmp(LanguageModes[i]->name, lm->name)) {
		freeLanguageModeRec(LanguageModes[i]);
		LanguageModes[i] = lm;
		break;
	    }
	}
	if (i == NLanguageModes) {
	    LanguageModes[NLanguageModes++] = lm;
   	    if (NLanguageModes > MAX_LANGUAGE_MODES)
   		return modeError(NULL, inString, inPtr,
   	    		"maximum allowable number of language modes exceeded");
	}
    	
    	/* if the string ends here, we're done */
   	inPtr += strspn(inPtr, " \t\n");
    	if (*inPtr == '\0')
    	    return True;
    } /* End for(;;) */
}

static char *writeLanguageModesString(void)
{
    int i;
    char *outStr, *escapedStr, *str, numBuf[25];
    textBuffer *outBuf;
    
    outBuf = BufCreate();
    for (i=0; i<NLanguageModes; i++) {
    	BufInsert(outBuf, outBuf->length, "\t");
    	BufInsert(outBuf, outBuf->length, LanguageModes[i]->name);
    	BufInsert(outBuf, outBuf->length, ":");
    	BufInsert(outBuf, outBuf->length, str = createExtString(
    	    	LanguageModes[i]->extensions, LanguageModes[i]->nExtensions));
    	NEditFree(str);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (LanguageModes[i]->recognitionExpr != NULL) {
    	    BufInsert(outBuf, outBuf->length,
    	    	    str=MakeQuotedString(LanguageModes[i]->recognitionExpr));
    	    NEditFree(str);
    	}
    	BufInsert(outBuf, outBuf->length, ":");
    	if (LanguageModes[i]->indentStyle != DEFAULT_INDENT)
    	    BufInsert(outBuf, outBuf->length,
    	    	    AutoIndentTypes[LanguageModes[i]->indentStyle]);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (LanguageModes[i]->wrapStyle != DEFAULT_WRAP)
    	    BufInsert(outBuf, outBuf->length,
    	    	    AutoWrapTypes[LanguageModes[i]->wrapStyle]);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (LanguageModes[i]->tabDist != DEFAULT_TAB_DIST) {
    	    sprintf(numBuf, "%d", LanguageModes[i]->tabDist);
    	    BufInsert(outBuf, outBuf->length, numBuf);
    	}
    	BufInsert(outBuf, outBuf->length, ":");
    	if (LanguageModes[i]->emTabDist != DEFAULT_EM_TAB_DIST) {
    	    sprintf(numBuf, "%d", LanguageModes[i]->emTabDist);
    	    BufInsert(outBuf, outBuf->length, numBuf);
    	}
    	BufInsert(outBuf, outBuf->length, ":");
    	if (LanguageModes[i]->delimiters != NULL) {
    	    BufInsert(outBuf, outBuf->length,
    	    	    str=MakeQuotedString(LanguageModes[i]->delimiters));
    	    NEditFree(str);
    	}
    	BufInsert(outBuf, outBuf->length, ":");
    	if (LanguageModes[i]->defTipsFile != NULL) {
    	    BufInsert(outBuf, outBuf->length,
    	    	    str=MakeQuotedString(LanguageModes[i]->defTipsFile));
    	    NEditFree(str);
    	}
        
    	BufInsert(outBuf, outBuf->length, "\n");
    }
    
    /* Get the output, and lop off the trailing newline */
    outStr = BufGetRange(outBuf, 0, outBuf->length - 1);
    BufFree(outBuf);
    escapedStr = EscapeSensitiveChars(outStr);
    NEditFree(outStr);
    return escapedStr;
}

static char *createExtString(char **extensions, int nExtensions)
{
    int e, length = 1;
    char *outStr, *outPtr;

    for (e=0; e<nExtensions; e++)
    	length += strlen(extensions[e]) + 1;
    outStr = outPtr = (char*)NEditMalloc(length);
    for (e=0; e<nExtensions; e++) {
    	strcpy(outPtr, extensions[e]);
    	outPtr += strlen(extensions[e]);
    	*outPtr++ = ' ';
    }
    if (nExtensions == 0)
    	*outPtr = '\0';
    else
    	*(outPtr-1) = '\0';
    return outStr;
}

static char **readExtensionList(char **inPtr, int *nExtensions)
{
    char *extensionList[MAX_FILE_EXTENSIONS];
    char **retList, *strStart;
    int i, len;
    
    /* skip over blank space */
    *inPtr += strspn(*inPtr, " \t");
    
    for (i=0; i<MAX_FILE_EXTENSIONS && **inPtr!=':' && **inPtr!='\0'; i++) {
    	*inPtr += strspn(*inPtr, " \t");
	strStart = *inPtr;
	while (**inPtr!=' ' && **inPtr!='\t' && **inPtr!=':' && **inPtr!='\0')
	    (*inPtr)++;
    	len = *inPtr - strStart;
    	extensionList[i] = (char*)NEditMalloc(len + 1);
    	strncpy(extensionList[i], strStart, len);
    	extensionList[i][len] = '\0';
    }
    *nExtensions = i;
    if (i == 0)
    	return NULL;
    retList = (char **)NEditMalloc(sizeof(char *) * i);
    memcpy(retList, extensionList, sizeof(char *) * i);
    return retList;
}

int ReadNumericField(char **inPtr, int *value)
{
    int charsRead;
    
    /* skip over blank space */
    *inPtr += strspn(*inPtr, " \t");
    
    if (sscanf(*inPtr, "%d%n", value, &charsRead) != 1)
    	return False;
    *inPtr += charsRead;
    return True;
}

/*
** Parse a symbolic field, skipping initial and trailing whitespace,
** stops on first invalid character or end of string.  Valid characters
** are letters, numbers, _, -, +, $, #, and internal whitespace.  Internal
** whitespace is compressed to single space characters.
*/
char *ReadSymbolicField(char **inPtr)
{
    char *outStr, *outPtr, *strStart, *strPtr;
    int len;
    
    /* skip over initial blank space */
    *inPtr += strspn(*inPtr, " \t");
    
    /* Find the first invalid character or end of string to know how
       much memory to allocate for the returned string */
    strStart = *inPtr;
    while (isalnum((unsigned char)**inPtr) || **inPtr=='_' || **inPtr=='-' ||  
      	    **inPtr=='+' || **inPtr=='$' || **inPtr=='#' || **inPtr==' ' || 
      	    **inPtr=='\t')
    	(*inPtr)++;
    len = *inPtr - strStart;
    if (len == 0)
    	return NULL;
    outStr = outPtr = (char*)NEditMalloc(len + 1);
    
    /* Copy the string, compressing internal whitespace to a single space */
    strPtr = strStart;
    while (strPtr - strStart < len) {
    	if (*strPtr == ' ' || *strPtr == '\t') {
    	    strPtr += strspn(strPtr, " \t");
    	    *outPtr++ = ' ';
    	} else
    	    *outPtr++ = *strPtr++;
    }
    
    /* If there's space on the end, take it back off */
    if (outPtr > outStr && *(outPtr-1) == ' ')
    	outPtr--;
    if (outPtr == outStr) {
    	NEditFree(outStr);
    	return NULL;
    }
    *outPtr = '\0';
    return outStr;
}

/*
** parse an individual quoted string.  Anything between
** double quotes is acceptable, quote characters can be escaped by "".
** Returns allocated string "string" containing
** argument minus quotes.  If not successful, returns False with
** (statically allocated) message in "errMsg".
*/
int ReadQuotedString(char **inPtr, char **errMsg, char **string)
{
    char *outPtr, *c;
    
    /* skip over blank space */
    *inPtr += strspn(*inPtr, " \t");
    
    /* look for initial quote */
    if (**inPtr != '\"') {
    	*errMsg = "expecting quoted string";
    	return False;
    }
    (*inPtr)++;
    
    /* calculate max length and allocate returned string */
    for (c= *inPtr; ; c++) {
    	if (*c == '\0') {
    	    *errMsg = "string not terminated";
    	    return False;
    	} else if (*c == '\"') {
    	    if (*(c+1) == '\"')
    	    	c++;
    	    else
    	    	break;
    	}
    }
    
    /* copy string up to end quote, transforming escaped quotes into quotes */
    *string = (char*)NEditMalloc(c - *inPtr + 1);
    outPtr = *string;
    while (True) {
    	if (**inPtr == '\"') {
    	    if (*(*inPtr+1) == '\"')
    	    	(*inPtr)++;
    	    else
    	    	break;
    	}
    	*outPtr++ = *(*inPtr)++;
    }
    *outPtr = '\0';

    /* skip end quote */
    (*inPtr)++;
    return True;
}

/*
** Replace characters which the X resource file reader considers control
** characters, such that a string will read back as it appears in "string".
** (So far, newline characters are replaced with with \n\<newline> and
** backslashes with \\.  This has not been tested exhaustively, and
** probably should be.  It would certainly be more asthetic if other
** control characters were replaced as well).
**
** Returns an allocated string which must be freed by the caller with NEditFree.
*/
char *EscapeSensitiveChars(const char *string)
{
    const char *c;
    char *outStr, *outPtr;
    int length = 0;

    /* calculate length and allocate returned string */
    for (c=string; *c!='\0'; c++) {
    	if (*c == '\\')
    	    length++;
    	else if (*c == '\n')
    	    length += 3;
    	length++;
    }
    outStr = (char*)NEditMalloc(length + 1);
    outPtr = outStr;
    
    /* add backslashes */
    for (c=string; *c!='\0'; c++) {
    	if (*c == '\\')
    	    *outPtr++ = '\\';
    	else if (*c == '\n') {
    	    *outPtr++ = '\\';
    	    *outPtr++ = 'n';
    	    *outPtr++ = '\\';
    	}
    	*outPtr++ = *c;
    }
    *outPtr = '\0';
    return outStr;
}

/*
** Adds double quotes around a string and escape existing double quote
** characters with two double quotes.  Enables the string to be read back
** by ReadQuotedString.
*/
char *MakeQuotedString(const char *string)
{
    const char *c;
    char *outStr, *outPtr;
    int length = 0;

    /* calculate length and allocate returned string */
    for (c=string; *c!='\0'; c++) {
    	if (*c == '\"')
    	    length++;
    	length++;
    }
    outStr = (char*)NEditMalloc(length + 3);
    outPtr = outStr;
    
    /* add starting quote */
    *outPtr++ = '\"';
    
    /* copy string, escaping quotes with "" */
    for (c=string; *c!='\0'; c++) {
    	if (*c == '\"')
    	    *outPtr++ = '\"';
    	*outPtr++ = *c;
    }
    
    /* add ending quote */
    *outPtr++ = '\"';

    /* terminate string and return */
    *outPtr = '\0';
    return outStr;
}

/*
** Read a dialog text field containing a symbolic name (language mode names,
** style names, highlight pattern names, colors, and fonts), clean the
** entered text of leading and trailing whitespace, compress all
** internal whitespace to one space character, and check it over for
** colons, which interfere with the preferences file reader/writer syntax.
** Returns NULL on error, and puts up a dialog if silent is False.  Returns
** an empty string if the text field is blank.
*/
char *ReadSymbolicFieldTextWidget(Widget textW, const char *fieldName, int silent)
{
    char *string, *stringPtr, *parsedString;
    
    /* read from the text widget */
    string = stringPtr = XmTextGetString(textW);
    
    /* parse it with the same routine used to read symbolic fields from
       files.  If the string is not read entirely, there are invalid
       characters, so warn the user if not in silent mode. */
    parsedString = ReadSymbolicField(&stringPtr);
    if (*stringPtr != '\0')
    {
        if (!silent)
        {
            *(stringPtr + 1) = '\0';
            DialogF(DF_WARN, textW, 1, "Invalid Character",
                    "Invalid character \"%s\" in %s", "OK", stringPtr,
                    fieldName);
            XmProcessTraversal(textW, XmTRAVERSE_CURRENT);
        }
        NEditFree(string);
        NEditFree(parsedString);
        return NULL;
    }
    NEditFree(string);
    if (parsedString == NULL) {
    	parsedString = NEditStrdup("");
    }
    return parsedString;
}

/*
** Create a pulldown menu pane with the names of the current language modes.
** XmNuserData for each item contains the language mode name.
*/
Widget CreateLanguageModeMenu(Widget parent, XtCallbackProc cbProc, void *cbArg)
{
    Widget menu, btn;
    int i;
    XmString s1;

    menu = CreatePulldownMenu(parent, "languageModes", NULL, 0);
    for (i=0; i<NLanguageModes; i++) {
        btn = XtVaCreateManagedWidget("languageMode", xmPushButtonGadgetClass,
        	menu,
        	XmNlabelString, s1=XmStringCreateSimple(LanguageModes[i]->name),
		XmNmarginHeight, 0,
    		XmNuserData, (void *)LanguageModes[i]->name, NULL);
        XmStringFree(s1);
	XtAddCallback(btn, XmNactivateCallback, cbProc, cbArg);
    }
    return menu;
}

/*
** Set the language mode menu in option menu "optMenu" to
** show a particular language mode
*/
void SetLangModeMenu(Widget optMenu, const char *modeName)
{
    int i;
    Cardinal nItems;
    WidgetList items;
    Widget pulldown, selectedItem;
    char *itemName;

    XtVaGetValues(optMenu, XmNsubMenuId, &pulldown, NULL);
    XtVaGetValues(pulldown, XmNchildren, &items, XmNnumChildren, &nItems, NULL);
    if (nItems == 0)
    	return;
    selectedItem = items[0];
    for (i=0; i<(int)nItems; i++) {
    	XtVaGetValues(items[i], XmNuserData, &itemName, NULL);
    	if (!strcmp(itemName, modeName)) {
    	    selectedItem = items[i];
    	    break;
    	}
    }
    XtVaSetValues(optMenu, XmNmenuHistory, selectedItem,NULL);
}

/*
** Create a submenu for chosing language mode for the current window.
*/
void CreateLanguageModeSubMenu(WindowInfo* window, const Widget parent,
        const char* name, const char* label, char mnemonic)
{
    XmString string = XmStringCreateSimple((char*) label);

    window->langModeCascade = XtVaCreateManagedWidget(name,
            xmCascadeButtonGadgetClass, parent,
            XmNlabelString, string,
            XmNmnemonic, mnemonic,
            XmNsubMenuId, NULL,
            NULL);
    XmStringFree(string);

    updateLanguageModeSubmenu(window);
}

/*
** Re-build the language mode sub-menu using the current data stored
** in the master list: LanguageModes.
*/
static void updateLanguageModeSubmenu(WindowInfo *window)
{
    int i;
    XmString s1;
    Widget menu, btn;
    Arg args[1] = {{XmNradioBehavior, (XtArgVal)True}};
    
    EnableWindowResourceDB(window);
    
    /* Destroy and re-create the menu pane */
    XtVaGetValues(window->langModeCascade, XmNsubMenuId, &menu, NULL);
    if (menu != NULL)
    	XtDestroyWidget(menu);
    menu = CreatePulldownMenu(XtParent(window->langModeCascade),
    	    "languageModes", args, 1);
    btn = XtVaCreateManagedWidget("languageMode",
            xmToggleButtonGadgetClass, menu,
            XmNlabelString, s1=XmStringCreateSimple("Plain"),
    	    XmNuserData, (void *)PLAIN_LANGUAGE_MODE,
    	    XmNset, window->languageMode==PLAIN_LANGUAGE_MODE, NULL);
    XmStringFree(s1);
    XtAddCallback(btn, XmNvalueChangedCallback, setLangModeCB, window);
    for (i=0; i<NLanguageModes; i++) {
        btn = XtVaCreateManagedWidget("languageMode",
            	xmToggleButtonGadgetClass, menu, 
            	XmNlabelString, s1=XmStringCreateSimple(LanguageModes[i]->name),
 	    	/* XmNmarginHeight, 0, */
   		XmNuserData, (void *)(intptr_t)i,
    		XmNset, window->languageMode==i, NULL);
        XmStringFree(s1);
	XtAddCallback(btn, XmNvalueChangedCallback, setLangModeCB, window);
    }
    XtVaSetValues(window->langModeCascade, XmNsubMenuId, menu, NULL);
    
    EnableDefaultColorProfileResourceDB(XtDisplay(window->mainWin));
}

static void setLangModeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    WindowInfo *window = WidgetToWindow(MENU_WIDGET(w));
    char *params[1];
    void *mode;
    
    if (!XmToggleButtonGetState(w))
    	return;
    	
    /* get name of language mode stored in userData field of menu item */
    XtVaGetValues(w, XmNuserData, &mode, NULL);
    
    /* If the mode didn't change, do nothing */
    if (window->languageMode == (int)(intptr_t)mode)
    	return;
    
    /* redo syntax highlighting word delimiters, etc. */
/*
    reapplyLanguageMode(window, (int)mode, False);
*/
    params[0] = (((int)(intptr_t)mode) == PLAIN_LANGUAGE_MODE) ? "" : LanguageModes[(int)(intptr_t)mode]->name;
    XtCallActionProc(window->textArea, "set_language_mode", NULL, params, 1);
}

void CreateColorProfilesSubMenu(WindowInfo *window, const Widget parent,
        const char *name, const char *label, char mnemonic)
{
    XmString string = XmStringCreateSimple((char*) label);

    window->colorProfileMenuPane = XtVaCreateManagedWidget(name,
            xmCascadeButtonGadgetClass, parent,
            XmNlabelString, string,
            XmNmnemonic, mnemonic,
            XmNsubMenuId, NULL,
            NULL);
    XmStringFree(string);
    
    updateColorProfilesMenu(window);
}

static void setColorProfileCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (!XmToggleButtonGetState(w)) {
        return;
    }
    
    WindowInfo *window = clientData;
    ColorProfile *cp;
    XtVaGetValues(w, XmNuserData, &cp, NULL);
    if(!cp) {
        return;
    }
    
    ColorProfile *currentCP = window->colorProfile;  
    
    if(cp == currentCP) {
        return;
    }
    
    int updateRes = !ColorProfileResourceDBEqual(currentCP, cp);
    
    LoadColorProfile(window->textArea, cp);
    if(updateRes) {
        XrmSetDatabase(XtDisplay(window->shell), cp->db);
    }
    
    SetDefResDBEnable(False);
    
    Boolean updateMenuBar = True;
    Widget shell = window->shell;
    for (WindowInfo *win = WindowList; win != NULL; win = win->next) {
        if(win->shell == shell) { 
            SetColorProfile(win, cp);
            if(updateRes) {
                ReloadWindowResources(win, updateMenuBar);
                updateMenuBar = False;
            }
        }
    }
    
    SetWindowGtkThemeVariant(XtDisplay(shell), XtWindow(shell), cp->windowThemeVariant);
    
    SetDefResDBEnable(True);
}

#ifdef ENABLE_COLORPROFILES
static void updateColorProfilesMenu(WindowInfo *window)
{
    XmString s1;
    Widget menu;
    Arg args[1] = {{XmNradioBehavior, (XtArgVal)True}};
    
    /* Destroy and re-create the menu pane */
    XtVaGetValues(window->colorProfileMenuPane, XmNsubMenuId, &menu, NULL);
    if (menu) {
        XtDestroyWidget(menu);
    }
    menu = CreatePulldownMenu(XtParent(window->colorProfileMenuPane), "colorProfiles", args, 1);
    
    int count = 0;
    ColorProfile *cp = colorProfiles;
    while(cp) {
        Widget menuItem = XtVaCreateManagedWidget("colorProfileMenuItem",
            	xmToggleButtonGadgetClass, menu, 
            	XmNlabelString, s1=XmStringCreateSimple(cp->name),
   		XmNuserData, (void *)cp,
    		XmNset, window->colorProfile == cp, NULL);
        XmStringFree(s1);
	XtAddCallback(menuItem, XmNvalueChangedCallback, setColorProfileCB, window);
        
        cp = cp->next;
        count++;
    }
    
    XtVaSetValues(window->colorProfileMenuPane, XmNsubMenuId, menu, NULL);
    if(count > 1) {
        XtManageChild(window->colorProfileMenuPane);
    } else {
        XtUnmanageChild(window->colorProfileMenuPane);
    }
}
#endif

/*
** Skip a delimiter and it's surrounding whitespace
*/
int SkipDelimiter(char **inPtr, char **errMsg)
{
    *inPtr += strspn(*inPtr, " \t");
    if (**inPtr != ':') {
    	*errMsg = "syntax error";
    	return False;
    }
    (*inPtr)++;
    *inPtr += strspn(*inPtr, " \t");
    return True;
}

/*
** Skip an optional separator and its surrounding whitespace
** return true if delimiter found
*/
int SkipOptSeparator(char separator, char **inPtr)
{
    *inPtr += strspn(*inPtr, " \t");
    if (**inPtr != separator) {
    	return False;
    }
    (*inPtr)++;
    *inPtr += strspn(*inPtr, " \t");
    return True;
}

/*
** Short-hand error processing for language mode parsing errors, frees
** lm (if non-null), prints a formatted message explaining where the
** error is, and returns False;
*/
static int modeError(languageModeRec *lm, const char *stringStart,
        const char *stoppedAt, const char *message)
{
    if (lm != NULL)
    	freeLanguageModeRec(lm);
    return ParseError(NULL, stringStart, stoppedAt,
    	    "language mode specification", message);
}

/*
** Report parsing errors in resource strings or macros, formatted nicely so
** the user can tell where things became botched.  Errors can be sent either
** to stderr, or displayed in a dialog.  For stderr, pass toDialog as NULL.
** For a dialog, pass the dialog parent in toDialog.
*/
int ParseError(Widget toDialog, const char *stringStart, const char *stoppedAt,
	const char *errorIn, const char *message)
{
    int len, nNonWhite = 0;
    const char *c;
    char *errorLine;
    
    for (c=stoppedAt; c>=stringStart; c--) {
    	if (c == stringStart)
    	    break;
    	else if (*c == '\n' && nNonWhite >= 5)
    	    break;
    	else if (*c != ' ' && *c != '\t')
    	    nNonWhite++;
    }
    len = stoppedAt - c + (*stoppedAt == '\0' ? 0 : 1);
    errorLine = (char*)NEditMalloc(len+4);
    strncpy(errorLine, c, len);
    errorLine[len++] = '<';
    errorLine[len++] = '=';
    errorLine[len++] = '=';
    errorLine[len] = '\0';
    if (toDialog == NULL)
    {
        fprintf(stderr, "XNEdit: %s in %s:\n%s\n", message, errorIn, errorLine);
    } else
    {
        DialogF(DF_WARN, toDialog, 1, "Parse Error", "%s in %s:\n%s", "OK",
                message, errorIn, errorLine);
    }
    NEditFree(errorLine);
    return False;
}

/*
** Compare two strings which may be NULL
*/
int AllocatedStringsDiffer(const char *s1, const char *s2)
{
    if (s1 == NULL && s2 == NULL)
    	return False;
    if (s1 == NULL || s2 == NULL)
    	return True;
    return strcmp(s1, s2);
}

static void updatePatternsTo5dot1(void)
{
    const char *htmlDefaultExpr = "^[ \t]*HTML[ \t]*:[ \t]*Default[ \t]*$";
    const char *vhdlAnchorExpr = "^[ \t]*VHDL:";
    
    /* Add new patterns if there aren't already existing patterns with
       the same name.  If possible, insert before VHDL in language mode
       list.  If not, just add to end */
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*PostScript:"))
	spliceString(&TempStringPrefs.highlight, "PostScript:Default",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.language, "^[ \t]*PostScript:"))
	spliceString(&TempStringPrefs.language,
		"PostScript:.ps .PS .eps .EPS .epsf .epsi::::::",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*Lex:"))
	spliceString(&TempStringPrefs.highlight, "Lex:Default",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.language, "^[ \t]*Lex:"))
	spliceString(&TempStringPrefs.language, "Lex:.lex::::::",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*SQL:"))
	spliceString(&TempStringPrefs.highlight, "SQL:Default",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.language, "^[ \t]*SQL:"))
	spliceString(&TempStringPrefs.language, "SQL:.sql::::::",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*Matlab:"))
	spliceString(&TempStringPrefs.highlight, "Matlab:Default",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.language, "^[ \t]*Matlab:"))
	spliceString(&TempStringPrefs.language, "Matlab:..m .oct .sci::::::",
		vhdlAnchorExpr);
    if (!regexFind(TempStringPrefs.smartIndent, "^[ \t]*Matlab:"))
	spliceString(&TempStringPrefs.smartIndent, "Matlab:Default", NULL);
    if (!regexFind(TempStringPrefs.styles, "^[ \t]*Label:"))
	spliceString(&TempStringPrefs.styles, "Label:red:Italic",
		"^[ \t]*Flag:");
    if (!regexFind(TempStringPrefs.styles, "^[ \t]*Storage Type1:"))
	spliceString(&TempStringPrefs.styles, "Storage Type1:saddle brown:Bold",
		"^[ \t]*String:");

    /* Replace html pattern with sgml html pattern, as long as there
       isn't an existing html pattern which will be overwritten */
    if (regexFind(TempStringPrefs.highlight, htmlDefaultExpr)) {
	regexReplace(&TempStringPrefs.highlight, htmlDefaultExpr,
	    	"SGML HTML:Default");
    	if (!regexReplace(&TempStringPrefs.language, "^[ \t]*HTML:.*$",
	    	"SGML HTML:.sgml .sgm .html .htm:\"\\<(?ihtml)\\>\":::::\n")) {
	    spliceString(&TempStringPrefs.language,
		    "SGML HTML:.sgml .sgm .html .htm:\"\\<(?ihtml)\\>\":::::\n",
		    vhdlAnchorExpr);
	}
    }
}

static void updatePatternsTo6dot1(void)
{
    const char *markdownLm6dot1 =
        "Markdown:.md .markdown .mdtxt .mdtext:::::::";
    const char *markdownHl6dot1 = "Markdown:Default";
    
    const char *emphStyle = "Emphasis:black:Italic";
    const char *strongStyle = "Strong:black:Bold";
    const char *hdrStyle = "Header:brown:Bold";
    
    /* Add new patterns if there aren't already existing patterns with
       the same name. */
    if (!regexFind(TempStringPrefs.language, "^[ \t]*Markdown:"))
	spliceString(&TempStringPrefs.language, markdownLm6dot1, "^[ \t]*Matlab:");
    
    /* Enable default highlighting patterns for these modes, unless already
       present */
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*Markdown:"))
	spliceString(&TempStringPrefs.highlight, markdownHl6dot1, "^[ \t]*Matlab:");
    
    /* Add new styles */
    if (!regexFind(TempStringPrefs.styles, "^[ \t]*Emphasis:"))
	spliceString(&TempStringPrefs.styles, emphStyle, NULL);
    if (!regexFind(TempStringPrefs.styles, "^[ \t]*Strong:"))
	spliceString(&TempStringPrefs.styles, strongStyle, "^[ \t]*Emphasis:");
    if (!regexFind(TempStringPrefs.styles, "^[ \t]*Header:"))
	spliceString(&TempStringPrefs.styles, hdrStyle, "^[ \t]*Strong:");
}

static void updatePatternsTo6dot2(void) {
    const char *luaLm6dot2 = "Lua:.lua:::::::";
    const char *luaHl6dot2 = "Lua:Default";
    
    const char *ocamlLm6dot2 = "OCaml:.ml::Auto:None:8:2:\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":";
    const char *ocamlHl6dot2 = "OCaml:Default";
    
    const char *stylePre26dot2 = "Preprocessor2:rebecca purple:Bold";
    
    /* Add new patterns if there aren't already existing patterns with
       the same name. */
    if (!regexFind(TempStringPrefs.language, "^[ \t]*Lua:"))
	spliceString(&TempStringPrefs.language, luaLm6dot2, "^[ \t]*Makefile:");
    
    if (!regexFind(TempStringPrefs.language, "^[ \t]*OCaml:"))
	spliceString(&TempStringPrefs.language, ocamlLm6dot2, "^[ \t]*Pascal:");
    
    /* Enable default highlighting patterns for these modes, unless already
       present */
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*Lua:"))
	spliceString(&TempStringPrefs.highlight, luaHl6dot2, "^[ \t]*Makefile:");
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*OCaml:"))
	spliceString(&TempStringPrefs.highlight, ocamlHl6dot2, "^[ \t]*Pascal:");
    
    /* Add new styles */
    if (!regexFind(TempStringPrefs.styles, "^[ \t]*Preprocessor2:"))
	spliceString(&TempStringPrefs.styles, stylePre26dot2, "^[ \t]*Character Const:");
}

static void updatePatternsTo6dot3(void) {
    const char *dockerLm6dot3 = "Dockerfile::::::::";
    const char *dockerHl6dot3 = "Dockerfile:Default";
    
    // Add new patterns if there aren't already existing patterns with
    // the same name.
    if (!regexFind(TempStringPrefs.language, "^[ \t]*Dockerfile:")) {
        spliceString(&TempStringPrefs.language, dockerLm6dot3, "^[ \t]*Fortran:");
    }
    
    // Enable default highlighting patterns for these modes, unless already
    // present
    if (!regexFind(TempStringPrefs.highlight, "^[ \t]*Dockerfile:")) {
	spliceString(&TempStringPrefs.highlight, dockerHl6dot3, "^[ \t]*Fortran:");
    }
}

/* 
 * We migrate a color from the X resources to the prefs if:
 *      1.  The prefs entry is equal to the default entry
 *      2.  The X resource is not equal to the default entry
 */
static void migrateColor(XrmDatabase prefDB, XrmDatabase appDB,
        char *class, char *name, int color_index, char *default_val)
{
    char *type, *valueString;
    XrmValue rsrcValue;
    
    /* If this color has been customized in the color dialog then use
        that value */
    if ( strcmp(default_val, PrefData.colorNames[color_index]) )
        return;
    
    /* Retrieve the value of the resource from the DB */
    if (XrmGetResource(prefDB, name, class, &type, &rsrcValue)) {
        if (strcmp(type, XmRString)) {
            fprintf(stderr,"Internal Error: Unexpected resource type, %s\n",
                    type);
            return;
        }
        valueString = rsrcValue.addr;
    } else if (XrmGetResource(appDB, name, class, &type, &rsrcValue)) {
        if (strcmp(type, XmRString)) {
            fprintf(stderr,"Internal Error: Unexpected resource type, %s\n",
                    type);
            return;
        }
        valueString = rsrcValue.addr;
    } else
        /* No resources set */
        return;
    
    /* An X resource is set.  If it's non-default, update the prefs. */
    if ( strcmp(valueString, default_val) ) {
        strncpy(PrefData.colorNames[color_index], valueString, 
                MAX_COLOR_LEN);
    }
}

/*
 * In 5.4 we moved color preferences from X resources to a color dialog,
 * meaning they're in the normal prefs system.  Users who have customized
 * their colors with X resources would probably prefer not to have to redo
 * the customization in the dialog, so we migrate them to the prefs for them.  
 */
static void migrateColorResources(XrmDatabase prefDB, XrmDatabase appDB)
{
    char classbuf[512];
    char namebuf[512];
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.Foreground");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.foreground");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, TEXT_FG_COLOR, 
            NEDIT_DEFAULT_FG);
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.Background");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.background");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, TEXT_BG_COLOR, 
            NEDIT_DEFAULT_TEXT_BG);
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.SelectForeground");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.selectForeground");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, SELECT_FG_COLOR, 
            NEDIT_DEFAULT_SEL_FG);
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.SelectBackground");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.selectBackground");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, SELECT_BG_COLOR, 
            NEDIT_DEFAULT_SEL_BG);
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.HighlightForeground");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.highlightForeground");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, HILITE_FG_COLOR, 
            NEDIT_DEFAULT_HI_FG);
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.HighlightBackground");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.highlightBackground");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, HILITE_BG_COLOR, 
            NEDIT_DEFAULT_HI_BG);
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.LineNumForeground");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.lineNumForeground");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, LINENO_FG_COLOR, 
            NEDIT_DEFAULT_LINENO_FG);
    
    snprintf(classbuf, 512, "%s%s", APP_CLASS, ".Text.CursorForeground");
    snprintf(namebuf, 512, "%s%s", APP_NAME, ".text.cursorForeground");
    migrateColor(prefDB, appDB, classbuf,
            namebuf, CURSOR_FG_COLOR, 
            NEDIT_DEFAULT_CURSOR_FG);
}

/*
** Inserts a string into intoString, reallocating it with nedit_realloc.  If
** regular expression atExpr is found, inserts the string before atExpr
** followed by a newline.  If atExpr is not found, inserts insertString
** at the end, PRECEDED by a newline.
*/
static void spliceString(char **intoString, const char *insertString, const char *atExpr)
{
    int beginPos, endPos;
    int intoLen = strlen(*intoString);
    int insertLen = strlen(insertString);
    char *newString = (char*)NEditMalloc(intoLen + insertLen + 2);
    
    if (atExpr != NULL && SearchString(*intoString, atExpr,
	    SEARCH_FORWARD, SEARCH_REGEX, False, 0, &beginPos, &endPos,
	    NULL, NULL, NULL)) {
	strncpy(newString, *intoString, beginPos);
    	strncpy(&newString[beginPos], insertString, insertLen);
	newString[beginPos+insertLen] = '\n';
	strncpy(&newString[beginPos+insertLen+1],
		&((*intoString)[beginPos]), intoLen - beginPos);
    } else {
	strncpy(newString, *intoString, intoLen);
	newString[intoLen] = '\n';
	strncpy(&newString[intoLen+1], insertString, insertLen);
    }
    newString[intoLen + insertLen + 1] = '\0';
    NEditFree(*intoString);
    *intoString = newString;
}

/*
** Simplified regular expression search routine which just returns true
** or false depending on whether inString matches expr
*/
static int regexFind(const char *inString, const char *expr)
{
    int beginPos, endPos;
    return SearchString(inString, expr, SEARCH_FORWARD, SEARCH_REGEX, False,
	    0, &beginPos, &endPos, NULL, NULL, NULL);
}

/*
** Simplified case-sensisitive string search routine which just 
** returns true or false depending on whether inString matches expr
*/
static int caseFind(const char *inString, const char *expr)
{
    int beginPos, endPos;
    return SearchString(inString, expr, SEARCH_FORWARD, SEARCH_CASE_SENSE,
            False, 0, &beginPos, &endPos, NULL, NULL, NULL);
}

/*
** Common implementation for simplified string replacement routines.
*/
static int stringReplace(char **inString, const char *expr, 
                         const char *replaceWith, int searchType,
			 int replaceLen)
{
    int beginPos, endPos, newLen;
    char *newString;
    int inLen = strlen(*inString);
    if (0 >= replaceLen) replaceLen = strlen(replaceWith);
    if (!SearchString(*inString, expr, SEARCH_FORWARD, searchType, False,
	    0, &beginPos, &endPos, NULL, NULL, NULL))
	return FALSE;
    newLen = inLen + replaceLen - (endPos-beginPos);
    newString = (char*)NEditMalloc(newLen + 1);
    strncpy(newString, *inString, beginPos);
    strncpy(&newString[beginPos], replaceWith, replaceLen);
    strncpy(&newString[beginPos+replaceLen],
	    &((*inString)[endPos]), inLen - endPos);
    newString[newLen] = '\0';
    NEditFree(*inString);
    *inString = newString;
    return TRUE;
}

/*
** Simplified regular expression replacement routine which replaces the
** first occurence of expr in inString with replaceWith, reallocating
** inString with NEditMalloc.  If expr is not found, does nothing and
** returns false.
*/
static int regexReplace(char **inString, const char *expr, 
                        const char *replaceWith)
{
    return stringReplace(inString, expr, replaceWith, SEARCH_REGEX, -1);
}

/*
** Simplified case-sensisitive string replacement routine which 
** replaces the first occurence of expr in inString with replaceWith,
** reallocating inString with NEditMalloc.  If expr is not found, does nothing 
** and returns false.
*/
static int caseReplace(char **inString, const char *expr, 
                       const char *replaceWith, int replaceLen)
{
    return stringReplace(inString, expr, replaceWith, SEARCH_CASE_SENSE,
                         replaceLen);
}

/*
** Looks for a (case-sensitive literal) match of an old macro text in a
** temporary macro commands buffer. If the text is found, it is replaced by
** a substring of the default macros, bounded by a given start and end pattern
** (inclusive). Returns the length of the replacement.
*/
static int replaceMacroIfUnchanged(const char* oldText, const char* newStart, 
                                   const char* newEnd)
{
    if (caseFind(TempStringPrefs.macroCmds, oldText)) {
        const char *start = strstr(PrefDescrip[2].defaultString, newStart);
	if (start) {
            const char *end = strstr(start, newEnd);
            if (end) {
                int length = (int)(end-start) + strlen(newEnd);
                caseReplace(&TempStringPrefs.macroCmds, oldText, start, length);
                return length;
            }
	}
    }
    return 0;
}

#ifdef SGI_CUSTOM
/*
** Present the user a dialog for specifying whether or not a short
** menu mode preference should be applied toward the default setting.
** Return False (function value) if operation was canceled, return True
** in setDefault if requested to reset the default value.
*/
static int shortPrefToDefault(Widget parent, const char *settingName, int *setDefault)
{
    char msg[100] = "";
    
    if (!GetPrefShortMenus()) {
        *setDefault = False;
        return True;
    }
    
    sprintf(msg, "%s\nSave as default for future windows as well?", settingName);
    switch (DialogF (DF_QUES, parent, 3, "Save Default", msg, "Yes", "No",
            "Cancel"))
    {
        case 1: /* yes */
            *setDefault = True;
            return True;
        case 2: /* no */
            *setDefault = False;
            return True;
        case 3: /* cancel */
            return False;
    }
    return False; /* not reached */
}
#endif

/* Decref the default calltips file(s) for this window */
void UnloadLanguageModeTipsFile(WindowInfo *window)
{
    int mode;
    
    mode = window->languageMode;
    if (mode != PLAIN_LANGUAGE_MODE && LanguageModes[mode]->defTipsFile) {
        DeleteTagsFile( LanguageModes[mode]->defTipsFile, TIP, False );
    }
}

/******************************************************************************
 * The Color selection dialog
 ******************************************************************************/

/* 
There are 8 colors:   And 8 indices:
textFg                TEXT_FG_COLOR
textBg                TEXT_BG_COLOR
selectFg              SELECT_FG_COLOR
selectBg              SELECT_BG_COLOR
hiliteFg              HILITE_FG_COLOR
hiliteBg              HILITE_BG_COLOR
lineNoFg              LINENO_FG_COLOR
cursorFg              CURSOR_FG_COLOR
*/

#define MARGIN_SPACING 10

/* 
 * Callbacks for field modifications
 */
static void cgTextModifiedCB(Widget w, XtPointer clientData,
      XtPointer callData)
{
    colorDialog *cd = (colorDialog *)clientData;
    Widget err = NULL;
    XtVaGetValues(w, XmNuserData, &err, NULL);
    showColorStatus(cd, w, err);
}

/* 
 * Helper functions for validating colors
 */
static int verifyAllColors(colorDialog *cd)
{
    /* Maybe just check for empty strings in error widgets instead? */
    return (checkColorStatus(cd, cd->textFgW) &&
            checkColorStatus(cd, cd->textBgW) &&
            checkColorStatus(cd, cd->selectFgW) &&
            checkColorStatus(cd, cd->selectBgW) &&
            checkColorStatus(cd, cd->hiliteFgW) &&
            checkColorStatus(cd, cd->hiliteBgW) &&
            checkColorStatus(cd, cd->lineNoFgW) &&
            checkColorStatus(cd, cd->cursorFgW) );
}

/* Returns True if the color is valid, False if it's not */
static Boolean checkColorStatus(colorDialog *cd, Widget colorFieldW)
{
    Colormap cMap;
    XColor colorDef;
    Status status;
    Display *display = XtDisplay(cd->shell);
    char *text = XmTextGetString(colorFieldW);
    XtVaGetValues(cd->shell, XtNcolormap, &cMap, NULL);
    status = XParseColor(display, cMap, text, &colorDef);
    NEditFree(text);
    return (status != 0);
}

/* Show or hide errorLabelW depending on whether or not colorFieldW 
    contains a valid color name. */
static void showColorStatus(colorDialog *cd, Widget colorFieldW, 
        Widget errorLabelW)
{
    /* Should set the OK/Apply button sensitivity here, instead 
       of leaving is sensitive and then complaining if an error. */
    XtSetMappedWhenManaged( errorLabelW, !checkColorStatus(cd, colorFieldW) );
}

// Remove profiles that are flagged as removed and return a list of all
// removed color profiles
static ColorProfile* clearColorProfileList(ColorProfile *cpList)
{
    ColorProfile *removed_begin = NULL;
    ColorProfile *removed_end = NULL;
    
    // the first element can never be removed
    // start with the second element
    ColorProfile *prevElm = cpList;
    ColorProfile *elm = cpList->next;
    while(elm) {
        if(elm->removed) {
            prevElm->next = elm->next;
            if(removed_end) {
                removed_end->next = elm;
                removed_end = elm;
            } else {
                removed_begin = elm;
                removed_end = elm;
            }
        } else {
            prevElm = elm;
        }
        elm = elm->next;
    }
    
    return removed_begin;
}

/* Update the colors in the window or in the preferences */
static void updateColors(colorDialog *cd)
{
    WindowInfo *window;
    
    saveColorProfileSettings(cd);
    
    ColorProfile *setProfile = NULL;
    
    // update color profiles
    ColorProfile *first = NULL;
    ColorProfile *prev = NULL;
    for(int i=0;i<cd->numColorProfiles;i++) {
        ColorProfile *profile = &cd->colorProfiles[i];
        ColorProfile *prefProfile = profile->orig;
        if(!prefProfile) {
            prefProfile = NEditMalloc(sizeof(ColorProfile));
            memset(prefProfile, 0, sizeof(ColorProfile));
        }
        
        if(!ColorProfileResourceDBEqual(prefProfile, profile)) {
            prefProfile->resDBLoaded = FALSE;
        }
        
        NEditFree(prefProfile->name);
        NEditFree(prefProfile->textFg);
        NEditFree(prefProfile->textBg);
        NEditFree(prefProfile->selectFg);
        NEditFree(prefProfile->selectBg);
        NEditFree(prefProfile->hiliteFg);
        NEditFree(prefProfile->hiliteBg);
        NEditFree(prefProfile->lineNoFg);
        NEditFree(prefProfile->lineNoBg);
        NEditFree(prefProfile->cursorFg);
        NEditFree(prefProfile->lineHiBg);
        NEditFree(prefProfile->ansiColorList);
        NEditFree(prefProfile->rainbowColorList);
        NEditFree(prefProfile->resourceFile);

        prefProfile->name = NEditStrdup(profile->name);
        prefProfile->textFg = NEditStrdup(profile->textFg);
        prefProfile->textBg = NEditStrdup(profile->textBg);
        prefProfile->selectFg = NEditStrdup(profile->selectFg);
        prefProfile->selectBg = NEditStrdup(profile->selectBg);
        prefProfile->hiliteFg = NEditStrdup(profile->hiliteFg);
        prefProfile->hiliteBg = NEditStrdup(profile->hiliteBg);
        prefProfile->lineNoFg = NEditStrdup(profile->lineNoFg);
        prefProfile->lineNoBg = NEditStrdup(profile->lineNoBg);
        prefProfile->cursorFg = NEditStrdup(profile->cursorFg);
        prefProfile->lineHiBg = NEditStrdup(profile->lineHiBg);
        prefProfile->ansiColorList = NEditStrdup(profile->ansiColorList);
        prefProfile->rainbowColorList = NEditStrdup(profile->rainbowColorList);
        prefProfile->resourceFile = profile->resourceFile ? NEditStrdup(profile->resourceFile) : NULL;
        prefProfile->styleType = profile->styleType;
        prefProfile->windowThemeVariant = profile->windowThemeVariant;

        prefProfile->colorsLoaded = FALSE;
               
        if(i == cd->selectedProfile) {
            setProfile = prefProfile;
        }

        if(prev) {
            prev->next = prefProfile;
        } else {
            first = prefProfile;
        }
        prev = prefProfile;
    }
    
    // update default profile
    ColorProfile defaultProfile = cd->colorProfiles[0];
    SetPrefColorName(TEXT_FG_COLOR  , defaultProfile.textFg  );
    SetPrefColorName(TEXT_BG_COLOR  , defaultProfile.textBg  );
    SetPrefColorName(SELECT_FG_COLOR, defaultProfile.selectFg);
    SetPrefColorName(SELECT_BG_COLOR, defaultProfile.selectBg);
    SetPrefColorName(HILITE_FG_COLOR, defaultProfile.hiliteFg);
    SetPrefColorName(HILITE_BG_COLOR, defaultProfile.hiliteBg);
    SetPrefColorName(LINENO_FG_COLOR, defaultProfile.lineNoFg);
    SetPrefColorName(LINENO_BG_COLOR, defaultProfile.lineNoBg);
    SetPrefColorName(CURSOR_FG_COLOR, defaultProfile.cursorFg);
    SetPrefColorName(CURSOR_LINE_BG_COLOR, defaultProfile.lineHiBg);
    SetPrefIndentRainbowColors(defaultProfile.rainbowColorList);
    SetPrefAnsiColorList(defaultProfile.ansiColorList); 
    
    // TODO: free previous color profiles that are not in use
    
    colorProfiles = first;
    
    //ColorProfile *removed = clearColorProfileList(colorProfiles);
    
    if(!setProfile) {
        // should not happen but this makes it extra safe
        setProfile = first;
    }
    
    SetPrefDefaultColorProfileName(setProfile->name);
    defaultColorProfile = setProfile;
    
    SetDefResDBEnable(False);
    
    // update windows
    for (window = WindowList; window != NULL; window = window->next) {
        ColorProfile *cp = window->colorProfile;  
        SetColorProfile(window, setProfile);
        XrmSetDatabase(XtDisplay(window->shell), setProfile->db);
        if(!ColorProfileResourceDBEqual(cp, setProfile)) {
            ReloadWindowResources(window, True);
        }
        
        updateColorProfilesMenu(window);
    }
    
    SetDefResDBEnable(True);
}


/* 
 * Dialog button callbacks
 */

static void colorDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = (colorDialog *)clientData;

    cd->window->colorDialog = NULL;
    
    // TODO: free stuff
    NEditFree(cd);
}

static void colorOkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = (colorDialog *)clientData;
    
    if(!verifyAllColors(cd))
    {
        DialogF(DF_ERR, w, 1, "Invalid Colors",
                "All colors must be valid to proceed.", "OK");
        return;
    }
    updateColors(cd);

    /* pop down and destroy the dialog */
    XtDestroyWidget(cd->shell);
}

static void colorApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = (colorDialog *)clientData;
    
    if(!verifyAllColors(cd))
    {
        DialogF(DF_ERR, w, 1, "Invalid Colors",
                "All colors must be valid to be applied.", "OK");
        return;
    }
    updateColors(cd);
}

static void colorCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = (colorDialog *)clientData;

    /* pop down and destroy the dialog */
    XtDestroyWidget(cd->shell);
}

static void updateCGchooser(Widget w, XtPointer clientData,
      XtPointer callData)
{
    Widget button = clientData;
    char *str = XmTextGetString(w);
    int dummy;
    SetParseColorError(0);
    Pixel color = AllocColor(button, str, &dummy, &dummy, &dummy);
    SetParseColorError(1);
    XtVaSetValues(button, XmNbackground, color, NULL);
    XtFree(str);
}

static void selectColorCB(Widget w, XtPointer clientData, XtPointer callData)
{
    Colormap cm;
    Pixel bg;
    Widget textfield = NULL;
    XtVaGetValues(w, XtNcolormap, &cm, XmNbackground, &bg, XmNuserData, &textfield, NULL);
    
    if(!textfield) return;
    
    XColor xcolor;
    memset(&xcolor, 0, sizeof(XColor));
    xcolor.pixel = bg;
    XQueryColor(XtDisplay(w), cm, &xcolor);
    
    int r = xcolor.red;
    int g = xcolor.green;
    int b = xcolor.blue;
    if(ColorChooser(XtParent(w), &r, &g, &b)) {
        r /= 257;
        g /= 257;
        b /= 257;
        char buf[32];
        snprintf(buf, 32, "#%02x%02x%02x", r, g, b);
        // alternative format
        //snprintf(buf, 32, "rgb:%02x/%02x/%02x", r, g, b);
        XmTextSetString(textfield, buf);
    }

}

/* Add a label, error label, and text entry label with a validation
    callback */
static Widget addColorGroup( Widget parent, const char *name, char mnemonic, 
        char *label, Widget *fieldW, Widget *errW, Widget topWidget, 
        int leftPos, int rightPos, 
        colorDialog *cd )
{
    Widget lblW;
    char *longerName;
    XmString s1;
    int nameLen = strlen(name);
       
    /* The label widget */
    longerName = (char*)NEditMalloc(nameLen+7);
    strcpy(longerName, name);
    strcat(longerName, "Lbl");
    lblW = XtVaCreateManagedWidget(longerName,
          xmLabelGadgetClass, parent,
          XmNlabelString, s1=XmStringCreateSimple( label ),
          XmNmnemonic, mnemonic,
          XmNtopAttachment, XmATTACH_WIDGET,
          XmNtopWidget, topWidget,
          XmNtopOffset, MARGIN_SPACING,
          XmNleftAttachment, XmATTACH_POSITION,
          XmNleftPosition, leftPos, NULL);
    XmStringFree(s1);
    
    /* The error label widget */
    strcpy(&(longerName[nameLen]), "ErrLbl");
    *errW = XtVaCreateManagedWidget(longerName,
          xmLabelWidgetClass, parent,
          XmNlabelString, s1=XmStringCreateSimple("(Invalid!)"),
          XmNalignment, XmALIGNMENT_END,
          XmNtopAttachment, XmATTACH_WIDGET,
          XmNtopWidget, topWidget,
          XmNtopOffset, MARGIN_SPACING,
          XmNleftAttachment, XmATTACH_WIDGET,
          XmNleftWidget, lblW,
          XmNrightAttachment, XmATTACH_POSITION,
          XmNrightPosition, rightPos, NULL);
    XmStringFree(s1);
    
    /* The text field entry widget */
    Widget colorChooserButton = XtVaCreateManagedWidget("colorChooser", xmPushButtonWidgetClass, parent,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, rightPos,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, lblW,
            XmNbackground, 0,
            XmNlabelString, s1 = XmStringCreateSimple("    "),
            NULL);
    XmStringFree(s1);
    XtAddCallback(colorChooserButton, XmNactivateCallback, selectColorCB, NULL);
     
    *fieldW = XtVaCreateManagedWidget(name, xmTextWidgetClass,
          parent,
          XmNcolumns, 12,
          XmNmaxLength, MAX_COLOR_LEN-1,
          XmNleftAttachment, XmATTACH_POSITION,
          XmNleftPosition, leftPos,
          XmNrightAttachment, XmATTACH_WIDGET,
          XmNrightWidget, colorChooserButton,
          XmNtopAttachment, XmATTACH_WIDGET,
          XmNtopWidget, lblW, 
          XmNuserData, *errW, NULL);
      
    Dimension shadowThickness = 1;
    XtVaGetValues(*fieldW, XmNshadowThickness, &shadowThickness, NULL);
    XtVaSetValues(colorChooserButton,
#if XmVersion > 2001 
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
#endif
            XmNbottomWidget, *fieldW,
            XmNshadowThickness, shadowThickness,
            XmNuserData, *fieldW, NULL);
    
    RemapDeleteKey(*fieldW);
    XtAddCallback(*fieldW, XmNvalueChangedCallback,
          cgTextModifiedCB, cd);
    XtAddCallback(*fieldW, XmNvalueChangedCallback,
          updateCGchooser, colorChooserButton);
    XtVaSetValues(lblW, XmNuserData, *fieldW, NULL);
    
    NEditFree(longerName);
    return *fieldW;
}

static void selectColorTab(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = clientData;
    
    if(!XmToggleButtonGetState(w)) {
        XmToggleButtonSetState(w, True, False);
        return;
    }
    
#ifdef DISABLE_COLORPROFILES
    int numtabs = 3;
#else
    int numtabs = 4;
#endif
    for(int i=0;i<numtabs;i++) {
        if(cd->tabs[i] != w) {
            XmToggleButtonSetState(cd->tabs[i], False, False);
            XtUnmanageChild(cd->tabForms[i]);
        } else {
            XtManageChild(cd->tabForms[i]);
        }
    }
}

static int irFindIndex(colorDialog *cd, Widget button, int type)
{
    for(int i=0;i<cd->numIndentRainbowColors;i++) {
        switch(type) {
            case 0: if(cd->indentRainbowColors[i].remove == button) return i;
            case 1: if(cd->indentRainbowColors[i].up == button) return i;
            case 2: if(cd->indentRainbowColors[i].down == button) return i;
        }
    }
    return -1;
}

#define IR_BTN_MARGIN 6

static void addRainbowColor(colorDialog *cd, const char *value);

static void irAdd(Widget w, XtPointer clientData, XtPointer callData)
{
    addRainbowColor(clientData, "#ffffff");
}

static void addAddBtn(colorDialog *cd)
{
    if(cd->addBtn) {
        XtUnmanageChild(cd->addBtn);
        XtDestroyWidget(cd->addBtn);
    }
    
    XmString s1 = XmStringCreateSimple("Add");
    Arg args[8];
    int n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, IR_BTN_MARGIN); n++;
    XtSetArg(args[n], XmNtopOffset, IR_BTN_MARGIN); n++;
    if(cd->numIndentRainbowColors == 0) {
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    } else {
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNtopWidget, cd->indentRainbowColors[cd->numIndentRainbowColors-1].remove); n++;
    }
    XtSetArg(args[n], XmNlabelString, s1); n++;
    
    cd->addBtn = XmCreatePushButton(cd->btnForm, "irAdd", args, n);
    XtManageChild(cd->addBtn);
    XtAddCallback(cd->addBtn, XmNactivateCallback, irAdd, cd);
    XmStringFree(s1); 
}


static void irColorRemove(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = clientData;
    int i = irFindIndex(cd, w, 0);
    if(i == -1) return;
    
    
    indentRainbowColor c = cd->indentRainbowColors[i]; 
    if(i == 0) {
        indentRainbowColor n = cd->indentRainbowColors[i+1];
        XtVaSetValues(n.remove, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.up, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.down, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.textfield, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.colorchooser, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, IR_BTN_MARGIN, NULL);
        
        if(cd->numIndentRainbowColors > 1) {
            memmove(cd->indentRainbowColors, cd->indentRainbowColors + 1, (cd->numIndentRainbowColors-1)*sizeof(indentRainbowColor));
        }
    } else if(i+1 != cd->numIndentRainbowColors) {
        indentRainbowColor p = cd->indentRainbowColors[i-1];
        indentRainbowColor n = cd->indentRainbowColors[i+1];
        XtVaSetValues(n.remove, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, p.remove, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.up, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, p.remove, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.down, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, p.remove, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.textfield, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, p.remove, XmNtopOffset, IR_BTN_MARGIN, NULL);
        XtVaSetValues(n.colorchooser, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, p.remove, XmNtopOffset, IR_BTN_MARGIN, NULL);
        memmove(cd->indentRainbowColors + i, cd->indentRainbowColors + i+1, (cd->numIndentRainbowColors-i-1)*sizeof(indentRainbowColor));
    }
    
    XtUnmanageChild(c.remove);
    XtUnmanageChild(c.up);
    XtUnmanageChild(c.down);
    XtUnmanageChild(c.textfield);
    XtUnmanageChild(c.colorchooser);
    XtDestroyWidget(c.remove);
    XtDestroyWidget(c.up);
    XtDestroyWidget(c.down);
    XtDestroyWidget(c.textfield);
    XtDestroyWidget(c.colorchooser);
    
    cd->numIndentRainbowColors--;
    
    addAddBtn(cd);
}

static void irColorUp(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = clientData;
    int i = irFindIndex(cd, w, 1);
    if(i < 1) return;
    
    Widget textfield = cd->indentRainbowColors[i].textfield;
    Widget textfieldPrev = cd->indentRainbowColors[i-1].textfield;
    
    char *prevStr = XmTextGetString(textfieldPrev);
    char *currentStr = XmTextGetString(textfield);
    XmTextSetString(textfieldPrev, currentStr);
    XmTextSetString(textfield, prevStr);
    XtFree(prevStr);
    XtFree(currentStr);
}

static void irColorDown(Widget w, XtPointer clientData, XtPointer callData)
{
    colorDialog *cd = clientData;
    int i = irFindIndex(cd, w, 2);
    if(i == -1 || i +1 >= cd->numIndentRainbowColors) return;
    
    Widget textfield = cd->indentRainbowColors[i].textfield;
    Widget textfieldNext = cd->indentRainbowColors[i+1].textfield;
    
    char *nextStr = XmTextGetString(textfieldNext);
    char *currentStr = XmTextGetString(textfield);
    XmTextSetString(textfieldNext, currentStr);
    XmTextSetString(textfield, nextStr);
    XtFree(nextStr);
    XtFree(currentStr);
}

static void addRainbowColor(colorDialog *cd, const char *value)
{
    XmString s;
    Arg args[20];
    int n = 0;
    
    // "remove" button
    if(cd->numIndentRainbowColors == 0) {
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    } else {
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNtopWidget, cd->indentRainbowColors[cd->numIndentRainbowColors-1].remove); n++;
    }
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, IR_BTN_MARGIN); n++;
    XtSetArg(args[n], XmNleftOffset, IR_BTN_MARGIN); n++;
    s = XmStringCreateSimple(" - ");
    XtSetArg(args[n], XmNlabelString, s); n++;
    Widget remove = XmCreatePushButton(cd->btnForm, "irRm", args, n);
    XmStringFree(s);
    XtManageChild(remove);
    XtAddCallback(remove, XmNactivateCallback, irColorRemove, cd);
    
    Dimension highlightThickness, shadowThickness;
    XtVaGetValues(remove, XmNhighlightThickness, &highlightThickness, XmNshadowThickness, &shadowThickness, NULL);
    
    // "move up" button
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, remove); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, remove); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, remove); n++;
    XtSetArg(args[n], XmNarrowDirection, XmARROW_UP); n++;
    XtSetArg(args[n], XmNhighlightThickness, highlightThickness); n++;
    XtSetArg(args[n], XmNshadowThickness, shadowThickness); n++;
    XtSetArg(args[n], XmNuserData, remove); n++;
    Widget up = XmCreateArrowButton(cd->btnForm, "irUp", args, n);
    XtManageChild(up);
    XtAddCallback(up, XmNactivateCallback, irColorUp, cd);
    
    // "move down" button
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, up); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, remove); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, remove); n++;
    XtSetArg(args[n], XmNarrowDirection, XmARROW_DOWN); n++;
    XtSetArg(args[n], XmNhighlightThickness, highlightThickness); n++;
    XtSetArg(args[n], XmNshadowThickness, shadowThickness); n++;
    XtSetArg(args[n], XmNuserData, remove); n++;
    Widget down = XmCreateArrowButton(cd->btnForm, "irDown", args, n);
    XtManageChild(down);
    XtAddCallback(down, XmNactivateCallback, irColorDown, cd);
    
    // text field
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, down); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, remove); n++;
    XtSetArg(args[n], XmNcolumns, 8); n++;
    Widget textfield = XmCreateText(cd->btnForm, "irText", args, n);
    XtManageChild(textfield);
    
    // Color Chooser
    n = 0;
    s = XmStringCreateSimple("    ");
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, textfield); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, remove); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, remove); n++;
    XtSetArg(args[n], XmNlabelString, s); n++;
    Widget colorChooserButton = XmCreatePushButton(cd->btnForm, "colorChooser", args, n);
    XmStringFree(s);
    XtAddCallback(colorChooserButton, XmNactivateCallback, selectColorCB, NULL);
    XtManageChild(colorChooserButton);
    
    shadowThickness = 1;
    XtVaGetValues(textfield, XmNshadowThickness, &shadowThickness, NULL);
    XtVaSetValues(colorChooserButton,
#if XmVersion > 2001
            XmNshadowThickness, shadowThickness,
#endif
            XmNuserData, textfield, NULL);
    
    // update color chooser when the textfield changes
    XtAddCallback(textfield, XmNvalueChangedCallback,
          updateCGchooser, colorChooserButton);
    
    // make sure the height of all widgets is the same
    XtVaSetValues(remove,
#if XmVersion > 2001
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
#endif
            XmNbottomWidget, textfield,
            XmNuserData, textfield, NULL);
    
    XmTextSetString(textfield, (char*)value);

    
    // add the new 
    if(cd->numIndentRainbowColors >= cd->allocIndentRainbowColors) {
        cd->allocIndentRainbowColors += 8;
        cd->indentRainbowColors = NEditRealloc(cd->indentRainbowColors, cd->allocIndentRainbowColors * sizeof(indentRainbowColor));
    }
    indentRainbowColor lineW;
    lineW.remove = remove;
    lineW.up = up;
    lineW.down = down;
    lineW.textfield = textfield;
    lineW.colorchooser = colorChooserButton;
    cd->indentRainbowColors[cd->numIndentRainbowColors++] = lineW;
    
    // update addBtn
    addAddBtn(cd);
}

static void colorDialogSelectProfile(colorDialog *cd, int index)
{
    cd->selectedProfile = index;
    
    clearRainbowColors(cd);
    
    loadColors(cd);
    loadAnsiColors(cd);
    loadColorProfileStyleSettings(cd);

    indentRainbowDialogLoadColors(cd);
    
    // temporarily change the profile name for the text drawing style dialog
    ColorProfile *cp = &cd->colorProfiles[index];
    SetColorProfileName(cp->name);
    SetColorProfileStyleType(cp->styleType);
}

static void colorDialogProfileSelected(
        Widget w,
        colorDialog *cd,
        XmComboBoxCallbackStruct *cb)
{
    saveColorProfileSettings(cd);
    colorDialogSelectProfile(cd, cb->item_position);
}

static void colorDialogUpdateProfileList(colorDialog *cd)
{
    XmStringTable profileList = NEditCalloc(cd->numColorProfiles, sizeof(XmString));
    for(int i=0;i<cd->numColorProfiles;i++) {
        profileList[i] = XmStringCreateLocalized(cd->colorProfiles[i].name);
    }
    
    XtVaSetValues(cd->profileDropDown, XmNitems, profileList, XmNitemCount, cd->numColorProfiles, NULL);
    XmComboBoxSelectItem(cd->profileDropDown, profileList[cd->selectedProfile]);
    for(int i=0;i<cd->numColorProfiles;i++) {
        XmStringFree(profileList[i]);
    }
    NEditFree(profileList);
}

static Boolean colorDialogCheckProfileName(colorDialog *cd, const char *name)
{
    for(int i=0;i<cd->numColorProfiles;i++) {
        if(!strcmp(cd->colorProfiles[i].name, name)) {
            return False;
        }
    }
    return True;
}

static Boolean colorDialogCheckProfileNameChars(colorDialog *cd, const char *name)
{
    size_t len = strlen(name);
    for(int i=0;i<len;i++) {
        char c = name[i];
        if(c == '\\' || c == ',' || c == ';') {
            DialogF(DF_ERR, cd->shell, 1, "Error",
                    "Character '%c' is not allowed in profile names.", "OK", c);
            return False;
        }
    }
    return True;
}


static void colorDialogProfileRename(Widget w, colorDialog *cd, XtPointer c)
{
    if(cd->selectedProfile == 0) {
        DialogF(DF_ERR, w, 1, "Error",
                "The default color profile cannot be renamed.", "OK");
        return;
    }
    
    char profileName[DF_MAX_PROMPT_LENGTH];
    profileName[0] = 0;
    
    int response = DialogF(
            DF_PROMPT,
            w,
            2,
            "Rename Profile", "New Profile Name:",
            profileName,
            "OK",
            "Cancel");
    
    if(response == 2 || strlen(profileName) == 0) {
        return;
    }
    
    // check profile name
    if(!colorDialogCheckProfileName(cd, profileName)) {
        DialogF(DF_ERR, w, 1, "Error",
                "The profile name '%s' is already in use.", "OK", profileName);
        return;
    }
    if(!colorDialogCheckProfileNameChars(cd, profileName)) {
        return;
    }
    
    ColorProfile *profile = &cd->colorProfiles[cd->selectedProfile];
    
    NEditFree(profile->name);
    profile->name = NEditStrdup(profileName);
    
    colorDialogUpdateProfileList(cd);
}

static void colorDialogProfileRemove(Widget w, colorDialog *cd, XtPointer c)
{
    if(cd->selectedProfile == 0) {
        DialogF(DF_ERR, w, 1, "Error",
                "The default color profile cannot be removed.", "OK");
        return;
    }
    
    ColorProfile *profile = &cd->colorProfiles[cd->selectedProfile];
    if(profile->orig) {
        profile->orig->removed = True;
    }
    ColorProfileFreeContent(profile);
    
    if(cd->selectedProfile+1 < cd->numColorProfiles) {
        int len = cd->numColorProfiles - cd->selectedProfile - 1;
        ColorProfile *dst = cd->colorProfiles + cd->selectedProfile;
        ColorProfile *src = dst + 1;
        memmove(
                dst,
                src,
                len * sizeof(ColorProfile));
    }
    cd->numColorProfiles--;
    cd->selectedProfile--;
    
    colorDialogSelectProfile(cd, cd->selectedProfile);
    colorDialogUpdateProfileList(cd);
}

static void colorDialogProfileNew(Widget w, colorDialog *cd, XtPointer c)
{
    char profileName[DF_MAX_PROMPT_LENGTH];
    profileName[0] = 0;
    
    int response = DialogF(
            DF_PROMPT,
            w,
            2,
            "Create Profile", "Profile Name:",
            profileName,
            "OK",
            "Cancel");
    
    if(response == 2 || strlen(profileName) == 0) {
        return;
    }
    
    // check profile name
    if(!colorDialogCheckProfileName(cd, profileName)) {
        DialogF(DF_ERR, w, 1, "Error",
                "The profile name '%s' is already in use.", "OK", profileName);
        return;
    }
    if(!colorDialogCheckProfileNameChars(cd, profileName)) {
        return;
    }
    
    cd->numColorProfiles++;
    cd->colorProfiles = NEditRealloc(cd->colorProfiles, cd->numColorProfiles * sizeof(ColorProfile));
    
    ColorProfile *newProfile = &cd->colorProfiles[cd->numColorProfiles-1];
    memset(newProfile, 0, sizeof(ColorProfile));
    
    // copy default values
    ColorProfileCopySettings(&cd->colorProfiles[cd->selectedProfile], newProfile);
    NEditFree(newProfile->name);
    newProfile->name = NEditStrdup(profileName);
    newProfile->orig = NULL;
    
    XmString s = XmStringCreateSimple(profileName);
    XmComboBoxAddItem(cd->profileDropDown, s, -1, False);
    XmComboBoxSelectItem(cd->profileDropDown, s);
    saveColorProfileSettings(cd);
    colorDialogSelectProfile(cd, cd->numColorProfiles-1);
    XmStringFree(s);
}

static void colorDialogOpenStylesSettings(Widget w, colorDialog *cd, XtPointer c)
{
    EditHighlightStyles(NULL);
}

static void colorDialogTextStyleChanged(Widget w, colorDialog *cd, XmToggleButtonCallbackStruct *tb)
{
    if(!tb->set) {
        return; // ignore unset events
    }

    int type = 0;
    if(w == cd->styleLightenW) {
        type = 1;
    } else if(w == cd->styleCustomW) {
        type = 2;
    }
    cd->colorProfiles[cd->selectedProfile].styleType = type;
    SetColorProfileStyleType(type);
}

/*
static void colorDialogThemeVariantChanged(Widget w, colorDialog *cd, XmToggleButtonCallbackStruct *tb)
{
    if(!tb->set) {
        return; // ignore unset events
    }
    
    int themeVariant = 0;
    if(w == cd->styleWMLightW) {
        themeVariant = 1;
    } else if(w == cd->styleWMDarkW) {
        themeVariant = 2;
    }
    
    cd->colorProfiles[cd->selectedProfile].windowThemeVariant = themeVariant;
}
*/

/* 
 * Code for the dialog itself
 */
void ChooseColors(WindowInfo *window)
{
    Widget form, tmpW, topW, infoLbl;
    Widget okBtn, applyBtn, closeBtn;
    colorDialog *cd;
    XmString s1;
    int ac;
    Arg args[20];
    
    /* if the dialog is already displayed, just pop it to the top and return */
    if (window->colorDialog != NULL) {
      RaiseDialogWindow(((colorDialog *)window->colorDialog)->shell);
      return;
    }
    
    /* Create a structure for keeping track of dialog state */
    cd = NEditNew(colorDialog);
    window->colorDialog = cd;
    
    /* Create a form widget in a dialog shell */
    ac = 0;
    XtSetArg(args[ac], XmNautoUnmanage, False); ac++;
    XtSetArg(args[ac], XmNresizePolicy, XmRESIZE_NONE); ac++;
    form = CreateFormDialog(window->shell, "choose colors", args, ac);
    XtVaSetValues(form, XmNshadowThickness, 0, NULL);
    cd->shell = XtParent(form);
    cd->window = window;
    XtVaSetValues(cd->shell, XmNtitle, "Colors", NULL);
    AddMotifCloseCallback(XtParent(form), colorCloseCB, cd);
    XtAddCallback(form, XmNdestroyCallback, colorDestroyCB, cd);
    
    /* color profile selection */
    /*
    s1 = XmStringCreateSimple("Color Profile");
    Widget profileLabel = XtVaCreateManagedWidget("colorProfileLabel", xmLabelWidgetClass, form,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNlabelString, s1,
            XmNtopOffset, 10,
            NULL);
    XmStringFree(s1);
    */
    
    int nProfiles = GetNumColorProfiles();
    
    cd->colorProfiles = NEditCalloc(nProfiles, sizeof(ColorProfile));
    cd->numColorProfiles = nProfiles;
    cd->selectedProfile = 0;
    
    XmStringTable cgProfileList = NEditCalloc(nProfiles, sizeof(XmString));
    ColorProfile *profile = GetColorProfiles();
    int i = 0;
    while(profile) {
        if(profile == defaultColorProfile) {
            cd->selectedProfile = i;
        }
        
        ColorProfileCopySettings(profile, &cd->colorProfiles[i]);
        
        cgProfileList[i] = XmStringCreateLocalized(profile->name);
        profile = profile->next;
        i++;
    }
    cd->colorProfiles[0].modified = TRUE;
    
#ifndef DISABLE_COLORPROFILES
    Widget colorProfileForm = XtVaCreateManagedWidget("colorProfileForm", xmFormWidgetClass, form,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            NULL);
    
    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    //XtSetArg(args[ac], XmNleftWidget, profileLabel); ac++;
    XtSetArg(args[ac], XmNitems, cgProfileList); ac++;
    XtSetArg(args[ac], XmNitemCount, nProfiles); ac++;
    XtSetArg(args[ac], XmNcolumns, 20); ac++;
    XtSetArg(args[ac], XmNhighlightThickness, 2); ac++;
    cd->profileDropDown = XmCreateDropDownList(colorProfileForm, "combobox", args, ac);
    XtManageChild(cd->profileDropDown);
    NEditFree(cgProfileList);
    
    s1 = XmStringCreateLocalized(defaultColorProfile->name);
    XmComboBoxSelectItem(cd->profileDropDown, s1);
    XmStringFree(s1);
    
    XtAddCallback(cd->profileDropDown, XmNselectionCallback,(XtCallbackProc)colorDialogProfileSelected, cd);
    
    s1 = XmStringCreateSimple("Rename");
    Widget colorProfileRename = XtVaCreateManagedWidget("colorProfileRename", xmPushButtonWidgetClass, colorProfileForm,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, cd->profileDropDown,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, cd->profileDropDown,
            XmNdefaultButtonShadowThickness 	, 0,
            XmNlabelString, s1,
            XmNhighlightThickness, 2,
            NULL);
    XmStringFree(s1);
    XtAddCallback(colorProfileRename, XmNactivateCallback,
                 (XtCallbackProc)colorDialogProfileRename, cd);
    
    s1 = XmStringCreateSimple("Remove");
    Widget colorProfileRemove = XtVaCreateManagedWidget("colorProfileRename", xmPushButtonWidgetClass, colorProfileForm,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, colorProfileRename,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, cd->profileDropDown,
            XmNlabelString, s1,
            XmNhighlightThickness, 2,
            NULL);
    XmStringFree(s1);
    XtAddCallback(colorProfileRemove, XmNactivateCallback,
                 (XtCallbackProc)colorDialogProfileRemove, cd);

    s1 = XmStringCreateSimple("New");
    Widget colorProfileNew = XtVaCreateManagedWidget("colorProfilNew", xmPushButtonWidgetClass, colorProfileForm,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, colorProfileRemove,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, cd->profileDropDown,
            XmNlabelString, s1,
            XmNhighlightThickness, 2,
            NULL);
    XmStringFree(s1);
    XtAddCallback(colorProfileNew, XmNactivateCallback,
                 (XtCallbackProc)colorDialogProfileNew, cd);
    
#endif
    
    /* tabs */
    cd->tabs[0] = XtVaCreateManagedWidget("tabButton", xmToggleButtonWidgetClass, form,
#ifdef DISABLE_COLORPROFILES
            XmNtopAttachment, XmATTACH_FORM,
            XmNrightPosition, 33,
#else
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, colorProfileForm,
            XmNrightPosition, 25,
            XmNtopOffset, 10,
#endif
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNfillOnSelect, True,
            XmNindicatorOn, False,
            XmNset, True,
            XmNlabelString, s1 = XmStringCreateSimple("General"),
            NULL);
    XmStringFree(s1);
    cd->tabs[1] = XtVaCreateManagedWidget("tabButton", xmToggleButtonWidgetClass, form,
#ifdef DISABLE_COLORPROFILES
            XmNtopAttachment, XmATTACH_FORM,
            XmNrightPosition, 66,
#else
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, colorProfileForm,
            XmNrightPosition, 50,
            XmNtopOffset, 10,
#endif
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, cd->tabs[0],
            XmNrightAttachment, XmATTACH_POSITION,
            XmNfillOnSelect, True,
            XmNindicatorOn, False,
            XmNlabelString, s1 = XmStringCreateSimple("Indent Rainbow"),
            NULL);
    XmStringFree(s1);
    cd->tabs[2] = XtVaCreateManagedWidget("tabButton", xmToggleButtonWidgetClass, form,
#ifdef DISABLE_COLORPROFILES
            XmNtopAttachment, XmATTACH_FORM,
            XmNrightPosition, 100,
#else
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, colorProfileForm,
            XmNrightPosition, 75,
            XmNtopOffset, 10,
#endif
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, cd->tabs[1],
            XmNrightAttachment, XmATTACH_POSITION,
            XmNfillOnSelect, True,
            XmNindicatorOn, False,
            XmNlabelString, s1 = XmStringCreateSimple("ANSI Colors"),
            NULL);
#ifndef DISABLE_COLORPROFILES
    cd->tabs[3] = XtVaCreateManagedWidget("tabButton", xmToggleButtonWidgetClass, form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, colorProfileForm,
            XmNtopOffset, 10,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, cd->tabs[2],
            XmNrightAttachment, XmATTACH_FORM,
            XmNfillOnSelect, True,
            XmNindicatorOn, False,
            XmNlabelString, s1 = XmStringCreateSimple("Styles"),
            NULL);
    XmStringFree(s1);
    XtAddCallback(cd->tabs[3], XmNvalueChangedCallback, selectColorTab, cd);
#endif
    XtAddCallback(cd->tabs[0], XmNvalueChangedCallback, selectColorTab, cd);
    XtAddCallback(cd->tabs[1], XmNvalueChangedCallback, selectColorTab, cd);
    XtAddCallback(cd->tabs[2], XmNvalueChangedCallback, selectColorTab, cd);
    
    topW = cd->tabs[0];
      
    
    /*
     * buttons
     */
    tmpW = cd->tabForms[0];
    
    /* The OK, Apply, and Cancel buttons */
    okBtn = XtVaCreateManagedWidget("ok",
            xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, MARGIN_SPACING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 10,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 30,
            NULL);
    XtAddCallback(okBtn, XmNactivateCallback, colorOkCB, cd);
    XmStringFree(s1);

    applyBtn = XtVaCreateManagedWidget(
            "apply", xmPushButtonWidgetClass, form,
          XmNlabelString, s1=XmStringCreateSimple("Apply"),
          XmNbottomAttachment, XmATTACH_FORM,
          XmNbottomOffset, MARGIN_SPACING,
          XmNmnemonic, 'A',
          XmNleftAttachment, XmATTACH_POSITION,
          XmNleftPosition, 40,
          XmNrightAttachment, XmATTACH_POSITION,
          XmNrightPosition, 60, NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, colorApplyCB, cd);
    XmStringFree(s1);
    
    closeBtn = XtVaCreateManagedWidget("close",
            xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Close"),
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, MARGIN_SPACING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 70,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 90,
            NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, colorCloseCB, cd);
    XmStringFree(s1);
    
    Widget sepW = XtVaCreateManagedWidget("sep",
            xmSeparatorGadgetClass, form,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, okBtn,
            XmNbottomOffset, MARGIN_SPACING,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM, NULL);
 
    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);
    
    
    /* tab forms */
    cd->tabForms[0] = XtVaCreateManagedWidget("colorForm", xmFormWidgetClass, form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, topW,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, sepW,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            NULL);
    cd->tabForms[1] = XtVaCreateWidget("colorForm", xmFormWidgetClass, form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, topW,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, sepW,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            NULL);
    cd->tabForms[2] = XtVaCreateWidget("colorForm", xmFormWidgetClass, form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, topW,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, sepW,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            NULL);
    cd->tabForms[3] = XtVaCreateWidget("styleForm", xmFormWidgetClass, form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, topW,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, sepW,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            NULL);

    /*
     * Tab 1: Generral
     */
    Widget tabForm = cd->tabForms[0];
  
    /* Information label */
    infoLbl = XtVaCreateManagedWidget("infoLbl",
            xmLabelGadgetClass, tabForm,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 6,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 1,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 99,
            XmNalignment, XmALIGNMENT_CENTER,
            XmNlabelString, s1 = XmStringCreateLtoR(
                "Colors can be entered as names (e.g. red, blue) or "
                "as RGB triples\nin the format #RRGGBB, where each digit "
                "is in the range 0-f.", XmFONTLIST_DEFAULT_TAG),
            NULL);
    XmStringFree(s1);
    topW = infoLbl;
    
    /* The left column (foregrounds) of color entry groups */
    tmpW = addColorGroup( tabForm, "textFg", 'P', "Plain Text Foreground", 
            &(cd->textFgW), &(cd->textFgErrW), topW, 1, 49, cd );  
    tmpW = addColorGroup( tabForm, "selectFg", 'S', "Selection Foreground",
            &(cd->selectFgW), &(cd->selectFgErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "hiliteFg", 'M', "Matching (..) Foreground",
            &(cd->hiliteFgW), &(cd->hiliteFgErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "lineNoFg", 'L', "Line Numbers Foreground",
            &(cd->lineNoFgW), &(cd->lineNoFgErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "lineNoBg", 'N', "Line Numbers Background",
            &(cd->lineNoBgW), &(cd->lineNoBgErrW), tmpW, 1, 49, cd );

    /* The right column (backgrounds) */
    tmpW = addColorGroup( tabForm, "textBg", 'T', "Text Area Background",
            &(cd->textBgW), &(cd->textBgErrW), topW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "selectBg", 'B', "Selection Background",
            &(cd->selectBgW), &(cd->selectBgErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "hiliteBg", 'h', "Matching (..) Background",
            &(cd->hiliteBgW), &(cd->hiliteBgErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "cursorFg", 'C', "Cursor Color",
            &(cd->cursorFgW), &(cd->cursorFgErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "cursorLineBg", 'U', "Cursor Line Background",
            &(cd->cursorLineBgW), &(cd->cursorLineBgErrW), tmpW, 51, 99, cd );

    tmpW = XtVaCreateManagedWidget("infoLbl",
            xmLabelGadgetClass, tabForm,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, tmpW,
            XmNtopOffset, MARGIN_SPACING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 1,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 99,
            XmNalignment, XmALIGNMENT_CENTER,
            XmNlabelString, s1 = XmStringCreateLtoR(
                "NOTE: Foreground colors only apply when syntax highlighting "
                "is DISABLED.\n", XmFONTLIST_DEFAULT_TAG),
            NULL);
    XmStringFree(s1);
    
    Dimension h = 60;
    XtVaGetValues(cd->textFgW, XmNheight, &h, NULL);
    
    tmpW = XtVaCreateManagedWidget("sepLbl", xmLabelGadgetClass, tabForm,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, tmpW,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNtopOffset, 3*h + 3*MARGIN_SPACING,
            XmNlabelString, s1 = XmStringCreateSimple(""), NULL);
    
    /*
     * Tab 2: Indent Rainbow Colors
     */
    tabForm = cd->tabForms[1];
    
    cd->scrollW = XtVaCreateManagedWidget("scrollw", xmScrolledWindowWidgetClass, tabForm,
            XmNleftAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftOffset, 10,
            XmNrightOffset, 10,
            XmNbottomOffset, 10,
            XmNtopOffset, 10,
            XmNscrollBarDisplayPolicy, XmAS_NEEDED,
            XmNscrollingPolicy, XmAUTOMATIC,
            XmNshadowThickness, 1,
            NULL);
    cd->btnForm = XtVaCreateManagedWidget("form", xmFormWidgetClass, cd->scrollW,
            XmNshadowThickness, 0, NULL);
    
    XtVaSetValues(cd->tabForms[1], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, cd->tabForms[0], NULL);
    
    // init widget array
    size_t irAlloc = 8;
    cd->allocIndentRainbowColors = irAlloc;
    cd->indentRainbowColors = NEditCalloc(irAlloc, sizeof(indentRainbowColor));
    cd->numIndentRainbowColors = 0;
    
    cd->addBtn = NULL;
    addAddBtn(cd);
    
    indentRainbowDialogLoadColors(cd);
        
    /*
     * Tab 3: ANSI Colors
     */
    
    topW = infoLbl;
    
    tabForm = cd->tabForms[2];
    /* The left column (foregrounds) of color entry groups */
    tmpW = addColorGroup( tabForm, "ansiBlack", 'P', "Black", 
            &(cd->ansiBlackW), &(cd->ansiBlackErrW), topW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "ansiRed", 'S', "Red",
            &(cd->ansiRedW), &(cd->ansiRedErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "ansiGreen", 'M', "Green",
            &(cd->ansiGreenW), &(cd->ansiGreenErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "ansiYellow", 'L', "Yellow",
            &(cd->ansiYellowW), &(cd->ansiYellowErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "ansiBlue", 'N', "Blue",
            &(cd->ansiBlueW), &(cd->ansiBlueErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "ansiMagenta", 'N', "Magenta",
            &(cd->ansiMagentaW), &(cd->ansiMagentaErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "ansiCyan", 'N', "Cyan",
            &(cd->ansiCyanW), &(cd->ansiCyanErrW), tmpW, 1, 49, cd );
    tmpW = addColorGroup( tabForm, "ansiWhite", 'N', "White",
            &(cd->ansiWhiteW), &(cd->ansiWhiteErrW), tmpW, 1, 49, cd );

    /* The right column (backgrounds) */
    tmpW = addColorGroup( tabForm, "ansiBrightBlack", 'T', "Bright Black",
            &(cd->ansiBrightBlackW), &(cd->ansiBrightBlackErrW), topW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "ansiBrightRed", 'B', "Bright Red",
            &(cd->ansiBrightRedW), &(cd->ansiBrightRedErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "ansiBrightGreen", 'h', "Bright Green",
            &(cd->ansiBrightGreenW), &(cd->ansiBrightGreenErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "ansiBrightYellow", 'C', "Bright Yellow",
            &(cd->ansiBrightYellowW), &(cd->ansiBrightYellowErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "ansiBrightBlue", 'U', "Bright Blue",
            &(cd->ansiBrightBlueW), &(cd->ansiBrightBlueErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "ansiBrightMagenta", 'U', "Bright Magenta",
            &(cd->ansiBrightMagentaW), &(cd->ansiBrightMagentaErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "ansiBrightCyan", 'U', "Bright Cyan",
            &(cd->ansiBrightCyanW), &(cd->ansiBrightCyanErrW), tmpW, 51, 99, cd );
    tmpW = addColorGroup( tabForm, "ansiBrightWhite", 'U', "Bright White",
            &(cd->ansiBrightWhiteW), &(cd->ansiBrightWhiteErrW), tmpW, 51, 99, cd );
    
    loadAnsiColors(cd);

    /*
     * Tab 4: Styles
     */
    tabForm = cd->tabForms[3]; 
    s1 = XmStringCreateLocalized("Text drawing styles can be adjusted for a color profile.\n"
                                 "Lightened Colors: Increase brightness to improve visibility against dark backgrounds.\n"
                                 "Custom Styles: Implement custom drawing styles by manually adjusting settings\n"
                                 "in the Text Drawing Styles menu.");
    Widget stInfoLabel = XtVaCreateManagedWidget("stInfoLabel",
            xmLabelGadgetClass, tabForm,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            //XmNrightAttachment, XmATTACH_FORM,
            XmNleftOffset, 6,
            XmNtopOffset, 6,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNlabelString, s1,
            NULL);
    XmStringFree(s1);

    Widget radioRC = XtVaCreateManagedWidget("style_radiobuttons",
            xmRowColumnWidgetClass, tabForm,
            XmNleftAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, stInfoLabel,
            XmNleftOffset, 6,
            XmNtopOffset, 6,
            XmNradioBehavior, True,
            XmNpacking, XmPACK_COLUMN,
    	    XmNnumColumns, 3,
            NULL);

    ColorProfile *sp = &cd->colorProfiles[cd->selectedProfile];

    s1 = XmStringCreateLocalized("Default Styles");
    cd->styleDefaultW = XtVaCreateManagedWidget("stDefaultRB",
            xmToggleButtonWidgetClass, radioRC,
            XmNlabelString, s1,
            XmNset, sp->styleType == 0 ? 1 : 0,
            NULL);
    XmStringFree(s1);
    XtAddCallback(
                cd->styleDefaultW,
                XmNvalueChangedCallback,
                (XtCallbackProc) colorDialogTextStyleChanged,
                cd);

    s1 = XmStringCreateLocalized("Lightened Colors");
    cd->styleLightenW = XtVaCreateManagedWidget("stDefaultRB",
            xmToggleButtonWidgetClass, radioRC,
            XmNlabelString, s1,
            XmNset, sp->styleType == 1 ? 1 : 0,
            NULL);
    XmStringFree(s1);
    XtAddCallback(
                cd->styleLightenW,
                XmNvalueChangedCallback,
                (XtCallbackProc) colorDialogTextStyleChanged,
                cd);

    s1 = XmStringCreateLocalized("Custom Styles");
    cd->styleCustomW = XtVaCreateManagedWidget("stDefaultRB",
            xmToggleButtonWidgetClass, radioRC,
            XmNlabelString, s1,
            XmNset, sp->styleType == 2 ? 1 : 0,
            NULL);
    XmStringFree(s1);
    XtAddCallback(
                cd->styleCustomW,
                XmNvalueChangedCallback,
                (XtCallbackProc) colorDialogTextStyleChanged,
                cd);

    s1 = XmStringCreateLocalized("Text Drawing Styles...");
    Widget stOpenStyleSettings = XtVaCreateManagedWidget("stOpenButton",
            xmPushButtonWidgetClass, tabForm,
            XmNleftAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, radioRC,
            XmNleftOffset, 6,
            XmNtopOffset, 6,
            XmNlabelString, s1,
            NULL);
    XtAddCallback(stOpenStyleSettings, XmNactivateCallback,
                 (XtCallbackProc)colorDialogOpenStylesSettings, NULL);
    XmStringFree(s1);
    
    s1 = XmStringCreateLocalized("X Resource File (absolute path or a path relative to XNEDIT_HOME)");
    cd->styleXResLabelW = XtVaCreateManagedWidget("stXResLabel",
            xmLabelGadgetClass, tabForm,
            XmNleftAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, stOpenStyleSettings,
            XmNleftOffset, 6,
            XmNtopOffset, 16,
            XmNlabelString, s1,
            NULL);
    XmStringFree(s1);
    
    cd->styleXResW = XtVaCreateManagedWidget("stXResText",
            xmTextWidgetClass, tabForm,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, cd->styleXResLabelW,
            XmNleftOffset, 6,
            XmNrightOffset, 6,
            XmNtopOffset, 6,
            NULL);
    XmTextSetString(cd->styleXResW, sp->resourceFile);
    
    // This is deprecated before it was deployed, because Gnome removed
    // support for _GTK_THEME_VARIANT
    // Maybe in the future this can be used
    /*
    s1 = XmStringCreateLocalized("Window Title Bar (GTK Theme Variant)");
    Widget stThemeVariantLabel = XtVaCreateManagedWidget("stThemeVariantLabel",
            xmLabelGadgetClass, tabForm,
            XmNleftAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, cd->styleXResW,
            XmNleftOffset, 6,
            XmNtopOffset, 16,
            XmNlabelString, s1,
            NULL);
    XmStringFree(s1);
    
    Widget wmRadioRC = XtVaCreateManagedWidget("style_radiobuttons",
            xmRowColumnWidgetClass, tabForm,
            XmNleftAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, stThemeVariantLabel,
            XmNleftOffset, 6,
            XmNtopOffset, 6,
            XmNradioBehavior, True,
            XmNpacking, XmPACK_COLUMN,
    	    XmNnumColumns, 3,
            NULL);
    
    s1 = XmStringCreateLocalized("Default");
    cd->styleWMDefaultW = XtVaCreateManagedWidget("stWMDefaultRB",
            xmToggleButtonWidgetClass, wmRadioRC,
            XmNlabelString, s1,
            XmNset, sp->windowThemeVariant == 0 ? 1 : 0,
            NULL);
    XmStringFree(s1);
    XtAddCallback(
                cd->styleWMDefaultW,
                XmNvalueChangedCallback,
                (XtCallbackProc) colorDialogThemeVariantChanged,
                cd);

    s1 = XmStringCreateLocalized("Light");
    cd->styleWMLightW = XtVaCreateManagedWidget("stWMDefaultRB",
            xmToggleButtonWidgetClass, wmRadioRC,
            XmNlabelString, s1,
            XmNset, sp->windowThemeVariant == 1 ? 1 : 0,
            NULL);
    XmStringFree(s1);
    XtAddCallback(
                cd->styleWMLightW,
                XmNvalueChangedCallback,
                (XtCallbackProc) colorDialogThemeVariantChanged,
                cd);

    s1 = XmStringCreateLocalized("Dark");
    cd->styleWMDarkW = XtVaCreateManagedWidget("stWMDefaultRB",
            xmToggleButtonWidgetClass, wmRadioRC,
            XmNlabelString, s1,
            XmNset, sp->windowThemeVariant == 2 ? 1 : 0,
            NULL);
    XmStringFree(s1);
    XtAddCallback(
                cd->styleWMDarkW,
                XmNvalueChangedCallback,
                (XtCallbackProc) colorDialogThemeVariantChanged,
                cd);
    */
    
    if(cd->selectedProfile == 0) {
        // disable settings that are not available for the default profile
        XtUnmanageChild(cd->styleXResLabelW);
        XtUnmanageChild(cd->styleXResW);
        XtSetSensitive(cd->styleDefaultW, False);
        XtSetSensitive(cd->styleLightenW, False);
        XtSetSensitive(cd->styleCustomW, False);
    }

    // TODO: style preview area
    
    /* Set initial values */
    loadColors(cd);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);
        
    /* put up dialog */
    ManageDialogCenteredOnPointer(form);
}

static void clearRainbowColors(colorDialog *cd)
{
    WidgetList children;
    int numChildren;
    XtVaGetValues(cd->btnForm, XmNchildren, &children, XmNnumChildren, &numChildren, NULL);
    
    // remove children
    for(int i=0;i<numChildren;i++) {
        XtUnmanageChild(children[i]);
        XtDestroyWidget(children[i]);
    }
    
    //NEditFree(cd->indentRainbowColors);
    //cd->indentRainbowColors = NULL;
    cd->numIndentRainbowColors = 0;
    
    cd->addBtn = NULL;
    addAddBtn(cd);
}

static void saveColorProfileSettings(colorDialog *cd)
{
    ColorProfile *profile = &cd->colorProfiles[cd->selectedProfile];
    
    /*
     * Tab 1: General
     */
    profile->textFg = XmTextGetString(cd->textFgW),
    profile->textBg = XmTextGetString(cd->textBgW),
    profile->selectFg = XmTextGetString(cd->selectFgW),
    profile->selectBg = XmTextGetString(cd->selectBgW),
    profile->hiliteFg = XmTextGetString(cd->hiliteFgW),
    profile->hiliteBg = XmTextGetString(cd->hiliteBgW),
    profile->lineNoFg = XmTextGetString(cd->lineNoFgW),
    profile->lineNoBg = XmTextGetString(cd->lineNoBgW),
    profile->cursorFg = XmTextGetString(cd->cursorFgW),
    profile->lineHiBg = XmTextGetString(cd->cursorLineBgW);
    
    /*
     * Tab 2: Indent Rainbow Colors
     */
    size_t alloc = cd->numIndentRainbowColors * 10;
    char *irStr = NEditMalloc(alloc);
    int pos = 0;
    
    for(int i=0;i<cd->numIndentRainbowColors;i++) {
        char *ir = XmTextGetString(cd->indentRainbowColors[i].textfield);
        size_t ir_len = strlen(ir);
        if(pos + ir_len + 2 >= alloc) {
            alloc += 4 * ir_len;
            irStr = NEditRealloc(irStr, alloc);
        }
        memcpy(irStr+pos, ir, ir_len);
        pos += ir_len;
        irStr[pos] = ';';
        pos++;
        XtFree(ir);
    }
    
    if(pos > 0 && irStr[pos-1] == ';') {
        irStr[pos-1] = '\0';
    }
    profile->rainbowColorList = irStr;
    
    /*
     * Tab 3: ANSI Colors
     */
    char *ansiBlack = XmTextGetString(cd->ansiBlackW);
    char *ansiRed = XmTextGetString(cd->ansiRedW);
    char *ansiGreen = XmTextGetString(cd->ansiGreenW);
    char *ansiYellow = XmTextGetString(cd->ansiYellowW);
    char *ansiBlue = XmTextGetString(cd->ansiBlueW);
    char *ansiMagenta = XmTextGetString(cd->ansiMagentaW);
    char *ansiCyan = XmTextGetString(cd->ansiCyanW);
    char *ansiWhite = XmTextGetString(cd->ansiWhiteW);
    char *ansiBrightBlack = XmTextGetString(cd->ansiBrightBlackW);
    char *ansiBrightRed = XmTextGetString(cd->ansiBrightRedW);
    char *ansiBrightGreen = XmTextGetString(cd->ansiBrightGreenW);
    char *ansiBrightYellow = XmTextGetString(cd->ansiBrightYellowW);
    char *ansiBrightBlue = XmTextGetString(cd->ansiBrightBlueW);
    char *ansiBrightMagenta = XmTextGetString(cd->ansiBrightMagentaW);
    char *ansiBrightCyan = XmTextGetString(cd->ansiBrightCyanW);
    char *ansiBrightWhite = XmTextGetString(cd->ansiBrightWhiteW);
    
    size_t len = strlen(ansiBlack) + strlen(ansiRed) + strlen(ansiGreen) + strlen(ansiYellow)
               + strlen(ansiBlue) + strlen(ansiMagenta) + strlen(ansiCyan) + strlen(ansiWhite)
               + strlen(ansiBrightBlack) + strlen(ansiBrightRed) + strlen(ansiBrightGreen) + strlen(ansiBrightYellow)
               + strlen(ansiBrightBlue) + strlen(ansiBrightMagenta) + strlen(ansiBrightCyan) + strlen(ansiBrightWhite)
               + 16;
    char *ansiColorList = NEditMalloc(len);
    snprintf(ansiColorList, len, "%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
            ansiBlack, ansiRed, ansiGreen, ansiYellow,
            ansiBlue, ansiMagenta, ansiCyan, ansiWhite,
            ansiBrightBlack, ansiBrightRed, ansiBrightGreen, ansiBrightYellow,
            ansiBrightBlue, ansiBrightMagenta, ansiBrightCyan, ansiBrightWhite);
    
    profile->ansiColorList = ansiColorList;
    
    profile->modified = TRUE;
    
    /*
     * Tab 4: Style Settings
     */
    char *resourceFile = XmTextGetString(cd->styleXResW);
    NEditFree(profile->resourceFile);
    if(resourceFile && strlen(resourceFile) > 0) {
        profile->resourceFile = resourceFile;
    } else {
        profile->resourceFile = NULL;
        XtFree(resourceFile);
    }
}

static void indentRainbowDialogLoadColors(colorDialog *cd)
{
    char *colorList = cd->colorProfiles[cd->selectedProfile].rainbowColorList;
    size_t len = colorList ? strlen(colorList) : 0;
    
    char *colors = malloc(len + 1);
    colors[0] = 0;
    
    int b = 0;
    for(int i=0;i<=len;i++) {
        int c = colorList[i];
        if(c == ';' || c == '\0') {
            memcpy(colors+b, colorList+b, i-b);
            colors[i] = '\0';
            addRainbowColor(cd, colors + b);
            b = i+1;
        }
    }
    free(colors);
}

char* ParseAnsiColorList(char **colorArray, const char *colorStr)
{
    for(int i=0;i<16;i++) {
        colorArray[i] = "#000000";
    }
    int ncolors = 0;
    
    size_t len = colorStr ? strlen(colorStr) : 0;
    
    char *colorListStr = NEditMalloc(len+1);
    memcpy(colorListStr, colorStr, len);
    colorListStr[len] = 0;
    
    int b = 0;
    for(int i=0;i<=len;i++) {
        char c = colorListStr[i];
        if(c == ';' || c == '\0') {
            colorArray[ncolors++] = colorListStr+b;
            colorListStr[i] = '\0';
            b = i+1;
        }
    }
    
    return colorListStr;
}

static void loadAnsiColors(colorDialog *cd)
{
    char *colors[16];
    
    char *colorListStr = ParseAnsiColorList(colors, cd->colorProfiles[cd->selectedProfile].ansiColorList);
    
    XmTextSetString(cd->ansiBlackW, colors[0]);
    XmTextSetString(cd->ansiRedW, colors[1]);
    XmTextSetString(cd->ansiGreenW, colors[2]);
    XmTextSetString(cd->ansiYellowW, colors[3]);
    XmTextSetString(cd->ansiBlueW, colors[4]);
    XmTextSetString(cd->ansiMagentaW, colors[5]);
    XmTextSetString(cd->ansiCyanW, colors[6]);
    XmTextSetString(cd->ansiWhiteW, colors[7]);
    XmTextSetString(cd->ansiBrightBlackW, colors[8]);
    XmTextSetString(cd->ansiBrightRedW, colors[9]);
    XmTextSetString(cd->ansiBrightGreenW, colors[10]);
    XmTextSetString(cd->ansiBrightYellowW, colors[11]);
    XmTextSetString(cd->ansiBrightBlueW, colors[12]);
    XmTextSetString(cd->ansiBrightMagentaW, colors[13]);
    XmTextSetString(cd->ansiBrightCyanW, colors[14]);
    XmTextSetString(cd->ansiBrightWhiteW, colors[15]);
   
    
    NEditFree(colorListStr);
}

static void loadColors(colorDialog *cd)
{
    ColorProfile *selectedProfile = &cd->colorProfiles[cd->selectedProfile];
    XmTextSetString(cd->textFgW,   selectedProfile->textFg);
    XmTextSetString(cd->textBgW,   selectedProfile->textBg);
    XmTextSetString(cd->selectFgW, selectedProfile->selectFg);
    XmTextSetString(cd->selectBgW, selectedProfile->selectBg);
    XmTextSetString(cd->hiliteFgW, selectedProfile->hiliteFg);
    XmTextSetString(cd->hiliteBgW, selectedProfile->hiliteBg);
    XmTextSetString(cd->lineNoFgW, selectedProfile->lineNoFg);
    XmTextSetString(cd->lineNoBgW, selectedProfile->lineNoBg);
    XmTextSetString(cd->cursorFgW, selectedProfile->cursorFg);
    XmTextSetString(cd->cursorLineBgW, selectedProfile->lineHiBg);
}

static void loadColorProfileStyleSettings(colorDialog *cd)
{
    ColorProfile *sp = &cd->colorProfiles[cd->selectedProfile];
    XtVaSetValues(cd->styleDefaultW, XmNset, sp->styleType == 0 ? 1 : 0, NULL);
    XtVaSetValues(cd->styleLightenW, XmNset, sp->styleType == 1 ? 1 : 0, NULL);
    XtVaSetValues(cd->styleCustomW, XmNset, sp->styleType == 2 ? 1 : 0, NULL);
    
    XmTextSetString(cd->styleXResW, sp->resourceFile);
    
    Boolean enableStyleSettings = cd->selectedProfile != 0;
    XtSetSensitive(cd->styleDefaultW, enableStyleSettings);
    XtSetSensitive(cd->styleLightenW, enableStyleSettings);
    XtSetSensitive(cd->styleCustomW, enableStyleSettings);
    if(enableStyleSettings) {
        XtManageChild(cd->styleXResLabelW);
        XtManageChild(cd->styleXResW);
    } else {
        XtUnmanageChild(cd->styleXResLabelW);
        XtUnmanageChild(cd->styleXResW);
    }
    
    //XtVaSetValues(cd->styleWMDefaultW, XmNset, sp->windowThemeVariant == 0 ? 1 : 0, NULL);
    //XtVaSetValues(cd->styleWMLightW, XmNset, sp->windowThemeVariant == 1 ? 1 : 0, NULL);
    //XtVaSetValues(cd->styleWMDarkW, XmNset, sp->windowThemeVariant == 2 ? 1 : 0, NULL);
}


ColorList ParseColorList(const char *str, size_t len)
{
    ColorList list = { NULL, NULL, 0 };
    
    // copy str
    list.liststr = NEditMalloc(len+1);
    memcpy(list.liststr, str, len);
    list.liststr[len] = 0;
    
    size_t alloc = 16;
    list.colors = NEditCalloc(alloc, sizeof(char*));
    
    // parse str
    int b = 0;
    for(int i=0;i<=len;i++) {
        char c = list.liststr[i];
        if(c == ';' || c == '\0') {
            if(list.ncolors >= alloc) {
                alloc += 8;
                list.colors = NEditRealloc(list.colors, alloc * sizeof(char*));
            }
            
            list.colors[list.ncolors++] = list.liststr+b;
            list.liststr[i] = '\0';
            b = i+1;
        }
    }
    
    return list;
}

void ColorProfileDestroy(ColorProfile *profile)
{
    ColorProfileFreeContent(profile);
    NEditFree(profile);
}

void ColorProfileFreeContent(ColorProfile *profile)
{
    NEditFree(profile->name);
    NEditFree(profile->textFg);
    NEditFree(profile->textBg);
    NEditFree(profile->selectFg);
    NEditFree(profile->hiliteFg);
    NEditFree(profile->hiliteBg);
    NEditFree(profile->lineNoFg);
    NEditFree(profile->lineNoBg);
    NEditFree(profile->cursorFg);
    NEditFree(profile->lineHiBg);
    NEditFree(profile->ansiColorList);
    NEditFree(profile->rainbowColorList);
    NEditFree(profile->resourceFile);
}

static int GetCgProfileName(const char *str, size_t len, const char **out_name, size_t *out_namelen)
{
    const char *name = str;
    size_t namelen = 0;
    for(int i=0;i<len;i++) {
        if(str[i] == ':') {
            namelen = i;
            break;
        }
    }
    while(namelen > 0 && isspace(*name)) {
        name++;
        namelen--;
    }
    if(namelen == 0) {
        return 0;
    }
    
    *out_name = name;
    *out_namelen = namelen;
    
    return 1;
}

static int GetCgProfileSection(const char *str, size_t len, const char **sectionName, size_t *sectionNameLen, const char **list, size_t *listlen)
{
    if(!GetCgProfileName(str, len, sectionName, sectionNameLen)) {
        return 0;
    }
    
    const char *listStr = str + *sectionNameLen + 1;
    size_t remainingLen = len - (listStr - str);
    size_t listStrLen = 0;
    
    if(remainingLen == 0 || listStr[0] != '{') {
        return 0;
    }
    
    // find '}'
    for(int i=0;i<remainingLen;i++) {
        if(listStr[i] == '}') {
            listStrLen = i+1;
            break;
        }
    }
    
    if(listStrLen <= 2) {
        // empty list str
        return 0;
    }
    
    *list = listStr+1;
    *listlen = listStrLen-2;
    
    return (listStr - str) + listStrLen;
}

static ColorProfile* ParseColorProfile(const char *str, size_t len)
{
    // get profile name
    const char *name = str;
    size_t namelen = 0;
    if(!GetCgProfileName(str, len, &name, &namelen)) {
        return NULL;
    }
    
    // get sections
    const char *remaining = name + namelen + 1;
    size_t remainingLen = len - (remaining - str);
    
    int err = 0;
    ColorProfile *profile = NEditMalloc(sizeof(ColorProfile));
    memset(profile, 0, sizeof(ColorProfile));
    profile->name = NEditMalloc(namelen+1);
    memcpy(profile->name, name, namelen);
    profile->name[namelen] = 0;
    
    while(remainingLen > 0) {
        const char *sectionName = NULL;
        size_t sectionNameLen = 0;
        const char *sectionColors = NULL;
        size_t sectionColorsLen = 0;
        
        int sectionLen = GetCgProfileSection(remaining, remainingLen, &sectionName, &sectionNameLen, &sectionColors, &sectionColorsLen);
        if(sectionLen > 0) {
            
            // parse colors
            if(!strncmp(sectionName, "colors", sectionNameLen)) {
                ColorList colors = ParseColorList(sectionColors, sectionColorsLen);
                profile->textFg = NEditStrdup(colors.ncolors >= 1 ? colors.colors[0] : GetPrefColorName(TEXT_FG_COLOR));
                profile->textBg = NEditStrdup(colors.ncolors >= 2 ? colors.colors[1] : GetPrefColorName(TEXT_BG_COLOR));
                profile->selectFg = NEditStrdup(colors.ncolors >= 3 ? colors.colors[2] : GetPrefColorName(SELECT_FG_COLOR));
                profile->selectBg = NEditStrdup(colors.ncolors >= 4 ? colors.colors[3] : GetPrefColorName(SELECT_BG_COLOR));
                profile->hiliteFg = NEditStrdup(colors.ncolors >= 5 ? colors.colors[4] : GetPrefColorName(HILITE_FG_COLOR));
                profile->hiliteBg = NEditStrdup(colors.ncolors >= 6 ? colors.colors[5] : GetPrefColorName(HILITE_BG_COLOR));
                profile->lineNoFg = NEditStrdup(colors.ncolors >= 7 ? colors.colors[6] : GetPrefColorName(LINENO_FG_COLOR));
                profile->lineNoBg = NEditStrdup(colors.ncolors >= 8 ? colors.colors[7] : GetPrefColorName(LINENO_BG_COLOR));
                profile->cursorFg = NEditStrdup(colors.ncolors >= 9 ? colors.colors[8] : GetPrefColorName(CURSOR_FG_COLOR));
                profile->lineHiBg = NEditStrdup(colors.ncolors >= 10 ? colors.colors[9] : GetPrefColorName(CURSOR_LINE_BG_COLOR));
                free(colors.liststr);
                free(colors.colors);
            } else if(!strncmp(sectionName, "ansi", sectionNameLen)) {
                profile->ansiColorList = NEditMalloc(sectionColorsLen+1);
                memcpy(profile->ansiColorList, sectionColors, sectionColorsLen);
                profile->ansiColorList[sectionColorsLen] = 0;
            } else if(!strncmp(sectionName, "rainbow", sectionNameLen)) {
                profile->rainbowColorList = NEditMalloc(sectionColorsLen+1);
                memcpy(profile->rainbowColorList, sectionColors, sectionColorsLen);
                profile->rainbowColorList[sectionColorsLen] = 0;
            } else if(!strncmp(sectionName, "styles", sectionNameLen)) {
                // the styles value is either 0, 1 or 2
                if(sectionColorsLen == 1) {
                    // profile->styleType is initialized with 0, we only
                    // need to check if the new value is 1 or 2
                    if(sectionColors[0] == '1') {
                        profile->styleType = 1;
                    } else if(sectionColors[0] == '2') {
                        profile->styleType = 2;
                    }
                }
            } else if(!strncmp(sectionName, "res", sectionNameLen)) {
                profile->resourceFile = NEditMalloc(sectionColorsLen+1);
                memcpy(profile->resourceFile, sectionColors, sectionColorsLen);
                profile->resourceFile[sectionColorsLen] = 0;
            } else if(!strncmp(sectionName, "wm", sectionNameLen)) {
                // wm value is 0, 1 or 2
                if(sectionColorsLen == 1) {
                    // profile->windowThemeVariant is initialized with 0, we only
                    // need to check if the new value is 1 or 2
                    if(sectionColors[0] == '1') {
                        profile->windowThemeVariant = 1;
                    } else if(sectionColors[0] == '2') {
                        profile->windowThemeVariant = 2;
                    }
                }
            } else {
                fprintf(stderr, "XNEdit: Error: cannot parse color profile %s: unknown str '%.*s'\n", profile->name, (int)sectionNameLen, sectionName);
            }
            
            if(!profile->rainbowColorList) {
                profile->rainbowColorList = NEditStrdup(GetPrefIndentRainbowColors());
            }
            if(!profile->ansiColorList) {
                profile->ansiColorList = NEditStrdup(GetPrefAnsiColorList());
            }   
            
            // next section
            remaining += sectionLen;
            remainingLen -= sectionLen;
            
            // skip ',' before next section
            if(remainingLen > 0 && remaining[0] == ',') {
                remainingLen--;
                remaining++;
            }
        } else {
            err = 1;
            break;
        }
    }
    
    if(err) {
        ColorProfileDestroy(profile);
        profile = NULL;
    }
    
    
    return profile;
}

void ParseColorProfiles(const char *str)
{
    ColorProfile *profilesListBegin = NULL;
    ColorProfile *profilesListEnd = NULL;
    
    size_t len = strlen(str);
    
#ifdef DISABLE_COLORPROFILES
    char *defaultProfileName = "default";
#else
    char *defaultProfileName = GetPrefDefaultColorProfileName();
    if(!defaultProfileName) {
        defaultProfileName = "default";
    }
#endif
       
    // each line contains one color profile
    // parse each line and add the profiles to a linked list
    int lineStart = 0;
    for(int i=0;i<=len;i++) {
        if(i == len || str[i] == '\n') {
            ColorProfile *colorProfile = ParseColorProfile(str+lineStart, i-lineStart);
            lineStart = i+1;
            
            if(colorProfile) {
                if(!strcmp(colorProfile->name, defaultProfileName)) {
                    defaultColorProfile = colorProfile;
                }
                
                if(profilesListEnd) {
                    profilesListEnd->next = colorProfile;
                    profilesListEnd = colorProfile;
                } else {
                    profilesListBegin = colorProfile;
                    profilesListEnd = colorProfile;
                }
            }
        }
    }
    
    // TODO: add default color profile
    ColorProfile *defaultProfile = NEditMalloc(sizeof(ColorProfile));
    memset(defaultProfile, 0, sizeof(ColorProfile));
    
    defaultProfile->name = NEditStrdup("default");
    defaultProfile->textFg = NEditStrdup(GetPrefColorName(TEXT_FG_COLOR));
    defaultProfile->textBg = NEditStrdup(GetPrefColorName(TEXT_BG_COLOR));
    defaultProfile->selectFg = NEditStrdup(GetPrefColorName(SELECT_FG_COLOR));
    defaultProfile->selectBg = NEditStrdup(GetPrefColorName(SELECT_BG_COLOR));
    defaultProfile->hiliteFg = NEditStrdup(GetPrefColorName(HILITE_FG_COLOR));
    defaultProfile->hiliteBg = NEditStrdup(GetPrefColorName(HILITE_BG_COLOR));
    defaultProfile->lineNoFg = NEditStrdup(GetPrefColorName(LINENO_FG_COLOR));
    defaultProfile->lineNoBg = NEditStrdup(GetPrefColorName(LINENO_BG_COLOR));
    defaultProfile->cursorFg = NEditStrdup(GetPrefColorName(CURSOR_FG_COLOR));
    defaultProfile->lineHiBg = NEditStrdup( GetPrefColorName(CURSOR_LINE_BG_COLOR));
    defaultProfile->rainbowColorList = NEditStrdup(GetPrefIndentRainbowColors());
    defaultProfile->ansiColorList = NEditStrdup(GetPrefAnsiColorList());
    
    defaultProfile->next = profilesListBegin;
    
    colorProfiles = defaultProfile;
    if(!defaultColorProfile) {
        defaultColorProfile = defaultProfile;
    }
    
    if(strcmp(defaultColorProfile->name, "default") != 0) {
        SetColorProfileName(defaultColorProfile->name);
        SetColorProfileStyleType(defaultColorProfile->styleType);
    }
}

/*
**  This function passes up a pointer to the static name of the default
**  shell, currently defined as the user's login shell.
**  In case of errors, the fallback of "sh" will be returned.
*/
static const char* getDefaultShell(void)
{
    struct passwd* passwdEntry = NULL;
    static char shellBuffer[MAXPATHLEN + 1] = "sh";

    passwdEntry = getpwuid(getuid());   /*  getuid() never fails.  */

    if (NULL == passwdEntry)
    {
        /*  Something bad happened! Do something, quick!  */
        perror("xnedit: Failed to get passwd entry (falling back to 'sh')");
        return "sh";
    }

    /*  *passwdEntry may be overwritten  */
    /*  TODO: To make this and other function calling getpwuid() more robust,
        passwdEntry should be kept in a central position (Core->sysinfo?).
        That way, local code would always get a current copy of passwdEntry,
        but could still be kept lean.  The obvious alternative of a central
        facility within NEdit to access passwdEntry would increase coupling
        and would have to cover a lot of assumptions.  */
    strncpy(shellBuffer, passwdEntry->pw_shell, MAXPATHLEN);
    shellBuffer[MAXPATHLEN] = '\0';

    return shellBuffer;
}


char* ChangeFontSize(const char *name, int newsize)
{
    char sizestr[32];
    snprintf(sizestr, 32, "%d", newsize);
    
    char* newFontName = FontNameAddAttribute(
        name,
        strlen(name),
        "size",
        sizestr);
    
    return newFontName;
}

static int parseSize(const char *sz) {
    int r = 0;
    
    size_t len = strlen(sz);
    if(len == 1) {
        switch(sz[0]) {
            default: break;
            case 's': return 0;
            case 'm': return 1;
            case 'l': return 2;
        }
    } else {
        if(!strcmp(sz, "small")) {
            r = 0;
        } else if(!strcmp(sz, "medium")) {
            r = 1;
        } else if(!strcmp(sz, "large")) {
            r = 2;
        }
    }
    
    return r;
}

static void parseIconSize(char *iconSize)
{
    char *iconSizeStr = strdup(iconSize);
    
    PrefData.closeIconSize = 0;
    PrefData.isrcFindIconSize = 0;
    PrefData.isrcClearIconSize = 0;
    
    int close = -1;
    int find = -1;
    int clear = -1;
    
    char *sizeStr = strtok(iconSizeStr, ",");
    while(sizeStr) {
        char *c = strchr(sizeStr, '=');
        if(!c) {
            int size = parseSize(sizeStr);
            if(close < 0) close = size;
            if(find < 0) find = size;
            if(clear < 0) clear = size;
        } else {
            int size = parseSize(c+1);
            *c = 0;
            if(!strcmp(sizeStr, "close")) {
                close = size;
            } else if(!strcmp(sizeStr, "isrcFind")) {
                find = size;
            } else if(!strcmp(sizeStr, "isrcClear")) {
                clear = size;
            }
        }
        
        sizeStr = strtok(NULL, ",");
    }
    
    if(close >= 0) PrefData.closeIconSize = close;
    if(find >= 0) PrefData.isrcFindIconSize = find;
    if(clear >= 0) PrefData.isrcClearIconSize = clear;
    
    free(iconSizeStr);
}


typedef struct {
    Widget shell;
    Widget scrolledwindow;
    Widget form;
    Widget fdDefaultView;
    Widget fdShowHidden;
    Widget fdSort;
    Widget icSize;
    Widget icClose;
    Widget icFind;
    Widget icClear;
    Widget edZoom;
    Widget edZoomMouseWheelDefaultBehavior;
    Widget edZoomMouseWheelInvertedBehavior;
    Widget edUndoPurgeLimit;
    Widget edUndoPurgeTrimTo;
    Widget edUndoWorryLimit;
    Widget edUndoWorryTrimTo;
    Widget edUndoOpLimit;
    Widget edUndoOpTrimTo;
} miscDialog;

static miscDialog md;

static void mdDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    
}

static void mdCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    XtDestroyWidget(md.shell);
    md.shell = NULL;
}

static void mdApplyCB(Widget w, XtPointer clientData, XtPointer callData) {
    int fsbView, fsbFileCmp;
    Boolean fsbShowHidden;
    XtVaGetValues(md.fdDefaultView, XmNselectedPosition, &fsbView, NULL);
    XtVaGetValues(md.fdSort, XmNselectedPosition, &fsbFileCmp, NULL);
    XtVaGetValues(md.fdShowHidden, XmNset, &fsbShowHidden, NULL);
    
    Boolean resetFsb = False;
    if(PrefData.fsbView != fsbView+1) {
        resetFsb = True;
    } else if(PrefData.fsbShowHidden != fsbShowHidden) {
        resetFsb = True;
    }
    SetPrefFsbView(fsbView+1);
    SetPrefFsbFileCmp(fsbFileCmp);
    SetPrefFsbShowHidden(fsbShowHidden);
    
    if(resetFsb) {
        FileDialogResetSettings();
    }
    
    int icSize = 0;
    XtVaGetValues(md.icSize, XmNselectedPosition, &icSize, NULL);
    
    if(icSize < 3) {
        if(icSize == 1) {
            SetPrefIconSize("medium");
        } else if(icSize == 2) {
            SetPrefIconSize("large");
        } else {
            SetPrefIconSize("small");
        }
    } else {
        int icClose, icFind, icClear;
        XtVaGetValues(md.icClose, XmNselectedPosition, &icClose, NULL);
        XtVaGetValues(md.icClear, XmNselectedPosition, &icClear, NULL);
        XtVaGetValues(md.icFind, XmNselectedPosition, &icFind, NULL);
        char *sizeStr[] = { "small", "medium", "large" };
        char iconSizeStr[128];
        snprintf(iconSizeStr, 128, "close=%s,isrcFind=%s,isrcClear=%s", sizeStr[icClose], sizeStr[icFind], sizeStr[icClear]);
        SetPrefIconSize(iconSizeStr);
    }
    
    XmString s1;
    XtVaGetValues(md.edZoom, XmNselectedItem, &s1, NULL);
    char *zoomStepStr = NULL;
    XmStringGetLtoR(s1, XmFONTLIST_DEFAULT_TAG, &zoomStepStr);
    int zoomStep = atoi(zoomStepStr);
    if(zoomStep == 0) {
        zoomStep = 1;
    }
    SetPrefZoomStep(zoomStep);
    XtFree(zoomStepStr);

    Boolean edZoomMouseWheel;
    XtVaGetValues(md.edZoomMouseWheelInvertedBehavior, XmNset, &edZoomMouseWheel, NULL);
    SetPrefZoomCtrlMouseWheel(edZoomMouseWheel);
    if (edZoomMouseWheel) {
        RadioButtonChangeState(md.edZoomMouseWheelInvertedBehavior, True, True);
    } else {
        RadioButtonChangeState(md.edZoomMouseWheelDefaultBehavior, True, True);
    }

    long undoOpLimit;
    long undoOpTrimTo;
    long undoPurgeLimit;
    long undoPurgeTrimTo;
    long undoWorryLimit;
    long undoWorryTrimTo;
    if(XNETextFieldGetInt(md.edUndoOpLimit, &undoOpLimit)) {
        SetPrefUndoOpLimit(undoOpLimit);
    }
    if(XNETextFieldGetInt(md.edUndoOpTrimTo, &undoOpTrimTo)) {
        SetPrefUndoOpTrimTo(undoOpTrimTo);
    }
    if(XNETextFieldGetInt(md.edUndoPurgeLimit, &undoPurgeLimit)) {
        SetPrefUndoPurgeLimit(undoPurgeLimit);
    }
    if(XNETextFieldGetInt(md.edUndoPurgeTrimTo, &undoPurgeTrimTo)) {
        SetPrefUndoPurgeTrimTo(undoPurgeTrimTo);
    }
    if(XNETextFieldGetInt(md.edUndoWorryLimit, &undoWorryLimit)) {
        SetPrefUndoWorryLimit(undoWorryLimit);
    }
    if(XNETextFieldGetInt(md.edUndoWorryTrimTo, &undoWorryTrimTo)) {
        SetPrefUndoWorryTrimTo(undoWorryTrimTo);
    }
    
}

static void mdOkCB(Widget w, XtPointer clientData, XtPointer callData) {
    mdApplyCB(w, clientData, callData);
    mdCloseCB(w, clientData, callData);
}

static void mdIconSizeCB(
        Widget w,
        XtPointer data,
        XmComboBoxCallbackStruct *cb)
{
    Boolean enable = cb->item_position > 2;
    XtSetSensitive(md.icClear, enable);
    XtSetSensitive(md.icClose, enable);
    XtSetSensitive(md.icFind, enable);
    
    if(!enable) {
        XmComboBoxSelectItem(md.icClear, cb->item_or_text);
        XmComboBoxSelectItem(md.icClose, cb->item_or_text);
        XmComboBoxSelectItem(md.icFind, cb->item_or_text);
    }
}

void MiscSettingsDialog(WindowInfo *window) {
    if(md.shell) {
        RaiseDialogWindow(md.shell);
        return;
    }
    
    int ac = 0;
    Arg args[20];
    
    //XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
    XtSetArg(args[ac], XmNtitle, "Miscellaneous Settings"); ac++;
    md.shell = CreateWidget(TheAppShell, "misc",
	    topLevelShellWidgetClass, args, ac);
    AddSmallIcon(md.shell);
    Widget form = XtVaCreateManagedWidget("form", xmFormWidgetClass,
	    md.shell, XmNautoUnmanage, False,
	    XmNresizePolicy, XmRESIZE_NONE, NULL);
    
    
    XmString s1;
    
    Widget okBtn = XtVaCreateManagedWidget("ok",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 4,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 31,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(okBtn, XmNactivateCallback, mdOkCB, NULL);
    XmStringFree(s1);

    Widget applyBtn = XtVaCreateManagedWidget("apply",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("Apply"),
            XmNmnemonic, 'A',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 35,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 64,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, mdApplyCB, NULL);
    XmStringFree(s1);

    Widget closeBtn = XtVaCreateManagedWidget("close",
            xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Close"),
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 68,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 96,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, mdCloseCB, NULL);
    XmStringFree(s1);
     
    md.scrolledwindow = XtVaCreateManagedWidget("sw", xmScrolledWindowWidgetClass, form,
            XmNwidth, 500,
            XmNheight, 500,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftOffset, 4,
            XmNrightOffset, 4,
            XmNtopOffset, 4,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, okBtn,
            XmNbottomOffset, 6,
            XmNscrollingPolicy, XmAUTOMATIC,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, closeBtn,
            XmNbottomOffset, 16,
            NULL);
      
    ac = 0;
    XtSetArg(args[ac], XmNwidth, 470); ac++;
    md.form = XmCreateForm(md.scrolledwindow, "form", args, ac);
    XtManageChild(md.form);
    
    // ----------------------- File Dialog Settings -----------------------
    s1 = XmStringCreateLocalized("File Dialog");
    Widget header1 = XtVaCreateManagedWidget("miscHeader", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 8,
            NULL);
    XmStringFree(s1);
    
    XmString fdViewItems[2];
    fdViewItems[0] = XmStringCreateLocalized("List");
    fdViewItems[1] = XmStringCreateLocalized("Detail");
    ac = 0;
    XtSetArg(args[ac], XmNitems, fdViewItems); ac++;
    XtSetArg(args[ac], XmNitemCount, 2); ac++;
    XtSetArg(args[ac], XmNcolumns, 12); ac++;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, header1); ac++;
    XtSetArg(args[ac], XmNtopOffset, 8); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    md.fdDefaultView = XmCreateDropDownList(md.form, "miscDropDown", args, ac);
    XtManageChild(md.fdDefaultView);
    XmStringFree(fdViewItems[0]);
    XmStringFree(fdViewItems[1]);
    
    s1 = XmStringCreateLocalized("Default View");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, header1,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.fdDefaultView,
            NULL);
    XmStringFree(s1);
    
    
    XmString fdSortItems[2];
    fdSortItems[0] = XmStringCreateLocalized("strcmp");
    fdSortItems[1] = XmStringCreateLocalized("strcasecmp");
    ac = 0;
    XtSetArg(args[ac], XmNitems, fdSortItems); ac++;
    XtSetArg(args[ac], XmNitemCount, 2); ac++;
    XtSetArg(args[ac], XmNcolumns, 12); ac++;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopOffset, 8); ac++;
    XtSetArg(args[ac], XmNtopWidget, md.fdDefaultView); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    md.fdSort = XmCreateDropDownList(md.form, "miscDropDown", args, ac);
    XtManageChild(md.fdSort);
    XmStringFree(fdSortItems[0]);
    XmStringFree(fdSortItems[1]);
    
    s1 = XmStringCreateLocalized("File Sorting");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.fdDefaultView,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.fdSort,
            NULL);
    XmStringFree(s1);
    
    s1 = XmStringCreateLocalized("Show hidden files");
    md.fdShowHidden = XtVaCreateManagedWidget("miscCheckbox", xmToggleButtonWidgetClass, md.form,
            XmNlabelString, s1,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.fdSort,
            XmNtopOffset, 10,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            NULL);
    
    // -----------------------    Icon Settings    -----------------------
    Widget separator1 = XtVaCreateManagedWidget("separator", xmSeparatorWidgetClass, md.form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.fdShowHidden,
            XmNtopOffset, 10,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, 8,
            NULL
            );
    
    s1 = XmStringCreateLocalized("Incremental Search Line / Tabbar");
    Widget header2 = XtVaCreateManagedWidget("miscHeader", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, separator1,
            XmNtopOffset, 14,
            NULL);
    XmStringFree(s1);
    
    XmString icSize[4];
    icSize[0] = XmStringCreateLocalized("Small");
    icSize[1] = XmStringCreateLocalized("Medium");
    icSize[2] = XmStringCreateLocalized("Large");
    icSize[3] = XmStringCreateLocalized("Individual");
    ac = 0;
    XtSetArg(args[ac], XmNitems, icSize); ac++;
    XtSetArg(args[ac], XmNitemCount, 4); ac++;
    XtSetArg(args[ac], XmNcolumns, 12); ac++;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, header2); ac++;
    XtSetArg(args[ac], XmNtopOffset, 8); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    md.icSize = XmCreateDropDownList(md.form, "miscDropDown", args, ac);
    XtManageChild(md.icSize);
    XtAddCallback(
            md.icSize,
            XmNselectionCallback,
            (XtCallbackProc)mdIconSizeCB,
            NULL);
    
    s1 = XmStringCreateLocalized("Icon Size");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, header2,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.icSize,
            NULL);
    XmStringFree(s1);
    
    ac = 0;
    XtSetArg(args[ac], XmNitems, icSize); ac++;
    XtSetArg(args[ac], XmNitemCount, 3); ac++;
    XtSetArg(args[ac], XmNcolumns, 12); ac++;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, md.icSize); ac++;
    XtSetArg(args[ac], XmNtopOffset, 8); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    md.icClose = XmCreateDropDownList(md.form, "miscDropDown", args, ac);
    XtManageChild(md.icClose);
    
    s1 = XmStringCreateLocalized("Tab Close Icon Size");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.icSize,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.icClose,
            NULL);
    XmStringFree(s1);
    
    ac = 0;
    XtSetArg(args[ac], XmNitems, icSize); ac++;
    XtSetArg(args[ac], XmNitemCount, 3); ac++;
    XtSetArg(args[ac], XmNcolumns, 12); ac++;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, md.icClose); ac++;
    XtSetArg(args[ac], XmNtopOffset, 8); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    md.icFind = XmCreateDropDownList(md.form, "miscDropDown", args, ac);
    XtManageChild(md.icFind);
    
    s1 = XmStringCreateLocalized("Find Icon Size");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.icClose,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.icFind,
            NULL);
    XmStringFree(s1);
    
    ac = 0;
    XtSetArg(args[ac], XmNitems, icSize); ac++;
    XtSetArg(args[ac], XmNitemCount, 3); ac++;
    XtSetArg(args[ac], XmNcolumns, 12); ac++;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, md.icFind); ac++;
    XtSetArg(args[ac], XmNtopOffset, 8); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    md.icClear = XmCreateDropDownList(md.form, "miscDropDown", args, ac);
    XtManageChild(md.icClear);
    
    s1 = XmStringCreateLocalized("Clear Icon Size");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.icFind,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.icClear,
            NULL);
    XmStringFree(s1);
    
    
    
    // -----------------------   Editor Settings   -----------------------
    Widget separator2 = XtVaCreateManagedWidget("separator", xmSeparatorWidgetClass, md.form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.icClear,
            XmNtopOffset, 10,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, 8,
            NULL
            );
    
    s1 = XmStringCreateLocalized("Text Editor");
    Widget header3 = XtVaCreateManagedWidget("miscHeader", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, separator2,
            XmNtopOffset, 14,
            NULL);
    XmStringFree(s1);
    
    XmString edSize[16];
    for(int i=0;i<16;i++) {
        char buf[8];
        snprintf(buf, 8, "%d", i+1);
        edSize[i] = XmStringCreateLocalized(buf);
    }
    ac = 0;
    XtSetArg(args[ac], XmNitems, edSize); ac++;
    XtSetArg(args[ac], XmNitemCount, 16); ac++;
    XtSetArg(args[ac], XmNcolumns, 12); ac++;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, header3); ac++;
    XtSetArg(args[ac], XmNtopOffset, 8); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    md.edZoom = XmCreateDropDownComboBox(md.form, "miscDropDown", args, ac);
    XtManageChild(md.edZoom);
    for(int i=0;i<16;i++) {
        XmStringFree(edSize[i]);
    } 
    
    s1 = XmStringCreateLocalized("Zoom Step");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, header3,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.edZoom,
            NULL);
    XmStringFree(s1);
    
    Widget pageZoomMouseWheelWrapper;
    
    pageZoomMouseWheelWrapper = XtVaCreateManagedWidget("pageZoomMouseWheelWrapper", xmRowColumnWidgetClass,
            md.form,
            XmNorientation, XmVERTICAL,
            XmNpacking, XmPACK_TIGHT,
            XmNradioBehavior, True,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopOffset, 10,
            XmNleftOffset, 8,
            XmNleftAttachment, XmATTACH_FORM,
            XmNtopWidget, md.edZoom,
            XmNbottomWidget, md.edUndoOpLimit,
            NULL);
    
    md.edZoomMouseWheelDefaultBehavior = XtVaCreateManagedWidget("CtrlPage_ShiftCtrlZoom",
            xmToggleButtonWidgetClass, pageZoomMouseWheelWrapper,
            XmNset, True,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateLocalized("Page with Ctrl+Mousewheel\nZoom with Shift+Ctrl+Mousewheel"),
            NULL);
    XmStringFree(s1);
    
    md.edZoomMouseWheelInvertedBehavior = XtVaCreateManagedWidget("CtrlZoom_ShiftCtrlPage",
            xmToggleButtonWidgetClass, pageZoomMouseWheelWrapper,
            XmNmarginHeight, 5,
            XmNlabelString, s1=XmStringCreateLocalized("Zoom with Ctrl+Mousewheel\nPage with Shift+Ctrl+Mousewheel"),
            NULL);
    XmStringFree(s1);
    
    md.edUndoOpLimit = XtVaCreateManagedWidget("miscTextField", XNEtextfieldWidgetClass, md.form,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, pageZoomMouseWheelWrapper,
            XmNtopOffset, 8,
            NULL);
    
    s1 = XmStringCreateLocalized("Undo Op Limit");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, pageZoomMouseWheelWrapper,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.edUndoOpLimit,
            XmNcolumns, 10,
            NULL);
    XmStringFree(s1);
    
    md.edUndoOpTrimTo = XtVaCreateManagedWidget("miscTextField", XNEtextfieldWidgetClass, md.form,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoOpLimit,
            XmNtopOffset, 8,
            NULL);
    
    s1 = XmStringCreateLocalized("Undo Op Trim To");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoOpLimit,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.edUndoOpTrimTo,
            XmNcolumns, 10,
            NULL);
    XmStringFree(s1);
    
    md.edUndoPurgeLimit = XtVaCreateManagedWidget("miscTextField", XNEtextfieldWidgetClass, md.form,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoOpTrimTo,
            XmNtopOffset, 8,
            NULL);
    
    s1 = XmStringCreateLocalized("Undo Purge Limit");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoOpTrimTo,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.edUndoPurgeLimit,
            XmNcolumns, 10,
            NULL);
    XmStringFree(s1);
    
    md.edUndoPurgeTrimTo = XtVaCreateManagedWidget("miscTextField", XNEtextfieldWidgetClass, md.form,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoPurgeLimit,
            XmNtopOffset, 8,
            NULL);
    
    s1 = XmStringCreateLocalized("Undo Purge Trim To");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoPurgeLimit,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.edUndoPurgeTrimTo,
            XmNcolumns, 10,
            NULL);
    XmStringFree(s1);
    
    md.edUndoWorryLimit = XtVaCreateManagedWidget("miscTextField", XNEtextfieldWidgetClass, md.form,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoPurgeTrimTo,
            XmNtopOffset, 8,
            NULL);
    
    s1 = XmStringCreateLocalized("Undo Worry Limit");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoPurgeTrimTo,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.edUndoWorryLimit,
            XmNcolumns, 10,
            NULL);
    XmStringFree(s1);
    
    md.edUndoWorryTrimTo = XtVaCreateManagedWidget("miscTextField", XNEtextfieldWidgetClass, md.form,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoWorryLimit,
            XmNtopOffset, 8,
            NULL);
    
    s1 = XmStringCreateLocalized("Undo Worry Trim To");
    XtVaCreateManagedWidget("miscLabel", xmLabelWidgetClass, md.form,
            XmNlabelString, s1,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 8,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, md.edUndoWorryLimit,
            XmNtopOffset, 8,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, md.edUndoWorryTrimTo,
            XmNcolumns, 10,
            NULL);
    XmStringFree(s1);
    
    
    // init data
    XmString fsbView = NULL;
    switch(PrefData.fsbView) {
        case 1: {
            fsbView = XmStringCreateSimple("List");
            break;
        }
        case 2: {
            fsbView = XmStringCreateSimple("Detail");
            break;
        }
    }
    if(fsbView) {
        XmComboBoxSelectItem(md.fdDefaultView, fsbView);
        XmStringFree(fsbView);
    }
    
    XmString fsbCmp = NULL;
    switch(PrefData.fsbFileCmp) {
        case 0: {
            fsbCmp = XmStringCreateSimple("strcmp");
            break;
        }
        case 1: {
            fsbCmp = XmStringCreateSimple("strcasecmp");
            break;
        }
    }
    if(fsbCmp) {
        XmComboBoxSelectItem(md.fdSort, fsbCmp);
        XmStringFree(fsbCmp);
    }
    
    XtVaSetValues(md.fdShowHidden, XmNset, PrefData.fsbShowHidden, NULL);
    if(PrefData.closeIconSize == PrefData.isrcClearIconSize && PrefData.isrcClearIconSize == PrefData.isrcFindIconSize) {
        if(PrefData.closeIconSize >= 0 && PrefData.closeIconSize <= 2) {
            XmComboBoxSelectItem(md.icSize, icSize[PrefData.closeIconSize]);
            XmComboBoxSelectItem(md.icClear, icSize[PrefData.closeIconSize]);
            XmComboBoxSelectItem(md.icFind, icSize[PrefData.closeIconSize]);
            XmComboBoxSelectItem(md.icClose, icSize[PrefData.closeIconSize]);
            XtSetSensitive(md.icClear, False);
            XtSetSensitive(md.icFind, False);
            XtSetSensitive(md.icClose, False);
        }
    } else {
        XmComboBoxSelectItem(md.icSize, icSize[3]); // individual
        
        if(PrefData.closeIconSize >= 0 && PrefData.closeIconSize <= 2) {
            XmComboBoxSelectItem(md.icClose, icSize[PrefData.closeIconSize]);
        }
        if(PrefData.isrcFindIconSize >= 0 && PrefData.isrcFindIconSize <= 2) {
            XmComboBoxSelectItem(md.icFind, icSize[PrefData.isrcFindIconSize]);
        }
        if(PrefData.isrcClearIconSize >= 0 && PrefData.isrcClearIconSize <= 2) {
            XmComboBoxSelectItem(md.icClear, icSize[PrefData.isrcClearIconSize]);
        }
    }
    
    char buf[16];
    int zoom = GetPrefZoomStep();
    snprintf(buf, 16, "%d", zoom);
    s1 = XmStringCreateSimple(buf);
    if(zoom >= 16) {
        XmComboBoxAddItem(md.edZoom, s1, 0, True);
    }
    XmComboBoxSelectItem(md.edZoom, s1);
    XmStringFree(s1);
    
    if (GetPrefZoomCtrlMouseWheel()) {
        RadioButtonChangeState(md.edZoomMouseWheelInvertedBehavior, True, True);
    } else {
        RadioButtonChangeState(md.edZoomMouseWheelDefaultBehavior, True, True);
    }
    
    
    XmStringFree(icSize[0]);
    XmStringFree(icSize[1]);
    XmStringFree(icSize[2]);
    
    
    XNETextFieldSetInt(md.edUndoOpLimit, GetPrefUndoOpLimit());
    XNETextFieldSetInt(md.edUndoOpTrimTo, GetPrefUndoOpTrimTo());
    XNETextFieldSetInt(md.edUndoPurgeLimit, GetPrefUndoPurgeLimit());
    XNETextFieldSetInt(md.edUndoPurgeTrimTo, GetPrefUndoPurgeTrimTo());
    XNETextFieldSetInt(md.edUndoWorryLimit, GetPrefUndoWorryLimit());
    XNETextFieldSetInt(md.edUndoWorryTrimTo, GetPrefUndoWorryTrimTo());
    
    
    
    XtAddCallback(form, XmNdestroyCallback, mdDestroyCB, NULL);
    AddMotifCloseCallback(md.shell, mdCloseCB, NULL);
    RealizeWithoutForcingPosition(md.shell);
}

