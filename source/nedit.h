/*******************************************************************************
*                                                                              *
* nedit.h -- Nirvana Editor Common Header File                                 *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
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

#ifndef NEDIT_NEDIT_H_INCLUDED
#define NEDIT_NEDIT_H_INCLUDED

#include "textBuf.h"
#include <sys/types.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/XmStrDefs.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#define NEDIT_VERSION           5
#define NEDIT_REVISION          7

/* Some default colors */
#define NEDIT_DEFAULT_FG        "black"
#define NEDIT_DEFAULT_TEXT_BG   "rgb:e5/e5/e5"
#define NEDIT_DEFAULT_SEL_FG    "black"
#define NEDIT_DEFAULT_SEL_BG    "rgb:cc/cc/cc"
#define NEDIT_DEFAULT_HI_FG     "white" /* These are colors for flashing */
#define NEDIT_DEFAULT_HI_BG     "red"   /*   matching parens. */
#define NEDIT_DEFAULT_LINENO_FG "black"
#define NEDIT_DEFAULT_CURSOR_FG "black"
#define NEDIT_DEFAULT_HELP_FG   "black"
#define NEDIT_DEFAULT_HELP_BG   "rgb:cc/cc/cc"


/* Tuning parameters */
#define SEARCHMAX 5119          /* Maximum length of search/replace strings */
#define MAX_SEARCH_HISTORY 100	/* Maximum length of search string history */
#define MAX_PANES 6		/* Max # of ADDITIONAL text editing panes
				   that can be added to a window */
#ifndef VMS
#define AUTOSAVE_CHAR_LIMIT 30	/* number of characters user can type before
				   NEdit generates a new backup file */
#else
#define AUTOSAVE_CHAR_LIMIT 80	/* set higher on VMS becaus saving is slower */
#endif /*VMS*/
#define AUTOSAVE_OP_LIMIT 8	/* number of distinct editing operations user
				   can do before NEdit gens. new backup file */
#define MAX_FONT_LEN 100	/* maximum length for a font name */
#define MAX_COLOR_LEN 30	/* maximum length for a color name */
#define MAX_MARKS 36	    	/* max. # of bookmarks (one per letter & #) */
#define MIN_LINE_NUM_COLS 4 	/* Min. # of columns in line number display */
#define APP_NAME "nedit"	/* application name for loading resources */
#define APP_CLASS "NEdit"	/* application class for loading resources */

/* The accumulated list of undo operations can potentially consume huge
   amounts of memory.  These tuning parameters determine how much undo infor-
   mation is retained.  Normally, the list is kept between UNDO_OP_LIMIT and
   UNDO_OP_TRIMTO in length (when the list reaches UNDO_OP_LIMIT, it is
   trimmed to UNDO_OP_TRIMTO then allowed to grow back to UNDO_OP_LIMIT).
   When there are very large amounts of saved text held in the list,
   UNDO_WORRY_LIMIT and UNDO_PURGE_LIMIT take over and cause the list to
   be trimmed back further to keep its size down. */
#define UNDO_PURGE_LIMIT 15000000 /* If undo list gets this large (in bytes),
				     trim it to length of UNDO_PURGE_TRIMTO */
#define UNDO_PURGE_TRIMTO 1	  /* Amount to trim the undo list in a purge */
#define UNDO_WORRY_LIMIT 2000000  /* If undo list gets this large (in bytes),
				     trim it to length of UNDO_WORRY_TRIMTO */
#define UNDO_WORRY_TRIMTO 5	  /* Amount to trim the undo list when memory
				     use begins to get serious */
#define UNDO_OP_LIMIT 400	  /* normal limit for length of undo list */
#define UNDO_OP_TRIMTO 200	  /* size undo list is normally trimmed to
				     when it exceeds UNDO_OP_TRIMTO in length */
#ifdef SGI_CUSTOM
#define MAX_SHORTENED_ITEMS 100   /* max. number of items excluded in short- */
#endif	    	    	    	  /*     menus mode */

typedef enum {NO_AUTO_INDENT, AUTO_INDENT, SMART_INDENT} IndentStyle;
typedef enum {NO_WRAP, NEWLINE_WRAP, CONTINUOUS_WRAP} WrapStyle;
typedef enum {NO_FLASH, FLASH_DELIMIT, FLASH_RANGE} ShowMatchingStyle;
enum virtKeyOverride { VIRT_KEY_OVERRIDE_NEVER, VIRT_KEY_OVERRIDE_AUTO,
                       VIRT_KEY_OVERRIDE_ALWAYS };

