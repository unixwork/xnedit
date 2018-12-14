/*******************************************************************************
*                                                                              *
* menu.c -- Nirvana Editor menus                                               *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
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

#include "menu.h"
#include "textBuf.h"
#include "text.h"
#include "nedit.h"
#include "file.h"
#include "window.h"
#include "search.h"
#include "selection.h"
#include "undo.h"
#include "shift.h"
#include "help.h"
#include "preferences.h"
#include "tags.h"
#include "userCmds.h"
#include "shell.h"
#include "macro.h"
#include "highlight.h"
#include "highlightData.h"
#include "interpret.h"
#include "smartIndent.h"
#include "windowTitle.h"
#include "../util/getfiles.h"
#include "../util/DialogF.h"
#include "../util/misc.h"
#include "../util/fileUtils.h"
#include "../util/utils.h"
#include "../Xlt/BubbleButton.h"
#include "../util/nedit_malloc.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#include <X11/X.h>
#include <Xm/Xm.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/MenuShell.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#if XmVersion >= 1002
#define MENU_WIDGET(w) (XmGetPostedFromWidget(XtParent(w)))
#else
#define MENU_WIDGET(w) (w)
#endif

/* Menu modes for SGI_CUSTOM short-menus feature */
enum menuModes {FULL, SHORT};

typedef void (*menuCallbackProc)();

extern void _XmDismissTearOff(Widget, XtPointer, XtPointer);

static void doActionCB(Widget w, XtPointer clientData, XtPointer callData);
static void doTabActionCB(Widget w, XtPointer clientData, XtPointer callData);
static void pasteColCB(Widget w, XtPointer clientData, XtPointer callData); 
static void shiftLeftCB(Widget w, XtPointer clientData, XtPointer callData);
static void shiftRightCB(Widget w, XtPointer clientData, XtPointer callData);
static void findCB(Widget w, XtPointer clientData, XtPointer callData);
static void findSameCB(Widget w, XtPointer clientData, XtPointer callData);
static void findSelCB(Widget w, XtPointer clientData, XtPointer callData);
static void findIncrCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceSameCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceFindSameCB(Widget w, XtPointer clientData, XtPointer callData);
static void markCB(Widget w, XtPointer clientData, XtPointer callData);
static void gotoMarkCB(Widget w, XtPointer clientData, XtPointer callData);
static void gotoMatchingCB(Widget w, XtPointer clientData, XtPointer callData);
static void autoIndentOffCB(Widget w, WindowInfo *window, caddr_t callData);
static void autoIndentCB(Widget w, WindowInfo *window, caddr_t callData);
static void smartIndentCB(Widget w, WindowInfo *window, caddr_t callData);
static void preserveCB(Widget w, WindowInfo *window, caddr_t callData);
static void autoSaveCB(Widget w, WindowInfo *window, caddr_t callData);
static void newlineWrapCB(Widget w, WindowInfo *window, caddr_t callData);
static void noWrapCB(Widget w, WindowInfo *window, caddr_t callData);
static void continuousWrapCB(Widget w, WindowInfo *window, caddr_t callData);
static void wrapMarginCB(Widget w, WindowInfo *window, caddr_t callData);
static void fontCB(Widget w, WindowInfo *window, caddr_t callData);
static void tabsCB(Widget w, WindowInfo *window, caddr_t callData);
static void backlightCharsCB(Widget w, WindowInfo *window, caddr_t callData);
static void showMatchingOffCB(Widget w, WindowInfo *window, caddr_t callData);
static void showMatchingDelimitCB(Widget w, WindowInfo *window, caddr_t callData);
static void showMatchingRangeCB(Widget w, WindowInfo *window, caddr_t callData);
static void matchSyntaxBasedCB(Widget w, WindowInfo *window, caddr_t callData);
static void statsCB(Widget w, WindowInfo *window, caddr_t callData);
static void autoIndentOffDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void autoIndentDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void smartIndentDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void autoSaveDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void preserveDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void noWrapDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void newlineWrapDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void contWrapDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void wrapMarginDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void shellSelDefCB(Widget widget, WindowInfo* window, caddr_t callData);
static void openInTabDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void tabBarDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void tabBarHideDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void tabSortDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void toolTipsDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void tabNavigateDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void statsLineDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void iSearchLineDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void lineNumsDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void pathInWindowsMenuDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void customizeTitleDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void tabsDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void showMatchingOffDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void showMatchingDelimitDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void showMatchingRangeDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void matchSyntaxBasedDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void highlightOffDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void highlightDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void backlightCharsDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void fontDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void colorDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void smartTagsDefCB(Widget parent, XtPointer client_data, XtPointer call_data);
static void showAllTagsDefCB(Widget parent, XtPointer client_data, XtPointer call_data);
static void languageDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void highlightingDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void smartMacrosDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void stylesDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void shellDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void macroDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void bgMenuDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void searchDlogsDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void beepOnSearchWrapDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void keepSearchDlogsDefCB(Widget w, WindowInfo *window,
	caddr_t callData);
static void searchWrapsDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void appendLFCB(Widget w, WindowInfo* window, caddr_t callData);
static void sortOpenPrevDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void reposDlogsDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void autoScrollDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void modWarnDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void modWarnRealDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void exitWarnDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void searchLiteralCB(Widget w, WindowInfo *window, caddr_t callData);
static void searchCaseSenseCB(Widget w, WindowInfo *window, caddr_t callData);
static void searchLiteralWordCB(Widget w, WindowInfo *window, caddr_t callData);
static void searchCaseSenseWordCB(Widget w, WindowInfo *window, caddr_t callData);
static void searchRegexNoCaseCB(Widget w, WindowInfo *window, caddr_t callData);
static void searchRegexCB(Widget w, WindowInfo *window, caddr_t callData);
#ifdef REPLACE_SCOPE
static void replaceScopeWindowCB(Widget w, WindowInfo *window, caddr_t callData);
static void replaceScopeSelectionCB(Widget w, WindowInfo *window, caddr_t callData);
static void replaceScopeSmartCB(Widget w, WindowInfo *window, caddr_t callData);
#endif
static void size24x80CB(Widget w, WindowInfo *window, caddr_t callData);
static void size40x80CB(Widget w, WindowInfo *window, caddr_t callData);
static void size60x80CB(Widget w, WindowInfo *window, caddr_t callData);
static void size80x80CB(Widget w, WindowInfo *window, caddr_t callData);
static void sizeCustomCB(Widget w, WindowInfo *window, caddr_t callData);
static void savePrefCB(Widget w, WindowInfo *window, caddr_t callData);
static void formFeedCB(Widget w, XtPointer clientData, XtPointer callData);
static void cancelShellCB(Widget w, WindowInfo *window, XtPointer callData);
static void learnCB(Widget w, WindowInfo *window, caddr_t callData);
static void finishLearnCB(Widget w, WindowInfo *window, caddr_t callData);
static void cancelLearnCB(Widget w, WindowInfo *window, caddr_t callData);
static void replayCB(Widget w, WindowInfo *window, caddr_t callData);
static void windowMenuCB(Widget w, WindowInfo *window, caddr_t callData);
static void prevOpenMenuCB(Widget w, WindowInfo *window, caddr_t callData);
static void unloadTagsFileMenuCB(Widget w, WindowInfo *window,
	caddr_t callData);
static void unloadTipsFileMenuCB(Widget w, WindowInfo *window,
	caddr_t callData);
static void newAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void newOppositeAP(Widget w, XEvent *event, String *args, 
        Cardinal *nArgs); 
static void newTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void openDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs); 
static void openAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void openSelectedAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void closeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void saveAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void saveAsDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs); 
static void saveAsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void revertDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void revertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void includeDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs); 
static void includeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void loadMacroDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs) ;
static void loadMacroAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void loadTagsDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs); 
static void loadTagsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void unloadTagsAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs); 
static void loadTipsDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs); 
static void loadTipsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void unloadTipsAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs); 
static void printAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void printSelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void exitAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void undoAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void redoAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void clearAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void selAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void shiftLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void shiftLeftTabAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void shiftRightAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void shiftRightTabAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void findDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void findAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void findSameAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void findSelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void findIncrAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void startIncrFindAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void replaceDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void replaceAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void replaceAllAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void replaceInSelAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void replaceSameAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void replaceFindAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void replaceFindSameAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void gotoAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void gotoDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void gotoSelectedAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void repeatDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void repeatMacroAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs);
static void markAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void markDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void gotoMarkAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void gotoMarkDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void selectToMatchingAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void gotoMatchingAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void findDefAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void showTipAP(Widget w, XEvent *event, String *args, Cardinal *nArgs); 
static void splitPaneAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void detachDocumentDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void detachDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void moveDocumentDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void nextDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void prevDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void lastDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void closePaneAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void capitalizeAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void lowercaseAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void fillAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void controlDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
#ifndef VMS
static void filterDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void shellFilterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void execDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void execAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void execLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void shellMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
#endif
static void macroMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void bgMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void beginningOfSelectionAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void endOfSelectionAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static Widget createMenu(Widget parent, char *name, char *label,
    	char mnemonic, Widget *cascadeBtn, int mode);
static Widget createMenuItem(Widget parent, char *name, char *label,
	char mnemonic, menuCallbackProc callback, void *cbArg, int mode);
static Widget createFakeMenuItem(Widget parent, char *name,
	menuCallbackProc callback, void *cbArg);
static Widget createMenuToggle(Widget parent, char *name, char *label,
	char mnemonic, menuCallbackProc callback, void *cbArg, int set,
	int mode);
static Widget createMenuRadioToggle(Widget parent, char *name, char *label,
	char mnemonic, menuCallbackProc callback, void *cbArg, int set,
	int mode);
static Widget createMenuSeparator(Widget parent, char *name, int mode);
static void invalidatePrevOpenMenus(void);
static void updateWindowMenu(const WindowInfo *window);
static void updatePrevOpenMenu(WindowInfo *window);
static void updateTagsFileMenu(WindowInfo *window);
static void updateTipsFileMenu(WindowInfo *window);
static int searchDirection(int ignoreArgs, String *args, Cardinal *nArgs);
static int searchWrap(int ignoreArgs, String *args, Cardinal *nArgs);
static int searchKeepDialogs(int ignoreArgs, String *args, Cardinal *nArgs);
static int searchType(int ignoreArgs, String *args, Cardinal *nArgs);
static char **shiftKeyToDir(XtPointer callData);
static void raiseCB(Widget w, WindowInfo *window, caddr_t callData);
static void openPrevCB(Widget w, char *name, caddr_t callData);
static void unloadTagsFileCB(Widget w, char *name, caddr_t callData);
static void unloadTipsFileCB(Widget w, char *name, caddr_t callData);
static int cmpStrPtr(const void *strA, const void *strB);
static void setWindowSizeDefault(int rows, int cols);
static void updateWindowSizeMenus(void);
static void updateWindowSizeMenu(WindowInfo *win);
static int strCaseCmp(const char *str1, const char *str2);
static int compareWindowNames(const void *windowA, const void *windowB);
static void bgMenuPostAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void tabMenuPostAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void raiseWindowAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs);
static void focusPaneAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setStatisticsLineAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setIncrementalSearchLineAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setShowLineNumbersAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setAutoIndentAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setWrapTextAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setWrapMarginAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setHighlightSyntaxAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setMakeBackupCopyAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setIncrementalBackupAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setShowMatchingAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setMatchSyntaxBasedAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setOvertypeModeAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setLockedAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setUseTabsAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setEmTabDistAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setTabDistAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setFontsAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
static void setLanguageModeAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs);
#ifdef SGI_CUSTOM
static void shortMenusCB(Widget w, WindowInfo *window, caddr_t callData);
static void addToToggleShortList(Widget w);
static int shortPrefAskDefault(Widget parent, Widget w, const char *settingName);
#endif

static HelpMenu * buildHelpMenu( Widget pane, HelpMenu * menu, 
	WindowInfo * window);

/* Application action table */
static XtActionsRec Actions[] = {
    {"new", newAP},
    {"new_opposite", newOppositeAP},
    {"new_tab", newTabAP},
    {"open", openAP},
    {"open-dialog", openDialogAP},
    {"open_dialog", openDialogAP},
    {"open-selected", openSelectedAP},
    {"open_selected", openSelectedAP},
    {"close", closeAP},
    {"save", saveAP},
    {"save-as", saveAsAP},
    {"save_as", saveAsAP},
    {"save-as-dialog", saveAsDialogAP},
    {"save_as_dialog", saveAsDialogAP},
    {"revert-to-saved", revertAP},
    {"revert_to_saved", revertAP},
    {"revert_to_saved_dialog", revertDialogAP},
    {"include-file", includeAP},
    {"include_file", includeAP},
    {"include-file-dialog", includeDialogAP},
    {"include_file_dialog", includeDialogAP},
    {"load-macro-file", loadMacroAP},
    {"load_macro_file", loadMacroAP},
    {"load-macro-file-dialog", loadMacroDialogAP},
    {"load_macro_file_dialog", loadMacroDialogAP},
    {"load-tags-file", loadTagsAP},
    {"load_tags_file", loadTagsAP},
    {"load-tags-file-dialog", loadTagsDialogAP},
    {"load_tags_file_dialog", loadTagsDialogAP},
    {"unload_tags_file", unloadTagsAP},
    {"load_tips_file", loadTipsAP},
    {"load_tips_file_dialog", loadTipsDialogAP},
    {"unload_tips_file", unloadTipsAP},
    {"print", printAP},
    {"print-selection", printSelAP},
    {"print_selection", printSelAP},
    {"exit", exitAP},
    {"undo", undoAP},
    {"redo", redoAP},
    {"delete", clearAP},
    {"select-all", selAllAP},
    {"select_all", selAllAP},
    {"shift-left", shiftLeftAP},
    {"shift_left", shiftLeftAP},
    {"shift-left-by-tab", shiftLeftTabAP},
    {"shift_left_by_tab", shiftLeftTabAP},
    {"shift-right", shiftRightAP},
    {"shift_right", shiftRightAP},
    {"shift-right-by-tab", shiftRightTabAP},
    {"shift_right_by_tab", shiftRightTabAP},
    {"find", findAP},
    {"find-dialog", findDialogAP},
    {"find_dialog", findDialogAP},
    {"find-again", findSameAP},
    {"find_again", findSameAP},
    {"find-selection", findSelAP},
    {"find_selection", findSelAP},
    {"find_incremental", findIncrAP},
    {"start_incremental_find", startIncrFindAP},
    {"replace", replaceAP},
    {"replace-dialog", replaceDialogAP},
    {"replace_dialog", replaceDialogAP},
    {"replace-all", replaceAllAP},
    {"replace_all", replaceAllAP},
    {"replace-in-selection", replaceInSelAP},
    {"replace_in_selection", replaceInSelAP},
    {"replace-again", replaceSameAP},
    {"replace_again", replaceSameAP},
    {"replace_find", replaceFindAP},
    {"replace_find_same", replaceFindSameAP},
    {"replace_find_again", replaceFindSameAP},
    {"goto-line-number", gotoAP},
    {"goto_line_number", gotoAP},
    {"goto-line-number-dialog", gotoDialogAP},
    {"goto_line_number_dialog", gotoDialogAP},
    {"goto-selected", gotoSelectedAP},
    {"goto_selected", gotoSelectedAP},
    {"mark", markAP},
    {"mark-dialog", markDialogAP},
    {"mark_dialog", markDialogAP},
    {"goto-mark", gotoMarkAP},
    {"goto_mark", gotoMarkAP},
    {"goto-mark-dialog", gotoMarkDialogAP},
    {"goto_mark_dialog", gotoMarkDialogAP},
    {"match", selectToMatchingAP},
    {"select_to_matching", selectToMatchingAP},
    {"goto_matching", gotoMatchingAP},
    {"find-definition", findDefAP},
    {"find_definition", findDefAP},
    {"show_tip", showTipAP},
    {"split-pane", splitPaneAP},
    {"split_pane", splitPaneAP},
    {"close-pane", closePaneAP},
    {"close_pane", closePaneAP},
    {"detach_document", detachDocumentAP},
    {"detach_document_dialog", detachDocumentDialogAP},
    {"move_document_dialog", moveDocumentDialogAP},
    {"next_document", nextDocumentAP},
    {"previous_document", prevDocumentAP},
    {"last_document", lastDocumentAP},
    {"uppercase", capitalizeAP},
    {"lowercase", lowercaseAP},
    {"fill-paragraph", fillAP},
    {"fill_paragraph", fillAP},
    {"control-code-dialog", controlDialogAP},
    {"control_code_dialog", controlDialogAP},
#ifndef VMS
    {"filter-selection-dialog", filterDialogAP},
    {"filter_selection_dialog", filterDialogAP},
    {"filter-selection", shellFilterAP},
    {"filter_selection", shellFilterAP},
    {"execute-command", execAP},
    {"execute_command", execAP},
    {"execute-command-dialog", execDialogAP},
    {"execute_command_dialog", execDialogAP},
    {"execute-command-line", execLineAP},
    {"execute_command_line", execLineAP},
    {"shell-menu-command", shellMenuAP},
    {"shell_menu_command", shellMenuAP},
#endif /*VMS*/
    {"macro-menu-command", macroMenuAP},
    {"macro_menu_command", macroMenuAP},
    {"bg_menu_command", bgMenuAP},
    {"post_window_bg_menu", bgMenuPostAP},
    {"post_tab_context_menu", tabMenuPostAP},
    {"beginning-of-selection", beginningOfSelectionAP},
    {"beginning_of_selection", beginningOfSelectionAP},
    {"end-of-selection", endOfSelectionAP},
    {"end_of_selection", endOfSelectionAP},
    {"repeat_macro", repeatMacroAP},
    {"repeat_dialog", repeatDialogAP},
    {"raise_window", raiseWindowAP},
    {"focus_pane", focusPaneAP},
    {"set_statistics_line", setStatisticsLineAP},
    {"set_incremental_search_line", setIncrementalSearchLineAP},
    {"set_show_line_numbers", setShowLineNumbersAP},
    {"set_auto_indent", setAutoIndentAP},
    {"set_wrap_text", setWrapTextAP},
    {"set_wrap_margin", setWrapMarginAP},
    {"set_highlight_syntax", setHighlightSyntaxAP},
#ifndef VMS
    {"set_make_backup_copy", setMakeBackupCopyAP},
#endif
    {"set_incremental_backup", setIncrementalBackupAP},
    {"set_show_matching", setShowMatchingAP},
    {"set_match_syntax_based", setMatchSyntaxBasedAP},
    {"set_overtype_mode", setOvertypeModeAP},
    {"set_locked", setLockedAP},
    {"set_tab_dist", setTabDistAP},
    {"set_em_tab_dist", setEmTabDistAP},
    {"set_use_tabs", setUseTabsAP},
    {"set_fonts", setFontsAP},
    {"set_language_mode", setLanguageModeAP}
};

/* List of previously opened files for File menu */
static int NPrevOpen = 0;
static char** PrevOpen = NULL;

#ifdef SGI_CUSTOM
/* Window to receive items to be toggled on and off in short menus mode */
static WindowInfo *ShortMenuWindow;
#endif

void HidePointerOnKeyedEvent(Widget w, XEvent *event)
{
    if (event && (event->type == KeyPress || event->type == KeyRelease)) {
        ShowHidePointer((TextWidget)w, True);
    }
}

/*
** Install actions for use in translation tables and macro recording, relating
** to menu item commands
*/
void InstallMenuActions(XtAppContext context)
{
    XtAppAddActions(context, Actions, XtNumber(Actions));
}

/*
** Return the (statically allocated) action table for menu item actions.
*/
XtActionsRec *GetMenuActions(int *nActions)
{
    *nActions = XtNumber(Actions);
    return Actions;
}

