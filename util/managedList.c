/*******************************************************************************
*									       *
* managedList.c -- User interface for reorderable list of records	       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
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
* November, 1995							       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "managedList.h"
#include "misc.h"
#include "DialogF.h"
#include "nedit_malloc.h"

#include <stdio.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* Common data between the managed list callback functions */
typedef struct {
    Widget listW, deleteBtn, copyBtn, moveUpBtn, moveDownBtn;
    void *(*getDialogDataCB)(void *, int, int *, void *);
    void *getDialogDataArg;
    void (*setDialogDataCB)(void *, void *);
    void *setDialogDataArg;
    void *(*copyItemCB)(void *);
    void (*freeItemCB)(void *);
    int (*deleteConfirmCB)(int, void *);
    void *deleteConfirmArg;
    int maxItems;
    int *nItems;
    void **itemList;
    int lastSelection;
} managedListData;

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData);
static void copyCB(Widget w, XtPointer clientData, XtPointer callData);
static void moveUpCB(Widget w, XtPointer clientData, XtPointer callData);
static void moveDownCB(Widget w, XtPointer clientData, XtPointer callData);
static int incorporateDialogData(managedListData *ml, int listPos,
    	int explicit);
static void updateDialogFromList(managedListData *ml, int selection);
static void updateListWidgetItem(managedListData *ml, int listPos);
static void listSelectionCB(Widget w, XtPointer clientData, XtPointer callData);
static int selectedListPosition(managedListData *ml);
static void selectItem(Widget listW, int itemIndex, int updateDialog);
static Widget shellOfWidget(Widget w);

/*
** Create a user interface to help manage a list of arbitrary data records
** which users can edit, add, delete, and reorder.
**
** The caller creates the overall dialog for presenting the data to the user,
** but embeds the list and button widgets created here (and list management
** code they activate) to handle the organization of the overall list.
**
** This routine creates a form widget containing the buttons and list widget
** with which the user interacts with the list data.  ManageListAndButtons
** can be used alternatively to take advantage of the management code with a
** different arrangement of the widgets (this routine puts buttons in a
** column on the left, list on the right) imposed here.
**
** "args" and "argc" are passed to the form widget creation routine, so that
** attachments can be specified for embedding the form in a dialog.
**
** See ManageListAndButtons for a description of the remaining arguments.
*/
Widget CreateManagedList(Widget parent, char *name, Arg *args,
    	int argC, void **itemList, int *nItems, int maxItems, int nColumns,
    	void *(*getDialogDataCB)(void *, int, int *, void *),
    	void *getDialogDataArg, void (*setDialogDataCB)(void *, void *),
    	void *setDialogDataArg, void (*freeItemCB)(void *))
{
    int ac;
    Arg al[20];
    XmString s1;
    Widget form, rowCol, listW;
    Widget deleteBtn, copyBtn, moveUpBtn, moveDownBtn;
    XmString *placeholderTable;
    char *placeholderStr;

    form = XmCreateForm(parent, name, args, argC);
    XtManageChild(form);
    rowCol = XtVaCreateManagedWidget("mlRowCol", xmRowColumnWidgetClass, form,
    	    XmNpacking, XmPACK_COLUMN,
    	    XmNleftAttachment, XmATTACH_FORM,
    	    XmNtopAttachment, XmATTACH_FORM,
    	    XmNbottomAttachment, XmATTACH_FORM, NULL);
    deleteBtn = XtVaCreateManagedWidget("delete", xmPushButtonWidgetClass,
    	    rowCol, XmNlabelString, s1=XmStringCreateSimple("Delete"), NULL);
    XmStringFree(s1);
    copyBtn = XtVaCreateManagedWidget("copy", xmPushButtonWidgetClass, rowCol,
    	    XmNlabelString, s1=XmStringCreateSimple("Copy"), NULL);
    XmStringFree(s1);
    moveUpBtn = XtVaCreateManagedWidget("moveUp", xmPushButtonWidgetClass,
    	    rowCol, XmNlabelString, s1=XmStringCreateSimple("Move ^"), NULL);
    XmStringFree(s1);
    moveDownBtn = XtVaCreateManagedWidget("moveDown", xmPushButtonWidgetClass,
    	    rowCol, XmNlabelString, s1=XmStringCreateSimple("Move v"), NULL);
    XmStringFree(s1);
    
    /* AFAIK the only way to make a list widget n-columns wide is to make up
       a fake initial string of that width, and create it with that */
    placeholderStr = (char*)NEditMalloc(nColumns+1);
    memset(placeholderStr, 'm', nColumns);
    placeholderStr[nColumns] = '\0';
    placeholderTable = StringTable(1, placeholderStr);
    NEditFree(placeholderStr);
 
    ac = 0;
    XtSetArg(al[ac], XmNscrollBarDisplayPolicy, XmAS_NEEDED); ac++;
    XtSetArg(al[ac], XmNlistSizePolicy, XmCONSTANT); ac++;
    XtSetArg(al[ac], XmNitems, placeholderTable); ac++;
    XtSetArg(al[ac], XmNitemCount, 1); ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, rowCol); ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    listW = XmCreateScrolledList(form, "list", al, ac);
    AddMouseWheelSupport(listW);
    XtManageChild(listW);
    FreeStringTable(placeholderTable);

    return ManageListAndButtons(listW, deleteBtn, copyBtn, moveUpBtn,
    	    moveDownBtn, itemList, nItems, maxItems, getDialogDataCB,
    	    getDialogDataArg, setDialogDataCB, setDialogDataArg, freeItemCB);
}

