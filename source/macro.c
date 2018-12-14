/*******************************************************************************
*                                                                              *
* macro.c -- Macro file processing, learn/replay, and built-in macro           *
*            subroutines                                                       *
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
* April, 1997                                                                  *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "macro.h"
#include "textBuf.h"
#include "text.h"
#include "nedit.h"
#include "window.h"
#include "preferences.h"
#include "interpret.h"
#include "parse.h"
#include "search.h"
#include "server.h"
#include "shell.h"
#include "smartIndent.h"
#include "userCmds.h"
#include "selection.h"
#include "../util/rbTree.h"
#include "tags.h"
#include "calltips.h"
#include "../util/DialogF.h"
#include "../util/misc.h"
#include "../util/fileUtils.h"
#include "../util/utils.h"
#include "../util/getfiles.h"
#include "highlight.h"
#include "highlightData.h"
#include "rangeset.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef VMS
#include <limits.h>
#include "../util/VMSparam.h"
#include <types.h>
#include <stat.h>
#include <unixio.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#include <fcntl.h>
#endif /*VMS*/
#include <inttypes.h>

#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/CutPaste.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h>
#include <Xm/List.h>
#include <Xm/ToggleB.h>
#include <Xm/DialogS.h>
#include <Xm/MessageB.h>
#include <Xm/SelectioB.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>
#include <Xm/Separator.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* Maximum number of actions in a macro and args in 
   an action (to simplify the reader) */
#define MAX_MACRO_ACTIONS 1024
#define MAX_ACTION_ARGS 40

/* How long to wait (msec) before putting up Macro Command banner */
#define BANNER_WAIT_TIME 6000

/* The following definitions cause an exit from the macro with a message */
/* added if (1) to remove compiler warnings on solaris */
#define M_FAILURE(s)  do { *errMsg = s; if (1) return False; } while (0)
#define M_STR_ALLOC_ASSERT(xDV) do { if (xDV.tag == STRING_TAG && !xDV.val.str.rep) { *errMsg = "Failed to allocate value: %s"; return(False); } } while (0)
#define M_ARRAY_INSERT_FAILURE() M_FAILURE("array element failed to insert: %s")

/* Data attached to window during shell command execution with
   information for controling and communicating with the process */
typedef struct {
    XtIntervalId bannerTimeoutID;
    XtWorkProcId continueWorkProcID;
    char bannerIsUp;
    char closeOnCompletion;
    Program *program;
    RestartData *context;
    Widget dialog;
} macroCmdInfo;

/* Widgets and global data for Repeat dialog */
typedef struct {
    WindowInfo *forWindow;
    char *lastCommand;
    Widget shell, repeatText, lastCmdToggle;
    Widget inSelToggle, toEndToggle;
} repeatDialog;

static void cancelLearn(void);
static void runMacro(WindowInfo *window, Program *prog);
static void finishMacroCmdExecution(WindowInfo *window);
static void repeatOKCB(Widget w, XtPointer clientData, XtPointer callData);
static void repeatApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static int doRepeatDialogAction(repeatDialog *rd, XEvent *event);
static void repeatCancelCB(Widget w, XtPointer clientData, XtPointer callData);
static void repeatDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void learnActionHook(Widget w, XtPointer clientData, String actionName,
	XEvent *event, String *params, Cardinal *numParams);
static void lastActionHook(Widget w, XtPointer clientData, String actionName,
	XEvent *event, String *params, Cardinal *numParams);
static char *actionToString(Widget w, char *actionName, XEvent *event,
	String *params, Cardinal numParams);
static int isMouseAction(const char *action);
static int isRedundantAction(const char *action);
static int isIgnoredAction(const char *action);
static int readCheckMacroString(Widget dialogParent, char *string,
	WindowInfo *runWindow, const char *errIn, char **errPos);
static void bannerTimeoutProc(XtPointer clientData, XtIntervalId *id);
static Boolean continueWorkProc(XtPointer clientData);
static int escapeStringChars(char *fromString, char *toString);
static int escapedStringLength(char *string);
static int lengthMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int minMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int maxMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int focusWindowMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int getRangeMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int getCharacterMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int replaceRangeMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int replaceSelectionMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int getSelectionMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int validNumberMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int replaceInStringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int replaceSubstringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int readFileMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int writeFileMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int appendFileMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int writeOrAppendFile(int append, WindowInfo *window,
    	DataValue *argList, int nArgs, DataValue *result, char **errMsg);
static int substringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int toupperMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int tolowerMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int stringToClipboardMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int clipboardToStringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int searchMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int searchStringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int setCursorPosMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int beepMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int selectMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int selectRectangleMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int tPrintMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int getenvMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int shellCmdMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int dialogMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static void dialogBtnCB(Widget w, XtPointer clientData, XtPointer callData);
static void dialogCloseCB(Widget w, XtPointer clientData, XtPointer callData);
#ifdef LESSTIF_VERSION
static void dialogEscCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *cont);
#endif /* LESSTIF_VERSION */
static int stringDialogMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static void stringDialogBtnCB(Widget w, XtPointer clientData,
    	XtPointer callData);
static void stringDialogCloseCB(Widget w, XtPointer clientData,
    	XtPointer callData);
#ifdef LESSTIF_VERSION
static void stringDialogEscCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *cont);
#endif /* LESSTIF_VERSION */
static int calltipMS(WindowInfo *window, DataValue *argList, int nArgs,
       DataValue *result, char **errMsg);
static int killCalltipMS(WindowInfo *window, DataValue *argList, int nArgs,
       DataValue *result, char **errMsg);
/* T Balinski */
static int listDialogMS(WindowInfo *window, DataValue *argList, int nArgs,
	DataValue *result, char **errMsg);
static void listDialogBtnCB(Widget w, XtPointer clientData,
	XtPointer callData);
static void listDialogCloseCB(Widget w, XtPointer clientData,
	XtPointer callData);
/* T Balinski End */
#ifdef LESSTIF_VERSION
static void listDialogEscCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *cont);
#endif /* LESSTIF_VERSION */
static int stringCompareMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int splitMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
/* DISASBLED for 5.4 
static int setBacklightStringMS(WindowInfo *window, DataValue *argList,
	int nArgs, DataValue *result, char **errMsg);
*/
static int cursorMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int lineMV(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg);
static int columnMV(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg);
static int fileNameMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int filePathMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int lengthMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int selectionStartMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int selectionEndMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int selectionLeftMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int selectionRightMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int statisticsLineMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int incSearchLineMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int showLineNumbersMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int autoIndentMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int wrapTextMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int highlightSyntaxMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int makeBackupCopyMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int incBackupMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int showMatchingMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int matchSyntaxBasedMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int overTypeModeMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int readOnlyMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int lockedMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int fileFormatMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int fontNameMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int fontNameItalicMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int fontNameBoldMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int fontNameBoldItalicMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int subscriptSepMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int minFontWidthMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int maxFontWidthMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int wrapMarginMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int topLineMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int numDisplayLinesMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int displayWidthMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int activePaneMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int nPanesMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int emptyArrayMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int serverNameMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg);
static int tabDistMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int emTabDistMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int useTabsMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int modifiedMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int languageModeMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int calltipIDMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg);
static int readSearchArgs(DataValue *argList, int nArgs, int*searchDirection,
	int *searchType, int *wrap, char **errMsg);
static int wrongNArgsErr(char **errMsg);
static int tooFewArgsErr(char **errMsg);
static int strCaseCmp(char *str1, char *str2);
static int readIntArg(DataValue dv, int *result, char **errMsg);
static int readStringArg(DataValue dv, char **result, char *stringStorage,
    	char **errMsg);
/* DISABLED FOR 5.4
static int backlightStringMV(WindowInfo *window, DataValue *argList,
	int nArgs, DataValue *result, char **errMsg);
*/
static int rangesetListMV(WindowInfo *window, DataValue *argList,
	int nArgs, DataValue *result, char **errMsg);
static int versionMV(WindowInfo* window, DataValue* argList, int nArgs,
        DataValue* result, char** errMsg);
static int rangesetCreateMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetDestroyMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetGetByNameMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetAddMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetSubtractMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetInvertMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetInfoMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetRangeMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg);
static int rangesetIncludesPosMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg);
static int rangesetSetColorMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg);
static int rangesetSetNameMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg);
static int rangesetSetModeMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg);

static int fillPatternResult(DataValue *result, char **errMsg, WindowInfo *window,
        char *patternName, Boolean preallocatedPatternName, Boolean includeName,
        char *styleName, int bufferPos);
static int getPatternByNameMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg);
static int getPatternAtPosMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg);

static int fillStyleResult(DataValue *result, char **errMsg,
        WindowInfo *window, char *styleName, Boolean preallocatedStyleName,
        Boolean includeName, int patCode, int bufferPos);
static int getStyleByNameMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg);
static int getStyleAtPosMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg);
static int filenameDialogMS(WindowInfo* window, DataValue* argList, int nArgs,
        DataValue* result, char** errMsg);

/* Built-in subroutines and variables for the macro language */
static BuiltInSubr MacroSubrs[] = {lengthMS, getRangeMS, tPrintMS,
        dialogMS, stringDialogMS, replaceRangeMS, replaceSelectionMS,
        setCursorPosMS, getCharacterMS, minMS, maxMS, searchMS,
        searchStringMS, substringMS, replaceSubstringMS, readFileMS,
        writeFileMS, appendFileMS, beepMS, getSelectionMS, validNumberMS,
        replaceInStringMS, selectMS, selectRectangleMS, focusWindowMS,
        shellCmdMS, stringToClipboardMS, clipboardToStringMS, toupperMS,
        tolowerMS, listDialogMS, getenvMS,
        stringCompareMS, splitMS, calltipMS, killCalltipMS,
/* DISABLED for 5.4        setBacklightStringMS,*/
        rangesetCreateMS, rangesetDestroyMS,
        rangesetAddMS, rangesetSubtractMS, rangesetInvertMS,
        rangesetInfoMS, rangesetRangeMS, rangesetIncludesPosMS,
        rangesetSetColorMS, rangesetSetNameMS, rangesetSetModeMS,
        rangesetGetByNameMS,
        getPatternByNameMS, getPatternAtPosMS,
        getStyleByNameMS, getStyleAtPosMS, filenameDialogMS
    };
#define N_MACRO_SUBRS (sizeof MacroSubrs/sizeof *MacroSubrs)
static const char *MacroSubrNames[N_MACRO_SUBRS] = {"length", "get_range", "t_print",
        "dialog", "string_dialog", "replace_range", "replace_selection",
        "set_cursor_pos", "get_character", "min", "max", "search",
        "search_string", "substring", "replace_substring", "read_file",
        "write_file", "append_file", "beep", "get_selection", "valid_number",
        "replace_in_string", "select", "select_rectangle", "focus_window",
        "shell_command", "string_to_clipboard", "clipboard_to_string",
        "toupper", "tolower", "list_dialog", "getenv",
        "string_compare", "split", "calltip", "kill_calltip",
/* DISABLED for 5.4        "set_backlight_string", */
        "rangeset_create", "rangeset_destroy",
        "rangeset_add", "rangeset_subtract", "rangeset_invert",
        "rangeset_info", "rangeset_range", "rangeset_includes",
        "rangeset_set_color", "rangeset_set_name", "rangeset_set_mode",
        "rangeset_get_by_name",
        "get_pattern_by_name", "get_pattern_at_pos",
        "get_style_by_name", "get_style_at_pos", "filename_dialog"
    };
static BuiltInSubr SpecialVars[] = {cursorMV, lineMV, columnMV,
        fileNameMV, filePathMV, lengthMV, selectionStartMV, selectionEndMV,
        selectionLeftMV, selectionRightMV, wrapMarginMV, tabDistMV,
        emTabDistMV, useTabsMV, languageModeMV, modifiedMV,
        statisticsLineMV, incSearchLineMV, showLineNumbersMV,
        autoIndentMV, wrapTextMV, highlightSyntaxMV,
        makeBackupCopyMV, incBackupMV, showMatchingMV, matchSyntaxBasedMV,
        overTypeModeMV, readOnlyMV, lockedMV, fileFormatMV,
        fontNameMV, fontNameItalicMV,
        fontNameBoldMV, fontNameBoldItalicMV, subscriptSepMV,
        minFontWidthMV, maxFontWidthMV, topLineMV, numDisplayLinesMV,
        displayWidthMV, activePaneMV, nPanesMV, emptyArrayMV,
        serverNameMV, calltipIDMV,
/* DISABLED for 5.4        backlightStringMV, */
	rangesetListMV, versionMV
    };
#define N_SPECIAL_VARS (sizeof SpecialVars/sizeof *SpecialVars)
static const char *SpecialVarNames[N_SPECIAL_VARS] = {"$cursor", "$line", "$column",
        "$file_name", "$file_path", "$text_length", "$selection_start",
        "$selection_end", "$selection_left", "$selection_right",
        "$wrap_margin", "$tab_dist", "$em_tab_dist", "$use_tabs",
        "$language_mode", "$modified",
        "$statistics_line", "$incremental_search_line", "$show_line_numbers",
        "$auto_indent", "$wrap_text", "$highlight_syntax",
        "$make_backup_copy", "$incremental_backup", "$show_matching", "$match_syntax_based",
        "$overtype_mode", "$read_only", "$locked", "$file_format",
        "$font_name", "$font_name_italic",
        "$font_name_bold", "$font_name_bold_italic", "$sub_sep",
        "$min_font_width", "$max_font_width", "$top_line", "$n_display_lines",
        "$display_width", "$active_pane", "$n_panes", "$empty_array",
        "$server_name", "$calltip_ID",
/* DISABLED for 5.4       "$backlight_string", */
        "$rangeset_list", "$VERSION"
    };

/* Global symbols for returning values from built-in functions */
#define N_RETURN_GLOBALS 5
enum retGlobalSyms {STRING_DIALOG_BUTTON, SEARCH_END, READ_STATUS,
	SHELL_CMD_STATUS, LIST_DIALOG_BUTTON};
static const char *ReturnGlobalNames[N_RETURN_GLOBALS] = {"$string_dialog_button",
    	"$search_end", "$read_status", "$shell_cmd_status",
	"$list_dialog_button"};
static Symbol *ReturnGlobals[N_RETURN_GLOBALS];

/* List of actions not useful when learning a macro sequence (also see below) */
static char* IgnoredActions[] = {"focusIn", "focusOut"};

/* List of actions intended to be attached to mouse buttons, which the user
   must be warned can't be recorded in a learn/replay sequence */
static const char* MouseActions[] = {"grab_focus", "extend_adjust", "extend_start",
	"extend_end", "secondary_or_drag_adjust", "secondary_adjust",
	"secondary_or_drag_start", "secondary_start", "move_destination",
	"move_to", "move_to_or_end_drag", "copy_to", "copy_to_or_end_drag",
	"exchange", "process_bdrag", "mouse_pan"};

/* List of actions to not record because they 
   generate further actions, more suitable for recording */
static const char* RedundantActions[] = {"open_dialog", "save_as_dialog",
    "revert_to_saved_dialog", "include_file_dialog", "load_macro_file_dialog",
    "load_tags_file_dialog", "find_dialog", "replace_dialog",
    "goto_line_number_dialog", "mark_dialog", "goto_mark_dialog",
    "control_code_dialog", "filter_selection_dialog", "execute_command_dialog",
    "repeat_dialog", "start_incremental_find"};

/* The last command executed (used by the Repeat command) */
static char *LastCommand = NULL;

/* The current macro to execute on Replay command */
static char *ReplayMacro = NULL;

/* Buffer where macro commands are recorded in Learn mode */
static textBuffer *MacroRecordBuf = NULL;

/* Action Hook id for recording actions for Learn mode */
static XtActionHookId MacroRecordActionHook = 0;

/* Window where macro recording is taking place */
static WindowInfo *MacroRecordWindow = NULL;

/* Arrays for translating escape characters in escapeStringChars */
static char ReplaceChars[] = "\\\"ntbrfav";
static char EscapeChars[] = "\\\"\n\t\b\r\f\a\v";

/*
** Install built-in macro subroutines and special variables for accessing
** editor information
*/
void RegisterMacroSubroutines(void)
{
    static DataValue subrPtr = {NO_TAG, {0}}, noValue = {NO_TAG, {0}};
    unsigned i;
    
    /* Install symbols for built-in routines and variables, with pointers
       to the appropriate c routines to do the work */
    for (i=0; i<N_MACRO_SUBRS; i++) {
    	subrPtr.val.subr = MacroSubrs[i];
    	InstallSymbol(MacroSubrNames[i], C_FUNCTION_SYM, subrPtr);
    }
    for (i=0; i<N_SPECIAL_VARS; i++) {
    	subrPtr.val.subr = SpecialVars[i];
    	InstallSymbol(SpecialVarNames[i], PROC_VALUE_SYM, subrPtr);
    }
    
    /* Define global variables used for return values, remember their
       locations so they can be set without a LookupSymbol call */
    for (i=0; i<N_RETURN_GLOBALS; i++)
    	ReturnGlobals[i] = InstallSymbol(ReturnGlobalNames[i], GLOBAL_SYM,
    	    	noValue);
}

#define MAX_LEARN_MSG_LEN ((2 * MAX_ACCEL_LEN) + 60)
void BeginLearn(WindowInfo *window)
{
    WindowInfo *win;
    XmString s;
    XmString xmFinish;
    XmString xmCancel;
    char *cFinish;
    char *cCancel;
    char message[MAX_LEARN_MSG_LEN];
    
    /* If we're already in learn mode, return */
    if (MacroRecordActionHook != 0)
    	return;
    
    /* dim the inappropriate menus and items, and undim finish and cancel */
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XtSetSensitive(win->learnItem, False);
    }
    SetSensitive(window, window->finishLearnItem, True);
    XtVaSetValues(window->cancelMacroItem, XmNlabelString,
    	    s=XmStringCreateSimple("Cancel Learn"), NULL);
    XmStringFree(s);
    SetSensitive(window, window->cancelMacroItem, True);
    
    /* Mark the window where learn mode is happening */
    MacroRecordWindow = window;
    
    /* Allocate a text buffer for accumulating the macro strings */
    MacroRecordBuf = BufCreate();
    
    /* Add the action hook for recording the actions */
    MacroRecordActionHook =
    	    XtAppAddActionHook(XtWidgetToApplicationContext(window->shell),
    	    learnActionHook, window);

    /* Extract accelerator texts from menu PushButtons */
    XtVaGetValues(window->finishLearnItem, XmNacceleratorText, &xmFinish, NULL);
    XtVaGetValues(window->cancelMacroItem, XmNacceleratorText, &xmCancel, NULL);

    /* Translate Motif strings to char* */
    cFinish = GetXmStringText(xmFinish);
    cCancel = GetXmStringText(xmCancel);

    /* Free Motif Strings */
    XmStringFree(xmFinish);
    XmStringFree(xmCancel);

    /* Create message */
    if (cFinish[0] == '\0') {
        if (cCancel[0] == '\0') {
            strncpy(message, "Learn Mode -- Use menu to finish or cancel",
                MAX_LEARN_MSG_LEN);
            message[MAX_LEARN_MSG_LEN - 1] = '\0';
        }
        else {
            sprintf(message,
                "Learn Mode -- Use menu to finish, press %s to cancel",
                cCancel);
        }
    }
    else {
        if (cCancel[0] == '\0') {
            sprintf(message,
                "Learn Mode -- Press %s to finish, use menu to cancel",
                cFinish);

        }
        else {
            sprintf(message,
                "Learn Mode -- Press %s to finish, %s to cancel",
                cFinish,
                cCancel);
        }
    }

    /* Free C-strings */
    NEditFree(cFinish);
    NEditFree(cCancel);

    /* Put up the learn-mode banner */
    SetModeMessage(window, message);    
}

void AddLastCommandActionHook(XtAppContext context)
{
    XtAppAddActionHook(context, lastActionHook, NULL);
}

void FinishLearn(void)
{
    WindowInfo *win;
    
    /* If we're not in learn mode, return */
    if (MacroRecordActionHook == 0)
    	return;
    
    /* Remove the action hook */
    XtRemoveActionHook(MacroRecordActionHook);
    MacroRecordActionHook = 0;
    
    /* Free the old learn/replay sequence */
    NEditFree(ReplayMacro);
    
    /* Store the finished action for the replay menu item */
    ReplayMacro = BufGetAll(MacroRecordBuf);
    
    /* Free the buffer used to accumulate the macro sequence */
    BufFree(MacroRecordBuf);
    
    /* Undim the menu items dimmed during learn */
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XtSetSensitive(win->learnItem, True);
    }
    if (IsTopDocument(MacroRecordWindow)) {
	XtSetSensitive(MacroRecordWindow->finishLearnItem, False);
	XtSetSensitive(MacroRecordWindow->cancelMacroItem, False);
    }
    
    /* Undim the replay and paste-macro buttons */
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
    	XtSetSensitive(win->replayItem, True);
    }
    DimPasteReplayBtns(True);
    
    /* Clear learn-mode banner */
    ClearModeMessage(MacroRecordWindow);
}