/*  This enum must be kept in parallel to the array TruncSubstitutionModes[]
    in preferences.c  */
enum truncSubstitution {TRUNCSUBST_SILENT, TRUNCSUBST_FAIL, TRUNCSUBST_WARN, TRUNCSUBST_IGNORE};

#define NO_FLASH_STRING		"off"
#define FLASH_DELIMIT_STRING	"delimiter"
#define FLASH_RANGE_STRING	"range"

#define CHARSET (XmStringCharSet)XmSTRING_DEFAULT_CHARSET

#define MKSTRING(string) \
	XmStringCreateLtoR(string, XmSTRING_DEFAULT_CHARSET)
	
#define SET_ONE_RSRC(widget, name, newValue) \
{ \
    static Arg args[1] = {{name, (XtArgVal)0}}; \
    args[0].value = (XtArgVal)newValue; \
    XtSetValues(widget, args, 1); \
}	

#define GET_ONE_RSRC(widget, name, valueAddr) \
{ \
    static Arg args[1] = {{name, (XtArgVal)0}}; \
    args[0].value = (XtArgVal)valueAddr; \
    XtGetValues(widget, args, 1); \
}

/* This handles all the different reasons files can be locked */
#define USER_LOCKED_BIT     0
#define PERM_LOCKED_BIT     1
#define TOO_MUCH_BINARY_DATA_LOCKED_BIT 2

#define LOCKED_BIT_TO_MASK(bitNum) (1 << (bitNum))
#define SET_LOCKED_BY_REASON(reasons, onOrOff, reasonBit) ((onOrOff) ? \
                    ((reasons) |= LOCKED_BIT_TO_MASK(reasonBit)) : \
                    ((reasons) &= ~LOCKED_BIT_TO_MASK(reasonBit)))

#define IS_USER_LOCKED(reasons) (((reasons) & LOCKED_BIT_TO_MASK(USER_LOCKED_BIT)) != 0)
#define SET_USER_LOCKED(reasons, onOrOff) SET_LOCKED_BY_REASON(reasons, onOrOff, USER_LOCKED_BIT)
#define IS_PERM_LOCKED(reasons) (((reasons) & LOCKED_BIT_TO_MASK(PERM_LOCKED_BIT)) != 0)
#define SET_PERM_LOCKED(reasons, onOrOff) SET_LOCKED_BY_REASON(reasons, onOrOff, PERM_LOCKED_BIT)
#define IS_TMBD_LOCKED(reasons) (((reasons) & LOCKED_BIT_TO_MASK(TOO_MUCH_BINARY_DATA_LOCKED_BIT)) != 0)
#define SET_TMBD_LOCKED(reasons, onOrOff) SET_LOCKED_BY_REASON(reasons, onOrOff, TOO_MUCH_BINARY_DATA_LOCKED_BIT)

#define IS_ANY_LOCKED_IGNORING_USER(reasons) (((reasons) & ~LOCKED_BIT_TO_MASK(USER_LOCKED_BIT)) != 0)
#define IS_ANY_LOCKED_IGNORING_PERM(reasons) (((reasons) & ~LOCKED_BIT_TO_MASK(PERM_LOCKED_BIT)) != 0)
#define IS_ANY_LOCKED(reasons) ((reasons) != 0)
#define CLEAR_ALL_LOCKS(reasons) ((reasons) = 0)

/* determine a safe size for a string to hold an integer-like number contained in xType */
#define TYPE_INT_STR_SIZE(xType) ((sizeof(xType) * 3) + 2)

/* Record on undo list */
typedef struct _UndoInfo {
    struct _UndoInfo *next;		/* pointer to the next undo record */
    int		type;
    int		startPos;
    int		endPos;
    int 	oldLen;
    char	*oldText;
    char	inUndo;			/* flag to indicate undo command on
    					   this record in progress.  Redirects
    					   SaveUndoInfo to save the next mod-
    					   ifications on the redo list instead
    					   of the undo list. */
    char	restoresToSaved;	/* flag to indicate undoing this
    	    	    	    	    	   operation will restore file to
    	    	    	    	    	   last saved (unmodified) state */
} UndoInfo;

/* Element in bookmark table */
typedef struct {
    char label;
    int cursorPos;
    selection sel;
} Bookmark;