/*
** Create the menu bar
*/
Widget CreateMenuBar(Widget parent, WindowInfo *window)
{
    Widget menuBar, menuPane, btn, subPane, subSubPane, subSubSubPane, cascade;

    /*
    ** cache user menus:
    ** allocate user menu cache
    */
    window->userMenuCache = CreateUserMenuCache();

    /*
    ** Create the menu bar (row column) widget
    */
    menuBar = XmCreateMenuBar(parent, "menuBar", NULL, 0);

#ifdef SGI_CUSTOM
    /*
    ** Short menu mode is a special feature for the SGI system distribution
    ** version of NEdit.
    **
    ** To make toggling short-menus mode faster (re-creating the menus was
    ** too slow), a list is kept in the window data structure of items to
    ** be turned on and off.  Initialize that list and give the menu creation
    ** routines a pointer to the window on which this list is kept.  This is
    ** (unfortunately) a global variable to keep the interface simple for
    ** the mainstream case.
    */
    ShortMenuWindow = window;
    window->nToggleShortItems = 0;
#endif

    /*
    ** "File" pull down menu.
    */
    menuPane = createMenu(menuBar, "fileMenu", "File", 0, NULL, SHORT);
    createMenuItem(menuPane, "new", "New", 'N', doActionCB, "new", SHORT);
    if ( GetPrefOpenInTab() )
        window->newOppositeItem = createMenuItem(menuPane, "newOpposite", 
                "New Window", 'W', doActionCB, "new_opposite", SHORT);
    else
        window->newOppositeItem = createMenuItem(menuPane, "newOpposite", 
                "New Tab", 'T', doActionCB, "new_opposite", SHORT);
    createMenuItem(menuPane, "open", "Open...", 'O', doActionCB, "open_dialog",
    	    SHORT);
    window->openSelItem=createMenuItem(menuPane, "openSelected", "Open Selected", 'd',
    	    doActionCB, "open_selected", FULL);
    if (GetPrefMaxPrevOpenFiles() > 0) {
	window->prevOpenMenuPane = createMenu(menuPane, "openPrevious",
    		"Open Previous", 'v', &window->prevOpenMenuItem, SHORT);
	XtSetSensitive(window->prevOpenMenuItem, NPrevOpen != 0);
	XtAddCallback(window->prevOpenMenuItem, XmNcascadingCallback,
    		(XtCallbackProc)prevOpenMenuCB, window);
    }
    createMenuSeparator(menuPane, "sep1", SHORT);
    window->closeItem = createMenuItem(menuPane, "close", "Close", 'C',
    	    doActionCB, "close", SHORT);
    createMenuItem(menuPane, "save", "Save", 'S', doActionCB, "save", SHORT);
    createMenuItem(menuPane, "saveAs", "Save As...", 'A', doActionCB,
    	    "save_as_dialog", SHORT);
    createMenuItem(menuPane, "revertToSaved", "Revert to Saved", 'R',
    	    doActionCB, "revert_to_saved_dialog", SHORT);
    createMenuSeparator(menuPane, "sep2", SHORT);
    createMenuItem(menuPane, "includeFile", "Include File...", 'I',
    	    doActionCB, "include_file_dialog", SHORT);
    createMenuItem(menuPane, "loadMacroFile", "Load Macro File...", 'M',
    	    doActionCB, "load_macro_file_dialog", FULL);
    createMenuItem(menuPane, "loadTagsFile", "Load Tags File...", 'g',
    	    doActionCB, "load_tags_file_dialog", FULL);
    window->unloadTagsMenuPane = createMenu(menuPane, "unloadTagsFiles",
	    "Unload Tags File", 'U', &window->unloadTagsMenuItem, FULL);
    XtSetSensitive(window->unloadTagsMenuItem, TagsFileList != NULL);
    XtAddCallback(window->unloadTagsMenuItem, XmNcascadingCallback,
	    (XtCallbackProc)unloadTagsFileMenuCB, window);
    createMenuItem(menuPane, "loadTipsFile", "Load Calltips File...", 'F',
    	    doActionCB, "load_tips_file_dialog", FULL);
    window->unloadTipsMenuPane = createMenu(menuPane, "unloadTipsFiles",
	    "Unload Calltips File", 'e', &window->unloadTipsMenuItem, FULL);
    XtSetSensitive(window->unloadTipsMenuItem, TipsFileList != NULL);
    XtAddCallback(window->unloadTipsMenuItem, XmNcascadingCallback,
	    (XtCallbackProc)unloadTipsFileMenuCB, window);
    createMenuSeparator(menuPane, "sep3", SHORT);
    createMenuItem(menuPane, "print", "Print...", 'P', doActionCB, "print",
    	    SHORT);
    window->printSelItem = createMenuItem(menuPane, "printSelection",
    	    "Print Selection...", 'l', doActionCB, "print_selection",
    	    SHORT);
    XtSetSensitive(window->printSelItem, window->wasSelected);
    createMenuSeparator(menuPane, "sep4", SHORT);
    createMenuItem(menuPane, "exit", "Exit", 'x', doActionCB, "exit", SHORT);   
    CheckCloseDim();

    /* 
    ** "Edit" pull down menu.
    */
    menuPane = createMenu(menuBar, "editMenu", "Edit", 0, NULL, SHORT);
    window->undoItem = createMenuItem(menuPane, "undo", "Undo", 'U',
    	    doActionCB, "undo", SHORT);
    XtSetSensitive(window->undoItem, False);
    window->redoItem = createMenuItem(menuPane, "redo", "Redo", 'R',
    	    doActionCB, "redo", SHORT);
    XtSetSensitive(window->redoItem, False);
    createMenuSeparator(menuPane, "sep1", SHORT);
    window->cutItem = createMenuItem(menuPane, "cut", "Cut", 't', doActionCB,
    	    "cut_clipboard", SHORT);
    XtSetSensitive(window->cutItem, window->wasSelected);
    window->copyItem = createMenuItem(menuPane, "copy", "Copy", 'C', doActionCB,
    	    "copy_clipboard", SHORT);
    XtSetSensitive(window->copyItem, window->wasSelected);
    createMenuItem(menuPane, "paste", "Paste", 'P', doActionCB,
    	    "paste_clipboard", SHORT);
    createMenuItem(menuPane, "pasteColumn", "Paste Column", 's', pasteColCB,
    	    window, SHORT);
    window->delItem=createMenuItem(menuPane, "delete", "Delete", 'D', doActionCB, "delete_selection",
    	    SHORT);
    XtSetSensitive(window->delItem, window->wasSelected);
    createMenuItem(menuPane, "selectAll", "Select All", 'A', doActionCB,
    	    "select_all", SHORT);
    createMenuSeparator(menuPane, "sep2", SHORT);
    createMenuItem(menuPane, "shiftLeft", "Shift Left", 'L',
    	    shiftLeftCB, window, SHORT);
    createFakeMenuItem(menuPane, "shiftLeftShift", shiftLeftCB, window);
    createMenuItem(menuPane, "shiftRight", "Shift Right", 'g',
    	    shiftRightCB, window, SHORT);
    createFakeMenuItem(menuPane, "shiftRightShift", shiftRightCB, window);
    window->lowerItem=createMenuItem(menuPane, "lowerCase", "Lower-case", 'w',
    	    doActionCB, "lowercase", SHORT);
    window->upperItem=createMenuItem(menuPane, "upperCase", "Upper-case", 'e',
    	    doActionCB, "uppercase", SHORT);
    createMenuItem(menuPane, "fillParagraph", "Fill Paragraph", 'F',
    	    doActionCB, "fill_paragraph", SHORT);
    createMenuSeparator(menuPane, "sep3", FULL);
    createMenuItem(menuPane, "insertFormFeed", "Insert Form Feed", 'I',
    	    formFeedCB, window, FULL);
    createMenuItem(menuPane, "insertCtrlCode", "Insert Ctrl Code...", 'n',
    	    doActionCB, "control_code_dialog", FULL);
#ifdef SGI_CUSTOM
    createMenuSeparator(menuPane, "sep4", SHORT);
    window->overtypeModeItem = createMenuToggle(menuPane, "overtype", "Overtype", 'O',
    	    doActionCB, "set_overtype_mode", False, SHORT);
    window->readOnlyItem = createMenuToggle(menuPane, "readOnly", "Read Only",
    	    'y', doActionCB, "set_locked", IS_USER_LOCKED(window->lockReasons), FULL);
#endif

    /* 
    ** "Search" pull down menu.
    */
    menuPane = createMenu(menuBar, "searchMenu", "Search", 0, NULL, SHORT);
    createMenuItem(menuPane, "find", "Find...", 'F', findCB, window, SHORT);
    createFakeMenuItem(menuPane, "findShift", findCB, window);
    window->findAgainItem=createMenuItem(menuPane, "findAgain", "Find Again", 'i', findSameCB, window,
    	    SHORT);
    XtSetSensitive(window->findAgainItem, NHist);
    createFakeMenuItem(menuPane, "findAgainShift", findSameCB, window);
    window->findSelItem=createMenuItem(menuPane, "findSelection", "Find Selection", 'S',
    	    findSelCB, window, SHORT);
    createFakeMenuItem(menuPane, "findSelectionShift", findSelCB, window);
    createMenuItem(menuPane, "findIncremental", "Find Incremental", 'n',
	    findIncrCB, window, SHORT);
    createFakeMenuItem(menuPane, "findIncrementalShift", findIncrCB, window);
    createMenuItem(menuPane, "replace", "Replace...", 'R', replaceCB, window,
    	    SHORT);
    createFakeMenuItem(menuPane, "replaceShift", replaceCB, window);
    window->replaceFindAgainItem=createMenuItem(menuPane, "replaceFindAgain", "Replace Find Again", 'A',
    	    replaceFindSameCB, window, SHORT);
    XtSetSensitive(window->replaceFindAgainItem, NHist);
    createFakeMenuItem(menuPane, "replaceFindAgainShift", replaceFindSameCB, window);
    window->replaceAgainItem=createMenuItem(menuPane, "replaceAgain", "Replace Again", 'p',
    	    replaceSameCB, window, SHORT);
    XtSetSensitive(window->replaceAgainItem, NHist);
    createFakeMenuItem(menuPane, "replaceAgainShift", replaceSameCB, window);
    createMenuSeparator(menuPane, "sep1", FULL);
    createMenuItem(menuPane, "gotoLineNumber", "Goto Line Number...", 'L',
    	    doActionCB, "goto_line_number_dialog", FULL);
    window->gotoSelItem=createMenuItem(menuPane, "gotoSelected", "Goto Selected", 'G',
    	    doActionCB, "goto_selected", FULL);
    createMenuSeparator(menuPane, "sep2", FULL);
    createMenuItem(menuPane, "mark", "Mark", 'k', markCB, window, FULL);
    createMenuItem(menuPane, "gotoMark", "Goto Mark", 'o', gotoMarkCB, window,
    	    FULL);
    createFakeMenuItem(menuPane, "gotoMarkShift", gotoMarkCB, window);
    createMenuSeparator(menuPane, "sep3", FULL);
    createMenuItem(menuPane, "gotoMatching", "Goto Matching (..)", 'M',
    	    gotoMatchingCB, window, FULL);
    createFakeMenuItem(menuPane, "gotoMatchingShift", gotoMatchingCB, window);
    window->findDefItem = createMenuItem(menuPane, "findDefinition",
    	    "Find Definition", 'D', doActionCB, "find_definition", FULL);
    XtSetSensitive(window->findDefItem, TagsFileList != NULL);
    window->showTipItem = createMenuItem(menuPane, "showCalltip",
    	    "Show Calltip", 'C', doActionCB, "show_tip", FULL);
    XtSetSensitive(window->showTipItem, (TagsFileList != NULL || 
                                         TipsFileList != NULL) );
    
    /*
    ** Preferences menu, Default Settings sub menu
    */
    menuPane = createMenu(menuBar, "preferencesMenu", "Preferences", 0, NULL,
    	    SHORT);
    subPane = createMenu(menuPane, "defaultSettings", "Default Settings", 'D',
    	    NULL, FULL);
    createMenuItem(subPane, "languageModes", "Language Modes...", 'L',
    	    languageDefCB, window, FULL);
    
    /* Auto Indent sub menu */
    subSubPane = createMenu(subPane, "autoIndent", "Auto Indent", 'A',
    	    NULL, FULL);
    window->autoIndentOffDefItem = createMenuRadioToggle(subSubPane, "off",
    	    "Off", 'O', autoIndentOffDefCB, window,
    	    GetPrefAutoIndent(PLAIN_LANGUAGE_MODE) == NO_AUTO_INDENT, SHORT);
    window->autoIndentDefItem = createMenuRadioToggle(subSubPane, "on",
    	    "On", 'n', autoIndentDefCB, window,
    	    GetPrefAutoIndent(PLAIN_LANGUAGE_MODE) == AUTO_INDENT, SHORT);
    window->smartIndentDefItem = createMenuRadioToggle(subSubPane, "smart",
    	    "Smart", 'S', smartIndentDefCB, window,
    	    GetPrefAutoIndent(PLAIN_LANGUAGE_MODE) == SMART_INDENT, SHORT);
    createMenuSeparator(subSubPane, "sep1", SHORT);
    createMenuItem(subSubPane, "ProgramSmartIndent", "Program Smart Indent...",
    	    'P', smartMacrosDefCB, window, FULL);
    
    /* Wrap sub menu */
    subSubPane = createMenu(subPane, "wrap", "Wrap", 'W', NULL, FULL);
    window->noWrapDefItem = createMenuRadioToggle(subSubPane,
    	    "none", "None", 'N', noWrapDefCB,
	    window, GetPrefWrap(PLAIN_LANGUAGE_MODE) == NO_WRAP, SHORT);
    window->newlineWrapDefItem = createMenuRadioToggle(subSubPane,
    	    "autoNewline", "Auto Newline", 'A', newlineWrapDefCB,
	    window, GetPrefWrap(PLAIN_LANGUAGE_MODE) == NEWLINE_WRAP, SHORT);
    window->contWrapDefItem = createMenuRadioToggle(subSubPane, "continuous",
    	    "Continuous", 'C', contWrapDefCB, window,
    	    GetPrefWrap(PLAIN_LANGUAGE_MODE) == CONTINUOUS_WRAP, SHORT);
    createMenuSeparator(subSubPane, "sep1", SHORT);
    createMenuItem(subSubPane, "wrapMargin", "Wrap Margin...", 'W',
    	    wrapMarginDefCB, window, SHORT);
    
    /* Smart Tags sub menu */
    subSubPane = createMenu(subPane, "smartTags", "Tag Collisions", 'l',
	    NULL, FULL);
    window->allTagsDefItem = createMenuRadioToggle(subSubPane, "showall",
	    "Show All", 'A', showAllTagsDefCB, window, !GetPrefSmartTags(),
	    FULL);
    window->smartTagsDefItem = createMenuRadioToggle(subSubPane, "smart",
	    "Smart", 'S', smartTagsDefCB, window, GetPrefSmartTags(), FULL);

    createMenuItem(subPane, "shellSel", "Command Shell...", 's', shellSelDefCB,
            window, SHORT);
    createMenuItem(subPane, "tabDistance", "Tab Stops...", 'T', tabsDefCB, window,
    	    SHORT);
    createMenuItem(subPane, "textFont", "Text Fonts...", 'F', fontDefCB, window,
    	    FULL);
    createMenuItem(subPane, "colors", "Colors...", 'C', colorDefCB, window,
    	    FULL);
    
    /* Customize Menus sub menu */
    subSubPane = createMenu(subPane, "customizeMenus", "Customize Menus",
    	    'u', NULL, FULL);
#ifndef VMS
    createMenuItem(subSubPane, "shellMenu", "Shell Menu...", 'S',
    	    shellDefCB, window, FULL);
#endif
    createMenuItem(subSubPane, "macroMenu", "Macro Menu...", 'M',
    	    macroDefCB, window, FULL);
    createMenuItem(subSubPane, "windowBackgroundMenu",
	    "Window Background Menu...", 'W', bgMenuDefCB, window, FULL);
    createMenuSeparator(subSubPane, "sep1", SHORT);
    window->sortOpenPrevDefItem = createMenuToggle(subSubPane, "sortOpenPrevMenu",
            "Sort Open Prev. Menu", 'o', sortOpenPrevDefCB, window,
            GetPrefSortOpenPrevMenu(), FULL);
    window->pathInWindowsMenuDefItem = createMenuToggle(subSubPane, "pathInWindowsMenu",
    	    "Show Path In Windows Menu", 'P', pathInWindowsMenuDefCB, window, GetPrefShowPathInWindowsMenu(),
    	    SHORT);

    createMenuItem(subPane, "custimizeTitle", "Customize Window Title...", 'd',
    	    customizeTitleDefCB, window, FULL);

    /* Search sub menu */
    subSubPane = createMenu(subPane, "searching", "Searching",
    	    'g', NULL, FULL);
    window->searchDlogsDefItem = createMenuToggle(subSubPane, "verbose",
    	    "Verbose", 'V', searchDlogsDefCB, window,
    	    GetPrefSearchDlogs(), SHORT);
    window->searchWrapsDefItem = createMenuToggle(subSubPane, "wrapAround",
    	    "Wrap Around", 'W', searchWrapsDefCB, window,
    	    GetPrefSearchWraps(), SHORT);
    window->beepOnSearchWrapDefItem = createMenuToggle(subSubPane,
	  "beepOnSearchWrap", "Beep On Search Wrap", 'B',
	  beepOnSearchWrapDefCB, window, GetPrefBeepOnSearchWrap(), SHORT);
    window->keepSearchDlogsDefItem = createMenuToggle(subSubPane,
    	    "keepDialogsUp", "Keep Dialogs Up", 'K',
    	    keepSearchDlogsDefCB, window, GetPrefKeepSearchDlogs(), SHORT);
    subSubSubPane = createMenu(subSubPane, "defaultSearchStyle",
    	    "Default Search Style", 'D', NULL, FULL);
    XtVaSetValues(subSubSubPane, XmNradioBehavior, True, NULL); 
    window->searchLiteralDefItem = createMenuToggle(subSubSubPane, "literal",
    	    "Literal", 'L', searchLiteralCB, window,
    	    GetPrefSearch() == SEARCH_LITERAL, FULL);
    window->searchCaseSenseDefItem = createMenuToggle(subSubSubPane,
    	    "caseSensitive", "Literal, Case Sensitive", 'C', searchCaseSenseCB, window,
    	    GetPrefSearch() == SEARCH_CASE_SENSE, FULL);
    window->searchLiteralWordDefItem = createMenuToggle(subSubSubPane, "literalWord",
    	    "Literal, Whole Word", 'W', searchLiteralWordCB, window,
    	    GetPrefSearch() == SEARCH_LITERAL_WORD, FULL);
    window->searchCaseSenseWordDefItem = createMenuToggle(subSubSubPane,
    	    "caseSensitiveWord", "Literal, Case Sensitive, Whole Word", 't', searchCaseSenseWordCB, window,
    	    GetPrefSearch() == SEARCH_CASE_SENSE_WORD, FULL);
    window->searchRegexDefItem = createMenuToggle(subSubSubPane,
    	    "regularExpression", "Regular Expression", 'R', searchRegexCB,
    	    window, GetPrefSearch() == SEARCH_REGEX, FULL);
    window->searchRegexNoCaseDefItem = createMenuToggle(subSubSubPane,
    	    "regularExpressionNoCase", "Regular Expression, Case Insensitive", 'I', searchRegexNoCaseCB, window,
    	    GetPrefSearch() == SEARCH_REGEX_NOCASE, FULL);
#ifdef REPLACE_SCOPE
    subSubSubPane = createMenu(subSubPane, "defaultReplaceScope",
    	    "Default Replace Scope", 'R', NULL, FULL);
    XtVaSetValues(subSubSubPane, XmNradioBehavior, True, NULL); 
    window->replScopeWinDefItem = createMenuToggle(subSubSubPane, "window",
    	    "In Window", 'W', replaceScopeWindowCB, window,
    	    GetPrefReplaceDefScope() == REPL_DEF_SCOPE_WINDOW, FULL);
    window->replScopeSelDefItem = createMenuToggle(subSubSubPane, "selection",
    	    "In Selection", 'S', replaceScopeSelectionCB, window,
    	    GetPrefReplaceDefScope() == REPL_DEF_SCOPE_SELECTION, FULL);
    window->replScopeSmartDefItem = createMenuToggle(subSubSubPane, "window",
    	    "Smart", 'm', replaceScopeSmartCB, window,
    	    GetPrefReplaceDefScope() == REPL_DEF_SCOPE_SMART, FULL);
#endif

    /* Syntax Highlighting sub menu */
    subSubPane = createMenu(subPane, "syntaxHighlighting","Syntax Highlighting",
    	    'H', NULL, FULL);
    window->highlightOffDefItem = createMenuRadioToggle(subSubPane, "off","Off",
    	    'O', highlightOffDefCB, window, !GetPrefHighlightSyntax(), FULL);
    window->highlightDefItem = createMenuRadioToggle(subSubPane, "on",
    	    "On", 'n', highlightDefCB, window, GetPrefHighlightSyntax(), FULL);
    createMenuSeparator(subSubPane, "sep1", SHORT);
    createMenuItem(subSubPane, "recognitionPatterns", "Recognition Patterns...",
    	    'R', highlightingDefCB, window, FULL);
    createMenuItem(subSubPane, "textDrawingStyles", "Text Drawing Styles...", 'T',
    	    stylesDefCB, window, FULL);
    window->backlightCharsDefItem = createMenuToggle(subPane,
          "backlightChars", "Apply Backlighting", 'g', backlightCharsDefCB,
          window, GetPrefBacklightChars(), FULL);

    /* tabbed editing sub menu */
    subSubPane = createMenu(subPane, "tabbedEditMenu", "Tabbed Editing", 0,
    	    &cascade, SHORT);
    window->openInTabDefItem = createMenuToggle(subSubPane, "openAsTab",
    	    "Open File In New Tab", 'T', openInTabDefCB, window,
	    GetPrefOpenInTab(), FULL);
    window->tabBarDefItem = createMenuToggle(subSubPane, "showTabBar",
    	    "Show Tab Bar", 'B', tabBarDefCB, window,
	    GetPrefTabBar(), FULL);
    window->tabBarHideDefItem = createMenuToggle(subSubPane,
    	    "hideTabBar", "Hide Tab Bar When Only One Document is Open", 'H', 
	    tabBarHideDefCB, window, GetPrefTabBarHideOne(), FULL);
    window->tabNavigateDefItem = createMenuToggle(subSubPane, "tabNavigateDef",
    	    "Next/Prev Tabs Across Windows", 'W', tabNavigateDefCB, window,
	    GetPrefGlobalTabNavigate(), FULL);
    window->tabSortDefItem = createMenuToggle(subSubPane, "tabSortDef",
    	    "Sort Tabs Alphabetically", 'S', tabSortDefCB, window,
	    GetPrefSortTabs(), FULL);
    
    window->toolTipsDefItem = createMenuToggle(subPane, "showTooltips",
    	    "Show Tooltips", 0, toolTipsDefCB, window, GetPrefToolTips(),
	    FULL);
    window->statsLineDefItem = createMenuToggle(subPane, "statisticsLine",
    	    "Statistics Line", 'S', statsLineDefCB, window, GetPrefStatsLine(),
    	    SHORT);
    window->iSearchLineDefItem = createMenuToggle(subPane,
	    "incrementalSearchLine", "Incremental Search Line", 'i',
	    iSearchLineDefCB, window, GetPrefISearchLine(), FULL);
    window->lineNumsDefItem = createMenuToggle(subPane, "showLineNumbers",
    	    "Show Line Numbers", 'N', lineNumsDefCB, window, GetPrefLineNums(),
    	    SHORT);
    window->saveLastDefItem = createMenuToggle(subPane, "preserveLastVersion",
    	    "Make Backup Copy (*.bck)", 'e', preserveDefCB, window,
    	    GetPrefSaveOldVersion(), SHORT);
    window->autoSaveDefItem = createMenuToggle(subPane, "incrementalBackup",
    	    "Incremental Backup", 'B', autoSaveDefCB, window, GetPrefAutoSave(),
    	    SHORT);

    
    /* Show Matching sub menu */
    subSubPane = createMenu(subPane, "showMatching", "Show Matching (..)", 'M',
	    NULL, FULL);
    window->showMatchingOffDefItem = createMenuRadioToggle(subSubPane, "off",
	    "Off", 'O', showMatchingOffDefCB, window, 
            GetPrefShowMatching() == NO_FLASH, SHORT);
    window->showMatchingDelimitDefItem = createMenuRadioToggle(subSubPane,
	    "delimiter", "Delimiter", 'D', showMatchingDelimitDefCB, window,
	    GetPrefShowMatching() == FLASH_DELIMIT, SHORT);
    window->showMatchingRangeDefItem = createMenuRadioToggle(subSubPane,
	    "range", "Range", 'R', showMatchingRangeDefCB, window,
	    GetPrefShowMatching() == FLASH_RANGE, SHORT);
    createMenuSeparator(subSubPane, "sep", SHORT);
    window->matchSyntaxBasedDefItem = createMenuToggle(subSubPane, 
	   "matchSyntax", "Syntax Based", 'S', matchSyntaxBasedDefCB, window,
	    GetPrefMatchSyntaxBased(), SHORT);

    /* Append LF at end of files on save */
    window->appendLFItem = createMenuToggle(subPane, "appendLFItem",
            "Terminate with Line Break on Save", 'v', appendLFCB, NULL,
            GetPrefAppendLF(), FULL);

    window->reposDlogsDefItem = createMenuToggle(subPane, "popupsUnderPointer",
    	    "Popups Under Pointer", 'P', reposDlogsDefCB, window,
    	    GetPrefRepositionDialogs(), FULL);
    window->autoScrollDefItem = createMenuToggle(subPane, "autoScroll",
    	    "Auto Scroll Near Window Top/Bottom", 0, autoScrollDefCB, window,
    	    GetPrefAutoScroll(), FULL);
    subSubPane = createMenu(subPane, "warnings", "Warnings", 'r', NULL, FULL);
    window->modWarnDefItem = createMenuToggle(subSubPane,
	    "filesModifiedExternally", "Files Modified Externally", 'F',
	    modWarnDefCB, window, GetPrefWarnFileMods(), FULL);
    window->modWarnRealDefItem = createMenuToggle(subSubPane,
	    "checkModifiedFileContents", "Check Modified File Contents", 'C',
	    modWarnRealDefCB, window, GetPrefWarnRealFileMods(), FULL);
    XtSetSensitive(window->modWarnRealDefItem, GetPrefWarnFileMods());
    window->exitWarnDefItem = createMenuToggle(subSubPane, "onExit", "On Exit", 'O',
	    exitWarnDefCB, window, GetPrefWarnExit(), FULL);
    
    /* Initial Window Size sub menu (simulates radioBehavior) */
    subSubPane = createMenu(subPane, "initialwindowSize",
    	    "Initial Window Size", 'z', NULL, FULL);
    /* XtVaSetValues(subSubPane, XmNradioBehavior, True, NULL);  */
    window->size24x80DefItem = btn = createMenuToggle(subSubPane, "24X80",
    	    "24 x 80", '2', size24x80CB, window, False, SHORT);
    XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, NULL);
    window->size40x80DefItem = btn = createMenuToggle(subSubPane, "40X80",
    	    "40 x 80", '4', size40x80CB, window, False, SHORT);
    XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, NULL);
    window->size60x80DefItem = btn = createMenuToggle(subSubPane, "60X80",
    	    "60 x 80", '6', size60x80CB, window, False, SHORT);
    XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, NULL);
    window->size80x80DefItem = btn = createMenuToggle(subSubPane, "80X80",
    	    "80 x 80", '8', size80x80CB, window, False, SHORT);
    XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, NULL);
    window->sizeCustomDefItem = btn = createMenuToggle(subSubPane, "custom",
    	    "Custom...", 'C', sizeCustomCB, window, False, SHORT);
    XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, NULL);
    updateWindowSizeMenu(window);
    
    /*
    ** Remainder of Preferences menu
    */
    createMenuItem(menuPane, "saveDefaults", "Save Defaults...", 'v',
    	    savePrefCB, window, FULL);
