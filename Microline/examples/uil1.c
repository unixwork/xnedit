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
#include <Mrm/MrmPublic.h>
#include <XmL/Folder.h>
#include <XmL/Grid.h>
#include <XmL/Progress.h>
#include <XmL/Tree.h>
#include <stdio.h>

main(argc, argv)
int	argc;
String argv[];
{
	Display *dpy;
	XtAppContext app;
	Widget toplevel, tree, shellForm;
	Pixmap pixmap;
	XmString str;
	MrmHierarchy hier;
	MrmCode clas;
	static char *files[] = {
		"uil1.uid"			};

	MrmInitialize ();
	XtToolkitInitialize();
	app = XtCreateApplicationContext();
	dpy = XtOpenDisplay(app, NULL, argv[0], "Uil1",
		NULL, 0, &argc, argv);
	if (dpy == NULL) {
		fprintf(stderr, "%s:  Can't open display\n", argv[0]);
		exit(1);
	}

	toplevel = XtVaAppCreateShell(argv[0], NULL,
		applicationShellWidgetClass, dpy,
		XmNwidth, 400,
		XmNheight, 300,
		NULL);

	if (MrmOpenHierarchy (1, files, NULL, &hier) != MrmSUCCESS)
		printf ("can't open hierarchy\n");

	MrmRegisterClass(0, NULL, "XmLCreateFolder",
		XmLCreateFolder, xmlFolderWidgetClass);
	MrmRegisterClass(0, NULL, "XmLCreateGrid",
		XmLCreateGrid, xmlGridWidgetClass);
	MrmRegisterClass(0, NULL, "XmLCreateProgress",
		XmLCreateProgress, xmlProgressWidgetClass);
	MrmRegisterClass(0, NULL, "XmLCreateTree",
		XmLCreateTree, xmlTreeWidgetClass);

	if (MrmFetchWidget(hier, "shellForm", toplevel, &shellForm,
		&clas) != MrmSUCCESS)
		printf("can't fetch shellForm\n");

	tree = XtNameToWidget(shellForm, "*tree");

	/* Add two rows to the Tree */
	pixmap = XmUNSPECIFIED_PIXMAP;
	str = XmStringCreateSimple("Root");
	XmLTreeAddRow(tree, 0, True, True, -1, pixmap, pixmap, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Child of Root");
	XmLTreeAddRow(tree, 1, False, False, -1, pixmap, pixmap, str);
	XmStringFree(str);

	XtManageChild(shellForm);
	XtRealizeWidget(toplevel);
	XtAppMainLoop(app);

	return (0);
}