/*
** Manage a list widget and a set of buttons which represent a list of
** records.  The caller provides facilities for editing the records
** individually, and this code handles the organization of the overall list,
** such that the user can modify, add, and delete records, and re-order the
** list.
**
** The format for the list of records managed by this code should be an
** array of size "maxItems" of pointers to record structures.  The records
** themselves can be of any format, but the first field of the structure
** must be a pointer to a character string which will be displayed as the
** item name in the list.  The list "itemList", and the number of items
** "nItems" are automatically updated by the list management routines as the
** user makes changes.
**
** The caller must provide routines for transferring data to and from the
** dialog fields dedicated to displaying and editing records in the list.
** The callback "setDialogDataCB" must take the contents of the item pointer
** passed to it, and display the data it contains, erasing any previously
** displayed data.  The format of the setDialogData callback is:
**
**    void setDialogDataCB(void *item, void *cbArg)
**
**	 item:	a pointer to the record to be displayed
**
**  	 cbArg:	an arbitrary argument passed on to the callback routine
**
** The callback "setDialogDataCB" must allocate (with NEditMalloc) and return a
** new record reflecting the current contents of the dialog fields.  It may
** do error checking on the data that the user has entered, and can abort
** whatever operation triggered the request by setting "abort" to True.
** This routine is called in a variety of contexts, such as the user
** clicking on a list item, or requesting that a copy be made of the current
** list item.  To aide in communicating errors to the user, the boolean value
** "explicitRequest" distinguishes between the case where the user has
** specifically requested that the fields be read, and the case where he
** may be surprised that errors are being reported, and require further
** explanation.  The format of the getDialogData callback is:
**
**    void *getDialogDataCB(void *oldItem, int explicitRequest, int *abort,
**  	      void *cbArg)
**
**	 oldItem: a pointer to the existing record being modified in the
**	    dialog, or NULL, if the user is modifying the "New" item.
**
**	 explicitRequest: True if the user directly asked for the records
**	    to be changed (as with an OK or Apply button).  If a less direct
**	    process resulted in the request, the user might need extra
**	    explanation and possibly a chance to proceed using the existing
**	    stored data (to use the data from oldItem, the routine should
**	    make a new copy).
**
**	 abort: Can be set to True if the dialog fields contain errors.
**	    Setting abort to True, stops whetever process made the request
**	    for updating the data in the list from the displayed data, and
**	    forces the user to remain focused on the currently displayed
**	    item until he either gives up or gets it right.
**
**	 cbArg: arbitrary data, passed on from where the callback was
**	    established in the list creation routines
**
**    The return value should be an allocated
**
** The callback "freeItemCB" should free the item passed to it:
**
**    void freeItemCB(void *item, void *cbArg)
**
**	 item:	a pointer to the record to be freed
**
** The difference between ManageListAndButtons and CreateManagedList, is that
** in this routine, the caller creates the list and button widgets and passes
** them here so that they be arranged explicitly, rather than relying on the
** default style imposed by CreateManagedList.  ManageListAndButtons simply
** attaches the appropriate callbacks to process mouse and keyboard input from
** the widgets.
*/
Widget ManageListAndButtons(Widget listW, Widget deleteBtn, Widget copyBtn,
    	Widget moveUpBtn, Widget moveDownBtn, void **itemList, int *nItems,
	int maxItems, void *(*getDialogDataCB)(void *, int, int *, void *),
    	void *getDialogDataArg, void (*setDialogDataCB)(void *, void *),
    	void *setDialogDataArg, void (*freeItemCB)(void *))
{
    managedListData *ml;
    
    /* Create a managedList data structure to hold information about the
       widgets, callbacks, and current state of the list */
    ml = (managedListData *)NEditMalloc(sizeof(managedListData));
    ml->listW = listW;
    ml->deleteBtn = deleteBtn;
    ml->copyBtn = copyBtn;
    ml->moveUpBtn = moveUpBtn;
    ml->moveDownBtn = moveDownBtn;
    ml->getDialogDataCB = NULL;
    ml->getDialogDataArg = getDialogDataArg;
    ml->setDialogDataCB = NULL;
    ml->setDialogDataArg = setDialogDataArg;
    ml->freeItemCB = freeItemCB;
    ml->deleteConfirmCB = NULL;
    ml->deleteConfirmArg = NULL;
    ml->nItems = nItems;
    ml->maxItems = maxItems;
    ml->itemList = itemList;
    ml->lastSelection = 1;

    /* Make the managed list data structure accessible from the list widget
       pointer, and make sure it gets freed when the list is destroyed */
    XtVaSetValues(ml->listW, XmNuserData, ml, NULL);
    XtAddCallback(ml->listW, XmNdestroyCallback, destroyCB, ml);
    
    /* Add callbacks for button and list actions */
    XtAddCallback(ml->deleteBtn, XmNactivateCallback, deleteCB, ml);
    XtAddCallback(ml->copyBtn, XmNactivateCallback, copyCB, ml);
    XtAddCallback(ml->moveUpBtn, XmNactivateCallback, moveUpCB, ml);
    XtAddCallback(ml->moveDownBtn, XmNactivateCallback, moveDownCB, ml);
    XtAddCallback(ml->listW, XmNbrowseSelectionCallback, listSelectionCB, ml);
    
    /* Initialize the list and buttons (don't set up the callbacks until
       this is done, so they won't get called on creation) */
    updateDialogFromList(ml, -1);
    ml->getDialogDataCB = getDialogDataCB;
    ml->setDialogDataCB = setDialogDataCB;
    
    return ml->listW;
}

