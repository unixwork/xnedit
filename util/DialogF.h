/*******************************************************************************
*                                                                              *
* DialogF.h -- Nirvana Editor Dialog Header File                               *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
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

#ifndef NEDIT_DIALOGF_H_INCLUDED
#define NEDIT_DIALOGF_H_INCLUDED

#include <X11/Intrinsic.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#define DF_ERR 1        /* Error Dialog       */
#define DF_INF 2        /* Information Dialog */
#define DF_MSG 3        /* Message Dialog     */
#define DF_QUES 4       /* Question Dialog    */
#define DF_WARN 5       /* Warning Dialog     */
#define DF_PROMPT 6     /* Prompt Dialog      */

/* longest message length supported. Note that dialogs may contains a file 
   name, which is only limited in length by MAXPATHLEN, so DF_MAX_MSG_LENGTH
   must be sufficiently larger than MAXPATHLEN. */
#define DF_MAX_MSG_LENGTH (2047 + MAXPATHLEN) 
#define DF_MAX_PROMPT_LENGTH 255            /* longest prompt string supported  */

unsigned DialogF(int dialog_type, Widget parent, unsigned n, const char* title,
        const char* msgstr, ...);                    /* variable # arguments */
void SetDialogFPromptHistory(char **historyList, int nItems);

#endif /* NEDIT_DIALOGF_H_INCLUDED */
