/*******************************************************************************
*									       *
* fileUtils.c -- File utilities for Nirvana applications		       *
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
* July 28, 1992								       *
*									       *
* Written by Mark Edel							       *
*									       *
* Modified by:	DMR - Ported to VMS (1st stage for Histo-Scope)		       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "fileUtils.h"
#include "utils.h"
#include "nedit_malloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <X11/Intrinsic.h>
#ifdef VAXC
#define NULL (void *) 0
#endif /*VAXC*/
#ifdef VMS
#include "vmsparam.h"
#include <stat.h>
#else
#include <sys/types.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#endif /*VMS*/

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#ifndef MAXSYMLINKS  /* should be defined in <sys/param.h> */
#define MAXSYMLINKS 20
#endif

#define TRUE 1
#define FALSE 0

/* Parameters to algorithm used to auto-detect DOS format files.  NEdit will
   scan up to the lesser of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
   characters of the beginning of the file, checking that all newlines are
   paired with carriage returns.  If even a single counterexample exists,
   the file is judged to be in Unix format. */
#define FORMAT_SAMPLE_LINES 5
#define FORMAT_SAMPLE_CHARS 2000

static char *nextSlash(char *ptr);
static char *prevSlash(char *ptr);
static int compareThruSlash(const char *string1, const char *string2);
static void copyThruSlash(char **toString, char **fromString);

/*
** Decompose a Unix file name into a file name and a path.
** Return non-zero value if it fails, zero else.
** For now we assume that filename and pathname are at
** least MAXPATHLEN chars long.
** To skip setting filename or pathname pass NULL for that argument.
*/
int
ParseFilename(const char *fullname, char *filename, char *pathname)
{
    int fullLen = strlen(fullname);
    int i, pathLen, fileLen;
	    
#ifdef VMS
    /* find the last ] or : */
    for (i=fullLen-1; i>=0; i--) {
    	if (fullname[i] == ']' || fullname[i] == ':')
	    break;
    }
#else  /* UNIX */
    char *viewExtendPath;
    int scanStart;
    
    /* For clearcase version extended paths, slash characters after the "@@/"
       should be considered part of the file name, rather than the path */
    if ((viewExtendPath = strstr(fullname, "@@/")) != NULL)
      	scanStart = viewExtendPath - fullname - 1;
    else
      	scanStart = fullLen - 1;
    
    /* find the last slash */
    for (i=scanStart; i>=0; i--) {
        if (fullname[i] == '/')
	    break;
    }
#endif

    /* move chars before / (or ] or :) into pathname,& after into filename */
    pathLen = i + 1;
    fileLen = fullLen - pathLen;
    if (pathname) {
      	if (pathLen >= MAXPATHLEN) {
            return 1;
	}
      	strncpy(pathname, fullname, pathLen);
      	pathname[pathLen] = 0;
    }
    if (filename) {
      	if (fileLen >= MAXPATHLEN) {
      	    return 2;
      	}
      	strncpy(filename, &fullname[pathLen], fileLen);
      	filename[fileLen] = 0;
    }

#ifndef VMS /* UNIX specific... Modify at a later date for VMS */
    if(pathname) {
	if (NormalizePathname(pathname)) {
	    return 1; /* pathname too long */
	}
	pathLen = strlen(pathname);
    }
#endif
    
    if (filename && pathname && ((pathLen + 1 + fileLen) >= MAXPATHLEN)) {
	return 1; /* pathname + filename too long */
    }
    return 0;
}

#ifndef VMS


