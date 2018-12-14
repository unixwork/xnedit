/*******************************************************************************
*                                                                              *
* window.c -- Nirvana Editor window creation/deletion                          *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* May 10, 1991                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "window.h"
#include "textBuf.h"
#include "textSel.h"
#include "text.h"
#include "textDisp.h"
#include "textP.h"
#include "nedit.h"
#include "menu.h"
#include "file.h"
#include "search.h"
#include "undo.h"
#include "preferences.h"
#include "selection.h"
#include "server.h"
#include "shell.h"
#include "macro.h"
#include "highlight.h"
#include "smartIndent.h"
#include "userCmds.h"
#include "nedit.bm"
#include "n.bm"
#include "windowTitle.h"
#include "interpret.h"
#include "rangeset.h"
#include "../util/clearcase.h"
#include "../util/misc.h"
#include "../util/fileUtils.h"
#include "../util/utils.h"
#include "../util/fileUtils.h"
#include "../util/DialogF.h"
#include "../Xlt/BubbleButtonP.h"
#include "../Microline/XmL/Folder.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#include "../util/clearcase.h"
#endif /*VMS*/
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#ifdef __unix__
#include <sys/time.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/PanedW.h>
#include <Xm/PanedWP.h>
#include <Xm/RowColumnP.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/SelectioB.h>
#include <Xm/List.h>
#include <Xm/Protocols.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/PrimitiveP.h>
#include <Xm/Frame.h>
#include <Xm/CascadeB.h>
#ifdef EDITRES
#include <X11/Xmu/Editres.h>
/* extern void _XEditResCheckMessages(); */
#endif /* EDITRES */

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* Initial minimum height of a pane.  Just a fallback in case setPaneMinHeight
   (which may break in a future release) is not available */
#define PANE_MIN_HEIGHT 39

/* Thickness of 3D border around statistics and/or incremental search areas
   below the main menu bar */
#define STAT_SHADOW_THICKNESS 1

/* bitmap data for the close-tab button */
#define close_width 11
#define close_height 11
static unsigned char close_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x8c, 0x01, 0xdc, 0x01, 0xf8, 0x00, 0x70, 0x00,
   0xf8, 0x00, 0xdc, 0x01, 0x8c, 0x01, 0x00, 0x00, 0x00, 0x00};

/* bitmap data for the isearch-find button */
#define isrcFind_width 11
#define isrcFind_height 11
static unsigned char isrcFind_bits[] = {
   0xe0, 0x01, 0x10, 0x02, 0xc8, 0x04, 0x08, 0x04, 0x08, 0x04, 0x00, 0x04,
   0x18, 0x02, 0xdc, 0x01, 0x0e, 0x00, 0x07, 0x00, 0x03, 0x00};

/* bitmap data for the isearch-clear button */
#define isrcClear_width 11
#define isrcClear_height 11
static unsigned char isrcClear_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x84, 0x01, 0xc4, 0x00, 0x64, 0x00,
   0xc4, 0x00, 0x84, 0x01, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00};

extern void _XmDismissTearOff(Widget, XtPointer, XtPointer);

static void hideTooltip(Widget tab);
static Pixmap createBitmapWithDepth(Widget w, char *data, unsigned int width,
	unsigned int height);
static WindowInfo *getNextTabWindow(WindowInfo *window, int direction,
        int crossWin, int wrap);
static Widget addTab(Widget folder, const char *string);
static int compareWindowNames(const void *windowA, const void *windowB);
static int getTabPosition(Widget tab);
static Widget manageToolBars(Widget toolBarsForm);
static void hideTearOffs(Widget menuPane);
static void CloseDocumentWindow(Widget w, WindowInfo *window, XtPointer callData);
static void closeTabCB(Widget w, Widget mainWin, caddr_t callData);
static void raiseTabCB(Widget w, XtPointer clientData, XtPointer callData);
static Widget createTextArea(Widget parent, WindowInfo *window, int rows,
        int cols, int emTabDist, char *delimiters, int wrapMargin,
        int lineNumCols);
static void showStats(WindowInfo *window, int state);
static void showISearch(WindowInfo *window, int state);
static void showStatsForm(WindowInfo *window);
static void addToWindowList(WindowInfo *window);
static void removeFromWindowList(WindowInfo *window);
static void focusCB(Widget w, WindowInfo *window, XtPointer callData);
static void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled,
        const char *deletedText, void *cbArg);
static void movedCB(Widget w, WindowInfo *window, XtPointer callData);
static void dragStartCB(Widget w, WindowInfo *window, XtPointer callData);
static void dragEndCB(Widget w, WindowInfo *window, dragEndCBStruct *callData);
static void closeCB(Widget w, WindowInfo *window, XtPointer callData);
static void saveYourselfCB(Widget w, Widget appShell, XtPointer callData);
static void setPaneDesiredHeight(Widget w, int height);
static void setPaneMinHeight(Widget w, int min);
static void addWindowIcon(Widget shell);
static void wmSizeUpdateProc(XtPointer clientData, XtIntervalId *id);
static void getGeometryString(WindowInfo *window, char *geomString);
#ifdef ROWCOLPATCH
static void patchRowCol(Widget w);
static void patchedRemoveChild(Widget child);
#endif
static void refreshMenuBar(WindowInfo *window);
static void cloneDocument(WindowInfo *window, WindowInfo *orgWin);
static void cloneTextPanes(WindowInfo *window, WindowInfo *orgWin);
static UndoInfo *cloneUndoItems(UndoInfo *orgList);
static Widget containingPane(Widget w);

static WindowInfo *inFocusDocument = NULL;  	/* where we are now */
static WindowInfo *lastFocusDocument = NULL;	    	/* where we came from */
static int DoneWithMoveDocumentDialog;
static int updateLineNumDisp(WindowInfo* window);
static int updateGutterWidth(WindowInfo* window);
static void deleteDocument(WindowInfo *window);
static void cancelTimeOut(XtIntervalId *timer);

/* From Xt, Shell.c, "BIGSIZE" */
static const Dimension XT_IGNORE_PPOSITION = 32767;