#ifdef SGI_CUSTOM
    window->shortMenusDefItem = createMenuToggle(menuPane,
    	    "shortMenus", "Short Menus", 'h', shortMenusCB, window,
    	    GetPrefShortMenus(), SHORT);
#endif
    createMenuSeparator(menuPane, "sep1", SHORT);
    window->statsLineItem = createMenuToggle(menuPane, "statisticsLine", "Statistics Line", 'S',
    	    statsCB, window, GetPrefStatsLine(), SHORT);
    window->iSearchLineItem = createMenuToggle(menuPane, "incrementalSearchLine","Incremental Search Line",
	    'I', doActionCB, "set_incremental_search_line", GetPrefISearchLine(), FULL);
    window->lineNumsItem = createMenuToggle(menuPane, "lineNumbers", "Show Line Numbers", 'N',
    	    doActionCB, "set_show_line_numbers", GetPrefLineNums(), SHORT);
    CreateLanguageModeSubMenu(window, menuPane, "languageMode",
    	    "Language Mode", 'L');
    subPane = createMenu(menuPane, "autoIndent", "Auto Indent",
	    'A', NULL, FULL);
    window->autoIndentOffItem = createMenuRadioToggle(subPane, "off", "Off",
    	    'O', autoIndentOffCB, window, window->indentStyle == NO_AUTO_INDENT,
	    SHORT);
    window->autoIndentItem = createMenuRadioToggle(subPane, "on", "On", 'n',
    	    autoIndentCB, window, window->indentStyle == AUTO_INDENT, SHORT);
    window->smartIndentItem = createMenuRadioToggle(subPane, "smart", "Smart",
    	    'S', smartIndentCB, window, window->indentStyle == SMART_INDENT,
	    SHORT);
    subPane = createMenu(menuPane, "wrap", "Wrap",
	    'W', NULL, FULL);
    window->noWrapItem = createMenuRadioToggle(subPane, "none",
    	    "None", 'N', noWrapCB, window,
    	    window->wrapMode==NO_WRAP, SHORT);
    window->newlineWrapItem = createMenuRadioToggle(subPane, "autoNewlineWrap",
    	    "Auto Newline", 'A', newlineWrapCB, window,
    	    window->wrapMode==NEWLINE_WRAP, SHORT);
    window->continuousWrapItem = createMenuRadioToggle(subPane,
    	    "continuousWrap", "Continuous", 'C', continuousWrapCB, window,
    	    window->wrapMode==CONTINUOUS_WRAP, SHORT);
    createMenuSeparator(subPane, "sep1", SHORT);
    createMenuItem(subPane, "wrapMargin", "Wrap Margin...", 'W',
    	    wrapMarginCB, window, SHORT);
    createMenuItem(menuPane, "tabs", "Tab Stops...", 'T', tabsCB, window, SHORT);
    createMenuItem(menuPane, "textFont", "Text Fonts...", 'F', fontCB, window,
    	    FULL);
    window->highlightItem = createMenuToggle(menuPane, "highlightSyntax",
	    "Highlight Syntax", 'H', doActionCB, "set_highlight_syntax",
	    GetPrefHighlightSyntax(), SHORT);
    window->backlightCharsItem = createMenuToggle(menuPane, "backlightChars",
          "Apply Backlighting", 'g', backlightCharsCB, window,
          window->backlightChars, FULL);
#ifndef VMS
    window->saveLastItem = createMenuToggle(menuPane, "makeBackupCopy",
    	    "Make Backup Copy (*.bck)", 'e', preserveCB, window,
    	    window->saveOldVersion, SHORT);
#endif
    window->autoSaveItem = createMenuToggle(menuPane, "incrementalBackup",
    	    "Incremental Backup", 'B', autoSaveCB, window, window->autoSave,
    	    SHORT);

    subPane = createMenu(menuPane, "showMatching", "Show Matching (..)",
        'M', NULL, FULL);
    window->showMatchingOffItem = createMenuRadioToggle(subPane, "off", "Off",
        'O', showMatchingOffCB, window, window->showMatchingStyle == NO_FLASH, 
        SHORT);
    window->showMatchingDelimitItem = createMenuRadioToggle(subPane,
	"delimiter", "Delimiter", 'D', showMatchingDelimitCB, window,
        window->showMatchingStyle == FLASH_DELIMIT, SHORT);
    window->showMatchingRangeItem = createMenuRadioToggle(subPane, "range", 
	"Range", 'R', showMatchingRangeCB, window, 
	window->showMatchingStyle == FLASH_RANGE, SHORT);
    createMenuSeparator(subPane, "sep", SHORT);
    window->matchSyntaxBasedItem = createMenuToggle(subPane, "matchSyntax",
	    "Syntax Based", 'S', matchSyntaxBasedCB, window,
	    window->matchSyntaxBased, SHORT);

#ifndef SGI_CUSTOM
    createMenuSeparator(menuPane, "sep2", SHORT);
    window->overtypeModeItem = createMenuToggle(menuPane, "overtype", "Overtype", 'O',
    	    doActionCB, "set_overtype_mode", False, SHORT);
    window->readOnlyItem = createMenuToggle(menuPane, "readOnly", "Read Only",
    	    'y', doActionCB, "set_locked", IS_USER_LOCKED(window->lockReasons), FULL);
#endif

#ifndef VMS
    /*
    ** Create the Shell menu
    */
    menuPane = window->shellMenuPane =
    	    createMenu(menuBar, "shellMenu", "Shell", 0, &cascade, FULL);
    btn = createMenuItem(menuPane, "executeCommand", "Execute Command...",
    	    'E', doActionCB, "execute_command_dialog", SHORT);
    XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, NULL);
    btn = createMenuItem(menuPane, "executeCommandLine", "Execute Command Line",
    	    'x', doActionCB, "execute_command_line", SHORT);
    XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, NULL);
    window->filterItem = createMenuItem(menuPane, "filterSelection",
    	    "Filter Selection...", 'F', doActionCB, "filter_selection_dialog",
    	    SHORT);
    XtVaSetValues(window->filterItem, XmNuserData, PERMANENT_MENU_ITEM,
    	    XmNsensitive, window->wasSelected, NULL);
    window->cancelShellItem = createMenuItem(menuPane, "cancelShellCommand",
    	    "Cancel Shell Command", 'C', cancelShellCB, window, SHORT);
    XtVaSetValues(window->cancelShellItem, XmNuserData, PERMANENT_MENU_ITEM,
    	    XmNsensitive, False, NULL);
    btn = createMenuSeparator(menuPane, "sep1", SHORT);
    XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, NULL);
#endif

    /*
    ** Create the Macro menu
    */
    menuPane = window->macroMenuPane =
    	    createMenu(menuBar, "macroMenu", "Macro", 0, &cascade, FULL);
    window->learnItem = createMenuItem(menuPane, "learnKeystrokes",
    	    "Learn Keystrokes", 'L', learnCB, window, SHORT);
    XtVaSetValues(window->learnItem , XmNuserData, PERMANENT_MENU_ITEM, NULL);
    window->finishLearnItem = createMenuItem(menuPane, "finishLearn",
    	    "Finish Learn", 'F', finishLearnCB, window, SHORT);
    XtVaSetValues(window->finishLearnItem , XmNuserData, PERMANENT_MENU_ITEM,
    	    XmNsensitive, False, NULL);
    window->cancelMacroItem = createMenuItem(menuPane, "cancelLearn",
    	    "Cancel Learn", 'C', cancelLearnCB, window, SHORT);
    XtVaSetValues(window->cancelMacroItem, XmNuserData, PERMANENT_MENU_ITEM,
    	    XmNsensitive, False, NULL);
    window->replayItem = createMenuItem(menuPane, "replayKeystrokes",
    	    "Replay Keystrokes", 'K', replayCB, window, SHORT);
    XtVaSetValues(window->replayItem, XmNuserData, PERMANENT_MENU_ITEM,
    	    XmNsensitive, GetReplayMacro() != NULL, NULL);
    window->repeatItem = createMenuItem(menuPane, "repeat",
    	    "Repeat...", 'R', doActionCB, "repeat_dialog", SHORT);
    XtVaSetValues(window->repeatItem, XmNuserData, PERMANENT_MENU_ITEM, NULL);
    btn = createMenuSeparator(menuPane, "sep1", SHORT);
    XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, NULL);

    /*
    ** Create the Windows menu
    */
    menuPane = window->windowMenuPane = createMenu(menuBar, "windowsMenu",
    	    "Windows", 0, &cascade, FULL);
    XtAddCallback(cascade, XmNcascadingCallback, (XtCallbackProc)windowMenuCB,
    	    window);
    window->splitPaneItem = createMenuItem(menuPane, "splitPane",
    	    "Split Pane", 'S', doActionCB, "split_pane", SHORT);
    XtVaSetValues(window->splitPaneItem, XmNuserData, PERMANENT_MENU_ITEM,
	    NULL);
    window->closePaneItem = createMenuItem(menuPane, "closePane",
    	    "Close Pane", 'C', doActionCB, "close_pane", SHORT);
    XtVaSetValues(window->closePaneItem, XmNuserData, PERMANENT_MENU_ITEM,NULL);
    XtSetSensitive(window->closePaneItem, False);

    btn = createMenuSeparator(menuPane, "sep01", SHORT);
    XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, NULL);
    window->detachDocumentItem = createMenuItem(menuPane, "detachBuffer",
    	    "Detach Tab", 'D', doActionCB, "detach_document", SHORT);
    XtSetSensitive(window->detachDocumentItem, False);

    window->moveDocumentItem = createMenuItem(menuPane, "moveDocument",
    	    "Move Tab To...", 'M', doActionCB, "move_document_dialog", SHORT);
    XtSetSensitive(window->moveDocumentItem, False);
    btn = createMenuSeparator(menuPane, "sep1", SHORT);
    XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, NULL);
    
    /* 
    ** Create "Help" pull down menu.
    */
    menuPane = createMenu(menuBar, "helpMenu", "Help", 0, &cascade, SHORT);
    XtVaSetValues(menuBar, XmNmenuHelpWidget, cascade, NULL);
    buildHelpMenu( menuPane, &H_M[0], window );

    return menuBar;
}

/*----------------------------------------------------------------------------*/

static Widget makeHelpMenuItem(

    Widget           parent,
    char            *name,      /* to be assigned to the child widget */
    char            *label,     /* text to be displayed in menu       */
    char             mnemonic,  /* letter in label to be underlined   */
    menuCallbackProc callback,  /* activated when menu item selected  */
    void            *cbArg,     /* passed to activated call back      */
    int              mode,      /* SGI_CUSTOM menu option             */
    enum HelpTopic   topic      /* associated with this menu item     */
)
{
    Widget menuItem = 
        createMenuItem( parent, name, label, mnemonic, callback, cbArg, mode );
    
    XtVaSetValues( menuItem, XmNuserData, (XtPointer)topic, NULL );
    return menuItem;
}

/*----------------------------------------------------------------------------*/

static void helpCB( Widget menuItem, XtPointer clientData, XtPointer callData )
{
    enum HelpTopic topic;
    XtPointer userData;
    
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(menuItem))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtVaGetValues( menuItem, XmNuserData, &userData, NULL );
    topic = (enum HelpTopic)userData;
    
    Help(topic);
}

/*----------------------------------------------------------------------------*/

#define NON_MENU_HELP 9

static HelpMenu * buildHelpMenu( 

    Widget       pane,  /* Menu pane on which to place new menu items */
    HelpMenu   * menu,  /* Data to drive building the help menu       */
    WindowInfo * window /* Main NEdit window information              */
)
{
#ifdef VMS
    int hideIt = 1;  /* All menu items matching this will be inaccessible */
#else
    int hideIt = -1; /* This value should make all menu items accessible  */
#endif

    if( menu != NULL )
    {
        int crntLevel = menu->level;

        /*-------------------------
        * For each menu element ...
        *-------------------------*/
        while( menu != NULL && menu->level == crntLevel )
        {
            /*----------------------------------------------
            * ... see if dealing with a separator or submenu
            *----------------------------------------------*/
            if( menu->topic == HELP_none )
            {
                if( menu->mnemonic == '-' )
                {
                    createMenuSeparator(pane, menu->wgtName, SHORT);
                    menu = menu->next;
                }
                else
                {
                    /*-------------------------------------------------------
                    * Do not show any of the submenu when it is to be hidden.
                    *-------------------------------------------------------*/
                    if( menu->hideIt == hideIt || menu->hideIt == NON_MENU_HELP )
                    {
                        do {     menu = menu->next;
                        } while( menu != NULL && menu->level > crntLevel );
                        
                    }
                    else
                    {
                        Widget subPane = 
                            createMenu( pane, menu->wgtName, menu->subTitle, 
                               menu->mnemonic, NULL, FULL);

                        menu = buildHelpMenu( subPane, menu->next, window );
                    }
                }
            }

            else
            {
                /*---------------------------------------
                * Show menu item if not going to hide it.
                * This is the easy way out of hiding
                * menu items. When entire submenus want
                * to be hidden, either the entire branch
                * will have to be marked, or this algorithm
                * will have to become a lot smarter.
                *---------------------------------------*/
                if( menu->hideIt != hideIt && menu->hideIt != NON_MENU_HELP )
                    makeHelpMenuItem( 
                        pane, menu->wgtName, HelpTitles[menu->topic], 
                        menu->mnemonic, helpCB, window, SHORT, menu->topic );

                menu = menu->next;
            }
        }
    }
    
    return menu;
}

/*----------------------------------------------------------------------------*/