/*
** Cancel Learn mode, or macro execution (they're bound to the same menu item)
*/
void CancelMacroOrLearn(WindowInfo *window)
{
    if (MacroRecordActionHook != 0)
    	cancelLearn();
    else if (window->macroCmdData != NULL)
    	AbortMacroCommand(window);
}

static void cancelLearn(void)
{
    WindowInfo *win;
    
    /* If we're not in learn mode, return */
    if (MacroRecordActionHook == 0)
    	return;

    /* Remove the action hook */
    XtRemoveActionHook(MacroRecordActionHook);
    MacroRecordActionHook = 0;
    
    /* Free the macro under construction */
    BufFree(MacroRecordBuf);
    
    /* Undim the menu items dimmed during learn */
    for (win=WindowList; win!=NULL; win=win->next) {
    	if (!IsTopDocument(win))
	    continue;
	XtSetSensitive(win->learnItem, True);
    }
    if (IsTopDocument(MacroRecordWindow)) {
	XtSetSensitive(MacroRecordWindow->finishLearnItem, False);
	XtSetSensitive(MacroRecordWindow->cancelMacroItem, False);
    }
    
    /* Clear learn-mode banner */
    ClearModeMessage(MacroRecordWindow);
}

/*
** Execute the learn/replay sequence stored in "window"
*/
void Replay(WindowInfo *window)
{
    Program *prog;
    char *errMsg, *stoppedAt;

    /* Verify that a replay macro exists and it's not empty and that */
    /* we're not already running a macro */
    if (ReplayMacro != NULL &&
            ReplayMacro[0] != 0 &&
            window->macroCmdData == NULL) {
        /* Parse the replay macro (it's stored in text form) and compile it into
        an executable program "prog" */
        prog = ParseMacro(ReplayMacro, &errMsg, &stoppedAt);
        if (prog == NULL) {
            fprintf(stderr,
                "NEdit internal error, learn/replay macro syntax error: %s\n",
                errMsg);
            return;
        }

        /* run the executable program */
        runMacro(window, prog);
    }
}

/*
**  Read the initial NEdit macro file if one exists.
*/
void ReadMacroInitFile(WindowInfo *window)
{
    const char* autoloadName = GetRCFileName(AUTOLOAD_NM);
    static int initFileLoaded = False;

    /*  GetRCFileName() might return NULL if an error occurs during
        creation of the preference file directory. */
    if (autoloadName != NULL && !initFileLoaded)
    {
        ReadMacroFile(window, autoloadName, False);
        initFileLoaded = True;
    }
}

/*
** Read an NEdit macro file.  Extends the syntax of the macro parser with
** define keyword, and allows intermixing of defines with immediate actions.
*/
int ReadMacroFile(WindowInfo *window, const char *fileName, int warnNotExist)
{
    int result;
    char *fileString;

    /* read-in macro file and force a terminating \n, to prevent syntax
    ** errors with statements on the last line
    */
    fileString = ReadAnyTextFile(fileName, True);
    if (fileString == NULL){
        if (errno != ENOENT || warnNotExist)
        {
            DialogF(DF_ERR, window->shell, 1, "Read Macro",
                    "Error reading macro file %s: %s", "OK", fileName,
#ifdef VMS
                    strerror(errno, vaxc$errno));
#else
                    strerror(errno));
#endif
        }
        return False;                    
    }

    /* Parse fileString */
    result = readCheckMacroString(window->shell, fileString, window, fileName,
	    NULL);
    NEditFree(fileString);
    return result;
}

/*
** Parse and execute a macro string including macro definitions.  Report
** parsing errors in a dialog posted over window->shell.
*/
int ReadMacroString(WindowInfo *window, char *string, const char *errIn)
{   
    return readCheckMacroString(window->shell, string, window, errIn, NULL);
}  

/*
** Check a macro string containing definitions for errors.  Returns True
** if macro compiled successfully.  Returns False and puts up
** a dialog explaining if macro did not compile successfully.
*/  
int CheckMacroString(Widget dialogParent, char *string, const char *errIn,
	char **errPos)
{
    return readCheckMacroString(dialogParent, string, NULL, errIn, errPos);
}    

/*
** Parse and optionally execute a macro string including macro definitions.
** Report parsing errors in a dialog posted over dialogParent, using the
** string errIn to identify the entity being parsed (filename, macro string,
** etc.).  If runWindow is specified, runs the macro against the window.  If
** runWindow is passed as NULL, does parse only.  If errPos is non-null, 
** returns a pointer to the error location in the string.
*/
static int readCheckMacroString(Widget dialogParent, char *string,
	WindowInfo *runWindow, const char *errIn, char **errPos)
{
    char *stoppedAt, *inPtr, *namePtr, *errMsg;
    char subrName[MAX_SYM_LEN];
    Program *prog;
    Symbol *sym;
    DataValue subrPtr;
    Stack* progStack = (Stack*) NEditMalloc(sizeof(Stack));
    progStack->top = NULL;
    progStack->size = 0;

    inPtr = string;
    while (*inPtr != '\0') {
    	
    	/* skip over white space and comments */
	while (*inPtr==' ' || *inPtr=='\t' || *inPtr=='\n'|| *inPtr=='#') {
	    if (*inPtr == '#')
	    	while (*inPtr != '\n' && *inPtr != '\0') inPtr++;
	    else
	    	inPtr++;
	}
	if (*inPtr == '\0')
	    break;
	
	/* look for define keyword, and compile and store defined routines */
	if (!strncmp(inPtr, "define", 6) && (inPtr[6]==' ' || inPtr[6]=='\t')) {
	    inPtr += 6;
	    inPtr += strspn(inPtr, " \t\n");
	    namePtr = subrName;
            while ((namePtr < &subrName[MAX_SYM_LEN - 1])
                   && (isalnum((unsigned char)*inPtr) || *inPtr == '_')) {
                *namePtr++ = *inPtr++;
            }
            *namePtr = '\0';
            if (isalnum((unsigned char)*inPtr) || *inPtr == '_') {
                return ParseError(dialogParent, string, inPtr, errIn,
                                  "subroutine name too long");
            }
	    inPtr += strspn(inPtr, " \t\n");
	    if (*inPtr != '{') {
	    	if (errPos != NULL) *errPos = stoppedAt;
		return ParseError(dialogParent, string, inPtr,
	    	    	errIn, "expected '{'");
	    }
	    prog = ParseMacro(inPtr, &errMsg, &stoppedAt);
	    if (prog == NULL) {
	    	if (errPos != NULL) *errPos = stoppedAt;
	    	return ParseError(dialogParent, string, stoppedAt,
	    	    	errIn, errMsg);
	    }
	    if (runWindow != NULL) {
		sym = LookupSymbol(subrName);
		if (sym == NULL) {
		    subrPtr.val.prog = prog;
		    subrPtr.tag = NO_TAG;
		    sym = InstallSymbol(subrName, MACRO_FUNCTION_SYM, subrPtr);
		} else {
	    	    if (sym->type == MACRO_FUNCTION_SYM)
		    	FreeProgram(sym->value.val.prog);
		    else
			sym->type = MACRO_FUNCTION_SYM;
	    	    sym->value.val.prog = prog;
		}
	    }
	    inPtr = stoppedAt;
	
	/* Parse and execute immediate (outside of any define) macro commands
	   and WAIT for them to finish executing before proceeding.  Note that
	   the code below is not perfect.  If you interleave code blocks with
	   definitions in a file which is loaded from another macro file, it
	   will probably run the code blocks in reverse order! */
	} else {
	    prog = ParseMacro(inPtr, &errMsg, &stoppedAt);
	    if (prog == NULL) {
                if (errPos != NULL) {
                    *errPos = stoppedAt;
                }

    	    	return ParseError(dialogParent, string, stoppedAt,
	    	    	errIn, errMsg);
	    }

	    if (runWindow != NULL) {
                XEvent nextEvent;
		if (runWindow->macroCmdData == NULL) {
	            runMacro(runWindow, prog);
		    while (runWindow->macroCmdData != NULL) {
			XtAppNextEvent(XtWidgetToApplicationContext(
				runWindow->shell),  &nextEvent);
                        ServerDispatchEvent(&nextEvent);
                    }
                } else {
                    /*  If we come here this means that the string was parsed
                        from within another macro via load_macro_file(). In
                        this case, plain code segments outside of define
                        blocks are rolled into one Program each and put on
                        the stack. At the end, the stack is unrolled, so the
                        plain Programs would be executed in the wrong order.

                        So we don't hand the Programs over to the interpreter
                        just yet (via RunMacroAsSubrCall()), but put it on a
                        stack of our own, reversing order once again.   */
                    Push(progStack, (void*) prog);
                }
	    }
	    inPtr = stoppedAt;
    	}
    }

    /*  Unroll reversal stack for macros loaded from macros.  */
    while (NULL != (prog = (Program*) Pop(progStack))) {
        RunMacroAsSubrCall(prog);
    }

    /*  This stack is empty, so just free it without checking the members.  */
    NEditFree(progStack);

    return True;
}

/*
** Run a pre-compiled macro, changing the interface state to reflect that
** a macro is running, and handling preemption, resumption, and cancellation.
** frees prog when macro execution is complete;
*/
static void runMacro(WindowInfo *window, Program *prog)
{
    DataValue result;
    char *errMsg;
    int stat;
    macroCmdInfo *cmdData;
    XmString s;
    
    /* If a macro is already running, just call the program as a subroutine,
       instead of starting a new one, so we don't have to keep a separate
       context, and the macros will serialize themselves automatically */
    if (window->macroCmdData != NULL) {
    	RunMacroAsSubrCall(prog);
	return;
    }
    
    /* put up a watch cursor over the waiting window */
    BeginWait(window->shell);
    
    /* enable the cancel menu item */
    XtVaSetValues(window->cancelMacroItem, XmNlabelString,
    	    s=XmStringCreateSimple("Cancel Macro"), NULL);
    XmStringFree(s);
    SetSensitive(window, window->cancelMacroItem, True);

    /* Create a data structure for passing macro execution information around
       amongst the callback routines which will process i/o and completion */
    cmdData = (macroCmdInfo *)NEditMalloc(sizeof(macroCmdInfo));
    window->macroCmdData = cmdData;
    cmdData->bannerIsUp = False;
    cmdData->closeOnCompletion = False;
    cmdData->program = prog;
    cmdData->context = NULL;
    cmdData->continueWorkProcID = 0;
    cmdData->dialog = NULL;
    
    /* Set up timer proc for putting up banner when macro takes too long */
    cmdData->bannerTimeoutID = XtAppAddTimeOut(
    	    XtWidgetToApplicationContext(window->shell), BANNER_WAIT_TIME,
    	    bannerTimeoutProc, window);
    
    /* Begin macro execution */
    stat = ExecuteMacro(window, prog, 0, NULL, &result, &cmdData->context,
    	    &errMsg);

    if (stat == MACRO_ERROR)
    {
        finishMacroCmdExecution(window);
        DialogF(DF_ERR, window->shell, 1, "Macro Error",
                "Error executing macro: %s", "OK", errMsg);
        return;
    }

    if (stat == MACRO_DONE) {
    	finishMacroCmdExecution(window);
    	return;
    }
    if (stat == MACRO_TIME_LIMIT) {
	ResumeMacroExecution(window);
	return;
    }
    /* (stat == MACRO_PREEMPT) Macro was preempted */
}

/*
** Continue with macro execution after preemption.  Called by the routines
** whose actions cause preemption when they have completed their lengthy tasks.
** Re-establishes macro execution work proc.  Window must be the window in
** which the macro is executing (the window to which macroCmdData is attached),
** and not the window to which operations are focused.
*/
void ResumeMacroExecution(WindowInfo *window)
{
    macroCmdInfo *cmdData = (macroCmdInfo *)window->macroCmdData;
    
    if (cmdData != NULL)
	cmdData->continueWorkProcID = XtAppAddWorkProc(
	    	XtWidgetToApplicationContext(window->shell),
	    	continueWorkProc, window);
}

/*
** Cancel the macro command in progress (user cancellation via GUI)
*/
void AbortMacroCommand(WindowInfo *window)
{
    if (window->macroCmdData == NULL)
    	return;
    
    /* If there's both a macro and a shell command executing, the shell command
       must have been called from the macro.  When called from a macro, shell
       commands don't put up cancellation controls of their own, but rely
       instead on the macro cancellation mechanism (here) */
#ifndef VMS
    if (window->shellCmdData != NULL)
    	AbortShellCommand(window);
#endif
    
    /* Free the continuation */
    FreeRestartData(((macroCmdInfo *)window->macroCmdData)->context);
    
    /* Kill the macro command */
    finishMacroCmdExecution(window);
}

/*
** Call this before closing a window, to clean up macro references to the
** window, stop any macro which might be running from it, free associated
** memory, and check that a macro is not attempting to close the window from
** which it is run.  If this is being called from a macro, and the window
** this routine is examining is the window from which the macro was run, this
** routine will return False, and the caller must NOT CLOSE THE WINDOW. 
** Instead, empty it and make it Untitled, and let the macro completion
** process close the window when the macro is finished executing.
*/
int MacroWindowCloseActions(WindowInfo *window)
{
    macroCmdInfo *mcd, *cmdData = window->macroCmdData;
    WindowInfo *w;

    if (MacroRecordActionHook != 0 && MacroRecordWindow == window) {
        FinishLearn();
    }

    /* If no macro is executing in the window, allow the close, but check
       if macros executing in other windows have it as focus.  If so, set
       their focus back to the window from which they were originally run */
    if (cmdData == NULL) {
    	for (w=WindowList; w!=NULL; w=w->next) {
	    mcd = (macroCmdInfo *)w->macroCmdData;
	    if (w == MacroRunWindow() && MacroFocusWindow() == window)
		SetMacroFocusWindow(MacroRunWindow());
	    else if (mcd != NULL && mcd->context->focusWindow == window)
		mcd->context->focusWindow = mcd->context->runWindow;
	}
    	return True;
    }
    
    /* If the macro currently running (and therefore calling us, because
       execution must otherwise return to the main loop to execute any
       commands), is running in this window, tell the caller not to close,
       and schedule window close on completion of macro */
    if (window == MacroRunWindow()) {
    	cmdData->closeOnCompletion = True;
	return False;
    }
    
    /* Free the continuation */
    FreeRestartData(cmdData->context);
    
    /* Kill the macro command */
    finishMacroCmdExecution(window);
    return True;
}

/*
** Clean up after the execution of a macro command: free memory, and restore
** the user interface state.
*/
static void finishMacroCmdExecution(WindowInfo *window)
{
    macroCmdInfo *cmdData = window->macroCmdData;
    int closeOnCompletion = cmdData->closeOnCompletion;
    XmString s;
    XClientMessageEvent event;

    /* Cancel pending timeout and work proc */
    if (cmdData->bannerTimeoutID != 0)
    	XtRemoveTimeOut(cmdData->bannerTimeoutID);
    if (cmdData->continueWorkProcID != 0)
    	XtRemoveWorkProc(cmdData->continueWorkProcID);
    
    /* Clean up waiting-for-macro-command-to-complete mode */
    EndWait(window->shell);
    XtVaSetValues(window->cancelMacroItem, XmNlabelString,
    	    s=XmStringCreateSimple("Cancel Learn"), NULL);
    XmStringFree(s);
    SetSensitive(window, window->cancelMacroItem, False);
    if (cmdData->bannerIsUp)
    	ClearModeMessage(window);

    /* If a dialog was up, get rid of it */
    if (cmdData->dialog != NULL)
    	XtDestroyWidget(XtParent(cmdData->dialog));

    /* Free execution information */
    FreeProgram(cmdData->program);
    NEditFree(cmdData);
    window->macroCmdData = NULL;
    
    /* If macro closed its own window, window was made empty and untitled,
       but close was deferred until completion.  This is completion, so if
       the window is still empty, do the close */
    if (closeOnCompletion && !window->filenameSet && !window->fileChanged) {
    	CloseWindow(window);
	window = NULL;
    }

    /* If no other macros are executing, do garbage collection */
    SafeGC();
    
    /* In processing the .neditmacro file (and possibly elsewhere), there
       is an event loop which waits for macro completion.  Send an event
       to wake up that loop, otherwise execution will stall until the user
       does something to the window. */
    if (!closeOnCompletion) {
	event.format = 8;
	event.type = ClientMessage;
	XSendEvent(XtDisplay(window->shell), XtWindow(window->shell), False,
		NoEventMask, (XEvent *)&event);
    }
}

/*
** Do garbage collection of strings if there are no macros currently
** executing.  NEdit's macro language GC strategy is to call this routine
** whenever a macro completes.  If other macros are still running (preempted
** or waiting for a shell command or dialog), this does nothing and therefore
** defers GC to the completion of the last macro out.
*/
void SafeGC(void)
{
    WindowInfo *win;
    
    for (win=WindowList; win!=NULL; win=win->next)
	if (win->macroCmdData != NULL || InSmartIndentMacros(win))
	    return;
    GarbageCollectStrings();
}

/*
** Executes macro string "macro" using the lastFocus pane in "window".
** Reports errors via a dialog posted over "window", integrating the name
** "errInName" into the message to help identify the source of the error.
*/
void DoMacro(WindowInfo *window, const char *macro, const char *errInName)
{
    Program *prog;
    char *errMsg, *stoppedAt, *tMacro;
    int macroLen;
    
    /* Add a terminating newline (which command line users are likely to omit
       since they are typically invoking a single routine) */
    macroLen = strlen(macro);
    tMacro = (char*)NEditMalloc(strlen(macro)+2);
    strncpy(tMacro, macro, macroLen);
    tMacro[macroLen] = '\n';
    tMacro[macroLen+1] = '\0';
    
    /* Parse the macro and report errors if it fails */
    prog = ParseMacro(tMacro, &errMsg, &stoppedAt);
    if (prog == NULL) {
    	ParseError(window->shell, tMacro, stoppedAt, errInName, errMsg);
	NEditFree(tMacro);
    	return;
    }
    NEditFree(tMacro);

    /* run the executable program (prog is freed upon completion) */
    runMacro(window, prog);
}

/*
** Get the current Learn/Replay macro in text form.  Returned string is a
** pointer to the stored macro and should not be freed by the caller (and
** will cease to exist when the next replay macro is installed)
*/
char *GetReplayMacro(void)
{
    return ReplayMacro;
}

/*
** Present the user a dialog for "Repeat" command
*/
void RepeatDialog(WindowInfo *window)
{
    Widget form, selBox, radioBox, timesForm;
    repeatDialog *rd;
    Arg selBoxArgs[1];
    char *lastCmdLabel, *parenChar;
    XmString s1;
    int cmdNameLen;

    if (LastCommand == NULL)
    {
        DialogF(DF_WARN, window->shell, 1, "Repeat Macro",
                "No previous commands or learn/\nreplay sequences to repeat",
                "OK");
        return;
    }
    
    /* Remeber the last command, since the user is allowed to work in the
       window while the dialog is up */
    rd = (repeatDialog *)NEditMalloc(sizeof(repeatDialog));
    rd->lastCommand = NEditStrdup(LastCommand);
    
    /* make a label for the Last command item of the dialog, which includes
       the last executed action name */
    parenChar = strchr(LastCommand, '(');
    if (parenChar == NULL)
	return;
    cmdNameLen = parenChar-LastCommand;
    lastCmdLabel = (char*)NEditMalloc(16 + cmdNameLen);
    strcpy(lastCmdLabel, "Last Command (");
    strncpy(&lastCmdLabel[14], LastCommand, cmdNameLen);
    strcpy(&lastCmdLabel[14 + cmdNameLen], ")");
    
    XtSetArg(selBoxArgs[0], XmNautoUnmanage, False);
    selBox = CreatePromptDialog(window->shell, "repeat", selBoxArgs, 1);
    rd->shell = XtParent(selBox);
    XtAddCallback(rd->shell, XmNdestroyCallback, repeatDestroyCB, rd);
    XtAddCallback(selBox, XmNokCallback, repeatOKCB, rd);
    XtAddCallback(selBox, XmNapplyCallback, repeatApplyCB, rd);
    XtAddCallback(selBox, XmNcancelCallback, repeatCancelCB, rd);
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_SELECTION_LABEL));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_HELP_BUTTON));
    XtUnmanageChild(XmSelectionBoxGetChild(selBox, XmDIALOG_APPLY_BUTTON));
    XtVaSetValues(XtParent(selBox), XmNtitle, "Repeat Macro", NULL);
    AddMotifCloseCallback(XtParent(selBox), repeatCancelCB, rd);
    
    form = XtVaCreateManagedWidget("form", xmFormWidgetClass, selBox, NULL);

    radioBox = XtVaCreateManagedWidget("cmdSrc", xmRowColumnWidgetClass, form,
    	    XmNradioBehavior, True,
	    XmNorientation, XmHORIZONTAL,
	    XmNpacking, XmPACK_TIGHT,
	    XmNtopAttachment, XmATTACH_FORM,
    	    XmNleftAttachment, XmATTACH_FORM, NULL);
    rd->lastCmdToggle = XtVaCreateManagedWidget("lastCmdToggle",
    	    xmToggleButtonWidgetClass, radioBox, XmNset, True,
	    XmNlabelString, s1=XmStringCreateSimple(lastCmdLabel),
	    XmNmnemonic, 'C', NULL);
    XmStringFree(s1);
    NEditFree(lastCmdLabel);
    XtVaCreateManagedWidget("learnReplayToggle",
    	    xmToggleButtonWidgetClass, radioBox, XmNset, False,
	    XmNlabelString,
	    	s1=XmStringCreateSimple("Learn/Replay"),
	    XmNmnemonic, 'L',
	    XmNsensitive, ReplayMacro != NULL, NULL);
    XmStringFree(s1);

    timesForm = XtVaCreateManagedWidget("form", xmFormWidgetClass, form,
	    XmNtopAttachment, XmATTACH_WIDGET,
    	    XmNtopWidget, radioBox,
	    XmNtopOffset, 10,
    	    XmNleftAttachment, XmATTACH_FORM, NULL);
    radioBox = XtVaCreateManagedWidget("method", xmRowColumnWidgetClass,
	    timesForm,
    	    XmNradioBehavior, True,
	    XmNorientation, XmHORIZONTAL,
	    XmNpacking, XmPACK_TIGHT,
	    XmNtopAttachment, XmATTACH_FORM,
    	    XmNbottomAttachment, XmATTACH_FORM, 
    	    XmNleftAttachment, XmATTACH_FORM, NULL);
    rd->inSelToggle = XtVaCreateManagedWidget("inSelToggle",
    	    xmToggleButtonWidgetClass, radioBox, XmNset, False,
	    XmNlabelString, s1=XmStringCreateSimple("In Selection"),
	    XmNmnemonic, 'I', NULL);
    XmStringFree(s1);
    rd->toEndToggle = XtVaCreateManagedWidget("toEndToggle",
    	    xmToggleButtonWidgetClass, radioBox, XmNset, False,
	    XmNlabelString, s1=XmStringCreateSimple("To End"),
	    XmNmnemonic, 'T', NULL);
    XmStringFree(s1);
    XtVaCreateManagedWidget("nTimesToggle",
    	    xmToggleButtonWidgetClass, radioBox, XmNset, True,
	    XmNlabelString, s1=XmStringCreateSimple("N Times"),
	    XmNmnemonic, 'N',
	    XmNset, True, NULL);
    XmStringFree(s1);
    rd->repeatText = XtVaCreateManagedWidget("repeatText", xmTextWidgetClass,
	    timesForm,
    	    XmNcolumns, 5,
    	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNleftAttachment, XmATTACH_WIDGET,
    	    XmNleftWidget, radioBox, NULL);
    RemapDeleteKey(rd->repeatText);

    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);

    /* Set initial focus */