/*
** Create a new editor window
*/
WindowInfo *CreateWindow(const char *name, char *geometry, int iconic)
{
    Widget winShell, mainWin, menuBar, pane, text, stats, statsAreaForm;
    Widget closeTabBtn, tabForm, form;
    WindowInfo *window;
    Pixel bgpix, fgpix;
    Arg al[20];
    int ac;
    XmString s1;
    XmFontList statsFontList;
    WindowInfo *win;
    char newGeometry[MAX_GEOM_STRING_LEN];
    unsigned int rows, cols;
    int x = 0, y = 0, bitmask, showTabBar, state;

    static Pixmap isrcFind = 0;
    static Pixmap isrcClear = 0;
    static Pixmap closeTabPixmap = 0;

    /* Allocate some memory for the new window data structure */
    window = (WindowInfo *)NEditMalloc(sizeof(WindowInfo));
    
    /* initialize window structure */
    /* + Schwarzenberg: should a 
      memset(window, 0, sizeof(WindowInfo));
         be added here ?
    */
    window->replaceDlog = NULL;
    window->replaceText = NULL;
    window->replaceWithText = NULL;
    window->replaceWordToggle = NULL;
    window->replaceCaseToggle = NULL;
    window->replaceRegexToggle = NULL;
    window->findDlog = NULL;
    window->findText = NULL;
    window->findWordToggle = NULL;
    window->findCaseToggle = NULL;
    window->findRegexToggle = NULL;
    window->replaceMultiFileDlog = NULL;
    window->replaceMultiFilePathBtn = NULL;
    window->replaceMultiFileList = NULL;
    window->multiFileReplSelected = FALSE;
    window->multiFileBusy = FALSE;
    window->writableWindows = NULL;
    window->nWritableWindows = 0;
    window->fileChanged = FALSE;
    window->fileMode = 0;
    window->fileUid = 0;
    window->fileGid = 0;
    window->filenameSet = FALSE;
    window->fileFormat = UNIX_FILE_FORMAT;
    window->lastModTime = 0;
    window->fileMissing = True;
    strcpy(window->filename, name);
    window->undo = NULL;
    window->redo = NULL;
    window->nPanes = 0;
    window->autoSaveCharCount = 0;
    window->autoSaveOpCount = 0;
    window->undoOpCount = 0;
    window->undoMemUsed = 0;
    CLEAR_ALL_LOCKS(window->lockReasons);
    window->indentStyle = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
    window->autoSave = GetPrefAutoSave();
    window->saveOldVersion = GetPrefSaveOldVersion();
    window->wrapMode = GetPrefWrap(PLAIN_LANGUAGE_MODE);
    window->overstrike = False;
    window->showMatchingStyle = GetPrefShowMatching();
    window->matchSyntaxBased = GetPrefMatchSyntaxBased();
    window->showStats = GetPrefStatsLine();
    window->showISearchLine = GetPrefISearchLine();
    window->showLineNumbers = GetPrefLineNums();
    window->highlightSyntax = GetPrefHighlightSyntax();
    window->backlightCharTypes = NULL;
    window->backlightChars = GetPrefBacklightChars();
    if (window->backlightChars) {
        char *cTypes = GetPrefBacklightCharTypes();
        if (cTypes && window->backlightChars) {
            if ((window->backlightCharTypes = (char*)NEditMalloc(strlen(cTypes) + 1)))
                strcpy(window->backlightCharTypes, cTypes);
        }
    }
    window->modeMessageDisplayed = FALSE;
    window->modeMessage = NULL;
    window->ignoreModify = FALSE;
    window->windowMenuValid = FALSE;
    window->flashTimeoutID = 0;
    window->fileClosedAtom = None;
    window->wasSelected = FALSE;

    strcpy(window->fontName, GetPrefFontName());
    strcpy(window->italicFontName, GetPrefItalicFontName());
    strcpy(window->boldFontName, GetPrefBoldFontName());
    strcpy(window->boldItalicFontName, GetPrefBoldItalicFontName());
    window->colorDialog = NULL;
    window->fontList = GetPrefFontList();
    window->italicFontStruct = GetPrefItalicFont();
    window->boldFontStruct = GetPrefBoldFont();
    window->boldItalicFontStruct = GetPrefBoldItalicFont();
    window->fontDialog = NULL;
    window->nMarks = 0;
    window->markTimeoutID = 0;
    window->highlightData = NULL;
    window->shellCmdData = NULL;
    window->macroCmdData = NULL;
    window->smartIndentData = NULL;
    window->languageMode = PLAIN_LANGUAGE_MODE;
    window->iSearchHistIndex = 0;
    window->iSearchStartPos = -1;
    window->replaceLastRegexCase   = TRUE;
    window->replaceLastLiteralCase = FALSE;
    window->iSearchLastRegexCase   = TRUE;
    window->iSearchLastLiteralCase = FALSE;
    window->findLastRegexCase      = TRUE;
    window->findLastLiteralCase    = FALSE;
    window->tab = NULL;
    window->device = 0;
    window->inode = 0;

    /* If window geometry was specified, split it apart into a window position
       component and a window size component.  Create a new geometry string
       containing the position component only.  Rows and cols are stripped off
       because we can't easily calculate the size in pixels from them until the
       whole window is put together.  Note that the preference resource is only
       for clueless users who decide to specify the standard X geometry
       application resource, which is pretty useless because width and height
       are the same as the rows and cols preferences, and specifying a window
       location will force all the windows to pile on top of one another */
    if (geometry == NULL || geometry[0] == '\0')
        geometry = GetPrefGeometry();
    if (geometry == NULL || geometry[0] == '\0') {
        rows = GetPrefRows();
        cols = GetPrefCols();
        newGeometry[0] = '\0';
    } else {
        bitmask = XParseGeometry(geometry, &x, &y, &cols, &rows);
        if (bitmask == 0)
            fprintf(stderr, "Bad window geometry specified: %s\n", geometry);
        else {
            if (!(bitmask & WidthValue))
                cols = GetPrefCols();
            if (!(bitmask & HeightValue))
                rows = GetPrefRows();
        }
        CreateGeometryString(newGeometry, x, y, 0, 0,
                bitmask & ~(WidthValue | HeightValue));
    }
    
    /* Create a new toplevel shell to hold the window */
    ac = 0;
    XtSetArg(al[ac], XmNtitle, name); ac++;
    XtSetArg(al[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
#ifdef SGI_CUSTOM
    if (strncmp(name, "Untitled", 8) == 0) { 
        XtSetArg(al[ac], XmNiconName, APP_NAME); ac++;
    } else {
        XtSetArg(al[ac], XmNiconName, name); ac++;
    }
#else
    XtSetArg(al[ac], XmNiconName, name); ac++;
#endif
    XtSetArg(al[ac], XmNgeometry, newGeometry[0]=='\0'?NULL:newGeometry); ac++;
    XtSetArg(al[ac], XmNinitialState,
            iconic ? IconicState : NormalState); ac++;

    if (newGeometry[0] == '\0')
    {
        /* Workaround to make Xt ignore Motif's bad PPosition size changes. Even
           though we try to remove the PPosition in RealizeWithoutForcingPosition,
           it is not sufficient.  Motif will recompute the size hints some point
           later and put PPosition back! If the window is mapped after that time,
           then the window will again wind up at 0, 0.  So, XEmacs does this, and
           now we do.

           Alternate approach, relying on ShellP.h:

           ((WMShellWidget)winShell)->shell.client_specified &= ~_XtShellPPositionOK;
         */

        XtSetArg(al[ac], XtNx, XT_IGNORE_PPOSITION); ac++;
        XtSetArg(al[ac], XtNy, XT_IGNORE_PPOSITION); ac++;
    }

    winShell = CreateWidget(TheAppShell, "textShell",
                topLevelShellWidgetClass, al, ac);
    window->shell = winShell;

#ifdef EDITRES
    XtAddEventHandler (winShell, (EventMask)0, True,
                (XtEventHandler)_XEditResCheckMessages, NULL);
#endif /* EDITRES */

#ifndef SGI_CUSTOM
    addWindowIcon(winShell);
#endif

    /* Create a MainWindow to manage the menubar and text area, set the
       userData resource to be used by WidgetToWindow to recover the
       window pointer from the widget id of any of the window's widgets */
    XtSetArg(al[ac], XmNuserData, window); ac++;
    mainWin = XmCreateMainWindow(winShell, "main", al, ac);
    window->mainWin = mainWin;
    XtManageChild(mainWin);
    
    /* The statsAreaForm holds the stats line and the I-Search line. */
    statsAreaForm = XtVaCreateWidget("statsAreaForm", 
            xmFormWidgetClass, mainWin,
            XmNmarginWidth, STAT_SHADOW_THICKNESS,
            XmNmarginHeight, STAT_SHADOW_THICKNESS,
            /* XmNautoUnmanage, False, */
            NULL);
    
    /* NOTE: due to a bug in openmotif 2.1.30, NEdit used to crash when
       the i-search bar was active, and the i-search text widget was focussed,
       and the window's width was resized to nearly zero. 
       In theory, it is possible to avoid this by imposing a minimum
       width constraint on the nedit windows, but that width would have to
       be at least 30 characters, which is probably unacceptable.
       Amazingly, adding a top offset of 1 pixel to the toggle buttons of 
       the i-search bar, while keeping the the top offset of the text widget 
       to 0 seems to avoid avoid the crash. */
       
    window->iSearchForm = XtVaCreateWidget("iSearchForm", 
       	    xmFormWidgetClass, statsAreaForm,
	    XmNshadowThickness, 0,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNleftOffset, STAT_SHADOW_THICKNESS,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNtopOffset, STAT_SHADOW_THICKNESS,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNrightOffset, STAT_SHADOW_THICKNESS,
	    XmNbottomOffset, STAT_SHADOW_THICKNESS, NULL);
    if(window->showISearchLine)
        XtManageChild(window->iSearchForm);

    /* Disable keyboard traversal of the find, clear and toggle buttons.  We
       were doing this previously by forcing the keyboard focus back to the
       text widget whenever a toggle changed.  That causes an ugly focus flash
       on screen.  It's better just not to go there in the first place. 
       Plus, if the user really wants traversal, it's an X resource so it
       can be enabled without too much pain and suffering. */
    
    if (isrcFind == 0) {
        isrcFind = createBitmapWithDepth(window->iSearchForm,
                (char *)isrcFind_bits, isrcFind_width, isrcFind_height);
    }
    window->iSearchFindButton = XtVaCreateManagedWidget("iSearchFindButton",
            xmPushButtonWidgetClass, window->iSearchForm,
            XmNlabelString, s1=XmStringCreateSimple("Find"),
            XmNlabelType, XmPIXMAP,
            XmNlabelPixmap, isrcFind,
            XmNtraversalOn, False,
            XmNmarginHeight, 1,
            XmNmarginWidth, 1,
            XmNleftAttachment, XmATTACH_FORM,
            /* XmNleftOffset, 3, */
            XmNleftOffset, 0,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 1,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 1,
            NULL);
    XmStringFree(s1);

    window->iSearchCaseToggle = XtVaCreateManagedWidget("iSearchCaseToggle",
            xmToggleButtonWidgetClass, window->iSearchForm,
            XmNlabelString, s1=XmStringCreateSimple("Case"),
            XmNset, GetPrefSearch() == SEARCH_CASE_SENSE 
            || GetPrefSearch() == SEARCH_REGEX
            || GetPrefSearch() == SEARCH_CASE_SENSE_WORD,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNtopOffset, 1, /* see openmotif note above */
            XmNrightAttachment, XmATTACH_FORM,
            XmNmarginHeight, 0, 
            XmNtraversalOn, False,
            NULL);
    XmStringFree(s1);
    
    window->iSearchRegexToggle = XtVaCreateManagedWidget("iSearchREToggle",
            xmToggleButtonWidgetClass, window->iSearchForm,
            XmNlabelString, s1=XmStringCreateSimple("RegExp"),
            XmNset, GetPrefSearch() == SEARCH_REGEX_NOCASE 
            || GetPrefSearch() == SEARCH_REGEX,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNtopOffset, 1, /* see openmotif note above */
            XmNrightAttachment, XmATTACH_WIDGET,
            XmNrightWidget, window->iSearchCaseToggle,
            XmNmarginHeight, 0,
            XmNtraversalOn, False,
            NULL);
    XmStringFree(s1);
    
    window->iSearchRevToggle = XtVaCreateManagedWidget("iSearchRevToggle",
            xmToggleButtonWidgetClass, window->iSearchForm,
            XmNlabelString, s1=XmStringCreateSimple("Rev"),
            XmNset, False,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNtopOffset, 1, /* see openmotif note above */
            XmNrightAttachment, XmATTACH_WIDGET,
            XmNrightWidget, window->iSearchRegexToggle,
            XmNmarginHeight, 0,
            XmNtraversalOn, False,
            NULL);
    XmStringFree(s1);
    
    if (isrcClear == 0) {
        isrcClear = createBitmapWithDepth(window->iSearchForm,
                (char *)isrcClear_bits, isrcClear_width, isrcClear_height);
    }
    window->iSearchClearButton = XtVaCreateManagedWidget("iSearchClearButton",
            xmPushButtonWidgetClass, window->iSearchForm,
            XmNlabelString, s1=XmStringCreateSimple("<x"),
            XmNlabelType, XmPIXMAP,
            XmNlabelPixmap, isrcClear,
            XmNtraversalOn, False,
            XmNmarginHeight, 1,
            XmNmarginWidth, 1,
            XmNrightAttachment, XmATTACH_WIDGET,
            XmNrightWidget, window->iSearchRevToggle,
            XmNrightOffset, 2,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 1,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 1,
            NULL);
    XmStringFree(s1);

    window->iSearchText = XtVaCreateManagedWidget("iSearchText",
            xmTextWidgetClass, window->iSearchForm,
            XmNmarginHeight, 1,
            XmNnavigationType, XmEXCLUSIVE_TAB_GROUP,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, window->iSearchFindButton,
            XmNrightAttachment, XmATTACH_WIDGET,
            XmNrightWidget, window->iSearchClearButton,
            /* XmNrightOffset, 5, */
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 0, /* see openmotif note above */
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 0, NULL);
    RemapDeleteKey(window->iSearchText);

    SetISearchTextCallbacks(window);

    /* create the a form to house the tab bar and close-tab button */
    tabForm = XtVaCreateWidget("tabForm", 
       	    xmFormWidgetClass, statsAreaForm,
	    XmNmarginHeight, 0,
	    XmNmarginWidth, 0,
	    XmNspacing, 0,
    	    XmNresizable, False, 
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
	    XmNshadowThickness, 0, NULL);

    /* button to close top document */
    if (closeTabPixmap == 0) {
        closeTabPixmap = createBitmapWithDepth(tabForm, 
	        (char *)close_bits, close_width, close_height);
    }
    closeTabBtn = XtVaCreateManagedWidget("closeTabBtn",
      	    xmPushButtonWidgetClass, tabForm,
	    XmNmarginHeight, 0,
	    XmNmarginWidth, 0,
    	    XmNhighlightThickness, 0,
	    XmNlabelType, XmPIXMAP,
	    XmNlabelPixmap, closeTabPixmap,
    	    XmNshadowThickness, 1,
            XmNtraversalOn, False,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, 3,
            XmNbottomAttachment, XmATTACH_FORM,	    
            XmNbottomOffset, 3,
	    NULL);
    XtAddCallback(closeTabBtn, XmNactivateCallback, (XtCallbackProc)closeTabCB, 
	    mainWin);
    
    /* create the tab bar */
    window->tabBar = XtVaCreateManagedWidget("tabBar", 
       	    xmlFolderWidgetClass, tabForm,
	    XmNresizePolicy, XmRESIZE_PACK,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 0,
	    XmNrightAttachment, XmATTACH_WIDGET,
	    XmNrightWidget, closeTabBtn,
            XmNrightOffset, 5,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 0,
            XmNtopAttachment, XmATTACH_FORM,
	    NULL);

    window->tabMenuPane = CreateTabContextMenu(window->tabBar, window);
    AddTabContextMenuAction(window->tabBar);
    
    /* create an unmanaged composite widget to get the folder
       widget to hide the 3D shadow for the manager area.
       Note: this works only on the patched XmLFolder widget */
    form = XtVaCreateWidget("form",
	    xmFormWidgetClass, window->tabBar,
	    XmNheight, 1,
	    XmNresizable, False,
	    NULL);

    XtAddCallback(window->tabBar, XmNactivateCallback,
    	    raiseTabCB, NULL);

    window->tab = addTab(window->tabBar, name);

    /* A form to hold the stats line text and line/col widgets */
    window->statsLineForm = XtVaCreateWidget("statsLineForm",
            xmFormWidgetClass, statsAreaForm,
            XmNshadowThickness, 0,
            XmNtopAttachment, window->showISearchLine ?
                XmATTACH_WIDGET : XmATTACH_FORM,
            XmNtopWidget, window->iSearchForm,
            XmNrightAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNresizable, False,    /*  */
            NULL);
    
    /* A separate display of the line/column number */
    window->statsLineColNo = XtVaCreateManagedWidget("statsLineColNo",
            xmLabelWidgetClass, window->statsLineForm,
            XmNlabelString, s1=XmStringCreateSimple("L: ---  C: ---"),
            XmNshadowThickness, 0,
            XmNmarginHeight, 2,
            XmNtraversalOn, False,
            XmNtopAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM, /*  */
            NULL);
    XmStringFree(s1);

    /* Create file statistics display area.  Using a text widget rather than
       a label solves a layout problem with the main window, which messes up
       if the label is too long (we would need a resize callback to control
       the length when the window changed size), and allows users to select
       file names and line numbers.  Colors are copied from parent
       widget, because many users and some system defaults color text
       backgrounds differently from other widgets. */
    XtVaGetValues(window->statsLineForm, XmNbackground, &bgpix, NULL);
    XtVaGetValues(window->statsLineForm, XmNforeground, &fgpix, NULL);
    stats = XtVaCreateManagedWidget("statsLine", 
            xmTextWidgetClass,  window->statsLineForm,
            XmNbackground, bgpix,
            XmNforeground, fgpix,
            XmNshadowThickness, 0,
            XmNhighlightColor, bgpix,
            XmNhighlightThickness, 0,  /* must be zero, for OM (2.1.30) to 
	                                  aligns tatsLineColNo & statsLine */
            XmNmarginHeight, 1,        /* == statsLineColNo.marginHeight - 1,
	                                  to align with statsLineColNo */
            XmNscrollHorizontal, False,
            XmNeditMode, XmSINGLE_LINE_EDIT,
            XmNeditable, False,
            XmNtraversalOn, False,
            XmNcursorPositionVisible, False,
            XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET, /*  */
            XmNtopWidget, window->statsLineColNo,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_WIDGET,
            XmNrightWidget, window->statsLineColNo,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, /*  */
            XmNbottomWidget, window->statsLineColNo,
            XmNrightOffset, 3,
            NULL);
    window->statsLine = stats;

    /* Give the statsLine the same font as the statsLineColNo */
    XtVaGetValues(window->statsLineColNo, XmNfontList, &statsFontList, NULL);
    XtVaSetValues(window->statsLine, XmNfontList, statsFontList, NULL);
    
    /* Manage the statsLineForm */
    if(window->showStats)
        XtManageChild(window->statsLineForm);
    
    /* If the fontList was NULL, use the magical default provided by Motif,
       since it must have worked if we've gotten this far */
    if (window->fontList == NULL)
        XtVaGetValues(stats, XmNfontList, &window->fontList, NULL);

    /* Create the menu bar */
    menuBar = CreateMenuBar(mainWin, window);
    window->menuBar = menuBar;
    XtManageChild(menuBar);
    
    /* Create paned window to manage split pane behavior */
    pane = XtVaCreateManagedWidget("pane", xmPanedWindowWidgetClass,  mainWin,
            XmNseparatorOn, False,
            XmNspacing, 3, XmNsashIndent, -2, NULL);
    window->splitPane = pane;
    XmMainWindowSetAreas(mainWin, menuBar, statsAreaForm, NULL, NULL, pane);
    
    /* Store a copy of document/window pointer in text pane to support
       action procedures. See also WidgetToWindow() for info. */
    XtVaSetValues(pane, XmNuserData, window, NULL);

    /* Patch around Motif's most idiotic "feature", that its menu accelerators
       recognize Caps Lock and Num Lock as modifiers, and don't trigger if
       they are engaged */ 
    AccelLockBugPatch(pane, window->menuBar);

    /* Create the first, and most permanent text area (other panes may
       be added & removed, but this one will never be removed */
    text = createTextArea(pane, window, rows,cols,
            GetPrefEmTabDist(PLAIN_LANGUAGE_MODE), GetPrefDelimiters(),
            GetPrefWrapMargin(), window->showLineNumbers?MIN_LINE_NUM_COLS:0);
    XtManageChild(text);
    window->textArea = text;
    window->lastFocus = text;

    /* Set the initial colors from the globals. */
    SetColors(window,
              GetPrefColorName(TEXT_FG_COLOR  ),
              GetPrefColorName(TEXT_BG_COLOR  ),
              GetPrefColorName(SELECT_FG_COLOR),
              GetPrefColorName(SELECT_BG_COLOR),
              GetPrefColorName(HILITE_FG_COLOR),
              GetPrefColorName(HILITE_BG_COLOR),
              GetPrefColorName(LINENO_FG_COLOR),
              GetPrefColorName(CURSOR_FG_COLOR));
    
    /* Create the right button popup menu (note: order is important here,
       since the translation for popping up this menu was probably already
       added in createTextArea, but CreateBGMenu requires window->textArea
       to be set so it can attach the menu to it (because menu shells are
       finicky about the kinds of widgets they are attached to)) */
    window->bgMenuPane = CreateBGMenu(window);
    
    /* cache user menus: init. user background menu cache */
    InitUserBGMenuCache(&window->userBGMenuCache);

    /* Create the text buffer rather than using the one created automatically
       with the text area widget.  This is done so the syntax highlighting
       modify callback can be called to synchronize the style buffer BEFORE
       the text display's callback is called upon to display a modification */
    window->buffer = BufCreate();
    BufAddModifyCB(window->buffer, SyntaxHighlightModifyCB, window);
    
    /* Attach the buffer to the text widget, and add callbacks for modify */
    TextSetBuffer(text, window->buffer);
    BufAddModifyCB(window->buffer, modifiedCB, window);
    
    /* Designate the permanent text area as the owner for selections */
    HandleXSelections(text);
    
    /* Set the requested hardware tab distance and useTabs in the text buffer */
    BufSetTabDistance(window->buffer, GetPrefTabDist(PLAIN_LANGUAGE_MODE));
    window->buffer->useTabs = GetPrefInsertTabs();

    /* add the window to the global window list, update the Windows menus */
    addToWindowList(window);
    InvalidateWindowMenus();

    showTabBar = GetShowTabBar(window);
    if (showTabBar)
    	XtManageChild(tabForm);

    manageToolBars(statsAreaForm);

    if (showTabBar || window->showISearchLine || 
    	    window->showStats)
        XtManageChild(statsAreaForm);
    
    /* realize all of the widgets in the new window */
    RealizeWithoutForcingPosition(winShell);
    XmProcessTraversal(text, XmTRAVERSE_CURRENT);

    /* Make close command in window menu gracefully prompt for close */
    AddMotifCloseCallback(winShell, (XtCallbackProc)closeCB, window);
    
    /* Make window resizing work in nice character heights */
    UpdateWMSizeHints(window);
    
    /* Set the minimum pane height for the initial text pane */
    UpdateMinPaneHeights(window);
    
    /* create dialogs shared by all documents in a window */
    CreateFindDlog(window->shell, window);
    CreateReplaceDlog(window->shell, window);
    CreateReplaceMultiFileDlog(window);

    /* dim/undim Attach_Tab menu items */
    state = NDocuments(window) < NWindows();
    for(win=WindowList; win; win=win->next) {
    	if (IsTopDocument(win)) {
    	    XtSetSensitive(win->moveDocumentItem, state);
    	    XtSetSensitive(win->contextMoveDocumentItem, state);
	}
    }
    
    return window;
}

/*
** ButtonPress event handler for tabs.
*/
static void tabClickEH(Widget w, XtPointer clientData, XEvent *event)
{
    /* hide the tooltip when user clicks with any button. */
    if (BubbleButton_Timer(w)) {
    	XtRemoveTimeOut(BubbleButton_Timer(w));
    	BubbleButton_Timer(w) = (XtIntervalId)NULL;
    }
    else {
        hideTooltip(w);
    }
}

/*
** add a tab to the tab bar for the new document.
*/
static Widget addTab(Widget folder, const char *string)
{
    Widget tooltipLabel, tab;
    XmString s1;

    s1 = XmStringCreateSimple((char *)string);
    tab = XtVaCreateManagedWidget("tab",
	    xrwsBubbleButtonWidgetClass, folder,
	    /* XmNmarginWidth, <default@nedit.c>, */
	    /* XmNmarginHeight, <default@nedit.c>, */
  	    /* XmNalignment, <default@nedit.c>, */
  	    XmNlabelString, s1,
  	    XltNbubbleString, s1,
	    XltNshowBubble, GetPrefToolTips(),
	    XltNautoParkBubble, True,
	    XltNslidingBubble, False,
	    /* XltNdelay, 800,*/
	    /* XltNbubbleDuration, 8000,*/
	    NULL);
    XmStringFree(s1);

    /* there's things to do as user click on the tab */
    XtAddEventHandler(tab, ButtonPressMask, False, 
            (XtEventHandler)tabClickEH, (XtPointer)0);

    /* BubbleButton simply use reversed video for tooltips,
       we try to use the 'standard' color */
    tooltipLabel = XtNameToWidget(tab, "*BubbleLabel");
    XtVaSetValues(tooltipLabel,
    	    XmNbackground, AllocateColor(tab, GetPrefTooltipBgColor()),
    	    XmNforeground, AllocateColor(tab, NEDIT_DEFAULT_FG),
	    NULL);

    /* put borders around tooltip. BubbleButton use 
       transientShellWidgetClass as tooltip shell, which
       came without borders */
    XtVaSetValues(XtParent(tooltipLabel), XmNborderWidth, 1, NULL);
	
#ifdef LESSTIF_VERSION
    /* If we don't do this, no popup when right-click on tabs */
    AddTabContextMenuAction(tab);
#endif /* LESSTIF_VERSION */

    return tab;
}
	    
/*
** Comparison function for sorting windows by title.
** Windows are sorted by alphabetically by filename and then 
** alphabetically by path.
*/
static int compareWindowNames(const void *windowA, const void *windowB)
{
    int rc;
    const WindowInfo *a = *((WindowInfo**)windowA);
    const WindowInfo *b = *((WindowInfo**)windowB);

    rc = strcmp(a->filename, b->filename);
    if (rc != 0)
	 return rc;
    rc = strcmp(a->path, b->path);
    return rc;
}

/*
** Sort tabs in the tab bar alphabetically, if demanded so.
*/
void SortTabBar(WindowInfo *window)
{
    WindowInfo *w;
    WindowInfo **windows;
    WidgetList tabList;
    int i, j, nDoc, tabCount;

    if (!GetPrefSortTabs())
    	return;
	
    /* need more than one tab to sort */
    nDoc = NDocuments(window);
    if (nDoc < 2)
        return;

    /* first sort the documents */
    windows = (WindowInfo **)NEditMalloc(sizeof(WindowInfo *) * nDoc);
    for (w=WindowList, i=0; w!=NULL; w=w->next) {
    	if (window->shell == w->shell)
    	    windows[i++] = w;
    }
    qsort(windows, nDoc, sizeof(WindowInfo *), compareWindowNames);

    /* assign tabs to documents in sorted order */
    XtVaGetValues(window->tabBar, XmNtabWidgetList, &tabList,
                  XmNtabCount, &tabCount, NULL);

    for (i=0, j=0; i<tabCount && j<nDoc; i++) {
        if (tabList[i]->core.being_destroyed)
            continue;

        /* set tab as active */
        if (IsTopDocument(windows[j]))
            XmLFolderSetActiveTab(window->tabBar, i, False);

        windows[j]->tab = tabList[i];
        RefreshTabState(windows[j]);
        j++;
    }
    
    NEditFree(windows);
}

/* 
** find which document a tab belongs to
*/
WindowInfo *TabToWindow(Widget tab)
{
    WindowInfo *win;
    for (win=WindowList; win; win=win->next) {
    	if (win->tab == tab)
	    return win;
    }

    return NULL;
}

/*
** Close a document, or an editor window
*/
void CloseWindow(WindowInfo *window)
{
    int keepWindow, state;
    char name[MAXPATHLEN];
    WindowInfo *win, *topBuf = NULL, *nextBuf = NULL;

    /* Free smart indent macro programs */
    EndSmartIndent(window);
    
    /* Clean up macro references to the doomed window.  If a macro is
       executing, stop it.  If macro is calling this (closing its own
       window), leave the window alive until the macro completes */
    keepWindow = !MacroWindowCloseActions(window);
    
#ifndef VMS
    /* Kill shell sub-process and free related memory */
    AbortShellCommand(window);
#endif /*VMS*/
    
    /* Unload the default tips files for this language mode if necessary */
    UnloadLanguageModeTipsFile(window);

    /* If a window is closed while it is on the multi-file replace dialog
       list of any other window (or even the same one), we must update those
       lists or we end up with dangling references. Normally, there can 
       be only one of those dialogs at the same time (application modal),
       but LessTif doesn't even (always) honor application modalness, so
       there can be more than one dialog. */
    RemoveFromMultiReplaceDialog(window);
    
    /* Destroy the file closed property for this file */
    DeleteFileClosedProperty(window);

    /* Remove any possibly pending callback which might fire after the 
       widget is gone. */
    cancelTimeOut(&window->flashTimeoutID);
    cancelTimeOut(&window->markTimeoutID);

    /* if this is the last window, or must be kept alive temporarily because
       it's running the macro calling us, don't close it, make it Untitled */
    if (keepWindow || (WindowList == window && window->next == NULL)) {
        window->filename[0] = '\0';
        UniqueUntitledName(name);
        CLEAR_ALL_LOCKS(window->lockReasons);
        window->fileMode = 0;
        window->fileUid = 0;
        window->fileGid = 0;
        strcpy(window->filename, name);
        strcpy(window->path, "");
        window->ignoreModify = TRUE;
        BufSetAll(window->buffer, "");
        window->ignoreModify = FALSE;
        window->nMarks = 0;
        window->filenameSet = FALSE;
        window->fileMissing = TRUE;
        window->fileChanged = FALSE;
        window->fileFormat = UNIX_FILE_FORMAT;
        window->lastModTime = 0;
        window->device = 0;
        window->inode = 0;

        StopHighlighting(window);
        EndSmartIndent(window);
        UpdateWindowTitle(window);
        UpdateWindowReadOnly(window);
        XtSetSensitive(window->closeItem, FALSE);
        XtSetSensitive(window->readOnlyItem, TRUE);
        XmToggleButtonSetState(window->readOnlyItem, FALSE, FALSE);
        ClearUndoList(window);
        ClearRedoList(window);
        XmTextSetString(window->statsLine, ""); /* resets scroll pos of stats
                                                   line from long file names */
        UpdateStatsLine(window);
        DetermineLanguageMode(window, True);
	RefreshTabState(window);
        updateLineNumDisp(window);
        return;
    }
    
    /* Free syntax highlighting patterns, if any. w/o redisplaying */
    FreeHighlightingData(window);
        
    /* remove the buffer modification callbacks so the buffer will be
       deallocated when the last text widget is destroyed */
    BufRemoveModifyCB(window->buffer, modifiedCB, window);
    BufRemoveModifyCB(window->buffer, SyntaxHighlightModifyCB, window);

#ifdef ROWCOLPATCH
    patchRowCol(window->menuBar);
#endif
    
    /* free the undo and redo lists */
    ClearUndoList(window);
    ClearRedoList(window);
    
    /* close the document/window */
    if (NDocuments(window) > 1) {
        if (MacroRunWindow() && MacroRunWindow() != window
                && MacroRunWindow()->shell == window->shell) {
            nextBuf = MacroRunWindow();
            RaiseDocument(nextBuf);
        } else if (IsTopDocument(window)) {
            /* need to find a successor before closing a top document */
            nextBuf = getNextTabWindow(window, 1, 0, 0);
            RaiseDocument(nextBuf);
        } else {
            topBuf = GetTopDocument(window->shell);         
        }
    }
    
    /* remove the window from the global window list, update window menus */
    removeFromWindowList(window);
    InvalidateWindowMenus();
    CheckCloseDim(); /* Close of window running a macro may have been disabled. */

    /* remove the tab of the closing document from tab bar */
    XtDestroyWidget(window->tab);

    /* refresh tab bar after closing a document */
    if (nextBuf) {
	ShowWindowTabBar(nextBuf);
        updateLineNumDisp(nextBuf);
    } else if (topBuf) {
	ShowWindowTabBar(topBuf);
        updateLineNumDisp(topBuf);
    }
    
    /* dim/undim Detach_Tab menu items */
    win = nextBuf? nextBuf : topBuf;
    if (win) {
	state = NDocuments(win) > 1;
    	XtSetSensitive(win->detachDocumentItem, state);
    	XtSetSensitive(win->contextDetachDocumentItem, state);
    }

    /* dim/undim Attach_Tab menu items */
    state = NDocuments(WindowList) < NWindows();
    for(win=WindowList; win; win=win->next) {
    	if (IsTopDocument(win)) {    
    	    XtSetSensitive(win->moveDocumentItem, state);
    	    XtSetSensitive(win->contextMoveDocumentItem, state);
	}
    }

    /* free background menu cache for document */
    FreeUserBGMenuCache(&window->userBGMenuCache);

    /* destroy the document's pane, or the window */
    if (nextBuf || topBuf) {
        deleteDocument(window);
    } else
    {
        /* free user menu cache for window */
        FreeUserMenuCache(window->userMenuCache);

	/* remove and deallocate all of the widgets associated with window */
    	NEditFree(window->backlightCharTypes); /* we made a copy earlier on */
	CloseAllPopupsFor(window->shell);
    	XtDestroyWidget(window->shell);
    }

    /* deallocate the window data structure */
    NEditFree(window);
}

/*
** check if tab bar is to be shown on this window
*/
int GetShowTabBar(WindowInfo *window)
{
    if (!GetPrefTabBar())
     	return False;
    else if (NDocuments(window) == 1)
    	return !GetPrefTabBarHideOne();
    else
    	return True;
}

void ShowWindowTabBar(WindowInfo *window)
{
    if (GetPrefTabBar()) {
	if (GetPrefTabBarHideOne())
	    ShowTabBar(window, NDocuments(window)>1);
	else 
	    ShowTabBar(window, True);
    }
    else
	ShowTabBar(window, False);
}

/*
** Check if there is already a window open for a given file
*/
WindowInfo *FindWindowWithFile(const char *name, const char *path)
{
    WindowInfo* window;

/* I don't think this algorithm will work on vms so I am
   disabling it for now */
#ifndef VMS
    if (!GetPrefHonorSymlinks())
    {
        char fullname[MAXPATHLEN + 1];
        struct stat attribute;

        strncpy(fullname, path, MAXPATHLEN);
        strncat(fullname, name, MAXPATHLEN);
        fullname[MAXPATHLEN] = '\0';

        if (0 == stat(fullname, &attribute)) {
            for (window = WindowList; window != NULL; window = window->next) {
                if (attribute.st_dev == window->device
                        && attribute.st_ino == window->inode) {
                    return window;
                }
            }
        }   /*  else:  Not an error condition, just a new file. Continue to check
                whether the filename is already in use for an unsaved document.  */
    }
#endif

    for (window = WindowList; window != NULL; window = window->next) {
        if (!strcmp(window->filename, name) && !strcmp(window->path, path)) {
            return window;
        }
    }
    return NULL;
}

/*
** Add another independently scrollable pane to the current document,
** splitting the pane which currently has keyboard focus.
*/
void SplitPane(WindowInfo *window)
{
    short paneHeights[MAX_PANES+1];
    int insertPositions[MAX_PANES+1], topLines[MAX_PANES+1];
    int horizOffsets[MAX_PANES+1];
    int i, focusPane, emTabDist, wrapMargin, lineNumCols, totalHeight=0;
    char *delimiters;
    Widget text = NULL;
    textDisp *textD, *newTextD;
    
    /* Don't create new panes if we're already at the limit */
    if (window->nPanes >= MAX_PANES)
        return;
    
    /* Record the current heights, scroll positions, and insert positions
       of the existing panes, keyboard focus */
    focusPane = 0;
    for (i=0; i<=window->nPanes; i++) {
        text = i==0 ? window->textArea : window->textPanes[i-1];
        insertPositions[i] = TextGetCursorPos(text);
        XtVaGetValues(containingPane(text),XmNheight,&paneHeights[i],NULL);
        totalHeight += paneHeights[i];
        TextGetScroll(text, &topLines[i], &horizOffsets[i]);
        if (text == window->lastFocus)
            focusPane = i;
    }
    
    /* Unmanage & remanage the panedWindow so it recalculates pane heights */
    XtUnmanageChild(window->splitPane);
    
    /* Create a text widget to add to the pane and set its buffer and
       highlight data to be the same as the other panes in the document */
    XtVaGetValues(window->textArea, textNemulateTabs, &emTabDist,
            textNwordDelimiters, &delimiters, textNwrapMargin, &wrapMargin,
            textNlineNumCols, &lineNumCols, NULL);
    text = createTextArea(window->splitPane, window, 1, 1, emTabDist,
            delimiters, wrapMargin, lineNumCols);
    
    TextSetBuffer(text, window->buffer);
    if (window->highlightData != NULL)
    	AttachHighlightToWidget(text, window);
    if (window->backlightChars)
    {
        XtVaSetValues(text, textNbacklightCharTypes,
                window->backlightCharTypes, NULL);
    }
    XtManageChild(text);
    window->textPanes[window->nPanes++] = text;

    /* Fix up the colors */
    textD = ((TextWidget)window->textArea)->text.textD;
    newTextD = ((TextWidget)text)->text.textD;
    XtVaSetValues(text,
                XmNforeground, textD->fgPixel,
                XmNbackground, textD->bgPixel,
                NULL);
    TextDSetColors( newTextD, textD->fgPixel, textD->bgPixel, 
            textD->selectFGPixel, textD->selectBGPixel, textD->highlightFGPixel,
            textD->highlightBGPixel, textD->lineNumFGPixel, 
            textD->cursorFGPixel );
    
    /* Set the minimum pane height in the new pane */
    UpdateMinPaneHeights(window);

    /* adjust the heights, scroll positions, etc., to split the focus pane */
    for (i=window->nPanes; i>focusPane; i--) {
        insertPositions[i] = insertPositions[i-1];
        paneHeights[i] = paneHeights[i-1];
        topLines[i] = topLines[i-1];
        horizOffsets[i] = horizOffsets[i-1];
    }
    paneHeights[focusPane] = paneHeights[focusPane]/2;
    paneHeights[focusPane+1] = paneHeights[focusPane];
    
    for (i=0; i<=window->nPanes; i++) {
        text = i==0 ? window->textArea : window->textPanes[i-1];
        setPaneDesiredHeight(containingPane(text), paneHeights[i]);
    }

    /* Re-manage panedWindow to recalculate pane heights & reset selection */
    if (IsTopDocument(window))
    	XtManageChild(window->splitPane);
    
    /* Reset all of the heights, scroll positions, etc. */
    for (i=0; i<=window->nPanes; i++) {
        text = i==0 ? window->textArea : window->textPanes[i-1];
        TextSetCursorPos(text, insertPositions[i]);
        TextSetScroll(text, topLines[i], horizOffsets[i]);
        setPaneDesiredHeight(containingPane(text),
                            totalHeight/(window->nPanes+1));
    }
    XmProcessTraversal(window->lastFocus, XmTRAVERSE_CURRENT);
    
    /* Update the window manager size hints after the sizes of the panes have
       been set (the widget heights are not yet readable here, but they will
       be by the time the event loop gets around to running this timer proc) */
    XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell), 0,
            wmSizeUpdateProc, window);
}

