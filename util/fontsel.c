/*******************************************************************************
*                                                                              *
* fontsel.c -- Nirvana Font Selector                                           *
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
* June 2, 1993                                                                 *
*                                                                              *
* Written by Suresh Ravoor (assisted by Mark Edel)                             *
*                                                                              *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "fontsel.h"
#include "misc.h"
#include "nedit_malloc.h"
#include "DialogF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/MessageB.h>
#include <Xm/DialogS.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define MAX_ARGS                        20
#define MAX_NUM_FONTS                   32767
#define MAX_FONT_NAME_LEN               256
#define MAX_ENTRIES_IN_LIST             5000
#define MAX_DISPLAY_SIZE                150
#define DELIM                           '-'
#define NUM_COMPONENTS_FONT_NAME        14
#define TEMP_BUF_SIZE                   256
#define DISPLAY_HEIGHT                  90

enum listSpecifier { NONE, FONT, STYLE, SIZE };


/* local data structures and types */

typedef struct
{
        Widget          form;           /* widget id */
        Widget          okButton;       /* widget id */
        Widget          cancelButton;   /* widget id */
        Widget          fontList;       /* widget id */
        Widget          styleList;      /* widget id */
        Widget          sizeList;       /* widget id */
        Widget          fontNameField;  /* widget id */
        Widget          sizeToggle;     /* widget id */
        Widget          propFontToggle; /* widget id */
        Widget          dispField;      /* widget id */
        char            **fontData;     /* font name info  */
        int             numFonts;       /* number of fonts */
        char            *sel1;          /* selection from list 1 */
        char            *sel2;          /* selection from list 2 */
        char            *sel3;          /* selection from list 3 */
        int             showPropFonts;  /* toggle state - show prop fonts */
        int             showSizeInPixels;/* toggle state - size in pixels  */
        char            *fontName;      /* current font name */
        XFontStruct     *oldFont;       /* font data structure for dispSample */
        XmFontList      oldFontList;    /* font data structure for dispSample */
        int     exitFlag;               /* used for program exit control */
        int     destroyedFlag;          /* used to prevent double destruction */
        Pixel   sampleFG;               /* Colors for the sample field */
        Pixel   sampleBG;
}       xfselControlBlkType;


/* local function prototypes */

static void     getStringComponent(const char *inStr, int pos, char *outStr);
static void     setupScrollLists(int dontChange, xfselControlBlkType ctrlBlk);
static int      notPropFont(const char *font);
static int      styleMatch(xfselControlBlkType *ctrlBlk, const char *font);
static int      sizeMatch(xfselControlBlkType *ctrlBlk, const char *font);
static int      fontMatch(xfselControlBlkType *ctrlBlk, const char *font);
static void     addItemToList(char **buf, const char *item, int *count);
static void     getFontPart(const char *font, char *buff1);
static void     getStylePart(const char *font, char *buff1);
static void     getSizePart(const char *font, char *buff1, int inPixels);
static void     propFontToggleAction(Widget widget, 
                                     xfselControlBlkType *ctrlBlk, 
                                     XmToggleButtonCallbackStruct *call_data);
static void     sizeToggleAction(Widget widget,
                                 xfselControlBlkType *ctrlBlk, 
                                 XmToggleButtonCallbackStruct *call_data);
static void     fontAction(Widget widget, xfselControlBlkType *ctrlBlk, 
                                 XmListCallbackStruct *call_data);
static void     styleAction(Widget widget, xfselControlBlkType *ctrlBlk, 
                                 XmListCallbackStruct *call_data);
static void     sizeAction(Widget widget, xfselControlBlkType *ctrlBlk, 
                                 XmListCallbackStruct *call_data);
static void     choiceMade(xfselControlBlkType *ctrlBlk);
static void     dispSample(xfselControlBlkType *ctrlBlk);
static void     destroyCB(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmListCallbackStruct *call_data);
static void     cancelAction(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmListCallbackStruct *call_data);
static void     okAction(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmPushButtonCallbackStruct *call_data);
static void     startupFont(xfselControlBlkType *ctrlBlk, const char *font);
static void     setFocus(Widget w, xfselControlBlkType *ctrlBlk, XEvent *event, 
                                                Boolean *continueToDispatch);
static void     FindBigFont(xfselControlBlkType *ctrlBlk, char *bigFont);
static void     enableSample(xfselControlBlkType *ctrlBlk, Bool turn_on,
                             XmFontList *fontList);

/*******************************************************************************
*                                                                              *
*     FontSel ()                                                               *
*                                                                              *
*                                                                              *
*            Function to put up a modal font selection dialog box. The purpose *
*            of this routine is to allow the user to interactively view sample *
*            fonts and to choose a font for current use.                       *
*                                                                              *
*     Arguments:                                                               *
*                                                                              *
*            Widget     parent          - parent widget ID                     *
*                                                                              *
*            int        showPropFont    - ONLY_FIXED : shows only fixed fonts  *
*                                                      doesn't show prop font  *
*                                                      toggle button also.     *
*                                         PREF_FIXED : can select either fixed *
*                                                      or proportional fonts;  *
*                                                      but starting option is  *
*                                                      Fixed fonts.            *
*                                         PREF_PROP  : can select either fixed *
*                                                      or proportional fonts;  *
*                                                      but starting option is  *
*                                                      proportional fonts.     *
*                                                                              *
*           char *      currFont        - ASCII string that contains the name  *
*                                         of the currently selected font.      *
*                                                                              *
*           Pixel   sampleFG, sampleBG      - Foreground/Background colors in  *
*                                               which to display the sample    *
*                                               text.                          *
*                                                                              *
*                                                                              *
*     Returns:                                                                 *
*                                                                              *
*           pointer to an ASCII character string that contains the name of     *
*           the selected font (in X format for naming fonts); it is the users  *
*           responsibility to free the space allocated to this string.         *
*                                                                              *
*     Comments:                                                                *
*                                                                              *
*           The calling function has to call the appropriate routines to set   *
*           the current font to the one represented by the returned string.    *
*                                                                              *
*******************************************************************************/

