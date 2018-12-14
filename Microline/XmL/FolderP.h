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


#ifndef XmLFolderPH
#define XmLFolderPH

#include <Xm/XmP.h>

#ifdef MOTIF11
#else
#include <Xm/ManagerP.h>
#endif

#include "Folder.h"

typedef struct _XmLFolderPart
	{
	int debugLevel;
	Boolean serverDrawsArcsLarge;
	unsigned char cornerStyle, tabPlacement, resizePolicy;
	Boolean allowRotate, autoSelect;
	GC gc;
	Pixel inactiveBg, inactiveFg, blankBg;
	Pixmap blankPix;
	WidgetList tabs;
	int tabCount, tabAllocCount;
	Dimension marginWidth, marginHeight, spacing;
	Dimension cornerDimension, highlightThickness;
	Dimension pixmapMargin;
	Dimension tabHeight, tabWidth, tabBarHeight;
	Dimension minTabWidth, maxTabWidth;
	int tabsPerRow, activeRow;
	XtTranslations primTrans;
	Widget focusW, activeW;
	int activeTab;
	char allowLayout;
	XtCallbackList activateCallback;
	XmFontList fontList;

	WidgetClass tabWidgetClass;

	} XmLFolderPart;

typedef struct _XmLFolderRec
	{
	CorePart core;
	CompositePart composite;
	ConstraintPart constraint;
	XmManagerPart manager;
	XmLFolderPart folder;
	} XmLFolderRec;

typedef struct _XmLFolderClassPart
	{
	int unused;
	} XmLFolderClassPart;

typedef struct _XmLFolderClassRec
	{
	CoreClassPart core_class;
	CompositeClassPart composite_class;
	ConstraintClassPart constraint_class;
	XmManagerClassPart manager_class;
	XmLFolderClassPart folder_class;
	} XmLFolderClassRec;

extern XmLFolderClassRec xmlFolderClassRec;

typedef struct _XmLFolderConstraintPart
	{
	Position x, y;
	Dimension width, height;
	Dimension maxPixWidth, maxPixHeight;
	Dimension pixWidth, pixHeight;
	Dimension inactPixWidth, inactPixHeight;
	int row;
	Boolean firstInRow;
	Boolean freePix;
	Pixmap pix, inactPix;
	char *managedName;
	Widget managedW;
	} XmLFolderConstraintPart;

typedef struct _XmLFolderConstraintRec
	{
	XmManagerConstraintPart manager;
	XmLFolderConstraintPart folder;
	} XmLFolderConstraintRec, *XmLFolderConstraintPtr;

#endif 
