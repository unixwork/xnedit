/*******************************************************************************
*                                                                              *
* preference.h -- Nirvana Editor Preferences Header File                       *
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

#ifndef NEDIT_PREFERENCES_H_INCLUDED
#define NEDIT_PREFERENCES_H_INCLUDED

#include "nedit.h"

#include <X11/Intrinsic.h>
#include <X11/Xresource.h>
#include <Xm/Xm.h>
#include <X11/Xlib.h>

#define PLAIN_LANGUAGE_MODE -1

/* maximum number of language modes allowed */
#define MAX_LANGUAGE_MODES 127

#define MAX_TITLE_FORMAT_LEN 50

/* Identifiers for individual fonts in the help fonts list */
enum helpFonts {HELP_FONT, BOLD_HELP_FONT, ITALIC_HELP_FONT,
    BOLD_ITALIC_HELP_FONT, FIXED_HELP_FONT, BOLD_FIXED_HELP_FONT,
    ITALIC_FIXED_HELP_FONT, BOLD_ITALIC_FIXED_HELP_FONT, HELP_LINK_FONT,
    H1_HELP_FONT, H2_HELP_FONT, H3_HELP_FONT, NUM_HELP_FONTS
};

XrmDatabase CreateNEditPrefDB(int *argcInOut, char **argvInOut);
void RestoreNEditPrefs(XrmDatabase prefDB, XrmDatabase appDB);
void SaveNEditPrefs(Widget parent, int quietly);
void ImportPrefFile(const char *filename, int convertOld);
void MarkPrefsChanged(void);
int CheckPrefsChangesSaved(Widget dialogParent);
void SetPrefWrap(WrapStyle state);
WrapStyle GetPrefWrap(int langMode);
void SetPrefWrapMargin(int margin);
int GetPrefWrapMargin(void);
void SetPrefSearchDlogs(int state);
int GetPrefSearchDlogs(void);
void SetPrefKeepSearchDlogs(int state);
int GetPrefKeepSearchDlogs(void);
void SetPrefSearchWraps(int state);
int GetPrefSearchWraps(void);
void SetPrefStatsLine(int state);
int GetPrefStatsLine(void);
void SetPrefISearchLine(int state);
int GetPrefISearchLine(void);
void SetPrefTabBar(int state);
int GetPrefTabBar(void);
void SetPrefSortTabs(int state);
int GetPrefSortTabs(void);
void SetPrefTabBarHideOne(int state);
int GetPrefTabBarHideOne(void);
void SetPrefGlobalTabNavigate(int state);
int GetPrefGlobalTabNavigate(void);
void SetPrefToolTips(int state);
int GetPrefToolTips(void);
void SetPrefLineNums(int state);
int GetPrefLineNums(void);
void SetPrefShowPathInWindowsMenu(int state);
int GetPrefShowPathInWindowsMenu(void);
void SetPrefWarnFileMods(int state);
int GetPrefWarnFileMods(void);
void SetPrefWarnRealFileMods(int state);
int GetPrefWarnRealFileMods(void);
void SetPrefWarnExit(int state);
int GetPrefWarnExit(void);
void SetPrefSearch(int searchType);
int GetPrefSearch(void);
void SetPrefAutoIndent(IndentStyle state);
IndentStyle GetPrefAutoIndent(int langMode);
void SetPrefAutoSave(int state);
int GetPrefAutoSave(void);
void SetPrefSaveOldVersion(int state);
int GetPrefSaveOldVersion(void);
void SetPrefRows(int nRows);
int GetPrefRows(void);
void SetPrefCols(int nCols);
int GetPrefCols(void);
void SetPrefTabDist(int tabDist);
int GetPrefTabDist(int langMode);
void SetPrefEmTabDist(int tabDist);
int GetPrefEmTabDist(int langMode);
void SetPrefInsertTabs(int state);
int GetPrefInsertTabs(void);
void SetPrefShowMatching(ShowMatchingStyle state);
ShowMatchingStyle GetPrefShowMatching(void);
void SetPrefMatchSyntaxBased(int state);
int GetPrefMatchSyntaxBased(void);
void SetPrefHighlightSyntax(Boolean state);
Boolean GetPrefHighlightSyntax(void);
void SetPrefBacklightChars(int state);
int GetPrefBacklightChars(void);
void SetPrefBacklightCharTypes(char *types);
char *GetPrefBacklightCharTypes(void);
void SetPrefRepositionDialogs(int state);
int GetPrefRepositionDialogs(void);
void SetPrefAutoScroll(int state);
int GetPrefAutoScroll(void);
int GetVerticalAutoScroll(void);
void SetPrefAppendLF(int state);
int GetPrefAppendLF(void);
void SetPrefSortOpenPrevMenu(int state);
int GetPrefSortOpenPrevMenu(void);
char *GetPrefTagFile(void);
int GetPrefSmartTags(void);
void SetPrefSmartTags(int state);
int GetPrefAlwaysCheckRelTagsSpecs(void);
void SetPrefFont(char *fontName);
void SetPrefBoldFont(char *fontName);
void SetPrefItalicFont(char *fontName);
void SetPrefBoldItalicFont(char *fontName);
char *GetPrefFontName(void);
char *GetPrefBoldFontName(void);
char *GetPrefItalicFontName(void);
char *GetPrefBoldItalicFontName(void);
XmFontList GetPrefFontList(void);
XFontStruct *GetPrefBoldFont(void);
XFontStruct *GetPrefItalicFont(void);
XFontStruct *GetPrefBoldItalicFont(void);
char *GetPrefTooltipBgColor(void);
char *GetPrefHelpFontName(int index);
char *GetPrefHelpLinkColor(void);
char *GetPrefColorName(int colorIndex);
void SetPrefColorName(int colorIndex, const char *color);
void SetPrefShell(const char *shell);
const char* GetPrefShell(void);
char *GetPrefGeometry(void);
char *GetPrefServerName(void);
char *GetPrefBGMenuBtn(void);
void RowColumnPrefDialog(Widget parent);
void TabsPrefDialog(Widget parent, WindowInfo *forWindow);
void WrapMarginDialog(Widget parent, WindowInfo *forWindow);
int GetPrefMapDelete(void);
int GetPrefStdOpenDialog(void);
char *GetPrefDelimiters(void);
int GetPrefMaxPrevOpenFiles(void);
int GetPrefTypingHidesPointer(void);
#ifdef SGI_CUSTOM
void SetPrefShortMenus(int state);
int GetPrefShortMenus(void);
#endif
void SelectShellDialog(Widget parent, WindowInfo* forWindow);
void EditLanguageModes(void);
void ChooseFonts(WindowInfo *window, int forWindow);
void ChooseColors(WindowInfo *window);
char *LanguageModeName(int mode);
char *GetWindowDelimiters(const WindowInfo *window);
int ReadNumericField(char **inPtr, int *value);
char *ReadSymbolicField(char **inPtr);
char *ReadSymbolicFieldTextWidget(Widget textW, const char *fieldName, int silent);
int ReadQuotedString(char **inPtr, char **errMsg, char **string);
char *MakeQuotedString(const char *string);
char *EscapeSensitiveChars(const char *string);
int SkipDelimiter(char **inPtr, char **errMsg);
int SkipOptSeparator(char separator, char **inPtr);
int ParseError(Widget toDialog, const char *stringStart, const char *stoppedAt,
	const char *errorIn, const char *message);
