/*******************************************************************************
*                                                                              *
* motif.c:  Determine stability of Motif                                       *
*                                                                              *
* Copyright (C) 2003 Nathaniel Gray                                            *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
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
* July 28, 1992                                                                *
*                                                                              *
* Written by Nathaniel Gray                                                    *
*                                                                              *
* Modifications by:                                                            *
* Scott Tringali                                                               *
*                                                                              *
*******************************************************************************/

/*
 * About the different #defines that Motif gives us:
 * All Motifs #define several values.  These are the values in
 * Open Motif 2.1.30, for example:
 *     #define XmVERSION       2
 *     #define XmREVISION      1
 *     #define XmUPDATE_LEVEL  30
 *     #define XmVersion       (XmVERSION * 1000 + XmREVISION)
 *     #define XmVERSION_STRING "@(#)Motif Version 2.1.30"
 * 
 * In addition, LessTif #defines several values as shown here for
 * version 0.93.0:
 *     #define LESSTIF_VERSION  0
 *     #define LESSTIF_REVISION 93
 *     #define LesstifVersion   (LESSTIF_VERSION * 1000 + LESSTIF_REVISION)
 *     #define LesstifVERSION_STRING \
 *             "@(#)GNU/LessTif Version 2.1 Release 0.93.0"
 *
 * Also, in LessTif the XmVERSION_STRING is identical to the 
 * LesstifVERSION_STRING.  Unfortunately, the only way to find out the
 * "update level" of a LessTif release is to parse the LesstifVERSION_STRING.
 */

#include "motif.h"
#include <Xm/Xm.h>
#include <string.h>

#ifdef LESSTIF_VERSION
static enum MotifStability GetLessTifStability(void);
#else
static enum MotifStability GetOpenMotifStability(void);
#endif

/* 
 * These are versions of LessTif that are known to be stable with NEdit in
 * Motif 2.1 mode.
 */
static const char *const knownGoodLesstif[] = {
    "0.92.32",
    "0.93.0",
    "0.93.12",
    "0.93.18",
#ifndef __x86_64
    "0.93.94",    /* 64-bit build .93.94 is broken */
#endif
    NULL
};

/* 
 * These are versions of LessTif that are known NOT to be stable with NEdit in
 * Motif 2.1 mode.
 */
const char *const knownBadLessTif[] = {
    "0.93.25",
    "0.93.29",
    "0.93.34"
    "0.93.36",
    "0.93.39",
    "0.93.40",
    "0.93.41",
    "0.93.44",
#ifdef __x86_64
    "0.93.94",    /* 64-bit build .93.94 is broken */
#endif
    "0.93.95b",   /* SF bug 1087192 */
    "0.94.4",     /* Alt-H, ESC => crash */
    "0.95.0",     /* same as above */
    NULL
};


#ifdef LESSTIF_VERSION

static enum MotifStability GetLessTifStability(void)
{
    int i;
    const char *rev = NULL;
    
    /* We assume that the lesstif version is the string after the last
        space. */

    rev = strrchr(LesstifVERSION_STRING, ' ');

    if (rev == NULL)
        return MotifUnknown;

    rev += 1;

    /* Check for known good LessTif versions */
    for (i = 0; knownGoodLesstif[i]; i++)
        if (!strcmp(rev, knownGoodLesstif[i]))
            return MotifKnownGood;

    /* Check for known bad LessTif versions */
    for (i = 0; knownBadLessTif[i]; i++) 
        if (!strcmp(rev, knownBadLessTif[i]))
            return MotifKnownBad;
    
    return MotifUnknown;
}

#else

/* The stability depends on the patch level, so fold it into the
   usual XmVersion for easy comparison. */
static const int XmFullVersion = (XmVersion * 100 + XmUPDATE_LEVEL);

static enum MotifStability GetOpenMotifStability(void)
{
    enum MotifStability result = MotifUnknown;

    const Boolean really222 = 
        (strcmp("@(#)Motif Version 2.2.2", XmVERSION_STRING) == 0);
    
    if (XmFullVersion <= 200200)         /* 1.0 - 2.1 are fine */
    {
        result = MotifKnownGood;
    }
    else if ((XmFullVersion < 200202) || really222)  /* 2.2.0 - 2.2.2 are bad */
    {
        result = MotifKnownBad;
    }
    else if (XmFullVersion >= 200203 && XmFullVersion <= 200304) /* 2.2.3 - 2.3 is good */
    {
        result = MotifKnownGood;
    }
    else                            /* Anything else unknown */
    {
        result = MotifUnknown;
    }
        
    return result;
}

#endif


enum MotifStability GetMotifStability(void)
{
#ifdef LESSTIF_VERSION
    return GetLessTifStability();
#else 
    return GetOpenMotifStability();
#endif
}


const char *GetMotifStableVersions(void)
{
    int i;
    static char msg[sizeof knownGoodLesstif * 80];
    
    for (i = 0; knownGoodLesstif[i] != NULL; i++)
    {
        strcat(msg, knownGoodLesstif[i]);
        strcat(msg, "\n");
    }

    strcat(msg, "OpenMotif 2.1.30\n");
    strcat(msg, "OpenMotif 2.2.3\n");
    strcat(msg, "OpenMotif 2.3\n");
    strcat(msg, "OpenMotif 2.3.4\n");

    return msg;
}
