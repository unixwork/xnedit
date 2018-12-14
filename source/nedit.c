/*******************************************************************************
*									       *
* nedit.c -- Nirvana Editor main program				       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
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
* May 10, 1991								       *
*									       *
* Written by Mark Edel							       *
*									       *
* Modifications:							       *
*									       *
*	8/18/93 - Mark Edel & Joy Kyriakopulos - Ported to VMS		       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "nedit.h"
/* #include "textBuf.h" */
#include "file.h"
#include "preferences.h"
#include "regularExp.h"
#include "selection.h"
#include "tags.h"
#include "menu.h"
#include "macro.h"
#include "server.h"
#include "interpret.h"
#include "parse.h"
#include "help.h"
#include "../util/misc.h"
#include "../util/printUtils.h"
#include "../util/fileUtils.h"
#include "../util/getfiles.h"
#include "../util/motif.h"
#include "../util/nedit_malloc.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef NO_XMIM
#include <X11/Xlocale.h>
#else
#include <locale.h>
#endif

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>

#if XmVersion >= 1002
#include <Xm/RepType.h>
#endif

#ifdef VMS
#include <rmsdef.h>
#include "../util/VMSparam.h"
#include "../util/VMSUtils.h"
#else

#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

static void nextArg(int argc, char **argv, int *argIndex);
static int checkDoMacroArg(const char *macro);
static String neditLanguageProc(Display *dpy, String xnl, XtPointer closure);
static void maskArgvKeywords(int argc, char **argv, const char **maskArgs);
static void unmaskArgvKeywords(int argc, char **argv, const char **maskArgs);
static void fixupBrokenXKeysymDB(void);
static void patchResourcesForVisual(void);
static void patchResourcesForKDEbug(void);
static void patchLocaleForMotif(void);
static unsigned char* sanitizeVirtualKeyBindings(void);
static int sortAlphabetical(const void* k1, const void* k2);
static int virtKeyBindingsAreInvalid(const unsigned char* bindings);
static void restoreInsaneVirtualKeyBindings(unsigned char* bindings);
static void noWarningFilter(String);
static void showWarningFilter(String);

WindowInfo *WindowList = NULL;
Display *TheDisplay = NULL;
char *ArgV0 = NULL;
Boolean IsServer = False;
Widget TheAppShell;

/* Reasons for choice of default font qualifications:

   iso8859 appears to be necessary for newer versions of XFree86 that
   default to Unicode encoding, which doesn't quite work with Motif.
   Otherwise Motif puts up garbage (square blocks).

   (This of course, is a stupid default because there are far more iso8859
   apps than Unicode apps.  But the X folks insist it's a client bug.  Hah.)

   RedHat 7.3 won't default to '-1' for an encoding, if left with a *,
   and so reverts to "fixed".  Yech. */

#define NEDIT_DEFAULT_FONT      "-*-helvetica-medium-r-normal-*-14-*-*-*-*-*-iso8859-1," \
                                "-*-helvetica-bold-r-normal-*-14-*-*-*-*-*-iso8859-1=BOLD," \
                                "-*-helvetica-medium-o-normal-*-14-*-*-*-*-*-iso8859-1=ITALIC"

#define NEDIT_FIXED_FONT        "-*-courier-medium-r-normal-*-14-*-*-*-*-*-iso8859-1," \
                                "-*-courier-bold-r-normal-*-14-*-*-*-*-*-iso8859-1=BOLD," \
                                "-*-courier-medium-o-normal-*-14-*-*-*-*-*-iso8859-1=ITALIC"

#define NEDIT_DEFAULT_BG        "#b3b3b3"

#define NEDIT_TEXT_TRANSLATIONS "#override\\n" \
                                "Ctrl~Alt~Meta<KeyPress>v: paste-clipboard()\\n" \
                                "Ctrl~Alt~Meta<KeyPress>c: copy-clipboard()\\n" \
                                "Ctrl~Alt~Meta<KeyPress>x: cut-clipboard()\\n" \
                                "Ctrl~Alt~Meta<KeyPress>u: delete-to-start-of-line()\\n"