#if XmVersion >= 1002
    XtVaSetValues(form, XmNinitialFocus, timesForm, NULL);
    XtVaSetValues(timesForm, XmNinitialFocus, rd->repeatText, NULL);
#endif
    
    /* put up dialog */
    rd->forWindow = window;
    ManageDialogCenteredOnPointer(selBox);
}

static void repeatOKCB(Widget w, XtPointer clientData, XtPointer callData)
{
    repeatDialog *rd = (repeatDialog *)clientData;

    if (doRepeatDialogAction(rd, ((XmAnyCallbackStruct *)callData)->event))
    	XtDestroyWidget(rd->shell);
}

/* Note that the apply button is not managed in the repeat dialog.  The dialog
   itself is capable of non-modal operation, but to be complete, it needs
   to dynamically update last command, dimming of learn/replay, possibly a
   stop button for the macro, and possibly in-selection with selection */
static void repeatApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    doRepeatDialogAction((repeatDialog *)clientData,
	    ((XmAnyCallbackStruct *)callData)->event);
}

static int doRepeatDialogAction(repeatDialog *rd, XEvent *event)
{
    int nTimes;
    char nTimesStr[TYPE_INT_STR_SIZE(int)];
    char *params[2];
    
    /* Find out from the dialog how to repeat the command */
    if (XmToggleButtonGetState(rd->inSelToggle))
    {
        if (!rd->forWindow->buffer->primary.selected)
        {
            DialogF(DF_WARN, rd->shell, 1, "Repeat Macro",
                    "No selection in window to repeat within", "OK");
            XmProcessTraversal(rd->inSelToggle, XmTRAVERSE_CURRENT);
            return False;
        }
        params[0] = "in_selection";
    } else if (XmToggleButtonGetState(rd->toEndToggle))
    {
        params[0] = "to_end";
    } else
    {
        if (GetIntTextWarn(rd->repeatText, &nTimes, "number of times", True)
                != TEXT_READ_OK)
        {
            XmProcessTraversal(rd->repeatText, XmTRAVERSE_CURRENT);
            return False;
        }
        sprintf(nTimesStr, "%d", nTimes);
        params[0] = nTimesStr;
    }
    
    /* Figure out which command user wants to repeat */
    if (XmToggleButtonGetState(rd->lastCmdToggle))
	params[1] = NEditStrdup(rd->lastCommand);
    else {
	if (ReplayMacro == NULL)
	    return False;
	params[1] = NEditStrdup(ReplayMacro);
    }

    /* call the action routine repeat_macro to do the work */
    XtCallActionProc(rd->forWindow->lastFocus, "repeat_macro", event, params,2);
    NEditFree(params[1]);
    return True;
}

static void repeatCancelCB(Widget w, XtPointer clientData, XtPointer callData)
{
    repeatDialog *rd = (repeatDialog *)clientData;

    XtDestroyWidget(rd->shell);
}

static void repeatDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    repeatDialog *rd = (repeatDialog *)clientData;
    
    NEditFree(rd->lastCommand);
    NEditFree(rd);
}

/*
** Dispatches a macro to which repeats macro command in "command", either
** an integer number of times ("how" == positive integer), or within a
** selected range ("how" == REPEAT_IN_SEL), or to the end of the window
** ("how == REPEAT_TO_END).
**
** Note that as with most macro routines, this returns BEFORE the macro is
** finished executing
*/
void RepeatMacro(WindowInfo *window, const char *command, int how)
{
    Program *prog;
    char *errMsg, *stoppedAt, *loopMacro, *loopedCmd;

    if (command == NULL)
	return;
    
    /* Wrap a for loop and counter/tests around the command */
    if (how == REPEAT_TO_END)
	loopMacro = "lastCursor=-1\nstartPos=$cursor\n\
while($cursor>=startPos&&$cursor!=lastCursor){\nlastCursor=$cursor\n%s\n}\n";
    else if (how == REPEAT_IN_SEL)
	loopMacro = "selStart = $selection_start\nif (selStart == -1)\nreturn\n\
selEnd = $selection_end\nset_cursor_pos(selStart)\nselect(0,0)\n\
boundText = get_range(selEnd, selEnd+10)\n\
while($cursor >= selStart && $cursor < selEnd && \\\n\
get_range(selEnd, selEnd+10) == boundText) {\n\
startLength = $text_length\n%s\n\
selEnd += $text_length - startLength\n}\n";
    else
    	loopMacro = "for(i=0;i<%d;i++){\n%s\n}\n";
    loopedCmd = (char*)NEditMalloc(strlen(command) + strlen(loopMacro) + 25);
    if (how == REPEAT_TO_END || how == REPEAT_IN_SEL)
	sprintf(loopedCmd, loopMacro, command);
    else
	sprintf(loopedCmd, loopMacro, how, command);
    
    /* Parse the resulting macro into an executable program "prog" */
    prog = ParseMacro(loopedCmd, &errMsg, &stoppedAt);
    if (prog == NULL) {
	fprintf(stderr, "NEdit internal error, repeat macro syntax wrong: %s\n",
    		errMsg);
    	return;
    }
    NEditFree(loopedCmd);

    /* run the executable program */
    runMacro(window, prog);
}

/*
** Macro recording action hook for Learn/Replay, added temporarily during
** learn.
*/
static void learnActionHook(Widget w, XtPointer clientData, String actionName,
	XEvent *event, String *params, Cardinal *numParams)
{
    WindowInfo *window;
    int i;
    char *actionString;
    
    /* Select only actions in text panes in the window for which this
       action hook is recording macros (from clientData). */
    for (window=WindowList; window!=NULL; window=window->next) {
	if (window->textArea == w)
	    break;
	for (i=0; i<window->nPanes; i++) {
    	    if (window->textPanes[i] == w)
    	    	break;
	}
	if (i < window->nPanes)
	    break;
    }
    if (window == NULL || window != (WindowInfo *)clientData)
    	return;
    
    /* beep on un-recordable operations which require a mouse position, to
       remind the user that the action was not recorded */
    if (isMouseAction(actionName)) {
    	XBell(XtDisplay(w), 0);
    	return;
    }
    
    /* Record the action and its parameters */
    actionString = actionToString(w, actionName, event, params, *numParams);
    if (actionString != NULL) {
	BufInsert(MacroRecordBuf, MacroRecordBuf->length, actionString);
	NEditFree(actionString);
    }
}

/*
** Permanent action hook for remembering last action for possible replay
*/
static void lastActionHook(Widget w, XtPointer clientData, String actionName,
	XEvent *event, String *params, Cardinal *numParams)
{
    WindowInfo *window;
    int i;
    char *actionString;
    
    /* Find the window to which this action belongs */
    for (window=WindowList; window!=NULL; window=window->next) {
	if (window->textArea == w)
	    break;
	for (i=0; i<window->nPanes; i++) {
    	    if (window->textPanes[i] == w)
    	    	break;
	}
	if (i < window->nPanes)
	    break;
    }
    if (window == NULL)
    	return;

    /* The last action is recorded for the benefit of repeating the last
       action.  Don't record repeat_macro and wipe out the real action */
    if (!strcmp(actionName, "repeat_macro"))
	return;
        
    /* Record the action and its parameters */
    actionString = actionToString(w, actionName, event, params, *numParams);
    if (actionString != NULL) {
        NEditFree(LastCommand);
	LastCommand = actionString;
    }
}

/*
** Create a macro string to represent an invocation of an action routine.
** Returns NULL for non-operational or un-recordable actions.
*/
static char *actionToString(Widget w, char *actionName, XEvent *event,
	String *params, Cardinal numParams)
{
    char chars[20], *charList[1], *outStr, *outPtr;
    KeySym keysym;
    int i, nChars, nParams, length, nameLength;
#ifndef NO_XMIM
    int status;
#endif
    
    if (isIgnoredAction(actionName) || isRedundantAction(actionName) ||
	    isMouseAction(actionName))
    	return NULL;
    
    /* Convert self_insert actions, to insert_string */
    if (!strcmp(actionName, "self_insert") ||
    	    !strcmp(actionName, "self-insert")) {
    	actionName = "insert_string";
#ifdef NO_XMIM
	nChars = XLookupString((XKeyEvent *)event, chars, 19, &keysym, NULL);
	if (nChars == 0)
	    return NULL;
#else

	nChars = XmImMbLookupString(w, (XKeyEvent *)event,
				    chars, 19, &keysym, &status);
	if (nChars == 0 || status == XLookupNone ||
		status == XLookupKeySym || status == XBufferOverflow)
	    return NULL;
#endif
    	chars[nChars] = '\0';
    	charList[0] = chars;
    	params = charList;
    	nParams = 1;
    } else
    	nParams = numParams;
    	
    /* Figure out the length of string required */
    nameLength = strlen(actionName);
    length = nameLength + 3;
    for (i=0; i<nParams; i++)
	length += escapedStringLength(params[i]) + 4;
    
    /* Allocate the string and copy the information to it */
    outPtr = outStr = (char*)NEditMalloc(length + 1);
    strcpy(outPtr, actionName);
    outPtr += nameLength;
    *outPtr++ = '(';
    for (i=0; i<nParams; i++) {
	*outPtr++ = '\"';
	outPtr += escapeStringChars(params[i], outPtr);
	*outPtr++ = '\"'; *outPtr++ = ','; *outPtr++ = ' ';
    }
    if (nParams != 0)
	outPtr -= 2;
    *outPtr++ = ')'; *outPtr++ = '\n'; *outPtr++ = '\0';
    return outStr;
}

static int isMouseAction(const char *action)
{
    int i;
    
    for (i=0; i<(int)XtNumber(MouseActions); i++)
    	if (!strcmp(action, MouseActions[i]))
    	    return True;
    return False;
}

static int isRedundantAction(const char *action)
{
    int i;
    
    for (i=0; i<(int)XtNumber(RedundantActions); i++)
    	if (!strcmp(action, RedundantActions[i]))
    	    return True;
    return False;
}

static int isIgnoredAction(const char *action)
{
    int i;
    
    for (i=0; i<(int)XtNumber(IgnoredActions); i++)
    	if (!strcmp(action, IgnoredActions[i]))
    	    return True;
    return False;
}

/*
** Timer proc for putting up the "Macro Command in Progress" banner if
** the process is taking too long.
*/
#define MAX_TIMEOUT_MSG_LEN (MAX_ACCEL_LEN + 60)
static void bannerTimeoutProc(XtPointer clientData, XtIntervalId *id)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    XmString xmCancel;
    char *cCancel = "\0";
    char message[MAX_TIMEOUT_MSG_LEN];
    
    cmdData->bannerIsUp = True;

    /* Extract accelerator text from menu PushButtons */
    XtVaGetValues(window->cancelMacroItem, XmNacceleratorText, &xmCancel, NULL);

    if (!XmStringEmpty(xmCancel))
        {
        /* Translate Motif string to char* */
        cCancel = GetXmStringText(xmCancel);

        /* Free Motif String */
        XmStringFree(xmCancel);
        }

    /* Create message */
    if (cCancel[0] == '\0') {
        strncpy(message, "Macro Command in Progress", MAX_TIMEOUT_MSG_LEN);
        message[MAX_TIMEOUT_MSG_LEN - 1] = '\0';
    }
    else {
        sprintf(message,
            "Macro Command in Progress -- Press %s to Cancel",
            cCancel);
    }

    /* Free C-string */
    NEditFree(cCancel);

    SetModeMessage(window, message);
    cmdData->bannerTimeoutID = 0;
}

/*
** Work proc for continuing execution of a preempted macro.
**
** Xt WorkProcs are designed to run first-in first-out, which makes them
** very bad at sharing time between competing tasks.  For this reason, it's
** usually bad to use work procs anywhere where their execution is likely to
** overlap.  Using a work proc instead of a timer proc (which I usually
** prefer) here means macros will probably share time badly, but we're more
** interested in making the macros cancelable, and in continuing other work
** than having users run a bunch of them at once together.
*/
static Boolean continueWorkProc(XtPointer clientData)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    char *errMsg;
    int stat;
    DataValue result;
    
    stat = ContinueMacro(cmdData->context, &result, &errMsg);
    if (stat == MACRO_ERROR)
    {
        finishMacroCmdExecution(window);
        DialogF(DF_ERR, window->shell, 1, "Macro Error",
                "Error executing macro: %s", "OK", errMsg);
        return True;
    } else if (stat == MACRO_DONE)
    {
        finishMacroCmdExecution(window);
        return True;
    } else if (stat == MACRO_PREEMPT)
    {
        cmdData->continueWorkProcID = 0;
        return True;
    }
    
    /* Macro exceeded time slice, re-schedule it */
    if (stat != MACRO_TIME_LIMIT)
    	return True; /* shouldn't happen */
    return False;
}

/*
** Copy fromString to toString replacing special characters in strings, such
** that they can be read back by the macro parser's string reader.  i.e. double
** quotes are replaced by \", backslashes are replaced with \\, C-std control
** characters like \n are replaced with their backslash counterparts.  This
** routine should be kept reasonably in sync with yylex in parse.y.  Companion
** routine escapedStringLength predicts the length needed to write the string
** when it is expanded with the additional characters.  Returns the number
** of characters to which the string expanded.
*/
static int escapeStringChars(char *fromString, char *toString)
{
    char *e, *c, *outPtr = toString;
    
    /* substitute escape sequences */
    for (c=fromString; *c!='\0'; c++) {
    	for (e=EscapeChars; *e!='\0'; e++) {
    	    if (*c == *e) {
    		*outPtr++ = '\\';
    		*outPtr++ = ReplaceChars[e-EscapeChars];
		break;
	    }
	}
	if (*e == '\0')
    	   *outPtr++ = *c;
    }
    *outPtr = '\0';
    return outPtr - toString;
}

/*
** Predict the length of a string needed to hold a copy of "string" with
** special characters replaced with escape sequences by escapeStringChars.
*/
static int escapedStringLength(char *string)
{
    char *c, *e;
    int length = 0;

    /* calculate length and allocate returned string */
    for (c=string; *c!='\0'; c++) {
    	for (e=EscapeChars; *e!='\0'; e++) {
	    if (*c == *e) {
    		length++;
		break;
	    }
	}
    	length++;
    }
    return length;
}

/*
** Built-in macro subroutine for getting the length of a string
*/
static int lengthMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char *string, stringStorage[TYPE_INT_STR_SIZE(int)];
    
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage, errMsg))
	return False;
    result->tag = INT_TAG;
    result->val.n = strlen(string);
    return True;
}

/*
** Built-in macro subroutines for min and max
*/
static int minMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int minVal, value, i;
    
    if (nArgs == 1)
    	return tooFewArgsErr(errMsg);
    if (!readIntArg(argList[0], &minVal, errMsg))
    	return False;
    for (i=0; i<nArgs; i++) {
	if (!readIntArg(argList[i], &value, errMsg))
    	    return False;
    	minVal = value < minVal ? value : minVal;
    }
    result->tag = INT_TAG;
    result->val.n = minVal;
    return True;
}
static int maxMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int maxVal, value, i;
    
    if (nArgs == 1)
    	return tooFewArgsErr(errMsg);
    if (!readIntArg(argList[0], &maxVal, errMsg))
    	return False;
    for (i=0; i<nArgs; i++) {
	if (!readIntArg(argList[i], &value, errMsg))
    	    return False;
    	maxVal = value > maxVal ? value : maxVal;
    }
    result->tag = INT_TAG;
    result->val.n = maxVal;
    return True;
}

static int focusWindowMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    WindowInfo *w;
    char fullname[MAXPATHLEN];
    char normalizedString[MAXPATHLEN];

    /* Read the argument representing the window to focus to, and translate
       it into a pointer to a real WindowInfo */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);

    if (!readStringArg(argList[0], &string, stringStorage, errMsg)) {
        return False;
    } else if (!strcmp(string, "last")) {
        w = WindowList;
    } else if (!strcmp(string, "next")) {
        w = window->next;
    } else if (strlen(string) >= MAXPATHLEN) {
        *errMsg = "Pathname too long in focus_window()";
        return False;
    } else {
        /* just use the plain name as supplied */
	for (w=WindowList; w != NULL; w = w->next) {
	    sprintf(fullname, "%s%s", w->path, w->filename);
	    if (!strcmp(string, fullname)) {
		break;
	    }
	}
        /* didn't work? try normalizing the string passed in */
        if (w == NULL) {
            strncpy(normalizedString, string, MAXPATHLEN);
            normalizedString[MAXPATHLEN-1] = '\0';
            if (1 == NormalizePathname(normalizedString)) {
                /*  Something is broken with the input pathname. */
                *errMsg = "Pathname too long in focus_window()";
                return False;
            }
            for (w=WindowList; w != NULL; w = w->next) {
                sprintf(fullname, "%s%s", w->path, w->filename);
                if (!strcmp(normalizedString, fullname))
                    break;
            }
        }
    }
    
    /* If no matching window was found, return empty string and do nothing */
    if (w == NULL) {
	result->tag = STRING_TAG;
	result->val.str.rep = PERM_ALLOC_STR("");
        result->val.str.len = 0;
	return True;
    }
	
    /* Change the focused window to the requested one */
    SetMacroFocusWindow(w);

    /* turn on syntax highlight that might have been deferred */
    if (w->highlightSyntax && w->highlightData==NULL)
    	StartHighlighting(w, False);

    /* Return the name of the window */
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, strlen(w->path)+strlen(w->filename)+1);
    sprintf(result->val.str.rep, "%s%s", w->path, w->filename);
    return True;
}

/*
** Built-in macro subroutine for getting text from the current window's text
** buffer
*/
static int getRangeMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int from, to;
    textBuffer *buf = window->buffer;
    char *rangeText;
    
    /* Validate arguments and convert to int */
    if (nArgs != 2)
    	return wrongNArgsErr(errMsg);
    if (!readIntArg(argList[0], &from, errMsg))
    	return False;
    if (!readIntArg(argList[1], &to, errMsg))
	return False;
    if (from < 0) from = 0;
    if (from > buf->length) from = buf->length;
    if (to < 0) to = 0;
    if (to > buf->length) to = buf->length;
    if (from > to) {int temp = from; from = to; to = temp;}
    
    /* Copy text from buffer (this extra copy could be avoided if textBuf.c
       provided a routine for writing into a pre-allocated string) */
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, to - from + 1);
    rangeText = BufGetRange(buf, from, to);
    BufUnsubstituteNullChars(rangeText, buf);
    strcpy(result->val.str.rep, rangeText);
    /* Note: after the un-substitution, it is possible that strlen() != len,
       but that's because strlen() can't deal with 0-characters. */
    NEditFree(rangeText);
    return True;
}