char    *FontSel(Widget parent, int showPropFonts, const char *currFont,
            Pixel sampleFG, Pixel sampleBG)
{
    Widget          dialog, form, okButton, cancelButton;
    Widget          styleList, sizeList, fontName, fontList;
    Widget          sizeToggle, propFontToggle = NULL, dispField;
    Widget          nameLabel, sampleLabel;
    Arg             args[MAX_ARGS];
    int             n;
    XmString        tempStr;
    char            bigFont[MAX_FONT_NAME_LEN];
    xfselControlBlkType ctrlBlk;
    Display         *theDisplay;

    ctrlBlk.fontData    = XListFonts(XtDisplay(parent), 
                                     "-*-*-*-*-*-*-*-*-*-*-*-*-*-*", 
                                     MAX_NUM_FONTS, &ctrlBlk.numFonts);
    FindBigFont(&ctrlBlk, bigFont);
    ctrlBlk.oldFont = XLoadQueryFont(XtDisplay(parent), bigFont);
    ctrlBlk.oldFontList = XmFontListCreate(ctrlBlk.oldFont,
                                            XmSTRING_DEFAULT_CHARSET);
    ctrlBlk.sampleFG = sampleFG;
    ctrlBlk.sampleBG = sampleBG;

    dialog  = CreateDialogShell(parent, "Font Selector", args, 0);

    /*  Set up window sizes for form widget */

    n = 0;
    XtSetArg(args[n], XmNautoUnmanage, FALSE); n++;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;

    /*  Create form popup dialog widget */

    form    = XtCreateWidget ("Font Selector", xmFormWidgetClass, dialog, 
                              args, n);

    /*  Create pushbutton widgets */

    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNbottomOffset, 4); n++;
    XtSetArg(args[n], XmNtopOffset, 1); n++;
    XtSetArg(args[n], XmNrightPosition, 45); n++;
    XtSetArg(args[n], XmNwidth, 110); n++;
    XtSetArg(args[n], XmNheight, 28); n++;
    XtSetArg(args[n], XmNshowAsDefault, TRUE); n++;
    okButton = XtCreateManagedWidget("OK", xmPushButtonWidgetClass, form,
                                      args, n);

    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, okButton); n++;
    XtSetArg(args[n], XmNbottomWidget, okButton); n++;
    XtSetArg(args[n], XmNleftPosition, 55); n++;
    XtSetArg(args[n], XmNwidth, 110); n++;
    XtSetArg(args[n], XmNheight, 28); n++;
    cancelButton = XtCreateManagedWidget("Cancel", xmPushButtonWidgetClass,
                                         form, args, n);

    /*  create font name text widget and the corresponding label */

    n = 0; 
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++; 
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++; 
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++; 
    XtSetArg(args[n], XmNbottomWidget, okButton); n++; 
    XtSetArg(args[n], XmNleftPosition, 1); n++; 
    XtSetArg(args[n], XmNrightPosition, 99); n++; 
    XtSetArg(args[n], XmNeditable, True); n++;
    XtSetArg(args[n], XmNeditMode, XmSINGLE_LINE_EDIT); n++;
    XtSetArg(args[n], XmNmaxLength, MAX_FONT_NAME_LEN); n++;
    fontName = XtCreateManagedWidget("fontname", xmTextWidgetClass, form,
                                     args, n);
    RemapDeleteKey(fontName);   /* kludge to handle delete and BS */

    n = 0; 
    tempStr = XmStringCreate("Font Name:", XmSTRING_DEFAULT_CHARSET);
    XtSetArg(args[n], XmNlabelString, tempStr); n++;
    XtSetArg(args[n], XmNmnemonic, 'N'); n++;
    XtSetArg(args[n], XmNuserData, fontName); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++; 
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++; 
    XtSetArg(args[n], XmNleftWidget, fontName); n++; 
    XtSetArg(args[n], XmNbottomWidget, fontName); n++;
    XtSetArg(args[n], XmNtopOffset, 1); n++;
    nameLabel = XtCreateManagedWidget("Font Name:", xmLabelWidgetClass,
                                      form, args, n);
    XmStringFree(tempStr);

    /*  create sample display text field widget */

    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++; 
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++; 
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++; 
    XtSetArg(args[n], XmNrightPosition, 99); n++;
    XtSetArg(args[n], XmNbottomWidget, nameLabel); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++; 
    XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
    XtSetArg(args[n], XmNvalue, 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789");
            n++;
    XtSetArg(args[n], XmNforeground, sampleFG); n++;
    XtSetArg(args[n], XmNbackground, sampleBG); n++;
    dispField = XtCreateManagedWidget(" ", xmTextFieldWidgetClass, form, 
                                       args, n);

    n = 0; 
    tempStr = XmStringCreate("Sample:", XmSTRING_DEFAULT_CHARSET);
    XtSetArg(args[n], XmNlabelString, tempStr); n++;
    XtSetArg(args[n], XmNmnemonic, 'S'); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++; 
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++; 
    XtSetArg(args[n], XmNleftWidget, dispField); n++; 
    XtSetArg(args[n], XmNbottomWidget, dispField); n++;
    XtSetArg(args[n], XmNtopOffset, 1); n++;
    sampleLabel = XtCreateManagedWidget("Font Name:", xmLabelWidgetClass,
            form, args, n);
    XmStringFree(tempStr);

    /*  create toggle buttons */

    n = 0; 
    tempStr = XmStringCreate("Show Size in Points", 
                              XmSTRING_DEFAULT_CHARSET);
    XtSetArg(args[n], XmNlabelString, tempStr); n++;
    XtSetArg(args[n], XmNmnemonic, 'P'); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++; 
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++; 
    XtSetArg(args[n], XmNleftPosition, 2); n++; 
    XtSetArg(args[n], XmNtopOffset, 1); n++;
    XtSetArg(args[n], XmNbottomWidget, sampleLabel); n++;
    sizeToggle = XtCreateManagedWidget("sizetoggle", 
                      xmToggleButtonWidgetClass, form, args, n);
    XmStringFree(tempStr);

    if (showPropFonts != ONLY_FIXED)
    {
        n = 0; 
        tempStr = XmStringCreate("Show Proportional Width Fonts", 
                                 XmSTRING_DEFAULT_CHARSET);
        XtSetArg(args[n], XmNlabelString, tempStr); n++;
        XtSetArg(args[n], XmNmnemonic, 'W'); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++; 
        XtSetArg(args[n], XmNtopAttachment, 
                        XmATTACH_OPPOSITE_WIDGET); n++; 
        XtSetArg(args[n], XmNbottomAttachment, 
                        XmATTACH_OPPOSITE_WIDGET); n++; 
        XtSetArg(args[n], XmNrightPosition, 98); n++; 
        XtSetArg(args[n], XmNtopWidget, sizeToggle); n++;
        XtSetArg(args[n], XmNbottomWidget, sizeToggle); n++;
        XtSetArg(args[n], XmNtopOffset, 1); n++;
        propFontToggle = XtCreateManagedWidget("propfonttoggle", 
                            xmToggleButtonWidgetClass, form, args, n);
        XmStringFree(tempStr);
    }

    /*  create scroll list widgets */
    /*  "Font" list */

    n = 0;
    tempStr = XmStringCreate("Font:", XmSTRING_DEFAULT_CHARSET);
    XtSetArg(args[n], XmNlabelString, tempStr); n++;
    XtSetArg(args[n], XmNmnemonic, 'F'); n++; 
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++;
    nameLabel = XtCreateManagedWidget("Font:", xmLabelWidgetClass, form, 
                                      args, n);
    XmStringFree(tempStr);

    n = 0;
    XtSetArg(args[n], XmNvisibleItemCount, 15); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNbottomWidget, sizeToggle); n++;
    XtSetArg(args[n], XmNtopWidget, nameLabel); n++;
    XtSetArg(args[n], XmNleftWidget, nameLabel); n++;
    XtSetArg(args[n], XmNrightPosition, 52); n++;
    fontList = XmCreateScrolledList(form, "fontlist", args, n);
    AddMouseWheelSupport(fontList);
    XtManageChild(fontList);
    XtVaSetValues(nameLabel, XmNuserData, fontList, NULL);

    /* "Style" list */

    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNtopWidget, nameLabel); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftWidget, XtParent(fontList)); n++;
    XtSetArg(args[n], XmNbottomWidget, XtParent(fontList)); n++;
    XtSetArg(args[n], XmNrightPosition, 85); n++;
    styleList = XmCreateScrolledList(form, "stylelist", args, n);
    AddMouseWheelSupport(styleList);
    XtManageChild(styleList);

    n = 0;
    tempStr = XmStringCreate("Style:", XmSTRING_DEFAULT_CHARSET);
    XtSetArg(args[n], XmNmnemonic, 'y'); n++; 
    XtSetArg(args[n], XmNuserData, styleList); n++;
    XtSetArg(args[n], XmNlabelString, tempStr); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, XtParent(styleList)); n++;
    XtSetArg(args[n], XmNleftWidget, XtParent(styleList)); n++;
    XtCreateManagedWidget("Style:", xmLabelWidgetClass, form, args, n);
    XmStringFree(tempStr);

    /*  "Size" list */

    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, nameLabel); n++;
    XtSetArg(args[n], XmNleftWidget, XtParent(styleList)); n++;
    XtSetArg(args[n], XmNbottomWidget, XtParent(fontList)); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNrightPosition, 99); n++;
    sizeList = XmCreateScrolledList(form, "sizelist", args, n);
    AddMouseWheelSupport(sizeList);
    XtManageChild(sizeList);

    n = 0;
    tempStr = XmStringCreate("Size:", XmSTRING_DEFAULT_CHARSET);
    XtSetArg(args[n], XmNlabelString, tempStr); n++;
    XtSetArg(args[n], XmNmnemonic, 'z'); n++; 
    XtSetArg(args[n], XmNuserData, sizeList); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, XtParent(sizeList)); n++;
    XtSetArg(args[n], XmNleftWidget, XtParent(sizeList)); n++;
    XtCreateManagedWidget("Size:", xmLabelWidgetClass, form, args, n);
    XmStringFree(tempStr);

    /*  update form widgets cancel button */

    n = 0;
    XtSetArg(args[n], XmNcancelButton, cancelButton); n++;
    XtSetValues(form, args, n);


    /*  update application's control block structure */

    ctrlBlk.form            = form;
    ctrlBlk.okButton        = okButton;
    ctrlBlk.cancelButton    = cancelButton;
    ctrlBlk.fontList        = fontList;
    ctrlBlk.styleList       = styleList;
    ctrlBlk.sizeList        = sizeList;
    ctrlBlk.fontNameField   = fontName;
    ctrlBlk.sizeToggle      = sizeToggle;
    if (showPropFonts != ONLY_FIXED)
        ctrlBlk.propFontToggle  = propFontToggle;
    ctrlBlk.dispField       = dispField;
    ctrlBlk.exitFlag        = FALSE;
    ctrlBlk.destroyedFlag   = FALSE;
    ctrlBlk.showPropFonts   = showPropFonts;
    ctrlBlk.showSizeInPixels = TRUE;
    ctrlBlk.sel1            = NULL;
    ctrlBlk.sel2            = NULL;
    ctrlBlk.sel3            = NULL;
    ctrlBlk.fontName        = NULL;

    setupScrollLists(NONE, ctrlBlk);    /* update scroll lists */ 

    if (showPropFonts == PREF_PROP)
        XmToggleButtonSetState(propFontToggle, TRUE, FALSE); 
        
    /*  Register callback functions */

    if (showPropFonts != ONLY_FIXED)
        XtAddCallback(propFontToggle, XmNvalueChangedCallback, 
                (XtCallbackProc)propFontToggleAction, (char *)&ctrlBlk);
    XtAddCallback(sizeToggle, XmNvalueChangedCallback,
            (XtCallbackProc)sizeToggleAction, (char *)&ctrlBlk);
    XtAddCallback(fontList, XmNbrowseSelectionCallback,
            (XtCallbackProc)fontAction, (char *)&ctrlBlk);
    XtAddCallback(styleList, XmNbrowseSelectionCallback,
            (XtCallbackProc)styleAction, (char *)&ctrlBlk);
    XtAddCallback(sizeList, XmNbrowseSelectionCallback,
            (XtCallbackProc)sizeAction, (char *)&ctrlBlk);
    XtAddCallback(okButton, XmNactivateCallback,
            (XtCallbackProc)okAction, (char *)&ctrlBlk);
    XtAddCallback(cancelButton, XmNactivateCallback,
            (XtCallbackProc)cancelAction, (char *)&ctrlBlk);

    /* add event handler to setup input focus at start to scroll list */

    XtAddEventHandler(fontList, FocusChangeMask, FALSE,
            (XtEventHandler)setFocus, (char *)&ctrlBlk);
    XmProcessTraversal(fontList, XmTRAVERSE_CURRENT);

    /*  setup tabgroups */

    XmAddTabGroup(fontList);
    XmAddTabGroup(styleList);
    XmAddTabGroup(sizeList);
    XmAddTabGroup(sizeToggle);
    if (showPropFonts != ONLY_FIXED)
        XmAddTabGroup(propFontToggle);
    XmAddTabGroup(fontName);
    XmAddTabGroup(okButton);
    XmAddTabGroup(cancelButton);

    /* Make sure that we don't try to access the dialog if the user
       destroyed it (possibly indirectly, by destroying the parent). */
    XtAddCallback(dialog, XmNdestroyCallback,
            (XtCallbackProc)destroyCB, (char *)&ctrlBlk);
        
    /*  Link Motif Close option to cancel action */

    AddMotifCloseCallback(dialog, (XtCallbackProc)cancelAction, &ctrlBlk);

    /*  Handle dialog mnemonics  */

    AddDialogMnemonicHandler(form, FALSE);

    /*  Realize Widgets  */

    ManageDialogCenteredOnPointer(form);

    /* set up current font parameters */

    if (currFont[0] != '\0')
        startupFont(&ctrlBlk, currFont); 

    /* Make sure that we can still access the display in case the form
       gets destroyed */
    theDisplay = XtDisplay(form);

    /*  enter event loop */

    while (! ctrlBlk.exitFlag && ! ctrlBlk.destroyedFlag)
        XtAppProcessEvent(XtWidgetToApplicationContext(form), XtIMAll);

    if (! ctrlBlk.destroyedFlag) {
        /* Don't let the callback destroy the font name */
        XtRemoveCallback(dialog, XmNdestroyCallback,
                (XtCallbackProc)destroyCB, (char *)&ctrlBlk);
        XtDestroyWidget(dialog);
    }

    if (ctrlBlk.oldFont != NULL)
    {
        XFreeFont(theDisplay, ctrlBlk.oldFont);
        XmFontListFree(ctrlBlk.oldFontList);
    }

    return(ctrlBlk.fontName);
}