Widget GetPaneByIndex(WindowInfo *window, int paneIndex)
{
    Widget text = NULL;
    if (paneIndex >= 0 && paneIndex <= window->nPanes) {
        text = (paneIndex == 0) ? window->textArea : window->textPanes[paneIndex - 1];
    }
    return(text);
}

int WidgetToPaneIndex(WindowInfo *window, Widget w)
{
    int i;
    Widget text;
    int paneIndex = 0;
    
    for (i = 0; i <= window->nPanes; ++i) {
        text = (i == 0) ? window->textArea : window->textPanes[i - 1];
        if (text == w) {
            paneIndex = i;
        }
    }
    return(paneIndex);
}

/*
** Close the window pane that last had the keyboard focus.  (Actually, close
** the bottom pane and make it look like pane which had focus was closed)
*/
void ClosePane(WindowInfo *window)
{
    short paneHeights[MAX_PANES+1];
    int insertPositions[MAX_PANES+1], topLines[MAX_PANES+1];
    int horizOffsets[MAX_PANES+1];
    int i, focusPane;
    Widget text;
    
    /* Don't delete the last pane */
    if (window->nPanes <= 0)
        return;
    
    /* Record the current heights, scroll positions, and insert positions
       of the existing panes, and the keyboard focus */
    focusPane = 0;
    for (i=0; i<=window->nPanes; i++) {
        text = i==0 ? window->textArea : window->textPanes[i-1];
        insertPositions[i] = TextGetCursorPos(text);
        XtVaGetValues(containingPane(text),
                            XmNheight, &paneHeights[i], NULL);
        TextGetScroll(text, &topLines[i], &horizOffsets[i]);
        if (text == window->lastFocus)
            focusPane = i;
    }
    
    /* Unmanage & remanage the panedWindow so it recalculates pane heights */
    XtUnmanageChild(window->splitPane);
    
    /* Destroy last pane, and make sure lastFocus points to an existing pane.
       Workaround for OM 2.1.30: text widget must be unmanaged for 
       xmPanedWindowWidget to calculate the correct pane heights for
       the remaining panes, simply detroying it didn't seem enough */
    window->nPanes--;
    XtUnmanageChild(containingPane(window->textPanes[window->nPanes]));
    XtDestroyWidget(containingPane(window->textPanes[window->nPanes]));

    if (window->nPanes == 0)
        window->lastFocus = window->textArea;
    else if (focusPane > window->nPanes)
        window->lastFocus = window->textPanes[window->nPanes-1];
    
    /* adjust the heights, scroll positions, etc., to make it look
       like the pane with the input focus was closed */
    for (i=focusPane; i<=window->nPanes; i++) {
        insertPositions[i] = insertPositions[i+1];
        paneHeights[i] = paneHeights[i+1];
        topLines[i] = topLines[i+1];
        horizOffsets[i] = horizOffsets[i+1];
    }
    
    /* set the desired heights and re-manage the paned window so it will
       recalculate pane heights */
    for (i=0; i<=window->nPanes; i++) {
        text = i==0 ? window->textArea : window->textPanes[i-1];
        setPaneDesiredHeight(containingPane(text), paneHeights[i]);
    }

    if (IsTopDocument(window))
    	XtManageChild(window->splitPane);
    
    /* Reset all of the scroll positions, insert positions, etc. */
    for (i=0; i<=window->nPanes; i++) {
        text = i==0 ? window->textArea : window->textPanes[i-1];
        TextSetCursorPos(text, insertPositions[i]);
        TextSetScroll(text, topLines[i], horizOffsets[i]);
    }
    XmProcessTraversal(window->lastFocus, XmTRAVERSE_CURRENT);

    /* Update the window manager size hints after the sizes of the panes have
       been set (the widget heights are not yet readable here, but they will
       be by the time the event loop gets around to running this timer proc) */
    XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell), 0,
            wmSizeUpdateProc, window);
}

/*
** Turn on and off the display of line numbers
*/
void ShowLineNumbers(WindowInfo *window, int state)
{
    Widget text;
    int i, marginWidth;
    unsigned reqCols = 0;
    Dimension windowWidth;
    WindowInfo *win;
    textDisp *textD = ((TextWidget)window->textArea)->text.textD;
    
    if (window->showLineNumbers == state)
        return;
    window->showLineNumbers = state;
    
    /* Just setting window->showLineNumbers is sufficient to tell
       updateLineNumDisp() to expand the line number areas and the window
       size for the number of lines required.  To hide the line number
       display, set the width to zero, and contract the window width. */
    if (state) {
        reqCols = updateLineNumDisp(window);
    } else {
        XtVaGetValues(window->shell, XmNwidth, &windowWidth, NULL);
        XtVaGetValues(window->textArea,
	        textNmarginWidth, &marginWidth, NULL);
        XtVaSetValues(window->shell, XmNwidth,
                windowWidth - textD->left + marginWidth, NULL);
	
        for (i=0; i<=window->nPanes; i++) {
            text = i==0 ? window->textArea : window->textPanes[i-1];
            XtVaSetValues(text, textNlineNumCols, 0, NULL);
        }
    }

    /* line numbers panel is shell-level, hence other
       tabbed documents in the window should synch */
    for (win=WindowList; win; win=win->next) {
    	if (win->shell != window->shell || win == window)
	    continue;
	    
    	win->showLineNumbers = state;

        for (i=0; i<=win->nPanes; i++) {
            text = i==0 ? win->textArea : win->textPanes[i-1];
            /*  reqCols should really be cast here, but into what? XmRInt?  */
            XtVaSetValues(text, textNlineNumCols, reqCols, NULL);
        }               
    }
    
    /* Tell WM that the non-expandable part of the window has changed size */
    UpdateWMSizeHints(window);
}

void SetTabDist(WindowInfo *window, int tabDist)
{
    if (window->buffer->tabDist != tabDist) {
        int saveCursorPositions[MAX_PANES + 1];
        int saveVScrollPositions[MAX_PANES + 1];
        int saveHScrollPositions[MAX_PANES + 1];
        int paneIndex;
        
        window->ignoreModify = True;
        
        for (paneIndex = 0; paneIndex <= window->nPanes; ++paneIndex) {
            Widget w = GetPaneByIndex(window, paneIndex);
            textDisp *textD = ((TextWidget)w)->text.textD;

            TextGetScroll(w, &saveVScrollPositions[paneIndex], &saveHScrollPositions[paneIndex]);
            saveCursorPositions[paneIndex] = TextGetCursorPos(w);
            textD->modifyingTabDist = 1;
        }
        
        BufSetTabDistance(window->buffer, tabDist);

        for (paneIndex = 0; paneIndex <= window->nPanes; ++paneIndex) {
            Widget w = GetPaneByIndex(window, paneIndex);
            textDisp *textD = ((TextWidget)w)->text.textD;
            
            textD->modifyingTabDist = 0;
            TextSetCursorPos(w, saveCursorPositions[paneIndex]);
            TextSetScroll(w, saveVScrollPositions[paneIndex], saveHScrollPositions[paneIndex]);
        }
        
        window->ignoreModify = False;
    }
}

void SetEmTabDist(WindowInfo *window, int emTabDist)
{
    int i;

    XtVaSetValues(window->textArea, textNemulateTabs, emTabDist, NULL);
    for (i = 0; i < window->nPanes; ++i) {
        XtVaSetValues(window->textPanes[i], textNemulateTabs, emTabDist, NULL);
    }
}

/*
** Turn on and off the display of the statistics line
*/
void ShowStatsLine(WindowInfo *window, int state)
{
    WindowInfo *win;
    Widget text;
    int i;
    
    /* In continuous wrap mode, text widgets must be told to keep track of
       the top line number in absolute (non-wrapped) lines, because it can
       be a costly calculation, and is only needed for displaying line
       numbers, either in the stats line, or along the left margin */
    for (i=0; i<=window->nPanes; i++) {
        text = i==0 ? window->textArea : window->textPanes[i-1];
        TextDMaintainAbsLineNum(((TextWidget)text)->text.textD, state);
    }
    window->showStats = state;
    showStats(window, state);

    /* i-search line is shell-level, hence other tabbed
       documents in the window should synch */
    for (win=WindowList; win; win=win->next) {
    	if (win->shell != window->shell || win == window)
	    continue;
	win->showStats = state;
    }
}

/*
** Displays and undisplays the statistics line (regardless of settings of
** window->showStats or window->modeMessageDisplayed)
*/
static void showStats(WindowInfo *window, int state)
{
    if (state) {
        XtManageChild(window->statsLineForm);
        showStatsForm(window);
    } else {
        XtUnmanageChild(window->statsLineForm);
        showStatsForm(window);
    }
      
    /* Tell WM that the non-expandable part of the window has changed size */
    /* Already done in showStatsForm */
    /* UpdateWMSizeHints(window); */
}

/*
*/
static void showTabBar(WindowInfo *window, int state)
{
    if (state) {
	XtManageChild(XtParent(window->tabBar));
	showStatsForm(window);
    } else {
	XtUnmanageChild(XtParent(window->tabBar));
	showStatsForm(window);
    }
}

/*
*/
void ShowTabBar(WindowInfo *window, int state)
{
    if (XtIsManaged(XtParent(window->tabBar)) == state)
        return;
    showTabBar(window, state);
}

/*
** Turn on and off the continuing display of the incremental search line
** (when off, it is popped up and down as needed via TempShowISearch)
*/
void ShowISearchLine(WindowInfo *window, int state)
{
    WindowInfo *win;
    
    if (window->showISearchLine == state)
        return;
    window->showISearchLine = state;
    showISearch(window, state);

    /* i-search line is shell-level, hence other tabbed
       documents in the window should synch */
    for (win=WindowList; win; win=win->next) {
    	if (win->shell != window->shell || win == window)
	    continue;
	win->showISearchLine = state;
    }
}

/*
** Temporarily show and hide the incremental search line if the line is not
** already up.
*/
void TempShowISearch(WindowInfo *window, int state)
{
    if (window->showISearchLine)
        return;
    if (XtIsManaged(window->iSearchForm) != state)
        showISearch(window, state);
}

/*
** Put up or pop-down the incremental search line regardless of settings
** of showISearchLine or TempShowISearch
*/
static void showISearch(WindowInfo *window, int state)
{
    if (state) {
	XtManageChild(window->iSearchForm);
	showStatsForm(window);
    } else {
	XtUnmanageChild(window->iSearchForm);
	showStatsForm(window);
    }
      
    /* Tell WM that the non-expandable part of the window has changed size */
    /* This is already done in showStatsForm */
    /* UpdateWMSizeHints(window); */
}

/*
** Show or hide the extra display area under the main menu bar which
** optionally contains the status line and the incremental search bar
*/
static void showStatsForm(WindowInfo *window)
{
    Widget statsAreaForm = XtParent(window->statsLineForm);
    Widget mainW = XtParent(statsAreaForm);

    /* The very silly use of XmNcommandWindowLocation and XmNshowSeparator
       below are to kick the main window widget to position and remove the
       status line when it is managed and unmanaged.  At some Motif version
       level, the showSeparator trick backfires and leaves the separator
       shown, but fortunately the dynamic behavior is fixed, too so the
       workaround is no longer necessary, either.  (... the version where
       this occurs may be earlier than 2.1.  If the stats line shows
       double thickness shadows in earlier Motif versions, the #if XmVersion
       directive should be moved back to that earlier version) */
    if (manageToolBars(statsAreaForm)) {
        XtUnmanageChild(statsAreaForm);    /*... will this fix Solaris 7??? */
        XtVaSetValues(mainW, XmNcommandWindowLocation,
                XmCOMMAND_ABOVE_WORKSPACE, NULL);
#if XmVersion < 2001
        XtVaSetValues(mainW, XmNshowSeparator, True, NULL);
#endif
        XtManageChild(statsAreaForm);
        XtVaSetValues(mainW, XmNshowSeparator, False, NULL);
        UpdateStatsLine(window);
    } else {
        XtUnmanageChild(statsAreaForm);
        XtVaSetValues(mainW, XmNcommandWindowLocation,
                XmCOMMAND_BELOW_WORKSPACE, NULL);
    }
    
    /* Tell WM that the non-expandable part of the window has changed size */
    UpdateWMSizeHints(window);
}

/*
** Display a special message in the stats line (show the stats line if it
** is not currently shown).
*/
void SetModeMessage(WindowInfo *window, const char *message)
{
    /* this document may be hidden (not on top) or later made hidden,
       so we save a copy of the mode message, so we can restore the
       statsline when the document is raised to top again */
    window->modeMessageDisplayed = True;
    NEditFree(window->modeMessage);
    window->modeMessage = NEditStrdup(message);

    if (!IsTopDocument(window))
    	return;
	
    XmTextSetString(window->statsLine, (char*)message);
    /*
     * Don't invoke the stats line again, if stats line is already displayed.
     */
    if (!window->showStats)
	showStats(window, True);
}

/*
** Clear special statistics line message set in SetModeMessage, returns
** the statistics line to its original state as set in window->showStats
*/
void ClearModeMessage(WindowInfo *window)
{
    if (!window->modeMessageDisplayed)
    	return;

    window->modeMessageDisplayed = False;
    NEditFree(window->modeMessage);
    window->modeMessage = NULL;
    
    if (!IsTopDocument(window))
    	return;

    /*
     * Remove the stats line only if indicated by it's window state.
     */
    if (!window->showStats)
        showStats(window, False);
    UpdateStatsLine(window);
}

/*
** Count the windows
*/
int NWindows(void)
{
    WindowInfo *win;
    int n;
    
    for (win=WindowList, n=0; win!=NULL; win=win->next, n++);
    return n;
}

/*
** Set autoindent state to one of  NO_AUTO_INDENT, AUTO_INDENT, or SMART_INDENT.
*/
void SetAutoIndent(WindowInfo *window, IndentStyle state)
{
    int autoIndent = state == AUTO_INDENT, smartIndent = state == SMART_INDENT;
    int i;
    
    if (window->indentStyle == SMART_INDENT && !smartIndent)
        EndSmartIndent(window);
    else if (smartIndent && window->indentStyle != SMART_INDENT)
        BeginSmartIndent(window, True);
    window->indentStyle = state;
    XtVaSetValues(window->textArea, textNautoIndent, autoIndent,
            textNsmartIndent, smartIndent, NULL);
    for (i=0; i<window->nPanes; i++)
        XtVaSetValues(window->textPanes[i], textNautoIndent, autoIndent,
                textNsmartIndent, smartIndent, NULL);
    if (IsTopDocument(window)) {
	XmToggleButtonSetState(window->smartIndentItem, smartIndent, False);
	XmToggleButtonSetState(window->autoIndentItem, autoIndent, False);
	XmToggleButtonSetState(window->autoIndentOffItem,
	        state == NO_AUTO_INDENT, False);
    }
}

/*
** Set showMatching state to one of NO_FLASH, FLASH_DELIMIT or FLASH_RANGE.
** Update the menu to reflect the change of state.
*/
void SetShowMatching(WindowInfo *window, ShowMatchingStyle state)
{
    window->showMatchingStyle = state;
    if (IsTopDocument(window)) {
	XmToggleButtonSetState(window->showMatchingOffItem, 
            state == NO_FLASH, False);
	XmToggleButtonSetState(window->showMatchingDelimitItem, 
            state == FLASH_DELIMIT, False);
	XmToggleButtonSetState(window->showMatchingRangeItem, 
            state == FLASH_RANGE, False);
    }
}

/* 
** Update the "New (in X)" menu item to reflect the preferences
*/
void UpdateNewOppositeMenu(WindowInfo *window, int openInTab)
{
    XmString lbl;
    if ( openInTab )
        XtVaSetValues(window->newOppositeItem, 
                XmNlabelString, lbl=XmStringCreateSimple("New Window"), 
                XmNmnemonic, 'W', NULL);
    else
        XtVaSetValues(window->newOppositeItem, 
                XmNlabelString, lbl=XmStringCreateSimple("New Tab"), 
                XmNmnemonic, 'T', NULL);
    XmStringFree(lbl);
}