/*
** Built-in macro subroutine for getting a single character at the position
** given, from the current window
*/
static int getCharacterMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int pos;
    textBuffer *buf = window->buffer;
    
    /* Validate argument and convert it to int */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readIntArg(argList[0], &pos, errMsg))
    	return False;
    if (pos < 0) pos = 0;
    if (pos > buf->length) pos = buf->length;
    
    /* Return the character in a pre-allocated string) */
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, 2);
    result->val.str.rep[0] = BufGetCharacter(buf, pos);
    BufUnsubstituteNullChars(result->val.str.rep, buf);
    /* Note: after the un-substitution, it is possible that strlen() != len,
       but that's because strlen() can't deal with 0-characters. */
    return True;
}

/*
** Built-in macro subroutine for replacing text in the current window's text
** buffer
*/
static int replaceRangeMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int from, to;
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    textBuffer *buf = window->buffer;
    
    /* Validate arguments and convert to int */
    if (nArgs != 3)
    	return wrongNArgsErr(errMsg);
    if (!readIntArg(argList[0], &from, errMsg))
    	return False;
    if (!readIntArg(argList[1], &to, errMsg))
	return False;
    if (!readStringArg(argList[2], &string, stringStorage, errMsg))
    	return False;
    if (from < 0) from = 0;
    if (from > buf->length) from = buf->length;
    if (to < 0) to = 0;
    if (to > buf->length) to = buf->length;
    if (from > to) {int temp = from; from = to; to = temp;}
    
    /* Don't allow modifications if the window is read-only */
    if (IS_ANY_LOCKED(window->lockReasons)) {
	XBell(XtDisplay(window->shell), 0);
	result->tag = NO_TAG;
	return True;
    }
    
    /* There are no null characters in the string (because macro strings
       still have null termination), but if the string contains the
       character used by the buffer for null substitution, it could
       theoretically become a null.  In the highly unlikely event that
       all of the possible substitution characters in the buffer are used
       up, stop the macro and tell the user of the failure */
    if (!BufSubstituteNullChars(string, strlen(string), window->buffer)) {
	*errMsg = "Too much binary data in file";
	return False;
    }

    /* Do the replace */
    BufReplace(buf, from, to, string);
    result->tag = NO_TAG;
    return True;
}

/*
** Built-in macro subroutine for replacing the primary-selection selected
** text in the current window's text buffer
*/
static int replaceSelectionMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    
    /* Validate argument and convert to string */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage, errMsg))
    	return False;
    
    /* Don't allow modifications if the window is read-only */
    if (IS_ANY_LOCKED(window->lockReasons)) {
	XBell(XtDisplay(window->shell), 0);
	result->tag = NO_TAG;
	return True;
    }
     
    /* There are no null characters in the string (because macro strings
       still have null termination), but if the string contains the
       character used by the buffer for null substitution, it could
       theoretically become a null.  In the highly unlikely event that
       all of the possible substitution characters in the buffer are used
       up, stop the macro and tell the user of the failure */
    if (!BufSubstituteNullChars(string, strlen(string), window->buffer)) {
	*errMsg = "Too much binary data in file";
	return False;
    }
    
    /* Do the replace */
    BufReplaceSelected(window->buffer, string);
    result->tag = NO_TAG;
    return True;
}

/*
** Built-in macro subroutine for getting the text currently selected by
** the primary selection in the current window's text buffer, or in any
** part of screen if "any" argument is given
*/
static int getSelectionMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char *selText;

    /* Read argument list to check for "any" keyword, and get the appropriate
       selection */
    if (nArgs != 0 && nArgs != 1)
      	return wrongNArgsErr(errMsg);
    if (nArgs == 1) {
        if (argList[0].tag != STRING_TAG || strcmp(argList[0].val.str.rep, "any")) {
	    *errMsg = "Unrecognized argument to %s";
	    return False;
    	}
	selText = GetAnySelection(window);
	if (selText == NULL)
	    selText = NEditStrdup("");
    } else {
	selText = BufGetSelectionText(window->buffer);
    	BufUnsubstituteNullChars(selText, window->buffer);
    }
	
    /* Return the text as an allocated string */
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, selText);
    NEditFree(selText);
    return True;
}

/*
** Built-in macro subroutine for determining if implicit conversion of
** a string to number will succeed or fail
*/
static int validNumberMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg)
{
    char *string, stringStorage[TYPE_INT_STR_SIZE(int)];
    
    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }
    if (!readStringArg(argList[0], &string, stringStorage, errMsg)) {
        return False;
    }

    result->tag = INT_TAG;
    result->val.n = StringToNum(string, NULL);

    return True;
}

/*
** Built-in macro subroutine for replacing a substring within another string
*/
static int replaceSubstringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int from, to, length, replaceLen, outLen;
    char stringStorage[2][TYPE_INT_STR_SIZE(int)], *string, *replStr;
    
    /* Validate arguments and convert to int */
    if (nArgs != 4)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage[1], errMsg))
    	return False;
    if (!readIntArg(argList[1], &from, errMsg))
    	return False;
    if (!readIntArg(argList[2], &to, errMsg))
	return False;
    if (!readStringArg(argList[3], &replStr, stringStorage[1], errMsg))
    	return False;
    length = strlen(string);
    if (from < 0) from = 0;
    if (from > length) from = length;
    if (to < 0) to = 0;
    if (to > length) to = length;
    if (from > to) {int temp = from; from = to; to = temp;}
    
    /* Allocate a new string and do the replacement */
    replaceLen = strlen(replStr);
    outLen = length - (to - from) + replaceLen;
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, outLen+1);
    strncpy(result->val.str.rep, string, from);
    strncpy(&result->val.str.rep[from], replStr, replaceLen);
    strncpy(&result->val.str.rep[from + replaceLen], &string[to], length - to);
    return True;
}

/*
** Built-in macro subroutine for getting a substring of a string.
** Called as substring(string, from [, to])
*/
static int substringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int from, to, length;
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    
    /* Validate arguments and convert to int */
    if (nArgs != 2 && nArgs != 3)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage, errMsg))
    	return False;
    if (!readIntArg(argList[1], &from, errMsg))
    	return False;
    length = to = strlen(string);
    if (nArgs == 3)
        if (!readIntArg(argList[2], &to, errMsg))
            return False;
    if (from < 0) from += length;
    if (from < 0) from = 0;
    if (from > length) from = length;
    if (to < 0) to += length;
    if (to < 0) to = 0;
    if (to > length) to = length;
    if (from > to) to = from;
    
    /* Allocate a new string and copy the sub-string into it */
    result->tag = STRING_TAG;
    AllocNStringNCpy(&result->val.str, &string[from], to - from);
    return True;
}

static int toupperMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int i, length;
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    
    /* Validate arguments and convert to int */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage, errMsg))
    	return False;
    length = strlen(string);
    
    /* Allocate a new string and copy an uppercased version of the string it */
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, length + 1);
    for (i=0; i<length; i++)
    	result->val.str.rep[i] = toupper((unsigned char)string[i]);
    return True;
}

static int tolowerMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int i, length;
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    
    /* Validate arguments and convert to int */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage, errMsg))
    	return False;
    length = strlen(string);
    
    /* Allocate a new string and copy an lowercased version of the string it */
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, length + 1);
    for (i=0; i<length; i++)
    	result->val.str.rep[i] = tolower((unsigned char)string[i]);
    return True;
}

static int stringToClipboardMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    long itemID = 0;
    XmString s;
    int stat;
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    
    /* Get the string argument */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage, errMsg))
    	return False;
    
    /* Use the XmClipboard routines to copy the text to the clipboard.
       If errors occur, just give up.  */
    result->tag = NO_TAG;
    stat = SpinClipboardStartCopy(TheDisplay, XtWindow(window->textArea),
    	  s=XmStringCreateSimple("NEdit"), XtLastTimestampProcessed(TheDisplay),
	  window->textArea, NULL, &itemID);
    XmStringFree(s);
    if (stat != ClipboardSuccess)
    	return True;
    if (SpinClipboardCopy(TheDisplay, XtWindow(window->textArea), itemID, "STRING",
    	    string, strlen(string), 0, NULL) != ClipboardSuccess) {
        SpinClipboardEndCopy(TheDisplay, XtWindow(window->textArea), itemID);      
    	return True;
    }
    SpinClipboardEndCopy(TheDisplay, XtWindow(window->textArea), itemID);
    return True;
}

static int clipboardToStringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    unsigned long length, retLength;
    long id = 0;

    /* Should have no arguments */
    if (nArgs != 0)
    	return wrongNArgsErr(errMsg);
    
    /* Ask if there's a string in the clipboard, and get its length */
    if (SpinClipboardInquireLength(TheDisplay, XtWindow(window->shell), "STRING",
    	    &length) != ClipboardSuccess) {
    	result->tag = STRING_TAG;
    	result->val.str.rep = PERM_ALLOC_STR("");
    	result->val.str.len = 0;
        /*
         * Possibly, the clipboard can remain in a locked state after
         * a failure, so we try to remove the lock, just to be sure.
         */
        SpinClipboardUnlock(TheDisplay, XtWindow(window->shell));
	return True;
    }

    /* Allocate a new string to hold the data */
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, (int)length + 1);

    /* Copy the clipboard contents to the string */
    if (SpinClipboardRetrieve(TheDisplay, XtWindow(window->shell), "STRING",
    	    result->val.str.rep, length, &retLength, &id) != ClipboardSuccess) {
    	retLength = 0;
        /*
         * Possibly, the clipboard can remain in a locked state after
         * a failure, so we try to remove the lock, just to be sure.
         */
        SpinClipboardUnlock(TheDisplay, XtWindow(window->shell));
    }
    result->val.str.rep[retLength] = '\0';
    result->val.str.len = retLength;

    return True;
}


/*
** Built-in macro subroutine for reading the contents of a text file into
** a string.  On success, returns 1 in $readStatus, and the contents of the
** file as a string in the subroutine return value.  On failure, returns
** the empty string "" and an 0 $readStatus.
*/
static int readFileMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[TYPE_INT_STR_SIZE(int)], *name;
    struct stat statbuf;
    FILE *fp;
    int readLen;
    
    /* Validate arguments and convert to int */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &name, stringStorage, errMsg))
    	return False;
    
    /* Read the whole file into an allocated string */
    if ((fp = fopen(name, "r")) == NULL)
    	goto errorNoClose;
    if (fstat(fileno(fp), &statbuf) != 0)
    	goto error;
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, statbuf.st_size+1);
    readLen = fread(result->val.str.rep, sizeof(char), statbuf.st_size+1, fp);
    if (ferror(fp))
	goto error;
    if(!feof(fp)){
        /* Couldn't trust file size. Use slower but more general method */   
        int chunkSize = 1024;
        char *buffer;
        
        buffer = (char*)NEditMalloc(readLen * sizeof(char));
        memcpy(buffer, result->val.str.rep, readLen * sizeof(char));
        while (!feof(fp)){
            buffer = NEditRealloc(buffer, (readLen+chunkSize)*sizeof(char));
            readLen += fread(&buffer[readLen], sizeof(char), chunkSize, fp);
            if (ferror(fp)){
                NEditFree(buffer);
	        goto error;
            }
        }
        AllocNString(&result->val.str, readLen + 1);
        memcpy(result->val.str.rep, buffer, readLen * sizeof(char));
        NEditFree(buffer);
    }
    fclose(fp);
    
    /* Return the results */
    ReturnGlobals[READ_STATUS]->value.tag = INT_TAG;
    ReturnGlobals[READ_STATUS]->value.val.n = True;
    return True;

error:
    fclose(fp);

errorNoClose:
    ReturnGlobals[READ_STATUS]->value.tag = INT_TAG;
    ReturnGlobals[READ_STATUS]->value.val.n = False;
    result->tag = STRING_TAG;
    result->val.str.rep = PERM_ALLOC_STR("");
    result->val.str.len = 0;
    return True;
}

/*
** Built-in macro subroutines for writing or appending a string (parameter $1)
** to a file named in parameter $2. Returns 1 on successful write, or 0 if
** unsuccessful.
*/
static int writeFileMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    return writeOrAppendFile(False, window, argList, nArgs, result, errMsg);
}

static int appendFileMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    return writeOrAppendFile(True, window, argList, nArgs, result, errMsg);
}

static int writeOrAppendFile(int append, WindowInfo *window,
    	DataValue *argList, int nArgs, DataValue *result, char **errMsg)
{
    char stringStorage[2][TYPE_INT_STR_SIZE(int)], *name, *string;
    FILE *fp;
    
    /* Validate argument */
    if (nArgs != 2)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage[1], errMsg))
    	return False;
    if (!readStringArg(argList[1], &name, stringStorage[0], errMsg))
    	return False;
    
    /* open the file */
    if ((fp = fopen(name, append ? "a" : "w")) == NULL) {
	result->tag = INT_TAG;
	result->val.n = False;
	return True;
    }
    
    /* write the string to the file */
    fwrite(string, sizeof(char), strlen(string), fp);
    if (ferror(fp)) {
	fclose(fp);
	result->tag = INT_TAG;
	result->val.n = False;
	return True;
    }
    fclose(fp);
    
    /* return the status */
    result->tag = INT_TAG;
    result->val.n = True;
    return True;
}

/*
** Built-in macro subroutine for searching silently in a window without
** dialogs, beeps, or changes to the selection.  Arguments are: $1: string to
** search for, $2: starting position. Optional arguments may include the
** strings: "wrap" to make the search wrap around the beginning or end of the
** string, "backward" or "forward" to change the search direction ("forward" is
** the default), "literal", "case" or "regex" to change the search type
** (default is "literal").
**
** Returns the starting position of the match, or -1 if nothing matched.
** also returns the ending position of the match in $searchEndPos
*/
static int searchMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    DataValue newArgList[9];
    
    /* Use the search string routine, by adding the buffer contents as
       the string argument */
    if (nArgs > 8)
    	return wrongNArgsErr(errMsg);

    /* we remove constness from BufAsString() result since we know
       searchStringMS will not modify the result */
    newArgList[0].tag = STRING_TAG;
    newArgList[0].val.str.rep = (char *)BufAsString(window->buffer);
    newArgList[0].val.str.len = window->buffer->length;

    /* copy other arguments to the new argument list */
    memcpy(&newArgList[1], argList, nArgs * sizeof(DataValue));

    return searchStringMS(window, newArgList, nArgs+1, result, errMsg);
}

/*
** Built-in macro subroutine for searching a string.  Arguments are $1:
** string to search in, $2: string to search for, $3: starting position.
** Optional arguments may include the strings: "wrap" to make the search
** wrap around the beginning or end of the string, "backward" or "forward"
** to change the search direction ("forward" is the default), "literal",
** "case" or "regex" to change the search type (default is "literal").
**
** Returns the starting position of the match, or -1 if nothing matched.
** also returns the ending position of the match in $searchEndPos
*/
static int searchStringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int beginPos, wrap, direction, found = False, foundStart, foundEnd, type;
    int skipSearch = False, len;
    char stringStorage[2][TYPE_INT_STR_SIZE(int)], *string, *searchStr;
    
    /* Validate arguments and convert to proper types */
    if (nArgs < 3)
    	return tooFewArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage[0], errMsg))
    	return False;
    if (!readStringArg(argList[1], &searchStr, stringStorage[1], errMsg))
    	return False;
    if (!readIntArg(argList[2], &beginPos, errMsg))
    	return False;
    if (!readSearchArgs(&argList[3], nArgs-3, &direction, &type, &wrap, errMsg))
    	return False;
    
    len = argList[0].val.str.len;
    if (beginPos > len) {
	if (direction == SEARCH_FORWARD) {
	    if (wrap) {
		beginPos = 0; /* Wrap immediately */
	    } else {
		found = False;
		skipSearch = True;
	    }
	} else {
	    beginPos = len;
	}
    } else if (beginPos < 0) {
	if (direction == SEARCH_BACKWARD) {
	    if (wrap) {
		beginPos = len; /* Wrap immediately */
	    } else {
		found = False;
		skipSearch = True;
	    }
	} else {
	    beginPos = 0;
	}
    }
    
    if (!skipSearch) 
	found = SearchString(string, searchStr, direction, type, wrap, beginPos,
	    &foundStart, &foundEnd, NULL, NULL, GetWindowDelimiters(window));
    
    /* Return the results */
    ReturnGlobals[SEARCH_END]->value.tag = INT_TAG;
    ReturnGlobals[SEARCH_END]->value.val.n = found ? foundEnd : 0;
    result->tag = INT_TAG;
    result->val.n = found ? foundStart : -1;
    return True;
}

/*
** Built-in macro subroutine for replacing all occurences of a search string in
** a string with a replacement string.  Arguments are $1: string to search in,
** $2: string to search for, $3: replacement string. Also takes an optional
** search type: one of "literal", "case" or "regex" (default is "literal"), and
** an optional "copy" argument.
**
** Returns a new string with all of the replacements done.  If no replacements
** were performed and "copy" was specified, returns a copy of the original
** string.  Otherwise returns an empty string ("").
*/
static int replaceInStringMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[3][TYPE_INT_STR_SIZE(int)], *string, *searchStr, *replaceStr;
    char *argStr, *replacedStr;
    int searchType = SEARCH_LITERAL, copyStart, copyEnd;
    int replacedLen, replaceEnd, force=False, i;
    
    /* Validate arguments and convert to proper types */
    if (nArgs < 3 || nArgs > 5)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &string, stringStorage[0], errMsg))
    	return False;
    if (!readStringArg(argList[1], &searchStr, stringStorage[1], errMsg))
    	return False;
    if (!readStringArg(argList[2], &replaceStr, stringStorage[2], errMsg))
    	return False;
    for (i = 3; i < nArgs; i++) {
        /* Read the optional search type and force arguments */
	if (!readStringArg(argList[i], &argStr, stringStorage[2], errMsg))
    	    return False;
	if (!StringToSearchType(argStr, &searchType)) {
            /* It's not a search type.  is it "copy"? */
            if (!strcmp(argStr, "copy")) {
                force = True;
            } else {
    	        *errMsg = "unrecognized argument to %s";
    	        return False;
            }
    	}
    }
    
    /* Do the replace */
    replacedStr = ReplaceAllInString(string, searchStr, replaceStr, searchType,
	    &copyStart, &copyEnd, &replacedLen, GetWindowDelimiters(window));
    
    /* Return the results */
    result->tag = STRING_TAG;
    if (replacedStr == NULL) {
        if (force) {
            /* Just copy the original DataValue */
            if (argList[0].tag == STRING_TAG) {
                result->val.str.rep = argList[0].val.str.rep;
                result->val.str.len = argList[0].val.str.len;
            }
            else {
                AllocNStringCpy(&result->val.str, string);
            }
        }
        else {
            result->val.str.rep = PERM_ALLOC_STR("");
            result->val.str.len = 0;
        }
    }
    else {
	size_t remainder = strlen(&string[copyEnd]);
        replaceEnd = copyStart + replacedLen;
	AllocNString(&result->val.str, replaceEnd + remainder + 1);
	strncpy(result->val.str.rep, string, copyStart);
	strcpy(&result->val.str.rep[copyStart], replacedStr);
	strcpy(&result->val.str.rep[replaceEnd], &string[copyEnd]);
        NEditFree(replacedStr);
    }
    return True;
}

static int readSearchArgs(DataValue *argList, int nArgs, int *searchDirection,
	int *searchType, int *wrap, char **errMsg)
{
    int i;
    char *argStr, stringStorage[TYPE_INT_STR_SIZE(int)];
    
    *wrap = False;
    *searchDirection = SEARCH_FORWARD;
    *searchType = SEARCH_LITERAL;
    for (i=0; i<nArgs; i++) {
    	if (!readStringArg(argList[i], &argStr, stringStorage, errMsg))
    	    return False;
    	else if (!strcmp(argStr, "wrap"))
    	    *wrap = True;
    	else if (!strcmp(argStr, "nowrap"))
    	    *wrap = False;
    	else if (!strcmp(argStr, "backward"))
    	    *searchDirection = SEARCH_BACKWARD;
    	else if (!strcmp(argStr, "forward"))
    	    *searchDirection = SEARCH_FORWARD;
   	else if (!StringToSearchType(argStr, searchType)) {
    	    	*errMsg = "Unrecognized argument to %s";
    	    	return False;
	}
    }
    return True;
}

static int setCursorPosMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int pos;

    /* Get argument and convert to int */
    if (nArgs != 1)
    	return wrongNArgsErr(errMsg);
    if (!readIntArg(argList[0], &pos, errMsg))
    	return False;
    
    /* Set the position */
    TextSetCursorPos(window->lastFocus, pos);
    result->tag = NO_TAG;
    return True;
}

static int selectMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int start, end, startTmp;

    /* Get arguments and convert to int */
    if (nArgs != 2)
    	return wrongNArgsErr(errMsg);
    if (!readIntArg(argList[0], &start, errMsg))
    	return False;
    if (!readIntArg(argList[1], &end, errMsg))
    	return False;
    
    /* Verify integrity of arguments */
    if (start > end) {
    	startTmp = start;
	start = end;
	end = startTmp;
    }
    if (start < 0) start = 0;
    if (start > window->buffer->length) start = window->buffer->length;
    if (end < 0) end = 0;
    if (end > window->buffer->length) end = window->buffer->length;
    
    /* Make the selection */
    BufSelect(window->buffer, start, end);
    result->tag = NO_TAG;
    return True;
}