/*
** Update the currently selected list item from the dialog fields, using
** the getDialogDataCB callback.  "explicitRequest" is a boolean value
** passed to on to the getDialogDataCB callback to help set the tone for
** how error messages are presented (see ManageListAndButtons for more
** information).
*/
int UpdateManagedList(Widget listW, int explicitRequest)
{
    managedListData *ml;
    
    /* Recover the pointer to the managed list structure from the widget's
       userData pointer */
    XtVaGetValues(listW, XmNuserData, &ml, NULL);
    
    /* Make the update */
    return incorporateDialogData(ml, selectedListPosition(ml), explicitRequest);
}

/*
** Update the displayed list and data to agree with a data list which has
** been changed externally (not by the ManagedList list manager).
*/
void ChangeManagedListData(Widget listW)
{
    managedListData *ml;
    
    /* Recover the pointer to the managed list structure from the widget's
       userData pointer */
    XtVaGetValues(listW, XmNuserData, &ml, NULL);
    
    updateDialogFromList(ml, -1);
}

/*
** Change the selected item in the managed list given the index into the
** list being managed.
*/
void SelectManagedListItem(Widget listW, int itemIndex)
{
    selectItem(listW, itemIndex, True);
}

/*
** Return the index of the item currently selected in the list
*/
int ManagedListSelectedIndex(Widget listW)
{
    managedListData *ml;
    
    XtVaGetValues(listW, XmNuserData, &ml, NULL);
    return selectedListPosition(ml)-2;
}