/*
** Set the fonts for "window" from a font name, and updates the display.
** Also updates window->fontList which is used for statistics line.
**
** Note that this leaks memory and server resources.  In previous NEdit
** versions, fontLists were carefully tracked and freed, but X and Motif
** have some kind of timing problem when widgets are distroyed, such that
** fonts may not be freed immediately after widget destruction with 100%
** safety.  Rather than kludge around this with timerProcs, I have chosen
** to create new fontLists only when the user explicitly changes the font
** (which shouldn't happen much in normal NEdit operation), and skip the
** futile effort of freeing them.
*/
void SetFonts(WindowInfo *window, const char *fontName, const char *italicName,
        const char *boldName, const char *boldItalicName)
{
    XFontStruct *font, *oldFont;
    int i, oldFontWidth, oldFontHeight, fontWidth, fontHeight;
    int borderWidth, borderHeight, marginWidth, marginHeight;
    int primaryChanged, highlightChanged = False;
    Dimension oldWindowWidth, oldWindowHeight, oldTextWidth, oldTextHeight;
    Dimension textHeight, newWindowWidth, newWindowHeight;
    textDisp *textD = ((TextWidget)window->textArea)->text.textD;
    
    /* Check which fonts have changed */
    primaryChanged = strcmp(fontName, window->fontName);
    if (strcmp(italicName, window->italicFontName)) highlightChanged = True;
    if (strcmp(boldName, window->boldFontName)) highlightChanged = True;
    if (strcmp(boldItalicName, window->boldItalicFontName))
        highlightChanged = True;
    if (!primaryChanged && !highlightChanged)
        return;
        
    /* Get information about the current window sizing, to be used to
       determine the correct window size after the font is changed */
    XtVaGetValues(window->shell, XmNwidth, &oldWindowWidth, XmNheight,
            &oldWindowHeight, NULL);
    XtVaGetValues(window->textArea, XmNheight, &textHeight,
            textNmarginHeight, &marginHeight, textNmarginWidth,
            &marginWidth, textNfont, &oldFont, NULL);
    oldTextWidth = textD->width + textD->lineNumWidth;
    oldTextHeight = textHeight - 2*marginHeight;
    for (i=0; i<window->nPanes; i++) {
        XtVaGetValues(window->textPanes[i], XmNheight, &textHeight, NULL);
        oldTextHeight += textHeight - 2*marginHeight;
    }
    borderWidth = oldWindowWidth - oldTextWidth;
    borderHeight = oldWindowHeight - oldTextHeight;
    oldFontWidth = oldFont->max_bounds.width;
    oldFontHeight = textD->ascent + textD->descent;
    
        
    /* Change the fonts in the window data structure.  If the primary font
       didn't work, use Motif's fallback mechanism by stealing it from the
       statistics line.  Highlight fonts are allowed to be NULL, which
       is interpreted as "use the primary font" */
    if (primaryChanged) {
        strcpy(window->fontName, fontName);
        font = XLoadQueryFont(TheDisplay, fontName);
        if (font == NULL)
            XtVaGetValues(window->statsLine, XmNfontList, &window->fontList,
                    NULL);
        else
            window->fontList = XmFontListCreate(font, XmSTRING_DEFAULT_CHARSET);
    }
    if (highlightChanged) {
        strcpy(window->italicFontName, italicName);
        window->italicFontStruct = XLoadQueryFont(TheDisplay, italicName);
        strcpy(window->boldFontName, boldName);
        window->boldFontStruct = XLoadQueryFont(TheDisplay, boldName);
        strcpy(window->boldItalicFontName, boldItalicName);
        window->boldItalicFontStruct = XLoadQueryFont(TheDisplay, boldItalicName);
    }

    /* Change the primary font in all the widgets */
    if (primaryChanged) {
        font = GetDefaultFontStruct(TheDisplay, window->fontList);
        XtVaSetValues(window->textArea, textNfont, font, NULL);
        for (i=0; i<window->nPanes; i++)
            XtVaSetValues(window->textPanes[i], textNfont, font, NULL);
    }
    
    /* Change the highlight fonts, even if they didn't change, because
       primary font is read through the style table for syntax highlighting */
    if (window->highlightData != NULL)
        UpdateHighlightStyles(window);
        
    /* Change the window manager size hints. 
       Note: this has to be done _before_ we set the new sizes. ICCCM2
       compliant window managers (such as fvwm2) would otherwise resize
       the window twice: once because of the new sizes requested, and once
       because of the new size increments, resulting in an overshoot. */
    UpdateWMSizeHints(window);
    
    /* Use the information from the old window to re-size the window to a
       size appropriate for the new font, but only do so if there's only
       _one_ document in the window, in order to avoid growing-window bug */
    if (NDocuments(window) == 1) {
	fontWidth = GetDefaultFontStruct(TheDisplay, window->fontList)->max_bounds.width;
	fontHeight = textD->ascent + textD->descent;
	newWindowWidth = (oldTextWidth*fontWidth) / oldFontWidth + borderWidth;
	newWindowHeight = (oldTextHeight*fontHeight) / oldFontHeight + 
	        borderHeight;
	XtVaSetValues(window->shell, XmNwidth, newWindowWidth, XmNheight,
        	newWindowHeight, NULL);
    }
    
    /* Change the minimum pane height */
    UpdateMinPaneHeights(window);
}

void SetColors(WindowInfo *window, const char *textFg, const char *textBg,
        const char *selectFg, const char *selectBg, const char *hiliteFg, 
        const char *hiliteBg, const char *lineNoFg, const char *cursorFg)
{
    int i, dummy;
    Pixel   textFgPix   = AllocColor( window->textArea, textFg, 
                    &dummy, &dummy, &dummy),
            textBgPix   = AllocColor( window->textArea, textBg, 
                    &dummy, &dummy, &dummy),
            selectFgPix = AllocColor( window->textArea, selectFg, 
                    &dummy, &dummy, &dummy),
            selectBgPix = AllocColor( window->textArea, selectBg, 
                    &dummy, &dummy, &dummy),
            hiliteFgPix = AllocColor( window->textArea, hiliteFg, 
                    &dummy, &dummy, &dummy),
            hiliteBgPix = AllocColor( window->textArea, hiliteBg, 
                    &dummy, &dummy, &dummy),
            lineNoFgPix = AllocColor( window->textArea, lineNoFg, 
                    &dummy, &dummy, &dummy),
            cursorFgPix = AllocColor( window->textArea, cursorFg, 
                    &dummy, &dummy, &dummy);
    textDisp *textD;

    /* Update the main pane */
    XtVaSetValues(window->textArea,
            XmNforeground, textFgPix,
            XmNbackground, textBgPix,
            NULL);
    textD = ((TextWidget)window->textArea)->text.textD;
    TextDSetColors( textD, textFgPix, textBgPix, selectFgPix, selectBgPix, 
            hiliteFgPix, hiliteBgPix, lineNoFgPix, cursorFgPix );
    /* Update any additional panes */
    for (i=0; i<window->nPanes; i++) {
        XtVaSetValues(window->textPanes[i],
                XmNforeground, textFgPix,
                XmNbackground, textBgPix,
                NULL);
        textD = ((TextWidget)window->textPanes[i])->text.textD;
        TextDSetColors( textD, textFgPix, textBgPix, selectFgPix, selectBgPix, 
                hiliteFgPix, hiliteBgPix, lineNoFgPix, cursorFgPix );
    }
    
    /* Redo any syntax highlighting */
    if (window->highlightData != NULL)
        UpdateHighlightStyles(window);
}

/*
** Set insert/overstrike mode
*/
void SetOverstrike(WindowInfo *window, int overstrike)
{
    int i;

    XtVaSetValues(window->textArea, textNoverstrike, overstrike, NULL);
    for (i=0; i<window->nPanes; i++)
        XtVaSetValues(window->textPanes[i], textNoverstrike, overstrike, NULL);
    window->overstrike = overstrike;
}

/*
** Select auto-wrap mode, one of NO_WRAP, NEWLINE_WRAP, or CONTINUOUS_WRAP
*/
void SetAutoWrap(WindowInfo *window, WrapStyle state)
{
    int i;
    int autoWrap = state == NEWLINE_WRAP, contWrap = state == CONTINUOUS_WRAP;

    XtVaSetValues(window->textArea, textNautoWrap, autoWrap,
            textNcontinuousWrap, contWrap, NULL);
    for (i=0; i<window->nPanes; i++)
        XtVaSetValues(window->textPanes[i], textNautoWrap, autoWrap,
                textNcontinuousWrap, contWrap, NULL);
    window->wrapMode = state;
    
    if (IsTopDocument(window)) {
	XmToggleButtonSetState(window->newlineWrapItem, autoWrap, False);
	XmToggleButtonSetState(window->continuousWrapItem, contWrap, False);
	XmToggleButtonSetState(window->noWrapItem, state == NO_WRAP, False);
    }
}

/* 
** Set the auto-scroll margin
*/
void SetAutoScroll(WindowInfo *window, int margin)
{
    int i;
    
    XtVaSetValues(window->textArea, textNcursorVPadding, margin, NULL);
    for (i=0; i<window->nPanes; i++)
        XtVaSetValues(window->textPanes[i], textNcursorVPadding, margin, NULL);
}
    
/*
** Recover the window pointer from any widget in the window, by searching
** up the widget hierarcy for the top level container widget where the
** window pointer is stored in the userData field. In a tabbed window,
** this is the window pointer of the top (active) document, which is 
** returned if w is 'shell-level' widget - menus, find/replace dialogs, etc.
**
** To support action routine in tabbed windows, a copy of the window
** pointer is also store in the splitPane widget.
*/
WindowInfo *WidgetToWindow(Widget w)
{
    WindowInfo *window = NULL;
    Widget parent;
    
    while (True) {
    	/* return window pointer of document */
    	if (XtClass(w) == xmPanedWindowWidgetClass)
	    break;
	    
	if (XtClass(w) == topLevelShellWidgetClass) {
    	    WidgetList items;

	    /* there should be only 1 child for the shell -
	       the main window widget */
    	    XtVaGetValues(w, XmNchildren, &items, NULL);
	    w = items[0];
	    break;
	}
	
    	parent = XtParent(w);
    	if (parent == NULL)
    	    return NULL;
	
	/* make sure it is not a dialog shell */
    	if (XtClass(parent) == topLevelShellWidgetClass &&
	    	XmIsMainWindow(w))
    	    break;

    	w = parent;
    }
    
    XtVaGetValues(w, XmNuserData, &window, NULL);

    return window;
}

/*
** Change the window appearance and the window data structure to show
** that the file it contains has been modified
*/
void SetWindowModified(WindowInfo *window, int modified)
{
    if (window->fileChanged == FALSE && modified == TRUE) {
    	SetSensitive(window, window->closeItem, TRUE);
    	window->fileChanged = TRUE;
    	UpdateWindowTitle(window);
	RefreshTabState(window);
    } else if (window->fileChanged == TRUE && modified == FALSE) {
    	window->fileChanged = FALSE;
    	UpdateWindowTitle(window);
	RefreshTabState(window);
    }
}

/*
** Update the window title to reflect the filename, read-only, and modified
** status of the window data structure
*/
void UpdateWindowTitle(const WindowInfo *window)
{
    char *iconTitle, *title;
    
    if (!IsTopDocument(window))
    	return;

    title = FormatWindowTitle(window->filename,
                                    window->path,
#ifdef VMS
                                    NULL,
#else
                                    GetClearCaseViewTag(),
#endif /* VMS */
                                    GetPrefServerName(),
                                    IsServer,
                                    window->filenameSet,
                                    window->lockReasons,
                                    window->fileChanged,
                                    GetPrefTitleFormat());
                   
    iconTitle = (char*)NEditMalloc(strlen(window->filename) + 2); /* strlen("*")+1 */

    strcpy(iconTitle, window->filename);
    if (window->fileChanged)
        strcat(iconTitle, "*");
    XtVaSetValues(window->shell, XmNtitle, title, XmNiconName, iconTitle, NULL);
    
    /* If there's a find or replace dialog up in "Keep Up" mode, with a
       file name in the title, update it too */
    if (window->findDlog && XmToggleButtonGetState(window->findKeepBtn)) {
        sprintf(title, "Find (in %s)", window->filename);
        XtVaSetValues(XtParent(window->findDlog), XmNtitle, title, NULL);
    }
    if (window->replaceDlog && XmToggleButtonGetState(window->replaceKeepBtn)) {
        sprintf(title, "Replace (in %s)", window->filename);
        XtVaSetValues(XtParent(window->replaceDlog), XmNtitle, title, NULL);
    }
    NEditFree(iconTitle);

    /* Update the Windows menus with the new name */
    InvalidateWindowMenus();
}

/*
** Update the read-only state of the text area(s) in the window, and
** the ReadOnly toggle button in the File menu to agree with the state in
** the window data structure.
*/
void UpdateWindowReadOnly(WindowInfo *window)
{
    int i, state;
    
    if (!IsTopDocument(window))
    	return;

    state = IS_ANY_LOCKED(window->lockReasons);
    XtVaSetValues(window->textArea, textNreadOnly, state, NULL);
    for (i=0; i<window->nPanes; i++)
        XtVaSetValues(window->textPanes[i], textNreadOnly, state, NULL);
    XmToggleButtonSetState(window->readOnlyItem, state, FALSE);
    XtSetSensitive(window->readOnlyItem,
        !IS_ANY_LOCKED_IGNORING_USER(window->lockReasons));
}

/*
** Find the start and end of a single line selection.  Hides rectangular
** selection issues for older routines which use selections that won't
** span lines.
*/
int GetSimpleSelection(textBuffer *buf, int *left, int *right)
{
    int selStart, selEnd, isRect, rectStart, rectEnd, lineStart;

    /* get the character to match and its position from the selection, or
       the character before the insert point if nothing is selected.
       Give up if too many characters are selected */
    if (!BufGetSelectionPos(buf, &selStart, &selEnd, &isRect,
            &rectStart, &rectEnd))
        return False;
    if (isRect) {
        lineStart = BufStartOfLine(buf, selStart);
        selStart = BufCountForwardDispChars(buf, lineStart, rectStart);
        selEnd = BufCountForwardDispChars(buf, lineStart, rectEnd);
    }
    *left = selStart;
    *right = selEnd;
    return True;
}

/*
** If the selection (or cursor position if there's no selection) is not
** fully shown, scroll to bring it in to view.  Note that as written,
** this won't work well with multi-line selections.  Modest re-write
** of the horizontal scrolling part would be quite easy to make it work
** well with rectangular selections.
*/
void MakeSelectionVisible(WindowInfo *window, Widget textPane)
{
    int left, right, isRect, rectStart, rectEnd, horizOffset;
    int scrollOffset, leftX, rightX, y, rows, margin;
    int topLineNum, lastLineNum, rightLineNum, leftLineNum, linesToScroll;
    textDisp *textD = ((TextWidget)textPane)->text.textD;
    int topChar = TextFirstVisiblePos(textPane);
    int lastChar = TextLastVisiblePos(textPane);
    int targetLineNum;
    Dimension width;

    /* find out where the selection is */
    if (!BufGetSelectionPos(window->buffer, &left, &right, &isRect,
            &rectStart, &rectEnd)) {
        left = right = TextGetCursorPos(textPane);
        isRect = False;
    }

    /* Check vertical positioning unless the selection is already shown or
       already covers the display.  If the end of the selection is below
       bottom, scroll it in to view until the end selection is scrollOffset
       lines from the bottom of the display or the start of the selection
       scrollOffset lines from the top.  Calculate a pleasing distance from the
       top or bottom of the window, to scroll the selection to (if scrolling is
       necessary), around 1/3 of the height of the window */
    if (!((left >= topChar && right <= lastChar) ||
            (left <= topChar && right >= lastChar))) {
        XtVaGetValues(textPane, textNrows, &rows, NULL);
        scrollOffset = rows/3;
        TextGetScroll(textPane, &topLineNum, &horizOffset);
        if (right > lastChar) {
            /* End of sel. is below bottom of screen */
            leftLineNum = topLineNum +
                    TextDCountLines(textD, topChar, left, False);
            targetLineNum = topLineNum + scrollOffset;
            if (leftLineNum >= targetLineNum) {
                /* Start of sel. is not between top & target */
                linesToScroll = TextDCountLines(textD, lastChar, right, False) +
                        scrollOffset;
                if (leftLineNum - linesToScroll < targetLineNum)
                    linesToScroll = leftLineNum - targetLineNum;
                /* Scroll start of selection to the target line */
                TextSetScroll(textPane, topLineNum+linesToScroll, horizOffset);
            }
        } else if (left < topChar) {
            /* Start of sel. is above top of screen */
            lastLineNum = topLineNum + rows;
            rightLineNum = lastLineNum -
                    TextDCountLines(textD, right, lastChar, False);
            targetLineNum = lastLineNum - scrollOffset;
            if (rightLineNum <= targetLineNum) {
                /* End of sel. is not between bottom & target */
                linesToScroll = TextDCountLines(textD, left, topChar, False) +
                        scrollOffset;
                if (rightLineNum + linesToScroll > targetLineNum)
                    linesToScroll = targetLineNum - rightLineNum;
                /* Scroll end of selection to the target line */
                TextSetScroll(textPane, topLineNum-linesToScroll, horizOffset);
            }
        }
    }

    /* If either end of the selection off screen horizontally, try to bring it
       in view, by making sure both end-points are visible.  Using only end
       points of a multi-line selection is not a great idea, and disaster for
       rectangular selections, so this part of the routine should be re-written
       if it is to be used much with either.  Note also that this is a second
       scrolling operation, causing the display to jump twice.  It's done after
       vertical scrolling to take advantage of TextPosToXY which requires it's
       reqested position to be vertically on screen) */
    if (    TextPosToXY(textPane, left, &leftX, &y) &&
            TextPosToXY(textPane, right, &rightX, &y) && leftX <= rightX) {
        TextGetScroll(textPane, &topLineNum, &horizOffset);
        XtVaGetValues(textPane, XmNwidth, &width, textNmarginWidth, &margin,
                NULL);
        if (leftX < margin + textD->lineNumLeft + textD->lineNumWidth)
            horizOffset -=
                    margin + textD->lineNumLeft + textD->lineNumWidth - leftX;
        else if (rightX > width - margin)
            horizOffset += rightX - (width - margin);
        TextSetScroll(textPane, topLineNum, horizOffset);
    }

    /* make sure that the statistics line is up to date */
    UpdateStatsLine(window);
}

static Widget createTextArea(Widget parent, WindowInfo *window, int rows,
        int cols, int emTabDist, char *delimiters, int wrapMargin,
        int lineNumCols)
{
    Widget text, sw, hScrollBar, vScrollBar, frame;
        
    /* Create a text widget inside of a scrolled window widget */
    sw = XtVaCreateManagedWidget("scrolledW", xmScrolledWindowWidgetClass,
            parent, XmNpaneMaximum, SHRT_MAX,
            XmNpaneMinimum, PANE_MIN_HEIGHT, XmNhighlightThickness, 0, NULL); 
    hScrollBar = XtVaCreateManagedWidget("textHorScrollBar",
            xmScrollBarWidgetClass, sw, XmNorientation, XmHORIZONTAL, 
            XmNrepeatDelay, 10, NULL);
    vScrollBar = XtVaCreateManagedWidget("textVertScrollBar",
            xmScrollBarWidgetClass, sw, XmNorientation, XmVERTICAL,
            XmNrepeatDelay, 10, NULL);
    frame = XtVaCreateManagedWidget("textFrame", xmFrameWidgetClass, sw,
            XmNshadowType, XmSHADOW_IN, NULL);      
    text = XtVaCreateManagedWidget("text", textWidgetClass, frame,
            textNbacklightCharTypes, window->backlightCharTypes,
            textNrows, rows, textNcolumns, cols,
            textNlineNumCols, lineNumCols,
            textNemulateTabs, emTabDist,
            textNfont, GetDefaultFontStruct(TheDisplay, window->fontList),
            textNhScrollBar, hScrollBar, textNvScrollBar, vScrollBar,
            textNreadOnly, IS_ANY_LOCKED(window->lockReasons),
            textNwordDelimiters, delimiters,
            textNwrapMargin, wrapMargin,
            textNautoIndent, window->indentStyle == AUTO_INDENT,
            textNsmartIndent, window->indentStyle == SMART_INDENT,
            textNautoWrap, window->wrapMode == NEWLINE_WRAP,
            textNcontinuousWrap, window->wrapMode == CONTINUOUS_WRAP,
            textNoverstrike, window->overstrike,
            textNhidePointer, (Boolean) GetPrefTypingHidesPointer(),
            textNcursorVPadding, GetVerticalAutoScroll(),
            NULL);

    XtVaSetValues(sw, XmNworkWindow, frame, XmNhorizontalScrollBar, 
                    hScrollBar, XmNverticalScrollBar, vScrollBar, NULL);

    /* add focus, drag, cursor tracking, and smart indent callbacks */
    XtAddCallback(text, textNfocusCallback, (XtCallbackProc)focusCB, window);
    XtAddCallback(text, textNcursorMovementCallback, (XtCallbackProc)movedCB,
            window);
    XtAddCallback(text, textNdragStartCallback, (XtCallbackProc)dragStartCB,
            window);
    XtAddCallback(text, textNdragEndCallback, (XtCallbackProc)dragEndCB,
            window);
    XtAddCallback(text, textNsmartIndentCallback, SmartIndentCB, window);
            
    /* This makes sure the text area initially has a the insert point shown
       ... (check if still true with the nedit text widget, probably not) */
    XmAddTabGroup(containingPane(text));
    
    /* compensate for Motif delete/backspace problem */
    RemapDeleteKey(text);

    /* Augment translation table for right button popup menu */
    AddBGMenuAction(text);
    
    /* If absolute line numbers will be needed for display in the statistics
       line, tell the widget to maintain them (otherwise, it's a costly
       operation and performance will be better without it) */
    TextDMaintainAbsLineNum(((TextWidget)text)->text.textD, window->showStats);
   
    return text;
}

static void movedCB(Widget w, WindowInfo *window, XtPointer callData) 
{
    TextWidget textWidget = (TextWidget) w;

    if (window->ignoreModify)
        return;

    /* update line and column nubers in statistics line */
    UpdateStatsLine(window);
    
    /* Check the character before the cursor for matchable characters */
    FlashMatching(window, w);
    
    /* Check for changes to read-only status and/or file modifications */
    CheckForChangesToFile(window);

    /*  This callback is not only called for focussed panes, but for newly
        created panes as well. So make sure that the cursor is left alone
        for unfocussed panes.
        TextWidget have no state per se about focus, so we use the related
        ID for the blink procedure.  */
    if (0 != textWidget->text.cursorBlinkProcID)
    {
        /*  Start blinking the caret again.  */
        ResetCursorBlink(textWidget, False);
    }
}