static int selectRectangleMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int start, end, left, right;

    /* Get arguments and convert to int */
    if (nArgs != 4)
    	return wrongNArgsErr(errMsg);
    if (!readIntArg(argList[0], &start, errMsg))
    	return False;
    if (!readIntArg(argList[1], &end, errMsg))
    	return False;
    if (!readIntArg(argList[2], &left, errMsg))
    	return False;
    if (!readIntArg(argList[3], &right, errMsg))
    	return False;
    
    /* Make the selection */
    BufRectSelect(window->buffer, start, end, left, right);
    result->tag = NO_TAG;
    return True;
}

/*
** Macro subroutine to ring the bell
*/
static int beepMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    if (nArgs != 0)
    	return wrongNArgsErr(errMsg);
    XBell(XtDisplay(window->shell), 0);
    result->tag = NO_TAG;
    return True;
}

static int tPrintMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[TYPE_INT_STR_SIZE(int)], *string;
    int i;
    
    if (nArgs == 0)
    	return tooFewArgsErr(errMsg);
    for (i=0; i<nArgs; i++) {
	if (!readStringArg(argList[i], &string, stringStorage, errMsg))
	    return False;
	printf("%s%s", string, i==nArgs-1 ? "" : " ");
    }
    fflush( stdout );
    result->tag = NO_TAG;
    return True;
}

/*
** Built-in macro subroutine for getting the value of an environment variable
*/
static int getenvMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[1][TYPE_INT_STR_SIZE(int)];
    char *name;
    char *value;

    /* Get name of variable to get */
    if (nArgs != 1)
      	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &name, stringStorage[0], errMsg)) {
	*errMsg = "argument to %s must be a string";
	return False;
    }
    value = getenv(name);
    if (value == NULL)
	value = "";
	
    /* Return the text as an allocated string */
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, value);
    return True;
}

static int shellCmdMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[2][TYPE_INT_STR_SIZE(int)], *cmdString, *inputString;

    if (nArgs != 2)
    	return wrongNArgsErr(errMsg);
    if (!readStringArg(argList[0], &cmdString, stringStorage[0], errMsg))
    	return False;
    if (!readStringArg(argList[1], &inputString, stringStorage[1], errMsg))
    	return False;
    
    /* Shell command execution requires that the macro be suspended, so
       this subroutine can't be run if macro execution can't be interrupted */
    if (MacroRunWindow()->macroCmdData == NULL) {
      *errMsg = "%s can't be called from non-suspendable context";
       return False;
    }
	
#ifdef VMS
    *errMsg = "Shell commands not supported under VMS";
    return False;
#else
    ShellCmdToMacroString(window, cmdString, inputString);
    result->tag = INT_TAG;
    result->val.n = 0;
    return True;
#endif /*VMS*/
}

/*
** Method used by ShellCmdToMacroString (called by shellCmdMS), for returning
** macro string and exit status after the execution of a shell command is
** complete.  (Sorry about the poor modularity here, it's just not worth
** teaching other modules about macro return globals, since other than this,
** they're not used outside of macro.c)
*/
void ReturnShellCommandOutput(WindowInfo *window, const char *outText, int status)
{
    DataValue retVal;
    macroCmdInfo *cmdData = window->macroCmdData;
    
    if (cmdData == NULL)
    	return;
    retVal.tag = STRING_TAG;
    AllocNStringCpy(&retVal.val.str, outText);
    ModifyReturnedValue(cmdData->context, retVal);
    ReturnGlobals[SHELL_CMD_STATUS]->value.tag = INT_TAG;
    ReturnGlobals[SHELL_CMD_STATUS]->value.val.n = status;
}

static int dialogMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    macroCmdInfo *cmdData;
    char stringStorage[TYPE_INT_STR_SIZE(int)];
    char btnStorage[TYPE_INT_STR_SIZE(int)];
    char *btnLabel;
    char *message;
    Arg al[20];
    int ac;
    Widget dialog, btn;
    int i, nBtns;
    XmString s1, s2;
    
    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    window = MacroRunWindow();
    cmdData = window->macroCmdData;
    
    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
      *errMsg = "%s can't be called from non-suspendable context";
       return False;
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (nArgs == 0) {
    	*errMsg = "%s subroutine called with no arguments";
    	return False;
    }
    if (!readStringArg(argList[0], &message, stringStorage, errMsg)) {
        return False;
    }

    /* check that all button labels can be read */
    for (i=1; i<nArgs; i++) {
        if (!readStringArg(argList[i], &btnLabel, btnStorage, errMsg)) {
            return False;
        }
    }

    /* pick up the first button */
    if (nArgs == 1) {
        btnLabel = "OK";
        nBtns = 1;
    }
    else {
        nBtns = nArgs - 1;
        argList++;
        readStringArg(argList[0], &btnLabel, btnStorage, errMsg);
    }

    /* Create the message box dialog widget and its dialog shell parent */
    ac = 0;
    XtSetArg(al[ac], XmNtitle, " "); ac++;
    XtSetArg(al[ac], XmNmessageString, s1=MKSTRING(message)); ac++;
    XtSetArg(al[ac], XmNokLabelString, s2=XmStringCreateSimple(btnLabel)); ac++;
    dialog = CreateMessageDialog(window->shell, "macroDialog", al, ac);
    if (1 == nArgs)
    {
        /*  Only set margin width for the default OK button  */
        XtVaSetValues(XmMessageBoxGetChild(dialog, XmDIALOG_OK_BUTTON),
                XmNmarginWidth, BUTTON_WIDTH_MARGIN,
                NULL);
    }

    XmStringFree(s1);
    XmStringFree(s2);
    AddMotifCloseCallback(XtParent(dialog), dialogCloseCB, window);
    XtAddCallback(dialog, XmNokCallback, dialogBtnCB, window);
    XtVaSetValues(XmMessageBoxGetChild(dialog, XmDIALOG_OK_BUTTON),
    	    XmNuserData, (XtPointer)1, NULL);
    cmdData->dialog = dialog;

    /* Unmanage default buttons, except for "OK" */
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    
    /* Make callback for the unmanaged cancel button (which can
       still get executed via the esc key) activate close box action */
    XtAddCallback(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON),
	    XmNactivateCallback, dialogCloseCB, window);

    /* Add user specified buttons (1st is already done) */
    for (i=1; i<nBtns; i++) {
        readStringArg(argList[i], &btnLabel, btnStorage, errMsg);
    	btn = XtVaCreateManagedWidget("mdBtn", xmPushButtonWidgetClass, dialog,
    	    	XmNlabelString, s1=XmStringCreateSimple(btnLabel),
    	    	XmNuserData, (XtPointer)(intptr_t)(i+1), NULL);
    	XtAddCallback(btn, XmNactivateCallback, dialogBtnCB, window);
    	XmStringFree(s1);
    }

#ifdef LESSTIF_VERSION
    /* Workaround for Lesstif (e.g. v2.1 r0.93.18) that doesn't handle
       the escape key for closing the dialog (probably because the
       cancel button is not managed). */
    XtAddEventHandler(dialog, KeyPressMask, False, dialogEscCB, 
            (XtPointer)window);
    XtGrabKey(dialog, XKeysymToKeycode(XtDisplay(dialog), XK_Escape), 0,
	    True, GrabModeAsync, GrabModeAsync);
#endif /* LESSTIF_VERSION */
    
    /* Put up the dialog */
    ManageDialogCenteredOnPointer(dialog);
    
    /* Stop macro execution until the dialog is complete */
    PreemptMacro();
    
    /* Return placeholder result.  Value will be changed by button callback */
    result->tag = INT_TAG;
    result->val.n = 0;
    return True;
}

static void dialogBtnCB(Widget w, XtPointer clientData, XtPointer callData)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    XtPointer userData;
    DataValue retVal;
    
    /* Return the index of the button which was pressed (stored in the userData
       field of the button widget).  The 1st button, being a gadget, is not
       returned in w. */
    if (cmdData == NULL)
    	return; /* shouldn't happen */
    if (XtClass(w) == xmPushButtonWidgetClass) {
	XtVaGetValues(w, XmNuserData, &userData, NULL);
	retVal.val.n = (int)(intptr_t)userData;
    } else
    	retVal.val.n = 1;
    retVal.tag = INT_TAG;
    ModifyReturnedValue(cmdData->context, retVal);

    /* Pop down the dialog */
    XtDestroyWidget(XtParent(cmdData->dialog));
    cmdData->dialog = NULL;

    /* Continue preempted macro execution */
    ResumeMacroExecution(window);
}

static void dialogCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    DataValue retVal;
    
    /* Return 0 to show that the dialog was closed via the window close box */
    retVal.val.n = 0;
    retVal.tag = INT_TAG;
    ModifyReturnedValue(cmdData->context, retVal);

    /* Pop down the dialog */
    XtDestroyWidget(XtParent(cmdData->dialog));
    cmdData->dialog = NULL;

    /* Continue preempted macro execution */
    ResumeMacroExecution(window);
}

#ifdef LESSTIF_VERSION
static void dialogEscCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *cont)
{
    if (event->xkey.keycode != XKeysymToKeycode(XtDisplay(w), XK_Escape))
    	return;
    if (clientData != NULL) {
        dialogCloseCB(w, (WindowInfo *)clientData, NULL);
    }
    *cont = False;
}
#endif /* LESSTIF_VERSION */

static int stringDialogMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    macroCmdInfo *cmdData;
    char stringStorage[TYPE_INT_STR_SIZE(int)];
    char btnStorage[TYPE_INT_STR_SIZE(int)];
    char *btnLabel;
    char *message;
    Widget dialog, btn;
    int i, nBtns;
    XmString s1, s2;
    Arg al[20];
    int ac;
    
    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    window = MacroRunWindow();
    cmdData = window->macroCmdData;
    
    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
      *errMsg = "%s can't be called from non-suspendable context";
       return False;
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (nArgs == 0) {
    	*errMsg = "%s subroutine called with no arguments";
    	return False;
    }
    if (!readStringArg(argList[0], &message, stringStorage, errMsg)) {
        return False;
    }
    /* check that all button labels can be read */
    for (i=1; i<nArgs; i++) {
        if (!readStringArg(argList[i], &btnLabel, stringStorage, errMsg)) {
            return False;
        }
    }
    if (nArgs == 1) {
        btnLabel = "OK";
        nBtns = 1;
    }
    else {
        nBtns = nArgs - 1;
        argList++;
        readStringArg(argList[0], &btnLabel, btnStorage, errMsg);
    }

    /* Create the selection box dialog widget and its dialog shell parent */
    ac = 0;
    XtSetArg(al[ac], XmNtitle, " "); ac++;
    XtSetArg(al[ac], XmNselectionLabelString, s1=MKSTRING(message)); ac++;
    XtSetArg(al[ac], XmNokLabelString, s2=XmStringCreateSimple(btnLabel)); ac++;
    dialog = CreatePromptDialog(window->shell, "macroStringDialog", al, ac);
    if (1 == nArgs)
    {
        /*  Only set margin width for the default OK button  */
        XtVaSetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_OK_BUTTON),
                XmNmarginWidth, BUTTON_WIDTH_MARGIN,
                NULL);
    }

    XmStringFree(s1);
    XmStringFree(s2);
    AddMotifCloseCallback(XtParent(dialog), stringDialogCloseCB, window);
    XtAddCallback(dialog, XmNokCallback, stringDialogBtnCB, window);
    XtVaSetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_OK_BUTTON),
    	    XmNuserData, (XtPointer)1, NULL);
    cmdData->dialog = dialog;

    /* Unmanage unneded widgets */
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    
    /* Make callback for the unmanaged cancel button (which can
       still get executed via the esc key) activate close box action */
    XtAddCallback(XmSelectionBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON),
	    XmNactivateCallback, stringDialogCloseCB, window);

    /* Add user specified buttons (1st is already done).  Selection box
       requires a place-holder widget to be added before buttons can be
       added, that's what the separator below is for */
    XtVaCreateWidget("x", xmSeparatorWidgetClass, dialog, NULL);
    for (i=1; i<nBtns; i++) {
        readStringArg(argList[i], &btnLabel, btnStorage, errMsg);
    	btn = XtVaCreateManagedWidget("mdBtn", xmPushButtonWidgetClass, dialog,
    	    	XmNlabelString, s1=XmStringCreateSimple(btnLabel),
    	    	XmNuserData, (XtPointer)(intptr_t)(i+1), NULL);
    	XtAddCallback(btn, XmNactivateCallback, stringDialogBtnCB, window);
    	XmStringFree(s1);
    }
    
#ifdef LESSTIF_VERSION
    /* Workaround for Lesstif (e.g. v2.1 r0.93.18) that doesn't handle
       the escape key for closing the dialog (probably because the
       cancel button is not managed). */
    XtAddEventHandler(dialog, KeyPressMask, False, stringDialogEscCB, 
            (XtPointer)window);
    XtGrabKey(dialog, XKeysymToKeycode(XtDisplay(dialog), XK_Escape), 0,
	    True, GrabModeAsync, GrabModeAsync);
#endif /* LESSTIF_VERSION */

    /* Put up the dialog */
    ManageDialogCenteredOnPointer(dialog);
    
    /* Stop macro execution until the dialog is complete */
    PreemptMacro();
    
    /* Return placeholder result.  Value will be changed by button callback */
    result->tag = INT_TAG;
    result->val.n = 0;
    return True;
}

static void stringDialogBtnCB(Widget w, XtPointer clientData,
    	XtPointer callData)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    XtPointer userData;
    DataValue retVal;
    char *text;
    int btnNum;

    /* shouldn't happen, but would crash if it did */
    if (cmdData == NULL)
    	return; 

    /* Return the string entered in the selection text area */
    text = XmTextGetString(XmSelectionBoxGetChild(cmdData->dialog,
    	    XmDIALOG_TEXT));
    retVal.tag = STRING_TAG;
    AllocNStringCpy(&retVal.val.str, text);
    NEditFree(text);
    ModifyReturnedValue(cmdData->context, retVal);
    
    /* Find the index of the button which was pressed (stored in the userData
       field of the button widget).  The 1st button, being a gadget, is not
       returned in w. */
    if (XtClass(w) == xmPushButtonWidgetClass) {
	XtVaGetValues(w, XmNuserData, &userData, NULL);
	btnNum = (int)(intptr_t)userData;
    } else
    	btnNum = 1;
    
    /* Return the button number in the global variable $string_dialog_button */
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.tag = INT_TAG;
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.val.n = btnNum;

    /* Pop down the dialog */
    XtDestroyWidget(XtParent(cmdData->dialog));
    cmdData->dialog = NULL;

    /* Continue preempted macro execution */
    ResumeMacroExecution(window);
}

static void stringDialogCloseCB(Widget w, XtPointer clientData,
    	XtPointer callData)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    DataValue retVal;

    /* shouldn't happen, but would crash if it did */
    if (cmdData == NULL)
    	return; 

    /* Return an empty string */
    retVal.tag = STRING_TAG;
    retVal.val.str.rep = PERM_ALLOC_STR("");
    retVal.val.str.len = 0;
    ModifyReturnedValue(cmdData->context, retVal);
    
    /* Return button number 0 in the global variable $string_dialog_button */
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.tag = INT_TAG;
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.val.n = 0;

    /* Pop down the dialog */
    XtDestroyWidget(XtParent(cmdData->dialog));
    cmdData->dialog = NULL;

    /* Continue preempted macro execution */
    ResumeMacroExecution(window);
}

#ifdef LESSTIF_VERSION
static void stringDialogEscCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *cont)
{
    if (event->xkey.keycode != XKeysymToKeycode(XtDisplay(w), XK_Escape))
    	return;
    if (clientData != NULL) {
        stringDialogCloseCB(w, (WindowInfo *)clientData, NULL);
    }
    *cont = False;
}
#endif /* LESSTIF_VERSION */

/*
** A subroutine to put up a calltip
** First arg is either text to be displayed or a key for tip/tag lookup.
** Optional second arg is the buffer position beneath which to display the 
**      upper-left corner of the tip.  Default (or -1) puts it under the cursor.
** Additional optional arguments:
**      "tipText": (default) Indicates first arg is text to be displayed in tip.
**      "tipKey":   Indicates first arg is key in calltips database.  If key
**                  is not found in tip database then the tags database is also 
**                  searched.
**      "tagKey":   Indicates first arg is key in tags database.  (Skips 
**                  search in calltips database.)
**      "center":   Horizontally center the calltip at the position
**      "right":    Put the right edge of the calltip at the position
**                  "center" and "right" cannot both be specified.
**      "above":    Place the calltip above the position
**      "strict":   Don't move the calltip to keep it on-screen and away
**                  from the cursor's line.
** 
** Returns the new calltip's ID on success, 0 on failure.
**
** Does this need to go on IgnoredActions?  I don't think so, since
** showing a calltip may be part of the action you want to learn.
*/
static int calltipMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    char stringStorage[TYPE_INT_STR_SIZE(int)], *tipText, *txtArg;
    Boolean anchored = False, lookup = True;
    int mode = -1, i;
    int anchorPos, hAlign = TIP_LEFT, vAlign = TIP_BELOW, 
            alignMode = TIP_SLOPPY;
    
    /* Read and check the string */
    if (nArgs < 1) {
        *errMsg = "%s subroutine called with too few arguments";
        return False;
    }
    if (nArgs > 6) {
        *errMsg = "%s subroutine called with too many arguments";
        return False;
    }
    
    /* Read the tip text or key */
    if (!readStringArg(argList[0], &tipText, stringStorage, errMsg))
        return False;

    /* Read the anchor position (-1 for unanchored) */
    if (nArgs > 1) {
        if (!readIntArg(argList[1], &anchorPos, errMsg))
            return False;
    } else {
        anchorPos = -1;
    }
    if (anchorPos >= 0) anchored = True;
    
    /* Any further args are directives for relative positioning */
    for (i = 2; i < nArgs; ++i) {
        if (!readStringArg(argList[i], &txtArg, stringStorage, errMsg)){
            return False;
        }
        switch( txtArg[0] ) {
          case 'c':
            if (strcmp(txtArg, "center"))
                goto bad_arg;
            hAlign = TIP_CENTER;
            break;
          case 'r':
            if (strcmp(txtArg, "right"))
                goto bad_arg;
            hAlign = TIP_RIGHT;
            break;
          case 'a':
            if (strcmp(txtArg, "above"))
                goto bad_arg;
            vAlign = TIP_ABOVE;
            break;
          case 's':
            if (strcmp(txtArg, "strict"))
                goto bad_arg;
            alignMode = TIP_STRICT;
            break;
          case 't':
            if (!strcmp(txtArg, "tipText"))
                mode = -1;
            else if (!strcmp(txtArg, "tipKey"))
                mode = TIP;
            else if (!strcmp(txtArg, "tagKey"))
                mode = TIP_FROM_TAG;
            else
                goto bad_arg;
            break;
          default:
            goto bad_arg;
        }
    }
    
    result->tag = INT_TAG;
    if (mode < 0) lookup = False;
    /* Look up (maybe) a calltip and display it */
    result->val.n = ShowTipString( window, tipText, anchored, anchorPos, lookup,
                                 mode, hAlign, vAlign, alignMode );

    return True;
    
bad_arg:
    /* This is how the (more informative) global var. version would work,
        assuming there was a global buffer called msg.  */
    /* sprintf(msg, "unrecognized argument to %%s: \"%s\"", txtArg);
    *errMsg = msg; */
    *errMsg = "unrecognized argument to %s";
    return False;
}

/*
** A subroutine to kill the current calltip
*/
static int killCalltipMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    int calltipID = 0;
    
    if (nArgs > 1) {
        *errMsg = "%s subroutine called with too many arguments";
        return False;
    }
    if (nArgs > 0) {
        if (!readIntArg(argList[0], &calltipID, errMsg))
            return False;
    }
    
    KillCalltip( window, calltipID );
    
    result->tag = NO_TAG;
    return True;
}

/*
 * A subroutine to get the ID of the current calltip, or 0 if there is none.
 */
static int calltipIDMV(WindowInfo *window, DataValue *argList, 
        int nArgs, DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = GetCalltipID(window, 0);
    return True;
}