/* Identifiers for the different colors that can be adjusted. */
enum colorTypes {
    TEXT_FG_COLOR,
    TEXT_BG_COLOR,
    SELECT_FG_COLOR,
    SELECT_BG_COLOR,
    HILITE_FG_COLOR,
    HILITE_BG_COLOR,
    LINENO_FG_COLOR,
    CURSOR_FG_COLOR,
    NUM_COLORS
};

/* cache user menus: manage mode of user menu list element */
typedef enum {
    UMMM_UNMANAGE,     /* user menu item is unmanaged */
    UMMM_UNMANAGE_ALL, /* user menu item is a sub menu and is
                          completely unmanaged (including nested
                          sub menus) */
    UMMM_MANAGE,       /* user menu item is managed; menu items
                          of potential sub menu are (un)managed
                          individually */
    UMMM_MANAGE_ALL    /* user menu item is a sub menu and is
                          completely managed */
} UserMenuManageMode;

/* structure representing one user menu item */
typedef struct _UserMenuListElement {
    UserMenuManageMode    umleManageMode;          /* current manage mode */
    UserMenuManageMode    umlePrevManageMode;      /* previous manage mode */
    char                 *umleAccKeys;             /* accelerator keys of item */
    Boolean               umleAccLockPatchApplied; /* indicates, if accelerator
                                                      lock patch is applied */
    Widget                umleMenuItem;            /* menu item represented by
                                                      this element */
    Widget                umleSubMenuPane;         /* holds menu pane, if item
                                                      represents a sub menu */
    struct _UserMenuList *umleSubMenuList;         /* elements of sub menu, if
                                                      item represents a sub menu */
} UserMenuListElement;

/* structure holding a list of user menu items */
typedef struct _UserMenuList {
    int                   umlNbrItems;
    UserMenuListElement **umlItems;
} UserMenuList;

/* structure holding cache info about Shell and Macro menus, which are
   shared over all "tabbed" documents (needed to manage/unmanage this
   user definable menus when language mode changes) */
typedef struct _UserMenuCache {
    int          umcLanguageMode;     /* language mode applied for shared
                                         user menus */
    Boolean      umcShellMenuCreated; /* indicating, if all shell menu items
                                         were created */
    Boolean      umcMacroMenuCreated; /* indicating, if all macro menu items
                                         were created */
    UserMenuList umcShellMenuList;    /* list of all shell menu items */
    UserMenuList umcMacroMenuList;    /* list of all macro menu items */
} UserMenuCache;

/* structure holding cache info about Background menu, which is
   owned by each document individually (needed to manage/unmanage this
   user definable menu when language mode changes) */
typedef struct _UserBGMenuCache {
    int          ubmcLanguageMode;    /* language mode applied for background
                                         user menu */
    Boolean      ubmcMenuCreated;     /* indicating, if all background menu
                                         items were created */
    UserMenuList ubmcMenuList;        /* list of all background menu items */
} UserBGMenuCache;