/*
** handle actions called from the context menus of tabs.
*/
static void doTabActionCB(Widget w, XtPointer clientData, XtPointer callData)
{
    Widget menu = MENU_WIDGET(w);
    WindowInfo *win, *window = WidgetToWindow(menu);
    
    /* extract the window to be acted upon, see comment in
       tabMenuPostAP() for detail */
    XtVaGetValues(window->tabMenuPane, XmNuserData, &win, NULL);
    
    HidePointerOnKeyedEvent(win->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(win->lastFocus, (char *)clientData,
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void doActionCB(Widget w, XtPointer clientData, XtPointer callData)
{
    Widget menu = MENU_WIDGET(w);
    Widget widget = WidgetToWindow(menu)->lastFocus;
    String action = (String) clientData;
    XEvent* event = ((XmAnyCallbackStruct*) callData)->event;

    HidePointerOnKeyedEvent(widget, event);

    XtCallActionProc(widget, action, event, NULL, 0);
}

static void pasteColCB(Widget w, XtPointer clientData, XtPointer callData) 
{
    static char *params[1] = {"rect"};
    
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "paste_clipboard",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void shiftLeftCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
    	    ((XmAnyCallbackStruct *)callData)->event->xbutton.state & ShiftMask
    	    ? "shift_left_by_tab" : "shift_left",
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void shiftRightCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
    	    ((XmAnyCallbackStruct *)callData)->event->xbutton.state & ShiftMask
    	    ? "shift_right_by_tab" : "shift_right",
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void findCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "find_dialog",
    	    ((XmAnyCallbackStruct *)callData)->event,
    	    shiftKeyToDir(callData), 1);
}

static void findSameCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
     XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "find_again",
    	    ((XmAnyCallbackStruct *)callData)->event,
    	    shiftKeyToDir(callData), 1);
}

static void findSelCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "find_selection",
    	    ((XmAnyCallbackStruct *)callData)->event, 
    	    shiftKeyToDir(callData), 1);
}

static void findIncrCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
	    "start_incremental_find", ((XmAnyCallbackStruct *)callData)->event, 
    	    shiftKeyToDir(callData), 1);
}

static void replaceCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "replace_dialog",
    	    ((XmAnyCallbackStruct *)callData)->event,
    	    shiftKeyToDir(callData), 1);
}

static void replaceSameCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "replace_again",
    	    ((XmAnyCallbackStruct *)callData)->event,
    	    shiftKeyToDir(callData), 1);
}

static void replaceFindSameCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "replace_find_same",
    	    ((XmAnyCallbackStruct *)callData)->event,
    	    shiftKeyToDir(callData), 1);
}

static void markCB(Widget w, XtPointer clientData, XtPointer callData)
{
    XEvent *event = ((XmAnyCallbackStruct *)callData)->event;
    WindowInfo *window = WidgetToWindow(MENU_WIDGET(w));
    
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    if (event->type == KeyPress || event->type == KeyRelease)
    	BeginMarkCommand(window);
    else
    	XtCallActionProc(window->lastFocus, "mark_dialog", event, NULL, 0);
}

static void gotoMarkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    XEvent *event = ((XmAnyCallbackStruct *)callData)->event;
    WindowInfo *window = WidgetToWindow(MENU_WIDGET(w));
    int extend = event->xbutton.state & ShiftMask;
    static char *params[1] = {"extend"};
    
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    if (event->type == KeyPress || event->type == KeyRelease)
    	BeginGotoMarkCommand(window, extend);
    else
    	XtCallActionProc(window->lastFocus, "goto_mark_dialog", event, params,
		extend ? 1 : 0);
}

static void gotoMatchingCB(Widget w, XtPointer clientData, XtPointer callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
    	    ((XmAnyCallbackStruct *)callData)->event->xbutton.state & ShiftMask
    	    ? "select_to_matching" : "goto_matching",
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void autoIndentOffCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {"off"};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Auto Indent Off")) {
	autoIndentOffDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_auto_indent",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void autoIndentCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {"on"};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Auto Indent")) {
	autoIndentDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_auto_indent",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void smartIndentCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {"smart"};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Smart Indent")) {
	smartIndentDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_auto_indent",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void autoSaveCB(Widget w, WindowInfo *window, caddr_t callData)
{
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Incremental Backup")) {
	autoSaveDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_incremental_backup",
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void preserveCB(Widget w, WindowInfo *window, caddr_t callData)
{
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Make Backup Copy")) {
        preserveDefCB(w, window, callData);
        SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_make_backup_copy",
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void showMatchingOffCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {NO_FLASH_STRING};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Show Matching Off")) {
	showMatchingOffDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_show_matching",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void showMatchingDelimitCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {FLASH_DELIMIT_STRING};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Show Matching Delimiter")) {
	showMatchingDelimitDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_show_matching",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void showMatchingRangeCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {FLASH_RANGE_STRING};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Show Matching Range")) {
	showMatchingRangeDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_show_matching",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void matchSyntaxBasedCB(Widget w, WindowInfo *window, caddr_t callData)
{
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Match Syntax Based")) {
	matchSyntaxBasedDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_match_syntax_based",
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void fontCB(Widget w, WindowInfo *window, caddr_t callData)
{
    ChooseFonts(WidgetToWindow(MENU_WIDGET(w)), True);
}

static void noWrapCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {"none"};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "No Wrap")) {
	noWrapDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_wrap_text",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void newlineWrapCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {"auto"};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Auto Newline Wrap")) {
	newlineWrapDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_wrap_text",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void continuousWrapCB(Widget w, WindowInfo *window, caddr_t callData)
{
    static char *params[1] = {"continuous"};
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Continuous Wrap")) {
    	contWrapDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(menu)->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_wrap_text",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void wrapMarginCB(Widget w, WindowInfo *window, caddr_t callData)
{
    window = WidgetToWindow(MENU_WIDGET(w));

    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    WrapMarginDialog(window->shell, window);
}

static void backlightCharsCB(Widget w, WindowInfo *window, caddr_t callData)
{
    int applyBacklight = XmToggleButtonGetState(w);
    window = WidgetToWindow(MENU_WIDGET(w));
    SetBacklightChars(window, applyBacklight?GetPrefBacklightCharTypes():NULL);
}

static void tabsCB(Widget w, WindowInfo *window, caddr_t callData)
{
    window = WidgetToWindow(MENU_WIDGET(w));

    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    TabsPrefDialog(window->shell, window);
}

static void statsCB(Widget w, WindowInfo *window, caddr_t callData)
{
    Widget menu = MENU_WIDGET(w);

    window = WidgetToWindow(menu);

#ifdef SGI_CUSTOM
    if (shortPrefAskDefault(window->shell, w, "Statistics Line")) {
	statsLineDefCB(w, window, callData);
	SaveNEditPrefs(window->shell, GetPrefShortMenus());
    }
#endif
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "set_statistics_line",
    	    ((XmAnyCallbackStruct *)callData)->event, NULL, 0);
}

static void autoIndentOffDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefAutoIndent(NO_AUTO_INDENT);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->autoIndentOffDefItem, True, False);
    	XmToggleButtonSetState(win->autoIndentDefItem, False, False);
    	XmToggleButtonSetState(win->smartIndentDefItem, False, False);
    }
}

static void autoIndentDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefAutoIndent(AUTO_INDENT);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->autoIndentDefItem, True, False);
	XmToggleButtonSetState(win->autoIndentOffDefItem, False, False);
	XmToggleButtonSetState(win->smartIndentDefItem, False, False);
    }
}

static void smartIndentDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefAutoIndent(SMART_INDENT);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->smartIndentDefItem, True, False);
	XmToggleButtonSetState(win->autoIndentOffDefItem, False, False);
    	XmToggleButtonSetState(win->autoIndentDefItem, False, False);
    }
}

static void autoSaveDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefAutoSave(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->autoSaveDefItem, state, False);
    }
}

static void preserveDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefSaveOldVersion(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->saveLastDefItem, state, False);
    }
}

static void fontDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    ChooseFonts(WidgetToWindow(MENU_WIDGET(w)), False);
}

static void colorDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    ChooseColors(WidgetToWindow(MENU_WIDGET(w)));
}

static void noWrapDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefWrap(NO_WRAP);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->noWrapDefItem, True, False);
    	XmToggleButtonSetState(win->newlineWrapDefItem, False, False);
    	XmToggleButtonSetState(win->contWrapDefItem, False, False);
    }
}

static void newlineWrapDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefWrap(NEWLINE_WRAP);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->newlineWrapDefItem, True, False);
    	XmToggleButtonSetState(win->contWrapDefItem, False, False);
    	XmToggleButtonSetState(win->noWrapDefItem, False, False);
    }
}

static void contWrapDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefWrap(CONTINUOUS_WRAP);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->contWrapDefItem, True, False);
    	XmToggleButtonSetState(win->newlineWrapDefItem, False, False);
    	XmToggleButtonSetState(win->noWrapDefItem, False, False);
    }
}

static void wrapMarginDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    WrapMarginDialog(WidgetToWindow(MENU_WIDGET(w))->shell, NULL);
}

static void smartTagsDefCB(Widget w, XtPointer client_data, XtPointer callData)
{
    WindowInfo *win;
	
    SetPrefSmartTags(True);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XmToggleButtonSetState(win->smartTagsDefItem, True, False);
	XmToggleButtonSetState(win->allTagsDefItem, False, False);
    }
}

static void showAllTagsDefCB(Widget w, XtPointer client_data, XtPointer callData)
{
    WindowInfo *win;
	
    SetPrefSmartTags(False);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XmToggleButtonSetState(win->smartTagsDefItem, False, False);
	XmToggleButtonSetState(win->allTagsDefItem, True, False);
    }
}

static void shellSelDefCB(Widget widget, WindowInfo* window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(widget))->lastFocus,
            ((XmAnyCallbackStruct*) callData)->event);
    SelectShellDialog(WidgetToWindow(MENU_WIDGET(widget))->shell, NULL);
}

static void tabsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    TabsPrefDialog(WidgetToWindow(MENU_WIDGET(w))->shell, NULL);
}

static void showMatchingOffDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefShowMatching(NO_FLASH);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XmToggleButtonSetState(win->showMatchingOffDefItem, True, False);
	XmToggleButtonSetState(win->showMatchingDelimitDefItem, False, False);
	XmToggleButtonSetState(win->showMatchingRangeDefItem, False, False);
    }
}

static void showMatchingDelimitDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefShowMatching(FLASH_DELIMIT);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XmToggleButtonSetState(win->showMatchingOffDefItem, False, False);
	XmToggleButtonSetState(win->showMatchingDelimitDefItem, True, False);
	XmToggleButtonSetState(win->showMatchingRangeDefItem, False, False);
    }
}

static void showMatchingRangeDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefShowMatching(FLASH_RANGE);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XmToggleButtonSetState(win->showMatchingOffDefItem, False, False);
	XmToggleButtonSetState(win->showMatchingDelimitDefItem, False, False);
	XmToggleButtonSetState(win->showMatchingRangeDefItem, True, False);
    }
}

static void matchSyntaxBasedDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefMatchSyntaxBased(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
	    XmToggleButtonSetState(win->matchSyntaxBasedDefItem, state, False);
    }
}

static void backlightCharsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefBacklightChars(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
	    XmToggleButtonSetState(win->backlightCharsDefItem, state, False);
    }
}

static void highlightOffDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefHighlightSyntax(False);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->highlightOffDefItem, True, False);
    	XmToggleButtonSetState(win->highlightDefItem, False, False);
    }
}

static void highlightDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    SetPrefHighlightSyntax(True);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->highlightOffDefItem, False, False);
    	XmToggleButtonSetState(win->highlightDefItem, True, False);
    }
}

static void highlightingDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditHighlightPatterns(WidgetToWindow(MENU_WIDGET(w)));
}

static void smartMacrosDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditSmartIndentMacros(WidgetToWindow(MENU_WIDGET(w)));
}

static void stylesDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditHighlightStyles(NULL);
}

static void languageDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditLanguageModes();
}

#ifndef VMS
static void shellDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditShellMenu(WidgetToWindow(MENU_WIDGET(w)));
}
#endif /* VMS */

static void macroDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditMacroMenu(WidgetToWindow(MENU_WIDGET(w)));
}

static void bgMenuDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditBGMenu(WidgetToWindow(MENU_WIDGET(w)));
}

static void customizeTitleDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    window = WidgetToWindow(MENU_WIDGET(w));

    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    EditCustomTitleFormat(window);
}

static void searchDlogsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefSearchDlogs(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->searchDlogsDefItem, state, False);
    }
}

static void beepOnSearchWrapDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefBeepOnSearchWrap(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->beepOnSearchWrapDefItem, state, False);
    }
}

static void keepSearchDlogsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefKeepSearchDlogs(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->keepSearchDlogsDefItem, state, False);
    }
}

static void searchWrapsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefSearchWraps(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->searchWrapsDefItem, state, False);
    }
}

static void appendLFCB(Widget w, WindowInfo* window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    SetPrefAppendLF(state);
    for (win = WindowList; win != NULL; win = win->next) {
    	if (IsTopDocument(win))
            XmToggleButtonSetState(win->appendLFItem, state, False);
    }
}

static void sortOpenPrevDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference, make the other windows' menus agree,
       and invalidate their Open Previous menus */
    SetPrefSortOpenPrevMenu(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->sortOpenPrevDefItem, state, False);
    }
}

static void reposDlogsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefRepositionDialogs(state);
    SetPointerCenteredDialogs(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->reposDlogsDefItem, state, False);
    }
}

static void autoScrollDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefAutoScroll(state);
    /* XXX: Should we ensure auto-scroll now if needed? */
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->autoScrollDefItem, state, False);
    }
}

static void modWarnDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefWarnFileMods(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->modWarnDefItem, state, False);
	XtSetSensitive(win->modWarnRealDefItem, state);
    }
}

static void modWarnRealDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefWarnRealFileMods(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->modWarnRealDefItem, state, False);
    }
}

static void exitWarnDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefWarnExit(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->exitWarnDefItem, state, False);
    }
}

static void openInTabDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefOpenInTab(state);
    for (win=WindowList; win!=NULL; win=win->next)
    	XmToggleButtonSetState(win->openInTabDefItem, state, False);
}

static void tabBarDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefTabBar(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->tabBarDefItem, state, False);
    	ShowWindowTabBar(win);
    }
}

static void tabBarHideDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefTabBarHideOne(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XmToggleButtonSetState(win->tabBarHideDefItem, state, False);
    	ShowWindowTabBar(win);
    }
}

static void toolTipsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefToolTips(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	XtVaSetValues(win->tab, XltNshowBubble, GetPrefToolTips(), NULL);
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->toolTipsDefItem, state, False);
    }
}

static void tabNavigateDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefGlobalTabNavigate(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->tabNavigateDefItem, state, False);
    }
}

static void tabSortDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefSortTabs(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->tabSortDefItem, state, False);
    }
    
    /* If we just enabled sorting, sort all tabs.  Note that this reorders
       the next pointers underneath us, which is scary, but SortTabBar never
       touches windows that are earlier in the WindowList so it's ok. */
    if (state) {
        Widget shell=(Widget)0;
        for (win=WindowList; win!=NULL; win=win->next) {
            if ( win->shell != shell ) {
                SortTabBar(win);
                shell = win->shell;
            }
        }
    }
}

static void statsLineDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefStatsLine(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->statsLineDefItem, state, False);
    }
}

static void iSearchLineDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefISearchLine(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->iSearchLineDefItem, state, False);
    }
}

static void lineNumsDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefLineNums(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->lineNumsDefItem, state, False);
    }
}

static void pathInWindowsMenuDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int state = XmToggleButtonGetState(w);

    /* Set the preference and make the other windows' menus agree */
    SetPrefShowPathInWindowsMenu(state);
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (IsTopDocument(win))
    	    XmToggleButtonSetState(win->pathInWindowsMenuDefItem, state, False);
    }
    InvalidateWindowMenus(); 
}

static void searchLiteralCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefSearch(SEARCH_LITERAL);
    	for (win=WindowList; win!=NULL; win=win->next){
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->searchLiteralDefItem, True, False);
    	    XmToggleButtonSetState(win->searchCaseSenseDefItem, False, False);
    	    XmToggleButtonSetState(win->searchLiteralWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchRegexDefItem, False, False);
	    XmToggleButtonSetState(win->searchRegexNoCaseDefItem, False, False);
    	}
    }
}

static void searchCaseSenseCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefSearch(SEARCH_CASE_SENSE);
    	for (win=WindowList; win!=NULL; win=win->next) {
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->searchLiteralDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseDefItem, True, False);
    	    XmToggleButtonSetState(win->searchLiteralWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchRegexDefItem, False, False);
	    XmToggleButtonSetState(win->searchRegexNoCaseDefItem, False, False);
    	}
    }
}

static void searchLiteralWordCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefSearch(SEARCH_LITERAL_WORD);
    	for (win=WindowList; win!=NULL; win=win->next){
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->searchLiteralDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseDefItem, False, False);
    	    XmToggleButtonSetState(win->searchLiteralWordDefItem, True, False);
    	    XmToggleButtonSetState(win->searchCaseSenseWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchRegexDefItem, False, False);
	    XmToggleButtonSetState(win->searchRegexNoCaseDefItem, False, False);
    	}
    }
}

static void searchCaseSenseWordCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefSearch(SEARCH_CASE_SENSE_WORD);
    	for (win=WindowList; win!=NULL; win=win->next) {
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->searchLiteralDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseDefItem, False, False);
    	    XmToggleButtonSetState(win->searchLiteralWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseWordDefItem, True, False);
    	    XmToggleButtonSetState(win->searchRegexDefItem, False, False);
	    XmToggleButtonSetState(win->searchRegexNoCaseDefItem, False, False);
    	}
    }
}

static void searchRegexCB(Widget w, WindowInfo *window, caddr_t callData)
{
   WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefSearch(SEARCH_REGEX);
    	for (win=WindowList; win!=NULL; win=win->next){
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->searchLiteralDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseDefItem, False, False);
    	    XmToggleButtonSetState(win->searchLiteralWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchRegexDefItem, True, False);
	    XmToggleButtonSetState(win->searchRegexNoCaseDefItem, False, False);
    	}
    }
}

static void searchRegexNoCaseCB(Widget w, WindowInfo *window, caddr_t callData)
{
   WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefSearch(SEARCH_REGEX_NOCASE);
    	for (win=WindowList; win!=NULL; win=win->next){
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->searchLiteralDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseDefItem, False, False);
    	    XmToggleButtonSetState(win->searchLiteralWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchCaseSenseWordDefItem, False, False);
    	    XmToggleButtonSetState(win->searchRegexDefItem, False, False);
	    XmToggleButtonSetState(win->searchRegexNoCaseDefItem, True, False);
    	}
    }
}

#ifdef REPLACE_SCOPE
static void replaceScopeWindowCB(Widget w, WindowInfo *window, caddr_t callData)
{
   WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefReplaceDefScope(REPL_DEF_SCOPE_WINDOW);
    	for (win=WindowList; win!=NULL; win=win->next){
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->replScopeWinDefItem, True, False);
    	    XmToggleButtonSetState(win->replScopeSelDefItem, False, False);
    	    XmToggleButtonSetState(win->replScopeSmartDefItem, False, False);
    	}
    }
}

static void replaceScopeSelectionCB(Widget w, WindowInfo *window, caddr_t callData)
{
   WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefReplaceDefScope(REPL_DEF_SCOPE_SELECTION);
    	for (win=WindowList; win!=NULL; win=win->next){
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->replScopeWinDefItem, False, False);
    	    XmToggleButtonSetState(win->replScopeSelDefItem, True, False);
    	    XmToggleButtonSetState(win->replScopeSmartDefItem, False, False);
    	}
    }
}

static void replaceScopeSmartCB(Widget w, WindowInfo *window, caddr_t callData)
{
   WindowInfo *win;

    /* Set the preference and make the other windows' menus agree */
    if (XmToggleButtonGetState(w)) {
    	SetPrefReplaceDefScope(REPL_DEF_SCOPE_SMART);
    	for (win=WindowList; win!=NULL; win=win->next){
    	    if (!IsTopDocument(win))
		continue;
    	    XmToggleButtonSetState(win->replScopeWinDefItem, False, False);
    	    XmToggleButtonSetState(win->replScopeSelDefItem, False, False);
    	    XmToggleButtonSetState(win->replScopeSmartDefItem, True, False);
    	}
    }
}
#endif