/*
**  filename_dialog([title[, mode[, defaultPath[, filter[, defaultName]]]]])
**
**  Presents a FileSelectionDialog to the user prompting for a new file.
**
**  Options are:
**  title       - will be the title of the dialog, defaults to "Choose file".
**  mode        - if set to "exist" (default), the "New File Name" TextField
**                of the FSB will be unmanaged. If "new", the TextField will
**                be managed.
**  defaultPath - is the default path to use. Default (or "") will use the
**                active document's directory.
**  filter      - the file glob which determines which files to display.
**                Is set to "*" if filter is "" and by default.
**  defaultName - is the default filename that is filled in automatically.
**
** Returns "" if the user cancelled the dialog, otherwise returns the path to
** the file that was selected
**
** Note that defaultName doesn't work on all *tifs.  :-(
*/
static int filenameDialogMS(WindowInfo* window, DataValue* argList, int nArgs,
        DataValue* result, char** errMsg)
{
    char stringStorage[5][TYPE_INT_STR_SIZE(int)];
    char filename[MAXPATHLEN + 1];
    char* title = "Choose Filename";
    char* mode = "exist";
    char* defaultPath = "";
    char* filter = "";
    char* defaultName = "";
    char* orgDefaultPath;
    char* orgFilter;
    int gfnResult;

    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    window = MacroRunWindow();

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (NULL == window->macroCmdData) {
        M_FAILURE("%s can't be called from non-suspendable context");
    }

    /*  Get the argument list.  */
    if (nArgs > 0 && !readStringArg(argList[0], &title, stringStorage[0],
            errMsg)) {
        return False;
    }

    if (nArgs > 1 && !readStringArg(argList[1], &mode, stringStorage[1],
            errMsg)) {
        return False;
    }
    if (0 != strcmp(mode, "exist") && 0 != strcmp(mode, "new")) {
        M_FAILURE("Invalid value for mode in %s");
    }

    if (nArgs > 2 && !readStringArg(argList[2], &defaultPath, stringStorage[2],
            errMsg)) {
        return False;
    }

    if (nArgs > 3 && !readStringArg(argList[3], &filter, stringStorage[3],
            errMsg)) {
        return False;
    }

    if (nArgs > 4 && !readStringArg(argList[4], &defaultName, stringStorage[4],
            errMsg)) {
        return False;
    }

    if (nArgs > 5) {
        M_FAILURE("%s called with too many arguments. Expects at most 5 arguments.");
    }

    /*  Set default directory (saving original for later)  */
    orgDefaultPath = GetFileDialogDefaultDirectory();
    if ('\0' != defaultPath[0]) {
        SetFileDialogDefaultDirectory(defaultPath);
    } else {
        SetFileDialogDefaultDirectory(window->path);
    }

    /*  Set filter (saving original for later)  */
    orgFilter = GetFileDialogDefaultPattern();
    if ('\0' != filter[0]) {
        SetFileDialogDefaultPattern(filter);
    }

    /*  Fork to one of the worker methods from util/getfiles.c.
        (This should obviously be refactored.)  */
    if (0 == strcmp(mode, "exist")) {
        gfnResult = GetExistingFilename(window->shell, title, filename);
    } else {
        gfnResult = GetNewFilename(window->shell, title, filename, defaultName);
    }   /*  Invalid values are weeded out above.  */ 

    /*  Reset original values and free temps  */
    SetFileDialogDefaultDirectory(orgDefaultPath);
    SetFileDialogDefaultPattern(orgFilter);
    NEditFree(orgDefaultPath);
    NEditFree(orgFilter);

    result->tag = STRING_TAG;
    if (GFN_OK == gfnResult) {
        /*  Got a string, copy it to the result  */
        if (!AllocNStringNCpy(&result->val.str, filename, MAXPATHLEN)) {
            M_FAILURE("failed to allocate return value: %s");
        }
    } else {
        /* User cancelled.  Return "" */
        result->val.str.rep = PERM_ALLOC_STR("");
        result->val.str.len = 0;
    }

    return True;
}

/* T Balinski */
static int listDialogMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    macroCmdInfo *cmdData;
    char stringStorage[TYPE_INT_STR_SIZE(int)];
    char textStorage[TYPE_INT_STR_SIZE(int)];
    char btnStorage[TYPE_INT_STR_SIZE(int)];
    char *btnLabel;
    char *message, *text;
    Widget dialog, btn;
    int i, nBtns;
    XmString s1, s2;
    long nlines = 0;
    char *p, *old_p, **text_lines, *tmp;
    int tmp_len;
    int n, is_last;
    XmString *test_strings;
    int tabDist;
    Arg al[20];
    int ac;

  
    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    window = MacroRunWindow();
    cmdData = window->macroCmdData;
    
    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
      *errMsg = "%s can't be called from non-suspendable context";
       return False;
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (nArgs < 2) {
      *errMsg = "%s subroutine called with no message, string or arguments";
      return False;
    }

    if (!readStringArg(argList[0], &message, stringStorage, errMsg))
      return False;

    if (!readStringArg(argList[1], &text, textStorage, errMsg))
      return False;

    if (!text || text[0] == '\0') {
      *errMsg = "%s subroutine called with empty list data";
      return False;
    }

    /* check that all button labels can be read */
    for (i=2; i<nArgs; i++)
      if (!readStringArg(argList[i], &btnLabel, btnStorage, errMsg))
          return False;

    /* pick up the first button */
    if (nArgs == 2) {
      btnLabel = "OK";
      nBtns = 1;
    }
    else {
      nBtns = nArgs - 2;
      argList += 2;
      readStringArg(argList[0], &btnLabel, btnStorage, errMsg);
    }

    /* count the lines in the text - add one for unterminated last line */
    nlines = 1;
    for (p = text; *p; p++)
      if (*p == '\n')
          nlines++;

    /* now set up arrays of pointers to lines */
    /*   test_strings to hold the display strings (tab expanded) */
    /*   text_lines to hold the original text lines (without the '\n's) */
    test_strings = (XmString *) NEditMalloc(sizeof(XmString) * nlines);
    text_lines = (char **)NEditMalloc(sizeof(char *) * (nlines + 1));
    for (n = 0; n < nlines; n++) {
      test_strings[n] = (XmString)0;
      text_lines[n] = (char *)0;
    }
    text_lines[n] = (char *)0;        /* make sure this is a null-terminated table */

    /* pick up the tabDist value */
    tabDist = window->buffer->tabDist;

    /* load the table */
    n = 0;
    is_last = 0;
    p = old_p = text;
    tmp_len = 0;      /* current allocated size of temporary buffer tmp */
    tmp = (char*)NEditMalloc(1);  /* temporary buffer into which to expand tabs */
    do {
      is_last = (*p == '\0');
      if (*p == '\n' || is_last) {
          *p = '\0';
          if (strlen(old_p) > 0) {    /* only include non-empty lines */
              char *s, *t;
              int l;

              /* save the actual text line in text_lines[n] */
              text_lines[n] = (char *)NEditMalloc(strlen(old_p) + 1);
              strcpy(text_lines[n], old_p);

              /* work out the tabs expanded length */
              for (s = old_p, l = 0; *s; s++)
                  l += (*s == '\t') ? tabDist - (l % tabDist) : 1;

              /* verify tmp is big enough then tab-expand old_p into tmp */
              if (l > tmp_len)
                  tmp = (char*)NEditRealloc(tmp, (tmp_len = l) + 1);
              for (s = old_p, t = tmp, l = 0; *s; s++) {
                  if (*s == '\t') {
                      for (i = tabDist - (l % tabDist); i--; l++)
                          *t++ = ' ';
                  }
                  else {
                    *t++ = *s;
                    l++;
                  }
              }
              *t = '\0';
              /* that's it: tmp is the tab-expanded version of old_p */
              test_strings[n] = MKSTRING(tmp);
              n++;
          }
          old_p = p + 1;
          if (!is_last)
              *p = '\n';              /* put back our newline */
      }
      p++;
    } while (!is_last);

    NEditFree(tmp);                /* don't need this anymore */
    nlines = n;
    if (nlines == 0) {
      test_strings[0] = MKSTRING("");
      nlines = 1;
    }

    /* Create the selection box dialog widget and its dialog shell parent */
    ac = 0;
    XtSetArg(al[ac], XmNtitle, " "); ac++;
    XtSetArg(al[ac], XmNlistLabelString, s1=MKSTRING(message)); ac++;
    XtSetArg(al[ac], XmNlistItems, test_strings); ac++;
    XtSetArg(al[ac], XmNlistItemCount, nlines); ac++;
    XtSetArg(al[ac], XmNlistVisibleItemCount, (nlines > 10) ? 10 : nlines); ac++;
    XtSetArg(al[ac], XmNokLabelString, s2=XmStringCreateSimple(btnLabel)); ac++;
    dialog = CreateSelectionDialog(window->shell, "macroListDialog", al, ac);
    if (2 == nArgs)
    {
        /*  Only set margin width for the default OK button  */
        XtVaSetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_OK_BUTTON),
                XmNmarginWidth, BUTTON_WIDTH_MARGIN,
                NULL);
    }

    AddMotifCloseCallback(XtParent(dialog), listDialogCloseCB, window);
    XtAddCallback(dialog, XmNokCallback, listDialogBtnCB, window);
    XtVaSetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_OK_BUTTON),
          XmNuserData, (XtPointer)1, NULL);
    XmStringFree(s1);
    XmStringFree(s2);
    cmdData->dialog = dialog;

    /* forget lines stored in list */
    while (n--)
        XmStringFree(test_strings[n]);
    NEditFree(test_strings);

    /* modify the list */
    XtVaSetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_LIST),
                XmNselectionPolicy, XmSINGLE_SELECT,
                XmNuserData, (XtPointer)text_lines, NULL);

    /* Unmanage unneeded widgets */
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_APPLY_BUTTON));
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_SELECTION_LABEL));
    
    /* Make callback for the unmanaged cancel button (which can
       still get executed via the esc key) activate close box action */
    XtAddCallback(XmSelectionBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON),
	    XmNactivateCallback, listDialogCloseCB, window);

    /* Add user specified buttons (1st is already done).  Selection box
       requires a place-holder widget to be added before buttons can be
       added, that's what the separator below is for */
    XtVaCreateWidget("x", xmSeparatorWidgetClass, dialog, NULL);
    for (i=1; i<nBtns; i++) {
      readStringArg(argList[i], &btnLabel, btnStorage, errMsg);
      btn = XtVaCreateManagedWidget("mdBtn", xmPushButtonWidgetClass, dialog,
              XmNlabelString, s1=XmStringCreateSimple(btnLabel),
              XmNuserData, (XtPointer)(intptr_t)(i+1), NULL);
      XtAddCallback(btn, XmNactivateCallback, listDialogBtnCB, window);
      XmStringFree(s1);
    }
    
#ifdef LESSTIF_VERSION
    /* Workaround for Lesstif (e.g. v2.1 r0.93.18) that doesn't handle
       the escape key for closing the dialog. */
    XtAddEventHandler(dialog, KeyPressMask, False, listDialogEscCB, 
            (XtPointer)window);
    XtGrabKey(dialog, XKeysymToKeycode(XtDisplay(dialog), XK_Escape), 0,
	    True, GrabModeAsync, GrabModeAsync);
#endif /* LESSTIF_VERSION */
    
    /* Put up the dialog */
    ManageDialogCenteredOnPointer(dialog);
    
    /* Stop macro execution until the dialog is complete */
    PreemptMacro();
    
    /* Return placeholder result.  Value will be changed by button callback */
    result->tag = INT_TAG;
    result->val.n = 0;
    return True;
}

static void listDialogBtnCB(Widget w, XtPointer clientData,
      XtPointer callData)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    XtPointer userData;
    DataValue retVal;
    char *text;
    char **text_lines;
    int btnNum;
    int n_sel, *seltable, sel_index = 0;
    Widget theList;
    size_t length;

    /* shouldn't happen, but would crash if it did */
    if (cmdData == NULL)
      return; 

    theList = XmSelectionBoxGetChild(cmdData->dialog, XmDIALOG_LIST);
    /* Return the string selected in the selection list area */
    XtVaGetValues(theList, XmNuserData, &text_lines, NULL);
    if (!XmListGetSelectedPos(theList, &seltable, &n_sel)) {
      n_sel = 0;
    }
    else {
      sel_index = seltable[0] - 1;
      NEditFree(seltable);
    }

    if (!n_sel) {
      text = NEditStrdup("");
      length = 0;
    }
    else {
      length = strlen((char *)text_lines[sel_index]);
      text = NEditStrdup(text_lines[sel_index]);
    }

    /* don't need text_lines anymore: free it */
    for (sel_index = 0; text_lines[sel_index]; sel_index++)
      NEditFree(text_lines[sel_index]);
    NEditFree(text_lines);

    retVal.tag = STRING_TAG;
    retVal.val.str.rep = text;
    retVal.val.str.len = length;
    ModifyReturnedValue(cmdData->context, retVal);
    
    /* Find the index of the button which was pressed (stored in the userData
       field of the button widget).  The 1st button, being a gadget, is not
       returned in w. */
    if (XtClass(w) == xmPushButtonWidgetClass) {
      XtVaGetValues(w, XmNuserData, &userData, NULL);
      btnNum = (int)(intptr_t)userData;
    } else
      btnNum = 1;
    
    /* Return the button number in the global variable $list_dialog_button */
    ReturnGlobals[LIST_DIALOG_BUTTON]->value.tag = INT_TAG;
    ReturnGlobals[LIST_DIALOG_BUTTON]->value.val.n = btnNum;

    /* Pop down the dialog */
    XtDestroyWidget(XtParent(cmdData->dialog));
    cmdData->dialog = NULL;

    /* Continue preempted macro execution */
    ResumeMacroExecution(window);
}

static void listDialogCloseCB(Widget w, XtPointer clientData,
      XtPointer callData)
{
    WindowInfo *window = (WindowInfo *)clientData;
    macroCmdInfo *cmdData = window->macroCmdData;
    DataValue retVal;
    char **text_lines;
    int sel_index;
    Widget theList;

    /* shouldn't happen, but would crash if it did */
    if (cmdData == NULL)
      return; 

    /* don't need text_lines anymore: retrieve it then free it */
    theList = XmSelectionBoxGetChild(cmdData->dialog, XmDIALOG_LIST);
    XtVaGetValues(theList, XmNuserData, &text_lines, NULL);
    for (sel_index = 0; text_lines[sel_index]; sel_index++)
      NEditFree(text_lines[sel_index]);
    NEditFree(text_lines);

    /* Return an empty string */
    retVal.tag = STRING_TAG;
    retVal.val.str.rep = NEditStrdup("");
    retVal.val.str.len = 0;
    ModifyReturnedValue(cmdData->context, retVal);
    
    /* Return button number 0 in the global variable $list_dialog_button */
    ReturnGlobals[LIST_DIALOG_BUTTON]->value.tag = INT_TAG;
    ReturnGlobals[LIST_DIALOG_BUTTON]->value.val.n = 0;

    /* Pop down the dialog */
    XtDestroyWidget(XtParent(cmdData->dialog));
    cmdData->dialog = NULL;

    /* Continue preempted macro execution */
    ResumeMacroExecution(window);
}
/* T Balinski End */

#ifdef LESSTIF_VERSION
static void listDialogEscCB(Widget w, XtPointer clientData, XEvent *event,
    	Boolean *cont)
{
    if (event->xkey.keycode != XKeysymToKeycode(XtDisplay(w), XK_Escape))
    	return;
    if (clientData != NULL) {
        listDialogCloseCB(w, (WindowInfo *)clientData, NULL);
    }
    *cont = False;
}
#endif /* LESSTIF_VERSION */


static int stringCompareMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[3][TYPE_INT_STR_SIZE(int)];
    char *leftStr, *rightStr, *argStr;
    int considerCase = True;
    int i;
    int compareResult;
    
    if (nArgs < 2) {
	return(wrongNArgsErr(errMsg));
    }
    if (!readStringArg(argList[0], &leftStr, stringStorage[0], errMsg))
        return False;
    if (!readStringArg(argList[1], &rightStr, stringStorage[1], errMsg))
        return False;
    for (i = 2; i < nArgs; ++i) {
    	if (!readStringArg(argList[i], &argStr, stringStorage[2], errMsg))
    	    return False;
    	else if (!strcmp(argStr, "case"))
    	    considerCase = True;
    	else if (!strcmp(argStr, "nocase"))
    	    considerCase = False;
    	else {
    	    *errMsg = "Unrecognized argument to %s";
    	    return False;
    	}
    }
    if (considerCase) {
        compareResult = strcmp(leftStr, rightStr);
        compareResult = (compareResult > 0) ? 1 : ((compareResult < 0) ? -1 : 0);
    }
    else {
        compareResult = strCaseCmp(leftStr, rightStr);
    }
    result->tag = INT_TAG;
    result->val.n = compareResult;
    return True;
}

/*
** This function is intended to split strings into an array of substrings
** Importatnt note: It should always return at least one entry with key 0
** split("", ",") result[0] = ""
** split("1,2", ",") result[0] = "1" result[1] = "2"
** split("1,2,", ",") result[0] = "1" result[1] = "2" result[2] = ""
** 
** This behavior is specifically important when used to break up
** array sub-scripts
*/

static int splitMS(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char stringStorage[3][TYPE_INT_STR_SIZE(int)];
    char *sourceStr, *splitStr, *typeSplitStr;
    int searchType, beginPos, foundStart, foundEnd, strLength, lastEnd;
    int found, elementEnd, indexNum;
    char indexStr[TYPE_INT_STR_SIZE(int)], *allocIndexStr;
    DataValue element;
    int elementLen;
    
    if (nArgs < 2) {
        return(wrongNArgsErr(errMsg));
    }
    if (!readStringArg(argList[0], &sourceStr, stringStorage[0], errMsg)) {
        *errMsg = "first argument must be a string: %s";
        return(False);
    }
    if (!readStringArg(argList[1], &splitStr, stringStorage[1], errMsg)) {
        splitStr = NULL;
    }
    else {
        if (splitStr[0] == 0) {
            splitStr = NULL;
        }
    }
    if (splitStr == NULL) {
        *errMsg = "second argument must be a non-empty string: %s";
        return(False);
    }
    if (nArgs > 2 && readStringArg(argList[2], &typeSplitStr, stringStorage[2], errMsg)) {
      	if (!StringToSearchType(typeSplitStr, &searchType)) {
            *errMsg = "unrecognized argument to %s";
            return(False);
        }
    }
    else {
    	searchType = SEARCH_LITERAL;
    }
    
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    beginPos = 0;
    lastEnd = 0;
    indexNum = 0;
    strLength = strlen(sourceStr);
    found = 1;
    while (found && beginPos < strLength) {
        sprintf(indexStr, "%d", indexNum);
        allocIndexStr = AllocString(strlen(indexStr) + 1);
        if (!allocIndexStr) {
            *errMsg = "array element failed to allocate key: %s";
            return(False);
        }
        strcpy(allocIndexStr, indexStr);
        found = SearchString(sourceStr, splitStr, SEARCH_FORWARD, searchType,
            False, beginPos, &foundStart, &foundEnd,
	        NULL, NULL, GetWindowDelimiters(window));
        elementEnd = found ? foundStart : strLength;
        elementLen = elementEnd - lastEnd;
        element.tag = STRING_TAG;
        if (!AllocNStringNCpy(&element.val.str, &sourceStr[lastEnd], elementLen)) {
            *errMsg = "failed to allocate element value: %s";
            return(False);
        }

        if (!ArrayInsert(result, allocIndexStr, &element)) {
            M_ARRAY_INSERT_FAILURE();
        }

        if (found) {
            if (foundStart == foundEnd) {
                beginPos = foundEnd + 1; /* Avoid endless loop for 0-width match */
            } else {
                beginPos = foundEnd;
            }
        } else {
            beginPos = strLength; /* Break the loop */
        }
        lastEnd = foundEnd;
        ++indexNum;
    }
    if (found) {
        sprintf(indexStr, "%d", indexNum);
        allocIndexStr = AllocString(strlen(indexStr) + 1);
        if (!allocIndexStr) {
            *errMsg = "array element failed to allocate key: %s";
            return(False);
        }
        strcpy(allocIndexStr, indexStr);
        element.tag = STRING_TAG;
        if (lastEnd == strLength) {
            /* The pattern mathed the end of the string. Add an empty chunk. */
            element.val.str.rep = PERM_ALLOC_STR("");
            element.val.str.len = 0;

            if (!ArrayInsert(result, allocIndexStr, &element)) {
                M_ARRAY_INSERT_FAILURE();
            }
        } else {
            /* We skipped the last character to prevent an endless loop. 
               Add it to the list. */
            elementLen = strLength - lastEnd;
            if (!AllocNStringNCpy(&element.val.str, &sourceStr[lastEnd], elementLen)) {
                *errMsg = "failed to allocate element value: %s";
                return(False);
            }

            if (!ArrayInsert(result, allocIndexStr, &element)) {
                M_ARRAY_INSERT_FAILURE();
            }

            /* If the pattern can match zero-length strings, we may have to
               add a final empty chunk. 
               For instance:  split("abc\n", "$", "regex")
                 -> matches before \n and at end of string
                 -> expected output: "abc", "\n", ""
               The '\n' gets added in the lines above, but we still have to
               verify whether the pattern also matches the end of the string,
               and add an empty chunk in case it does. */
            found = SearchString(sourceStr, splitStr, SEARCH_FORWARD, 
                searchType, False, strLength, &foundStart, &foundEnd, 
                NULL, NULL, GetWindowDelimiters(window));
            if (found) {
                ++indexNum;
                sprintf(indexStr, "%d", indexNum);
                allocIndexStr = AllocString(strlen(indexStr) + 1);
                if (!allocIndexStr) {
                    *errMsg = "array element failed to allocate key: %s";
                    return(False);
                }
                strcpy(allocIndexStr, indexStr);
                element.tag = STRING_TAG;
                element.val.str.rep = PERM_ALLOC_STR("");
                element.val.str.len = 0;

                if (!ArrayInsert(result, allocIndexStr, &element)) {
                    M_ARRAY_INSERT_FAILURE();
                }
            }
        }
    }
    return(True);
}