/*
** Add a delete-confirmation callback to a managed list.  This will be called
** when the user presses the Delete button on the managed list.  The callback
** can put up a dialog, and optionally abort the operation by returning False.
*/
void AddDeleteConfirmCB(Widget listW, int (*deleteConfirmCB)(int, void *),
    	void *deleteConfirmArg)
{
    managedListData *ml;
    
    XtVaGetValues(listW, XmNuserData, &ml, NULL);
    ml->deleteConfirmCB = deleteConfirmCB;
    ml->deleteConfirmArg = deleteConfirmArg;
}

/*
** Called on destruction of the list widget
*/
static void destroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* Free the managed list data structure */
    NEditFree(clientData);
}

/*
** Button callbacks: deleteCB, copyCB, moveUpCB, moveDownCB
*/
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData)
{
    managedListData *ml = (managedListData *)clientData;
    int i, ind, listPos;
        
    /* get the selected list position and the item to be deleted */
    listPos = selectedListPosition(ml);
    ind = listPos-2;
    
    /* if there's a delete confirmation callback, call it first, and allow
       it to request that the operation be aborted */
    if (ml->deleteConfirmCB != NULL)
    	if (!(*ml->deleteConfirmCB)(ind, ml->deleteConfirmArg))
    	    return;

    /* free the item and remove it from the list */
    (*ml->freeItemCB)(ml->itemList[ind]);
    for (i=ind; i<*ml->nItems-1; i++)
    	ml->itemList[i] = ml->itemList[i+1];
    (*ml->nItems)--;
    
    /* update the list widget and move the selection to the previous item
       in the list and display the fields appropriate  for that entry */
    updateDialogFromList(ml, ind-1);
}

static void copyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    managedListData *ml = (managedListData *)clientData;
    int i, listPos, abort = False;
    void *item;
    
    /* get the selected list position and the item to be copied */
    listPos = selectedListPosition(ml);
    if (listPos == 1)
    	return; /* can't copy "new" */

    if ((*ml->nItems) == ml->maxItems) {
        DialogF(DF_ERR, shellOfWidget(ml->listW), 1, "Limits exceeded",
                        "Cannot copy item.\nToo many items in list.",
                        "OK");
        return;
    }

    /* Bring the entry up to date (could result in operation being canceled) */
    item = (*ml->getDialogDataCB)(ml->itemList[listPos-2], False, &abort,
    	    ml->getDialogDataArg);
    if (abort)
    	return;
    if (item != NULL) {
	(*ml->freeItemCB)(ml->itemList[listPos-2]);
	ml->itemList[listPos-2] = item;
    }
    
    /* Make a copy by requesting the data again.
       In case getDialogDataCB() returned a fallback value, the dialog may
       not be in sync with the internal list. If we _explicitly_ request the
       data again, we could get an invalid answer. Therefore, we first update
       the dialog to make sure that we can copy the right data. */
    updateDialogFromList(ml, listPos-2);
    item = (*ml->getDialogDataCB)(ml->itemList[listPos-2], True, &abort,
    	    ml->getDialogDataArg);
    if (abort)
	return;
        
    /* add the item to the item list */
    for (i= *ml->nItems; i>=listPos; i--)
    	ml->itemList[i] = ml->itemList[i-1];
    ml->itemList[listPos-1] = item;
    (*ml->nItems)++;
    
    /* redisplay the list widget and select the new item */
    updateDialogFromList(ml, listPos-1);
}

