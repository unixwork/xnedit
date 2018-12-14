/*******************************************************************************
*									       *
* prefFile.c -- Nirvana utilities for providing application preferences files  *
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
* June 3, 1993								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "prefFile.h"
#include "fileUtils.h"
#include "utils.h"
#include "nedit_malloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef VMS
#include "VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif
#include <Xm/Xm.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#include <inttypes.h>

#define N_BOOLEAN_STRINGS 13
static const char *TrueStrings[N_BOOLEAN_STRINGS] = {"True", "true", "TRUE", "T", "t",
	"Yes", "yes", "YES", "y", "Y", "on", "On", "ON"};
static const char *FalseStrings[N_BOOLEAN_STRINGS] = {"False", "false", "FALSE", "F", "f",
	"No", "no", "NO", "n", "N", "off", "Off", "OFF"};

static void readPrefs(XrmDatabase prefDB, XrmDatabase appDB,
        const char *appName, const char *appClass,
	PrefDescripRec *rsrcDescrip, int nRsrc, int overlay);
static int stringToPref(const char *string, PrefDescripRec *rsrcDescrip);
static char *removeWhiteSpace(const char *string);

/*
** Preferences File
**
** An application maintains a preferences file so that users can
** quickly save and restore program options from within a program,
** without being forced to learn the X resource mechanism.
**
** Preference files are the same format as X resource files, and
** are read using the X resource file reader.  X-savvy users are allowed
** to move resources out of a preferences file to their X resource
** files.  They would do so if they wanted to attach server-specific
** preferences (such as fonts and colors) to different X servers, or to
** combine additional preferences served only by X resources with those
** provided by the program's menus.
*/

/*
** Preference description table
**
** A preference description table contains the information necessary
** to read preference resources and store their values in a data
** structure.  The table can, so far, describe four types
** of values (this will probably be expanded in the future to include
** more types): ints, booleans, enumerations, and strings.  Each entry 
** includes the name and class for saving and restoring the parameter
** in X database format, the data type, a default value in the form of
** a character string, and the address where the parameter value is
** be stored.  Strings and enumerations take an additional argument.
** For strings, it is the maximum length string that can safely be
** stored or NULL to indicate that new space should be allocated and a
** pointer to it stored in the value address.  For enums, it is an array
** of string pointers to the names of each of its possible values. The
** last value in a preference record is a flag for determining whether
** the value should be written to the save file by SavePreferences.
*/

/*
** CreatePreferencesDatabase
**
** Process a preferences file and the command line options pertaining to
** the X resources used to set those preferences.  Create an X database
** of the results.  The reason for this odd set of functionality is
** to process command line options before XtDisplayInitialize reads them
** into the application database that the toolkit attaches to the display.
** This allows command line arguments to properly override values specified
** in the preferences file.
**
** 	fileName	Name only of the preferences file to be found
**			in the user's home directory
**	appName		Application name to use in reading the preference
**			resources
**	opTable		Xrm command line option table for the resources
**			used in the preferences file ONLY.  Command line
**			options for other X resources should be processed
**			by XtDisplayInitialize.
**	nOptions	Number of items in opTable
**	argcInOut	Address of argument count.  This will be altered
**			to remove the command line options that are 
**			recognized in the option table.
**	argvInOut	Argument vector.  Will be altered as argcInOut.
*/
XrmDatabase CreatePreferencesDatabase(const char *fullName, const char *appName, 
	 XrmOptionDescList opTable, int nOptions, unsigned int *argcInOut,
	 char **argvInOut)
{
    XrmDatabase db;
    int argcCopy;
    char **argvCopy;
    char *fileString;
    static XrmOptionDescRec xrmOnlyTable[] =
	    {{"-xrm", NULL, XrmoptionResArg, (caddr_t)NULL}};
        
    /* read the preferences file into an X database.
       On failure prefDB will be NULL. */
    if (NULL == fullName)
    {
        db = NULL;
    } else
    {
        fileString = ReadAnyTextFile(fullName, False);
        if (NULL == fileString)
        {
            db = NULL;
        } else
        {
            char* rsrcName;
            db = XrmGetStringDatabase(fileString);
            NEditFree(fileString);

            /*  Add a resource to the database which remembers that
                the file is read, so that NEdit will know it.  */
            rsrcName = (char*) NEditMalloc(strlen(appName) + 14);
            sprintf(rsrcName, "%s.prefFileRead", appName);
            XrmPutStringResource(&db, rsrcName, "True");
            NEditFree(rsrcName);
        }
    }
    
    /* parse the command line, storing results in the preferences database */
    XrmParseCommand(&db, opTable, nOptions, appName, (int *)argcInOut,
    	    argvInOut);
    
    /* process -xrm (resource setting by resource name) arguments so those
       pertaining to preference resources will be included in the database.
       Don't remove -xrm arguments from the argument vector, however, so
       XtDisplayInitialize can still read the non-preference resources */
    argvCopy = (char**)NEditMalloc(sizeof(char *) * *argcInOut);
    memcpy(argvCopy, argvInOut, sizeof(char *) * *argcInOut);
    argcCopy = *argcInOut;
    XrmParseCommand(&db, xrmOnlyTable, XtNumber(xrmOnlyTable), appName,
    	    &argcCopy, argvCopy);
    NEditFree((char *)argvCopy);
    return db;
}