int AllocatedStringsDiffer(const char *s1, const char *s2);
void SetLanguageMode(WindowInfo *window, int mode, int forceNewDefaults);
int FindLanguageMode(const char *languageName);
void UnloadLanguageModeTipsFile(WindowInfo *window);
void DetermineLanguageMode(WindowInfo *window, int forceNewDefaults);
Widget CreateLanguageModeMenu(Widget parent, XtCallbackProc cbProc,
	void *cbArg);
void SetLangModeMenu(Widget optMenu, const char *modeName);
void CreateLanguageModeSubMenu(WindowInfo* window, const Widget parent,
        const char* name, const char* label, char mnemonic);
void SetPrefFindReplaceUsesSelection(int state);
int GetPrefFindReplaceUsesSelection(void);
int GetPrefStickyCaseSenseBtn(void);
void SetPrefBeepOnSearchWrap(int state);
int GetPrefBeepOnSearchWrap(void);
#ifdef REPLACE_SCOPE
void SetPrefReplaceDefScope(int scope);
int GetPrefReplaceDefScope(void);
#endif
void SetPrefTitleFormat(const char* format);
const char* GetPrefTitleFormat(void);
int GetPrefOverrideVirtKeyBindings(void);
int GetPrefTruncSubstitution(void);
int GetPrefOpenInTab(void);
void SetPrefUndoModifiesSelection(Boolean);
void SetPrefOpenInTab(int state);
Boolean GetPrefUndoModifiesSelection(void);
Boolean GetPrefFocusOnRaise(void);
Boolean GetPrefHonorSymlinks(void);
Boolean GetPrefForceOSConversion(void);
void SetPrefFocusOnRaise(Boolean);

#endif /* NEDIT_PREFERENCES_H_INCLUDED */