static char *fallbackResources[] = {
    /* Try to avoid Motif's horrificly ugly default colors and fonts,
       if the user's environment provides no usable defaults.  We try
       to choose a Windows-y default color setting here.  Editable text 
       fields are forced to a fixed-pitch font for usability.

       By using the VendorShell fontList resources, Motif automatically
       groups the fonts into the right classes.  It's then easier for
       the user or environment to override this sensibly:

         nedit -xrm '*textFontList: myfont'

       This is broken in recent versions of LessTif.

       If using OpenMotif 2.3.3 or better, support XFT fonts.  XFT is
       claimed supported in OpenMotif 2.3.0, but doesn't work very well,
       as insensitive text buttons are blank.  That bug is fixed in 2.3.3.
      */

#if (XmVersion >= 2003 && XmUPDATE_LEVEL >= 3 && USE_XFT == 1)
    "*buttonRenderTable:        defaultRT",
    "*labelRenderTable:         defaultRT",
    "*textRenderTable:          fixedRT",
    "*defaultRT.fontType:       FONT_IS_XFT",
    "*defaultRT.fontName:       Sans",
    "*defaultRT.fontSize:       9",
    "*fixedRT.fontType:         FONT_IS_XFT",
    "*fixedRT.fontName:         Monospace",
    "*fixedRT.fontSize:         9",
#elif LESSTIF_VERSION
    "*FontList: "               NEDIT_DEFAULT_FONT,
    "*XmText.FontList: "        NEDIT_FIXED_FONT,
    "*XmTextField.FontList: "   NEDIT_FIXED_FONT,
    "*XmList.FontList: "        NEDIT_FIXED_FONT,
    "*XmFileSelectionBox*XmList.FontList: " 	 NEDIT_FIXED_FONT,
#else
    "*buttonFontList: "         NEDIT_DEFAULT_FONT,
    "*labelFontList: "          NEDIT_DEFAULT_FONT,
    "*textFontList: "           NEDIT_FIXED_FONT,
#endif

    "*background: "             NEDIT_DEFAULT_BG,
    "*foreground: "             NEDIT_DEFAULT_FG,
    "*XmText.foreground: "      NEDIT_DEFAULT_FG,
    "*XmText.background: "      NEDIT_DEFAULT_TEXT_BG,
    "*XmList.foreground: "      NEDIT_DEFAULT_FG,
    "*XmList.background: "      NEDIT_DEFAULT_TEXT_BG,
    "*XmTextField.foreground: " NEDIT_DEFAULT_FG,
    "*XmTextField.background: " NEDIT_DEFAULT_TEXT_BG,

    /* Use baseTranslations as per Xt Programmer's Manual, 10.2.12 */
    "*XmText.baseTranslations: " NEDIT_TEXT_TRANSLATIONS,
    "*XmTextField.baseTranslations: " NEDIT_TEXT_TRANSLATIONS,

    "*XmLFolder.highlightThickness: 0",
    "*XmLFolder.shadowThickness:    1",
    "*XmLFolder.maxTabWidth:        150",
    "*XmLFolder.traversalOn:        False",
    "*XmLFolder.inactiveForeground: #666" ,
    "*tab.alignment: XmALIGNMENT_BEGINNING",
    "*tab.marginWidth: 0",
    "*tab.marginHeight: 1",

    /* Prevent the file selection box from acting stupid. */
    "*XmFileSelectionBox.resizePolicy: XmRESIZE_NONE",
    "*XmFileSelectionBox.textAccelerators:",
    "*XmFileSelectionBox.pathMode: XmPATH_MODE_RELATIVE",
    "*XmFileSelectionBox.width: 500",
    "*XmFileSelectionBox.height: 400",

    /* NEdit-specific widgets.  Theses things should probably be manually
       jammed into the database, rather than fallbacks.  We really want
       the accelerators to be there even if someone creates an app-defaults
       file against our wishes. */

    "*text.lineNumForeground: " NEDIT_DEFAULT_LINENO_FG,
    "*text.background: " NEDIT_DEFAULT_TEXT_BG,
    "*text.foreground: " NEDIT_DEFAULT_FG,
    "*text.highlightForeground: " NEDIT_DEFAULT_HI_FG,
    "*text.highlightBackground: " NEDIT_DEFAULT_HI_BG,
    "*textFrame.shadowThickness: 1",
    "*menuBar.marginHeight: 0",
    "*menuBar.shadowThickness: 1",
    "*pane.sashHeight: 11",
    "*pane.sashWidth: 11",
    "*pane.marginWidth: 0",
    "*pane.marginHeight: 0",
    "*scrolledW*spacing: 0",
    "*text.selectionArrayCount: 3",
    "*helpText.background: " NEDIT_DEFAULT_HELP_BG,
    "*helpText.foreground: " NEDIT_DEFAULT_HELP_FG,
    "*helpText.selectBackground: " NEDIT_DEFAULT_BG,
    "*statsLine.background: " NEDIT_DEFAULT_BG,
    "*statsLine.FontList: " NEDIT_DEFAULT_FONT,
    "*calltip.background: LemonChiffon1",
    "*calltip.foreground: black",
    "*iSearchForm*highlightThickness: 1",
    "*fileMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*editMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*searchMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*preferencesMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*windowsMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*shellMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*macroMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*helpMenu.tearOffModel: XmTEAR_OFF_ENABLED",
    "*fileMenu.mnemonic: F",
    "*fileMenu.new.accelerator: Ctrl<Key>n",
    "*fileMenu.new.acceleratorText: Ctrl+N",
    "*fileMenu.newOpposite.accelerator: Shift Ctrl<Key>n",
    "*fileMenu.newOpposite.acceleratorText: Shift+Ctrl+N",
    "*fileMenu.open.accelerator: Ctrl<Key>o",
    "*fileMenu.open.acceleratorText: Ctrl+O",
    "*fileMenu.openSelected.accelerator: Ctrl<Key>y",
    "*fileMenu.openSelected.acceleratorText: Ctrl+Y",
    "*fileMenu.close.accelerator: Ctrl<Key>w",
    "*fileMenu.close.acceleratorText: Ctrl+W",
    "*fileMenu.save.accelerator: Ctrl<Key>s",
    "*fileMenu.save.acceleratorText: Ctrl+S",
    "*fileMenu.includeFile.accelerator: Alt<Key>i",
    "*fileMenu.includeFile.acceleratorText: Alt+I",
    "*fileMenu.print.accelerator: Ctrl<Key>p",
    "*fileMenu.print.acceleratorText: Ctrl+P",
    "*fileMenu.exit.accelerator: Ctrl<Key>q",
    "*fileMenu.exit.acceleratorText: Ctrl+Q",
    "*editMenu.mnemonic: E",
    "*editMenu.undo.accelerator: Ctrl<Key>z",
    "*editMenu.undo.acceleratorText: Ctrl+Z",
    "*editMenu.redo.accelerator: Shift Ctrl<Key>z",
    "*editMenu.redo.acceleratorText: Shift+Ctrl+Z",
    /*  Clipboard accelerators prevent the use of the clipboard in iSearch's
        XmText, so they are left out. Their job is done by translations in
        the main text widget, so the acceleratorText is still kept.  */
    "*editMenu.cut.acceleratorText: Ctrl+X",
    "*editMenu.copy.acceleratorText: Ctrl+C",
    "*editMenu.paste.acceleratorText: Ctrl+V",
    "*editMenu.pasteColumn.accelerator: Shift Ctrl<Key>v",
    "*editMenu.pasteColumn.acceleratorText: Ctrl+Shift+V",
    "*editMenu.delete.acceleratorText: Del",
    "*editMenu.selectAll.accelerator: Ctrl<Key>a",
    "*editMenu.selectAll.acceleratorText: Ctrl+A",
    "*editMenu.shiftLeft.accelerator: Ctrl<Key>9",
    "*editMenu.shiftLeft.acceleratorText: [Shift]Ctrl+9",
    "*editMenu.shiftLeftShift.accelerator: Shift Ctrl<Key>9",
    "*editMenu.shiftRight.accelerator: Ctrl<Key>0",
    "*editMenu.shiftRight.acceleratorText: [Shift]Ctrl+0",
    "*editMenu.shiftRightShift.accelerator: Shift Ctrl<Key>0",
    "*editMenu.upperCase.accelerator: Ctrl<Key>6",
    "*editMenu.upperCase.acceleratorText: Ctrl+6",
    "*editMenu.lowerCase.accelerator: Shift Ctrl<Key>6",
    "*editMenu.lowerCase.acceleratorText: Shift+Ctrl+6",
    "*editMenu.fillParagraph.accelerator: Ctrl<Key>j",
    "*editMenu.fillParagraph.acceleratorText: Ctrl+J",
    "*editMenu.insertFormFeed.accelerator: Alt Ctrl<Key>l",
    "*editMenu.insertFormFeed.acceleratorText: Alt+Ctrl+L",
    "*editMenu.insertCtrlCode.accelerator: Alt Ctrl<Key>i",
    "*editMenu.insertCtrlCode.acceleratorText: Alt+Ctrl+I",
    "*searchMenu.mnemonic: S",
    "*searchMenu.find.accelerator: Ctrl<Key>f",
    "*searchMenu.find.acceleratorText: [Shift]Ctrl+F",
    "*searchMenu.findShift.accelerator: Shift Ctrl<Key>f",
    "*searchMenu.findAgain.accelerator: Ctrl<Key>g",
    "*searchMenu.findAgain.acceleratorText: [Shift]Ctrl+G",
    "*searchMenu.findAgainShift.accelerator: Shift Ctrl<Key>g",
    "*searchMenu.findSelection.accelerator: Ctrl<Key>h",
    "*searchMenu.findSelection.acceleratorText: [Shift]Ctrl+H",
    "*searchMenu.findSelectionShift.accelerator: Shift Ctrl<Key>h",
    "*searchMenu.findIncremental.accelerator: Ctrl<Key>i",
    "*searchMenu.findIncrementalShift.accelerator: Shift Ctrl<Key>i",
    "*searchMenu.findIncremental.acceleratorText: [Shift]Ctrl+I",
    "*searchMenu.replace.accelerator: Ctrl<Key>r",
    "*searchMenu.replace.acceleratorText: [Shift]Ctrl+R",
    "*searchMenu.replaceShift.accelerator: Shift Ctrl<Key>r",
    "*searchMenu.findReplace.accelerator: Ctrl<Key>r",
    "*searchMenu.findReplace.acceleratorText: [Shift]Ctrl+R",
    "*searchMenu.findReplaceShift.accelerator: Shift Ctrl<Key>r",
    "*searchMenu.replaceFindAgain.accelerator: Ctrl<Key>t",
    "*searchMenu.replaceFindAgain.acceleratorText: [Shift]Ctrl+T",
    "*searchMenu.replaceFindAgainShift.accelerator: Shift Ctrl<Key>t",
    "*searchMenu.replaceAgain.accelerator: Alt<Key>t",
    "*searchMenu.replaceAgain.acceleratorText: [Shift]Alt+T",
    "*searchMenu.replaceAgainShift.accelerator: Shift Alt<Key>t",
    "*searchMenu.gotoLineNumber.accelerator: Ctrl<Key>l",
    "*searchMenu.gotoLineNumber.acceleratorText: Ctrl+L",
    "*searchMenu.gotoSelected.accelerator: Ctrl<Key>e",
    "*searchMenu.gotoSelected.acceleratorText: Ctrl+E",
    "*searchMenu.mark.accelerator: Alt<Key>m",
    "*searchMenu.mark.acceleratorText: Alt+M a-z",
    "*searchMenu.gotoMark.accelerator: Alt<Key>g",
    "*searchMenu.gotoMark.acceleratorText: [Shift]Alt+G a-z",
    "*searchMenu.gotoMarkShift.accelerator: Shift Alt<Key>g",
    "*searchMenu.gotoMatching.accelerator: Ctrl<Key>m",
    "*searchMenu.gotoMatching.acceleratorText: [Shift]Ctrl+M",
    "*searchMenu.gotoMatchingShift.accelerator: Shift Ctrl<Key>m",
    "*searchMenu.findDefinition.accelerator: Ctrl<Key>d",
    "*searchMenu.findDefinition.acceleratorText: Ctrl+D",
    "*searchMenu.showCalltip.accelerator: Ctrl<Key>apostrophe",
    "*searchMenu.showCalltip.acceleratorText: Ctrl+'",    
    "*preferencesMenu.mnemonic: P",
    "*preferencesMenu.statisticsLine.accelerator: Alt<Key>a",
    "*preferencesMenu.statisticsLine.acceleratorText: Alt+A",
    "*preferencesMenu.overtype.acceleratorText: Insert",
    "*shellMenu.mnemonic: l",
    "*shellMenu.filterSelection.accelerator: Alt<Key>r",
    "*shellMenu.filterSelection.acceleratorText: Alt+R",
    "*shellMenu.executeCommand.accelerator: Alt<Key>x",
    "*shellMenu.executeCommand.acceleratorText: Alt+X",
    "*shellMenu.executeCommandLine.accelerator: Ctrl<Key>KP_Enter",
    "*shellMenu.executeCommandLine.acceleratorText: Ctrl+KP Enter",
    "*shellMenu.cancelShellCommand.accelerator: Ctrl<Key>period",
    "*shellMenu.cancelShellCommand.acceleratorText: Ctrl+.",
    "*macroMenu.mnemonic: c",
    "*macroMenu.learnKeystrokes.accelerator: Alt<Key>k",
    "*macroMenu.learnKeystrokes.acceleratorText: Alt+K",
    "*macroMenu.finishLearn.accelerator: Alt<Key>k",
    "*macroMenu.finishLearn.acceleratorText: Alt+K",
    "*macroMenu.cancelLearn.accelerator: Ctrl<Key>period",
    "*macroMenu.cancelLearn.acceleratorText: Ctrl+.",
    "*macroMenu.replayKeystrokes.accelerator: Ctrl<Key>k",
    "*macroMenu.replayKeystrokes.acceleratorText: Ctrl+K",
    "*macroMenu.repeat.accelerator: Ctrl<Key>comma",
    "*macroMenu.repeat.acceleratorText: Ctrl+,",
    "*windowsMenu.mnemonic: W",
    "*windowsMenu.splitPane.accelerator: Ctrl<Key>2",
    "*windowsMenu.splitPane.acceleratorText: Ctrl+2",
    "*windowsMenu.closePane.accelerator: Ctrl<Key>1",
    "*windowsMenu.closePane.acceleratorText: Ctrl+1",
    "*helpMenu.mnemonic: H",
    "nedit.help.helpForm.sw.helpText*baseTranslations: #override\
<Key>Tab:help-focus-buttons()\\n\
<Key>Return:help-button-action(\"close\")\\n\
Ctrl<Key>F:help-button-action(\"find\")\\n\
Ctrl<Key>G:help-button-action(\"findAgain\")\\n\
<KeyPress>osfCancel:help-button-action(\"close\")\\n\
~Meta~Ctrl~Shift<Btn1Down>:\
    grab-focus() help-hyperlink()\\n\
~Meta~Ctrl~Shift<Btn1Up>:\
    help-hyperlink(\"current\", \"process-cancel\", \"extend-end\")\\n\
~Meta~Ctrl~Shift<Btn2Down>:\
    process-bdrag() help-hyperlink()\\n\
~Meta~Ctrl~Shift<Btn2Up>:\
    help-hyperlink(\"new\", \"process-cancel\", \"copy-to\")",
    NULL
};