/*
** RestorePreferences
**
** Fill in preferences data from two X databases, values in prefDB taking
** precidence over those in appDB.
*/	 
void RestorePreferences(XrmDatabase prefDB, XrmDatabase appDB,
	const char *appName, const char *appClass, PrefDescripRec *rsrcDescrip, int nRsrc)
{
    readPrefs(prefDB, appDB, appName, appClass, rsrcDescrip, nRsrc, False);
}

/*
** OverlayPreferences
**
** Incorporate preference specified in database "prefDB", preserving (not
** restoring to default) existing preferences, not mentioned in "prefDB"
*/	 
void OverlayPreferences(XrmDatabase prefDB, const char *appName,
        const char *appClass, PrefDescripRec *rsrcDescrip, int nRsrc)
{
    readPrefs(NULL, prefDB, appName, appClass, rsrcDescrip, nRsrc, True);
}

static void readPrefs(XrmDatabase prefDB, XrmDatabase appDB,
        const char *appName, const char *appClass, PrefDescripRec *rsrcDescrip,
        int nRsrc, int overlay)
{
    char rsrcName[256], rsrcClass[256], *valueString, *type;
    XrmValue rsrcValue;
    int i;

    /* read each resource, trying first the preferences file database, then
       the application database, then the default value if neither are found */
    for (i=0; i<nRsrc; i++) {
    	sprintf(rsrcName,"%s.%s", appName, rsrcDescrip[i].name);
    	sprintf(rsrcClass, "%s.%s", appClass, rsrcDescrip[i].class);
    	if (prefDB!=NULL &&
    	       XrmGetResource(prefDB, rsrcName, rsrcClass, &type, &rsrcValue)) {
    	    if (strcmp(type, XmRString)) {
                fprintf(stderr,"nedit: Internal Error: Unexpected resource type, %s\n",
    	    		type);
    	    	return;
    	    }
    	    valueString = rsrcValue.addr;
    	} else if (XrmGetResource(appDB,rsrcName,rsrcClass,&type,&rsrcValue)) {
    	    if (strcmp(type, XmRString)) {
                fprintf(stderr,"nedit: Internal Error: Unexpected resource type, %s\n",
    	    		type);
    	    	return;
    	    }
    	    valueString = rsrcValue.addr;
    	} else
    	    valueString = rsrcDescrip[i].defaultString;
	if (overlay && valueString == rsrcDescrip[i].defaultString)
	    continue;
    	if (!stringToPref(valueString, &rsrcDescrip[i]))
            fprintf(stderr, "nedit: Could not read value of resource %s\n", rsrcName);
    }
}

/*
** RestoreDefaultPreferences
**
** Restore preferences to their default values as stored in rsrcDesrcip
*/
void RestoreDefaultPreferences(PrefDescripRec *rsrcDescrip, int nRsrc)
{
    int i;
    
    for (i=0; i<nRsrc; i++)
	stringToPref(rsrcDescrip[i].defaultString, &rsrcDescrip[i]);
}

