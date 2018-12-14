/*******************************************************************************
*                                                                              *
* refString.c -- Nirvana editor string handling                                *
*                                                                              *
* Copyright (C) 200 Scott Tringali                                             *
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
* July, 1993                                                                   *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "refString.h"
#include "nedit_malloc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define RCS_SIZE 0x10000

struct rcs;

struct rcs_stats
{
    int talloc, tshar, tgiveup, tbytes, tbyteshared;
};

struct rcs
{
    struct rcs *next;
    char       *string;
    int         usage;
};

static struct rcs       *Rcs[RCS_SIZE];
static struct rcs_stats  RcsStats;

static unsigned const DJB2_SEED = 5381u;
/* djb2s hash for null-terminated string, seeded version */
static unsigned djb2s(char const* key, unsigned hash)
{
    char c;
    while (!!(c = *key++))
        hash = ((hash << 5) + hash) ^ c;
    return hash;
}

/* Compute hash address from a string key */
unsigned StringHashAddr(const char *key)
{
    return djb2s(key, DJB2_SEED);
}

/* Compute hash address from a null-termintated list of strings */
unsigned StringsHashAddr(const char** keys)
{
    unsigned hash = DJB2_SEED;
    char const* key;
    while (!!(key = *keys++))
        hash = djb2s(key, hash);
    return hash;
}

/*
** Take a normal string, create a shared string from it if need be,
** and return pointer to that shared string.
**
** Returned strings are const because they are shared.  Do not modify them!
*/

const char *RefStringDup(const char *str)
{
    unsigned bucket;
    size_t len;
    struct rcs *rp;
    char *newstr;

    if (str == NULL)
        return NULL;

    len = strlen(str);

    RcsStats.talloc++;

    /* Find it in hash */
    bucket = StringHashAddr(str) % RCS_SIZE;
    rp = Rcs[bucket];
    for (; rp; rp = rp->next)
        if (!strcmp(str, rp->string)) break;

    newstr = NULL;
    if (rp)  /* It exists, return it and bump ref ct */
    {
        rp->usage++;
        newstr = rp->string;

        RcsStats.tshar++;
        RcsStats.tbyteshared += len;
    }
    else     /* Doesn't exist, conjure up a new one. */
    {
        rp = NEditNew(struct rcs);
        rp->usage = 1;
        rp->next = Rcs[bucket];
        Rcs[bucket] = rp;

        rp->string = (char*) NEditMalloc(len + 1);
        memcpy(rp->string, str, len + 1);

        newstr = rp->string;
    }

    RcsStats.tbytes += len;
    return newstr;
}

/*
** Decrease the reference count on a shared string.  When the reference
** count reaches zero, free the master string.
*/

void RefStringFree(const char *rcs_str)
{
    int bucket;
    struct rcs *rp;
    struct rcs *prev = NULL;

    if (rcs_str == NULL)
        return;
        
    bucket = StringHashAddr(rcs_str) % RCS_SIZE;

    /* find it in hash */
    for (rp = Rcs[bucket]; rp; rp = rp->next)
    {
        if (rcs_str == rp->string)
            break;
        prev = rp;
    }

    if (rp)  /* It's a shared string, decrease ref count */
    {
        rp->usage--;
        
        if (rp->usage < 0) /* D'OH! */
        {
            fprintf(stderr, "NEdit: internal error deallocating shared string.");
            return;
        }

        if (rp->usage == 0)  /* Last one- free the storage */
        {
            NEditFree(rp->string);
            if (prev)
                prev->next = rp->next;
            else
                Rcs[bucket] = rp->next;
            NEditFree(rp);
        }
    }
    else    /* Doesn't appear to be a shared string */
    {
        fprintf(stderr, "NEdit: attempt to free a non-shared string.");
        return;
    }
}

