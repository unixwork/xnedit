/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Microline Widget Library, originally made available under the NPL by Neuron Data <http://www.neurondata.com>.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * In addition, as a special exception to the GNU GPL, the copyright holders
 * give permission to link the code of this program with the Motif and Open
 * Motif libraries (or with modified versions of these that use the same
 * license), and distribute linked combinations including the two. You
 * must obey the GNU General Public License in all respects for all of
 * the code used other than linking with Motif/Open Motif. If you modify
 * this file, you may extend this exception to your version of the file,
 * but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 *
 * ***** END LICENSE BLOCK ***** */


#include <Xm/Xm.h>
#include <Xm/DrawnB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <XmL/Folder.h>

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, form, folder, tab, folderForm;
	XmString str;
	char pageName[20], tabName[20];
	int i;

	shell =  XtAppInitialize(&app, "Folder2", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	form = XtVaCreateManagedWidget("form",
		xmFormWidgetClass, shell,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XmNmarginWidth, 8,
		XmNmarginHeight, 8,
		XmNshadowThickness, 0,
		NULL);

	folder = XtVaCreateManagedWidget("folder",
		xmlFolderWidgetClass, form,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XmNtabPlacement, XmFOLDER_RIGHT,
		XmNmarginWidth, 10,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);

	for (i = 0; i < 3; i++)
	{
		sprintf(pageName, "Page %d", i);

		/* Add a tab (DrawnButton) to the Folder */
		sprintf(tabName, "Tab %d", i);
		str = XmStringCreateSimple(tabName);
		tab = XtVaCreateManagedWidget("tab",
			xmDrawnButtonWidgetClass, folder,
			XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
			XtVaTypedArg, XmNforeground, XmRString, "black", 6,
			XmNlabelString, str,
			XmNtabManagedName, pageName,
			NULL);
		XmStringFree(str);

		/* Add a Form to the Folder which will appear in the page */
		/* area. This Form will be managed by the tab created above */
		/* because it has the same tabManagedName as the tab widget */
		folderForm = XtVaCreateManagedWidget("folderForm",
			xmFormWidgetClass, folder,
			XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
			XmNtabManagedName, pageName,
			NULL);

		/* Add a Label as a child of the Form */
		XtVaCreateManagedWidget(pageName,
			xmLabelWidgetClass, folderForm,
			XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
			XtVaTypedArg, XmNforeground, XmRString, "black", 6,
			XmNmarginWidth, 100,
			XmNmarginHeight, 80,
			XmNtopAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			NULL);
	}

	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}