/*  gets a specific substring from a string */

static void getStringComponent(const char *inStr, int pos, char *outStr)
{
    int i, j;

    *outStr = '\0';

    if (pos > NUM_COMPONENTS_FONT_NAME)
    {
        fprintf(stderr, "Warning: getStringComponent being used for ");
        fprintf(stderr, "pos > %d\nIf such ", NUM_COMPONENTS_FONT_NAME);
        fprintf(stderr, "use is intended remove these warning lines\n");
    }
    
    for (i = 0; (pos > 0) && (inStr[i] != '\0'); i++)
        if (inStr[i] == DELIM)
            pos--;
    
    if (inStr[i] == '\0')
        return;

    for (j = 0; (inStr[i] != DELIM) && (inStr[i] != '\0'); i++, j++)
        outStr[j] = inStr[i];
    outStr[j] = '\0';
}


/* parse through the fontlist data and set up the three scroll lists */

static void setupScrollLists(int dontChange, xfselControlBlkType ctrlBlk)
{
    char        *itemBuf1[MAX_ENTRIES_IN_LIST];
    char        *itemBuf2[MAX_ENTRIES_IN_LIST];
    char        *itemBuf3[MAX_ENTRIES_IN_LIST];
    int         itemCount1, itemCount2, itemCount3;
    char        buff1[TEMP_BUF_SIZE];
    XmString    items[MAX_ENTRIES_IN_LIST];
    int         i;

    itemCount1 = 0;
    itemCount2 = 0;
    itemCount3 = 0;

    for (i = 0; i < ctrlBlk.numFonts && i < MAX_ENTRIES_IN_LIST; i++)
    {
        if ((dontChange != FONT) &&
            (styleMatch(&ctrlBlk, ctrlBlk.fontData[i])) &&
            (sizeMatch (&ctrlBlk, ctrlBlk.fontData[i])) &&
            ((ctrlBlk.showPropFonts == PREF_PROP) || 
             (notPropFont(ctrlBlk.fontData[i]))))
        {
            getFontPart(ctrlBlk.fontData[i], buff1);
            addItemToList(itemBuf1, buff1, &itemCount1);
        }

        if ((dontChange != STYLE) &&
            (fontMatch(&ctrlBlk, ctrlBlk.fontData[i])) &&
            (sizeMatch (&ctrlBlk, ctrlBlk.fontData[i])) &&
            ((ctrlBlk.showPropFonts == PREF_PROP) || 
             (notPropFont(ctrlBlk.fontData[i]))))
        {
            getStylePart(ctrlBlk.fontData[i], buff1);
            addItemToList(itemBuf2, buff1, &itemCount2);
        }

        if ((dontChange != SIZE) &&
            (fontMatch(&ctrlBlk, ctrlBlk.fontData[i])) &&
            (styleMatch (&ctrlBlk, ctrlBlk.fontData[i])) &&
            ((ctrlBlk.showPropFonts == PREF_PROP) || 
             (notPropFont(ctrlBlk.fontData[i]))))
        {
            getSizePart(ctrlBlk.fontData[i], buff1, ctrlBlk.showSizeInPixels);
            addItemToList(itemBuf3, buff1, &itemCount3);
        }
    }   /* end - for (i = 0; i < ctrlBlk.numFonts; i++) */

    /*  recreate all three scroll lists where necessary */
    if (dontChange != FONT)
    {
        for (i = 0; i < itemCount1; i++)
        {
            items[i] = XmStringCreate(itemBuf1[i], XmSTRING_DEFAULT_CHARSET);
            NEditFree(itemBuf1[i]);
        }
        XmListDeleteAllItems(ctrlBlk.fontList);
        XmListAddItems(ctrlBlk.fontList, items, itemCount1, 1);
        if (ctrlBlk.sel1 != NULL)
        {
            XmStringFree(items[0]);
            items[0] = XmStringCreate(ctrlBlk.sel1, XmSTRING_DEFAULT_CHARSET);
            XmListSelectItem(ctrlBlk.fontList, items[0], FALSE);
            XmListSetBottomItem(ctrlBlk.fontList, items[0]);
        }
        for (i = 0; i < itemCount1; i++)
            XmStringFree(items[i]);
    }

    if (dontChange != STYLE)
    {
        for (i = 0; i < itemCount2; i++)
        {
            items[i] = XmStringCreate(itemBuf2[i], XmSTRING_DEFAULT_CHARSET);
            NEditFree(itemBuf2[i]);
        }
        XmListDeleteAllItems(ctrlBlk.styleList);
        XmListAddItems(ctrlBlk.styleList, items, itemCount2, 1);
        if (ctrlBlk.sel2 != NULL)
        {
            XmStringFree(items[0]);
            items[0] = XmStringCreate(ctrlBlk.sel2, XmSTRING_DEFAULT_CHARSET);
            XmListSelectItem(ctrlBlk.styleList, items[0], FALSE);
            XmListSetBottomItem(ctrlBlk.styleList, items[0]);
        }
        for (i = 0; i < itemCount2; i++)
            XmStringFree(items[i]);
    }

    if (dontChange != SIZE)
    {
        for (i = 0; i < itemCount3; i++)
        {
            items[i] = XmStringCreate(itemBuf3[i],
                              XmSTRING_DEFAULT_CHARSET);
            NEditFree(itemBuf3[i]);
        }
        XmListDeleteAllItems(ctrlBlk.sizeList);
        XmListAddItems(ctrlBlk.sizeList, items, itemCount3, 1);
        if (ctrlBlk.sel3 != NULL)
        {
            XmStringFree(items[0]);
            items[0] = XmStringCreate(ctrlBlk.sel3, XmSTRING_DEFAULT_CHARSET);
            XmListSelectItem(ctrlBlk.sizeList, items[0], FALSE);
            XmListSetBottomItem(ctrlBlk.sizeList, items[0]);
        }
        for (i = 0; i < itemCount3; i++)
            XmStringFree(items[i]);
    }
}


