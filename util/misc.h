/*******************************************************************************
*                                                                              *
* misc.h -- Nirvana Editor Miscellaneous Header File                           *
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

#ifndef NEDIT_MISC_H_INCLUDED
#define NEDIT_MISC_H_INCLUDED

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/CutPaste.h>

#define TEXT_READ_OK 0
#define TEXT_IS_BLANK 1
#define TEXT_NOT_NUMBER 2

/* maximum length for a window geometry string */
#define MAX_GEOM_STRING_LEN 24

/* Maximum length for a menu accelerator string.
   Which e.g. can be parsed by misc.c:parseAccelString()
   (how many modifier keys can you hold down at once?) */
#define MAX_ACCEL_LEN 100

/*  button margin width to avoid cramped buttons  */
#define BUTTON_WIDTH_MARGIN 12


void AddMotifCloseCallback(Widget shell, XtCallbackProc closeCB, void *arg);
void SuppressPassiveGrabWarnings(void);
void PopDownBugPatch(Widget w);
void RemapDeleteKey(Widget w);
void SetDeleteRemap(int state);
void RealizeWithoutForcingPosition(Widget shell);
void RemovePPositionHint(Widget shell);
void ManageDialogCenteredOnPointer(Widget dialogChild);
void SetPointerCenteredDialogs(int state);
void RaiseDialogWindow(Widget shell);
void RaiseShellWindow(Widget shell, Boolean focus);
void RaiseWindow(Display *display, Window w, Boolean focus);
void AddDialogMnemonicHandler(Widget dialog, int unmodifiedToo);
void RemoveDialogMnemonicHandler(Widget dialog);
void AccelLockBugPatch(Widget topWidget, Widget topMenuContainer);
void UpdateAccelLockPatch(Widget topWidget, Widget newButton);
char *GetXmStringText(XmString fromString);
XFontStruct *GetDefaultFontStruct(Display *d, XmFontList font);
XmString* StringTable(int count, ...);
void FreeStringTable(XmString *table);
void SimulateButtonPress(Widget widget);
Widget AddMenuItem(Widget parent, char *name, char *label, char mnemonic,
	char *acc, char *accText, XtCallbackProc callback, void *cbArg);
Widget AddMenuToggle(Widget parent, char *name, char *label, char mnemonic,
	char *acc, char *accText, XtCallbackProc callback, void *cbArg,int set);
Widget AddSubMenu(Widget parent, char *name, char *label, char mnemonic);
void SetIntText(Widget text, int value);
int GetFloatText(Widget text, double *value);
int GetIntText(Widget text, int *value);
int GetFloatTextWarn(Widget text, double *value, const char *fieldName, int warnBlank);
int GetIntTextWarn(Widget text, int *value, const char *fieldName, int warnBlank);
int TextWidgetIsBlank(Widget textW);
void MakeSingleLineTextW(Widget textW);
void BeginWait(Widget topCursorWidget);
void BusyWait(Widget anyWidget);
void EndWait(Widget topCursorWidget);
void PasswordText(Widget w, char *passTxt);
void AddHistoryToTextWidget(Widget textW, char ***historyList, int *nItems);
void AddToHistoryList(char *newItem, char ***historyList, int *nItems);
void CreateGeometryString(char *string, int x, int y,
	int width, int height, int bitmask);
Boolean FindBestVisual(Display *display, const char *appName, const char *appClass,
	Visual **visual, int *depth, Colormap *colormap);
Widget CreateDialogShell(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreatePopupMenu(Widget parent, char *name, ArgList arglist,
	Cardinal argcount);
Widget CreatePulldownMenu(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreatePromptDialog(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreateSelectionDialog(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreateFormDialog(Widget parent, char *name, ArgList arglist,
    	Cardinal  argcount);
Widget CreateFileSelectionDialog(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreateQuestionDialog(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreateMessageDialog(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreateErrorDialog(Widget parent, char *name, ArgList arglist,
	Cardinal  argcount);
Widget CreateShellWithBestVis(String appName, String appClass, 
	WidgetClass class, Display *display, ArgList args, Cardinal nArgs);
Widget CreatePopupShellWithBestVis(String shellName, WidgetClass class,
    Widget parent, ArgList arglist, Cardinal argcount);
Widget CreateWidget(Widget parent, const char *name, WidgetClass class,
	ArgList arglist, Cardinal  argcount);
Modifiers GetNumLockModMask(Display *display);
void InstallMouseWheelActions(XtAppContext context);
void AddMouseWheelSupport(Widget w);
void RadioButtonChangeState(Widget widget, Boolean state, Boolean notify);
void CloseAllPopupsFor(Widget shell);
long QueryCurrentDesktop(Display *display, Window rootWindow);
long QueryDesktop(Display *display, Widget shell);
int SpinClipboardStartCopy(Display *display, Window window,
        XmString clip_label, Time timestamp, Widget widget,
        XmCutPasteProc callback, long *item_id);
int SpinClipboardCopy(Display *display, Window window, long item_id,
        char *format_name, XtPointer buffer, unsigned long length,
        long private_id, long *data_id);
int SpinClipboardEndCopy(Display *display, Window window, long item_id);
int SpinClipboardInquireLength(Display *display, Window window,
        char *format_name, unsigned long *length);
int SpinClipboardRetrieve(Display *display, Window window, char *format_name,
        XtPointer buffer, unsigned long length, unsigned long *num_bytes,
        long *private_id);
int SpinClipboardLock(Display *display, Window window);
int SpinClipboardUnlock(Display *display, Window window);
void WmClientMsg(Display *disp, Window win, const char *msg,
        unsigned long data0, unsigned long data1,
        unsigned long data2, unsigned long data3,
        unsigned long data4);

#endif /* NEDIT_MISC_H_INCLUDED */
