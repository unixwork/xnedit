/*******************************************************************************
*                                                                              *
* help.h --  Nirvana Editor Help Display                                       *
*                                                                              *
* Copyright (c) 1999-2001 Mark Edel                                            *
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
* September 10, 1991                                                           *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#ifndef NEDIT_HELP_H_INCLUDED
#define NEDIT_HELP_H_INCLUDED

#include "help_topic.h"

#include <X11/Intrinsic.h>

/*============================================================================*/
/*                          VARIABLE TYPE DEFINITIONS                         */
/*============================================================================*/

typedef struct HelpMenu         /* Maintains help menu structure */
{
    struct HelpMenu * next;
    int               level;    /* menu level, submenu > 1               */
    enum HelpTopic    topic;    /* HELP_none for submenu & separator     */
    char            * wgtName;
    int               hideIt;   /* value which determines displayability */
    char              mnemonic; /* '-' for separator                     */
    char            * subTitle; /* title for sub menu, or NULL           */
} HelpMenu;

typedef struct Href             /* Source to topic internal hyperlinks */
{
    struct Href *  next;
    int            location;    /* position to link in topic    */
    enum HelpTopic topic;       /* target of link in this topic */
    char *         source;      /* hypertext link characters    */
} Href;

/*============================================================================*/
/*                             VARIABLE DECLARATIONS                          */
/*============================================================================*/

extern HelpMenu H_M[];
extern char *HelpTitles[];
extern const char linkdate[];
extern const char linktime[];

/*============================================================================*/
/*                             PROGRAM PROTOTYPES                             */
/*============================================================================*/

void Help(enum HelpTopic topic);
void PrintVersion(void);
void InstallHelpLinkActions(XtAppContext context);

#endif /* NEDIT_HELP_H_INCLUDED */