/*  returns TRUE if argument is not name of a proportional font */

static int  notPropFont(const char *font)
{
    char    buff1[TEMP_BUF_SIZE];

    getStringComponent(font, 11, buff1);
    if ((strcmp(buff1, "p") == 0) || (strcmp(buff1, "P") == 0))
        return(FALSE);
    else
        return(TRUE);
}


/*  returns TRUE if the style portion of the font matches the currently
    selected style */

static int  styleMatch(xfselControlBlkType *ctrlBlk, const char *font)
{
    char    buff[TEMP_BUF_SIZE];

    if (ctrlBlk->sel2 == NULL)
        return(TRUE);

    getStylePart(font, buff);

    if (strcmp(buff, ctrlBlk->sel2) == 0)
        return(TRUE);
    else
        return(FALSE);
}


/*  returns TRUE if the size portion of the font matches the currently
    selected size */

static int  sizeMatch(xfselControlBlkType *ctrlBlk, const char *font)
{
    char    buff[TEMP_BUF_SIZE];

    if (ctrlBlk->sel3 == NULL)
        return(TRUE);

    getSizePart(font, buff, ctrlBlk->showSizeInPixels);
    if (strcmp(buff, ctrlBlk->sel3) == 0)
        return(TRUE);
    else
        return(FALSE);
}