static void moveUpCB(Widget w, XtPointer clientData, XtPointer callData)
{
    managedListData *ml = (managedListData *)clientData;
    int ind, listPos;
    void *temp;
        
    /* get the item index currently selected in the menu item list */
    listPos = selectedListPosition(ml);
    ind = listPos-2;
     
    /* Bring the item up to date with the dialog fields (It would be better
       if this could be avoided, because user errors will be flagged here,
       but it's not worth re-writing everything for such a trivial point) */
    if (!incorporateDialogData(ml, ml->lastSelection, False))
    	return;
     
    /* shuffle the item up in the menu item list */
    temp = ml->itemList[ind];
    ml->itemList[ind] = ml->itemList[ind-1];
    ml->itemList[ind-1] = temp;
    
    /* update the list widget and keep the selection on moved item */
    updateDialogFromList(ml, ind-1);
}

static void moveDownCB(Widget w, XtPointer clientData, XtPointer callData)
{
    managedListData *ml = (managedListData *)clientData;
    int ind, listPos;
    void *temp;
        
    /* get the item index currently selected in the menu item list */
    listPos = selectedListPosition(ml);
    ind = listPos-2;

    /* Bring the item up to date with the dialog fields (I wish this could
       be avoided) */
    if (!incorporateDialogData(ml, ml->lastSelection, False))
    	return;
     
    /* shuffle the item down in the menu item list */
    temp = ml->itemList[ind];
    ml->itemList[ind] = ml->itemList[ind+1];
    ml->itemList[ind+1] = temp;
    
    /* update the list widget and keep the selection on moved item */
    updateDialogFromList(ml, ind+1);
}

/*
** Called when the user clicks on an item in the list widget
*/
static void listSelectionCB(Widget w, XtPointer clientData, XtPointer callData)
{
    managedListData *ml = (managedListData *)clientData;
    int ind, listPos = ((XmListCallbackStruct *)callData)->item_position;
    
    /* Save the current dialog fields before overwriting them.  If there's an
       error, force the user to go back to the old selection and fix it
       before proceeding */
    if (ml->getDialogDataCB != NULL && ml->lastSelection != 0) {
	if (!incorporateDialogData(ml, ml->lastSelection, False)) {
	    XmListDeselectAllItems(ml->listW);
	    XmListSelectPos(ml->listW, ml->lastSelection, False);
    	    return;
    	}
    	/* reselect item because incorporateDialogData can alter selection */
    	selectItem(ml->listW, listPos-2, False);
    }
    ml->lastSelection = listPos;
    
    /* Dim or un-dim buttons at bottom of dialog based on whether the
       selected item is a menu entry, or "New" */
    if (listPos == 1) {
    	XtSetSensitive(ml->copyBtn, False);
    	XtSetSensitive(ml->deleteBtn, False);
    	XtSetSensitive(ml->moveUpBtn, False);
    	XtSetSensitive(ml->moveDownBtn, False);
    } else {
    	XtSetSensitive(ml->copyBtn, True);
    	XtSetSensitive(ml->deleteBtn, True);
    	XtSetSensitive(ml->moveUpBtn, listPos != 2);
    	XtSetSensitive(ml->moveDownBtn, listPos != *ml->nItems+1);
    }
        
    /* get the index of the item currently selected in the item list */
    ind = listPos - 2;
    
    /* tell the caller to show the new item */
    if (ml->setDialogDataCB != NULL)
    	(*ml->setDialogDataCB)(listPos==1 ? NULL : ml->itemList[ind],
    	    	ml->setDialogDataArg);
}

/*
** Incorporate the current contents of the dialog fields into the list
** being managed, and if necessary change the display in the list widget.
** The data is obtained by calling the getDialogDataCB callback, which
** is allowed to reject whatever request triggered the update.  If the
** request is rejected, the return value from this function will be False.
*/
static int incorporateDialogData(managedListData *ml, int listPos, int explicit)
{
    int abort = False;
    void *item;
     
    /* Get the current contents of the dialog fields.  Callback will set
       abort to True if canceled */
    item = (*ml->getDialogDataCB)(listPos == 1 ? NULL : ml->itemList[
    	    listPos-2], explicit, &abort, ml->getDialogDataArg);
    if (abort)
    	return False;
    if (item == NULL) /* don't modify if fields are empty */
    	return True;
    	
    /* If the item is "new" add a new entry to the list, otherwise,
       modify the entry with the text fields from the dialog */
    if (listPos == 1) {
        if ((*ml->nItems) == ml->maxItems) {
            DialogF(DF_ERR, shellOfWidget(ml->listW), 1, "Limits exceeded",
                            "Cannot add new item.\nToo many items in list.",
                            "OK");
            return False;
        }
	ml->itemList[(*ml->nItems)++] = item;
	updateDialogFromList(ml, *ml->nItems - 1);
    } else {
	(*ml->freeItemCB)(ml->itemList[listPos-2]);
	ml->itemList[listPos-2] = item;
	updateListWidgetItem(ml, listPos);
    }
    return True;
}

