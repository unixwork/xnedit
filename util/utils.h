/*******************************************************************************
*                                                                              *
* utils.h -- Nirvana Editor Utilities Header File                              *
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

#ifndef NEDIT_UTILS_H_INCLUDED
#define NEDIT_UTILS_H_INCLUDED

#include <sys/utsname.h>
#include <sys/types.h>

#ifdef VMS
#include "vmsparam.h"
#else
#include <sys/param.h>
#endif /*VMS*/

const char *GetCurrentDir(void);
const char *GetHomeDir(void);
char *PrependHome(const char *filename, char *buf, size_t buflen);
const char *GetUserName(void);
const char *GetNameOfHost(void);
int Min(int i1, int i2);
const char* GetRCFileName(int type);

/*
**  Simple stack implementation which only keeps void pointers.
**  The stack must already be allocated and initialised:
**
**  Stack* stack = (Stack*) XtMalloc(sizeof(Stack));
**  stack->top = NULL;
**  stack->size = 0;
**
**  NULL is not allowed to pass in, as it is used to signal an empty stack.
**
**  The user should only ever care about Stack, stackObject is an internal
**  object. (So it should really go in utils.c. A forward reference was
**  refused by my compiler for some reason though.)
*/
typedef struct _stackObject {
    void* value;
    struct _stackObject* next;
} stackObject;

typedef struct {
    unsigned size;
    stackObject* top;
} Stack;

void Push(Stack* stack, const void* value);
void* Pop(Stack* stack);

/* N_FILE_TYPES must be the last entry!! This saves us from counting. */
enum {NEDIT_RC, AUTOLOAD_NM, NEDIT_HISTORY, N_FILE_TYPES};

/* If anyone knows where to get this from system include files (in a machine
   independent way), please change this (L_cuserid is apparently not ANSI) */
#define MAXUSERNAMELEN 32

/* Ditto for the maximum length for a node name.  SYS_NMLN is not available
   on most systems, and I don't know what the portable alternative is. */
#ifdef SYS_NMLN
#define MAXNODENAMELEN SYS_NMLN
#else
#define MAXNODENAMELEN (MAXPATHLEN+2)
#endif

#endif /* NEDIT_UTILS_H_INCLUDED */