/*  returns TRUE if the font portion of the font matches the currently
    selected font */

static int  fontMatch(xfselControlBlkType *ctrlBlk, const char *font)
{
    char    buff[TEMP_BUF_SIZE];

    if (ctrlBlk->sel1 == NULL)
        return(TRUE);

    getFontPart(font, buff);
    if (strcmp(buff, ctrlBlk->sel1) == 0)
        return(TRUE);
    else
        return(FALSE);
}


/*  inserts a string into correct sorted position in a list */

static void addItemToList(char **buf, const char *item, int *count)
{
    int i, j;

    if (*count == MAX_ENTRIES_IN_LIST)
    {
        fprintf(stderr, "Trying to add more than MAX_ENTRIES_IN_LIST ");
        fprintf(stderr, "(%d) entries to array\n", MAX_ENTRIES_IN_LIST);
        return;
    }   

    for (i = 0; i < *count; i++)
    {
        if (strcmp(buf[i], item) == 0)
            return;
        if (strcmp(buf[i], item) > 0)
            break;
    }

    for (j = *count; j > i; j--)
        buf[j] = buf[j-1];
    buf[i] = NEditStrdup(item);
    (*count)++;
}


/*  given a font name this function returns the part used in the first 
    scroll list */

static void getFontPart(const char *font, char *buff1)
{
    char    buff2[TEMP_BUF_SIZE], buff3[TEMP_BUF_SIZE];
    char    buff4[TEMP_BUF_SIZE];

    getStringComponent(font, 2, buff1);
    getStringComponent(font, 1, buff2);

    sprintf(buff3, "%s (%s", buff1, buff2);

    getStringComponent(font, 13, buff1);
    getStringComponent(font, 14, buff4);

    if (((strncmp(buff1, "iso8859", 7) == 0) || 
         (strncmp(buff1, "ISO8859", 7) == 0)) && (strcmp(buff4, "1") == 0))
        sprintf(buff1, "%s)", buff3);
    else
    {
        sprintf(buff2, "%s, %s,", buff3, buff1);
        sprintf(buff1, "%s %s)", buff2, buff4);
    }
}