/*
** Expand tilde characters which begin file names as done by the shell
** If it doesn't work just out leave pathname unmodified.
** This implementation is neither fast, nor elegant, nor ...
*/
int
ExpandTilde(char *pathname)
{
    struct passwd *passwdEntry;
    char username[MAXPATHLEN], temp[MAXPATHLEN];
    char *nameEnd;
    unsigned len_left;
    
    if (pathname[0] != '~')
	return TRUE;
    nameEnd = strchr(&pathname[1], '/');
    if (nameEnd == NULL) {
	nameEnd = pathname + strlen(pathname);
    }
    strncpy(username, &pathname[1], nameEnd - &pathname[1]);
    username[nameEnd - &pathname[1]] = '\0';
    /* We might consider to re-use the GetHomeDir() function,
       but to keep the code more similar for both cases ... */
    if (username[0] == '\0') {
    	passwdEntry = getpwuid(getuid());
	if ((passwdEntry == NULL) || (*(passwdEntry->pw_dir)== '\0')) {
  	   /* This is really serious, so just exit. */
           perror("NEdit/nc: getpwuid() failed ");
           exit(EXIT_FAILURE);
	}
    }
    else {
    	passwdEntry = getpwnam(username);
        if ((passwdEntry == NULL) || (*(passwdEntry->pw_dir)== '\0')) {
           /* username was just an input by the user, this is no
	      indication for some (serious) problems */
           return FALSE;
	}
    }

    strcpy(temp, passwdEntry->pw_dir);
    strcat(temp, "/");
    len_left= sizeof(temp)-strlen(temp)-1;
    if (len_left < strlen(nameEnd)) {
      /* It won't work out */
       return FALSE;
    }
    strcat(temp, nameEnd);
    strcpy(pathname, temp);
    return TRUE;
}

/*
 * Resolve symbolic links (if any) for the absolute path given in pathIn 
 * and place the resolved absolute path in pathResolved. 
 * -  pathIn must contain an absolute path spec.
 * -  pathResolved must point to a buffer of minimum size MAXPATHLEN.
 *
 * Returns:
 *   TRUE  if pathResolved contains a valid resolved path
 *         OR pathIn is not a symlink (pathResolved will have the same
 *	      contents like pathIn)
 *
 *   FALSE an error occured while trying to resolve the symlink, i.e.
 *         pathIn was no absolute path or the link is a loop.
 */
int
ResolvePath(const char * pathIn, char * pathResolved) 
{
    char resolveBuf[MAXPATHLEN], pathBuf[MAXPATHLEN];
    char *pathEnd;
    int rlResult, loops;

#ifdef NO_READLINK
    strncpy(pathResolved, pathIn, MAXPATHLEN);
    /* If there are no links at all, it's a valid "resolved" path */
    return TRUE;
#else
    /* !! readlink does NOT recognize loops, i.e. links like file -> ./file */
    for (loops=0; loops<MAXSYMLINKS; loops++) {
#ifdef UNICOS
	rlResult=readlink((char *)pathIn, resolveBuf, MAXPATHLEN-1);
#else
	rlResult=readlink(pathIn, resolveBuf, MAXPATHLEN-1);
#endif
        if (rlResult<0) {

#ifndef __Lynx__
  	    if (errno == EINVAL)
#else
	    if (errno == ENXIO)
#endif
            {
                /* It's not a symlink - we are done */
                strncpy(pathResolved, pathIn, MAXPATHLEN);
                pathResolved[MAXPATHLEN-1] = '\0';
                return TRUE;
            } else {
                return FALSE;
            }
        } else if (rlResult == 0) {
	    return FALSE;
	}

	resolveBuf[rlResult]=0;

        if (resolveBuf[0]!='/') {
	    strncpy(pathBuf, pathIn, MAXPATHLEN);
	    pathBuf[MAXPATHLEN-1] = '\0';
	    pathEnd=strrchr(pathBuf, '/');
            if (!pathEnd) {
	      	return FALSE;
            }
	    strcpy(pathEnd+1, resolveBuf);
	} else {
	    strcpy(pathBuf, resolveBuf);
	}
	NormalizePathname(pathBuf);
	pathIn=pathBuf;
    }

    return FALSE;
#endif /* NO_READLINK */
}


