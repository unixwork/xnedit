/*******************************************************************************
*                                                                              *
* help_topic.h --  Nirvana Editor help display                                 *
*                                                                              *
                 Generated on Jan 14, 2017 (Do NOT edit!)
                 Source of content from file help.etx
*                                                                              *
* Copyright (c) 1999-2017 Mark Edel                                            *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version.                                                                     *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* In addition, as a special exception to the GNU GPL, the copyright holders    *
* give permission to link the code of this program with the Motif and Open     *
* Motif libraries (or with modified versions of these that use the same        *
* license), and distribute linked combinations including the two. You must     *
* obey the GNU General Public License in all respects for all of the code used *
* other than linking with Motif/Open Motif. If you modify this file, you may   *
* extend this exception to your version of the file, but you are not obligated *
* to do so. If you do not wish to do so, delete this exception statement from  *
* your version.                                                                *
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

#define MAX_HEADING   3
#define STL_HD        16+1
#define STL_LINK      16
#define STL_NM_HEADER 'R'
#define STL_NM_LINK   'Q'
#define STYLE_MARKER  '\01'
#define STYLE_PLAIN   'A'
#define TKN_LIST_SIZE 4

enum HelpTopic {
    HELP_START,
    HELP_SELECT,
    HELP_SEARCH,
    HELP_CLIPBOARD,
    HELP_MOUSE,
    HELP_KEYBOARD,
    HELP_FILL,
    HELP_INTERFACE,
    HELP_FORMAT,
    HELP_PROGRAMMER,
    HELP_TABS,
    HELP_INDENT,
    HELP_SYNTAX,
    HELP_TAGS,
    HELP_CALLTIPS,
    HELP_BASICSYNTAX,
    HELP_ESCAPESEQUENCES,
    HELP_PARENCONSTRUCTS,
    HELP_ADVANCEDTOPICS,
    HELP_EXAMPLES,
    HELP_SHELL,
    HELP_LEARN,
    HELP_MACRO_LANG,
    HELP_MACRO_SUBRS,
    HELP_RANGESET,
    HELP_HILITEINFO,
    HELP_ACTIONS,
    HELP_CUSTOMIZE,
    HELP_PREFERENCES,
    HELP_RESOURCES,
    HELP_BINDING,
    HELP_PATTERNS,
    HELP_SMART_INDENT,
    HELP_COMMAND_LINE,
    HELP_SERVER,
    HELP_RECOVERY,
    HELP_VERSION,
    HELP_DISTRIBUTION,
    HELP_MAILING_LIST,
    HELP_DEFECTS,
    HELP_TABS_DIALOG,
    HELP_CUSTOM_TITLE_DIALOG,
    HELP_LAST_ENTRY,
    HELP_none = 0x7fffffff  /* Illegal topic */ 
};

#define NUM_TOPICS HELP_LAST_ENTRY