static const char cmdLineHelp[] =
#ifndef VMS
"Usage:  nedit [-read] [-create] [-line n | +n] [-server] [-do command]\n\
	      [-tags file] [-tabs n] [-wrap] [-nowrap] [-autowrap]\n\
	      [-autoindent] [-noautoindent] [-autosave] [-noautosave]\n\
	      [-lm languagemode] [-rows n] [-columns n] [-font font]\n\
	      [-geometry geometry] [-iconic] [-noiconic] [-svrname name]\n\
	      [-display [host]:server[.screen] [-xrm resourcestring]\n\
	      [-import file] [-background color] [-foreground color]\n\
	      [-tabbed] [-untabbed] [-group] [-V|-version] [-h|-help]\n\
	      [--] [file...]\n";
#else
"[Sorry, no on-line help available.]\n"; /* Why is that ? */
#endif /*VMS*/

int main(int argc, char **argv)
{
    int i, lineNum, nRead, fileSpecified = FALSE, editFlags = CREATE;
    int gotoLine = False, macroFileRead = False, opts = True;
    int iconic=False, tabbed = -1, group = 0, isTabbed;
    char *toDoCommand = NULL, *geometry = NULL, *langMode = NULL;
    char filename[MAXPATHLEN], pathname[MAXPATHLEN];
    XtAppContext context;
    XrmDatabase prefDB;
    WindowInfo *window = NULL, *lastFile = NULL;
    static const char *protectedKeywords[] = {"-iconic", "-icon", "-geometry",
            "-g", "-rv", "-reverse", "-bd", "-bordercolor", "-borderwidth",
	    "-bw", "-title", NULL};
    unsigned char* invalidBindings = NULL;

    /* Warn user if this has been compiled wrong. */
    enum MotifStability stability = GetMotifStability();

    if (stability == MotifKnownBad) {
        fputs("nedit: WARNING: This version of NEdit is built incorrectly, and will be unstable.\n",
              stderr);
#ifndef BUILD_BROKEN_NEDIT
    /* Dear maintainers who are patching this; please have mercy on your users and link against 
       a stable version of OpenMotif. */
        exit(EXIT_FAILURE);
#endif
    }

    /* Save the command which was used to invoke nedit for restart command */
    ArgV0 = argv[0];

    /* Set locale for C library, X, and Motif input functions. 
       Reverts to "C" if requested locale not available. */
    XtSetLanguageProc(NULL, neditLanguageProc, NULL);
 
    /* Initialize X toolkit (does not open display yet) */
    XtToolkitInitialize();
    context = XtCreateApplicationContext();
    
    /* Set up a warning handler to trap obnoxious Xt grab warnings */
    SuppressPassiveGrabWarnings();

    /* Set up a handler to suppress X warning messages by default */
    XtAppSetWarningHandler(context, noWarningFilter);
    
    /* Set up default resources if no app-defaults file is found */
    XtAppSetFallbackResources(context, fallbackResources);
    
#if XmVersion >= 1002
    /* Allow users to change tear off menus with X resources */
    XmRepTypeInstallTearOffModelConverter();
#endif
    
#ifdef VMS
    /* Convert the command line to Unix style (This is not an ideal solution) */
    ConvertVMSCommandLine(&argc, &argv);
#endif /*VMS*/
#ifdef __EMX__
    /* expand wildcards if necessary */
    _wildcard(&argc, &argv);
#endif
    
    /* Read the preferences file and command line into a database */
    prefDB = CreateNEditPrefDB(&argc, argv);

    /* Open the display and read X database and remaining command line args.
       XtOpenDisplay must be allowed to process some of the resource arguments
       with its inaccessible internal option table, but others, like -geometry
       and -iconic are per-window and it should not be allowed to consume them,
       so we temporarily masked them out. */
    maskArgvKeywords(argc, argv, protectedKeywords);
    /* X.Org 6.8 and above add support for ARGB visuals (with alpha-channel),
       typically with a 32-bit color depth. By default, NEdit uses the visual
       with the largest color depth. However, both OpenMotif and Lesstif
       cannot handle ARGB visuals (crashes, weird display effects, ...), so
       NEdit should avoid selecting such a visual.
       Unfortunatly, there appears to be no reliable way to identify
       ARGB visuals that doesn't require some of the recent X.Org
       extensions. Luckily, the X.Org developers have provided a mechanism
       that can hide these problematic visuals from the application. This can
       be achieved by setting the XLIB_SKIP_ARGB_VISUALS environment variable.
       Users can set this variable before starting NEdit, but it is much 
       more convenient that NEdit takes care of this. This must be done before
       the display is opened (empirically verified). */
    putenv("XLIB_SKIP_ARGB_VISUALS=1");
    TheDisplay = XtOpenDisplay (context, NULL, APP_NAME, APP_CLASS,
	    NULL, 0, &argc, argv);
    unmaskArgvKeywords(argc, argv, protectedKeywords);
    if (!TheDisplay) {
        /* Respond to -V or -version even if there is no display */
        for (i = 1; i < argc && strcmp(argv[i], "--"); i++)
        {
            if (0 == strcmp(argv[i], "-V") || 0 == strcmp(argv[i], "-version"))
            {
                PrintVersion();
                exit(EXIT_SUCCESS);
            }
        }
	fputs ("NEdit: Can't open display\n", stderr);
	exit(EXIT_FAILURE);
    }
    
    /* Must be done before creating widgets */
    fixupBrokenXKeysymDB();
    patchResourcesForVisual();
    patchResourcesForKDEbug();
    
    /* Initialize global symbols and subroutines used in the macro language */
    InitMacroGlobals();
    RegisterMacroSubroutines();

    /* Store preferences from the command line and .nedit file, 
       and set the appropriate preferences */
    RestoreNEditPrefs(prefDB, XtDatabase(TheDisplay));

    /* Intercept syntactically invalid virtual key bindings BEFORE we 
       create any shells. */
    invalidBindings = sanitizeVirtualKeyBindings();
    
    /* Create a hidden application shell that is the parent of all the
       main editor windows.  Realize it so it the window can act as 
       group leader. */
    TheAppShell = CreateShellWithBestVis(APP_NAME, 
                                         APP_CLASS,
                                         applicationShellWidgetClass,
                                         TheDisplay,
                                         NULL,
                                         0);
    
    /* Restore the original bindings ASAP such that other apps are not affected. */
    restoreInsaneVirtualKeyBindings(invalidBindings);

    XtSetMappedWhenManaged(TheAppShell, False);
    XtRealizeWidget(TheAppShell);

#ifndef NO_SESSION_RESTART
    AttachSessionMgrHandler(TheAppShell);
#endif

    /* More preference stuff */
    LoadPrintPreferences(XtDatabase(TheDisplay), APP_NAME, APP_CLASS, True);
    SetDeleteRemap(GetPrefMapDelete());
    SetPointerCenteredDialogs(GetPrefRepositionDialogs());
    SetGetEFTextFieldRemoval(!GetPrefStdOpenDialog());
    
    /* Set up action procedures for menu item commands */
    InstallMenuActions(context);
    
    /* Add Actions for following hyperlinks in the help window */
    InstallHelpLinkActions(context);
    /* Add actions for mouse wheel support in scrolled windows (except text
       area) */
    InstallMouseWheelActions(context);
    
    /* Install word delimiters for regular expression matching */
    SetREDefaultWordDelimiters(GetPrefDelimiters());
    
    /* Read the nedit dynamic database of files for the Open Previous
       command (and eventually other information as well) */
    ReadNEditDB();
    
    /* Process -import command line argument before others which might
       open windows (loading preferences doesn't update menu settings,
       which would then be out of sync with the real preference settings) */
    for (i=1; i<argc; i++) {
      	if(!strcmp(argv[i], "--")) {
	    break; /* treat all remaining arguments as filenames */
	} else if (!strcmp(argv[i], "-import")) {
    	    nextArg(argc, argv, &i);
    	    ImportPrefFile(argv[i], False);
	} else if (!strcmp(argv[i], "-importold")) {
    	    nextArg(argc, argv, &i);
    	    ImportPrefFile(argv[i], True);
	}
    }
    
    /* Load the default tags file. Don't complain if it doesn't load, the tag
       file resource is intended to be set and forgotten.  Running nedit in a
       directory without a tags should not cause it to spew out errors. */
    if (*GetPrefTagFile() != '\0') {
        AddTagsFile(GetPrefTagFile(), TAG);
    }

    if (strcmp(GetPrefServerName(), "") != 0) {
        IsServer = True;
    }

    /* Process any command line arguments (-tags, -do, -read, -create,
       +<line_number>, -line, -server, and files to edit) not already
       processed by RestoreNEditPrefs. */
    fileSpecified = FALSE;
    for (i=1; i<argc; i++) {
        if (opts && !strcmp(argv[i], "--")) { 
    	    opts = False; /* treat all remaining arguments as filenames */
	    continue;
	} else if (opts && !strcmp(argv[i], "-tags")) {
    	    nextArg(argc, argv, &i);
    	    if (!AddTagsFile(argv[i], TAG))
    	    	fprintf(stderr, "NEdit: Unable to load tags file\n");
    	} else if (opts && !strcmp(argv[i], "-do")) {
    	    nextArg(argc, argv, &i);
	    if (checkDoMacroArg(argv[i]))
    	    	toDoCommand = argv[i];
    	} else if (opts && !strcmp(argv[i], "-read")) {
    	    editFlags |= PREF_READ_ONLY;
    	} else if (opts && !strcmp(argv[i], "-create")) {
    	    editFlags |= SUPPRESS_CREATE_WARN;
    	} else if (opts && !strcmp(argv[i], "-tabbed")) {
    	    tabbed = 1;
    	    group = 0;	/* override -group option */
    	} else if (opts && !strcmp(argv[i], "-untabbed")) {
    	    tabbed = 0;
    	    group = 0;	/* override -group option */
    	} else if (opts && !strcmp(argv[i], "-group")) {
    	    group = 2; /* 2: start new group, 1: in group */
    	} else if (opts && !strcmp(argv[i], "-line")) {
    	    nextArg(argc, argv, &i);
	    nRead = sscanf(argv[i], "%d", &lineNum);
	    if (nRead != 1)
    		fprintf(stderr, "NEdit: argument to line should be a number\n");
    	    else
    	    	gotoLine = True;
    	} else if (opts && (*argv[i] == '+')) {
    	    nRead = sscanf((argv[i]+1), "%d", &lineNum);
	    if (nRead != 1)
    		fprintf(stderr, "NEdit: argument to + should be a number\n");
    	    else
    	    	gotoLine = True;
    	} else if (opts && !strcmp(argv[i], "-server")) {
    	    IsServer = True;
        } else if (opts && !strcmp(argv[i], "-xwarn")) {
            XtAppSetWarningHandler(context, showWarningFilter);
	} else if (opts && (!strcmp(argv[i], "-iconic") || 
	                    !strcmp(argv[i], "-icon"))) {
    	    iconic = True;
	} else if (opts && !strcmp(argv[i], "-noiconic")) {
    	    iconic = False;
	} else if (opts && (!strcmp(argv[i], "-geometry") || 
	                    !strcmp(argv[i], "-g"))) {
	    nextArg(argc, argv, &i);
    	    geometry = argv[i];
	} else if (opts && !strcmp(argv[i], "-lm")) {
	    nextArg(argc, argv, &i);
    	    langMode = argv[i];
	} else if (opts && !strcmp(argv[i], "-import")) {
	    nextArg(argc, argv, &i); /* already processed, skip */
	} else if (opts && (!strcmp(argv[i], "-V") || 
	                    !strcmp(argv[i], "-version"))) {
	    PrintVersion();
	    exit(EXIT_SUCCESS);
	} else if (opts && (!strcmp(argv[i], "-h") ||
			    !strcmp(argv[i], "-help"))) {
	    fprintf(stderr, "%s", cmdLineHelp);
	    exit(EXIT_SUCCESS);
	} else if (opts && (*argv[i] == '-')) {
#ifdef VMS
	    *argv[i] = '/';
#endif /*VMS*/
    	    fprintf(stderr, "nedit: Unrecognized option %s\n%s", argv[i],
    	    	    cmdLineHelp);
    	    exit(EXIT_FAILURE);
    	} else {
#ifdef VMS
	    int numFiles, j;
	    char **nameList = NULL;
	    /* Use VMS's LIB$FILESCAN for filename in argv[i] to process */
	    /* wildcards and to obtain a full VMS file specification     */
	    numFiles = VMSFileScan(argv[i], &nameList, NULL, INCLUDE_FNF);
            /* Should we warn the user if he provided a -line or -do switch
               and a wildcard pattern that expands to more than one file?  */
	    /* for each expanded file name do: */
	    for (j = 0; j < numFiles; ++j) {
	    	if (ParseFilename(nameList[j], filename, pathname) == 0) {
		    /* determine if file is to be openned in new tab, by
		       factoring the options -group, -tabbed & -untabbed */
    		    if (group == 2) {
	        	isTabbed = 0;  /* start a new window for new group */
			group = 1;     /* next file will be within group */
		    } else if (group == 1) {
	    		isTabbed = 1;  /* new tab for file in group */
		    } else {           /* not in group */
	    		isTabbed = tabbed==-1? GetPrefOpenInTab() : tabbed; 
		    }

		    /* Files are opened in background to improve opening speed
		       by defering certain time  consuiming task such as syntax
		       highlighting. At the end of the file-opening loop, the 
		       last file opened will be raised to restore those deferred
		       items. The current file may also be raised if there're
		       macros to execute on. */
		    window = EditExistingFile(WindowList, filename, pathname, 
		    	    editFlags, geometry, iconic, langMode, isTabbed, 
			    True);
		    fileSpecified = TRUE;

		    if (window) {
			CleanUpTabBarExposeQueue(window);

			/* raise the last file of previous window */
			if (lastFile && window->shell != lastFile->shell) {
			    CleanUpTabBarExposeQueue(lastFile);
			    RaiseDocument(lastFile);
			}
			
			if (!macroFileRead) {
		            ReadMacroInitFile(WindowList);
		            macroFileRead = True;
			}
	    		if (gotoLine)
	    	            SelectNumberedLine(window, lineNum);
			if (toDoCommand != NULL) {
			    DoMacro(window, toDoCommand, "-do macro");
			    toDoCommand = NULL;
			    if (!IsValidWindow(window))
		    		window = NULL; /* window closed by macro */
			    if (lastFile && !IsValidWindow(lastFile))
		    		lastFile = NULL; /* window closed by macro */
			}
		    }
		    
		    /* register last opened file for later use */
		    if (window)
    	    		lastFile = window;
                } else {
		    fprintf(stderr, "nedit: file name too long: %s\n", nameList[j]);
                }
		free(nameList[j]);
	    }
	    if (nameList != NULL)
	    	free(nameList);
#else
	    if (ParseFilename(argv[i], filename, pathname) == 0 ) {
		/* determine if file is to be openned in new tab, by
		   factoring the options -group, -tabbed & -untabbed */
    		if (group == 2) {
	            isTabbed = 0;  /* start a new window for new group */
		    group = 1;     /* next file will be within group */
		} else if (group == 1) {
	    	    isTabbed = 1;  /* new tab for file in group */
		} else {           /* not in group */
	    	    isTabbed = tabbed==-1? GetPrefOpenInTab() : tabbed; 
		}
		
		/* Files are opened in background to improve opening speed
		   by defering certain time  consuiming task such as syntax
		   highlighting. At the end of the file-opening loop, the 
		   last file opened will be raised to restore those deferred
		   items. The current file may also be raised if there're
		   macros to execute on. */
		window = EditExistingFile(WindowList, filename, pathname, 
		    	editFlags, geometry, iconic, langMode, isTabbed, True);
    	    	fileSpecified = TRUE;
		if (window) {
		    CleanUpTabBarExposeQueue(window);

		    /* raise the last tab of previous window */
		    if (lastFile && window->shell != lastFile->shell) {
			CleanUpTabBarExposeQueue(lastFile);
			RaiseDocument(lastFile);
		    }

		    if (!macroFileRead) {
			ReadMacroInitFile(WindowList);
			macroFileRead = True;
		    }
		    if (gotoLine)
			SelectNumberedLine(window, lineNum);
		    if (toDoCommand != NULL) {
			DoMacro(window, toDoCommand, "-do macro");
	    	    	toDoCommand = NULL;
			if (!IsValidWindow(window))
		    	    window = NULL; /* window closed by macro */
			if (lastFile && !IsValidWindow(lastFile))
		    	    lastFile = NULL; /* window closed by macro */
		    }
		}
		
		/* register last opened file for later use */
		if (window)
    	    	    lastFile = window;
	    } else {
		fprintf(stderr, "nedit: file name too long: %s\n", argv[i]);
	    }
#endif /*VMS*/

            /* -line/+n does only affect the file following this switch */
            gotoLine = False;
	}
    }
#ifdef VMS
    VMSFileScanDone();
#endif /*VMS*/
    
    /* Raise the last file opened */
    if (lastFile) {
	CleanUpTabBarExposeQueue(lastFile);
	RaiseDocument(lastFile);
    }
    CheckCloseDim();

    /* If no file to edit was specified, open a window to edit "Untitled" */
    if (!fileSpecified) {
    	EditNewFile(NULL, geometry, iconic, langMode, NULL);
	ReadMacroInitFile(WindowList);
	CheckCloseDim();
	if (toDoCommand != NULL)
	    DoMacro(WindowList, toDoCommand, "-do macro");
    }
    
    /* Begin remembering last command invoked for "Repeat" menu item */
    AddLastCommandActionHook(context);

    /* Set up communication port and write ~/.nedit_server_process file */
    if (IsServer)
    	InitServerCommunication();

    /* Process events. */
    if (IsServer)
    	ServerMainLoop(context);
    else
    	XtAppMainLoop(context);

    /* Not reached but this keeps some picky compilers happy */
    return EXIT_SUCCESS;
}