/*
** Return 0 if everything's fine. In fact it always return 0... (No it doesn't)
** Capable to handle arbitrary path length (>MAXPATHLEN)!
**
**  FIXME: Documentation
**  FIXME: Change return value to False and True.
*/
int NormalizePathname(char *pathname)
{
    /* if this is a relative pathname, prepend current directory */
#ifdef __EMX__
    /* OS/2, ...: welcome to the world of drive letters ... */
    if (!_fnisabs(pathname)) {
#else
    if (pathname[0] != '/') {
#endif
        char *oldPathname;
        size_t len;

        /* make a copy of pathname to work from */
	oldPathname=(char *)malloc(strlen(pathname)+1);
	strcpy(oldPathname, pathname);
	/* get the working directory and prepend to the path */
	strcpy(pathname, GetCurrentDir());

	/* check for trailing slash, or pathname being root dir "/":
	   don't add a second '/' character as this may break things
	   on non-un*x systems */
        len = strlen(pathname); /* GetCurrentDir() returns non-NULL value */

        /*  Apart from the fact that people putting conditional expressions in
            ifs should be caned: How should len ever become 0 if GetCurrentDir()
            always returns a useful value?
            FIXME: Check and document GetCurrentDir() return behaviour.  */
        if (0 == len ? 1 : pathname[len-1] != '/')
        {
            strcat(pathname, "/");
        }
	strcat(pathname, oldPathname);
	free(oldPathname);
    }

    /* compress out .. and . */
    return CompressPathname(pathname);
}


/*
** Return 0 if everything's fine, 1 else.
**
**  FIXME: Documentation
**  FIXME: Change return value to False and True.
*/
int CompressPathname(char *pathname)
{
    char *buf, *inPtr, *outPtr;
    struct stat statbuf;

    /* (Added by schwarzenberg)
    ** replace multiple slashes by a single slash 
    **  (added by yooden)
    **  Except for the first slash. From the Single UNIX Spec: "A pathname
    **  that begins with two successive slashes may be interpreted in an
    **  implementation-dependent manner"
    */
    inPtr = pathname;
    buf = (char*) malloc(strlen(pathname) + 2);
    outPtr = buf;
    *outPtr++ = *inPtr++;
    while (*inPtr)
    {
        *outPtr = *inPtr++;
        if ('/' == *outPtr)
        {
            while ('/' == *inPtr)
            {
                inPtr++;
            }
        }
        outPtr++;
    }
    *outPtr=0;
    strcpy(pathname, buf);
    
    /* compress out . and .. */
    inPtr = pathname;
    outPtr = buf;
    /* copy initial / */
    copyThruSlash(&outPtr, &inPtr);
    while (inPtr != NULL) {
	/* if the next component is "../", remove previous component */
	if (compareThruSlash(inPtr, "../")) {
		*outPtr = 0;
	    /* If the ../ is at the beginning, or if the previous component
	       is a symbolic link, preserve the ../.  It is not valid to
	       compress ../ when the previous component is a symbolic link
	       because ../ is relative to where the link points.  If there's
	       no S_ISLNK macro, assume system does not do symbolic links. */
#ifdef S_ISLNK
	    if(outPtr-1 == buf || (lstat(buf, &statbuf) == 0 &&
		    S_ISLNK(statbuf.st_mode))) {
		copyThruSlash(&outPtr, &inPtr);
	    } else
#endif	    
	    {
		/* back up outPtr to remove last path name component */
		outPtr = prevSlash(outPtr);
		inPtr = nextSlash(inPtr);
	    }
	} else if (compareThruSlash(inPtr, "./")) {
	    /* don't copy the component if it's the redundant "./" */
	    inPtr = nextSlash(inPtr);
	} else {
	    /* copy the component to outPtr */
	    copyThruSlash(&outPtr, &inPtr);
	}
    }
    /* updated pathname with the new value */
    if (strlen(buf)>MAXPATHLEN) {
       fprintf(stderr, "nedit: CompressPathname(): file name too long %s\n",
               pathname);
       free(buf);
       return 1;
    }
    else {
       strcpy(pathname, buf);
       free(buf);
       return 0;
    }
}

static char
*nextSlash(char *ptr)
{
    for(; *ptr!='/'; ptr++) {
    	if (*ptr == '\0')
	    return NULL;
    }
    return ptr + 1;
}

static char
*prevSlash(char *ptr)
{
    for(ptr -= 2; *ptr!='/'; ptr--);
    return ptr + 1;
}

static int
compareThruSlash(const char *string1, const char *string2)
{
    while (TRUE) {
    	if (*string1 != *string2)
	    return FALSE;
	if (*string1 =='\0' || *string1=='/')
	    return TRUE;
	string1++;
	string2++;
    }
}

static void
copyThruSlash(char **toString, char **fromString)
{
    char *to = *toString;
    char *from = *fromString;
    
    while (TRUE) {
        *to = *from;
        if (*from =='\0') {
            *fromString = NULL;
            return;
        }
	if (*from=='/') {
	    *toString = to + 1;
	    *fromString = from + 1;
	    return;
	}
	from++;
	to++;
    }
}

#else /* VMS */

/* 
** Dummy versions of the public functions for VMS.
*/

/*
** Return 0 if everything's fine, 1 else.
*/
int NormalizePathname(char *pathname)
{
    return 0;
}

/*
** Return 0 if everything's fine, 1 else.
*/
int CompressPathname(char *pathname)
{
    return 0;
}

/*
 * Returns:
 *   TRUE  if no error occured
 *
 *   FALSE if an error occured.
 */
int ResolvePath(const char * pathIn, char * pathResolved)
{
    if (strlen(pathIn) < MAXPATHLEN) {
	strcpy(pathResolved, pathIn);
	return TRUE;
    } else {
 	return FALSE;
    }
}

#endif /* VMS */

/*
** Return the trailing 'n' no. of path components
*/
const char
*GetTrailingPathComponents(const char* path,
                                      int noOfComponents)
{
    /* Start from the rear */
    const char* ptr = path + strlen(path);
    int count = 0;

    while (--ptr > path) {
        if (*ptr == '/') {
            if (count++ == noOfComponents) {
                break;
            }
        }
    }
    return(ptr);
} 

/*
** Samples up to a maximum of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
** characters, to determine whether fileString represents a MS DOS or Macintosh
** format file.  If there's ANY ambiguity (a newline in the sample not paired
** with a return in an otherwise DOS looking file, or a newline appearing in
** the sampled portion of a Macintosh looking file), the file is judged to be
** Unix format.
*/
int FormatOfFile(const char *fileString)
{
    const char *p;
    int nNewlines = 0, nReturns = 0;
    
    for (p=fileString; *p!='\0' && p < fileString + FORMAT_SAMPLE_CHARS; p++) {
	if (*p == '\n') {
	    nNewlines++;
	    if (p == fileString || *(p-1) != '\r')
		return UNIX_FILE_FORMAT;
	    if (nNewlines >= FORMAT_SAMPLE_LINES)
		return DOS_FILE_FORMAT;
	} else if (*p == '\r')
	    nReturns++;
    }
    if (nNewlines > 0)
	return DOS_FILE_FORMAT;
    if (nReturns > 0)
	return MAC_FILE_FORMAT;
    return UNIX_FILE_FORMAT;
}

/*
** Converts a string (which may represent the entire contents of the file)
** from DOS or Macintosh format to Unix format.  Conversion is done in-place.
** In the DOS case, the length will be shorter, and passed length will be
** modified to reflect the new length. The routine has support for blockwise
** file to string conversion: if the fileString has a trailing '\r' and 
** 'pendingCR' is not zero, the '\r' is deposited in there and is not
** converted. If there is no trailing '\r', a 0 is deposited in 'pendingCR'
** It's the caller's responsability to make sure that the pending character, 
** if present, is inserted at the beginning of the next block to convert.
*/
void ConvertFromDosFileString(char *fileString, int *length, 
    char* pendingCR)
{
    char *outPtr = fileString;
    char *inPtr = fileString;
    if (pendingCR) *pendingCR = 0;
    while (inPtr < fileString + *length) {
    	if (*inPtr == '\r') {
	    if (inPtr < fileString + *length - 1) {
		if (*(inPtr + 1) == '\n')
		    inPtr++;
	    } else {
		if (pendingCR) {
		    *pendingCR = *inPtr;
		    break; /* Don't copy this trailing '\r' */
		}
	    }
	}
	*outPtr++ = *inPtr++;
    }
    *outPtr = '\0';
    *length = outPtr - fileString;
}
void ConvertFromMacFileString(char *fileString, int length)
{
    char *inPtr = fileString;
    while (inPtr < fileString + length) {
        if (*inPtr == '\r' )
            *inPtr = '\n';
        inPtr++;
    }
}

/*
** Converts a string (which may represent the entire contents of the file) from
** Unix to DOS format.  String is re-allocated (with malloc), and length is
** modified.  If allocation fails, which it may, because this can potentially
** be a huge hunk of memory, returns FALSE and no conversion is done.
**
** This could be done more efficiently by asking doSave to allocate some
** extra memory for this, and only re-allocating if it wasn't enough.  If
** anyone cares about the performance or the potential for running out of
** memory on a save, it should probably be redone.
*/
int ConvertToDosFileString(char **fileString, int *length)
{
    char *outPtr, *outString;
    char *inPtr = *fileString;
    int inLength = *length;
    int outLength = 0;

    /* How long a string will we need? */
    while (inPtr < *fileString + inLength) {
    	if (*inPtr == '\n')
	    outLength++;
	inPtr++;
	outLength++;
    }
    
    /* Allocate the new string */
    outString = (char*)NEditMalloc(outLength + 1);
    if (outString == NULL)
	return FALSE;
    
    /* Do the conversion, free the old string */
    inPtr = *fileString;
    outPtr = outString;
    while (inPtr < *fileString + inLength) {
    	if (*inPtr == '\n')
	    *outPtr++ = '\r';
	*outPtr++ = *inPtr++;
    }
    *outPtr = '\0';
    NEditFree(*fileString);
    *fileString = outString;
    *length = outLength;
    return TRUE;
}

/*
** Converts a string (which may represent the entire contents of the file)
** from Unix to Macintosh format.
*/
void ConvertToMacFileString(char *fileString, int length)
{
    char *inPtr = fileString;
    
    while (inPtr < fileString + length) {
	if (*inPtr == '\n' )
            *inPtr = '\r';
	inPtr++;
    }
}

/*
** Reads a text file into a string buffer, converting line breaks to 
** unix-style if appropriate. 
**
** Force a terminating \n, if this is requested
*/
char *ReadAnyTextFile(const char *fileName, int forceNL)
{
    struct stat statbuf;
    FILE *fp;
    int fileLen, readLen;
    char *fileString;
    int format;
            
    /* Read the whole file into fileString */
    if ((fp = fopen(fileName, "r")) == NULL) {
      return NULL;
    }
    if (fstat(fileno(fp), &statbuf) != 0) {
      fclose(fp);
      return NULL;
    }
    fileLen = statbuf.st_size;
    /* +1 = space for null
    ** +1 = possible additional \n
    */
    fileString = (char*)NEditMalloc(fileLen + 2);
    readLen = fread(fileString, sizeof(char), fileLen, fp);
    if (ferror(fp)) {
      NEditFree(fileString);
      fclose(fp);
      return NULL;
    }
    fclose(fp);
    fileString[readLen] = 0;

    /* Convert linebreaks? */
    format = FormatOfFile(fileString);
    if (format == DOS_FILE_FORMAT){
        char pendingCR;
        ConvertFromDosFileString(fileString, &readLen, &pendingCR);
    } else if (format == MAC_FILE_FORMAT){
        ConvertFromMacFileString(fileString, readLen);
    }

    /* now, that the fileString is in Unix format, check for terminating \n */
    if (forceNL && fileString[readLen - 1] != '\n') {
        fileString[readLen]     = '\n';
        fileString[readLen + 1] = '\0';
    }

    return fileString;
}