static void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled,
        const char *deletedText, void *cbArg) 
{
    WindowInfo *window = (WindowInfo *)cbArg;
    int selected = window->buffer->primary.selected;
    
    /* update the table of bookmarks */
    if (!window->ignoreModify) {
        UpdateMarkTable(window, pos, nInserted, nDeleted);
    }
    
    /* Check and dim/undim selection related menu items */
    if ((window->wasSelected && !selected) ||
        (!window->wasSelected && selected)) {
    	window->wasSelected = selected;
	
	/* do not refresh shell-level items (window, menu-bar etc)
	   when motifying non-top document */
        if (IsTopDocument(window)) {
    	    XtSetSensitive(window->printSelItem, selected);
    	    XtSetSensitive(window->cutItem, selected);
    	    XtSetSensitive(window->copyItem, selected);
            XtSetSensitive(window->delItem, selected);
            /* Note we don't change the selection for items like
               "Open Selected" and "Find Selected".  That's because
               it works on selections in external applications.
               Desensitizing it if there's no NEdit selection 
               disables this feature. */
#ifndef VMS
            XtSetSensitive(window->filterItem, selected);
#endif

            DimSelectionDepUserMenuItems(window, selected);
            if (window->replaceDlog != NULL && XtIsManaged(window->replaceDlog))
            {
        	UpdateReplaceActionButtons(window);
            }
	}
    }

    /* When the program needs to make a change to a text area without without
       recording it for undo or marking file as changed it sets ignoreModify */
    if (window->ignoreModify || (nDeleted == 0 && nInserted == 0))
        return;

    /* Make sure line number display is sufficient for new data */
    updateLineNumDisp(window);

    /* Save information for undoing this operation (this call also counts
       characters and editing operations for triggering autosave */
    SaveUndoInformation(window, pos, nInserted, nDeleted, deletedText);
    
    /* Trigger automatic backup if operation or character limits reached */
    if (window->autoSave &&
            (window->autoSaveCharCount > AUTOSAVE_CHAR_LIMIT ||
             window->autoSaveOpCount > AUTOSAVE_OP_LIMIT)) {
        WriteBackupFile(window);
        window->autoSaveCharCount = 0;
        window->autoSaveOpCount = 0;
    }
    
    /* Indicate that the window has now been modified */ 
    SetWindowModified(window, TRUE);

    /* Update # of bytes, and line and col statistics */
    UpdateStatsLine(window);
    
    /* Check if external changes have been made to file and warn user */
    CheckForChangesToFile(window);
}

static void focusCB(Widget w, WindowInfo *window, XtPointer callData) 
{
    /* record which window pane last had the keyboard focus */
    window->lastFocus = w;
    
    /* update line number statistic to reflect current focus pane */
    UpdateStatsLine(window);
    
    /* finish off the current incremental search */
    EndISearch(window);
    
    /* Check for changes to read-only status and/or file modifications */
    CheckForChangesToFile(window);
}

static void dragStartCB(Widget w, WindowInfo *window, XtPointer callData) 
{
    /* don't record all of the intermediate drag steps for undo */
    window->ignoreModify = True;
}

static void dragEndCB(Widget w, WindowInfo *window, dragEndCBStruct *callData) 
{
    /* restore recording of undo information */
    window->ignoreModify = False;
    
    /* Do nothing if drag operation was canceled */
    if (callData->nCharsInserted == 0)
        return;
        
    /* Save information for undoing this operation not saved while
       undo recording was off */
    modifiedCB(callData->startPos, callData->nCharsInserted,
            callData->nCharsDeleted, 0, callData->deletedText, window);
}

static void closeCB(Widget w, WindowInfo *window, XtPointer callData) 
{
    window = WidgetToWindow(w);
    if (!WindowCanBeClosed(window)) {
        return;
    }
    
    CloseDocumentWindow(w, window, callData);
}

#ifndef NO_SESSION_RESTART
static void saveYourselfCB(Widget w, Widget appShell, XtPointer callData)
{
    WindowInfo *win, *topWin, **revWindowList;
    char geometry[MAX_GEOM_STRING_LEN];
    int argc = 0, maxArgc, nWindows, i;
    char **argv;
    int wasIconic = False;
    int n, nItems;
    WidgetList children;

    /* Allocate memory for an argument list and for a reversed list of
       windows.  The window list is reversed for IRIX 4DWM and any other
       window/session manager combination which uses window creation
       order for re-associating stored geometry information with
       new windows created by a restored application */
    maxArgc = 4;  /* nedit -server -svrname name */
    nWindows = 0;
    for (win=WindowList; win!=NULL; win=win->next) {
        maxArgc += 5;  /* -iconic -group -geometry WxH+x+y filename */
        nWindows++;
    }
    argv = (char **)NEditMalloc(maxArgc*sizeof(char *));
    revWindowList = (WindowInfo **)NEditMalloc(sizeof(WindowInfo *)*nWindows);
    for (win=WindowList, i=nWindows-1; win!=NULL; win=win->next, i--)
        revWindowList[i] = win;
        
    /* Create command line arguments for restoring each window in the list */
    argv[argc++] = NEditStrdup(ArgV0);
    if (IsServer) {
        argv[argc++] = NEditStrdup("-server");
        if (GetPrefServerName()[0] != '\0') {
            argv[argc++] = NEditStrdup("-svrname");
            argv[argc++] = NEditStrdup(GetPrefServerName());
        }
    }

    /* editor windows are popup-shell children of top-level appShell */
    XtVaGetValues(appShell, XmNchildren, &children, 
    	    XmNnumChildren, &nItems, NULL);

    for (n=nItems-1; n>=0; n--) {
    	WidgetList tabs;
	int tabCount;
	
	if (strcmp(XtName(children[n]), "textShell") ||
	  ((topWin = WidgetToWindow(children[n])) == NULL))
	    continue;   /* skip non-editor windows */ 

	/* create a group for each window */
        getGeometryString(topWin, geometry);
        argv[argc++] = NEditStrdup("-group");
        argv[argc++] = NEditStrdup("-geometry");
        argv[argc++] = NEditStrdup(geometry);
        if (IsIconic(topWin)) {
            argv[argc++] = NEditStrdup("-iconic");
            wasIconic = True;
        } else if (wasIconic) {
            argv[argc++] = NEditStrdup("-noiconic");
            wasIconic = False;
	}
	
	/* add filename of each tab in window... */
    	XtVaGetValues(topWin->tabBar, XmNtabWidgetList, &tabs,
            	XmNtabCount, &tabCount, NULL);

    	for (i=0; i< tabCount; i++) {
	    win = TabToWindow(tabs[i]);
            if (win->filenameSet) {
		/* add filename */
        	argv[argc] = (char*)NEditMalloc(strlen(win->path) +
                	strlen(win->filename) + 1);
        	sprintf(argv[argc++], "%s%s", win->path, win->filename);
            }
	}
    }

    NEditFree(revWindowList);

    /* Set the window's WM_COMMAND property to the created command line */
    XSetCommand(TheDisplay, XtWindow(appShell), argv, argc);
    for (i=0; i<argc; i++)
        NEditFree(argv[i]);
    NEditFree(argv);
}

void AttachSessionMgrHandler(Widget appShell)
{
    static Atom wmpAtom, syAtom = 0;

    /* Add wm protocol callback for making nedit restartable by session
       managers.  Doesn't yet handle multiple-desktops or iconifying right. */
    if (syAtom == 0) {
        wmpAtom = XmInternAtom(TheDisplay, "WM_PROTOCOLS", FALSE);
        syAtom = XmInternAtom(TheDisplay, "WM_SAVE_YOURSELF", FALSE);
    }
    XmAddProtocolCallback(appShell, wmpAtom, syAtom,
            (XtCallbackProc)saveYourselfCB, (XtPointer)appShell);
}
#endif /* NO_SESSION_RESTART */
        
/*
** Returns true if window is iconic (as determined by the WM_STATE property
** on the shell window.  I think this is the most reliable way to tell,
** but if someone has a better idea please send me a note).
*/
int IsIconic(WindowInfo *window)
{
    unsigned long *property = NULL;
    unsigned long nItems;
    unsigned long leftover;
    static Atom wmStateAtom = 0;
    Atom actualType;
    int actualFormat;
    int result;
  
    if (wmStateAtom == 0)
        wmStateAtom = XInternAtom(XtDisplay(window->shell), "WM_STATE", False); 
    if (XGetWindowProperty(XtDisplay(window->shell), XtWindow(window->shell),
            wmStateAtom, 0L, 1L, False, wmStateAtom, &actualType, &actualFormat,
            &nItems, &leftover, (unsigned char **)&property) != Success ||
            nItems != 1 || property == NULL)
        return FALSE;
    result = *property == IconicState;
    NEditFree(property);
    return result;
}

/*
** Add a window to the the window list.
*/
static void addToWindowList(WindowInfo *window) 
{
    WindowInfo *temp;

    temp = WindowList;
    WindowList = window;
    window->next = temp;
}

/*
** Remove a window from the list of windows
*/
static void removeFromWindowList(WindowInfo *window)
{
    WindowInfo *temp;

    if (WindowList == window)
        WindowList = window->next;
    else {
        for (temp = WindowList; temp != NULL; temp = temp->next) {
            if (temp->next == window) {
                temp->next = window->next;
                break;
            }
        }
    }
}

/*
**  Set the new gutter width in the window. Sadly, the only way to do this is
**  to set it on every single document, so we have to iterate over them.
**
**  (Iteration taken from NDocuments(); is there a better way to do it?)
*/
static int updateGutterWidth(WindowInfo* window)
{
    WindowInfo* document;
    int reqCols = MIN_LINE_NUM_COLS;
    int newColsDiff = 0;
    int maxCols = 0;

    for (document = WindowList; NULL != document; document = document->next) {
        if (document->shell == window->shell) {
            /*  We found ourselves a document from this window.  */
            int lineNumCols, tmpReqCols;
            textDisp *textD = ((TextWidget) document->textArea)->text.textD;

            XtVaGetValues(document->textArea,
                    textNlineNumCols, &lineNumCols,
                    NULL);

            /* Is the width of the line number area sufficient to display all the
               line numbers in the file?  If not, expand line number field, and the
               window width. */

            if (lineNumCols > maxCols) {
                maxCols = lineNumCols;
            }

            tmpReqCols = textD->nBufferLines < 1
                    ? 1
                    : (int) log10((double) textD->nBufferLines + 1) + 1;

            if (tmpReqCols > reqCols) {
                reqCols = tmpReqCols;
            }
        }
    }

    if (reqCols != maxCols) {
        XFontStruct *fs;
        Dimension windowWidth;
        short fontWidth;

        newColsDiff = reqCols - maxCols;

        XtVaGetValues(window->textArea, textNfont, &fs, NULL);
        fontWidth = fs->max_bounds.width;

        XtVaGetValues(window->shell, XmNwidth, &windowWidth, NULL);
        XtVaSetValues(window->shell,
                XmNwidth, (Dimension) windowWidth + (newColsDiff * fontWidth),
                NULL);

        UpdateWMSizeHints(window);
    }

    for (document = WindowList; NULL != document; document = document->next) {
        if (document->shell == window->shell) {
            Widget text;
            int i;
            int lineNumCols;

            XtVaGetValues(document->textArea,
                textNlineNumCols, &lineNumCols, NULL);

            if (lineNumCols == reqCols) {
                continue;
            }

            /*  Update all panes of this document.  */
            for (i = 0; i <= document->nPanes; i++) {
                text = 0==i ? document->textArea : document->textPanes[i-1];
                XtVaSetValues(text, textNlineNumCols, reqCols, NULL);
            }
        }
    }

    return reqCols;
}

/*
**  If necessary, enlarges the window and line number display area to make
**  room for numbers.
*/
static int updateLineNumDisp(WindowInfo* window)
{
    if (!window->showLineNumbers) {
        return 0;
    }
    
    /* Decide how wide the line number field has to be to display all
       possible line numbers */
    return updateGutterWidth(window);
}

/*
** Update the optional statistics line.  
*/
void UpdateStatsLine(WindowInfo *window)
{
    int line, pos, colNum;
    char *string, *format, slinecol[32];
    Widget statW = window->statsLine;
    XmString xmslinecol;
#ifdef SGI_CUSTOM
    char *sleft, *smid, *sright;
#endif
    
    if (!IsTopDocument(window))
      return;

    /* This routine is called for each character typed, so its performance
       affects overall editor perfomance.  Only update if the line is on. */ 
    if (!window->showStats)
        return;
    
    /* Compose the string to display. If line # isn't available, leave it off */
    pos = TextGetCursorPos(window->lastFocus);
    string = (char*)NEditMalloc(strlen(window->filename) + strlen(window->path) + 45);
    format = window->fileFormat == DOS_FILE_FORMAT ? " DOS" :
            (window->fileFormat == MAC_FILE_FORMAT ? " Mac" : "");
    if (!TextPosToLineAndCol(window->lastFocus, pos, &line, &colNum)) {
        sprintf(string, "%s%s%s %d bytes", window->path, window->filename,
                format, window->buffer->length);
        sprintf(slinecol, "L: ---  C: ---");
    } else {
        sprintf(slinecol, "L: %d  C: %d", line, colNum);
        if (window->showLineNumbers)
            sprintf(string, "%s%s%s byte %d of %d", window->path,
                    window->filename, format, pos, 
                    window->buffer->length);
        else
            sprintf(string, "%s%s%s %d bytes", window->path,
                    window->filename, format, window->buffer->length);
    }
    
    /* Update the line/column number */
    xmslinecol = XmStringCreateSimple(slinecol);
    XtVaSetValues( window->statsLineColNo, 
            XmNlabelString, xmslinecol, NULL );
    XmStringFree(xmslinecol);
    
    /* Don't clobber the line if there's a special message being displayed */
    if (!window->modeMessageDisplayed) {
        /* Change the text in the stats line */
#ifdef SGI_CUSTOM
        /* don't show full pathname, just dir and filename (+ byte info) */
        smid = strchr(string, '/'); 
        if ( smid != NULL ) {
            sleft = smid;
            sright = strrchr(string, '/'); 
            while (strcmp(smid, sright)) {
                    sleft = smid;
                    smid = strchr(sleft + 1, '/');
            }
            XmTextReplace(statW, 0, XmTextGetLastPosition(statW), sleft + 1);
        } else
            XmTextReplace(statW, 0, XmTextGetLastPosition(statW), string);
#else
        XmTextReplace(statW, 0, XmTextGetLastPosition(statW), string);
#endif
    }
    NEditFree(string);
    
    /* Update the line/col display */
    xmslinecol = XmStringCreateSimple(slinecol);
    XtVaSetValues(window->statsLineColNo,
            XmNlabelString, xmslinecol, NULL);
    XmStringFree(xmslinecol);
}

static Boolean currentlyBusy = False;
static long busyStartTime = 0;
static Boolean modeMessageSet = False;

/*
 * Auxiliary function for measuring elapsed time during busy waits.
 */
static long getRelTimeInTenthsOfSeconds()
{
#ifdef __unix__
    struct timeval current;
    gettimeofday(&current, NULL);
    return (current.tv_sec*10 + current.tv_usec/100000) & 0xFFFFFFFL;
#else
    time_t current;
    time(&current);
    return (current*10) & 0xFFFFFFFL;
#endif
}

void AllWindowsBusy(const char *message)
{
    WindowInfo *w;

    if (!currentlyBusy)
    {
	busyStartTime = getRelTimeInTenthsOfSeconds();
	modeMessageSet = False;
        
        for (w=WindowList; w!=NULL; w=w->next)
        {
            /* We don't the display message here yet, but defer it for 
               a while. If the wait is short, we don't want
               to have it flash on and off the screen.  However,
               we can't use a time since in generally we are in
               a tight loop and only processing exposure events, so it's
               up to the caller to make sure that this routine is called 
               at regular intervals.
            */
            BeginWait(w->shell);
        }
    } else if (!modeMessageSet && message && 
		getRelTimeInTenthsOfSeconds() - busyStartTime > 10) {
	/* Show the mode message when we've been busy for more than a second */ 
	for (w=WindowList; w!=NULL; w=w->next) {
	    SetModeMessage(w, message);
	}
	modeMessageSet = True;
    }
    BusyWait(WindowList->shell);
            
    currentlyBusy = True;        
}

void AllWindowsUnbusy(void)
{
    WindowInfo *w;

    for (w=WindowList; w!=NULL; w=w->next)
    {
        ClearModeMessage(w);
        EndWait(w->shell);
    }

    currentlyBusy = False;
    modeMessageSet = False;
    busyStartTime = 0;
}

/*
** Paned windows are impossible to adjust after they are created, which makes
** them nearly useless for NEdit (or any application which needs to dynamically
** adjust the panes) unless you tweek some private data to overwrite the
** desired and minimum pane heights which were set at creation time.  These
** will probably break in a future release of Motif because of dependence on
** private data.
*/
static void setPaneDesiredHeight(Widget w, int height)
{
    ((XmPanedWindowConstraintPtr)w->core.constraints)->panedw.dheight = height;
}
static void setPaneMinHeight(Widget w, int min)
{
    ((XmPanedWindowConstraintPtr)w->core.constraints)->panedw.min = min;
}

/*
** Update the window manager's size hints.  These tell it the increments in
** which it is allowed to resize the window.  While this isn't particularly
** important for NEdit (since it can tolerate any window size), setting these
** hints also makes the resize indicator show the window size in characters
** rather than pixels, which is very helpful to users.
*/
void UpdateWMSizeHints(WindowInfo *window)
{
    Dimension shellWidth, shellHeight, textHeight, hScrollBarHeight;
    int marginHeight, marginWidth, totalHeight, nCols, nRows;
    XFontStruct *fs;
    int i, baseWidth, baseHeight, fontHeight, fontWidth;
    Widget hScrollBar;
    textDisp *textD = ((TextWidget)window->textArea)->text.textD;

    /* Find the dimensions of a single character of the text font */
    XtVaGetValues(window->textArea, textNfont, &fs, NULL);
    fontHeight = textD->ascent + textD->descent;
    fontWidth = fs->max_bounds.width;

    /* Find the base (non-expandable) width and height of the editor window.
    
       FIXME:
       To workaround the shrinking-window bug on some WM such as Metacity,
       which caused the window to shrink as we switch between documents 
       using different font sizes on the documents in the same window, the 
       base width, and similarly the base height, is ajusted such that:
            shellWidth = baseWidth + cols * textWidth
       There are two issues with this workaround:
       1. the right most characters may appear partially obsure
       2. the Col x Row info reported by the WM will be based on the fully
          display text.
    */
    XtVaGetValues(window->textArea, XmNheight, &textHeight,
            textNmarginHeight, &marginHeight, textNmarginWidth, &marginWidth,
            NULL);
    totalHeight = textHeight - 2*marginHeight;
    for (i=0; i<window->nPanes; i++) {
        XtVaGetValues(window->textPanes[i], XmNheight, &textHeight, 
                textNhScrollBar, &hScrollBar, NULL);
        totalHeight += textHeight - 2*marginHeight;
        if (!XtIsManaged(hScrollBar)) {
            XtVaGetValues(hScrollBar, XmNheight, &hScrollBarHeight, NULL);
            totalHeight -= hScrollBarHeight;
        }
    }
    
    XtVaGetValues(window->shell, XmNwidth, &shellWidth,
            XmNheight, &shellHeight, NULL);
    nCols = textD->width / fontWidth;
    nRows = totalHeight / fontHeight;
    baseWidth = shellWidth - nCols * fontWidth;
    baseHeight = shellHeight - nRows * fontHeight;

    /* Set the size hints in the shell widget */
    XtVaSetValues(window->shell, XmNwidthInc, fs->max_bounds.width,
            XmNheightInc, fontHeight,
            XmNbaseWidth, baseWidth, XmNbaseHeight, baseHeight,
            XmNminWidth, baseWidth + fontWidth,
            XmNminHeight, baseHeight + (1+window->nPanes) * fontHeight, NULL);

    /* Motif will keep placing this on the shell every time we change it,
       so it needs to be undone every single time.  This only seems to
       happen on mult-head dispalys on screens 1 and higher. */

    RemovePPositionHint(window->shell);
}

/*
** Update the minimum allowable height for a split pane after a change
** to font or margin height.
*/
void UpdateMinPaneHeights(WindowInfo *window)
{
    textDisp *textD = ((TextWidget)window->textArea)->text.textD;
    Dimension hsbHeight, swMarginHeight,frameShadowHeight;
    int i, marginHeight, minPaneHeight;
    Widget hScrollBar;

    /* find the minimum allowable size for a pane */
    XtVaGetValues(window->textArea, textNhScrollBar, &hScrollBar, NULL);
    XtVaGetValues(containingPane(window->textArea),
            XmNscrolledWindowMarginHeight, &swMarginHeight, NULL);
    XtVaGetValues(XtParent(window->textArea),
            XmNshadowThickness, &frameShadowHeight, NULL);
    XtVaGetValues(window->textArea, textNmarginHeight, &marginHeight, NULL);
    XtVaGetValues(hScrollBar, XmNheight, &hsbHeight, NULL);
    minPaneHeight = textD->ascent + textD->descent + marginHeight*2 +
            swMarginHeight*2 + hsbHeight + 2*frameShadowHeight;
    
    /* Set it in all of the widgets in the window */
    setPaneMinHeight(containingPane(window->textArea), minPaneHeight);
    for (i=0; i<window->nPanes; i++)
        setPaneMinHeight(containingPane(window->textPanes[i]),
                   minPaneHeight);
}