static void nextArg(int argc, char **argv, int *argIndex)
{
    if (*argIndex + 1 >= argc) {
#ifdef VMS
	    *argv[*argIndex] = '/';
#endif /*VMS*/
    	fprintf(stderr, "NEdit: %s requires an argument\n%s", argv[*argIndex],
    	        cmdLineHelp);
    	exit(EXIT_FAILURE);
    }
    (*argIndex)++;
}

/*
** Return True if -do macro is valid, otherwise write an error on stderr
*/
static int checkDoMacroArg(const char *macro)
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
    
    /* Do a test parse */
    prog = ParseMacro(tMacro, &errMsg, &stoppedAt);
    NEditFree(tMacro);
    if (prog == NULL) {
    	ParseError(NULL, tMacro, stoppedAt, "argument to -do", errMsg);
	return False;
    }
    FreeProgram(prog);
    return True;
}

/*
** maskArgvKeywords and unmaskArgvKeywords mangle selected keywords by
** replacing the '-' with a space, for the purpose of hiding them from
** XtOpenDisplay's option processing.  Why this silly scheme?  XtOpenDisplay
** really needs to see command line arguments, particularly -display, but
** there's no way to change the option processing table it uses, to keep
** it from consuming arguments which are meant to apply per-window, like
** -geometry and -iconic.
*/
static void maskArgvKeywords(int argc, char **argv, const char **maskArgs)
{
    int i, k;

    for (i=1; i<argc; i++)
	for (k=0; maskArgs[k]!=NULL; k++)
	    if (!strcmp(argv[i], maskArgs[k]))
    		argv[i][0] = ' ';
}