/*
** Set the backlighting string resource for the current window. If no parameter
** is passed or the value "default" is passed, it attempts to set the preference
** value of the resource. If the empty string is passed, the backlighting string
** will be cleared, turning off backlighting.
*/
/* DISABLED for 5.4
static int setBacklightStringMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg)
{
    char *backlightString;

    if (nArgs == 0) {
      backlightString = GetPrefBacklightCharTypes();
    }
    else if (nArgs == 1) {
      if (argList[0].tag != STRING_TAG) {
          *errMsg = "%s not called with a string parameter";
          return False;
      }
      backlightString = argList[0].val.str.rep;
    }
    else
      return wrongNArgsErr(errMsg);

    if (strcmp(backlightString, "default") == 0)
      backlightString = GetPrefBacklightCharTypes();
    if (backlightString && *backlightString == '\0')  / * empty string param * /
      backlightString = NULL;                 / * turns of backlighting * /

    SetBacklightChars(window, backlightString);
    return True;
} */

static int cursorMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = TextGetCursorPos(window->lastFocus);
    return True;
}

static int lineMV(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg)
{
    int line, cursorPos, colNum;

    result->tag = INT_TAG;
    cursorPos = TextGetCursorPos(window->lastFocus);
    if (!TextPosToLineAndCol(window->lastFocus, cursorPos, &line, &colNum))
    	line = BufCountLines(window->buffer, 0, cursorPos) + 1;
    result->val.n = line;
    return True;
}

static int columnMV(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg)
{
    textBuffer *buf = window->buffer;
    int cursorPos;

    result->tag = INT_TAG;
    cursorPos = TextGetCursorPos(window->lastFocus);
    result->val.n = BufCountDispChars(buf, BufStartOfLine(buf, cursorPos),
	    cursorPos);
    return True;
}

static int fileNameMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, window->filename);
    return True;
}

static int filePathMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, window->path);
    return True;
}

static int lengthMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->buffer->length;
    return True;
}

static int selectionStartMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->buffer->primary.selected ?
    	    window->buffer->primary.start : -1;
    return True;
}

static int selectionEndMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->buffer->primary.selected ?
    	    window->buffer->primary.end : -1;
    return True;
}

static int selectionLeftMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    selection *sel = &window->buffer->primary;
    
    result->tag = INT_TAG;
    result->val.n = sel->selected && sel->rectangular ? sel->rectStart : -1;
    return True;
}

static int selectionRightMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    selection *sel = &window->buffer->primary;
    
    result->tag = INT_TAG;
    result->val.n = sel->selected && sel->rectangular ? sel->rectEnd : -1;
    return True;
}

static int wrapMarginMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int margin, nCols;
    
    XtVaGetValues(window->textArea, textNcolumns, &nCols,
    	    textNwrapMargin, &margin, NULL);
    result->tag = INT_TAG;
    result->val.n = margin == 0 ? nCols : margin;
    return True;
}

static int statisticsLineMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->showStats ? 1 : 0;
    return True;
}

static int incSearchLineMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->showISearchLine ? 1 : 0;
    return True;
}

static int showLineNumbersMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->showLineNumbers ? 1 : 0;
    return True;
}

static int autoIndentMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    char *res = NULL;

    switch (window->indentStyle) {
        case NO_AUTO_INDENT:
            res = PERM_ALLOC_STR("off");
            break;
        case AUTO_INDENT:
            res = PERM_ALLOC_STR("on");
            break;
        case SMART_INDENT:
            res = PERM_ALLOC_STR("smart");
            break;
        default:
            *errMsg = "Invalid indent style value encountered in %s";
            return False;
            break;
    }
    result->tag = STRING_TAG;
    result->val.str.rep = res;
    result->val.str.len = strlen(res);
    return True;
}

static int wrapTextMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    char *res = NULL;

    switch (window->wrapMode) {
        case NO_WRAP:
            res = PERM_ALLOC_STR("none");
            break;
        case NEWLINE_WRAP:
            res = PERM_ALLOC_STR("auto");
            break;
        case CONTINUOUS_WRAP:
            res = PERM_ALLOC_STR("continuous");
            break;
        default:
            *errMsg = "Invalid wrap style value encountered in %s";
            return False;
            break;
    }
    result->tag = STRING_TAG;
    result->val.str.rep = res;
    result->val.str.len = strlen(res);
    return True;
}

static int highlightSyntaxMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->highlightSyntax ? 1 : 0;
    return True;
}

static int makeBackupCopyMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->saveOldVersion ? 1 : 0;
    return True;
}

static int incBackupMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->autoSave ? 1 : 0;
    return True;
}

static int showMatchingMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    char *res = NULL;

    switch (window->showMatchingStyle) {
        case NO_FLASH:
            res = PERM_ALLOC_STR(NO_FLASH_STRING);
            break;
        case FLASH_DELIMIT:
            res = PERM_ALLOC_STR(FLASH_DELIMIT_STRING);
            break;
        case FLASH_RANGE:
            res = PERM_ALLOC_STR(FLASH_RANGE_STRING);
            break;
        default:
            *errMsg = "Invalid match flashing style value encountered in %s";
            return False;
            break;
    }
    result->tag = STRING_TAG;
    result->val.str.rep = res;
    result->val.str.len = strlen(res);
    return True;
}

static int matchSyntaxBasedMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->matchSyntaxBased ? 1 : 0;
    return True;
}



static int overTypeModeMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->overstrike ? 1 : 0;
    return True;
}

static int readOnlyMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = (IS_ANY_LOCKED(window->lockReasons)) ? 1 : 0;
    return True;
}

static int lockedMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = (IS_USER_LOCKED(window->lockReasons)) ? 1 : 0;
    return True;
}

static int fileFormatMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    char *res = NULL;

    switch (window->fileFormat) {
        case UNIX_FILE_FORMAT:
            res = PERM_ALLOC_STR("unix");
            break;
        case DOS_FILE_FORMAT:
            res = PERM_ALLOC_STR("dos");
            break;
        case MAC_FILE_FORMAT:
            res = PERM_ALLOC_STR("macintosh");
            break;
        default:
            *errMsg = "Invalid linefeed style value encountered in %s";
            return False;
    }
    result->tag = STRING_TAG;
    result->val.str.rep = res;
    result->val.str.len = strlen(res);
    return True;
}

static int fontNameMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, window->fontName);
    return True;
}

static int fontNameItalicMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, window->italicFontName);
    return True;
}

static int fontNameBoldMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, window->boldFontName);
    return True;
}

static int fontNameBoldItalicMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, window->boldItalicFontName);
    return True;
}

static int subscriptSepMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    result->val.str.rep = PERM_ALLOC_STR(ARRAY_DIM_SEP);
    result->val.str.len = strlen(result->val.str.rep);
    return True;
}

static int minFontWidthMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = TextGetMinFontWidth(window->textArea, window->highlightSyntax);
    return True;
}

static int maxFontWidthMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = TextGetMaxFontWidth(window->textArea, window->highlightSyntax);
    return True;
}

static int topLineMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = TextFirstVisibleLine(window->lastFocus);
    return True;
}

static int numDisplayLinesMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = TextNumVisibleLines(window->lastFocus);
    return True;
}

static int displayWidthMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = TextVisibleWidth(window->lastFocus);
    return True;
}

static int activePaneMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = WidgetToPaneIndex(window, window->lastFocus) + 1;
    return True;
}

static int nPanesMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->nPanes + 1;
    return True;
}

static int emptyArrayMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = NULL;
    return True;
}

static int serverNameMV(WindowInfo *window, DataValue *argList, int nArgs,
    DataValue *result, char **errMsg)
{
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, GetPrefServerName());
    return True;
}

static int tabDistMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->buffer->tabDist;
    return True;
}

static int emTabDistMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    int dist;
    
    XtVaGetValues(window->textArea, textNemulateTabs, &dist, NULL);
    result->tag = INT_TAG;
    result->val.n = dist;
    return True;
}

static int useTabsMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->buffer->useTabs;
    return True;
}

static int modifiedMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    result->tag = INT_TAG;
    result->val.n = window->fileChanged;
    return True;
}

static int languageModeMV(WindowInfo *window, DataValue *argList, int nArgs,
    	DataValue *result, char **errMsg)
{
    char *lmName = LanguageModeName(window->languageMode);
    
    if (lmName == NULL)
    	lmName = "Plain";
    result->tag = STRING_TAG;
    AllocNStringCpy(&result->val.str, lmName);
    return True;
}

/* DISABLED for 5.4
static int backlightStringMV(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg)
{
    char *backlightString = window->backlightCharTypes;

    result->tag = STRING_TAG;
    if (!backlightString || !window->backlightChars)
      backlightString = "";
    AllocNStringCpy(&result->val.str, backlightString);
    return True;
} */

/* -------------------------------------------------------------------------- */

/*
** Range set macro variables and functions
*/
static int rangesetListMV(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    RangesetTable *rangesetTable = window->buffer->rangesetTable;
    unsigned char *rangesetList;
    char *allocIndexStr;
    char indexStr[TYPE_INT_STR_SIZE(int)] ;
    int nRangesets, i;
    DataValue element;

    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();
    
    if (rangesetTable == NULL) {
        return True;
    }
    
    rangesetList = RangesetGetList(rangesetTable);
    nRangesets = strlen((char*)rangesetList);
    for(i = 0; i < nRangesets; i++) {
        element.tag = INT_TAG;
        element.val.n = rangesetList[i];

        sprintf(indexStr, "%d", nRangesets - i - 1);
        allocIndexStr = AllocString(strlen(indexStr) + 1);
        if (allocIndexStr == NULL)
            M_FAILURE("Failed to allocate array key in %s");
        strcpy(allocIndexStr, indexStr);
        
        if (!ArrayInsert(result, allocIndexStr, &element))
            M_FAILURE("Failed to insert array element in %s");
    }

    return True;
}

/*
**  Returns the version number of the current macro language implementation.
**  For releases, this is the same number as NEdit's major.minor version
**  number to keep things simple. For developer versions this could really
**  be anything.
**
**  Note that the current way to build $VERSION builds the same value for
**  different point revisions. This is done because the macro interface
**  does not change for the same version.
*/
static int versionMV(WindowInfo* window, DataValue* argList, int nArgs,
        DataValue* result, char** errMsg)
{
    static unsigned version = NEDIT_VERSION * 1000 + NEDIT_REVISION;

    result->tag = INT_TAG;
    result->val.n = version;
    return True;
}

/*
** Built-in macro subroutine to create a new rangeset or rangesets.  
** If called with one argument: $1 is the number of rangesets required and 
** return value is an array indexed 0 to n, with the rangeset labels as values;
** (or an empty array if the requested number of rangesets are not available).
** If called with no arguments, returns a single rangeset label (not an array),
** or an empty string if there are no rangesets available. 
*/
static int rangesetCreateMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    int label;
    int i, nRangesetsRequired;
    DataValue element;
    char indexStr[TYPE_INT_STR_SIZE(int)], *allocIndexStr;
    
    RangesetTable *rangesetTable = window->buffer->rangesetTable;
       
    if (nArgs > 1)
        return wrongNArgsErr(errMsg);

    if (rangesetTable == NULL) {
        window->buffer->rangesetTable = rangesetTable = 
                RangesetTableAlloc(window->buffer);
    }
    
    if (nArgs == 0) {
        label = RangesetCreate(rangesetTable);

        result->tag = INT_TAG;
        result->val.n = label;
        return True;
    }
    else {
        if (!readIntArg(argList[0], &nRangesetsRequired, errMsg))
            return False;
        
        result->tag = ARRAY_TAG;
        result->val.arrayPtr = ArrayNew();
        
        if (nRangesetsRequired > nRangesetsAvailable(rangesetTable))
            return True;
        
        for (i = 0; i < nRangesetsRequired; i++) {
            element.tag = INT_TAG;
            element.val.n = RangesetCreate(rangesetTable);
            
            sprintf(indexStr, "%d", i);
            allocIndexStr = AllocString(strlen(indexStr) + 1);
            if (!allocIndexStr) {
                *errMsg = "Array element failed to allocate key: %s";
                return(False);
            }
            strcpy(allocIndexStr, indexStr);
            ArrayInsert(result, allocIndexStr, &element);
        }
        
        return True;
    }
}

/*
** Built-in macro subroutine for forgetting a range set.
*/
static int rangesetDestroyMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    RangesetTable *rangesetTable = window->buffer->rangesetTable;
    DataValue *array;
    DataValue element;
    char keyString[TYPE_INT_STR_SIZE(int)];
    int deleteLabels[N_RANGESETS];
    int i, arraySize;
    int label = 0;
    
    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }
    
    if (argList[0].tag == ARRAY_TAG) {
        array = &argList[0];
        arraySize = ArraySize(array);
        
        if (arraySize > N_RANGESETS) {
            M_FAILURE("Too many elements in array in %s");
        }
        
        for (i = 0; i < arraySize; i++) {
            sprintf(keyString, "%d", i);
            
            if (!ArrayGet(array, keyString, &element)) {
                M_FAILURE("Invalid key in array in %s");
            }
            
            if (!readIntArg(element, &label, errMsg)
                    || !RangesetLabelOK(label)) {
                M_FAILURE("Invalid rangeset label in array in %s");
            }
            
            deleteLabels[i] = label;
        }
        
        for (i = 0; i < arraySize; i++) {
            RangesetForget(rangesetTable, deleteLabels[i]);
        }
    } else {
        if (!readIntArg(argList[0], &label, errMsg)
                || !RangesetLabelOK(label)) {
            M_FAILURE("Invalid rangeset label in %s");
        }
        
        if(rangesetTable != NULL) {
            RangesetForget(rangesetTable, label);
        }
    }

    /* set up result */
    result->tag = NO_TAG;
    return True;
}


/*
** Built-in macro subroutine for getting all range sets with a specfic name.
** Arguments are $1: range set name.
** return value is an array indexed 0 to n, with the rangeset labels as values;
*/
static int rangesetGetByNameMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    char stringStorage[1][TYPE_INT_STR_SIZE(int)];
    Rangeset *rangeset;
    int label;
    char *name, *rangeset_name;
    RangesetTable *rangesetTable = window->buffer->rangesetTable;
    unsigned char *rangesetList;
    char *allocIndexStr;
    char indexStr[TYPE_INT_STR_SIZE(int)] ;
    int nRangesets, i, insertIndex = 0;
    DataValue element;

    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }

    if (!readStringArg(argList[0], &name, stringStorage[0], errMsg)) {
        M_FAILURE("First parameter is not a name string in %s");
    }

    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();
    
    if (rangesetTable == NULL) {
        return True;
    }
    
    rangesetList = RangesetGetList(rangesetTable);
    nRangesets = strlen((char *)rangesetList);
    for (i = 0; i < nRangesets; ++i) {
        label = rangesetList[i];
        rangeset = RangesetFetch(rangesetTable, label);
        if (rangeset) {
            rangeset_name = RangesetGetName(rangeset);
            if (strcmp(name, rangeset_name ? rangeset_name : "") == 0) {
                element.tag = INT_TAG;
                element.val.n = label;

                sprintf(indexStr, "%d", insertIndex);
                allocIndexStr = AllocString(strlen(indexStr) + 1);
                if (allocIndexStr == NULL)
                    M_FAILURE("Failed to allocate array key in %s");

                strcpy(allocIndexStr, indexStr);

                if (!ArrayInsert(result, allocIndexStr, &element))
                    M_FAILURE("Failed to insert array element in %s");

                ++insertIndex;
            }
        }
    }

    return True;
}
    
/*
** Built-in macro subroutine for adding to a range set. Arguments are $1: range
** set label (one integer), then either (a) $2: source range set label, 
** (b) $2: int start-range, $3: int end-range, (c) nothing (use selection
** if any to specify range to add - must not be rectangular). Returns the 
** index of the newly added range (cases b and c), or 0 (case a).
*/
static int rangesetAddMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    textBuffer *buffer = window->buffer;
    RangesetTable *rangesetTable = buffer->rangesetTable;
    Rangeset *targetRangeset, *sourceRangeset;
    int start, end, isRect, rectStart, rectEnd, maxpos, index;
    int label = 0;

    if (nArgs < 1 || nArgs > 3)
        return wrongNArgsErr(errMsg);

    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    targetRangeset = RangesetFetch(rangesetTable, label);

    if (targetRangeset == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }
    
    start = end = -1;

    if (nArgs == 1) {
        /* pick up current selection in this window */
        if (!BufGetSelectionPos(buffer, &start, &end,
              &isRect, &rectStart, &rectEnd) || isRect) {
            M_FAILURE("Selection missing or rectangular in call to %s");
        }
        if (!RangesetAddBetween(targetRangeset, start, end)) {
            M_FAILURE("Failure to add selection in %s");
        }
    }
    
    if (nArgs == 2) {
        /* add ranges taken from a second set */
        if (!readIntArg(argList[1], &label, errMsg)
                || !RangesetLabelOK(label)) {
            M_FAILURE("Second parameter is an invalid rangeset label in %s");
        }
      
        sourceRangeset = RangesetFetch(rangesetTable, label);
        if (sourceRangeset == NULL) {
            M_FAILURE("Second rangeset does not exist in %s");
        }
      
        RangesetAdd(targetRangeset, sourceRangeset);
    }
    
    if (nArgs == 3) {
        /* add a range bounded by the start and end positions in $2, $3 */
        if (!readIntArg(argList[1], &start, errMsg)) {
            return False;
        }
        if (!readIntArg(argList[2], &end, errMsg)) {
            return False;
        }

        /* make sure range is in order and fits buffer size */
        maxpos = buffer->length;
        if (start < 0) start = 0;
        if (start > maxpos) start = maxpos;
        if (end < 0) end = 0;
        if (end > maxpos) end = maxpos;
        if (start > end) {int temp = start; start = end; end = temp;}

        if ((start != end) && !RangesetAddBetween(targetRangeset, start, end)) {
            M_FAILURE("Failed to add range in %s");
        }
    }

    /* (to) which range did we just add? */
    if (nArgs != 2 && start >= 0) {
        start = (start + end) / 2;      /* "middle" of added range */
        index = 1 + RangesetFindRangeOfPos(targetRangeset, start, False);
    }
    else {
        index = 0;
    }

    /* set up result */
    result->tag = INT_TAG;
    result->val.n = index;
    return True;
}


/*
** Built-in macro subroutine for removing from a range set. Almost identical to
** rangesetAddMS() - only changes are from RangesetAdd()/RangesetAddBetween()
** to RangesetSubtract()/RangesetSubtractBetween(), the handling of an 
** undefined destination range, and that it returns no value.
*/
static int rangesetSubtractMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    textBuffer *buffer = window->buffer;
    RangesetTable *rangesetTable = buffer->rangesetTable;
    Rangeset *targetRangeset, *sourceRangeset;
    int start, end, isRect, rectStart, rectEnd, maxpos;
    int label = 0;

    if (nArgs < 1 || nArgs > 3) {
        return wrongNArgsErr(errMsg);
    }

    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }
    
    targetRangeset = RangesetFetch(rangesetTable, label);
    if (targetRangeset == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    if (nArgs == 1) {
      /* remove current selection in this window */
        if (!BufGetSelectionPos(buffer, &start, &end, &isRect, &rectStart, &rectEnd) 
                || isRect) {
            M_FAILURE("Selection missing or rectangular in call to %s");
        }
        RangesetRemoveBetween(targetRangeset, start, end);
    }
    
    if (nArgs == 2) {
        /* remove ranges taken from a second set */
        if (!readIntArg(argList[1], &label, errMsg)
                || !RangesetLabelOK(label)) {
            M_FAILURE("Second parameter is an invalid rangeset label in %s");
        }
        
        sourceRangeset = RangesetFetch(rangesetTable, label);
        if (sourceRangeset == NULL) {
            M_FAILURE("Second rangeset does not exist in %s");
        }
        RangesetRemove(targetRangeset, sourceRangeset);
    }
    
    if (nArgs == 3) {
        /* remove a range bounded by the start and end positions in $2, $3 */
        if (!readIntArg(argList[1], &start, errMsg))
            return False;
        if (!readIntArg(argList[2], &end, errMsg))
            return False;

        /* make sure range is in order and fits buffer size */
        maxpos = buffer->length;
        if (start < 0) start = 0;
        if (start > maxpos) start = maxpos;
        if (end < 0) end = 0;
        if (end > maxpos) end = maxpos;
        if (start > end) {int temp = start; start = end; end = temp;}

        RangesetRemoveBetween(targetRangeset, start, end);
    }

    /* set up result */
    result->tag = NO_TAG;
    return True;
}