static void size24x80CB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    setWindowSizeDefault(24, 80);
}

static void size40x80CB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    setWindowSizeDefault(40, 80);
}

static void size60x80CB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    setWindowSizeDefault(60, 80);
}

static void size80x80CB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    setWindowSizeDefault(80, 80);
}

static void sizeCustomCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    RowColumnPrefDialog(WidgetToWindow(MENU_WIDGET(w))->shell);
    updateWindowSizeMenus();
}

static void savePrefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    SaveNEditPrefs(WidgetToWindow(MENU_WIDGET(w))->shell, False);
}

static void formFeedCB(Widget w, XtPointer clientData, XtPointer callData)
{
    static char *params[1] = {"\f"};
    
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    XtCallActionProc(WidgetToWindow(MENU_WIDGET(w))->lastFocus, "insert_string",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void cancelShellCB(Widget w, WindowInfo *window, XtPointer callData)
{
#ifndef VMS
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    AbortShellCommand(WidgetToWindow(MENU_WIDGET(w)));
#endif
}

static void learnCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    BeginLearn(WidgetToWindow(MENU_WIDGET(w)));
}

static void finishLearnCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    FinishLearn();
}

static void cancelLearnCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    CancelMacroOrLearn(WidgetToWindow(MENU_WIDGET(w)));
}

static void replayCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    Replay(WidgetToWindow(MENU_WIDGET(w)));
}

static void windowMenuCB(Widget w, WindowInfo *window, caddr_t callData)
{
    window = WidgetToWindow(MENU_WIDGET(w));
    
    if (!window->windowMenuValid) {
    	updateWindowMenu(window);
    	window->windowMenuValid = True;
    }
}

static void prevOpenMenuCB(Widget w, WindowInfo *window, caddr_t callData)
{
    window = WidgetToWindow(MENU_WIDGET(w));

    updatePrevOpenMenu(window);
}

static void unloadTagsFileMenuCB(Widget w, WindowInfo *window, caddr_t callData)
{
    updateTagsFileMenu(WidgetToWindow(MENU_WIDGET(w)));
}

static void unloadTipsFileMenuCB(Widget w, WindowInfo *window, caddr_t callData)
{
    updateTipsFileMenu(WidgetToWindow(MENU_WIDGET(w)));
}

/*
** open a new tab or window.
*/
static void newAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    int openInTab = GetPrefOpenInTab();

    if (*nArgs > 0) {
        if (strcmp(args[0], "prefs") == 0) {
            /* accept default */;
        }
        else if (strcmp(args[0], "tab") == 0) {
            openInTab = 1;
        }
        else if (strcmp(args[0], "window") == 0) {
            openInTab = 0;
        }
        else if (strcmp(args[0], "opposite") == 0) {
            openInTab = !openInTab;
        }
        else {
            fprintf(stderr, "nedit: Unknown argument to action procedure \"new\": %s\n", args[0]);
        }
    }
    
    EditNewFile(openInTab? window : NULL, NULL, False, NULL, window->path);
    CheckCloseDim();
}

/*
** These are just here because our techniques make it hard to bind a menu item
** to an action procedure that takes arguments.  The user doesn't need to know
** about them -- they can use new( "opposite" ) or new( "tab" ).
*/
static void newOppositeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);

    EditNewFile(GetPrefOpenInTab()? NULL : window, NULL, False, NULL,
            window->path);
    CheckCloseDim();
}
static void newTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);

    EditNewFile(window, NULL, False, NULL, window->path);
    CheckCloseDim();
}

static void openDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    char fullname[MAXPATHLEN], *params[2];
    int response;
    int n=1;
    
    response = PromptForExistingFile(window, "Open File", fullname);
    if (response != GFN_OK)
    	return;
    params[0] = fullname;

    if (*nArgs>0 && !strcmp(args[0], "1"))
        params[n++] = "1";
    
    XtCallActionProc(window->lastFocus, "open", event, params, n);
    CheckCloseDim();
}

static void openAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    char filename[MAXPATHLEN], pathname[MAXPATHLEN];
    
    if (*nArgs == 0) {
    	fprintf(stderr, "nedit: open action requires file argument\n");
    	return;
    }
    if (0 != ParseFilename(args[0], filename, pathname)
            || strlen(filename) + strlen(pathname) > MAXPATHLEN - 1) {
        fprintf(stderr, "nedit: invalid file name for open action: %s\n",
                args[0]);
        return;
    }
    EditExistingFile(window, filename, pathname, 0, NULL, False, 
            NULL, GetPrefOpenInTab(), False);
    CheckCloseDim();
}

static void openSelectedAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    OpenSelectedFile(WidgetToWindow(w), event->xbutton.time);
    CheckCloseDim();
}

static void closeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    int preResponse = PROMPT_SBC_DIALOG_RESPONSE;
    
    if (*nArgs > 0) {
        if (strcmp(args[0], "prompt") == 0) {
            preResponse = PROMPT_SBC_DIALOG_RESPONSE;
        }
        else if (strcmp(args[0], "save") == 0) {
            preResponse = YES_SBC_DIALOG_RESPONSE;
        }
        else if (strcmp(args[0], "nosave") == 0) {
            preResponse = NO_SBC_DIALOG_RESPONSE;
        }
    }
    CloseFileAndWindow(WidgetToWindow(w), preResponse);
    CheckCloseDim();
}

static void saveAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);

    if (CheckReadOnly(window))
    	return;
    SaveWindow(window);
}

static void saveAsDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    int response, addWrap, fileFormat;
    char fullname[MAXPATHLEN], *params[2];
    
    response = PromptForNewFile(window, "Save File As", fullname,
	    &fileFormat, &addWrap);
    if (response != GFN_OK)
    	return;
    window->fileFormat = fileFormat;
    params[0] = fullname;
    params[1] = "wrapped";
    XtCallActionProc(window->lastFocus, "save_as", event, params, addWrap?2:1);
}

static void saveAsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (*nArgs == 0) {
    	fprintf(stderr, "nedit: save_as action requires file argument\n");
    	return;
    }
    SaveWindowAs(WidgetToWindow(w), args[0],
    	    *nArgs == 2 && !strCaseCmp(args[1], "wrapped"));
}

static void revertDialogAP(Widget w, XEvent *event, String *args,
        Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    int b;
    
    /* re-reading file is irreversible, prompt the user first */
    if (window->fileChanged)
    {
        b = DialogF(DF_QUES, window->shell, 2, "Discard Changes",
                "Discard changes to\n%s%s?", "OK", "Cancel", window->path,
                window->filename);
    } else
    {
        b = DialogF(DF_QUES, window->shell, 2, "Reload File",
                "Re-load file\n%s%s?", "Re-read", "Cancel", window->path,
                window->filename);
    }

    if (b != 1)
    {
        return;
    }
    XtCallActionProc(window->lastFocus, "revert_to_saved", event, NULL, 0);
}


static void revertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    RevertToSaved(WidgetToWindow(w));
}

static void includeDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    char filename[MAXPATHLEN], *params[1];
    int response;
    
    if (CheckReadOnly(window))
    	return;
    response = PromptForExistingFile(window, "Include File", filename);
    if (response != GFN_OK)
    	return;
    params[0] = filename;
    XtCallActionProc(window->lastFocus, "include_file", event, params, 1);
}

static void includeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);

    if (CheckReadOnly(window))
    	return;
    if (*nArgs == 0) {
    	fprintf(stderr, "nedit: include action requires file argument\n");
    	return;
    }
    IncludeFile(WidgetToWindow(w), args[0]);
}

static void loadMacroDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    char filename[MAXPATHLEN], *params[1];
    int response;
    
    response = PromptForExistingFile(window, "Load Macro File", filename);
    if (response != GFN_OK)
    	return;
    params[0] = filename;
    XtCallActionProc(window->lastFocus, "load_macro_file", event, params, 1);
}

static void loadMacroAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    if (*nArgs == 0) {
    	fprintf(stderr,"nedit: load_macro_file action requires file argument\n");
    	return;
    }
    ReadMacroFile(WidgetToWindow(w), args[0], True);
}

static void loadTagsDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    char filename[MAXPATHLEN], *params[1];
    int response;
    
    response = PromptForExistingFile(window, "Load Tags File", filename);
    if (response != GFN_OK)
    	return;
    params[0] = filename;
    XtCallActionProc(window->lastFocus, "load_tags_file", event, params, 1);
}

static void loadTagsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    if (*nArgs == 0) {
    	fprintf(stderr,"nedit: load_tags_file action requires file argument\n");
    	return;
    }

    if (!AddTagsFile(args[0], TAG))
    {
        DialogF(DF_WARN, WidgetToWindow(w)->shell, 1, "Error Reading File",
                "Error reading ctags file:\n'%s'\ntags not loaded", "OK",
                args[0]);
    }
}

static void unloadTagsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    if (*nArgs == 0) {
	fprintf(stderr,
		"nedit: unload_tags_file action requires file argument\n");
	return;
    }
    
    if (DeleteTagsFile(args[0], TAG, True)) {
    	WindowInfo *win;

	/* refresh the "Unload Tags File" tear-offs after unloading, or 
	   close the tear-offs if all tags files have been unloaded */
	for (win=WindowList; win!=NULL; win=win->next) {
    	    if (IsTopDocument(win) && 
	            !XmIsMenuShell(XtParent(win->unloadTagsMenuPane))) {
    		if (XtIsSensitive(win->unloadTagsMenuItem))
		    updateTagsFileMenu(win);
		else
		    _XmDismissTearOff(XtParent(win->unloadTagsMenuPane),
		            NULL, NULL);
	    }
	}
    }
}

static void loadTipsDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    char filename[MAXPATHLEN], *params[1];
    int response;
    
    response = PromptForExistingFile(window, "Load Calltips File", filename);
    if (response != GFN_OK)
    	return;
    params[0] = filename;
    XtCallActionProc(window->lastFocus, "load_tips_file", event, params, 1);
}

static void loadTipsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    if (*nArgs == 0) {
    	fprintf(stderr,"nedit: load_tips_file action requires file argument\n");
    	return;
    }

    if (!AddTagsFile(args[0], TIP))
    {
        DialogF(DF_WARN, WidgetToWindow(w)->shell, 1, "Error Reading File",
                "Error reading tips file:\n'%s'\ntips not loaded", "OK",
                args[0]);
    }
}

static void unloadTipsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    if (*nArgs == 0) {
	fprintf(stderr,
		"nedit: unload_tips_file action requires file argument\n");
	return;
    }
    /* refresh the "Unload Calltips File" tear-offs after unloading, or 
       close the tear-offs if all tips files have been unloaded */
    if (DeleteTagsFile(args[0], TIP, True)) {
    	WindowInfo *win;

	for (win=WindowList; win!=NULL; win=win->next) {
    	    if (IsTopDocument(win) && 
	            !XmIsMenuShell(XtParent(win->unloadTipsMenuPane))) {
    		if (XtIsSensitive(win->unloadTipsMenuItem))
		    updateTipsFileMenu(win);
		else
		    _XmDismissTearOff(XtParent(win->unloadTipsMenuPane),
		            NULL, NULL);
	    }
	}
    }
}

static void printAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    PrintWindow(WidgetToWindow(w), False);
}

static void printSelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    PrintWindow(WidgetToWindow(w), True);
}

static void exitAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);

    if (!CheckPrefsChangesSaved(window->shell))
        return;
    
    /* If this is not the last window (more than one window is open),
       confirm with the user before exiting. */
    if (GetPrefWarnExit() && !(window == WindowList && window->next == NULL)) {
        int resp, titleLen, lineLen;
        char exitMsg[DF_MAX_MSG_LENGTH], *ptr, *title;
	char filename[MAXPATHLEN];
        WindowInfo *win;

        /* List the windows being edited and make sure the
           user really wants to exit */
        ptr = exitMsg;
	lineLen = 0;
        strcpy(ptr, "Editing: "); ptr += 9; lineLen += 9;
        for (win=WindowList; win!=NULL; win=win->next) {
    	    sprintf(filename, "%s%s", win->filename, win->fileChanged? "*": "");
	    title = filename;
            titleLen = strlen(title);
            if (ptr - exitMsg + titleLen + 30 >= DF_MAX_MSG_LENGTH) {
        	strcpy(ptr, "..."); ptr += 3;
        	break;
            }
	    if (lineLen + titleLen + (win->next==NULL?5:2) > 50) {
		*ptr++ = '\n';
		lineLen = 0;
	    }
	    if (win->next == NULL) {
		sprintf(ptr, "and %s.", title);
		ptr += 5 + titleLen;
		lineLen += 5 + titleLen;
	    } else {
		sprintf(ptr, "%s, ", title);
		ptr += 2 + titleLen;
		lineLen += 2 + titleLen;
	    }
        }
        sprintf(ptr, "\n\nExit NEdit?");
        resp = DialogF(DF_QUES, window->shell, 2, "Exit", "%s", "Exit",
                "Cancel", exitMsg);
        if (resp == 2)
                return;
    }

    /* Close all files and exit when the last one is closed */
    if (CloseAllFilesAndWindows())
        exit(EXIT_SUCCESS);
}

static void undoAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    Undo(window);
}

static void redoAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    Redo(window);
}

static void clearAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    BufRemoveSelected(window->buffer);
}

static void selAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    BufSelect(window->buffer, 0, window->buffer->length);
}

static void shiftLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    ShiftSelection(window, SHIFT_LEFT, False);
}

static void shiftLeftTabAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    ShiftSelection(window, SHIFT_LEFT, True);
}

static void shiftRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    ShiftSelection(window, SHIFT_RIGHT, False);
}

static void shiftRightTabAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    ShiftSelection(window, SHIFT_RIGHT, True);
}

static void findDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    DoFindDlog(WidgetToWindow(w), searchDirection(0, args, nArgs),
               searchKeepDialogs(0, args, nArgs), searchType(0, args, nArgs),
               event->xbutton.time);
}

static void findAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (*nArgs == 0) {
    	fprintf(stderr, "nedit: find action requires search string argument\n");
    	return;
    }
    SearchAndSelect(WidgetToWindow(w), searchDirection(1, args, nArgs), args[0],
    	    searchType(1, args, nArgs), searchWrap(1, args, nArgs));
}

static void findSameAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    SearchAndSelectSame(WidgetToWindow(w), searchDirection(0, args, nArgs),
	 	searchWrap(0, args, nArgs));
}

static void findSelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    SearchForSelected(WidgetToWindow(w), searchDirection(0, args, nArgs),
    	    searchType(0, args, nArgs), searchWrap(0, args, nArgs),
            event->xbutton.time);
}

static void startIncrFindAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    BeginISearch(WidgetToWindow(w), searchDirection(0, args, nArgs));
}

static void findIncrAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    int i, continued = FALSE;
    if (*nArgs == 0) {
    	fprintf(stderr, "nedit: find action requires search string argument\n");
    	return;
    }
    for (i=1; i<(int)*nArgs; i++)
    	if (!strCaseCmp(args[i], "continued"))
    	    continued = TRUE;
    SearchAndSelectIncremental(WidgetToWindow(w),
	    searchDirection(1, args, nArgs), args[0],
	    searchType(1, args, nArgs), searchWrap(1, args, nArgs), continued); 
}

static void replaceDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    DoFindReplaceDlog(window, searchDirection(0, args, nArgs),
        searchKeepDialogs(0, args, nArgs), searchType(0, args, nArgs),
        event->xbutton.time);
}

static void replaceAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    if (*nArgs < 2) {
    	fprintf(stderr,
    	"nedit: replace action requires search and replace string arguments\n");
    	return;
    }
    SearchAndReplace(window, searchDirection(2, args, nArgs),
    	    args[0], args[1], searchType(2, args, nArgs), searchWrap(2, args, nArgs));
}

static void replaceAllAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    if (*nArgs < 2) {
    	fprintf(stderr,
    "nedit: replace_all action requires search and replace string arguments\n");
    	return;
    }
    ReplaceAll(window, args[0], args[1], searchType(2, args, nArgs));
}

static void replaceInSelAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    if (*nArgs < 2) {
    	fprintf(stderr,
  "nedit: replace_in_selection requires search and replace string arguments\n");
    	return;
    }
    ReplaceInSelection(window, args[0], args[1],
    	    searchType(2, args, nArgs));
}

static void replaceSameAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    ReplaceSame(window, searchDirection(0, args, nArgs), searchWrap(0, args, nArgs));
}

static void replaceFindAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    {
        return;
    }

    if (*nArgs < 2)
    {
        DialogF(DF_WARN, window->shell, 1, "Error in replace_find",
                "replace_find action requires search and replace string arguments",
                "OK");
        return;
    }

    ReplaceAndSearch(window, searchDirection(2, args, nArgs), args[0], args[1],
            searchType(2, args, nArgs), searchWrap(0, args, nArgs));
}

static void replaceFindSameAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
       return;
    ReplaceFindSame(window, searchDirection(0, args, nArgs), searchWrap(0, args, nArgs));
}

static void gotoAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    int lineNum, column, position, curCol;
    
    /* Accept various formats:
          [line]:[column]   (menu action)
          line              (macro call)
          line, column      (macro call) */
    if (*nArgs == 0 || *nArgs > 2 
            || (*nArgs == 1
                && StringToLineAndCol(args[0], &lineNum, &column ) == -1)
            || (*nArgs == 2
                && (!StringToNum(args[0], &lineNum)
                    || !StringToNum(args[1], &column)))) {
        fprintf(stderr,"nedit: goto_line_number action requires line and/or column number\n");
        return;
    }

    /* User specified column, but not line number */
    if (lineNum == -1) {
        position = TextGetCursorPos(w);
        if (TextPosToLineAndCol(w, position, &lineNum, &curCol) == False) {
            return;
        }
    } else if ( column == -1 ) {
        /* User didn't specify a column */
        SelectNumberedLine(WidgetToWindow(w), lineNum);
        return;
    }

    position = TextLineAndColToPos(w, lineNum, column );
    if ( position == -1 ) {
        return;
    }

    TextSetCursorPos(w, position);
    return;
}

static void gotoDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    GotoLineNumber(WidgetToWindow(w));
}

static void gotoSelectedAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    GotoSelectedLineNumber(WidgetToWindow(w), event->xbutton.time);
}

static void repeatDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    RepeatDialog(WidgetToWindow(w));
}

static void repeatMacroAP(Widget w, XEvent *event, String *args,
    	Cardinal *nArgs)
{
    int how;
    
    if (*nArgs != 2) {
    	fprintf(stderr, "nedit: repeat_macro requires two arguments\n");
    	return;
    }
    if (!strcmp(args[0], "in_selection"))
	how = REPEAT_IN_SEL;
    else if (!strcmp(args[0], "to_end"))
	how = REPEAT_TO_END;
    else if (sscanf(args[0], "%d", &how) != 1) {
    	fprintf(stderr, "nedit: repeat_macro requires method/count\n");
    	return;
    }
    RepeatMacro(WidgetToWindow(w), args[1], how);
}

static void markAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (*nArgs == 0 || strlen(args[0]) != 1 || 
      	    !isalnum((unsigned char)args[0][0])) {
    	fprintf(stderr,"nedit: mark action requires a single-letter label\n");
    	return;
    }
    AddMark(WidgetToWindow(w), w, args[0][0]);
}

static void markDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    MarkDialog(WidgetToWindow(w));
}

static void gotoMarkAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (*nArgs == 0 || strlen(args[0]) != 1 || 
      	    !isalnum((unsigned char)args[0][0])) {
     	fprintf(stderr,
     	    	"nedit: goto_mark action requires a single-letter label\n");
     	return;
    }
    GotoMark(WidgetToWindow(w), w, args[0][0], *nArgs > 1 &&
	    !strcmp(args[1], "extend"));
}

static void gotoMarkDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    GotoMarkDialog(WidgetToWindow(w), *nArgs!=0 && !strcmp(args[0], "extend"));
}

static void selectToMatchingAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    SelectToMatchingCharacter(WidgetToWindow(w));
}

static void gotoMatchingAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    GotoMatchingCharacter(WidgetToWindow(w));
}

static void findDefAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    FindDefinition(WidgetToWindow(w), event->xbutton.time,
	    *nArgs == 0 ? NULL : args[0]);
}

static void showTipAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) 
{
    FindDefCalltip(WidgetToWindow(w), event->xbutton.time,
	    *nArgs == 0 ? NULL : args[0]);
}

static void splitPaneAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    SplitPane(window);
    if (IsTopDocument(window)) {
	XtSetSensitive(window->splitPaneItem, window->nPanes < MAX_PANES);
	XtSetSensitive(window->closePaneItem, window->nPanes > 0);
    }
}