static void unmaskArgvKeywords(int argc, char **argv, const char **maskArgs)
{
    int i, k;

    for (i=1; i<argc; i++)
	for (k=0; maskArgs[k]!=NULL; k++)
	    if (argv[i][0]==' ' && !strcmp(&argv[i][1], &maskArgs[k][1]))
    		argv[i][0] = '-';
}


/*
** Some Linux distros ship with XKEYSYMDB set to a bogus filename which
** breaks all Motif applications.  Ignore that, and let X fall back on the
** default which is far more likely to work.
*/
static void fixupBrokenXKeysymDB(void)
{
    const char *keysym = getenv("XKEYSYMDB");
    
    if (keysym != NULL && access(keysym, F_OK) != 0)
        putenv("XKEYSYMDB");
}

/*
** If we're not using the default visual, then some default resources in
** the database (colors) are not valid, because they are indexes into the
** default colormap.  If we used them blindly, then we'd get "random"
** unreadable colors.  So we inspect the resource list, and use the
** fallback "grey" color instead if this is the case.
*/
static void patchResourcesForVisual(void)
{
    Cardinal    i;
    Visual     *visual;
    int         depth;
    Colormap    map;
    Boolean     usingDefaultVisual;
    XrmDatabase db;
    
    if (!TheDisplay)
        return;

    db = XtDatabase(TheDisplay);
    
    usingDefaultVisual = FindBestVisual(TheDisplay,
                                        APP_NAME,
                                        APP_CLASS,
                                        &visual,
                                        &depth,
                                        &map);

    if (!usingDefaultVisual)
    {
#ifndef LESSTIF_VERSION
          /* 
             For non-Lesstif versions, we have to put non-default visuals etc.
             in the resource data base to make sure that all (shell) widgets
             inherit them, especially Motif's own shells (eg, drag icons).
             
             For Lesstif, this doesn't work, but luckily, Lesstif handles 
             non-default visuals etc. properly for its own shells and 
             we can take care of things for our shells (eg, call tips) through
             our shell creation wrappers in misc.c. 
          */
            
          XrmValue value;
          value.addr = (XPointer)&visual;
          value.size = sizeof(visual);
          XrmPutResource(&db, "*visual", "Visual", &value);
          value.addr = (XPointer)&map;
          value.size = sizeof(map);
          XrmPutResource(&db, "*colormap", "Colormap", &value);
          value.addr = (XPointer)&depth;
          value.size = sizeof(depth);
          XrmPutResource(&db, "*depth", "Int", &value);

        /* Drag-and-drop visuals do not work well when using a different
           visual.  One some systems, you'll just get a funny-looking icon
           (maybe all-black) but on other systems it crashes with a BadMatch
           error.  This appears to be an OSF Motif bug.  It would be nicer 
           to just disable the visual itself, instead of the entire drag
           operation.
          
           Update: this is no longer necessary since all problems with 
           non-default visuals should now be solved. 
           
           XrmPutStringResource(&db, "*dragInitiatorProtocolStyle", "DRAG_NONE");
         */
#endif

        for (i = 1; i < XtNumber(fallbackResources); ++i)
        {
            Cardinal resIndex = i - 1;
            
            if (strstr(fallbackResources[resIndex], "*background:") ||
                strstr(fallbackResources[resIndex], "*foreground:"))
            {
                /* Qualify by application name to prevent them from being
                   converted against the wrong colormap. */
                char buf[1024] = "*" APP_NAME;
                strcat(buf, fallbackResources[resIndex]);
                XrmPutLineResource(&db, buf);
            }
        }
    }
}

