/*******************************************************************************
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
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *                                                                         *
*                                                                              *
*******************************************************************************/

#include "filter.h"
#include "window.h"
#include "nedit.h"

#include "../util/misc.h"
#include "../util/managedList.h"

#include <Xm/XmAll.h>

static IOFilter *filters;
static size_t allocFilters;
static size_t numFilters;


typedef struct {
    Widget shell;
    WindowInfo *window;
    Widget managedListW;
    Widget nameW;
    Widget cmdInW;
    Widget cmdOutW;

    IOFilter **filters;
    int nfilters;
} filterDialog;

static filterDialog fd  = { NULL, NULL, NULL, NULL, NULL, NULL};


static void filterOkCB(Widget w, XtPointer clientData, XtPointer callData)
{

}

static void filterApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{

}

static void filterCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* pop down and destroy the dialog */
    XtDestroyWidget(fd.shell);
    fd.shell = NULL;
}

static void *fdGetDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg)
{
    return NULL;
}

static void fdSetDisplayedCB(void *item, void *cbArg)
{

}

static void fdFreeItemCB(void *item)
{

}

#define FD_LIST_RIGHT 30
#define FD_LEFT_MARGIN_POS 1
#define FD_RIGHT_MARGIN_POS 99
#define FD_H_MARGIN 10

void FilterSettings(WindowInfo *window)
{
    XmString s1;

    if(fd.shell) {
        RaiseDialogWindow(fd.shell);
        return;
    }


    int ac = 0;
    Arg args[20];
    //XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
    XtSetArg(args[ac], XmNtitle, "Filters"); ac++;
    fd.shell = CreateWidget(TheAppShell, "filters",
	    topLevelShellWidgetClass, args, ac);
    AddSmallIcon(fd.shell);
    Widget form = XtVaCreateManagedWidget("filtersForm", xmFormWidgetClass,
	    fd.shell, XmNautoUnmanage, False,
	    XmNresizePolicy, XmRESIZE_NONE, NULL);

    Widget topLbl = XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form,
            XmNlabelString, s1=MKSTRING(
"To modify the properties of an existing filter, select the name\n\
from the list on the left.  Select \"New\" to add a new filter to the list."),
        XmNmnemonic, 'N',
        XmNtopAttachment, XmATTACH_POSITION,
        XmNtopPosition, 2,
        XmNleftAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 6,
        NULL);
    XmStringFree(s1);




    Widget okBtn = XtVaCreateManagedWidget("ok",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 10,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 30,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(okBtn, XmNactivateCallback, filterOkCB, NULL);
    XmStringFree(s1);

    Widget applyBtn = XtVaCreateManagedWidget("apply",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("Apply"),
            XmNmnemonic, 'A',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 40,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 60,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, filterApplyCB, NULL);
    XmStringFree(s1);

    Widget closeBtn = XtVaCreateManagedWidget("close",
            xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Close"),
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 70,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 90,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 6,
            NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, filterCloseCB, NULL);
    XmStringFree(s1);

    Widget sep1 = XtVaCreateManagedWidget("sep1", xmSeparatorGadgetClass, form,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, okBtn,
        XmNbottomOffset, 4,
        NULL);

    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopOffset, FD_H_MARGIN); ac++;
    XtSetArg(args[ac], XmNtopWidget, topLbl); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNleftPosition, FD_LEFT_MARGIN_POS); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNrightPosition, FD_LIST_RIGHT-1); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNbottomWidget, sep1); ac++;
    XtSetArg(args[ac], XmNbottomOffset, FD_H_MARGIN); ac++;
    fd.managedListW = CreateManagedList(form, "list", args, ac,
            (void **)fd.filters, &fd.nfilters,
            256, 20, fdGetDisplayedCB, NULL, fdSetDisplayedCB,
            form, fdFreeItemCB);

    s1 = XmStringCreateLocalized("Input Filter Command");
    Widget lblCmdIn = XtVaCreateManagedWidget("sep1", xmLabelGadgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, topLbl,
        XmNtopOffset, FD_H_MARGIN,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        XmNlabelString, s1,
        XmNalignment, XmALIGNMENT_BEGINNING,
        NULL);
    XmStringFree(s1);

    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, lblCmdIn); ac++;
    XtSetArg(args[ac], XmNtopOffset, 6); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNleftWidget, fd.managedListW); ac++;
    XtSetArg(args[ac], XmNleftOffset, 6); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(args[ac], XmNrightOffset, 6); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNbottomPosition, 47); ac++;
    XtSetArg(args[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
    XtSetArg(args[ac], XmNrows, 4); ac++;
    fd.cmdInW = XmCreateScrolledText(form, "cmdInText", args, ac);
    XtManageChild(fd.cmdInW);

    s1 = XmStringCreateLocalized("Output Filter Command");
    Widget lblCmdOut = XtVaCreateManagedWidget("sep1", xmLabelGadgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, fd.cmdInW,
        XmNtopOffset, FD_H_MARGIN,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, fd.managedListW,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 6,
        XmNrightOffset, 6,
        XmNlabelString, s1,
        XmNalignment, XmALIGNMENT_BEGINNING,
        NULL);
    XmStringFree(s1);

    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopWidget, lblCmdOut); ac++;
    XtSetArg(args[ac], XmNtopOffset, 6); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNleftWidget, fd.managedListW); ac++;
    XtSetArg(args[ac], XmNleftOffset, 6); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(args[ac], XmNrightOffset, 6); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNbottomWidget, sep1); ac++;
    XtSetArg(args[ac], XmNbottomOffset, FD_H_MARGIN); ac++;
    XtSetArg(args[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
    XtSetArg(args[ac], XmNrows, 4); ac++;
    fd.cmdOutW = XmCreateScrolledText(form, "cmdInText", args, ac);
    XtManageChild(fd.cmdOutW);

    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);

    //XtAddCallback(form, XmNdestroyCallback, hsDestroyCB, NULL);
    //AddMotifCloseCallback(fd.shell, hsCloseCB, NULL);

    RealizeWithoutForcingPosition(fd.shell);
}