/*  given a font name this function returns the part used in the second 
    scroll list */

static void getStylePart(const char *font, char *buff1)
{
    char    buff2[TEMP_BUF_SIZE], buff3[TEMP_BUF_SIZE];

    getStringComponent(font, 3, buff3);
    getStringComponent(font, 5, buff2);

    if ((strcmp(buff2, "normal") != 0) && (strcmp(buff2, "Normal") != 0) &&
        (strcmp(buff2, "NORMAL") != 0))
        sprintf(buff1, "%s %s", buff3, buff2);
    else
        strcpy(buff1, buff3);

    getStringComponent(font, 6, buff2);

    if (buff2[0] != '\0')
        sprintf(buff3, "%s %s", buff1, buff2);
    else
        strcpy(buff3, buff1);

    getStringComponent(font, 4, buff2);

    if ((strcmp(buff2, "o") == 0) || (strcmp(buff2, "O") == 0))
        sprintf(buff1, "%s oblique", buff3);
    else if ((strcmp(buff2, "i") == 0) || (strcmp(buff2, "I") == 0))
        sprintf(buff1, "%s italic", buff3);

    if (strcmp(buff1, " ") == 0)
        strcpy(buff1, "-");
}


/*  given a font name this function returns the part used in the third 
    scroll list */

static void getSizePart(const char *font, char *buff1, int inPixels)
{
    int size;

    if (inPixels)
    {
        getStringComponent(font, 7, buff1);
        size = atoi(buff1);
        sprintf(buff1, "%2d", size);
    }
    else
    {
        double temp;

        getStringComponent(font, 8, buff1);
        size = atoi(buff1);
        temp = (double)size / 10.0;
        if (buff1[strlen(buff1) - 1] == '0')
        {
            size = (int)floor(temp+0.5);
            sprintf(buff1, "%2d", size);
        }
        else
            sprintf(buff1, "%4.1f", temp);
    }
}


/*  Call back functions start from here - suffix Action in the function name
    is for the callback function for the corresponding widget */

static void propFontToggleAction(Widget widget, 
                     xfselControlBlkType *ctrlBlk, 
                     XmToggleButtonCallbackStruct *call_data)
{
    if (call_data->reason == XmCR_VALUE_CHANGED)
    {
        if (ctrlBlk->showPropFonts == PREF_FIXED)
            ctrlBlk->showPropFonts = PREF_PROP;
        else
            ctrlBlk->showPropFonts = PREF_FIXED;

        NEditFree(ctrlBlk->sel1);
        ctrlBlk->sel1 = NULL;

        NEditFree(ctrlBlk->sel2);
        ctrlBlk->sel2 = NULL;

        NEditFree(ctrlBlk->sel3);
        ctrlBlk->sel3 = NULL;

        setupScrollLists(NONE, *ctrlBlk);

        XmTextSetString(ctrlBlk->fontNameField, "");
        enableSample(ctrlBlk, False, NULL);
    }
}


static void sizeToggleAction(Widget widget,
                 xfselControlBlkType *ctrlBlk, 
                 XmToggleButtonCallbackStruct *call_data)
{
    int         i, makeSelection;
    char        newSize[10];
    XmString    str;

    if (call_data->reason == XmCR_VALUE_CHANGED)
    {
        makeSelection = (ctrlBlk->sel3 != NULL);

        for (i = 0; (makeSelection) && (i < ctrlBlk->numFonts); i++)
            if ((fontMatch(ctrlBlk, ctrlBlk->fontData[i])) &&
                (styleMatch(ctrlBlk, ctrlBlk->fontData[i])) &&
                (sizeMatch(ctrlBlk, ctrlBlk->fontData[i])))
            {
                getSizePart(ctrlBlk->fontData[i], newSize, 
                            !ctrlBlk->showSizeInPixels);
                break;
            }
                
        if (ctrlBlk->showSizeInPixels)
            ctrlBlk->showSizeInPixels = FALSE;
        else
            ctrlBlk->showSizeInPixels = TRUE;

        NEditFree(ctrlBlk->sel3);

        ctrlBlk->sel3 = NULL;
        setupScrollLists(NONE, *ctrlBlk);

        if (makeSelection)
        {
            str = XmStringCreate(newSize, XmSTRING_DEFAULT_CHARSET);
            XmListSelectItem(ctrlBlk->sizeList, str, TRUE);
            XmListSetBottomItem(ctrlBlk->sizeList, str);
            XmStringFree(str);
        }
    }
}


