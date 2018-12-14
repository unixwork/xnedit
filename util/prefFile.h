/*******************************************************************************
*                                                                              *
* prefFile.h -- Nirvana Editor Preference File Header File                     *
*                                                                              *
* Copyright 2002 The NEdit Developers                                          *
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

#ifndef NEDIT_PREFFILE_H_INCLUDED
#define NEDIT_PREFFILE_H_INCLUDED

#include <X11/Intrinsic.h>

enum PrefDataTypes {PREF_INT, PREF_BOOLEAN, PREF_ENUM, PREF_STRING,
	PREF_ALLOC_STRING};

typedef struct _PrefDescripRec {
    char *name;
    char *class;
    int dataType;
    char *defaultString;
    void *valueAddr;
    void *arg;
    int save;
} PrefDescripRec;

XrmDatabase CreatePreferencesDatabase(const char *fileName,
         const char *appName, XrmOptionDescList opTable,
         int nOptions, unsigned int *argcInOut, char **argvInOut);
void RestorePreferences(XrmDatabase prefDB, XrmDatabase appDB,
	const char *appName, const char *appClass, PrefDescripRec *rsrcDescrip, int nRsrc);
void OverlayPreferences(XrmDatabase prefDB, const char *appName,
        const char *appClass, PrefDescripRec *rsrcDescrip, int nRsrc);
void RestoreDefaultPreferences(PrefDescripRec *rsrcDescrip, int nRsrc);
int SavePreferences(Display *display, const char *fileName,
        const  char *fileHeader, PrefDescripRec *rsrcDescrip, int nRsrc);

#endif /* NEDIT_PREFFILE_H_INCLUDED */
