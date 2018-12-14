/*******************************************************************************
*                                                                              *
* nedit_malloc.c -- Nirvana Editor memory handling                             *
*                                                                              *
* Copyright (C) 2015 Ivan Skytte Joergensen				       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
* 									       *
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
* April, 1997								       *
*									       *
* Written by Mark Edel  						       *
*									       *
*******************************************************************************/

#include "nedit_malloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* The reason for these routines are that we have a single place to handle
 * out-of-memory conditions. Also memory returned by Xt* is freed with NEditFree
 * (investigation back to X11R4 reveals that XtMalloc ultimately called plain
 * malloc(). Also, XtMalloc() and friends take a 'Cardinal' size which is
 * always 32-bit preventing 4GB+ file support
 */

void *NEditMalloc(size_t size)
{
    void *ptr = malloc(size);
    if(!ptr) {
        fprintf(stderr,"NEditMalloc(%lu) failed\n", (unsigned long)size);
        exit(1);
    }
    return ptr;
}


void *NEditCalloc(size_t nmemb, size_t size)
{
    void *ptr = NEditMalloc(nmemb*size);
    memset(ptr,0,nmemb*size);
    return ptr;
}


void *NEditRealloc(void *ptr, size_t new_size)
{
    void *new_ptr = realloc(ptr,new_size);
    if(!new_ptr && new_size) {
        fprintf(stderr,"NEditRealloc(%lu) failed\n", (unsigned long)new_size);
        exit(1);
    }
    return new_ptr;
}


void NEditFree(void *ptr)
{
    free(ptr);
}


char *NEditStrdup(const char *str)
{
    size_t len;
    if(!str)
        return NULL;
    len = strlen(str);
    char *new_str= (char*)malloc(len+1);
    if(!new_str) {
        fprintf(stderr,"NEditStrdup(%lu) failed\n", (unsigned long)len);
        exit(1);
    }
    memcpy(new_str,str,len+1);
    return new_str;
}