/*
** Built-in macro subroutine to invert a range set. Argument is $1: range set
** label (one alphabetic character). Returns nothing. Fails if range set 
** undefined.
*/
static int rangesetInvertMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    
    RangesetTable *rangesetTable = window->buffer->rangesetTable;
    Rangeset *rangeset;
    int label = 0;

    if (nArgs != 1)
        return wrongNArgsErr(errMsg);

    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }
    
    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    rangeset = RangesetFetch(rangesetTable, label);
    if (rangeset == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }
    
    if (RangesetInverse(rangeset) < 0) {
        M_FAILURE("Problem inverting rangeset in %s");
    }  

    /* set up result */
    result->tag = NO_TAG;
    return True;
}


/*
** Built-in macro subroutine for finding out info about a rangeset.  Takes one
** argument of a rangeset label.  Returns an array with the following keys:
**    defined, count, color, mode.
*/
static int rangesetInfoMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    RangesetTable *rangesetTable = window->buffer->rangesetTable;
    Rangeset *rangeset = NULL;
    int count, defined;
    char *color, *name, *mode;
    DataValue element;
    int label = 0;
    
    if (nArgs != 1)
      return wrongNArgsErr(errMsg);
        
    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if (rangesetTable != NULL) {
        rangeset = RangesetFetch(rangesetTable, label);
    }

    RangesetGetInfo(rangeset, &defined, &label, &count, &color, &name, &mode);
    
    /* set up result */    
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();
    
    element.tag = INT_TAG;
    element.val.n = defined;
    if (!ArrayInsert(result, PERM_ALLOC_STR("defined"), &element))
        M_FAILURE("Failed to insert array element \"defined\" in %s");
        
    element.tag = INT_TAG;
    element.val.n = count;
    if (!ArrayInsert(result, PERM_ALLOC_STR("count"), &element))
        M_FAILURE("Failed to insert array element \"count\" in %s");
        
    element.tag = STRING_TAG;
    if (!AllocNStringCpy(&element.val.str, color))
        M_FAILURE("Failed to allocate array value \"color\" in %s");
    if (!ArrayInsert(result, PERM_ALLOC_STR("color"), &element))
        M_FAILURE("Failed to insert array element \"color\" in %s");
  
    element.tag = STRING_TAG;
    if (!AllocNStringCpy(&element.val.str, name))
        M_FAILURE("Failed to allocate array value \"name\" in %s");
    if (!ArrayInsert(result, PERM_ALLOC_STR("name"), &element)) {
        M_FAILURE("Failed to insert array element \"name\" in %s");
    }
  
    element.tag = STRING_TAG;
    if (!AllocNStringCpy(&element.val.str, mode))
        M_FAILURE("Failed to allocate array value \"mode\" in %s");
    if (!ArrayInsert(result, PERM_ALLOC_STR("mode"), &element))
        M_FAILURE("Failed to insert array element \"mode\" in %s");
  
    return True;
}

/*
** Built-in macro subroutine for finding the extent of a range in a set. 
** If only one parameter is supplied, use the spanning range of all 
** ranges, otherwise select the individual range specified.  Returns 
** an array with the keys "start" and "end" and values  
*/
static int rangesetRangeMS(WindowInfo *window, DataValue *argList, int nArgs,
      DataValue *result, char **errMsg)
{
    textBuffer *buffer = window->buffer;
    RangesetTable *rangesetTable = buffer->rangesetTable;
    Rangeset *rangeset;
    int start, end, dummy, rangeIndex, ok;
    DataValue element;
    int label = 0;

    if (nArgs < 1 || nArgs > 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }
    
    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    ok = False;
    rangeset = RangesetFetch(rangesetTable, label);
    if (rangeset != NULL) {
        if (nArgs == 1) {
            rangeIndex = RangesetGetNRanges(rangeset) - 1;
            ok = RangesetFindRangeNo(rangeset, 0, &start, &dummy);
            ok &= RangesetFindRangeNo(rangeset, rangeIndex, &dummy, &end);
            rangeIndex = -1;
        }
        else if (nArgs == 2) {
            if (!readIntArg(argList[1], &rangeIndex, errMsg)) {
                return False;
            }
            ok = RangesetFindRangeNo(rangeset, rangeIndex-1, &start, &end);
        }
    }

    /* set up result */    
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();
    
    if (!ok)
        return True;
    
    element.tag = INT_TAG;
    element.val.n = start;
    if (!ArrayInsert(result, PERM_ALLOC_STR("start"), &element))
        M_FAILURE("Failed to insert array element \"start\" in %s");

    element.tag = INT_TAG;
    element.val.n = end;
    if (!ArrayInsert(result, PERM_ALLOC_STR("end"), &element))
        M_FAILURE("Failed to insert array element \"end\" in %s");

    return True;
}

/*
** Built-in macro subroutine for checking a position against a range. If only
** one parameter is supplied, the current cursor position is used. Returns 
** false (zero) if not in a range, range index (1-based) if in a range; 
** fails if parameters were bad.
*/
static int rangesetIncludesPosMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg)
{
    textBuffer *buffer = window->buffer;
    RangesetTable *rangesetTable = buffer->rangesetTable;
    Rangeset *rangeset;
    int pos, rangeIndex, maxpos;
    int label = 0;

    if (nArgs < 1 || nArgs > 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) { 
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }
    
    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }
    
    rangeset = RangesetFetch(rangesetTable, label);
    if (rangeset == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    if (nArgs == 1) {
        pos = TextGetCursorPos(window->lastFocus);
    }
    else if (nArgs == 2) {
        if (!readIntArg(argList[1], &pos, errMsg))
            return False;
    }

    maxpos = buffer->length;
    if (pos < 0 || pos > maxpos) {
        rangeIndex = 0;
    }
    else {
        rangeIndex = RangesetFindRangeOfPos(rangeset, pos, False) + 1;
    }

    /* set up result */
    result->tag = INT_TAG;
    result->val.n = rangeIndex;
    return True;
}

/*
** Set the color of a range set's ranges. it is ignored if the color cannot be
** found/applied. If no color is applied, any current color is removed. Returns
** true if the rangeset is valid.
*/
static int rangesetSetColorMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg)
{
    char stringStorage[1][TYPE_INT_STR_SIZE(int)];
    textBuffer *buffer = window->buffer;
    RangesetTable *rangesetTable = buffer->rangesetTable;
    Rangeset *rangeset;
    char *color_name;
    int label = 0;

    if (nArgs != 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    rangeset = RangesetFetch(rangesetTable, label);
    if (rangeset == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    color_name = "";
    if (rangeset != NULL) {
        if (!readStringArg(argList[1], &color_name, stringStorage[0], errMsg)) {
            M_FAILURE("Second parameter is not a color name string in %s");
        }
    }
    
    RangesetAssignColorName(rangeset, color_name);
            
    /* set up result */
    result->tag = NO_TAG;
    return True;
}

/*
** Set the name of a range set's ranges. Returns
** true if the rangeset is valid.
*/
static int rangesetSetNameMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg)
{
    char stringStorage[1][TYPE_INT_STR_SIZE(int)];
    textBuffer *buffer = window->buffer;
    RangesetTable *rangesetTable = buffer->rangesetTable;
    Rangeset *rangeset;
    char *name;
    int label = 0;

    if (nArgs != 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readIntArg(argList[0], &label, errMsg)
        || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    rangeset = RangesetFetch(rangesetTable, label);
    if (rangeset == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    name = "";
    if (rangeset != NULL) {
        if (!readStringArg(argList[1], &name, stringStorage[0], errMsg)) {
            M_FAILURE("Second parameter is not a valid name string in %s");
        }
    }
    
    RangesetAssignName(rangeset, name);
            
    /* set up result */
    result->tag = NO_TAG;
    return True;
}

/*
** Change a range's modification response. Returns true if the rangeset is
** valid and the response type name is valid.
*/
static int rangesetSetModeMS(WindowInfo *window, DataValue *argList,
      int nArgs, DataValue *result, char **errMsg)
{
    char stringStorage[1][TYPE_INT_STR_SIZE(int)];
    textBuffer *buffer = window->buffer;
    RangesetTable *rangesetTable = buffer->rangesetTable;
    Rangeset *rangeset;
    char *update_fn_name;
    int ok;
    int label = 0;

    if (nArgs < 1 || nArgs > 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readIntArg(argList[0], &label, errMsg) 
            || !RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if (rangesetTable == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    rangeset = RangesetFetch(rangesetTable, label);
    if (rangeset == NULL) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    update_fn_name = "";
    if (rangeset != NULL) {
        if (nArgs == 2) {
            if (!readStringArg(argList[1], &update_fn_name, stringStorage[0], errMsg)) {
                M_FAILURE("Second parameter is not a string in %s");
            }
        }
    }
    
    ok = RangesetChangeModifyResponse(rangeset, update_fn_name);
    
    if (!ok) {
        M_FAILURE("Second parameter is not a valid mode in %s");
    }

    /* set up result */
    result->tag = NO_TAG;
    return True;
}

/* -------------------------------------------------------------------------- */


/*
** Routines to get details directly from the window.
*/

/*
** Sets up an array containing information about a style given its name or
** a buffer position (bufferPos >= 0) and its highlighting pattern code
** (patCode >= 0).
** From the name we obtain:
**      ["color"]       Foreground color name of style
**      ["background"]  Background color name of style if specified
**      ["bold"]        '1' if style is bold, '0' otherwise
**      ["italic"]      '1' if style is italic, '0' otherwise 
** Given position and pattern code we obtain:
**      ["rgb"]         RGB representation of foreground color of style
**      ["back_rgb"]    RGB representation of background color of style
**      ["extent"]      Forward distance from position over which style applies
** We only supply the style name if the includeName parameter is set:
**      ["style"]       Name of style
**
*/
static int fillStyleResult(DataValue *result, char **errMsg,
        WindowInfo *window, char *styleName, Boolean preallocatedStyleName,
        Boolean includeName, int patCode, int bufferPos)
{
    DataValue DV;
    char colorValue[20];
    int r, g, b;
    
    /* initialize array */
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    /* the following array entries will be strings */
    DV.tag = STRING_TAG;

    if (includeName) {
        /* insert style name */
        if (preallocatedStyleName) {
            DV.val.str.rep = styleName;
            DV.val.str.len = strlen(styleName);
        }
        else {
            AllocNStringCpy(&DV.val.str, styleName);
        }
        M_STR_ALLOC_ASSERT(DV);
        if (!ArrayInsert(result, PERM_ALLOC_STR("style"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    /* insert color name */
    AllocNStringCpy(&DV.val.str, ColorOfNamedStyle(styleName));
    M_STR_ALLOC_ASSERT(DV);
    if (!ArrayInsert(result, PERM_ALLOC_STR("color"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    /* Prepare array element for color value
       (only possible if we pass through the dynamic highlight pattern tables
       in other words, only if we have a pattern code) */
    if (patCode) {
        HighlightColorValueOfCode(window, patCode, &r, &g, &b);
        sprintf(colorValue, "#%02x%02x%02x", r/256, g/256, b/256);
        AllocNStringCpy(&DV.val.str, colorValue);
        M_STR_ALLOC_ASSERT(DV);
        if (!ArrayInsert(result, PERM_ALLOC_STR("rgb"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    /* Prepare array element for background color name */
    AllocNStringCpy(&DV.val.str, BgColorOfNamedStyle(styleName));
    M_STR_ALLOC_ASSERT(DV);
    if (!ArrayInsert(result, PERM_ALLOC_STR("background"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    /* Prepare array element for background color value
       (only possible if we pass through the dynamic highlight pattern tables
       in other words, only if we have a pattern code) */
    if (patCode) {
        GetHighlightBGColorOfCode(window, patCode, &r, &g, &b);
        sprintf(colorValue, "#%02x%02x%02x", r/256, g/256, b/256);
        AllocNStringCpy(&DV.val.str, colorValue);
        M_STR_ALLOC_ASSERT(DV);
        if (!ArrayInsert(result, PERM_ALLOC_STR("back_rgb"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    /* the following array entries will be integers */
    DV.tag = INT_TAG;

    /* Put boldness value in array */
    DV.val.n = FontOfNamedStyleIsBold(styleName);
    if (!ArrayInsert(result, PERM_ALLOC_STR("bold"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    /* Put italicity value in array */
    DV.val.n = FontOfNamedStyleIsItalic(styleName);
    if (!ArrayInsert(result, PERM_ALLOC_STR("italic"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    if (bufferPos >= 0) {
        /* insert extent */
        const char *styleNameNotUsed = NULL;
        DV.val.n = StyleLengthOfCodeFromPos(window, bufferPos, &styleNameNotUsed);
        if (!ArrayInsert(result, PERM_ALLOC_STR("extent"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }
    return True;
}

/*
** Returns an array containing information about the style of name $1
**      ["color"]       Foreground color name of style
**      ["background"]  Background color name of style if specified
**      ["bold"]        '1' if style is bold, '0' otherwise
**      ["italic"]      '1' if style is italic, '0' otherwise 
**
*/
static int getStyleByNameMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg)
{
    char stringStorage[1][TYPE_INT_STR_SIZE(int)];
    char *styleName;

    /* Validate number of arguments */
    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }
    
    /* Prepare result */
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = NULL;

    if (!readStringArg(argList[0], &styleName, stringStorage[0], errMsg)) {
        M_FAILURE("First parameter is not a string in %s");
    }

    if (!NamedStyleExists(styleName)) {
        /* if the given name is invalid we just return an empty array. */
        return True;
    }
    
    return fillStyleResult(result, errMsg, window,
        styleName, (argList[0].tag == STRING_TAG), False, 0, -1);
}

/*
** Returns an array containing information about the style of position $1
**      ["style"]       Name of style
**      ["color"]       Foreground color name of style
**      ["background"]  Background color name of style if specified
**      ["bold"]        '1' if style is bold, '0' otherwise
**      ["italic"]      '1' if style is italic, '0' otherwise 
**      ["rgb"]         RGB representation of foreground color of style
**      ["back_rgb"]    RGB representation of background color of style
**      ["extent"]      Forward distance from position over which style applies
**
*/
static int getStyleAtPosMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg)
{
    int patCode;
    int bufferPos;
    textBuffer *buf = window->buffer;

    /* Validate number of arguments */
    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }
    
    /* Prepare result */
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = NULL;

    if (!readIntArg(argList[0], &bufferPos, errMsg)) {
        return False;
    }

    /*  Verify sane buffer position */
    if ((bufferPos < 0) || (bufferPos >= buf->length)) {
        /*  If the position is not legal, we cannot guess anything about
            the style, so we return an empty array. */
        return True;
    }

    /* Determine pattern code */
    patCode = HighlightCodeOfPos(window, bufferPos);
    if (patCode == 0) {
        /* if there is no pattern we just return an empty array. */
        return True;
    }

    return fillStyleResult(result, errMsg, window,
        HighlightStyleOfCode(window, patCode), False, True, patCode, bufferPos);
}

/*
** Sets up an array containing information about a pattern given its name or
** a buffer position (bufferPos >= 0).
** From the name we obtain:
**      ["style"]       Name of style
**      ["extent"]      Forward distance from position over which style applies
** We only supply the pattern name if the includeName parameter is set:
**      ["pattern"]     Name of pattern
**
*/
static int fillPatternResult(DataValue *result, char **errMsg,
        WindowInfo *window, char *patternName, Boolean preallocatedPatternName,
        Boolean includeName, char* styleName, int bufferPos)
{
    DataValue DV;

    /* initialize array */
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    /* the following array entries will be strings */
    DV.tag = STRING_TAG;

    if (includeName) {
        /* insert pattern name */
        if (preallocatedPatternName) {
            DV.val.str.rep = patternName;
            DV.val.str.len = strlen(patternName);
        }
        else {
            AllocNStringCpy(&DV.val.str, patternName);
        }
        M_STR_ALLOC_ASSERT(DV);
        if (!ArrayInsert(result, PERM_ALLOC_STR("pattern"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    /* insert style name */
    AllocNStringCpy(&DV.val.str, styleName);
    M_STR_ALLOC_ASSERT(DV);
    if (!ArrayInsert(result, PERM_ALLOC_STR("style"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    /* the following array entries will be integers */
    DV.tag = INT_TAG;

    if (bufferPos >= 0) {
        /* insert extent */
        int checkCode = 0;
        DV.val.n = HighlightLengthOfCodeFromPos(window, bufferPos, &checkCode);
        if (!ArrayInsert(result, PERM_ALLOC_STR("extent"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    return True;
}

/*
** Returns an array containing information about a highlighting pattern. The
** single parameter contains the pattern name for which this information is
** requested.
** The returned array looks like this:
**      ["style"]       Name of style
*/
static int getPatternByNameMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg)
{
    char stringStorage[1][TYPE_INT_STR_SIZE(int)];
    char *patternName = NULL;
    highlightPattern *pattern;
    
    /* Begin of building the result. */
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = NULL;

    /* Validate number of arguments */
    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }
    
    if (!readStringArg(argList[0], &patternName, stringStorage[0], errMsg)) {
        M_FAILURE("First parameter is not a string in %s");
    }
    
    pattern = FindPatternOfWindow(window, patternName);
    if (pattern == NULL) {
        /* The pattern's name is unknown. */
        return True;
    }

    return fillPatternResult(result, errMsg, window, patternName,
            (argList[0].tag == STRING_TAG), False, pattern->style, -1);
}

/*
** Returns an array containing information about the highlighting pattern
** applied at a given position, passed as the only parameter.
** The returned array looks like this:
**      ["pattern"]     Name of pattern
**      ["style"]       Name of style
**      ["extent"]      Distance from position over which this pattern applies
*/
static int getPatternAtPosMS(WindowInfo *window, DataValue *argList, int nArgs,
        DataValue *result, char **errMsg)
{
    int bufferPos = -1;
    textBuffer *buffer = window->buffer;
    int patCode = 0;
    
    /* Begin of building the result. */
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = NULL;

    /* Validate number of arguments */
    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }
    
    /* The most straightforward case: Get a pattern, style and extent
       for a buffer position. */
    if (!readIntArg(argList[0], &bufferPos, errMsg)) {
        return False;
    }

    /*  Verify sane buffer position
     *  You would expect that buffer->length would be among the sane
     *  positions, but we have n characters and n+1 buffer positions. */
    if ((bufferPos < 0) || (bufferPos >= buffer->length)) {
        /*  If the position is not legal, we cannot guess anything about
            the highlighting pattern, so we return an empty array. */
        return True;
    }

    /* Determine the highlighting pattern used */
    patCode = HighlightCodeOfPos(window, bufferPos);
    if (patCode == 0) {
        /* if there is no highlighting pattern we just return an empty array. */
        return True;
    }

    return fillPatternResult(result, errMsg, window,
        HighlightNameOfCode(window, patCode), False, True,
        HighlightStyleOfCode(window, patCode), bufferPos);
}

static int wrongNArgsErr(char **errMsg)
{
    *errMsg = "Wrong number of arguments to function %s";
    return False;
}

static int tooFewArgsErr(char **errMsg)
{
    *errMsg = "Too few arguments to function %s";
    return False;
}

/*
** strCaseCmp compares its arguments and returns 0 if the two strings
** are equal IGNORING case differences.  Otherwise returns 1 or -1
** depending on relative comparison.
*/
static int strCaseCmp(char *str1, char *str2)
{
    char *c1, *c2;

    for (c1 = str1, c2 = str2;
            (*c1 != '\0' && *c2 != '\0')
                    && toupper((unsigned char)*c1) == toupper((unsigned char)*c2);
            ++c1, ++c2)
    {
    }

    if (((unsigned char)toupper((unsigned char)*c1))
            > ((unsigned char)toupper((unsigned char)*c2)))
    {
        return(1);
    } else if (((unsigned char)toupper((unsigned char)*c1))
            < ((unsigned char)toupper((unsigned char)*c2)))
    {
        return(-1);
    } else
    {
        return(0);
    }
}

/*
** Get an integer value from a tagged DataValue structure.  Return True
** if conversion succeeded, and store result in *result, otherwise
** return False with an error message in *errMsg.
*/
static int readIntArg(DataValue dv, int *result, char **errMsg)
{
    char *c;
    
    if (dv.tag == INT_TAG) {
    	*result = dv.val.n;
    	return True;
    } else if (dv.tag == STRING_TAG) {
	for (c=dv.val.str.rep; *c != '\0'; c++) {
    	    if (!(isdigit((unsigned char)*c) || *c == ' ' || *c == '\t')) {
    		goto typeError;
    	    }
    	}
	sscanf(dv.val.str.rep, "%d", result);
	return True;
    }
    
typeError:
    *errMsg = "%s called with non-integer argument";
    return False;
}

/*
** Get an string value from a tagged DataValue structure.  Return True
** if conversion succeeded, and store result in *result, otherwise
** return False with an error message in *errMsg.  If an integer value
** is converted, write the string in the space provided by "stringStorage",
** which must be large enough to handle ints of the maximum size.
*/
static int readStringArg(DataValue dv, char **result, char *stringStorage,
    	char **errMsg)
{
    if (dv.tag == STRING_TAG) {
    	*result = dv.val.str.rep;
    	return True;
    } else if (dv.tag == INT_TAG) {
	sprintf(stringStorage, "%d", dv.val.n);
	*result = stringStorage;
	return True;
    }
    *errMsg = "%s called with unknown object";
    return False;
}