/*
** Several KDE version (2.x and 3.x) ship with a template application-default 
** file for NEdit in which several strings have to be substituted in order to 
** make it a valid .ad file. However, for some reason (a KDE bug?), the
** template sometimes ends up in the resource db unmodified, such that several
** invalid entries are present. This function checks for the presence of such
** invalid entries and silently replaces them with NEdit's default values where
** necessary. Without this, NEdit will typically write several warnings to 
** the terminal (Cannot convert string "FONTLIST" to type FontStruct etc) and
** fall back on some really ugly colors and fonts.
*/
static void patchResourcesForKDEbug(void)
{
    /*
     * These are the resources found the Nedit.ad template shipped with KDE 3.0.
     */
    static const char* buggyResources[][3] = { 
     { "*fontList",               "FONTLIST",          NEDIT_DEFAULT_FONT     }, 
     { "*XmText.background",      "BACKGROUND",        NEDIT_DEFAULT_TEXT_BG  }, 
     { "*XmText.foreground",      "FOREGROUND",        NEDIT_DEFAULT_FG       }, 
     { "*XmTextField.background", "BACKGROUND",        NEDIT_DEFAULT_TEXT_BG  }, 
     { "*XmTextField.foreground", "FOREGROUND",        NEDIT_DEFAULT_FG       }, 
     { "*XmList.background",      "BACKGROUND",        NEDIT_DEFAULT_TEXT_BG  }, 
     { "*XmList.foreground",      "FOREGROUND",        NEDIT_DEFAULT_FG       }, 
     { "*helpText.background",    "BACKGROUND",        NEDIT_DEFAULT_HELP_BG  },
     { "*helpText.foreground",    "FOREGROUND",        NEDIT_DEFAULT_HELP_FG  },
     { "*background",             "BACKGROUND",        NEDIT_DEFAULT_BG       },
     { "*foreground",             "FOREGROUND",        NEDIT_DEFAULT_FG,      },
     { "*selectColor",            "BACKGROUND",        NEDIT_DEFAULT_SEL_BG   },
     { "*highlightColor",         "BACKGROUND",        NEDIT_DEFAULT_HI_BG    },
     { "*text.background",        "WINDOW_BACKGROUND", NEDIT_DEFAULT_TEXT_BG  },
     { "*text.foreground",        "WINDOW_FOREGROUND", NEDIT_DEFAULT_FG       },
     { "*text.selectBackground",  "SELECT_BACKGROUND", NEDIT_DEFAULT_SEL_BG   },
     { "*text.selectForeground",  "SELECT_FOREGROUND", NEDIT_DEFAULT_SEL_FG   },
     { "*text.cursorForeground",  "WINDOW_FOREGROUND", NEDIT_DEFAULT_CURSOR_FG},
  /* { "*remapDeleteKey",         "False",                                    }, OK */
  /* { "!*text.heavyCursor",      "True"                                      }, OK */
  /* { "!*BlinkRate",             "0"                                         }, OK */
  /* { "*shell",                  "/bin/sh"                                   }, OK */
     { "*statsLine.background",   "BACKGROUND",        NEDIT_DEFAULT_BG       },
     { "*statsLine.foreground",   "FOREGROUND",        NEDIT_DEFAULT_FG       },
     { NULL,                      NULL,                NULL                   } };
    XrmDatabase db;
    int i;
    
    if (!TheDisplay)
        return;

    db = XtDatabase(TheDisplay);
    
    i = 0;
    while (buggyResources[i][0])
    {
        const char* resource = buggyResources[i][0];
        const char* buggyValue = buggyResources[i][1];
        const char* defaultValue = buggyResources[i][2];
        char name[128] = APP_NAME;
        char class[128] = APP_CLASS;
        char* type;
        XrmValue resValue;
        
        strcat(name, resource);
        strcat(class, resource); /* Is this ok ? */
        
        if (XrmGetResource(db, name, class, &type, &resValue) &&
            !strcmp(type, XmRString))
        {
            /* Buggy value? Replace by the default. */
            if (!strcmp(resValue.addr, buggyValue))
            {
                XrmPutStringResource(&db, &name[0], (char*)defaultValue);
            }
        }    
        ++i;
    }
}