/*
** SavePreferences
**
** Create or replace an application preference file according to
** the resource descriptions in rsrcDesrcip.
*/
int SavePreferences(Display *display, const char *fullName,
        const char *fileHeader,	PrefDescripRec *rsrcDescrip, int nRsrc)
{
    char *appName, *appClass, **enumStrings;
    FILE *fp;
    int type;
    int i;
    
    /* open the file */
    if ((fp = fopen(fullName, "w")) == NULL)
    	return False;
    
    /* write the file header text out to the file */
    fprintf(fp, "%s\n", fileHeader);
    
    /* write out the resources so they can be read by XrmGetFileDatabase */
    XtGetApplicationNameAndClass(display, &appName, &appClass);
    for (i=0; i<nRsrc; i++) {
    	if (rsrcDescrip[i].save) {
    	    type = rsrcDescrip[i].dataType;
    	    fprintf(fp, "%s.%s: ", appName, rsrcDescrip[i].name);
    	    if (type == PREF_STRING)
    		fprintf(fp, "%s", (char *)rsrcDescrip[i].valueAddr);
    	    if (type == PREF_ALLOC_STRING)
    		fprintf(fp, "%s", *(char **)rsrcDescrip[i].valueAddr);
    	    else if (type == PREF_ENUM) {
    		enumStrings = (char **)rsrcDescrip[i].arg;
    		fprintf(fp,"%s", enumStrings[*(int *)rsrcDescrip[i].valueAddr]);
    	    } else if (type == PREF_INT)
    		fprintf(fp, "%d", *(int *)rsrcDescrip[i].valueAddr);
    	    else if (type == PREF_BOOLEAN) {
    		if (*(int *)rsrcDescrip[i].valueAddr)
    	    	    fprintf(fp, "True");
    		else
    	    	    fprintf(fp, "False");
    	    }
    	    fprintf(fp, "\n");
    	}
    }
    fclose(fp);
    return True;
}

static int stringToPref(const char *string, PrefDescripRec *rsrcDescrip)
{
    int i;
    char *cleanStr, *endPtr, **enumStrings;
    
    switch (rsrcDescrip->dataType) {
      case PREF_INT:
	cleanStr = removeWhiteSpace(string);
	*(int *)rsrcDescrip->valueAddr =
		strtol(cleanStr, &endPtr, 10);
	if (strlen(cleanStr) == 0) {		/* String is empty */
	    *(int *)rsrcDescrip->valueAddr = 0;
	    NEditFree(cleanStr);
	    return False;
	} else if (*endPtr != '\0') {		/* Whole string not parsed */
    	    *(int *)rsrcDescrip->valueAddr = 0;
	    NEditFree(cleanStr);
    	    return False;
    	}
	NEditFree(cleanStr);
	return True;
      case PREF_BOOLEAN:
      	cleanStr = removeWhiteSpace(string);
      	for (i=0; i<N_BOOLEAN_STRINGS; i++) {
      	    if (!strcmp(TrueStrings[i], cleanStr)) {
      	    	*(int *)rsrcDescrip->valueAddr = True;
      	    	NEditFree(cleanStr);
      	    	return True;
      	    }
      	    if (!strcmp(FalseStrings[i], cleanStr)) {
      	    	*(int *)rsrcDescrip->valueAddr = False;
      	    	NEditFree(cleanStr);
      	    	return True;
      	    }
      	}
      	NEditFree(cleanStr);
      	*(int *)rsrcDescrip->valueAddr = False;
    	return False;
      case PREF_ENUM:
      	cleanStr = removeWhiteSpace(string);
      	enumStrings = (char **)rsrcDescrip->arg;
      	for (i=0; enumStrings[i]!=NULL; i++) {
      	    if (!strcmp(enumStrings[i], cleanStr)) {
      	    	*(int *)rsrcDescrip->valueAddr = i;
      	    	NEditFree(cleanStr);
      	    	return True;
      	    }
      	}
      	NEditFree(cleanStr);
      	*(int *)rsrcDescrip->valueAddr = 0;
    	return False;
      case PREF_STRING:
	if (strlen(string) >= (size_t)(intptr_t)rsrcDescrip->arg)
      	    return False;
	strncpy(rsrcDescrip->valueAddr, string, (size_t)(intptr_t)rsrcDescrip->arg);
      	return True;
      case PREF_ALLOC_STRING:
      	*(char **)rsrcDescrip->valueAddr = NEditStrdup(string);
      	return True;
    }
    return False;
}

/*
** Remove the white space (blanks and tabs) from a string and return
** the result in a newly allocated string as the function value
*/
static char *removeWhiteSpace(const char *string)
{
    char *outPtr, *outString;
    
    outPtr = outString = (char*)NEditMalloc(strlen(string)+1);
    while (TRUE) {
    	if (*string != ' ' && *string != '\t')
	    *(outPtr++) = *(string++);
	else
	    string++;
    	if (*string == 0) {
	    *outPtr = 0;
	    return outString;
	}
    }
}

/*******************
Implementation Note:
Q: Why aren't you using the Xt type conversion services?
A: 1) To create a save file, you also need to convert values back to text form,
and there are no converters for that direction.  2) XtGetApplicationResources
can only be used on the resource database created by the X toolkit at 
initialization time, and there is no way to intervene in the creation of
that database or store new resources in it reliably after it is created.
3) The alternative, XtConvertAndStore is not adequately documented.  The
toolkit mauual does not explain why it overwrites its input value structure.
4) XtGetApplicationResources and XtConvertAndStore do not work well together
because they use different storage strategies for certain data types.
*******************/