/* The WindowInfo structure holds the information on a Document. A number
   of 'tabbed' documents may reside within a shell window, hence some of 
   its members are of 'shell-level'; namely the find/replace dialogs, the
   menu bar & its associated members, the components on the stats area
   (i-search line, statsline and tab bar), plus probably a few others.
   See CreateWindow() and CreateDocument() for more info.
   
   Each document actually 'lives' within its splitPane widget member, 
   which can be raised to become the 'top' (visible) document by function
   RaiseDocument(). The non-top documents may still be accessed through
   macros, or the context menu on the tab bar.
   
   Prior to the introduction of tabbed mode, each window may house only 
   one document, making it effectively an 'editor window', hence the name
   WindowInfo. This struct name has been preserved to ease the transition
   when tabbed mode was introduced after NEdit 5.4.
*/
typedef struct _WindowInfo {
    struct _WindowInfo *next;
    Widget	shell;			/* application shell of window */
    Widget	mainWin;		/* main window of shell */
    Widget	splitPane;		/* paned win. for splitting text area */
    Widget	textArea;		/* the first text editing area widget */
    Widget	textPanes[MAX_PANES];	/* additional ones created on demand */
    Widget	lastFocus;		/* the last pane to have kbd. focus */
    Widget	statsLine;		/* file stats information display */
    Widget      statsLineForm;
    Widget      statsLineColNo;         /* Line/Column information display */
    Widget  	iSearchForm;	    	/* incremental search line widgets */
    Widget  	iSearchFindButton;
    Widget  	iSearchText;
    Widget  	iSearchClearButton;
    Widget  	iSearchRegexToggle;
    Widget  	iSearchCaseToggle;
    Widget  	iSearchRevToggle;
    Widget	menuBar;    	    	/* the main menu bar */
    Widget	tabBar;			/* tab bar for tabbed window */
    Widget	tab;			/* tab for this document */
    Widget	replaceDlog;		/* replace dialog */
    Widget	replaceText;		/* replace dialog settable widgets... */
    Widget	replaceWithText;
    Widget    	replaceCaseToggle;
    Widget	replaceWordToggle;    
    Widget	replaceRegexToggle;    
    Widget	replaceRevToggle;
    Widget	replaceKeepBtn;
    Widget	replaceBtns;
    Widget	replaceBtn;
    Widget	replaceAllBtn;
#ifndef REPLACE_SCOPE
    Widget      replaceInWinBtn;
    Widget	replaceInSelBtn;
#endif
    Widget	replaceSearchTypeBox;
    Widget	replaceFindBtn;
    Widget	replaceAndFindBtn;
    Widget	findDlog;		/* find dialog */
    Widget	findText;		/* find dialog settable widgets... */
    Widget      findCaseToggle;
    Widget      findWordToggle;    
    Widget      findRegexToggle;    
    Widget	findRevToggle;
    Widget	findKeepBtn;
    Widget	findBtns;
    Widget	findBtn;
    Widget	findSearchTypeBox;
    Widget	replaceMultiFileDlog;	/* Replace in multiple files */
    Widget	replaceMultiFileList;
    Widget	replaceMultiFilePathBtn;
    Widget	fontDialog;		/* NULL, unless font dialog is up */
    Widget	colorDialog;		/* NULL, unless color dialog is up */
    Widget	readOnlyItem;		/* menu bar settable widgets... */
    Widget	autoSaveItem;
    Widget	saveLastItem;
    Widget      openSelItem;
    Widget      newOppositeItem;
    Widget	closeItem;
    Widget	printSelItem;
    Widget	undoItem;
    Widget	redoItem;
    Widget	cutItem;
    Widget	delItem;
    Widget	copyItem;
    Widget	lowerItem;
    Widget	upperItem;
    Widget      findSelItem;
    Widget      findAgainItem;
    Widget	replaceFindAgainItem;
    Widget 	replaceAgainItem;
    Widget      gotoSelItem;
    Widget	langModeCascade;
    Widget	findDefItem;
    Widget	showTipItem;
    Widget	autoIndentOffItem;
    Widget	autoIndentItem;
    Widget	smartIndentItem;
    Widget  	noWrapItem;
    Widget  	newlineWrapItem;
    Widget  	continuousWrapItem;
    Widget	statsLineItem;
    Widget	iSearchLineItem;
    Widget	lineNumsItem;
    Widget	showMatchingOffItem;
    Widget	showMatchingDelimitItem;
    Widget	showMatchingRangeItem;
    Widget	matchSyntaxBasedItem;
    Widget	overtypeModeItem;
    Widget	highlightItem;
    Widget	windowMenuPane;
    Widget	shellMenuPane;
    Widget	macroMenuPane;
    Widget  	bgMenuPane;
    Widget  	tabMenuPane;
    Widget  	prevOpenMenuPane;
    Widget  	prevOpenMenuItem;
    Widget  	unloadTagsMenuPane;
    Widget  	unloadTagsMenuItem;
    Widget  	unloadTipsMenuPane;
    Widget  	unloadTipsMenuItem;
    Widget	filterItem;
    Widget	autoIndentOffDefItem;
    Widget	autoIndentDefItem;
    Widget	smartIndentDefItem;
    Widget	autoSaveDefItem;
    Widget	saveLastDefItem;
    Widget	noWrapDefItem;
    Widget	newlineWrapDefItem;
    Widget	contWrapDefItem;
    Widget	showMatchingOffDefItem;
    Widget	showMatchingDelimitDefItem;
    Widget	showMatchingRangeDefItem;
    Widget	matchSyntaxBasedDefItem;
    Widget	highlightOffDefItem;
    Widget	highlightDefItem;
    Widget	backlightCharsItem;
    Widget	backlightCharsDefItem;
    Widget	searchDlogsDefItem;
    Widget      beepOnSearchWrapDefItem;
    Widget	keepSearchDlogsDefItem;
    Widget	searchWrapsDefItem;
    Widget      appendLFItem;
    Widget	sortOpenPrevDefItem;
    Widget	allTagsDefItem;
    Widget	smartTagsDefItem;
    Widget	reposDlogsDefItem;
    Widget      autoScrollDefItem;
    Widget	openInTabDefItem;
    Widget	tabBarDefItem;
    Widget	tabBarHideDefItem;
    Widget	toolTipsDefItem;
    Widget	tabNavigateDefItem;
    Widget      tabSortDefItem;
    Widget	statsLineDefItem;
    Widget	iSearchLineDefItem;
    Widget	lineNumsDefItem;
    Widget	pathInWindowsMenuDefItem;
    Widget  	modWarnDefItem;
    Widget  	modWarnRealDefItem;
    Widget  	exitWarnDefItem;
    Widget	searchLiteralDefItem;
    Widget	searchCaseSenseDefItem;
    Widget	searchLiteralWordDefItem;
    Widget	searchCaseSenseWordDefItem;
    Widget	searchRegexNoCaseDefItem;
    Widget	searchRegexDefItem;
#ifdef REPLACE_SCOPE
    Widget	replScopeWinDefItem;
    Widget	replScopeSelDefItem;
    Widget	replScopeSmartDefItem;
#endif
    Widget	size24x80DefItem;
    Widget	size40x80DefItem;
    Widget	size60x80DefItem;
    Widget	size80x80DefItem;
    Widget	sizeCustomDefItem;
    Widget	cancelShellItem;
    Widget	learnItem;
    Widget	finishLearnItem;
    Widget	cancelMacroItem;
    Widget	replayItem;
    Widget	repeatItem;
    Widget	splitPaneItem;
    Widget	closePaneItem;
    Widget	detachDocumentItem;
    Widget	moveDocumentItem;
    Widget	contextMoveDocumentItem;
    Widget	contextDetachDocumentItem;
    Widget  	bgMenuUndoItem;
    Widget  	bgMenuRedoItem;
#ifdef SGI_CUSTOM
    Widget	shortMenusDefItem;
    Widget	toggleShortItems[MAX_SHORTENED_ITEMS]; /* Menu items to be
    	    	    	    	    	   managed and unmanaged to toggle
    	    	    	    	    	   short menus on and off */
    int     	nToggleShortItems;
#endif
    char	filename[MAXPATHLEN];	/* name component of file being edited*/
    char	path[MAXPATHLEN];	/* path component of file being edited*/
    unsigned	fileMode;		/* permissions of file being edited */
    uid_t	fileUid; 		/* last recorded user id of the file */
    gid_t	fileGid;		/* last recorded group id of the file */
    int     	fileFormat; 	    	/* whether to save the file straight
    	    	    	    	    	   (Unix format), or convert it to
					   MS DOS style with \r\n line breaks */
    time_t    	lastModTime; 	    	/* time of last modification to file */
    dev_t       device;                 /*  device where the file resides */
    ino_t       inode;                  /*  file's inode  */
    UndoInfo	*undo;			/* info for undoing last operation */
    UndoInfo	*redo;			/* info for redoing last undone op */
    textBuffer	*buffer;		/* holds the text being edited */
    int		nPanes;			/* number of additional text editing
    					   areas, created by splitWindow */
    int		autoSaveCharCount;	/* count of single characters typed
    					   since last backup file generated */
    int		autoSaveOpCount;	/* count of editing operations "" */
    int		undoOpCount;		/* count of stored undo operations */
    int		undoMemUsed;		/* amount of memory (in bytes)
    					   dedicated to the undo list */
    char	fontName[MAX_FONT_LEN];	/* names of the text fonts in use */
    char	italicFontName[MAX_FONT_LEN];
    char	boldFontName[MAX_FONT_LEN];
    char	boldItalicFontName[MAX_FONT_LEN];
    XmFontList	fontList;		/* fontList for the primary font */
    XFontStruct *italicFontStruct;	/* fontStructs for highlighting fonts */
    XFontStruct *boldFontStruct;
    XFontStruct *boldItalicFontStruct;
    XtIntervalId flashTimeoutID;	/* timer procedure id for getting rid
    					   of highlighted matching paren.  Non-
    					   zero val. means highlight is drawn */
    int		flashPos;		/* position saved for erasing matching
    					   paren highlight (if one is drawn) */
    int 	wasSelected;		/* last selection state (for dim/undim
    					   of selection related menu items */
    Boolean	filenameSet;		/* is the window still "Untitled"? */ 
    Boolean	fileChanged;		/* has window been modified? */
    Boolean     fileMissing;            /* is the window's file gone? */
    int         lockReasons;            /* all ways a file can be locked */
    Boolean	autoSave;		/* is autosave turned on? */
    Boolean	saveOldVersion;		/* keep old version in filename.bck */
    IndentStyle	indentStyle;		/* whether/how to auto indent */
    WrapStyle	wrapMode;		/* line wrap style: NO_WRAP,
    	    	    	    	    	   NEWLINE_WRAP or CONTINUOUS_WRAP */
    Boolean	overstrike;		/* is overstrike mode turned on ? */
    ShowMatchingStyle showMatchingStyle;/* How to show matching parens:
					   NO_FLASH, FLASH_DELIMIT, or
					   FLASH_RANGE */
    char	matchSyntaxBased;	/* Use syntax info to show matching */
    Boolean	showStats;		/* is stats line supposed to be shown */
    Boolean 	showISearchLine;    	/* is incr. search line to be shown */
    Boolean 	showLineNumbers;    	/* is the line number display shown */
    Boolean	highlightSyntax;	/* is syntax highlighting turned on? */
    Boolean	backlightChars;		/* is char backlighting turned on? */
    char	*backlightCharTypes;	/* what backlighting to use */
    Boolean	modeMessageDisplayed;	/* special stats line banner for learn
    					   and shell command executing modes */
    char	*modeMessage;		/* stats line banner content for learn
    					   and shell command executing modes */
    Boolean	ignoreModify;		/* ignore modifications to text area */
    Boolean	windowMenuValid;	/* is window menu up to date? */
    int		rHistIndex, fHistIndex;	/* history placeholders for */
    int     	iSearchHistIndex;	/*   find and replace dialogs */
    int     	iSearchStartPos;    	/* start pos. of current incr. search */
    int       	iSearchLastBeginPos;    /* beg. pos. last match of current i.s.*/
    int     	nMarks;     	    	/* number of active bookmarks */
    XtIntervalId markTimeoutID;	    	/* backup timer for mark event handler*/
    Bookmark	markTable[MAX_MARKS];	/* marked locations in window */
    void    	*highlightData; 	/* info for syntax highlighting */
    void    	*shellCmdData;  	/* when a shell command is executing,
    	    	    	    	    	   info. about it, otherwise, NULL */
    void    	*macroCmdData;  	/* same for macro commands */
    void    	*smartIndentData;   	/* compiled macros for smart indent */
    Atom	fileClosedAtom;         /* Atom used to tell nc that the file is closed */
    int    	languageMode;	    	/* identifies language mode currently
    	    	    	    	    	   selected in the window */
    Boolean	multiFileReplSelected;	/* selected during last multi-window 
					   replacement operation (history) */
    struct _WindowInfo**		/* temporary list of writable windows */
		writableWindows;	/* used during multi-file replacements */
    int		nWritableWindows;	/* number of elements in the list */
    Bool 	multiFileBusy;		/* suppresses multiple beeps/dialogs
					   during multi-file replacements */
    Bool 	replaceFailed;		/* flags replacements failures during
					   multi-file replacements */
    Bool	replaceLastRegexCase;   /* last state of the case sense button
                                           in regex mode for replace dialog */
    Bool	replaceLastLiteralCase; /* idem, for literal mode */
    Bool	iSearchLastRegexCase;   /* idem, for regex mode in 
                                           incremental search bar */
    Bool	iSearchLastLiteralCase; /* idem, for literal mode */
    Bool	findLastRegexCase; 	/* idem, for regex mode in find dialog */
    Bool	findLastLiteralCase;    /* idem, for literal mode */
    
#ifdef REPLACE_SCOPE
    int		replaceScope;		/* Current scope for replace dialog */
    Widget	replaceScopeWinToggle;	/* Scope for replace = window */
    Widget	replaceScopeSelToggle;	/* Scope for replace = selection */
    Widget	replaceScopeMultiToggle;/* Scope for replace = multiple files */
#endif
    UserMenuCache   *userMenuCache;     /* cache user menus: */
    UserBGMenuCache  userBGMenuCache;   /* shell & macro menu are shared over all
                                           "tabbed" documents, while each document
                                           has its own background menu. */
} WindowInfo;

extern WindowInfo *WindowList;
extern Display *TheDisplay;
extern Widget TheAppShell;
extern char *ArgV0;
extern Boolean IsServer;

#endif /* NEDIT_NEDIT_H_INCLUDED */