/*
** It seems OSF Motif cannot handle locales with UTF-8 at the end, crashing
** in various places.  The easiest one to find is to open the File Open
** dialog box.  So we lop off UTF-8 if it's there and continue.  Newer 
** versions of Linux distros (e.g., RedHat 8) set the default language to
** to have "UTF-8" at the end, so users were seeing these crashes.
**
** LessTif and OpenMotif 2.3 don't appear to have this problem.
**
** Some versions OpenMotif 2.2.3 don't have this problem, but there's
** no way to tell which.
*/

static void patchLocaleForMotif(void)
{
#if !(defined(LESSTIF_VERSION) || XmVersion >= 2003)
    const char *ctype;
    char ctypebuf[1024];
    char *utf_start;
    
    /* We have to check LC_CTYPE specifically here, because the system
       might specify different locales for different categories (why
       anyone would want to do this is beyond me).  As far as I can
       tell, only LC_CTYPE causes OSF Motif to crash.  If it turns out
       others do, we'll have to iterate over a list of locale cateogries
       and patch every one of them. */
       
    ctype = setlocale(LC_CTYPE, NULL);
    
    if (!ctype)
        return;

    strncpy(ctypebuf, ctype, sizeof ctypebuf);

    if ((utf_start = strstr(ctypebuf, ".utf8")) || 
        (utf_start = strstr(ctypebuf, ".UTF-8")))
    {
        *utf_start = '\0'; /* Samurai chop */
        setlocale(LC_CTYPE, ctypebuf);
    }
#endif
}


/*
** Same as the default X language procedure, except we check if Motif can
** handle the locale as well.
*/