/* Add an icon to an applicaction shell widget.  addWindowIcon adds a large
** (primary window) icon, AddSmallIcon adds a small (secondary window) icon.
**
** Note: I would prefer that these were not hardwired, but yhere is something
** weird about the  XmNiconPixmap resource that prevents it from being set
** from the defaults in the application resource database.
*/
static void addWindowIcon(Widget shell)
{ 
    static Pixmap iconPixmap = 0, maskPixmap = 0;

    if (iconPixmap == 0) {
        iconPixmap = XCreateBitmapFromData(TheDisplay,
                RootWindowOfScreen(XtScreen(shell)), (char *)iconBits,
                iconBitmapWidth, iconBitmapHeight);
        maskPixmap = XCreateBitmapFromData(TheDisplay,
                RootWindowOfScreen(XtScreen(shell)), (char *)maskBits,
                iconBitmapWidth, iconBitmapHeight);
    }
    XtVaSetValues(shell, XmNiconPixmap, iconPixmap, XmNiconMask, maskPixmap,
            NULL);
}
void AddSmallIcon(Widget shell)
{ 
    static Pixmap iconPixmap = 0, maskPixmap = 0;

    if (iconPixmap == 0) {
        iconPixmap = XCreateBitmapFromData(TheDisplay,
                RootWindowOfScreen(XtScreen(shell)), (char *)n_bits,
                n_width, n_height);
        maskPixmap = XCreateBitmapFromData(TheDisplay,
                RootWindowOfScreen(XtScreen(shell)), (char *)n_mask,
                n_width, n_height);
    }
    XtVaSetValues(shell, XmNiconPixmap, iconPixmap,
            XmNiconMask, maskPixmap, NULL);
}

/* 
** Create pixmap per the widget's color depth setting. 
**
** This fixes a BadMatch (X_CopyArea) error due to mismatching of 
** color depth between the bitmap (depth of 1) and the screen, 
** specifically on when linked to LessTif v1.2 (release 0.93.18 
** & 0.93.94 tested).  LessTif v2.x showed no such problem. 
*/
static Pixmap createBitmapWithDepth(Widget w, char *data, unsigned int width,
	unsigned int height)
{
    Pixmap pixmap;
    Pixel fg, bg;
    int depth;

    XtVaGetValues (w, XmNforeground, &fg, XmNbackground, &bg,
	    XmNdepth, &depth, NULL);
    pixmap = XCreatePixmapFromBitmapData(XtDisplay(w),
            RootWindowOfScreen(XtScreen(w)), (char *)data,
            width, height, fg, bg, depth);

    return pixmap;
}

/*
** Save the position and size of a window as an X standard geometry string.
** A string of at least MAX_GEOMETRY_STRING_LEN characters should be
** provided in the argument "geomString" to receive the result.
*/
static void getGeometryString(WindowInfo *window, char *geomString)
{
    int x, y, fontWidth, fontHeight, baseWidth, baseHeight;
    unsigned int width, height, dummyW, dummyH, bw, depth, nChild;
    Window parent, root, *child, w = XtWindow(window->shell);
    Display *dpy = XtDisplay(window->shell);
    
    /* Find the width and height from the window of the shell */
    XGetGeometry(dpy, w, &root, &x, &y, &width, &height, &bw, &depth);
    
    /* Find the top left corner (x and y) of the window decorations.  (This
       is what's required in the geometry string to restore the window to it's
       original position, since the window manager re-parents the window to
       add it's title bar and menus, and moves the requested window down and
       to the left.)  The position is found by traversing the window hier-
       archy back to the window to the last parent before the root window */
    for(;;) {
        XQueryTree(dpy, w, &root, &parent,  &child, &nChild);
        XFree((char*)child);
        if (parent == root)
            break;
        w = parent;
    }
    XGetGeometry(dpy, w, &root, &x, &y, &dummyW, &dummyH, &bw, &depth);
    
    /* Use window manager size hints (set by UpdateWMSizeHints) to
       translate the width and height into characters, as opposed to pixels */
    XtVaGetValues(window->shell, XmNwidthInc, &fontWidth,
            XmNheightInc, &fontHeight, XmNbaseWidth, &baseWidth,
            XmNbaseHeight, &baseHeight, NULL);
    width = (width-baseWidth) / fontWidth;
    height = (height-baseHeight) / fontHeight;

    /* Write the string */
    CreateGeometryString(geomString, x, y, width, height,
                XValue | YValue | WidthValue | HeightValue);
}

/*
** Xt timer procedure for updating size hints.  The new sizes of objects in
** the window are not ready immediately after adding or removing panes.  This
** is a timer routine to be invoked with a timeout of 0 to give the event
** loop a chance to finish processing the size changes before reading them
** out for setting the window manager size hints.
*/
static void wmSizeUpdateProc(XtPointer clientData, XtIntervalId *id)
{
    UpdateWMSizeHints((WindowInfo *)clientData);
}

#ifdef ROWCOLPATCH
/*
** There is a bad memory reference in the delete_child method of the
** RowColumn widget in some Motif versions (so far, just Solaris with Motif
** 1.2.3) which appears durring the phase 2 destroy of the widget. This
** patch replaces the method with a call to the Composite widget's
** delete_child method.  The composite delete_child method handles part,
** but not all of what would have been done by the original method, meaning
** that this is dangerous and should be used sparingly.  Note that
** patchRowCol is called only in CloseWindow, before the widget is about to
** be destroyed, and only on systems where the bug has been observed
*/
static void patchRowCol(Widget w)
{
    ((XmRowColumnClassRec *)XtClass(w))->composite_class.delete_child =
            patchedRemoveChild;
}
static void patchedRemoveChild(Widget child)
{
    /* Call composite class method instead of broken row col delete_child
       method */
    (*((CompositeWidgetClass)compositeWidgetClass)->composite_class.
                delete_child) (child);
}
#endif /* ROWCOLPATCH */

/*
** Set the backlight character class string
*/
void SetBacklightChars(WindowInfo *window, char *applyBacklightTypes)
{
    int i;
    int is_applied = XmToggleButtonGetState(window->backlightCharsItem) ? 1 : 0;
    int do_apply = applyBacklightTypes ? 1 : 0;

    window->backlightChars = do_apply;

    NEditFree(window->backlightCharTypes);
    if (window->backlightChars &&
      (window->backlightCharTypes = (char*)NEditMalloc(strlen(applyBacklightTypes)+1)))
      strcpy(window->backlightCharTypes, applyBacklightTypes);
    else
      window->backlightCharTypes = NULL;

    XtVaSetValues(window->textArea,
          textNbacklightCharTypes, window->backlightCharTypes, NULL);
    for (i=0; i<window->nPanes; i++)
      XtVaSetValues(window->textPanes[i],
              textNbacklightCharTypes, window->backlightCharTypes, NULL);
    if (is_applied != do_apply)
      SetToggleButtonState(window, window->backlightCharsItem, do_apply, False);
}

/*
** perform generic management on the children (toolbars) of toolBarsForm,
** a.k.a. statsForm, by setting the form attachment of the managed child 
** widgets per their position/order.
**
** You can optionally create separator after a toolbar widget with it's 
** widget name set to "TOOLBAR_SEP", which will appear below the toolbar
** widget. These seperators will then be managed automatically by this  
** routine along with the toolbars they 'attached' to.
**
** It also takes care of the attachment offset settings of the child
** widgets to keep the border lines of the parent form displayed, so
** you don't have set them before hand.
**
** Note: XtManage/XtUnmange the target child (toolbar) before calling this
**       function.
**
** Returns the last toolbar widget managed.
**
*/
static Widget manageToolBars(Widget toolBarsForm)
{
    Widget topWidget = NULL;
    WidgetList children;
    int n, nItems=0;

    XtVaGetValues(toolBarsForm, XmNchildren, &children, 
    	    XmNnumChildren, &nItems, NULL);

    for (n=0; n<nItems; n++) {
    	Widget tbar = children[n];
	
    	if (XtIsManaged(tbar)) {	    
	    if (topWidget) {
		XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_WIDGET,
			XmNtopWidget, topWidget,
		    	XmNbottomAttachment, XmATTACH_NONE,
	    	    	XmNleftOffset, STAT_SHADOW_THICKNESS,
	    	    	XmNrightOffset, STAT_SHADOW_THICKNESS,			
			NULL);
	    }
	    else {
	    	/* the very first toolbar on top */
	    	XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_FORM,
		    	XmNbottomAttachment, XmATTACH_NONE,
	    	    	XmNleftOffset, STAT_SHADOW_THICKNESS,
	    	    	XmNtopOffset, STAT_SHADOW_THICKNESS,			
	    	    	XmNrightOffset, STAT_SHADOW_THICKNESS,			
			NULL);
	    }

	    topWidget = tbar;	    

	    /* if the next widget is a separator, turn it on */
	    if (n+1<nItems && !strcmp(XtName(children[n+1]), "TOOLBAR_SEP")) {
    	    	XtManageChild(children[n+1]);
	    }	    
	}
	else {
	    /* Remove top attachment to widget to avoid circular dependency.
	       Attach bottom to form so that when the widget is redisplayed
	       later, it will trigger the parent form to resize properly as
	       if the widget is being inserted */
	    XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_NONE,
		    XmNbottomAttachment, XmATTACH_FORM, NULL);
	    
	    /* if the next widget is a separator, turn it off */
	    if (n+1<nItems && !strcmp(XtName(children[n+1]), "TOOLBAR_SEP")) {
    	    	XtUnmanageChild(children[n+1]);
	    }
	}
    }
    
    if (topWidget) {
    	if (strcmp(XtName(topWidget), "TOOLBAR_SEP")) {
	    XtVaSetValues(topWidget, 
		    XmNbottomAttachment, XmATTACH_FORM,
		    XmNbottomOffset, STAT_SHADOW_THICKNESS,
		    NULL);
	}
	else {
	    /* is a separator */
	    Widget wgt;
	    XtVaGetValues(topWidget, XmNtopWidget, &wgt, NULL);
	    
	    /* don't need sep below bottom-most toolbar */
	    XtUnmanageChild(topWidget);
	    XtVaSetValues(wgt, 
		    XmNbottomAttachment, XmATTACH_FORM,
		    XmNbottomOffset, STAT_SHADOW_THICKNESS,
		    NULL);
	}
    }
    
    return topWidget;
}

/*
** Calculate the dimension of the text area, in terms of rows & cols, 
** as if there's only one single text pane in the window.
*/
static void getTextPaneDimension(WindowInfo *window, int *nRows, int *nCols)
{
    Widget hScrollBar;
    Dimension hScrollBarHeight, paneHeight;
    int marginHeight, marginWidth, totalHeight, fontHeight;
    textDisp *textD = ((TextWidget)window->textArea)->text.textD;

    /* width is the same for panes */
    XtVaGetValues(window->textArea, textNcolumns, nCols, NULL);
    
    /* we have to work out the height, as the text area may have been split */
    XtVaGetValues(window->textArea, textNhScrollBar, &hScrollBar,
            textNmarginHeight, &marginHeight, textNmarginWidth, &marginWidth,
	    NULL);
    XtVaGetValues(hScrollBar, XmNheight, &hScrollBarHeight, NULL);
    XtVaGetValues(window->splitPane, XmNheight, &paneHeight, NULL);
    totalHeight = paneHeight - 2*marginHeight -hScrollBarHeight;
    fontHeight = textD->ascent + textD->descent;
    *nRows = totalHeight/fontHeight;
}

/*
** Create a new document in the shell window.
** Document are created in 'background' so that the user 
** menus, ie. the Macro/Shell/BG menus, will not be updated 
** unnecessarily; hence speeding up the process of opening 
** multiple files.
*/
WindowInfo* CreateDocument(WindowInfo* shellWindow, const char* name)
{
    Widget pane, text;
    WindowInfo *window;
    int nCols, nRows;
    
    /* Allocate some memory for the new window data structure */
    window = (WindowInfo *)NEditMalloc(sizeof(WindowInfo));
    
    /* inherit settings and later reset those required */
    memcpy(window, shellWindow, sizeof(WindowInfo));
    
#if 0
    /* share these dialog items with parent shell */
    window->replaceDlog = NULL;
    window->replaceText = NULL;
    window->replaceWithText = NULL;
    window->replaceWordToggle = NULL;
    window->replaceCaseToggle = NULL;
    window->replaceRegexToggle = NULL;
    window->findDlog = NULL;
    window->findText = NULL;
    window->findWordToggle = NULL;
    window->findCaseToggle = NULL;
    window->findRegexToggle = NULL;
    window->replaceMultiFileDlog = NULL;
    window->replaceMultiFilePathBtn = NULL;
    window->replaceMultiFileList = NULL;
    window->showLineNumbers = GetPrefLineNums();
    window->showStats = GetPrefStatsLine();
    window->showISearchLine = GetPrefISearchLine();
#endif

    window->multiFileReplSelected = FALSE;
    window->multiFileBusy = FALSE;
    window->writableWindows = NULL;
    window->nWritableWindows = 0;
    window->fileChanged = FALSE;
    window->fileMissing = True;
    window->fileMode = 0;
    window->fileUid = 0;
    window->fileGid = 0;
    window->filenameSet = FALSE;
    window->fileFormat = UNIX_FILE_FORMAT;
    window->lastModTime = 0;
    strcpy(window->filename, name);
    window->undo = NULL;
    window->redo = NULL;
    window->nPanes = 0;
    window->autoSaveCharCount = 0;
    window->autoSaveOpCount = 0;
    window->undoOpCount = 0;
    window->undoMemUsed = 0;
    CLEAR_ALL_LOCKS(window->lockReasons);
    window->indentStyle = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
    window->autoSave = GetPrefAutoSave();
    window->saveOldVersion = GetPrefSaveOldVersion();
    window->wrapMode = GetPrefWrap(PLAIN_LANGUAGE_MODE);
    window->overstrike = False;
    window->showMatchingStyle = GetPrefShowMatching();
    window->matchSyntaxBased = GetPrefMatchSyntaxBased();
    window->highlightSyntax = GetPrefHighlightSyntax();
    window->backlightCharTypes = NULL;
    window->backlightChars = GetPrefBacklightChars();
    if (window->backlightChars) {
      char *cTypes = GetPrefBacklightCharTypes();
      if (cTypes && window->backlightChars) {
          if ((window->backlightCharTypes = (char*)NEditMalloc(strlen(cTypes) + 1)))
              strcpy(window->backlightCharTypes, cTypes);
      }
    }
    window->modeMessageDisplayed = FALSE;
    window->modeMessage = NULL;
    window->ignoreModify = FALSE;
    window->windowMenuValid = FALSE;
    window->flashTimeoutID = 0;
    window->fileClosedAtom = None;
    window->wasSelected = FALSE;
    strcpy(window->fontName, GetPrefFontName());
    strcpy(window->italicFontName, GetPrefItalicFontName());
    strcpy(window->boldFontName, GetPrefBoldFontName());
    strcpy(window->boldItalicFontName, GetPrefBoldItalicFontName());
    window->colorDialog = NULL;
    window->fontList = GetPrefFontList();
    window->italicFontStruct = GetPrefItalicFont();
    window->boldFontStruct = GetPrefBoldFont();
    window->boldItalicFontStruct = GetPrefBoldItalicFont();
    window->fontDialog = NULL;
    window->nMarks = 0;
    window->markTimeoutID = 0;
    window->highlightData = NULL;
    window->shellCmdData = NULL;
    window->macroCmdData = NULL;
    window->smartIndentData = NULL;
    window->languageMode = PLAIN_LANGUAGE_MODE;
    window->iSearchHistIndex = 0;
    window->iSearchStartPos = -1;
    window->replaceLastRegexCase   = TRUE;
    window->replaceLastLiteralCase = FALSE;
    window->iSearchLastRegexCase   = TRUE;
    window->iSearchLastLiteralCase = FALSE;
    window->findLastRegexCase      = TRUE;
    window->findLastLiteralCase    = FALSE;
    window->tab = NULL;
    window->bgMenuUndoItem = NULL;
    window->bgMenuRedoItem = NULL;
    window->device = 0;
    window->inode = 0;

    if (window->fontList == NULL)
        XtVaGetValues(shellWindow->statsLine, XmNfontList, 
    	    	&window->fontList,NULL);

    getTextPaneDimension(shellWindow, &nRows, &nCols);
    
    /* Create pane that actaully holds the new document. As 
       document is created in 'background', we need to hide
       it. If we leave it unmanaged without setting it to
       the XmNworkWindow of the mainWin, due to a unknown
       bug in Motif where splitpane's scrollWindow child 
       somehow came up with a height taller than the splitpane,
       the bottom part of the text editing widget is obstructed
       when later brought up by  RaiseDocument(). So we first 
       manage it hidden, then unmanage it and reset XmNworkWindow, 
       then let RaiseDocument() show it later. */
    pane = XtVaCreateWidget("pane",
    	    xmPanedWindowWidgetClass, window->mainWin,
    	    XmNmarginWidth, 0, XmNmarginHeight, 0, XmNseparatorOn, False,
    	    XmNspacing, 3, XmNsashIndent, -2,
	    XmNmappedWhenManaged, False,
	    NULL);
    XtVaSetValues(window->mainWin, XmNworkWindow, pane, NULL);
    XtManageChild(pane);
    window->splitPane = pane;
    
    /* Store a copy of document/window pointer in text pane to support
       action procedures. See also WidgetToWindow() for info. */
    XtVaSetValues(pane, XmNuserData, window, NULL);

    /* Patch around Motif's most idiotic "feature", that its menu accelerators
       recognize Caps Lock and Num Lock as modifiers, and don't trigger if
       they are engaged */ 
    AccelLockBugPatch(pane, window->menuBar);

    /* Create the first, and most permanent text area (other panes may
       be added & removed, but this one will never be removed */
    text = createTextArea(pane, window, nRows, nCols,
    	    GetPrefEmTabDist(PLAIN_LANGUAGE_MODE), GetPrefDelimiters(),
	    GetPrefWrapMargin(), window->showLineNumbers?MIN_LINE_NUM_COLS:0);
    XtManageChild(text);
    window->textArea = text;
    window->lastFocus = text;
    
    /* Set the initial colors from the globals. */
    SetColors(window,
              GetPrefColorName(TEXT_FG_COLOR  ),
              GetPrefColorName(TEXT_BG_COLOR  ),
              GetPrefColorName(SELECT_FG_COLOR),
              GetPrefColorName(SELECT_BG_COLOR),
              GetPrefColorName(HILITE_FG_COLOR),
              GetPrefColorName(HILITE_BG_COLOR),
              GetPrefColorName(LINENO_FG_COLOR),
              GetPrefColorName(CURSOR_FG_COLOR));
    
    /* Create the right button popup menu (note: order is important here,
       since the translation for popping up this menu was probably already
       added in createTextArea, but CreateBGMenu requires window->textArea
       to be set so it can attach the menu to it (because menu shells are
       finicky about the kinds of widgets they are attached to)) */
    window->bgMenuPane = CreateBGMenu(window);
    
    /* cache user menus: init. user background menu cache */
    InitUserBGMenuCache(&window->userBGMenuCache);

    /* Create the text buffer rather than using the one created automatically
       with the text area widget.  This is done so the syntax highlighting
       modify callback can be called to synchronize the style buffer BEFORE
       the text display's callback is called upon to display a modification */
    window->buffer = BufCreate();
    BufAddModifyCB(window->buffer, SyntaxHighlightModifyCB, window);
    
    /* Attach the buffer to the text widget, and add callbacks for modify */
    TextSetBuffer(text, window->buffer);
    BufAddModifyCB(window->buffer, modifiedCB, window);
    
    /* Designate the permanent text area as the owner for selections */
    HandleXSelections(text);
    
    /* Set the requested hardware tab distance and useTabs in the text buffer */
    BufSetTabDistance(window->buffer, GetPrefTabDist(PLAIN_LANGUAGE_MODE));
    window->buffer->useTabs = GetPrefInsertTabs();
    window->tab = addTab(window->tabBar, name);

    /* add the window to the global window list, update the Windows menus */
    InvalidateWindowMenus();
    addToWindowList(window);

#ifdef LESSTIF_VERSION
    /* FIXME: Temporary workaround for disappearing-text-window bug
              when linking to Lesstif.
       
       After changes is made to statsAreaForm (parent of statsline,
       i-search line and tab bar) widget such as enabling/disabling
       the statsline, the XmForm widget enclosing the text widget 
       somehow refused to resize to fit the text widget. Resizing
       the shell window or making changes [again] to the statsAreaForm 
       appeared to bring out the text widget, though doesn't fix it for
       the subsequently added documents. Here we try to do the latter 
       for all new documents created. */
    if (XtIsManaged(XtParent(window->statsLineForm))) {
    	XtUnmanageChild(XtParent(window->statsLineForm));
    	XtManageChild(XtParent(window->statsLineForm));    
    }
#endif /* LESSTIF_VERSION */

    /* return the shell ownership to previous tabbed doc */
    XtVaSetValues(window->mainWin, XmNworkWindow, shellWindow->splitPane, NULL);
    XLowerWindow(TheDisplay, XtWindow(window->splitPane));
    XtUnmanageChild(window->splitPane);
    XtVaSetValues(window->splitPane, XmNmappedWhenManaged, True, NULL);
    
    return window;
}