static void enableSample(xfselControlBlkType *ctrlBlk, Bool turn_on, 
        XmFontList *fontList)
{
    int n=0;
    Arg args[4];

    XtSetArg(args[n], XmNeditable, turn_on); n++;
    XtSetArg(args[n], XmNcursorPositionVisible, turn_on); n++;
    if( turn_on ) {
        if( !fontList ) {
            fprintf(stderr, "nedit: Internal error in fontsel.c, line %i\n", \
                    __LINE__);
        } else {
            XtSetArg(args[n], XmNfontList, *fontList); n++;
        }
        XtSetArg(args[n], XmNforeground, ctrlBlk->sampleFG); n++;
    } else {
        XtSetArg(args[n], XmNforeground, ctrlBlk->sampleBG); n++;
    }
    XtSetValues(ctrlBlk->dispField, args, n);
    /* Make sure the sample area gets resized if the font size changes */
    XtUnmanageChild(ctrlBlk->dispField);
    XtManageChild(ctrlBlk->dispField);
}


static void fontAction(Widget widget, xfselControlBlkType *ctrlBlk, 
                 XmListCallbackStruct *call_data)
{
    char    *sel;

    XmStringGetLtoR(call_data->item, XmSTRING_DEFAULT_CHARSET, &sel);

    if (ctrlBlk->sel1 == NULL)
    {
        ctrlBlk->sel1 = NEditStrdup(sel);
    }
    else
    {
        if (strcmp(ctrlBlk->sel1, sel) == 0)
        {           /* Unselecting current selection */
            NEditFree(ctrlBlk->sel1);
            ctrlBlk->sel1 = NULL;
            XmListDeselectItem(widget, call_data->item);
        }
        else
        {
            NEditFree(ctrlBlk->sel1);
            ctrlBlk->sel1 = NEditStrdup(sel);
        }
    }

    NEditFree(sel);
    setupScrollLists(FONT, *ctrlBlk);
    if ((ctrlBlk->sel1 != NULL) && (ctrlBlk->sel2 != NULL) && 
        (ctrlBlk->sel3 != NULL))
        choiceMade(ctrlBlk);
    else
    {
        enableSample(ctrlBlk, False, NULL);
        XmTextSetString(ctrlBlk->fontNameField, "");
    }
}


static void styleAction(Widget widget, xfselControlBlkType *ctrlBlk, 
                 XmListCallbackStruct *call_data)
{
    char    *sel;

    XmStringGetLtoR(call_data->item, XmSTRING_DEFAULT_CHARSET, &sel);

    if (ctrlBlk->sel2 == NULL)
    {
        ctrlBlk->sel2 = NEditStrdup(sel);
    }
    else
    {
        if (strcmp(ctrlBlk->sel2, sel) == 0)
        {           /* unselecting current selection */
            NEditFree(ctrlBlk->sel2);
            ctrlBlk->sel2 = NULL;
            XmListDeselectItem(widget, call_data->item);
        }
        else
        {
            NEditFree(ctrlBlk->sel2);
            ctrlBlk->sel2 = NEditStrdup(sel);
        }
    }

    NEditFree(sel);
    setupScrollLists(STYLE, *ctrlBlk);
    if ((ctrlBlk->sel1 != NULL) && (ctrlBlk->sel2 != NULL) && 
        (ctrlBlk->sel3 != NULL))
        choiceMade(ctrlBlk);
    else
    {
        enableSample(ctrlBlk, False, NULL);
        XmTextSetString(ctrlBlk->fontNameField, "");
    }
}

static void sizeAction(Widget widget, xfselControlBlkType *ctrlBlk, 
                 XmListCallbackStruct *call_data)
{
    char    *sel;

    XmStringGetLtoR(call_data->item, XmSTRING_DEFAULT_CHARSET, &sel);

    if (ctrlBlk->sel3 == NULL)
    {
        ctrlBlk->sel3 = NEditStrdup(sel);
    }
    else
    {
        if (strcmp(ctrlBlk->sel3, sel) == 0)
        {           /* unselecting current selection */
            NEditFree(ctrlBlk->sel3);
            ctrlBlk->sel3 = NULL;
            XmListDeselectItem(widget, call_data->item);
        }
        else
        {
            NEditFree(ctrlBlk->sel3);
            ctrlBlk->sel3 = NEditStrdup(sel);
        }
    }

    NEditFree(sel);
    setupScrollLists(SIZE, *ctrlBlk);
    if ((ctrlBlk->sel1 != NULL) && (ctrlBlk->sel2 != NULL) && 
        (ctrlBlk->sel3 != NULL))
        choiceMade(ctrlBlk);
    else
    {
        enableSample(ctrlBlk, False, NULL);
        XmTextSetString(ctrlBlk->fontNameField, "");
    }
}

/*  function called when all three choices have been made; sets up font
    name and displays sample font */