static void closePaneAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    ClosePane(window);
    if (IsTopDocument(window)) {
	XtSetSensitive(window->splitPaneItem, window->nPanes < MAX_PANES);
	XtSetSensitive(window->closePaneItem, window->nPanes > 0);
    }
}

static void detachDocumentDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    int resp;
    
    if (NDocuments(window) < 2)
    	return;
    
    resp = DialogF(DF_QUES, window->shell, 2, "Detach %s?", 
	    "Detach", "Cancel", window->filename);

    if (resp == 1)
    	DetachDocument(window);
}

static void detachDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (NDocuments(window) < 2)
    	return;
    
    DetachDocument(window);
}

static void moveDocumentDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    MoveDocumentDialog(WidgetToWindow(w));
}

static void nextDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    NextDocument(WidgetToWindow(w));    
}

static void prevDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    PreviousDocument(WidgetToWindow(w));    
}

static void lastDocumentAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{    
    LastDocument(WidgetToWindow(w));    
}

static void capitalizeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    UpcaseSelection(window);
}

static void lowercaseAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    DowncaseSelection(window);
}

static void fillAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (CheckReadOnly(window))
    	return;
    FillSelection(window);
}

static void controlDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    unsigned char charCodeString[2];
    char charCodeText[DF_MAX_PROMPT_LENGTH], dummy[DF_MAX_PROMPT_LENGTH];
    char *params[1];
    int charCode, nRead, response;
    
    if (CheckReadOnly(window))
    	return;

    response = DialogF(DF_PROMPT, window->shell, 2, "Insert Ctrl Code",
            "ASCII Character Code:", charCodeText, "OK", "Cancel");

    if (response == 2)
    	return;
    /* If we don't scan for a trailing string invalid input
       would be accepted sometimes. */
    nRead = sscanf(charCodeText, "%i%s", &charCode, dummy);
    if (nRead != 1 || charCode < 0 || charCode > 255) {
    	XBell(TheDisplay, 0);
	return;
    }
    charCodeString[0] = (unsigned char)charCode;
    charCodeString[1] = '\0';
    params[0] = (char *)charCodeString;

    if (!BufSubstituteNullChars((char *)charCodeString, 1, window->buffer))
    {
        DialogF(DF_ERR, window->shell, 1, "Error", "Too much binary data",
                "OK");
        return;
    }

    XtCallActionProc(w, "insert_string", event, params, 1);
}

#ifndef VMS
static void filterDialogAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    char *params[1], cmdText[DF_MAX_PROMPT_LENGTH];
    int resp;
    static char **cmdHistory = NULL;
    static int nHistoryCmds = 0;
    
    if (CheckReadOnly(window))
    	return;
    if (!window->buffer->primary.selected) {
    	XBell(TheDisplay, 0);
	return;
    }
    
    SetDialogFPromptHistory(cmdHistory, nHistoryCmds);

    resp = DialogF(DF_PROMPT, window->shell, 2, "Filter Selection",
            "Shell command:   (use up arrow key to recall previous)",
            cmdText, "OK", "Cancel");

    if (resp == 2)
    	return;
    AddToHistoryList(cmdText, &cmdHistory, &nHistoryCmds);
    params[0] = cmdText;
    XtCallActionProc(w, "filter_selection", event, params, 1);
}

static void shellFilterAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);

    if (CheckReadOnly(window))
    	return;
    if (*nArgs == 0) {
    	fprintf(stderr,
    		"nedit: filter_selection requires shell command argument\n");
    	return;
    }
    FilterSelection(window, args[0],
    	    event->xany.send_event == MACRO_EVENT_MARKER);
}

static void execDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    char *params[1], cmdText[DF_MAX_PROMPT_LENGTH];
    int resp;
    static char **cmdHistory = NULL;
    static int nHistoryCmds = 0;

    if (CheckReadOnly(window))
    	return;
    SetDialogFPromptHistory(cmdHistory, nHistoryCmds);

    resp = DialogF(DF_PROMPT, window->shell, 2, "Execute Command",
            "Shell command:   (use up arrow key to recall previous;\n"
            "%% expands to current filename, # to line number)", cmdText, "OK",
            "Cancel");

    if (resp == 2)
    	return;
    AddToHistoryList(cmdText, &cmdHistory, &nHistoryCmds);
    params[0] = cmdText;
    XtCallActionProc(w, "execute_command", event, params, 1);;
}

static void execAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);

    if (CheckReadOnly(window))
    	return;
    if (*nArgs == 0) {
    	fprintf(stderr,
    		"nedit: execute_command requires shell command argument\n");
    	return;
    }
    ExecShellCommand(window, args[0],
    	    event->xany.send_event == MACRO_EVENT_MARKER);
}

static void execLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);

    if (CheckReadOnly(window))
    	return;
    ExecCursorLine(window, event->xany.send_event == MACRO_EVENT_MARKER);
}

static void shellMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (*nArgs == 0) {
    	fprintf(stderr,
    		"nedit: shell_menu_command requires item-name argument\n");
    	return;
    }
    HidePointerOnKeyedEvent(w, event);
    DoNamedShellMenuCmd(WidgetToWindow(w), args[0],
    	    event->xany.send_event == MACRO_EVENT_MARKER);
}
#endif

static void macroMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (*nArgs == 0) {
    	fprintf(stderr,
    		"nedit: macro_menu_command requires item-name argument\n");
    	return;
    }
    /* Don't allow users to execute a macro command from the menu (or accel)
       if there's already a macro command executing, UNLESS the macro is
       directly called from another one.  NEdit can't handle
       running multiple, independent uncoordinated, macros in the same
       window.  Macros may invoke macro menu commands recursively via the
       macro_menu_command action proc, which is important for being able to
       repeat any operation, and to embed macros within eachother at any
       level, however, a call here with a macro running means that THE USER
       is explicitly invoking another macro via the menu or an accelerator,
       UNLESS the macro event marker is set */
    if (event->xany.send_event != MACRO_EVENT_MARKER) {
	if (WidgetToWindow(w)->macroCmdData != NULL) {
	    XBell(TheDisplay, 0);
            return;
	}
    }
    HidePointerOnKeyedEvent(w, event);
    DoNamedMacroMenuCmd(WidgetToWindow(w), args[0]);
}

static void bgMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs)
{
    if (*nArgs == 0) {
    	fprintf(stderr,
    		"nedit: bg_menu_command requires item-name argument\n");
    	return;
    }
    /* Same remark as for macro menu commands (see above). */
    if (event->xany.send_event != MACRO_EVENT_MARKER) {
	if (WidgetToWindow(w)->macroCmdData != NULL) {
	    XBell(TheDisplay, 0);
            return;
	}
    }
    HidePointerOnKeyedEvent(w, event);
    DoNamedBGMenuCmd(WidgetToWindow(w), args[0]);
}

static void beginningOfSelectionAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textBuffer *buf = TextGetBuffer(w);
    int start, end, isRect, rectStart, rectEnd;
    
    if (!BufGetSelectionPos(buf, &start, &end, &isRect, &rectStart, &rectEnd))
    	return;
    if (!isRect)
    	TextSetCursorPos(w, start);
    else
    	TextSetCursorPos(w, BufCountForwardDispChars(buf,
    		BufStartOfLine(buf, start), rectStart));
}

static void endOfSelectionAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    textBuffer *buf = TextGetBuffer(w);
    int start, end, isRect, rectStart, rectEnd;
    
    if (!BufGetSelectionPos(buf, &start, &end, &isRect, &rectStart, &rectEnd))
    	return;
    if (!isRect)
    	TextSetCursorPos(w, end);
    else
    	TextSetCursorPos(w, BufCountForwardDispChars(buf,
    		BufStartOfLine(buf, end), rectEnd));
}

static void raiseWindowAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    WindowInfo *nextWindow;
    WindowInfo *tmpWindow;
    int windowIndex;
    Boolean focus = GetPrefFocusOnRaise();

    if (*nArgs > 0) {
        if (strcmp(args[0], "last") == 0) {
            window = WindowList;
        }
        else if (strcmp(args[0], "first") == 0) {
            window = WindowList;
            if (window != NULL) {
                nextWindow = window->next;
                while (nextWindow != NULL) {
                    window = nextWindow;
                    nextWindow = nextWindow->next;
                }
            }
        }
        else if (strcmp(args[0], "previous") == 0) {
            tmpWindow = window;
            window = WindowList;
            if (window != NULL) {
                nextWindow = window->next;
                while (nextWindow != NULL && nextWindow != tmpWindow) {
                    window = nextWindow;
                    nextWindow = nextWindow->next;
                }
                if (nextWindow == NULL && tmpWindow != WindowList) {
                    window = NULL;
                }
            }
        }
        else if (strcmp(args[0], "next") == 0) {
            if (window != NULL) {
                window = window->next;
                if (window == NULL) {
                    window = WindowList;
                }
            }
        }
        else {
            if (sscanf(args[0], "%d", &windowIndex) == 1) {
                if (windowIndex > 0) {
                    for (window = WindowList; window != NULL && windowIndex > 1;
                        --windowIndex) {
                        window = window->next;
                    }
                }
                else if (windowIndex < 0) {
                    for (window = WindowList; window != NULL;
                        window = window->next) {
                        ++windowIndex;
                    }
                    if (windowIndex >= 0) {
                        for (window = WindowList; window != NULL &&
                            windowIndex > 0; window = window->next) {
                            --windowIndex;
                        }
                    }
                    else {
                        window = NULL;
                    }
                }
                else {
                    window = NULL;
                }
            }
            else {
                window = NULL;
            }
        }

        if (*nArgs > 1) {
            if (strcmp(args[1], "focus") == 0) {
                focus = True;
            }
            else if (strcmp(args[1], "nofocus") == 0) {
                focus = False;
            }
        }
    }
    if (window != NULL) {
        RaiseFocusDocumentWindow(window, focus);
    }
    else {
        XBell(TheDisplay, 0);
    }
}

static void focusPaneAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Widget newFocusPane = NULL;
    int paneIndex;
    
    if (*nArgs > 0) {
        if (strcmp(args[0], "first") == 0) {
            paneIndex = 0;
        }
        else if (strcmp(args[0], "last") == 0) {
            paneIndex = window->nPanes;
        }
        else if (strcmp(args[0], "next") == 0) {
            paneIndex = WidgetToPaneIndex(window, window->lastFocus) + 1;
            if (paneIndex > window->nPanes) {
                paneIndex = 0;
            }
        }
        else if (strcmp(args[0], "previous") == 0) {
            paneIndex = WidgetToPaneIndex(window, window->lastFocus) - 1;
            if (paneIndex < 0) {
                paneIndex = window->nPanes;
            }
        }
        else {
            if (sscanf(args[0], "%d", &paneIndex) == 1) {
                if (paneIndex > 0) {
                    paneIndex = paneIndex - 1;
                }
                else if (paneIndex < 0) {
                    paneIndex = window->nPanes + (paneIndex + 1);
                }
                else {
                    paneIndex = -1;
                }
            }
        }
        if (paneIndex >= 0 && paneIndex <= window->nPanes) {
            newFocusPane = GetPaneByIndex(window, paneIndex);
        }
        if (newFocusPane != NULL) {
            window->lastFocus = newFocusPane;
            XmProcessTraversal(window->lastFocus, XmTRAVERSE_CURRENT);
        }
        else {
            XBell(TheDisplay, 0);
        }
    }
    else {
        fprintf(stderr, "nedit: focus_pane requires argument\n");
    }
}

#define ACTION_BOOL_PARAM_OR_TOGGLE(newState, numArgs, argvVal, oValue, actionName) \
    if ((numArgs) > 0) { \
        int intState; \
        \
        if (sscanf(argvVal[0], "%d", &intState) == 1) { \
            (newState) = (intState != 0); \
        } \
        else { \
            fprintf(stderr, "nedit: %s requires 0 or 1 argument\n", actionName); \
            return; \
        } \
    } \
    else { \
        (newState) = !(oValue); \
    }

static void setStatisticsLineAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    /* stats line is a shell-level item, so we toggle the button
       state regardless of it's 'topness' */
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, window->showStats,
            "set_statistics_line");
    XmToggleButtonSetState(window->statsLineItem, newState, False);
    ShowStatsLine(window, newState);
}

static void setIncrementalSearchLineAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    /* i-search line is a shell-level item, so we toggle the button
       state regardless of it's 'topness' */
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args,
            window->showISearchLine, "set_incremental_search_line");
    XmToggleButtonSetState(window->iSearchLineItem, newState, False);
    ShowISearchLine(window, newState);
}

static void setShowLineNumbersAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    /* line numbers panel is a shell-level item, so we toggle the button
       state regardless of it's 'topness' */
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, 
            window->showLineNumbers, "set_show_line_numbers");
    XmToggleButtonSetState(window->lineNumsItem, newState, False);
    ShowLineNumbers(window, newState);
}

static void setAutoIndentAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    if (*nArgs > 0) {
        if (strcmp(args[0], "off") == 0) {
            SetAutoIndent(window, NO_AUTO_INDENT);
        }
        else if (strcmp(args[0], "on") == 0) {
            SetAutoIndent(window, AUTO_INDENT);
        }
        else if (strcmp(args[0], "smart") == 0) {
            SetAutoIndent(window, SMART_INDENT);
        }
        else {
            fprintf(stderr, "nedit: set_auto_indent invalid argument\n");
        }
    }
    else {
        fprintf(stderr, "nedit: set_auto_indent requires argument\n");
    }
}

static void setWrapTextAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    if (*nArgs > 0) {
        if (strcmp(args[0], "none") == 0) {
            SetAutoWrap(window, NO_WRAP);
        }
        else if (strcmp(args[0], "auto") == 0) {
            SetAutoWrap(window, NEWLINE_WRAP);
        }
        else if (strcmp(args[0], "continuous") == 0) {
            SetAutoWrap(window, CONTINUOUS_WRAP);
        }
        else {
            fprintf(stderr, "nedit: set_wrap_text invalid argument\n");
        }
    }
    else {
        fprintf(stderr, "nedit: set_wrap_text requires argument\n");
    }
}

static void setWrapMarginAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (*nArgs > 0) {
        int newMargin = 0;
        if (sscanf(args[0], "%d", &newMargin) == 1 &&
            newMargin >= 0 &&
            newMargin < 1000) {
            int i;
            
            XtVaSetValues(window->textArea, textNwrapMargin, newMargin, NULL);
            for (i = 0; i < window->nPanes; ++i) {
                XtVaSetValues(window->textPanes[i], textNwrapMargin, newMargin, NULL);
            }
        }
        else {
            fprintf(stderr,
                "nedit: set_wrap_margin requires integer argument >= 0 and < 1000\n");
        }
    }
    else {
        fprintf(stderr, "nedit: set_wrap_margin requires argument\n");
    }
}

static void setHighlightSyntaxAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, window->highlightSyntax, "set_highlight_syntax");

    if (IsTopDocument(window))
    	XmToggleButtonSetState(window->highlightItem, newState, False);
    window->highlightSyntax = newState;
    if (window->highlightSyntax) {
        StartHighlighting(window, True);
    } else {
        StopHighlighting(window);
    }
}

static void setMakeBackupCopyAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, window->saveOldVersion, "set_make_backup_copy");

#ifndef VMS
    if (IsTopDocument(window))
    	XmToggleButtonSetState(window->saveLastItem, newState, False);
#endif
    window->saveOldVersion = newState;
}

static void setIncrementalBackupAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, window->autoSave, "set_incremental_backup");

    if (IsTopDocument(window))
    	XmToggleButtonSetState(window->autoSaveItem, newState, False);
    window->autoSave = newState;
}

static void setShowMatchingAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    if (*nArgs > 0) {
        if (strcmp(args[0], NO_FLASH_STRING) == 0) {
            SetShowMatching(window, NO_FLASH);
        }
        else if (strcmp(args[0], FLASH_DELIMIT_STRING) == 0) {
            SetShowMatching(window, FLASH_DELIMIT);
        }
        else if (strcmp(args[0], FLASH_RANGE_STRING) == 0) {
            SetShowMatching(window, FLASH_RANGE);
        }
        /* For backward compatibility with pre-5.2 versions, we also
           accept 0 and 1 as aliases for NO_FLASH and FLASH_DELIMIT.
           It is quite unlikely, though, that anyone ever used this 
           action procedure via the macro language or a key binding,
           so this can probably be left out safely. */
        else if (strcmp(args[0], "0") == 0) {
           SetShowMatching(window, NO_FLASH);
        }
        else if (strcmp(args[0], "1") == 0) {
           SetShowMatching(window, FLASH_DELIMIT);
        }
        else {
            fprintf(stderr, "nedit: Invalid argument for set_show_matching\n");
        }
    }
    else {
        fprintf(stderr, "nedit: set_show_matching requires argument\n");
    }
}

static void setMatchSyntaxBasedAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, window->matchSyntaxBased, "set_match_syntax_based");

    if (IsTopDocument(window))
    	XmToggleButtonSetState(window->matchSyntaxBasedItem, newState, False);
    window->matchSyntaxBased = newState;
}

static void setOvertypeModeAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    if (window == NULL)
        return;
 
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, window->overstrike, "set_overtype_mode");

    if (IsTopDocument(window))
    	XmToggleButtonSetState(window->overtypeModeItem, newState, False);
    SetOverstrike(window, newState);
}

static void setLockedAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, IS_USER_LOCKED(window->lockReasons), "set_locked");
    
    SET_USER_LOCKED(window->lockReasons, newState);
    if (IsTopDocument(window))
    	XmToggleButtonSetState(window->readOnlyItem, IS_ANY_LOCKED(window->lockReasons), False);
    UpdateWindowTitle(window);
    UpdateWindowReadOnly(window);
}

static void setTabDistAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (*nArgs > 0) {
        int newTabDist = 0;
        if (sscanf(args[0], "%d", &newTabDist) == 1 &&
            newTabDist > 0 &&
            newTabDist <= MAX_EXP_CHAR_LEN) {
            SetTabDist(window, newTabDist);
        }
        else {
            fprintf(stderr,
                "nedit: set_tab_dist requires integer argument > 0 and <= %d\n",
                MAX_EXP_CHAR_LEN);
        }
    }
    else {
        fprintf(stderr, "nedit: set_tab_dist requires argument\n");
    }
}

static void setEmTabDistAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    if (*nArgs > 0) {
        int newEmTabDist = 0;
        if (sscanf(args[0], "%d", &newEmTabDist) == 1 &&
            newEmTabDist < 1000) {
            if (newEmTabDist < 0) {
                newEmTabDist = 0;
            }
            SetEmTabDist(window, newEmTabDist);
        }
        else {
            fprintf(stderr,
                "nedit: set_em_tab_dist requires integer argument >= -1 and < 1000\n");
        }
    }
    else {
        fprintf(stderr, "nedit: set_em_tab_dist requires integer argument\n");
    }
}

static void setUseTabsAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    Boolean newState;
    
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, *nArgs, args, window->buffer->useTabs, "set_use_tabs");

    window->buffer->useTabs = newState;
}

static void setFontsAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    if (*nArgs >= 4) {
        SetFonts(window, args[0], args[1], args[2], args[3]);
    }
    else {
        fprintf(stderr, "nedit: set_fonts requires 4 arguments\n");
    }
}

static void setLanguageModeAP(Widget w, XEvent *event, String *args,
    Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);

    if (*nArgs > 0) {
        SetLanguageMode(window, FindLanguageMode(args[0]), FALSE);
    }
    else {
        fprintf(stderr, "nedit: set_language_mode requires argument\n");
    }
}

/*
** Same as AddSubMenu from libNUtil.a but 1) mnemonic is optional (NEdit
** users like to be able to re-arrange the mnemonics so they can set Alt
** key combinations as accelerators), 2) supports the short/full option
** of SGI_CUSTOM mode, 3) optionally returns the cascade button widget
** in "cascadeBtn" if "cascadeBtn" is non-NULL.
*/
static Widget createMenu(Widget parent, char *name, char *label,
    	char mnemonic, Widget *cascadeBtn, int mode)
{
    Widget menu, cascade;
    XmString st1;

    menu = CreatePulldownMenu(parent, name, NULL, 0);
    cascade = XtVaCreateWidget(name, xmCascadeButtonWidgetClass, parent, 
    	XmNlabelString, st1=XmStringCreateSimple(label),
    	XmNsubMenuId, menu, NULL);
    XmStringFree(st1);
    if (mnemonic != 0)
    	XtVaSetValues(cascade, XmNmnemonic, mnemonic, NULL);
#ifdef SGI_CUSTOM
    if (mode == SHORT || !GetPrefShortMenus())
    	XtManageChild(cascade);
    if (mode == FULL)
    	addToToggleShortList(cascade);
#else
    XtManageChild(cascade);
#endif
    if (cascadeBtn != NULL)
    	*cascadeBtn = cascade;
    return menu;
}