/*
** return the next/previous docment on the tab list.
**
** If <wrap> is true then the next tab of the rightmost tab will be the
** second tab from the right, and the the previous tab of the leftmost
** tab will be the second from the left.  This is useful for getting
** the next tab after a tab detaches/closes and you don't want to wrap around.
*/
static WindowInfo *getNextTabWindow(WindowInfo *window, int direction,
        int crossWin, int wrap)
{
    WidgetList tabList, tabs;
    WindowInfo *win;
    int tabCount, tabTotalCount;
    int tabPos, nextPos;
    int i, n;
    int nBuf = crossWin? NWindows() : NDocuments(window);

    if (nBuf <= 1)
    	return NULL;

    /* get the list of tabs */
    tabs = (WidgetList)NEditMalloc(sizeof(Widget) * nBuf);
    tabTotalCount = 0;
    if (crossWin) {
	int n, nItems;
	WidgetList children;

	XtVaGetValues(TheAppShell, XmNchildren, &children, 
    		XmNnumChildren, &nItems, NULL);
	
	/* get list of tabs in all windows */
    	for (n=0; n<nItems; n++) {
	    if (strcmp(XtName(children[n]), "textShell") ||
	      ((win = WidgetToWindow(children[n])) == NULL))
	    	continue;   /* skip non-text-editor windows */ 
	    
    	    XtVaGetValues(win->tabBar, XmNtabWidgetList, &tabList,
            	    XmNtabCount, &tabCount, NULL);
	    
    	    for (i=0; i< tabCount; i++) {
	    	tabs[tabTotalCount++] = tabList[i];
	    }
	}
    }
    else {
	/* get list of tabs in this window */
    	XtVaGetValues(window->tabBar, XmNtabWidgetList, &tabList,
            	XmNtabCount, &tabCount, NULL);

    	for (i=0; i< tabCount; i++) {
	    if (TabToWindow(tabList[i]))    /* make sure tab is valid */
	    	tabs[tabTotalCount++] = tabList[i];
	}
    }
    
    /* find the position of the tab in the tablist */
    tabPos = 0;
    for (n=0; n<tabTotalCount; n++) {
    	if (tabs[n] == window->tab) {
	    tabPos = n;
	    break;
	}
    }
    
    /* calculate index position of next tab */
    nextPos = tabPos + direction;
    if (nextPos >= nBuf) {
        if (wrap)
            nextPos = 0;
        else
            nextPos = nBuf - 2; 
    } else if (nextPos < 0) {
        if (wrap)
    	    nextPos = nBuf - 1;
        else
            nextPos = 1;
    }
    
    /* return the document where the next tab belongs to */
    win = TabToWindow(tabs[nextPos]);
    NEditFree(tabs);
    return win;
}

/*
** return the integer position of a tab in the tabbar it
** belongs to, or -1 if there's an error, somehow.
*/
static int getTabPosition(Widget tab)
{
    WidgetList tabList;
    int i, tabCount;
    Widget tabBar = XtParent(tab);
    
    XtVaGetValues(tabBar, XmNtabWidgetList, &tabList,
            XmNtabCount, &tabCount, NULL);

    for (i=0; i< tabCount; i++) {
    	if (tab == tabList[i])
	    return i;
    }
    
    return -1; /* something is wrong! */
}

/*
** update the tab label, etc. of a tab, per the states of it's document.
*/
void RefreshTabState(WindowInfo *win)
{
    XmString s1, tipString;
    char labelString[MAXPATHLEN];
    char *tag = XmFONTLIST_DEFAULT_TAG;
    unsigned char alignment;

    /* Set tab label to document's filename. Position of
       "*" (modified) will change per label alignment setting */
    XtVaGetValues(win->tab, XmNalignment, &alignment, NULL);
    if (alignment != XmALIGNMENT_END) {
       sprintf(labelString, "%s%s",
               win->fileChanged? "*" : "",
               win->filename);
    } else {
       sprintf(labelString, "%s%s",
               win->filename,
               win->fileChanged? "*" : "");
    }

    /* Make the top document stand out a little more */
    if (IsTopDocument(win))
        tag = "BOLD";

    s1 = XmStringCreateLtoR(labelString, tag);

    if (GetPrefShowPathInWindowsMenu() && win->filenameSet) {
       strcat(labelString, " - ");
       strcat(labelString, win->path);
    }
    tipString=XmStringCreateSimple(labelString);
    
    XtVaSetValues(win->tab,
	    XltNbubbleString, tipString,
	    XmNlabelString, s1,
	    NULL);
    XmStringFree(s1);
    XmStringFree(tipString);
}

/*
** close all the documents in a window
*/
int CloseAllDocumentInWindow(WindowInfo *window) 
{
    WindowInfo *win;
    
    if (NDocuments(window) == 1) {
    	/* only one document in the window */
    	return CloseFileAndWindow(window, PROMPT_SBC_DIALOG_RESPONSE);
    }
    else {
	Widget winShell = window->shell;
	WindowInfo *topDocument;

    	/* close all _modified_ documents belong to this window */
	for (win = WindowList; win; ) {
    	    if (win->shell == winShell && win->fileChanged) {
	    	WindowInfo *next = win->next;
    	    	if (!CloseFileAndWindow(win, PROMPT_SBC_DIALOG_RESPONSE))
		    return False;
		win = next;
	    }
	    else
	    	win = win->next;
	}

    	/* see there's still documents left in the window */
	for (win = WindowList; win; win=win->next)
	    if (win->shell == winShell)
	    	break;
	
	if (win) {
	    topDocument = GetTopDocument(winShell);

    	    /* close all non-top documents belong to this window */
	    for (win = WindowList; win; ) {
    		if (win->shell == winShell && win != topDocument) {
	    	    WindowInfo *next = win->next;
    	    	    if (!CloseFileAndWindow(win, PROMPT_SBC_DIALOG_RESPONSE))
			return False;
		    win = next;
		}
		else
	    	    win = win->next;
	    }

	    /* close the last document and its window */
    	    if (!CloseFileAndWindow(topDocument, PROMPT_SBC_DIALOG_RESPONSE))
		return False;
	}
    }
    
    return True;
}

static void CloseDocumentWindow(Widget w, WindowInfo *window, XtPointer callData) 
{
    int nDocuments = NDocuments(window);
    
    if (nDocuments == NWindows()) {
    	/* this is only window, then exit */
	XtCallActionProc(WindowList->lastFocus, "exit",
    		((XmAnyCallbackStruct *)callData)->event, NULL, 0);
    }
    else {
        if (nDocuments == 1) {
	    CloseFileAndWindow(window, PROMPT_SBC_DIALOG_RESPONSE);
	}
    	else {
            int resp = 1;
            if (GetPrefWarnExit())
                resp = DialogF(DF_QUES, window->shell, 2, "Close Window",
	    	    "Close ALL documents in this window?", "Close", "Cancel");

            if (resp == 1)
    		CloseAllDocumentInWindow(window);
	}
    }
}

/*
** Refresh the menu entries per the settings of the
** top document.
*/
void RefreshMenuToggleStates(WindowInfo *window)
{
    WindowInfo *win;
    
    if (!IsTopDocument(window))
	return;
	
    /* File menu */
    XtSetSensitive(window->printSelItem, window->wasSelected);

    /* Edit menu */
    XtSetSensitive(window->undoItem, window->undo != NULL);
    XtSetSensitive(window->redoItem, window->redo != NULL);
    XtSetSensitive(window->printSelItem, window->wasSelected);
    XtSetSensitive(window->cutItem, window->wasSelected);
    XtSetSensitive(window->copyItem, window->wasSelected);
    XtSetSensitive(window->delItem, window->wasSelected);
    
    /* Preferences menu */
    XmToggleButtonSetState(window->statsLineItem, window->showStats, False);
    XmToggleButtonSetState(window->iSearchLineItem, window->showISearchLine, False);
    XmToggleButtonSetState(window->lineNumsItem, window->showLineNumbers, False);
    XmToggleButtonSetState(window->highlightItem, window->highlightSyntax, False);
    XtSetSensitive(window->highlightItem, window->languageMode != PLAIN_LANGUAGE_MODE);
    XmToggleButtonSetState(window->backlightCharsItem, window->backlightChars, False);
#ifndef VMS
    XmToggleButtonSetState(window->saveLastItem, window->saveOldVersion, False);
#endif
    XmToggleButtonSetState(window->autoSaveItem, window->autoSave, False);
    XmToggleButtonSetState(window->overtypeModeItem, window->overstrike, False);
    XmToggleButtonSetState(window->matchSyntaxBasedItem, window->matchSyntaxBased, False);
    XmToggleButtonSetState(window->readOnlyItem, IS_USER_LOCKED(window->lockReasons), False);

    XtSetSensitive(window->smartIndentItem, 
            SmartIndentMacrosAvailable(LanguageModeName(window->languageMode)));

    SetAutoIndent(window, window->indentStyle);
    SetAutoWrap(window, window->wrapMode);
    SetShowMatching(window, window->showMatchingStyle);
    SetLanguageMode(window, window->languageMode, FALSE);
    
    /* Windows Menu */
    XtSetSensitive(window->splitPaneItem, window->nPanes < MAX_PANES);
    XtSetSensitive(window->closePaneItem, window->nPanes > 0);
    XtSetSensitive(window->detachDocumentItem, NDocuments(window)>1);
    XtSetSensitive(window->contextDetachDocumentItem, NDocuments(window)>1);

    for (win=WindowList; win; win=win->next)
    	if (win->shell != window->shell)  
	    break;
    XtSetSensitive(window->moveDocumentItem, win != NULL);
}

/*
** Refresh the various settings/state of the shell window per the
** settings of the top document.
*/
static void refreshMenuBar(WindowInfo *window)
{
    RefreshMenuToggleStates(window);
    
    /* Add/remove language specific menu items */
    UpdateUserMenus(window);
    
    /* refresh selection-sensitive menus */
    DimSelectionDepUserMenuItems(window, window->wasSelected);
}

/*
** remember the last document.
*/
WindowInfo *MarkLastDocument(WindowInfo *window)
{
    WindowInfo *prev = lastFocusDocument;
    
    if (window)
    	lastFocusDocument = window;
	
    return prev;
}

/*
** remember the active (top) document.
*/
WindowInfo *MarkActiveDocument(WindowInfo *window)
{
    WindowInfo *prev = inFocusDocument;

    if (window)
    	inFocusDocument = window;

    return prev;
}

/*
** Bring up the next window by tab order
*/
void NextDocument(WindowInfo *window)
{
    WindowInfo *win;
    
    if (WindowList->next == NULL)
    	return;

    win = getNextTabWindow(window, 1, GetPrefGlobalTabNavigate(), 1);
    if (win == NULL)
    	return;
	
    if (window->shell == win->shell)
	RaiseDocument(win);
    else
    	RaiseFocusDocumentWindow(win, True);
}

/*
** Bring up the previous window by tab order
*/
void PreviousDocument(WindowInfo *window)
{
    WindowInfo *win;
    
    if (WindowList->next == NULL)
    	return;

    win = getNextTabWindow(window, -1, GetPrefGlobalTabNavigate(), 1);
    if (win == NULL)
    	return;
	
    if (window->shell == win->shell)
	RaiseDocument(win);
    else
    	RaiseFocusDocumentWindow(win, True);
}

/*
** Bring up the last active window
*/
void LastDocument(WindowInfo *window)
{
    WindowInfo *win;
    
    for(win = WindowList; win; win=win->next)
    	if (lastFocusDocument == win)
	    break;
    
    if (!win)
    	return;

    if (window->shell == win->shell)
	RaiseDocument(win);
    else
    	RaiseFocusDocumentWindow(win, True);
	
}

/*
** make sure window is alive is kicking
*/
int IsValidWindow(WindowInfo *window)
{
    WindowInfo *win;
    
    for(win = WindowList; win; win=win->next)
    	if (window == win)
	    return True;
    
    
    return False;
}

/*
** raise the document and its shell window and focus depending on pref.
*/
void RaiseDocumentWindow(WindowInfo *window)
{
    if (!window)
    	return;
	
    RaiseDocument(window);
    RaiseShellWindow(window->shell, GetPrefFocusOnRaise());
}

/*
** raise the document and its shell window and optionally focus.
*/
void RaiseFocusDocumentWindow(WindowInfo *window, Boolean focus)
{
    if (!window)
    	return;
	
    RaiseDocument(window);
    RaiseShellWindow(window->shell, focus);
}

/*
** Redisplay menu tearoffs previously hid by hideTearOffs()
*/
static void redisplayTearOffs(Widget menuPane)
{
    WidgetList itemList;
    Widget subMenuID;
    Cardinal nItems;
    int n;

    /* redisplay all submenu tearoffs */
    XtVaGetValues(menuPane, XmNchildren, &itemList, 
            XmNnumChildren, &nItems, NULL);
    for (n=0; n<(int)nItems; n++) {
    	if (XtClass(itemList[n]) == xmCascadeButtonWidgetClass) {
	    XtVaGetValues(itemList[n], XmNsubMenuId, &subMenuID, NULL);
	    redisplayTearOffs(subMenuID);
	}
    }

    /* redisplay tearoff for this menu */
    if (!XmIsMenuShell(XtParent(menuPane)))
    	ShowHiddenTearOff(menuPane);    
}

/*
** hide all the tearoffs spawned from this menu.
** It works recursively to close the tearoffs of the submenus
*/
static void hideTearOffs(Widget menuPane)
{
    WidgetList itemList;
    Widget subMenuID;
    Cardinal nItems;
    int n;

    /* hide all submenu tearoffs */
    XtVaGetValues(menuPane, XmNchildren, &itemList, 
            XmNnumChildren, &nItems, NULL);
    for (n=0; n<(int)nItems; n++) {
    	if (XtClass(itemList[n]) == xmCascadeButtonWidgetClass) {
	    XtVaGetValues(itemList[n], XmNsubMenuId, &subMenuID, NULL);
	    hideTearOffs(subMenuID);
	}
    }

    /* hide tearoff for this menu */
    if (!XmIsMenuShell(XtParent(menuPane)))
    	XtUnmapWidget(XtParent(menuPane));    
}

/*
** Raise a tabbed document within its shell window.
**
** NB: use RaiseDocumentWindow() to raise the doc and 
**     its shell window.
*/
void RaiseDocument(WindowInfo *window)
{
    WindowInfo *win, *lastwin;        
	
    if (!window || !WindowList)
    	return;

    lastwin = MarkActiveDocument(window);
    if (lastwin != window && IsValidWindow(lastwin))
    	MarkLastDocument(lastwin);

    /* document already on top? */
    XtVaGetValues(window->mainWin, XmNuserData, &win, NULL);
    if (win == window)
    	return;    

    /* set the document as top document */
    XtVaSetValues(window->mainWin, XmNuserData, window, NULL);
    
    /* show the new top document */ 
    XtVaSetValues(window->mainWin, XmNworkWindow, window->splitPane, NULL);
    XtManageChild(window->splitPane);
    XRaiseWindow(TheDisplay, XtWindow(window->splitPane));

    /* Turn on syntax highlight that might have been deferred.
       NB: this must be done after setting the document as
           XmNworkWindow and managed, else the parent shell 
	   window may shrink on some window-managers such as 
	   metacity, due to changes made in UpdateWMSizeHints().*/
    if (window->highlightSyntax && window->highlightData==NULL)
    	StartHighlighting(window, False);

    /* put away the bg menu tearoffs of last active document */
    hideTearOffs(win->bgMenuPane);

    /* restore the bg menu tearoffs of active document */
    redisplayTearOffs(window->bgMenuPane);
    
    /* set tab as active */
    XmLFolderSetActiveTab(window->tabBar,
    	    getTabPosition(window->tab), False);

    /* set keyboard focus. Must be done before unmanaging previous
       top document, else lastFocus will be reset to textArea */
    XmProcessTraversal(window->lastFocus, XmTRAVERSE_CURRENT);
    
    /* we only manage the top document, else the next time a document
       is raised again, it's textpane might not resize properly.
       Also, somehow (bug?) XtUnmanageChild() doesn't hide the 
       splitPane, which obscure lower part of the statsform when
       we toggle its components, so we need to put the document at 
       the back */
    XLowerWindow(TheDisplay, XtWindow(win->splitPane));
    XtUnmanageChild(win->splitPane);
    RefreshTabState(win);

    /* now refresh window state/info. RefreshWindowStates() 
       has a lot of work to do, so we update the screen first so
       the document appears to switch swiftly. */
    XmUpdateDisplay(window->splitPane);
    RefreshWindowStates(window);
    RefreshTabState(window);
    
    /* put away the bg menu tearoffs of last active document */
    hideTearOffs(win->bgMenuPane);

    /* restore the bg menu tearoffs of active document */
    redisplayTearOffs(window->bgMenuPane);

    /* Make sure that the "In Selection" button tracks the presence of a
       selection and that the window inherits the proper search scope. */
    if (window->replaceDlog != NULL && XtIsManaged(window->replaceDlog))
    {
#ifdef REPLACE_SCOPE
        window->replaceScope = win->replaceScope;
#endif
        UpdateReplaceActionButtons(window);
    }

    UpdateWMSizeHints(window);
}

WindowInfo* GetTopDocument(Widget w)
{
    WindowInfo *window = WidgetToWindow(w);
    
    return WidgetToWindow(window->shell);
}

Boolean IsTopDocument(const WindowInfo *window)
{
    return window == GetTopDocument(window->shell)? True : False;
}

static void deleteDocument(WindowInfo *window)
{
    if (NULL == window) {
        return;
    }

    XtDestroyWidget(window->splitPane);
}

/*
** return the number of documents owned by this shell window
*/
int NDocuments(WindowInfo *window)
{
    WindowInfo *win;
    int nDocument = 0;
    
    for (win = WindowList; win; win = win->next) {
    	if (win->shell == window->shell)
	    nDocument++;
    }
    
    return nDocument;
}

/* 
** refresh window state for this document
*/
void RefreshWindowStates(WindowInfo *window)
{
    if (!IsTopDocument(window))
    	return;
	
    if (window->modeMessageDisplayed)
    	XmTextSetString(window->statsLine, window->modeMessage);
    else
    	UpdateStatsLine(window);
    UpdateWindowReadOnly(window);
    UpdateWindowTitle(window);

    /* show/hide statsline as needed */
    if (window->modeMessageDisplayed && !XtIsManaged(window->statsLineForm)) {
    	/* turn on statline to display mode message */
    	showStats(window, True);
    }
    else if (window->showStats && !XtIsManaged(window->statsLineForm)) {
    	/* turn on statsline since it is enabled */
    	showStats(window, True);
    }
    else if (!window->showStats && !window->modeMessageDisplayed &&
             XtIsManaged(window->statsLineForm)) {
    	/* turn off statsline since there's nothing to show */
    	showStats(window, False);
    }
    
    /* signal if macro/shell is running */
    if (window->shellCmdData || window->macroCmdData)
    	BeginWait(window->shell);
    else
    	EndWait(window->shell);

    /* we need to force the statsline to reveal itself */
    if (XtIsManaged(window->statsLineForm)) {
	XmTextSetCursorPosition(window->statsLine, 0);     /* start of line */
	XmTextSetCursorPosition(window->statsLine, 9000);  /* end of line */
    }
    
    XmUpdateDisplay(window->statsLine);    
    refreshMenuBar(window);

    updateLineNumDisp(window);
}