static void choiceMade(xfselControlBlkType *ctrlBlk)
{
    int i;

    NEditFree(ctrlBlk->fontName);
    ctrlBlk->fontName = NULL;

    for (i = 0; i < ctrlBlk->numFonts; i++)
    {
        if ((fontMatch(ctrlBlk, ctrlBlk->fontData[i])) &&
            (styleMatch(ctrlBlk, ctrlBlk->fontData[i])) &&
            (sizeMatch (ctrlBlk, ctrlBlk->fontData[i])))
        {
            ctrlBlk->fontName = NEditStrdup(ctrlBlk->fontData[i]);
            break;
        }
    }

    if (ctrlBlk->fontName != NULL)
    {
        XmTextSetString(ctrlBlk->fontNameField, ctrlBlk->fontName);
        dispSample(ctrlBlk);
    }
    else
    {
        DialogF (DF_ERR, ctrlBlk->form, 1, "Font Specification",
                "Invalid Font Specification", "OK");
    }
}


/*  loads selected font and displays sample text in that font */

static void dispSample(xfselControlBlkType *ctrlBlk)
{
    XFontStruct     *font;
    XmFontList      fontList;
    Display         *display;

    display     = XtDisplay(ctrlBlk->form);
    font        = XLoadQueryFont(display, ctrlBlk->fontName);
    fontList    = XmFontListCreate(font, XmSTRING_DEFAULT_CHARSET);

    enableSample(ctrlBlk, True, &fontList);

    if (ctrlBlk->oldFont != NULL)
    {
        XFreeFont(display, ctrlBlk->oldFont);
        XmFontListFree(ctrlBlk->oldFontList);
    }
    ctrlBlk->oldFont    = font;
    ctrlBlk->oldFontList    = fontList;
}


static void destroyCB(Widget widget, xfselControlBlkType *ctrlBlk,
                 XmListCallbackStruct *call_data)
{
    /* Prevent double destruction of the font selection dialog */
    ctrlBlk->destroyedFlag = TRUE;
    cancelAction(widget, ctrlBlk, call_data);
}

static void cancelAction(Widget widget, xfselControlBlkType *ctrlBlk,
                 XmListCallbackStruct *call_data)
{
    NEditFree(ctrlBlk->sel1);
    NEditFree(ctrlBlk->sel2);
    NEditFree(ctrlBlk->sel3);
    NEditFree(ctrlBlk->fontName);

    ctrlBlk->fontName = NULL;
    XFreeFontNames(ctrlBlk->fontData);

    ctrlBlk->exitFlag = TRUE;
}


static void okAction(Widget widget, xfselControlBlkType *ctrlBlk,
                 XmPushButtonCallbackStruct *call_data)
{
    char    *fontPattern;
    char    **fontName;
    int i;

    fontPattern = XmTextGetString(ctrlBlk->fontNameField);
    fontName    = XListFonts(XtDisplay(ctrlBlk->form), fontPattern, 1, &i);
    NEditFree(fontPattern);
    
    if ((fontName == NULL) || (i == 0))
    {
        DialogF (DF_ERR, ctrlBlk->okButton, 1, "Font Specification",
                "Invalid Font Specification", "OK");
        XFreeFontNames(fontName);
    }
    else
    {
        NEditFree(ctrlBlk->fontName);
        ctrlBlk->fontName = NEditStrdup(fontName[0]);

        NEditFree(ctrlBlk->sel1);
        NEditFree(ctrlBlk->sel2);
        NEditFree(ctrlBlk->sel3);
    
        XFreeFontNames(fontName);
        XFreeFontNames(ctrlBlk->fontData);

        ctrlBlk->exitFlag = TRUE;
    }
}


/*  if current font is passed as an argument then this function is
    invoked and sets up initial entries */

static void startupFont(xfselControlBlkType *ctrlBlk, const char *font)
{
    int         i;
    char        **fontName;
    char        part[TEMP_BUF_SIZE];
    XmString    str;

    fontName = XListFonts(XtDisplay(ctrlBlk->form), font, 1, &i);

    if ((fontName == NULL) || (i == 0))
    {           /*  invalid font passed in at startup */
        XFreeFontNames(fontName);
        return;
    }

    ctrlBlk->fontName = NEditStrdup(fontName[0]);

    getFontPart(fontName[0], part);
    XFreeFontNames(fontName);
    str = XmStringCreate(part, XmSTRING_DEFAULT_CHARSET);
    XmListSetBottomItem(ctrlBlk->fontList, str);
    XmListSelectItem(ctrlBlk->fontList, str, TRUE);
    XmListSelectItem(ctrlBlk->fontList, str, TRUE);
    XmStringFree(str);

    dispSample(ctrlBlk);
    XmTextSetString(ctrlBlk->fontNameField, ctrlBlk->fontName);
}


/*  hacked code to move initial input focus to first scroll list and at the 
    same time have the OK button as the default button */

static void     setFocus(Widget w, xfselControlBlkType *ctrlBlk, XEvent *event, 
                        Boolean *continueToDispatch)
{
    int n;
    Arg args[2];

    *continueToDispatch = TRUE;

    n = 0;
    XtSetArg(args[n], XmNdefaultButton, ctrlBlk->okButton); n++;
    XtSetValues(ctrlBlk->form, args, n);
}


/*  finds the name of the biggest font less than the given limit 
    MAX_DISPLAY_SIZE used to set up the initial height of the display widget
*/

static void FindBigFont(xfselControlBlkType *ctrlBlk, char *bigFont)
{
    int     i, maxSize, ind = -1, size;
    char    sizeStr[10];

    for (i = 0, maxSize = 0; i < ctrlBlk->numFonts; i++)
    {
        getStringComponent(ctrlBlk->fontData[i], 7, sizeStr);
        size = atoi(sizeStr);
        if ((size > maxSize) && (size < MAX_DISPLAY_SIZE))
        {
            ind   = i;
            maxSize = size;
        }
    }
    if (ind >= 0) {
        strcpy(bigFont, ctrlBlk->fontData[ind]);
    }
    else {
        bigFont[0] = 0;
    }
}