/*
** Same as AddMenuItem from libNUtil.a without setting the accelerator
** (these are set in the fallback app-defaults so users can change them),
** and with the short/full option required in SGI_CUSTOM mode.
*/
static Widget createMenuItem(Widget parent, char *name, char *label,
	char mnemonic, menuCallbackProc callback, void *cbArg, int mode)
{
    Widget button;
    XmString st1;
    

    button = XtVaCreateWidget(name, xmPushButtonWidgetClass, parent, 
    	    XmNlabelString, st1=XmStringCreateSimple(label),
    	    XmNmnemonic, mnemonic, NULL);
    XtAddCallback(button, XmNactivateCallback, (XtCallbackProc)callback, cbArg);
    XmStringFree(st1);
#ifdef SGI_CUSTOM
    if (mode == SHORT || !GetPrefShortMenus())
    	XtManageChild(button);
    if (mode == FULL)
    	addToToggleShortList(button);
    XtVaSetValues(button, XmNuserData, PERMANENT_MENU_ITEM, NULL);
#else
    XtManageChild(button);
#endif
    return button;
}

/*
** "fake" menu items allow accelerators to be attached, but don't show up
** in the menu.  They are necessary to process the shifted menu items because
** Motif does not properly process the event descriptions in accelerator
** resources, and you can't specify "shift key is optional"
*/
static Widget createFakeMenuItem(Widget parent, char *name,
	menuCallbackProc callback, void *cbArg)
{
    Widget button;
    XmString st1;
    
    button = XtVaCreateManagedWidget(name, xmPushButtonWidgetClass, parent,
    	    XmNlabelString, st1=XmStringCreateSimple(""),
    	    XmNshadowThickness, 0,
    	    XmNmarginHeight, 0,
    	    XmNheight, 0, NULL);
    XtAddCallback(button, XmNactivateCallback, (XtCallbackProc)callback, cbArg);
    XmStringFree(st1);
    XtVaSetValues(button, XmNtraversalOn, False, NULL);

    return button;
}

/*
** Add a toggle button item to an already established pull-down or pop-up
** menu, including mnemonics, accelerators and callbacks.
*/
static Widget createMenuToggle(Widget parent, char *name, char *label,
	char mnemonic, menuCallbackProc callback, void *cbArg, int set,
	int mode)
{
    Widget button;
    XmString st1;
    
    button = XtVaCreateWidget(name, xmToggleButtonWidgetClass, parent, 
    	    XmNlabelString, st1=XmStringCreateSimple(label),
    	    XmNmnemonic, mnemonic,
    	    XmNset, set, NULL);
    XtAddCallback(button, XmNvalueChangedCallback, (XtCallbackProc)callback,
    	    cbArg);
    XmStringFree(st1);
#ifdef SGI_CUSTOM
    if (mode == SHORT || !GetPrefShortMenus())
    	XtManageChild(button);
    if (mode == FULL)
    	addToToggleShortList(button);
    XtVaSetValues(button, XmNuserData, PERMANENT_MENU_ITEM, NULL);
#else
    XtManageChild(button);
#endif
    return button;
}

/*
** Create a toggle button with a diamond (radio-style) appearance
*/
static Widget createMenuRadioToggle(Widget parent, char *name, char *label,
	char mnemonic, menuCallbackProc callback, void *cbArg, int set,
	int mode)
{
    Widget button;
    button = createMenuToggle(parent, name, label, mnemonic, callback, cbArg,
	    set, mode);
    XtVaSetValues(button, XmNindicatorType, XmONE_OF_MANY, NULL);
    return button;
}

static Widget createMenuSeparator(Widget parent, char *name, int mode)
{
    Widget button;
    
    button = XmCreateSeparator(parent, name, NULL, 0);
#ifdef SGI_CUSTOM
    if (mode == SHORT || !GetPrefShortMenus())
    	XtManageChild(button);
    if (mode == FULL)
    	addToToggleShortList(button);
    XtVaSetValues(button, XmNuserData, PERMANENT_MENU_ITEM, NULL);
#else
    XtManageChild(button);
#endif
    return button;
}

/*
** Make sure the close menu item is dimmed appropriately for the current
** set of windows.  It should be dim only for the last Untitled, unmodified,
** editor window, and sensitive otherwise.
*/
void CheckCloseDim(void)
{
    WindowInfo *window;
    
    if (WindowList == NULL)
    	return;
    if (WindowList->next==NULL &&
    	    !WindowList->filenameSet && !WindowList->fileChanged) {
    	XtSetSensitive(WindowList->closeItem, FALSE);
    	return;
    }
    
    for (window=WindowList; window!=NULL; window=window->next) {
    	if (!IsTopDocument(window))
	    continue;
    	XtSetSensitive(window->closeItem, True);
    }
}

/*
** Invalidate the Window menus of all NEdit windows to but don't change
** the menus until they're needed (Originally, this was "UpdateWindowMenus",
** but creating and destroying manu items for every window every time a
** new window was created or something changed, made things move very
** slowly with more than 10 or so windows).
*/
void InvalidateWindowMenus(void)
{
    WindowInfo *w;

    /* Mark the window menus invalid (to be updated when the user pulls one
       down), unless the menu is torn off, meaning it is visible to the user
       and should be updated immediately */
    for (w=WindowList; w!=NULL; w=w->next) {
    	if (!XmIsMenuShell(XtParent(w->windowMenuPane)))
    	    updateWindowMenu(w);
    	else
    	    w->windowMenuValid = False;
    }
}

/*
** Mark the Previously Opened Files menus of all NEdit windows as invalid.
** Since actually changing the menus is slow, they're just marked and updated
** when the user pulls one down.
*/
static void invalidatePrevOpenMenus(void)
{
    WindowInfo *w;

    /* Mark the menus invalid (to be updated when the user pulls one
       down), unless the menu is torn off, meaning it is visible to the user
       and should be updated immediately */
    for (w=WindowList; w!=NULL; w=w->next) {
    	if (!XmIsMenuShell(XtParent(w->prevOpenMenuPane)))
    	    updatePrevOpenMenu(w);
    }
}

/*
** Add a file to the list of previously opened files for display in the
** File menu.
*/
void AddToPrevOpenMenu(const char *filename)
{
    int i;
    char *nameCopy;
    WindowInfo *w;

    /* If the Open Previous command is disabled, just return */
    if (GetPrefMaxPrevOpenFiles() < 1) {
    	return;
    }

    /*  Refresh list of previously opened files to avoid Big Race Condition,
        where two sessions overwrite each other's changes in NEdit's
        history file.
        Of course there is still Little Race Condition, which occurs if a
        Session A reads the list, then Session B reads the list and writes
        it before Session A gets a chance to write.  */
    ReadNEditDB();

    /* If the name is already in the list, move it to the start */
    for (i=0; i<NPrevOpen; i++) {
    	if (!strcmp(filename, PrevOpen[i])) {
    	    nameCopy = PrevOpen[i];
    	    memmove(&PrevOpen[1], &PrevOpen[0], sizeof(char *) * i);
    	    PrevOpen[0] = nameCopy;
    	    invalidatePrevOpenMenus();
	    WriteNEditDB();
    	    return;
    	}
    }
    
    /* If the list is already full, make room */
    if (NPrevOpen >= GetPrefMaxPrevOpenFiles()) {
        /*  This is only safe if GetPrefMaxPrevOpenFiles() > 0.  */
        NEditFree(PrevOpen[--NPrevOpen]);
    }
    
    /* Add it to the list */
    nameCopy = NEditStrdup(filename);
    memmove(&PrevOpen[1], &PrevOpen[0], sizeof(char *) * NPrevOpen);
    PrevOpen[0] = nameCopy;
    NPrevOpen++;
    
    /* Mark the Previously Opened Files menu as invalid in all windows */
    invalidatePrevOpenMenus();

    /* Undim the menu in all windows if it was previously empty */
    if (NPrevOpen > 0) {
    	for (w=WindowList; w!=NULL; w=w->next) {
    	    if (!IsTopDocument(w))
		continue;
    	    XtSetSensitive(w->prevOpenMenuItem, True);
	}
    }
    
    /* Write the menu contents to disk to restore in later sessions */
    WriteNEditDB();
}

static char* getWindowsMenuEntry(const WindowInfo* window)
{
    static char fullTitle[MAXPATHLEN * 2 + 3+ 1];

    sprintf(fullTitle, "%s%s", window->filename, 
	  window->fileChanged? "*" : "");

    if (GetPrefShowPathInWindowsMenu() && window->filenameSet)
    {
       strcat(fullTitle, " - ");
       strcat(fullTitle, window->path);
    }
    
    return(fullTitle);
}             

/*
** Update the Window menu of a single window to reflect the current state of
** all NEdit windows as determined by the global WindowList.
*/
static void updateWindowMenu(const WindowInfo *window)
{
    WindowInfo *w;
    WidgetList items;
    Cardinal nItems;
    int i, n, nWindows, windowIndex;
    WindowInfo **windows;
    
    if (!IsTopDocument(window))
    	return;
	
    /* Make a sorted list of windows */
    for (w=WindowList, nWindows=0; w!=NULL; w=w->next, nWindows++);
    windows = (WindowInfo **)NEditMalloc(sizeof(WindowInfo *) * nWindows);
    for (w=WindowList, i=0; w!=NULL; w=w->next, i++)
    	windows[i] = w;
    qsort(windows, nWindows, sizeof(WindowInfo *), compareWindowNames);

    /* if the menu is torn off, unmanage the menu pane
       before updating it to prevent the tear-off menu
       from shrinking/expanding as the menu entries
       are added */
    if (!XmIsMenuShell(XtParent(window->windowMenuPane)))
    	XtUnmanageChild(window->windowMenuPane);

    /* While it is not possible on some systems (ibm at least) to substitute
       a new menu pane, it is possible to substitute menu items, as long as
       at least one remains in the menu at all times. This routine assumes
       that the menu contains permanent items marked with the value
       PERMANENT_MENU_ITEM in the userData resource, and adds and removes items
       which it marks with the value TEMPORARY_MENU_ITEM */
    
    /* Go thru all of the items in the menu and rename them to
       match the window list.  Delete any extras */
    XtVaGetValues(window->windowMenuPane, XmNchildren, &items,
    	    XmNnumChildren, &nItems, NULL);
    windowIndex = 0;
    nWindows = NWindows();
    for (n=0; n<(int)nItems; n++) {
        XtPointer userData;
    	XtVaGetValues(items[n], XmNuserData, &userData, NULL);
    	if (userData == TEMPORARY_MENU_ITEM) {
	    if (windowIndex >= nWindows) {
    		/* unmanaging before destroying stops parent from displaying */
    		XtUnmanageChild(items[n]);
    		XtDestroyWidget(items[n]);	    	
	    } else {
                XmString st1;
                char* title = getWindowsMenuEntry(windows[windowIndex]);
		XtVaSetValues(items[n], XmNlabelString,
    	    		st1=XmStringCreateSimple(title), NULL);
		XtRemoveAllCallbacks(items[n], XmNactivateCallback);
		XtAddCallback(items[n], XmNactivateCallback,
			(XtCallbackProc)raiseCB, windows[windowIndex]);
	    	XmStringFree(st1);
		windowIndex++;
	    }
	}
    }
    
    /* Add new items for the titles of the remaining windows to the menu */
    for (; windowIndex<nWindows; windowIndex++) {
        XmString st1;
        char* title = getWindowsMenuEntry(windows[windowIndex]);
        Widget btn = XtVaCreateManagedWidget("win", xmPushButtonWidgetClass,
    		window->windowMenuPane, 
    		XmNlabelString, st1=XmStringCreateSimple(title),
		XmNmarginHeight, 0,
    		XmNuserData, TEMPORARY_MENU_ITEM, NULL);
	XtAddCallback(btn, XmNactivateCallback, (XtCallbackProc)raiseCB, 
	    	windows[windowIndex]);
    	XmStringFree(st1);
    }
    NEditFree(windows);

    /* if the menu is torn off, we need to manually adjust the
       dimension of the menuShell _before_ re-managing the menu
       pane, to either expose the hidden menu entries or remove
       the empty space */
    if (!XmIsMenuShell(XtParent(window->windowMenuPane))) {
    	Dimension width, height;
	
	XtVaGetValues(window->windowMenuPane, XmNwidth, &width,
	        XmNheight, &height, NULL);
	XtVaSetValues(XtParent(window->windowMenuPane), XmNwidth, width,
	        XmNheight, height, NULL);
        XtManageChild(window->windowMenuPane);
    }
}

/*
** Update the Previously Opened Files menu of a single window to reflect the
** current state of the list as retrieved from FIXME.
** Thanks to Markus Schwarzenberg for the sorting part.
*/
static void updatePrevOpenMenu(WindowInfo *window)
{
    Widget btn;
    WidgetList items;
    Cardinal nItems;
    int n, index;
    XmString st1;
    char **prevOpenSorted;

    /*  Read history file to get entries written by other sessions.  */
    ReadNEditDB();
                
    /* Sort the previously opened file list if requested */
    prevOpenSorted = (char **)NEditMalloc(NPrevOpen * sizeof(char*));
    memcpy(prevOpenSorted, PrevOpen, NPrevOpen * sizeof(char*));
    if (GetPrefSortOpenPrevMenu())
    	qsort(prevOpenSorted, NPrevOpen, sizeof(char*), cmpStrPtr);

    /* Go thru all of the items in the menu and rename them to match the file
       list.  In older Motifs (particularly ibm), it was dangerous to replace
       a whole menu pane, which would be much simpler.  However, since the
       code was already written for the Windows menu and is well tested, I'll
       stick with this weird method of re-naming the items */
    XtVaGetValues(window->prevOpenMenuPane, XmNchildren, &items,
            XmNnumChildren, &nItems, NULL);
    index = 0;
    for (n=0; n<(int)nItems; n++) {
        if (index >= NPrevOpen) {
            /* unmanaging before destroying stops parent from displaying */
            XtUnmanageChild(items[n]);
            XtDestroyWidget(items[n]);          
        } else {
            XtVaSetValues(items[n], XmNlabelString,
                    st1=XmStringCreateSimple(prevOpenSorted[index]), NULL);
            XtRemoveAllCallbacks(items[n], XmNactivateCallback);
            XtAddCallback(items[n], XmNactivateCallback,
                    (XtCallbackProc)openPrevCB, prevOpenSorted[index]);
            XmStringFree(st1);
            index++;
        }
    }
    
    /* Add new items for the remaining file names to the menu */
    for (; index<NPrevOpen; index++) {
        btn = XtVaCreateManagedWidget("win", xmPushButtonWidgetClass,
                window->prevOpenMenuPane, 
                XmNlabelString, st1=XmStringCreateSimple(prevOpenSorted[index]),
                XmNmarginHeight, 0,
                XmNuserData, TEMPORARY_MENU_ITEM, NULL);
        XtAddCallback(btn, XmNactivateCallback, (XtCallbackProc)openPrevCB, 
                prevOpenSorted[index]);
        XmStringFree(st1);
    }
                
    NEditFree(prevOpenSorted);
}

/*
** This function manages the display of the Tags File Menu, which is displayed
** when the user selects Un-load Tags File.
*/
static void updateTagsFileMenu(WindowInfo *window)
{
    tagFile *tf;
    Widget btn;
    WidgetList items;
    Cardinal nItems;
    int n;
    XmString st1;
		
    /* Go thru all of the items in the menu and rename them to match the file
       list.  In older Motifs (particularly ibm), it was dangerous to replace
       a whole menu pane, which would be much simpler.  However, since the
       code was already written for the Windows menu and is well tested, I'll
       stick with this weird method of re-naming the items */
    XtVaGetValues(window->unloadTagsMenuPane, XmNchildren, &items,
	    XmNnumChildren, &nItems, NULL);
    tf = TagsFileList;
    for (n=0; n<(int)nItems; n++) {
	if (!tf) {
	    /* unmanaging before destroying stops parent from displaying */
	    XtUnmanageChild(items[n]);
	    XtDestroyWidget(items[n]);          
	} else {
	    XtVaSetValues(items[n], XmNlabelString,
		    st1=XmStringCreateSimple(tf->filename), NULL);
	    XtRemoveAllCallbacks(items[n], XmNactivateCallback);
	    XtAddCallback(items[n], XmNactivateCallback,
		    (XtCallbackProc)unloadTagsFileCB, tf->filename);
	    XmStringFree(st1);
	    tf = tf->next;
	}
    }
    
    /* Add new items for the remaining file names to the menu */
    while (tf) {
	btn = XtVaCreateManagedWidget("win", xmPushButtonWidgetClass,
		window->unloadTagsMenuPane, XmNlabelString,
		st1=XmStringCreateSimple(tf->filename),XmNmarginHeight, 0,
		XmNuserData, TEMPORARY_MENU_ITEM, NULL);
	XtAddCallback(btn, XmNactivateCallback,
		(XtCallbackProc)unloadTagsFileCB, tf->filename);
	XmStringFree(st1);
	tf = tf->next;
    }
}

/*
** This function manages the display of the Tips File Menu, which is displayed
** when the user selects Un-load Calltips File.
*/
static void updateTipsFileMenu(WindowInfo *window)
{
    tagFile *tf;
    Widget btn;
    WidgetList items;
    Cardinal nItems;
    int n;
    XmString st1;
		
    /* Go thru all of the items in the menu and rename them to match the file
       list.  In older Motifs (particularly ibm), it was dangerous to replace
       a whole menu pane, which would be much simpler.  However, since the
       code was already written for the Windows menu and is well tested, I'll
       stick with this weird method of re-naming the items */
    XtVaGetValues(window->unloadTipsMenuPane, XmNchildren, &items,
	    XmNnumChildren, &nItems, NULL);
    tf = TipsFileList;
    for (n=0; n<(int)nItems; n++) {
	if (!tf) {
	    /* unmanaging before destroying stops parent from displaying */
	    XtUnmanageChild(items[n]);
	    XtDestroyWidget(items[n]);          
	} else {
	    XtVaSetValues(items[n], XmNlabelString,
		    st1=XmStringCreateSimple(tf->filename), NULL);
	    XtRemoveAllCallbacks(items[n], XmNactivateCallback);
	    XtAddCallback(items[n], XmNactivateCallback,
		    (XtCallbackProc)unloadTipsFileCB, tf->filename);
	    XmStringFree(st1);
	    tf = tf->next;
	}
    }
    
    /* Add new items for the remaining file names to the menu */
    while (tf) {
	btn = XtVaCreateManagedWidget("win", xmPushButtonWidgetClass,
		window->unloadTipsMenuPane, XmNlabelString,
		st1=XmStringCreateSimple(tf->filename),XmNmarginHeight, 0,
		XmNuserData, TEMPORARY_MENU_ITEM, NULL);
	XtAddCallback(btn, XmNactivateCallback,
		(XtCallbackProc)unloadTipsFileCB, tf->filename);
	XmStringFree(st1);
	tf = tf->next;
    }
}

/*
** Comparison function for sorting file names for the Open Previous submenu
*/
static int cmpStrPtr(const void *strA, const void *strB)
{                       
    return strcmp(*((char**)strA), *((char**)strB));
}

#ifdef VMS
    static char neditDBBadFilenameChars[] = "\n\t*?(){}!@#%&' ";
#else
    static char neditDBBadFilenameChars[] = "\n";
#endif