static String neditLanguageProc(Display *dpy, String xnl, XtPointer closure)
{
    /* "xnl" will be set if the user passes in a new language via the
       "-xnllanguage" flag.  If it's empty, then setlocale will get
       the default locale by some system-dependent means (usually, 
       reading some environment variables). */

    if (! setlocale(LC_ALL, xnl)) {
        XtWarning("locale not supported by C library, locale unchanged");
    }

    patchLocaleForMotif();

    if (! XSupportsLocale()) {
        XtWarning("locale not supported by Xlib, locale set to C");
        setlocale(LC_ALL, "C");
    }
    if (! XSetLocaleModifiers(""))
        XtWarning("X locale modifiers not supported, using default");

    return setlocale(LC_ALL, NULL); /* re-query in case overwritten */
}

static int sortAlphabetical(const void* k1, const void* k2)
{
    const char* key1 = *(const char**)k1;
    const char* key2 = *(const char**)k2;
    return strcmp(key1, key2);
}

/*
 * Checks whether a given virtual key binding string is invalid. 
 * A binding is considered invalid if there are duplicate key entries.
 */
static int virtKeyBindingsAreInvalid(const unsigned char* bindings)
{
    int maxCount = 1, i, count;
    const char  *pos = (const char*)bindings;
    char *copy;
    char *pos2, *pos3;
    char **keys;

    /* First count the number of bindings; bindings are separated by \n
       strings. The number of bindings equals the number of \n + 1.
       Beware of leading and trailing \n; the number is actually an
       upper bound on the number of entries. */
    while ((pos = strstr(pos, "\n"))) 
    { 
        ++pos; 
        ++maxCount;
    }
    
    if (maxCount == 1) 
        return False; /* One binding is always ok */
    
    keys = (char**)NEditMalloc(maxCount*sizeof(char*));
    copy = NEditStrdup((const char*)bindings);
    i = 0;
    pos2 = copy;
    
    count = 0;
    while (i<maxCount && pos2 && *pos2)
    {
        while (isspace((int) *pos2) || *pos2 == '\n') ++pos2;
        
        if (*pos2 == '!') /* Ignore comment lines */
        {
            pos2 = strstr(pos2, "\n");
            continue; /* Go to the next line */
        }

        if (*pos2)
        {
            keys[i++] = pos2;
            ++count;
            pos3 = strstr(pos2, ":");
            if (pos3) 
            {
                *pos3++ = 0; /* Cut the string and jump to the next entry */
                pos2 = pos3;
            }
            pos2 = strstr(pos2, "\n");
        }
    }
    
    if (count <= 1)
    {
       free(keys);
       NEditFree(copy);
       return False; /* No conflict */
    }
    
    /* Sort the keys and look for duplicates */
    qsort((void*)keys, count, sizeof(const char*), sortAlphabetical);
    for (i=1; i<count; ++i)
    {
        if (!strcmp(keys[i-1], keys[i]))
        {
            /* Duplicate detected */
            free(keys);
            NEditFree(copy);
            return True;
        }
    }
    free(keys);
    NEditFree(copy);
    return False;
}

/*
 * Optionally sanitizes the Motif default virtual key bindings. 
 * Some applications install invalid bindings (attached to the root window),
 * which cause certain keys to malfunction in NEdit.
 * Through an X-resource, users can choose whether they want
 *   - to always keep the existing bindings
 *   - to override the bindings only if they are invalid
 *   - to always override the existing bindings.
 */

static Atom virtKeyAtom;

static unsigned char* sanitizeVirtualKeyBindings(void)
{
    int overrideBindings = GetPrefOverrideVirtKeyBindings();
    Window rootWindow;
    const char *virtKeyPropName = "_MOTIF_DEFAULT_BINDINGS";
    Atom dummyAtom;
    int getFmt;
    unsigned long dummyULong, nItems;
    unsigned char *insaneVirtKeyBindings = NULL;
    
    if (overrideBindings == VIRT_KEY_OVERRIDE_NEVER) return NULL;
    
    virtKeyAtom =  XInternAtom(TheDisplay, virtKeyPropName, False);
    rootWindow = RootWindow(TheDisplay, DefaultScreen(TheDisplay));

    /* Remove the property, if it exists; we'll restore it later again */
    if (XGetWindowProperty(TheDisplay, rootWindow, virtKeyAtom, 0, INT_MAX, 
                           True, XA_STRING, &dummyAtom, &getFmt, &nItems, 
                           &dummyULong, &insaneVirtKeyBindings) != Success 
        || nItems == 0) 
    {
        return NULL; /* No binding yet; nothing to do */
    }
    
    if (overrideBindings == VIRT_KEY_OVERRIDE_AUTO)
    {   
        if (!virtKeyBindingsAreInvalid(insaneVirtKeyBindings))
        {
            /* Restore the property immediately; it seems valid */
            XChangeProperty(TheDisplay, rootWindow, virtKeyAtom, XA_STRING, 8,
                            PropModeReplace, insaneVirtKeyBindings, 
                            strlen((const char*)insaneVirtKeyBindings));
            XFree((char*)insaneVirtKeyBindings);
            return NULL; /* Prevent restoration */
        }
    }
    return insaneVirtKeyBindings;
}

/*
 * NEdit should not mess with the bindings installed by other apps, so we
 * just restore whatever was installed, if necessary
 */
static void restoreInsaneVirtualKeyBindings(unsigned char *insaneVirtKeyBindings)
{
   if (insaneVirtKeyBindings)
   {
      Window rootWindow = RootWindow(TheDisplay, DefaultScreen(TheDisplay));
      /* Restore the root window atom, such that we don't affect 
         other apps. */
      XChangeProperty(TheDisplay, rootWindow, virtKeyAtom, XA_STRING, 8,
                      PropModeReplace, insaneVirtKeyBindings, 
                      strlen((const char*)insaneVirtKeyBindings));
      XFree((char*)insaneVirtKeyBindings);
   }
}

/*
** Warning handler that suppresses harmless but annoying warnings generated
** by non-production Lesstif versions.
*/
static void showWarningFilter(String message)
{
  const char* bogusMessages[] = {
#ifdef LESSTIF_VERSION
    "XmFontListCreate() is an obsolete function!",
    "No type converter registered for 'String' to 'PathMode' conversion.",
    "XtRemoveGrab asked to remove a widget not on the list",
#endif
    NULL 
  };
  const char **bogusMessage = &bogusMessages[0]; 
  
  while (*bogusMessage) {
      size_t bogusLen = strlen(*bogusMessage);
      if (strncmp(message, *bogusMessage, bogusLen) == 0) {
#ifdef DEBUG_LESSTIF_WARNINGS
         /* Developers may want to see which messages are suppressed. */
         fprintf(stderr, "[SUPPRESSED] %s\n", message);
#endif        
         return;
      }
      ++bogusMessage;
  }
  
  /* An unknown message. Keep it. */
  fprintf(stderr, "%s\n", message);
}

static void noWarningFilter(String message)
{
  return;
}