/*
** Update the list widget to reflect the current contents of the managed item
** list, set the item that should now be highlighted, and call getDisplayed
** on the newly selected item to fill in the dialog fields.
*/
static void updateDialogFromList(managedListData *ml, int selection)
{
    int i;
    XmString *stringTable;
    
    /* On many systems under Motif 1.1 the list widget can't handle items
       being changed while anything is selected! */
    XmListDeselectAllItems(ml->listW);

    /* Fill in the list widget with the names from the item list */
    stringTable = (XmString *)NEditMalloc(sizeof(XmString) * (*ml->nItems+1));
    stringTable[0] = XmStringCreateSimple("New");
    for (i=0; i < *ml->nItems; i++)
    	stringTable[i+1] = XmStringCreateSimple(*(char **)ml->itemList[i]);
    XtVaSetValues(ml->listW, XmNitems, stringTable,
    	    XmNitemCount, *ml->nItems+1, NULL);
    for (i=0; i < *ml->nItems+1; i++)
    	XmStringFree(stringTable[i]);
    NEditFree(stringTable);

    /* Select the requested item (indirectly filling in the dialog fields),
       but don't trigger an update of the last selected item from the current
       dialog fields */
    ml->lastSelection = 0;
    selectItem(ml->listW, selection, True);
}

/*
** Update one item of the managed list widget to reflect the current contents
** of the managed item list.
*/
static void updateListWidgetItem(managedListData *ml, int listPos)
{
    int savedPos;
    XmString newString[1];
    
    /* save the current selected position (Motif sometimes does stupid things
       to the selection when a change is made, like selecting the new item
       if it matches the name of currently selected one) */
    savedPos = selectedListPosition(ml);
    XmListDeselectAllItems(ml->listW);
    
    /* update the list */
    newString[0] = XmStringCreateSimple(*(char **)ml->itemList[listPos-2]);
    XmListReplaceItemsPos(ml->listW, newString, 1, listPos);
    XmStringFree(newString[0]);
    
    /* restore the selected position */
    XmListSelectPos(ml->listW, savedPos, False);
}

/*
** Get the position of the selection in the menu item list widget
*/
static int selectedListPosition(managedListData *ml)
{
    int listPos;
    int *posList = NULL, posCount = 0;

    if (!XmListGetSelectedPos(ml->listW, &posList, &posCount)) {
	fprintf(stderr, "Internal error (nothing selected)");
    	return 1;
    }
    listPos = *posList;
    NEditFree(posList);
    if (listPos < 1 || listPos > *ml->nItems+1) {
    	fprintf(stderr, "Internal error (XmList bad value)");
    	return 1;
    }
    return listPos;
}

/*
** Select an item in the list given the list (array) index value.
** If updateDialog is True, trigger a complete dialog update, which
** could potentially reject the change.
*/
static void selectItem(Widget listW, int itemIndex, int updateDialog)
{
    int topPos, nVisible, selection = itemIndex+2;
    
    /* Select the item */
    XmListDeselectAllItems(listW);
    XmListSelectPos(listW, selection, updateDialog);
    
    /* If the selected item is not visible, scroll the list */
    XtVaGetValues(listW, XmNtopItemPosition, &topPos, XmNvisibleItemCount,
    	    &nVisible, NULL);
    if (selection < topPos)
    	XmListSetPos(listW, selection);
    else if (selection >= topPos + nVisible)
    	XmListSetPos(listW, selection - nVisible + 1);
}

static Widget shellOfWidget(Widget w)
{
    while(1) {
        if (!w) return 0;
        if (XtIsSubclass(w, shellWidgetClass)) return w;
        w = XtParent(w);
    }      
}