/*
** Write dynamic database of file names for "Open Previous".  Eventually,
** this may hold window positions, and possibly file marks, in which case,
** it should be moved to a different module, but for now it's just a list
** of previously opened files.
*/
void WriteNEditDB(void)
{
    const char* fullName = GetRCFileName(NEDIT_HISTORY);
    FILE *fp;
    int i;
    static char fileHeader[] =
            "# File name database for NEdit Open Previous command\n";

    if (fullName == NULL) {
        /*  GetRCFileName() might return NULL if an error occurs during
            creation of the preference file directory. */
        return;
    }

    /* If the Open Previous command is disabled, just return */
    if (GetPrefMaxPrevOpenFiles() < 1) {
        return;
    }

    /* open the file */
    if ((fp = fopen(fullName, "w")) == NULL) {
#ifdef VMS
        /* When the version number, ";1" is specified as part of the file
           name, fopen(fullName, "w"), will only open for writing if the 
           file does not exist. Using, fopen(fullName, "r+"), opens an
           existing file for "update" - read/write pointer is placed at the
           beginning of file. 
           By calling ftruncate(), we discard the old contents and avoid 
           trailing garbage in the file if the new contents is shorter. */
        if ((fp = fopen(fullName, "r+")) == NULL) {
            return;
        }
        if (ftruncate(fileno(fp), 0) != 0) {
            fclose(fp);
            return;
        }
#else
        return;
#endif        
    }

    /* write the file header text to the file */
    fprintf(fp, "%s", fileHeader);

    /* Write the list of file names */
    for (i = 0; i < NPrevOpen; ++i) {
        size_t lineLen = strlen(PrevOpen[i]);

        if (lineLen > 0 && PrevOpen[i][0] != '#' &&
            strcspn(PrevOpen[i], neditDBBadFilenameChars) == lineLen) {
            fprintf(fp, "%s\n", PrevOpen[i]);
        }
    }

    fclose(fp);
}

/*
**  Read database of file names for 'Open Previous' submenu.
**
**  Eventually, this may hold window positions, and possibly file marks (in
**  which case it should be moved to a different module) but for now it's
**  just a list of previously opened files.
**
**  This list is read once at startup and potentially refreshed before a
**  new entry is about to be written to the file or before the menu is
**  displayed. If the file is modified since the last read (or not read
**  before), it is read in, otherwise nothing is done.
*/
void ReadNEditDB(void)
{
    const char *fullName = GetRCFileName(NEDIT_HISTORY);
    char line[MAXPATHLEN + 2];
    char *nameCopy;
    struct stat attribute;
    FILE *fp;
    size_t lineLen;
    static time_t lastNeditdbModTime = 0;

    /*  If the Open Previous command is disabled or the user set the
        resource to an (invalid) negative value, just return.  */
    if (GetPrefMaxPrevOpenFiles() < 1) {
        return;
    }

    /* Initialize the files list and allocate a (permanent) block memory
       of the size prescribed by the maxPrevOpenFiles resource */
    if (!PrevOpen) {
        PrevOpen = (char**) NEditMalloc(sizeof(char*) * GetPrefMaxPrevOpenFiles());
        NPrevOpen = 0;
    }

    /* Don't move this check ahead of the previous statements. PrevOpen
       must be initialized at all times. */
    if (fullName == NULL)
    {
        /*  GetRCFileName() might return NULL if an error occurs during
            creation of the preference file directory. */
        return;
    }

    /*  Stat history file to see whether someone touched it after this
        session last changed it.  */
    if (0 == stat(fullName, &attribute)) {
        if (lastNeditdbModTime >= attribute.st_mtime) {
            /*  Do nothing, history file is unchanged.  */
            return;
        } else {
            /*  Memorize modtime to compare to next time.  */
            lastNeditdbModTime = attribute.st_mtime;
        }
    } else {
        /*  stat() failed, probably for non-exiting history database.  */
        if (ENOENT != errno)
        {
            perror("nedit: Error reading history database");
        }
        return;
    }
      
    /* open the file */
    if ((fp = fopen(fullName, "r")) == NULL) {
        return;
    }

    /*  Clear previous list.  */
    while (0 != NPrevOpen) {
        NEditFree(PrevOpen[--NPrevOpen]);
    }

    /* read lines of the file, lines beginning with # are considered to be
       comments and are thrown away.  Lines are subject to cursory checking,
       then just copied to the Open Previous file menu list */
    while (True) {
        if (fgets(line, sizeof(line), fp) == NULL) {
            /* end of file */
            fclose(fp);
            return;
        }
        if (line[0] == '#') {
            /* comment */
            continue;
        }
        lineLen = strlen(line);
        if (lineLen == 0) {
            /* blank line */
            continue;
        }
        if (line[lineLen - 1] != '\n') {
            /* no newline, probably truncated */
            fprintf(stderr, "nedit: Line too long in history file\n");
            while (fgets(line, sizeof(line), fp) != NULL) {
                lineLen = strlen(line);
                if (lineLen > 0 && line[lineLen - 1] == '\n') {
                    break;
                }
            }
            continue;
        }
        line[--lineLen] = '\0';
        if (strcspn(line, neditDBBadFilenameChars) != lineLen) {
            /* non-filename characters */
            fprintf(stderr, "nedit: History file may be corrupted\n");
            continue;
        }
        nameCopy = NEditStrdup(line);
        PrevOpen[NPrevOpen++] = nameCopy;
        if (NPrevOpen >= GetPrefMaxPrevOpenFiles()) {
            /* too many entries */
            fclose(fp);
            return;
        }
    }
}

static void setWindowSizeDefault(int rows, int cols)
{
    SetPrefRows(rows);
    SetPrefCols(cols);
    updateWindowSizeMenus();
}

static void updateWindowSizeMenus(void)
{
    WindowInfo *win;
    
    for (win=WindowList; win!=NULL; win=win->next)
    	updateWindowSizeMenu(win);
}

static void updateWindowSizeMenu(WindowInfo *win)
{
    int rows = GetPrefRows(), cols = GetPrefCols();
    char title[50];
    XmString st1;
    
    if (!IsTopDocument(win))
	return;
	
    XmToggleButtonSetState(win->size24x80DefItem, rows==24&&cols==80,False);
    XmToggleButtonSetState(win->size40x80DefItem, rows==40&&cols==80,False);
    XmToggleButtonSetState(win->size60x80DefItem, rows==60&&cols==80,False);
    XmToggleButtonSetState(win->size80x80DefItem, rows==80&&cols==80,False);
    if ((rows!=24 && rows!=40 && rows!=60 && rows!=80) || cols!=80) {
    	XmToggleButtonSetState(win->sizeCustomDefItem, True, False);
    	sprintf(title, "Custom... (%d x %d)", rows, cols);
    	XtVaSetValues(win->sizeCustomDefItem,
    	    	XmNlabelString, st1=XmStringCreateSimple(title), NULL);
    	XmStringFree(st1);
    } else {
    	XmToggleButtonSetState(win->sizeCustomDefItem, False, False);
    	XtVaSetValues(win->sizeCustomDefItem,
    	    	XmNlabelString, st1=XmStringCreateSimple("Custom..."), NULL);
    	XmStringFree(st1);
    }
}

/*
** Scans action argument list for arguments "forward" or "backward" to
** determine search direction for search and replace actions.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static int searchDirection(int ignoreArgs, String *args, Cardinal *nArgs)
{
    int i;
    
    for (i=ignoreArgs; i<(int)*nArgs; i++) {
    	if (!strCaseCmp(args[i], "forward"))
    	    return SEARCH_FORWARD;
    	if (!strCaseCmp(args[i], "backward"))
    	    return SEARCH_BACKWARD;
    }
    return SEARCH_FORWARD;
}

/*
** Scans action argument list for arguments "keep" or "nokeep" to
** determine whether to keep dialogs up for search and replace.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static int searchKeepDialogs(int ignoreArgs, String *args, Cardinal *nArgs)
{
    int i;
    
    for (i=ignoreArgs; i<(int)*nArgs; i++) {
    	if (!strCaseCmp(args[i], "keep"))
    	    return TRUE;
    	if (!strCaseCmp(args[i], "nokeep"))
    	    return FALSE;
    }
    return GetPrefKeepSearchDlogs();
}

/*
** Scans action argument list for arguments "wrap" or "nowrap" to
** determine search direction for search and replace actions.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static int searchWrap(int ignoreArgs, String *args, Cardinal *nArgs)
{
    int i;
    
    for (i=ignoreArgs; i<(int)*nArgs; i++) {
    	if (!strCaseCmp(args[i], "wrap"))
    	    return(TRUE);
    	if (!strCaseCmp(args[i], "nowrap"))
    	    return(FALSE);
    }
    return GetPrefSearchWraps();
}

/*
** Scans action argument list for arguments "literal", "case" or "regex" to
** determine search type for search and replace actions.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static int searchType(int ignoreArgs, String *args, Cardinal *nArgs)
{
    int i, tmpSearchType;
    
    for (i=ignoreArgs; i<(int)*nArgs; i++) {
      	if (StringToSearchType(args[i], &tmpSearchType))
    	    return tmpSearchType;
    }
    return GetPrefSearch();
}

/*
** Return a pointer to the string describing search direction for search action
** routine parameters given a callback XmAnyCallbackStruct pointed to by
** "callData", by checking if the shift key is pressed (for search callbacks).
*/
static char **shiftKeyToDir(XtPointer callData)
{
    static char *backwardParam[1] = {"backward"};
    static char *forwardParam[1] = {"forward"};
    if (((XmAnyCallbackStruct *)callData)->event->xbutton.state & ShiftMask)
    	return backwardParam;
    return forwardParam;
}

static void raiseCB(Widget w, WindowInfo *window, caddr_t callData)
{
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    RaiseFocusDocumentWindow(window, True /* always focus */);
}

static void openPrevCB(Widget w, char *name, caddr_t callData)
{
    char *params[1];
    Widget menu = MENU_WIDGET(w);
    
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    params[0] = name;
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "open",
    	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
    CheckCloseDim();
}

static void unloadTagsFileCB(Widget w, char *name, caddr_t callData)
{
    char *params[1];
    Widget menu = MENU_WIDGET(w);
    
    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    params[0] = name;
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "unload_tags_file",
	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void unloadTipsFileCB(Widget w, char *name, caddr_t callData)
{
    char *params[1];
#if XmVersion >= 1002
    Widget menu = XmGetPostedFromWidget(XtParent(w)); /* If menu is torn off */
#else
    Widget menu = w;
#endif
    
    params[0] = name;
    XtCallActionProc(WidgetToWindow(menu)->lastFocus, "unload_tips_file",
	    ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

/*
** strCaseCmp compares its arguments and returns 0 if the two strings
** are equal IGNORING case differences.  Otherwise returns 1.
*/
static int strCaseCmp(const char *str1, const char *str2)
{
    const char *c1, *c2;
    
    for (c1=str1, c2=str2; *c1!='\0' && *c2!='\0'; c1++, c2++)
    	if (toupper((unsigned char)*c1) != toupper((unsigned char)*c2))
    	    return 1;
    if (*c1 == *c2) {
        return(0);
	 }
    else {
        return(1);
	 }
}

/*
** Comparison function for sorting windows by title for the window menu.
** Windows are sorted by Untitled and then alphabetically by filename and
** then alphabetically by path.
*/
static int compareWindowNames(const void *windowA, const void *windowB)
{
    int rc;
    const WindowInfo *a = *((WindowInfo**)windowA);
    const WindowInfo *b = *((WindowInfo**)windowB);
    /* Untitled first */
    rc = a->filenameSet ==  b->filenameSet ? 0 : 
	 a->filenameSet && !b->filenameSet ? 1 : -1;
    if (rc != 0)
	 return rc;
    rc = strcmp(a->filename, b->filename);
    if (rc != 0)
	 return rc;
    rc = strcmp(a->path, b->path);
    return rc;
}

/*
** Create popup for right button programmable menu
*/
Widget CreateBGMenu(WindowInfo *window)
{
    Arg args[1];
    
    /* There is still some mystery here.  It's important to get the XmNmenuPost
       resource set to the correct menu button, or the menu will not post
       properly, but there's also some danger that it will take over the entire
       button and interfere with text widget translations which use the button
       with modifiers.  I don't entirely understand why it works properly now
       when it failed often in development, and certainly ignores the ~ syntax
       in translation event specifications. */
    XtSetArg(args[0], XmNmenuPost, GetPrefBGMenuBtn());
    return CreatePopupMenu(window->textArea, "bgMenu", args, 1);
}

/*
** Create context popup menu for tabs & tab bar
*/
Widget CreateTabContextMenu(Widget parent, WindowInfo *window)
{
    Widget   menu;
    Arg      args[8];
    int      n;

    n = 0;
    XtSetArg(args[n], XmNtearOffModel, XmTEAR_OFF_DISABLED); n++;
    menu = CreatePopupMenu(parent, "tabContext", args, n);
    
    createMenuItem(menu, "new", "New Tab", 0, doTabActionCB, "new_tab", SHORT);
    createMenuItem(menu, "close", "Close Tab", 0, doTabActionCB, "close", SHORT);
    createMenuSeparator(menu, "sep1", SHORT);
    window->contextDetachDocumentItem = createMenuItem(menu, "detach",
            "Detach Tab", 0, doTabActionCB, "detach_document", SHORT);
    XtSetSensitive(window->contextDetachDocumentItem, False);
    window->contextMoveDocumentItem = createMenuItem(menu, "attach", 
            "Move Tab To...", 0, doTabActionCB, "move_document_dialog", SHORT);
    
    return menu;
}

/*
** Add a translation to the text widget to trigger the background menu using
** the mouse-button + modifier combination specified in the resource:
** nedit.bgMenuBtn.
*/
void AddBGMenuAction(Widget widget)
{
    static XtTranslations table = NULL;

    if (table == NULL) {
	char translations[MAX_ACCEL_LEN + 25];
	sprintf(translations, "%s: post_window_bg_menu()\n",GetPrefBGMenuBtn());
    	table = XtParseTranslationTable(translations);
    }
    XtOverrideTranslations(widget, table);
}

static void bgMenuPostAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window = WidgetToWindow(w);
    
    /* The Motif popup handling code BLOCKS events while the menu is posted,
       including the matching btn-up events which complete various dragging
       operations which it may interrupt.  Cancel to head off problems */
    XtCallActionProc(window->lastFocus, "process_cancel", event, NULL, 0);
    
    /* Pop up the menu */
    XmMenuPosition(window->bgMenuPane, (XButtonPressedEvent *)event);
    XtManageChild(window->bgMenuPane); 
    
    /* 
       These statements have been here for a very long time, but seem 
       unnecessary and are even dangerous: when any of the lock keys are on,
       Motif thinks it shouldn't display the background menu, but this 
       callback is called anyway. When we then grab the focus and force the
       menu to be drawn, bad things can happen (like a total lockup of the X
       server).
       
       XtPopup(XtParent(window->bgMenuPane), XtGrabNonexclusive); 
       XtMapWidget(XtParent(window->bgMenuPane));
       XtMapWidget(window->bgMenuPane);  
    */
}

void AddTabContextMenuAction(Widget widget)
{
    static XtTranslations table = NULL;

    if (table == NULL) {
	char *translations = "<Btn3Down>: post_tab_context_menu()\n";
    	table = XtParseTranslationTable(translations);
    }
    XtOverrideTranslations(widget, table);
}

/*
** action procedure for posting context menu of tabs
*/
static void tabMenuPostAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    WindowInfo *window;
    XButtonPressedEvent *xbutton = (XButtonPressedEvent *)event;
    Widget wgt;
    
    /* Determine if the context menu was called from tabs or gutter,
       then stored the corresponding window info as userData of
       the popup menu pane, which will later be extracted by
       doTabActionCB() to act upon. When the context menu was called
       from the gutter, the active doc is assumed.
       
       Lesstif requires the action [to pupop the menu] to also be
       to the tabs, else nothing happed when right-click on tabs. 
       Even so, the action procedure sometime appear to be called 
       from the gutter even if users did right-click on the tabs.
       Here we try to cater for the uncertainty. */
    if (XtClass(w) == xrwsBubbleButtonWidgetClass)
	window = TabToWindow(w);
    else if (xbutton->subwindow) {
    	wgt = XtWindowToWidget(XtDisplay(w), xbutton->subwindow);
	window = TabToWindow(wgt);
    }
    else {
    	window = WidgetToWindow(w);
    }
    XtVaSetValues(window->tabMenuPane, XmNuserData, (XtPointer)window, NULL);
    
    /* The Motif popup handling code BLOCKS events while the menu is posted,
       including the matching btn-up events which complete various dragging
       operations which it may interrupt.  Cancel to head off problems */
    XtCallActionProc(window->lastFocus, "process_cancel", event, NULL, 0);
    
    /* Pop up the menu */
    XmMenuPosition(window->tabMenuPane, (XButtonPressedEvent *)event);
    XtManageChild(window->tabMenuPane); 
}

/*
** Event handler for restoring the input hint of menu tearoffs
** previously disabled in ShowHiddenTearOff() 
*/
static void tearoffMappedCB(Widget w, XtPointer clientData, XUnmapEvent *event)
{
    Widget shell = (Widget)clientData;
    XWMHints *wmHints;

    if (event->type != MapNotify)
    	return;

    /* restore the input hint previously disabled in ShowHiddenTearOff() */
    wmHints = XGetWMHints(TheDisplay, XtWindow(shell));
    wmHints->input = True;
    wmHints->flags |= InputHint;
    XSetWMHints(TheDisplay, XtWindow(shell), wmHints);
    XFree(wmHints);

    /* we only need to do this only */
    XtRemoveEventHandler(shell, StructureNotifyMask, False,
    	    (XtEventHandler)tearoffMappedCB, shell);
}

/*
** Redisplay (map) a hidden tearoff
*/
void ShowHiddenTearOff(Widget menuPane)
{
    Widget shell;
    
    if (!menuPane)
    	return;
    
    shell = XtParent(menuPane);
    if (!XmIsMenuShell(shell)) {
	XWindowAttributes winAttr;

	XGetWindowAttributes(XtDisplay(shell), XtWindow(shell), &winAttr);
	if (winAttr.map_state == IsUnmapped) {
            XWMHints *wmHints;

	    /* to workaround a problem where the remapped tearoffs
	       always receive the input focus insteads of the text
	       editing window, we disable the input hint of the 
	       tearoff shell temporarily. */
	    wmHints = XGetWMHints(XtDisplay(shell), XtWindow(shell));
	    wmHints->input = False;
	    wmHints->flags |= InputHint;
	    XSetWMHints(XtDisplay(shell), XtWindow(shell), wmHints);
	    XFree(wmHints);

    	    /* show the tearoff */
	    XtMapWidget(shell);

    	    /* the input hint will be restored when the tearoff
	       is mapped */
	    XtAddEventHandler(shell, StructureNotifyMask, False,
    		    (XtEventHandler)tearoffMappedCB, shell);
	}
    }
}

#ifdef SGI_CUSTOM
static void shortMenusCB(Widget w, WindowInfo *window, caddr_t callData)
{
    WindowInfo *win;
    int i, state = XmToggleButtonGetState(w);
    Widget parent;

    window = WidgetToWindow(w);

    HidePointerOnKeyedEvent(WidgetToWindow(MENU_WIDGET(w))->lastFocus,
            ((XmAnyCallbackStruct *)callData)->event);
    /* Set the preference */
    SetPrefShortMenus(state);
    
    /* Re-create the menus for all windows */
    for (win=WindowList; win!=NULL; win=win->next) {
    	for (i=0; i<win->nToggleShortItems; i++) {
    	    if (state)
    	    	XtUnmanageChild(win->toggleShortItems[i]);
    	    else
    	    	XtManageChild(win->toggleShortItems[i]);
    	}
    }
    if (GetPrefShortMenus())
    	SaveNEditPrefs(window->shell, True);
}

static void addToToggleShortList(Widget w)
{
    if (ShortMenuWindow->nToggleShortItems >= MAX_SHORTENED_ITEMS) {
    	fprintf(stderr,"nedit, internal error: increase MAX_SHORTENED_ITEMS\n");
    	return;
    }
    ShortMenuWindow->toggleShortItems[ShortMenuWindow->nToggleShortItems++] = w;
}   	       

/*
** Present the user a dialog for specifying whether or not a short
** menu mode preference should be applied toward the default setting.
** Return True if user requested to reset and save the default value.
** If operation was canceled, will return toggle widget "w" to it's 
** original (opposite) state.
*/
static int shortPrefAskDefault(Widget parent, Widget w, const char *settingName)
{
    char msg[100] = "";
    
    if (!GetPrefShortMenus()) {
    	return False;
    }
    
    sprintf(msg, "%s: %s\nSave as default for future windows as well?",
    	    settingName, XmToggleButtonGetState(w) ? "On" : "Off");
    switch (DialogF (DF_QUES, parent, 3, "Save Default", msg, "Yes", "No",
            "Cancel"))
    {
        case 1: /* yes */
            return True;
        case 2: /* no */
           return False;
        case 3: /* cancel */
            XmToggleButtonSetState(w, !XmToggleButtonGetState(w), False);
            return False;
    }
    return False; /* not reached */
}
#endif