static void cloneTextPanes(WindowInfo *window, WindowInfo *orgWin)
{
    short paneHeights[MAX_PANES+1];
    int insertPositions[MAX_PANES+1], topLines[MAX_PANES+1];
    int horizOffsets[MAX_PANES+1];
    int i, focusPane, emTabDist, wrapMargin, lineNumCols, totalHeight=0;
    char *delimiters;
    Widget text;
    selection sel;
    textDisp *textD, *newTextD;
    
    /* transfer the primary selection */
    memcpy(&sel, &orgWin->buffer->primary, sizeof(selection));
	    
    if (sel.selected) {
    	if (sel.rectangular)
    	    BufRectSelect(window->buffer, sel.start, sel.end,
		    sel.rectStart, sel.rectEnd);
    	else
    	    BufSelect(window->buffer, sel.start, sel.end);
    } else
    	BufUnselect(window->buffer);

    /* Record the current heights, scroll positions, and insert positions
       of the existing panes, keyboard focus */
    focusPane = 0;
    for (i=0; i<=orgWin->nPanes; i++) {
    	text = i==0 ? orgWin->textArea : orgWin->textPanes[i-1];
    	insertPositions[i] = TextGetCursorPos(text);
    	XtVaGetValues(containingPane(text), XmNheight, &paneHeights[i], NULL);
    	totalHeight += paneHeights[i];
    	TextGetScroll(text, &topLines[i], &horizOffsets[i]);
    	if (text == orgWin->lastFocus)
    	    focusPane = i;
    }
    
    window->nPanes = orgWin->nPanes;
    
    /* Copy some parameters */
    XtVaGetValues(orgWin->textArea, textNemulateTabs, &emTabDist,
    	    textNwordDelimiters, &delimiters, textNwrapMargin, &wrapMargin,
            NULL);
    lineNumCols = orgWin->showLineNumbers ? MIN_LINE_NUM_COLS : 0;
    XtVaSetValues(window->textArea, textNemulateTabs, emTabDist,
    	    textNwordDelimiters, delimiters, textNwrapMargin, wrapMargin,
	    textNlineNumCols, lineNumCols, NULL);
    
    
    /* clone split panes, if any */
    textD = ((TextWidget)window->textArea)->text.textD;
    if (window->nPanes) {
	/* Unmanage & remanage the panedWindow so it recalculates pane 
           heights */
    	XtUnmanageChild(window->splitPane);

	/* Create a text widget to add to the pane and set its buffer and
	   highlight data to be the same as the other panes in the orgWin */

	for(i=0; i<orgWin->nPanes; i++) {
	    text = createTextArea(window->splitPane, window, 1, 1, emTabDist,
    		    delimiters, wrapMargin, lineNumCols);
	    TextSetBuffer(text, window->buffer);

	    if (window->highlightData != NULL)
    		AttachHighlightToWidget(text, window);
	    XtManageChild(text);
	    window->textPanes[i] = text;

            /* Fix up the colors */
            newTextD = ((TextWidget)text)->text.textD;
            XtVaSetValues(text, XmNforeground, textD->fgPixel,
                    XmNbackground, textD->bgPixel, NULL);
            TextDSetColors(newTextD, textD->fgPixel, textD->bgPixel, 
                    textD->selectFGPixel, textD->selectBGPixel,
                    textD->highlightFGPixel,textD->highlightBGPixel,
                    textD->lineNumFGPixel, textD->cursorFGPixel);
	}
        
	/* Set the minimum pane height in the new pane */
	UpdateMinPaneHeights(window);

	for (i=0; i<=window->nPanes; i++) {
    	    text = i==0 ? window->textArea : window->textPanes[i-1];
    	    setPaneDesiredHeight(containingPane(text), paneHeights[i]);
	}

	/* Re-manage panedWindow to recalculate pane heights & reset selection */
    	XtManageChild(window->splitPane);
    }

    /* Reset all of the heights, scroll positions, etc. */
    for (i=0; i<=window->nPanes; i++) {
    	textDisp *textD;
	
    	text = i==0 ? window->textArea : window->textPanes[i-1];
	TextSetCursorPos(text, insertPositions[i]);
	TextSetScroll(text, topLines[i], horizOffsets[i]);

	/* dim the cursor */
    	textD = ((TextWidget)text)->text.textD;
	TextDSetCursorStyle(textD, DIM_CURSOR);
	TextDUnblankCursor(textD);
    }
        
    /* set the focus pane */
    for (i=0; i<=window->nPanes; i++) {
    	text = i==0 ? window->textArea : window->textPanes[i-1];
	if(i == focusPane) {
	    window->lastFocus = text;
    	    XmProcessTraversal(text, XmTRAVERSE_CURRENT);
	    break;
	}
    }
    
    /* Update the window manager size hints after the sizes of the panes have
       been set (the widget heights are not yet readable here, but they will
       be by the time the event loop gets around to running this timer proc) */
    XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell), 0,
    	    wmSizeUpdateProc, window);
}

/*
** clone a document's states and settings into the other.
*/
static void cloneDocument(WindowInfo *window, WindowInfo *orgWin)
{
    const char *orgDocument;
    char *params[4];
    int emTabDist;
    
    strcpy(window->path, orgWin->path);
    strcpy(window->filename, orgWin->filename);

    ShowLineNumbers(window, orgWin->showLineNumbers);

    window->ignoreModify = True;

    /* copy the text buffer */
    orgDocument = BufAsString(orgWin->buffer);
    BufSetAll(window->buffer, orgDocument);

    /* copy the tab preferences (here!) */
    BufSetTabDistance(window->buffer, orgWin->buffer->tabDist);
    window->buffer->useTabs = orgWin->buffer->useTabs;
    XtVaGetValues(orgWin->textArea, textNemulateTabs, &emTabDist, NULL);
    SetEmTabDist(window, emTabDist);
    
    window->ignoreModify = False;

    /* transfer text fonts */
    params[0] = orgWin->fontName;
    params[1] = orgWin->italicFontName;
    params[2] = orgWin->boldFontName;
    params[3] = orgWin->boldItalicFontName;
    XtCallActionProc(window->textArea, "set_fonts", NULL, params, 4);

    SetBacklightChars(window, orgWin->backlightCharTypes);
    
    /* Clone rangeset info.
       
       FIXME: 
       Cloning of rangesets must be done before syntax highlighting, 
       else the rangesets do not be highlighted (colored) properly
       if syntax highlighting is on.
    */
    window->buffer->rangesetTable =
	    RangesetTableClone(orgWin->buffer->rangesetTable, window->buffer);

    /* Syntax highlighting */    
    window->languageMode = orgWin->languageMode;
    window->highlightSyntax = orgWin->highlightSyntax;
    if (window->highlightSyntax)
    	StartHighlighting(window, False);

    /* copy states of original document */
    window->filenameSet = orgWin->filenameSet;
    window->fileFormat = orgWin->fileFormat;
    window->lastModTime = orgWin->lastModTime;
    window->fileChanged = orgWin->fileChanged;
    window->fileMissing = orgWin->fileMissing;
    window->lockReasons = orgWin->lockReasons;
    window->autoSaveCharCount = orgWin->autoSaveCharCount;
    window->autoSaveOpCount = orgWin->autoSaveOpCount;
    window->undoOpCount = orgWin->undoOpCount;
    window->undoMemUsed = orgWin->undoMemUsed;
    window->lockReasons = orgWin->lockReasons;
    window->autoSave = orgWin->autoSave;
    window->saveOldVersion = orgWin->saveOldVersion;
    window->wrapMode = orgWin->wrapMode;
    SetOverstrike(window, orgWin->overstrike);
    window->showMatchingStyle = orgWin->showMatchingStyle;
    window->matchSyntaxBased = orgWin->matchSyntaxBased;
#if 0    
    window->showStats = orgWin->showStats;
    window->showISearchLine = orgWin->showISearchLine;
    window->showLineNumbers = orgWin->showLineNumbers;
    window->modeMessageDisplayed = orgWin->modeMessageDisplayed;
    window->ignoreModify = orgWin->ignoreModify;
    window->windowMenuValid = orgWin->windowMenuValid;
    window->flashTimeoutID = orgWin->flashTimeoutID;
    window->wasSelected = orgWin->wasSelected;
    strcpy(window->fontName, orgWin->fontName);
    strcpy(window->italicFontName, orgWin->italicFontName);
    strcpy(window->boldFontName, orgWin->boldFontName);
    strcpy(window->boldItalicFontName, orgWin->boldItalicFontName);
    window->fontList = orgWin->fontList;
    window->italicFontStruct = orgWin->italicFontStruct;
    window->boldFontStruct = orgWin->boldFontStruct;
    window->boldItalicFontStruct = orgWin->boldItalicFontStruct;
    window->markTimeoutID = orgWin->markTimeoutID;
    window->highlightData = orgWin->highlightData;
    window->shellCmdData = orgWin->shellCmdData;
    window->macroCmdData = orgWin->macroCmdData;
    window->smartIndentData = orgWin->smartIndentData;
#endif    
    window->iSearchHistIndex = orgWin->iSearchHistIndex;
    window->iSearchStartPos = orgWin->iSearchStartPos;
    window->replaceLastRegexCase = orgWin->replaceLastRegexCase;
    window->replaceLastLiteralCase = orgWin->replaceLastLiteralCase;
    window->iSearchLastRegexCase = orgWin->iSearchLastRegexCase;
    window->iSearchLastLiteralCase = orgWin->iSearchLastLiteralCase;
    window->findLastRegexCase = orgWin->findLastRegexCase;
    window->findLastLiteralCase = orgWin->findLastLiteralCase;
    window->device = orgWin->device;
    window->inode = orgWin->inode;
    window->fileClosedAtom = orgWin->fileClosedAtom;
    orgWin->fileClosedAtom = None;
    
    /* copy the text/split panes settings, cursor pos & selection */
    cloneTextPanes(window, orgWin);
    
    /* copy undo & redo list */
    window->undo = cloneUndoItems(orgWin->undo);
    window->redo = cloneUndoItems(orgWin->redo);

    /* copy bookmarks */
    window->nMarks = orgWin->nMarks;
    memcpy(&window->markTable, &orgWin->markTable, 
            sizeof(Bookmark)*window->nMarks);

    /* kick start the auto-indent engine */
    window->indentStyle = NO_AUTO_INDENT;
    SetAutoIndent(window, orgWin->indentStyle);

    /* synchronize window state to this document */
    RefreshWindowStates(window);
}

static UndoInfo *cloneUndoItems(UndoInfo *orgList)
{
    UndoInfo *head = NULL, *undo, *clone, *last = NULL;

    for (undo = orgList; undo; undo = undo->next) {
	clone = (UndoInfo *)NEditMalloc(sizeof(UndoInfo));
	memcpy(clone, undo, sizeof(UndoInfo));

	if (undo->oldText) {
	    clone->oldText = (char*)NEditMalloc(strlen(undo->oldText)+1);
	    strcpy(clone->oldText, undo->oldText);
	}
	clone->next = NULL;

	if (last)
	    last->next = clone;
	else
	    head = clone;

	last = clone;
    }

    return head;
}

/*
** spin off the document to a new window
*/
WindowInfo *DetachDocument(WindowInfo *window)
{
    WindowInfo *win = NULL, *cloneWin;
    
    if (NDocuments(window) < 2)
    	return NULL;

    /* raise another document in the same shell window if the window
       being detached is the top document */
    if (IsTopDocument(window)) {
    	win = getNextTabWindow(window, 1, 0, 0);
    	RaiseDocument(win);
    }
    
    /* Create a new window */
    cloneWin = CreateWindow(window->filename, NULL, False);
    
    /* CreateWindow() simply adds the new window's pointer to the
       head of WindowList. We need to adjust the detached window's 
       pointer, so that macro functions such as focus_window("last")
       will travel across the documents per the sequence they're 
       opened. The new doc will appear to replace it's former self 
       as the old doc is closed. */
    WindowList = cloneWin->next;
    cloneWin->next = window->next;
    window->next = cloneWin;
    
    /* these settings should follow the detached document.
       must be done before cloning window, else the height 
       of split panes may not come out correctly */
    ShowISearchLine(cloneWin, window->showISearchLine);
    ShowStatsLine(cloneWin, window->showStats);

    /* clone the document & its pref settings */
    cloneDocument(cloneWin, window);

    /* remove the document from the old window */
    window->fileChanged = False;
    CloseFileAndWindow(window, NO_SBC_DIALOG_RESPONSE);
    
    /* refresh former host window */
    if (win) {
        RefreshWindowStates(win);
    }
    
    /* this should keep the new document window fresh */
    RefreshWindowStates(cloneWin);
    RefreshTabState(cloneWin);
    SortTabBar(cloneWin);

    return cloneWin;
}

/*
** Move document to an other window.
**
** the moving document will receive certain window settings from
** its new host, i.e. the window size, stats and isearch lines.
*/
WindowInfo *MoveDocument(WindowInfo *toWindow, WindowInfo *window)
{
    WindowInfo *win = NULL, *cloneWin;

    /* prepare to move document */
    if (NDocuments(window) < 2) {
    	/* hide the window to make it look like we are moving */
    	XtUnmapWidget(window->shell);
    }
    else if (IsTopDocument(window)) {
    	/* raise another document to replace the document being moved */
    	win = getNextTabWindow(window, 1, 0, 0);
    	RaiseDocument(win);
    }
    
    /* relocate the document to target window */
    cloneWin = CreateDocument(toWindow, window->filename);
    ShowTabBar(cloneWin, GetShowTabBar(cloneWin));
    cloneDocument(cloneWin, window);
    
    /* CreateDocument() simply adds the new window's pointer to the
       head of WindowList. We need to adjust the detached window's 
       pointer, so that macro functions such as focus_window("last")
       will travel across the documents per the sequence they're 
       opened. The new doc will appear to replace it's former self 
       as the old doc is closed. */
    WindowList = cloneWin->next;
    cloneWin->next = window->next;
    window->next = cloneWin;

    /* remove the document from the old window */
    window->fileChanged = False;
    CloseFileAndWindow(window, NO_SBC_DIALOG_RESPONSE);
    
    /* some menu states might have changed when deleting document */
    if (win)
    	RefreshWindowStates(win);
    
    /* this should keep the new document window fresh */
    RaiseDocumentWindow(cloneWin);
    RefreshTabState(cloneWin);
    SortTabBar(cloneWin);
    
    return cloneWin;
}

static void moveDocumentCB(Widget dialog, WindowInfo *window,
	XtPointer call_data)
{
    XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *) call_data;
    DoneWithMoveDocumentDialog = cbs->reason;
}

/*
** present dialog for selecting a target window to move this document
** into. Do nothing if there is only one shell window opened.
*/
void MoveDocumentDialog(WindowInfo *window)
{
    WindowInfo *win, *targetWin, **shellWinList;
    int i, nList=0, nWindows=0, ac;
    char tmpStr[MAXPATHLEN+50];
    Widget parent, dialog, listBox, moveAllOption;
    XmString *list = NULL;
    XmString popupTitle, s1;
    Arg csdargs[20];
    int *position_list, position_count;
    
    /* get the list of available shell windows, not counting
       the document to be moved */    
    nWindows = NWindows();
    list = (XmStringTable) NEditMalloc(nWindows * sizeof(XmString *));
    shellWinList = (WindowInfo **) NEditMalloc(nWindows * sizeof(WindowInfo *));

    for (win=WindowList; win; win=win->next) {
	if (!IsTopDocument(win) || win->shell == window->shell)
	    continue;
	
	sprintf(tmpStr, "%s%s",
		win->filenameSet? win->path : "", win->filename);

	list[nList] = XmStringCreateSimple(tmpStr);
	shellWinList[nList] = win;
	nList++;
    }

    /* stop here if there's no other window to move to */
    if (!nList) {
        NEditFree(list);
        NEditFree(shellWinList);
        return;
    }

    /* create the dialog */
    parent = window->shell;
    popupTitle = XmStringCreateSimple("Move Document");
    sprintf(tmpStr, "Move %s into window of", window->filename);
    s1 = XmStringCreateSimple(tmpStr);
    ac = 0;
    XtSetArg(csdargs[ac], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); ac++;
    XtSetArg(csdargs[ac], XmNdialogTitle, popupTitle); ac++;
    XtSetArg(csdargs[ac], XmNlistLabelString, s1); ac++;
    XtSetArg(csdargs[ac], XmNlistItems, list); ac++;
    XtSetArg(csdargs[ac], XmNlistItemCount, nList); ac++;
    XtSetArg(csdargs[ac], XmNvisibleItemCount, 12); ac++;
    XtSetArg(csdargs[ac], XmNautoUnmanage, False); ac++;
    dialog = CreateSelectionDialog(parent,"moveDocument",csdargs,ac);
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_SELECTION_LABEL));        
    XtAddCallback(dialog, XmNokCallback, (XtCallbackProc)moveDocumentCB, window);
    XtAddCallback(dialog, XmNapplyCallback, (XtCallbackProc)moveDocumentCB, window);
    XtAddCallback(dialog, XmNcancelCallback, (XtCallbackProc)moveDocumentCB, window);
    XmStringFree(s1);
    XmStringFree(popupTitle);

    /* free the window list */
    for (i=0; i<nList; i++)
	XmStringFree(list[i]);
    NEditFree(list);    

    /* create the option box for moving all documents */    
    s1 = MKSTRING("Move all documents in this window");
    moveAllOption =  XtVaCreateWidget("moveAll", 
    	    xmToggleButtonWidgetClass, dialog,
	    XmNlabelString, s1,
	    XmNalignment, XmALIGNMENT_BEGINNING,
	    NULL);
    XmStringFree(s1);
    
    if (NDocuments(window) >1)
	XtManageChild(moveAllOption);

    /* disable option if only one document in the window */
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_APPLY_BUTTON));

    s1 = MKSTRING("Move");
    XtVaSetValues (dialog, XmNokLabelString, s1, NULL);
    XmStringFree(s1);
    
    /* default to the first window on the list */
    listBox = XmSelectionBoxGetChild(dialog, XmDIALOG_LIST);
    XmListSelectPos(listBox, 1, True);

    /* show the dialog */
    DoneWithMoveDocumentDialog = 0;
    ManageDialogCenteredOnPointer(dialog);
    while (!DoneWithMoveDocumentDialog)
        XtAppProcessEvent(XtWidgetToApplicationContext(parent), XtIMAll);

    /* get the window to move document into */   
    XmListGetSelectedPos(listBox, &position_list, &position_count);
    targetWin = shellWinList[position_list[0]-1];
    NEditFree(position_list);
    
    /* now move document(s) */
    if (DoneWithMoveDocumentDialog == XmCR_OK) {
    	/* move top document */
	if (XmToggleButtonGetState(moveAllOption)) {
    	    /* move all documents */
	    for (win = WindowList; win; ) {		
    		if (win != window && win->shell == window->shell) {
	    	    WindowInfo *next = win->next;
    	    	    MoveDocument(targetWin, win);
		    win = next;
		}
		else
	    	    win = win->next;
	    }

	    /* invoking document is the last to move */
    	    MoveDocument(targetWin, window);
	}
	else {
    	    MoveDocument(targetWin, window);
	}
    }

    NEditFree(shellWinList);    
    XtDestroyWidget(dialog);	
}

static void hideTooltip(Widget tab)
{
    Widget tooltip = XtNameToWidget(tab, "*BubbleShell");
    
    if (tooltip)
    	XtPopdown(tooltip);
}

static void closeTabProc(XtPointer clientData, XtIntervalId *id)
{
    CloseFileAndWindow((WindowInfo*)clientData, PROMPT_SBC_DIALOG_RESPONSE);
}

/*
** callback to close-tab button.
*/
static void closeTabCB(Widget w, Widget mainWin, caddr_t callData)
{
    /* FIXME: XtRemoveActionHook() related coredump
    
       An unknown bug seems to be associated with the XtRemoveActionHook()
       call in FinishLearn(), which resulted in coredump if a tab was
       closed, in the middle of keystrokes learning, by clicking on the 
       close-tab button.
       
       As evident to our accusation, the coredump may be surpressed by 
       simply commenting out the XtRemoveActionHook() call. The bug was
       consistent on both Motif and Lesstif on various platforms.

       Closing the tab through either the "Close" menu or its accel key, 
       however, was without any trouble.
       
       While its actual mechanism is not well understood, we somehow 
       managed to workaround the bug by delaying the action of closing
       the tab. For now. */
    XtAppAddTimeOut(XtWidgetToApplicationContext(w), 0,
    	    closeTabProc, GetTopDocument(mainWin));
}

/*
** callback to clicks on a tab to raise it's document.
*/
static void raiseTabCB(Widget w, XtPointer clientData, XtPointer callData)
{
    XmLFolderCallbackStruct *cbs = (XmLFolderCallbackStruct *)callData;
    WidgetList tabList;
    Widget tab;

    XtVaGetValues(w, XmNtabWidgetList, &tabList, NULL);
    tab = tabList[cbs->pos];
    RaiseDocument(TabToWindow(tab));
}

static Widget containingPane(Widget w)
{
    /* The containing pane used to simply be the first parent, but with
       the introduction of an XmFrame, it's the grandparent. */
    return XtParent(XtParent(w));
}

static void cancelTimeOut(XtIntervalId *timer)
{
    if (*timer != 0)
    {
        XtRemoveTimeOut(*timer);
        *timer = 0;
    }    
}

/*
** set/clear toggle menu state if the calling document is on top.
*/
void SetToggleButtonState(WindowInfo *window, Widget w, Boolean state, 
        Boolean notify)
{
    if (IsTopDocument(window)) {
    	XmToggleButtonSetState(w, state, notify);
    }
}

/*
** set/clear menu sensitivity if the calling document is on top.
*/
void SetSensitive(WindowInfo *window, Widget w, Boolean sensitive)
{
    if (IsTopDocument(window)) {
    	XtSetSensitive(w, sensitive);
    }
}

/*
** Remove redundant expose events on tab bar.
*/
void CleanUpTabBarExposeQueue(WindowInfo *window)
{
    XEvent event;
    XExposeEvent ev;
    int count;
    
    if (window == NULL)
    	return;
    
    /* remove redundant expose events on tab bar */
    count=0;
    while (XCheckTypedWindowEvent(TheDisplay, XtWindow(window->tabBar), 
	   Expose, &event))
	count++;

    /* now we can update tabbar */
    if (count) {
	ev.type = Expose;
	ev.display = TheDisplay;
	ev.window = XtWindow(window->tabBar);
	ev.x = 0;
	ev.y = 0;
	ev.width = XtWidth(window->tabBar);
	ev.height = XtHeight(window->tabBar);
	ev.count = 0;
	XSendEvent(TheDisplay, XtWindow(window->tabBar), False,
		ExposureMask, (XEvent *)&ev);
    }
}    
