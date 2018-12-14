/*******************************************************************************
*									       *
* rangeset.c	 -- Nirvana Editor rangest functions			       *
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
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or	       *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License	       *
* for more details.							       *
*									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA	02111-1307 USA				       *
*									       *
* Nirvana Text Editor							       *
* Sep 26, 2002								       *
*									       *
* Written by Tony Balinski with contributions from Andrew Hood		       *
*									       *
* Modifications:							       *
*									       *
*									       *
*******************************************************************************/
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "textBuf.h"
#include "textDisp.h"
#include "rangeset.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* -------------------------------------------------------------------------- */

struct _Range {
    int start, end;			/* range from [start-]end */
};

typedef Rangeset *RangesetUpdateFn(Rangeset *p, int pos, int ins, int del);

struct _Rangeset {
    RangesetUpdateFn *update_fn;	/* modification update function */
    char *update_name;			/* update function name */
    int maxpos;				/* text buffer maxpos */
    int last_index;			/* a place to start looking */
    int n_ranges;			/* how many ranges in ranges */
    Range *ranges;			/* the ranges table */
    unsigned char label;		/* a number 1-63 */

    signed char color_set;              /* 0: unset; 1: set; -1: invalid */
    char *color_name;			/* the name of an assigned color */
    Pixel color;			/* the value of a particular color */
    textBuffer *buf;			/* the text buffer of the rangeset */
    char *name;                         /* name of rangeset */
};

struct _RangesetTable {
    int n_set;				/* how many sets are active */
    textBuffer *buf;			/* the text buffer of the rangeset */
    Rangeset set[N_RANGESETS];		/* the rangeset table */
    unsigned char order[N_RANGESETS];	/* inds of set[]s ordered by depth */
    unsigned char active[N_RANGESETS];	/* entry true if corresp. set active */
    unsigned char depth[N_RANGESETS];	/* depth[i]: pos of set[i] in order[] */
    unsigned char list[N_RANGESETS + 1];/* string of labels in depth order */
};

/* -------------------------------------------------------------------------- */

#define SWAPval(T,a,b) { T t; t = *(a); *(a) = *(b); *(b) = t; }

static unsigned char rangeset_labels[N_RANGESETS + 1] = {
      58, 10, 15,  1, 27, 52, 14,  3, 61, 13, 31, 30, 45, 28, 41, 55, 
      33, 20, 62, 34, 42, 18, 57, 47, 24, 49, 19, 50, 25, 38, 40,  2, 
      21, 39, 59, 22, 60,  4,  6, 16, 29, 37, 48, 46, 54, 43, 32, 56, 
      51,  7,  9, 63,  5,  8, 36, 44, 26, 11, 23, 17, 53, 35, 12, 0
   };

/* -------------------------------------------------------------------------- */

static RangesetUpdateFn rangesetInsDelMaintain;
static RangesetUpdateFn rangesetInclMaintain;
static RangesetUpdateFn rangesetDelInsMaintain;
static RangesetUpdateFn rangesetExclMaintain;
static RangesetUpdateFn rangesetBreakMaintain;

#define DEFAULT_UPDATE_FN_NAME	"maintain"

static struct {
    char *name;
    RangesetUpdateFn *update_fn;
} RangesetUpdateMap[] = {
    {DEFAULT_UPDATE_FN_NAME,	rangesetInsDelMaintain},
    {"ins_del",			rangesetInsDelMaintain},
    {"include",			rangesetInclMaintain},
    {"del_ins",			rangesetDelInsMaintain},
    {"exclude",			rangesetExclMaintain},
    {"break",			rangesetBreakMaintain},
    {(char *)0,			(RangesetUpdateFn *)0}
};

/* -------------------------------------------------------------------------- */

static Range *RangesNew(int n)
{
    Range *newRanges;

    if (n != 0) {
	/* We use a blocked allocation scheme here, with a block size of factor.
	   Only allocations of multiples of factor will be allowed.
	   Be sure to allocate at least one more than we really need, and
	   round up to next higher multiple of factor, ie
		n = (((n + 1) + factor - 1) / factor) * factor
	   If we choose factor = (1 << factor_bits), we can use shifts
	   instead of multiply/divide, ie
	        n = ((n + (1 << factor_bits)) >> factor_bits) << factor_bits
	   or
		n = (1 + (n >> factor_bits)) << factor_bits
	   Since the shifts just strip the end 1 bits, we can even get away
	   with
	   	n = ((1 << factor_bits) + n) & (~0 << factor_bits);
	   Finally, we decide on factor_bits according to the size of n:
	   if n >= 256, we probably want less reallocation on growth than
	   otherwise; choose some arbitrary values thus:
		factor_bits = (n >= 256) ? 6 : 4
	   so
	   	n = (n >= 256) ? (n + (1<<6)) & (~0<<6) : (n + (1<<4)) & (~0<<4)
	   or
	   	n = (n >= 256) ? ((n + 64) & ~63) : ((n + 16) & ~15)
	 */
	n = (n >= 256) ? ((n + 64) & ~63) : ((n + 16) & ~15);
	newRanges = (Range *)NEditMalloc(n * sizeof (Range));
	return newRanges;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static Range* RangesRealloc(Range* ranges, int n)
{
    int size;
    Range* newRanges;

    if (n > 0)
    {
        /* see RangesNew() for comments */
        n = (n >= 256) ? ((n + 64) & ~63) : ((n + 16) & ~15);
        size = n * sizeof (Range);
        newRanges = (Range*)NEditRealloc(ranges,size);
        return newRanges;
    } else {
        NEditFree(ranges);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static Range *RangesFree(Range *ranges)
{
    NEditFree(ranges);

    return NULL;
}

/* -------------------------------------------------------------------------- */

/*
** Refresh the given range on the screen. If the range indicated is null, we
** refresh the screen for the whole file.
*/

void RangesetRefreshRange(Rangeset *rangeset, int start, int end)
{
    if (rangeset->buf != NULL)
	BufCheckDisplay(rangeset->buf, start, end);
}

static void rangesetRefreshAllRanges(Rangeset *rangeset)
{
    int i;

    for (i = 0; i < rangeset->n_ranges; i++)
	RangesetRefreshRange(rangeset, rangeset->ranges[i].start, rangeset->ranges[i].end);
}

/* -------------------------------------------------------------------------- */

/*
** Remove all ranges from a range set.
*/

void RangesetEmpty(Rangeset *rangeset)
{
    Range *ranges = rangeset->ranges;
    int start, end;

    if (rangeset->color_name && rangeset->color_set > 0) {
	/* this range is colored: we need to clear it */
	rangeset->color_set = -1;

	while (rangeset->n_ranges--) {
	    start = ranges[rangeset->n_ranges].start;
	    end = ranges[rangeset->n_ranges].end;
	    RangesetRefreshRange(rangeset, start, end);
	}
    }

    NEditFree(rangeset->color_name);
    NEditFree(rangeset->name);

    rangeset->color_name = (char *)0;
    rangeset->name = (char *)0;
    rangeset->ranges = RangesFree(rangeset->ranges);
}

/* -------------------------------------------------------------------------- */

/*
** Initialise a new range set.
*/

void RangesetInit(Rangeset *rangeset, int label, textBuffer *buf)
{
    rangeset->label = (unsigned char)label;     /* a letter A-Z */
    rangeset->maxpos = 0;			/* text buffer maxpos */
    rangeset->last_index = 0;			/* a place to start looking */
    rangeset->n_ranges = 0;			/* how many ranges in ranges */
    rangeset->ranges = (Range *)0;		/* the ranges table */

    rangeset->color_name = (char *)0;
    rangeset->name = (char *)0;
    rangeset->color_set = 0;
    rangeset->buf = buf;

    rangeset->maxpos = buf->length;

    RangesetChangeModifyResponse(rangeset, DEFAULT_UPDATE_FN_NAME);
}

/*
** Change a range set's modification behaviour. Returns true (non-zero)
** if the update function name was found, else false.
*/

int RangesetChangeModifyResponse(Rangeset *rangeset, char *name)
{
    int i;

    if (name == NULL)
	name = DEFAULT_UPDATE_FN_NAME;

    for (i = 0; RangesetUpdateMap[i].name; i++)
	if (strcmp(RangesetUpdateMap[i].name, name) == 0) {
	    rangeset->update_fn = RangesetUpdateMap[i].update_fn;
	    rangeset->update_name = RangesetUpdateMap[i].name;
	    return 1;
	}

    return 0;
}

/* -------------------------------------------------------------------------- */

/*
** Find the index of the first integer in table greater than or equal to pos.
** Fails with len (the total number of entries). The start index base can be
** chosen appropriately.
*/

static int at_or_before(int *table, int base, int len, int val)
{
    int lo, mid = 0, hi;

    if (base >= len)
	return len;		/* not sure what this means! */

    lo = base;			/* first valid index */
    hi = len - 1;		/* last valid index */

    while (lo <= hi) {
	mid = (lo + hi) / 2;
	if (val == table[mid])
	    return mid;
	if (val < table[mid])
	    hi = mid - 1;
	else
	    lo = mid + 1;
    }
    /* if we get here, we didn't find val itself */
    if (val > table[mid])
	mid++;

    return mid;
}

static int weighted_at_or_before(int *table, int base, int len, int val)
{
    int lo, mid = 0, hi;
    int min, max;

    if (base >= len)
	return len;		/* not sure what this means! */

    lo = base;			/* first valid index */
    hi = len - 1;		/* last valid index */

    min = table[lo];		/* establish initial min/max */
    max = table[hi];

    if (val <= min)		/* initial range checks */
	return lo;		/* needed to avoid out-of-range mid values */
    else if (val > max)
	return len;
    else if (val == max)
	return hi;

    while (lo <= hi) {
        /* Beware of integer overflow when multiplying large numbers! */
        mid = lo + (int)((hi - lo) * (double)(val - min) / (max - min));
	/* we won't worry about min == max - values should be unique */

	if (val == table[mid])
	    return mid;
	if (val < table[mid]) {
	    hi = mid - 1;
	    max = table[mid];
	}
	else { /* val > table[mid] */
	    lo = mid + 1;
	    min = table[mid];
	}
    }

    /* if we get here, we didn't find val itself */
    if (val > table[mid])
	return mid + 1;

    return mid;
}

/* -------------------------------------------------------------------------- */

/*
** Find out whether the position pos is included in one of the ranges of 
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Note: ranges are indexed from zero. 
*/

int RangesetFindRangeNo(Rangeset *rangeset, int index, int *start, int *end)
{
    if (!rangeset || index < 0 || rangeset->n_ranges <= index || !rangeset->ranges)
	return 0;

    *start = rangeset->ranges[index].start;
    *end   = rangeset->ranges[index].end;

    return 1;
}

/*
** Find out whether the position pos is included in one of the ranges of
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Note: ranges are indexed from zero.
*/

int RangesetFindRangeOfPos(Rangeset *rangeset, int pos, int incl_end)
{
    int *ranges;
    int len, ind;

    if (!rangeset || !rangeset->n_ranges || !rangeset->ranges)
	return -1;

    ranges = (int *)rangeset->ranges;		/* { s1,e1, s2,e2, s3,e3,... } */
    len = rangeset->n_ranges * 2;
    ind = at_or_before(ranges, 0, len, pos);
    
    if (ind == len)
	return -1;			/* beyond end */

    if (ind & 1) {			/* ind odd: references an end marker */
	if (pos < ranges[ind] || (incl_end && pos == ranges[ind]))
	    return ind / 2;		/* return the range index */
    }
    else {				/* ind even: references start marker */
	if (pos == ranges[ind])
	    return ind / 2;		/* return the range index */
    }
    return -1;				/* not in any range */
}

/*
** Find out whether the position pos is included in one of the ranges of
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Essentially the same as the RangesetFindRangeOfPos() function, but uses the
** last_index member of the rangeset and weighted_at_or_before() for speedy
** lookup in refresh tasks. The rangeset is assumed to be valid, as is the
** position. We also don't allow checking of the endpoint.
** Returns the including range index, or -1 if not found.
*/

int RangesetCheckRangeOfPos(Rangeset *rangeset, int pos)
{
    int *ranges;
    int len, index, last;

    len = rangeset->n_ranges;
    if (len == 0)
	return -1;			/* no ranges */

    ranges = (int *)rangeset->ranges;		/* { s1,e1, s2,e2, s3,e3,... } */
    last = rangeset->last_index;

    /* try to profit from the last lookup by using its index */
    if (last >= len || last < 0) {
	last = (len > 0) ? len - 1 : 0;	/* make sure last is in range */
	rangeset->last_index = last;
    }

    len *= 2;
    last *= 2;

    if (pos >= ranges[last]) {		/* last even: this is a start */
	if (pos < ranges[last + 1])	/* checking an end here */
	    return last / 2;		/* no need to change rangeset->last_index */
	else
	    last += 2;			/* not in this range: move on */

	if (last == len)
	    return -1;			/* moved on too far */

	/* find the entry in the upper portion of ranges */
	index = weighted_at_or_before(ranges, last, len, pos); /* search end only */
    }
    else if (last > 0) {
	index = weighted_at_or_before(ranges, 0, last, pos); /* search front only */
    }
    else
	index = 0;

    rangeset->last_index = index / 2;

    if (index == len)
	return -1;			/* beyond end */

    if (index & 1) {			/* index odd: references an end marker */
	if (pos < ranges[index])
	    return index / 2;		/* return the range index */
    }
    else {				/* index even: references start marker */
	if (pos == ranges[index])
	    return index / 2;		/* return the range index */
    }
    return -1;				/* not in any range */
}

/* -------------------------------------------------------------------------- */

/*
** Merge the ranges in rangeset plusSet into rangeset origSet.
*/

int RangesetAdd(Rangeset *origSet, Rangeset *plusSet)
{
    Range *origRanges, *plusRanges, *newRanges, *oldRanges;
    int nOrigRanges, nPlusRanges;
    int isOld;

    origRanges = origSet->ranges;
    nOrigRanges = origSet->n_ranges;

    plusRanges = plusSet->ranges;
    nPlusRanges = plusSet->n_ranges;

    if (nPlusRanges == 0)
	return nOrigRanges;	/* no ranges in plusSet - nothing to do */

    newRanges = RangesNew(nOrigRanges + nPlusRanges);

    if (nOrigRanges == 0) {
	/* no ranges in destination: just copy the ranges from the other set */
	memcpy(newRanges, plusRanges, nPlusRanges * sizeof (Range));
	RangesFree(origSet->ranges);
	origSet->ranges = newRanges;
	origSet->n_ranges = nPlusRanges;
	for (nOrigRanges = 0; nOrigRanges < nPlusRanges; nOrigRanges++) {
	    RangesetRefreshRange(origSet, newRanges->start, newRanges->end);
	    newRanges++;
	}
	return nPlusRanges;
    }

    oldRanges = origRanges;
    origSet->ranges = newRanges;
    origSet->n_ranges = 0;

    /* in the following we merrily swap the pointers/counters of the two input
       ranges (from origSet and plusSet) - don't worry, they're both consulted 
       read-only - building the merged set in newRanges */

    isOld = 1;		/* true if origRanges points to a range in oldRanges[] */

    while (nOrigRanges > 0 || nPlusRanges > 0) {
	/* make the range with the lowest start value the origRanges range */
	if (nOrigRanges == 0 || 
              (nPlusRanges > 0 && origRanges->start > plusRanges->start)) {
	    SWAPval(Range *, &origRanges, &plusRanges);
	    SWAPval(int, &nOrigRanges, &nPlusRanges);
	    isOld = !isOld;
	}

	origSet->n_ranges++;		/* we're using a new result range */

	*newRanges = *origRanges++;
	nOrigRanges--;
	if (!isOld)
	    RangesetRefreshRange(origSet, newRanges->start, newRanges->end);

	/* now we must cycle over plusRanges, merging in the overlapped ranges */
	while (nPlusRanges > 0 && newRanges->end >= plusRanges->start) {
	    do {
		if (newRanges->end < plusRanges->end) {
		    if (isOld)
			RangesetRefreshRange(origSet, newRanges->end, plusRanges->end);
		    newRanges->end = plusRanges->end;
		}
		plusRanges++;
		nPlusRanges--;
	    } while (nPlusRanges > 0 && newRanges->end >= plusRanges->start);

	    /* by now, newRanges->end may have extended to overlap more ranges in origRanges,
	       so swap and start again */
	    SWAPval(Range *, &origRanges, &plusRanges);
	    SWAPval(int, &nOrigRanges, &nPlusRanges);
	    isOld = !isOld;
	}

	/* OK: now *newRanges holds the result of merging all the first ranges from
	   origRanges and plusRanges - now we have a break in contiguity, so move on to the
	   next newRanges in the result */
	newRanges++;
    }

    /* finally, forget the old rangeset values, and reallocate the new ones */
    RangesFree(oldRanges);
    origSet->ranges = RangesRealloc(origSet->ranges, origSet->n_ranges);

    return origSet->n_ranges;
}


/* -------------------------------------------------------------------------- */

/*
** Subtract the ranges of minusSet from rangeset origSet.
*/

int RangesetRemove(Rangeset *origSet, Rangeset *minusSet)
{
    Range *origRanges, *minusRanges, *newRanges, *oldRanges;
    int nOrigRanges, nMinusRanges;

    origRanges = origSet->ranges;
    nOrigRanges = origSet->n_ranges;

    minusRanges = minusSet->ranges;
    nMinusRanges = minusSet->n_ranges;

    if (nOrigRanges == 0 || nMinusRanges == 0)
	return 0;		/* no ranges in origSet or minusSet - nothing to do */

    /* we must provide more space: each range in minusSet might split a range in origSet */
    newRanges = RangesNew(origSet->n_ranges + minusSet->n_ranges);

    oldRanges = origRanges;
    origSet->ranges = newRanges;
    origSet->n_ranges = 0;

    /* consider each range in origRanges - we do not change any of minusRanges's data, but we
       may change origRanges's - it will be discarded at the end */

    while (nOrigRanges > 0) {
        do {
            /* skip all minusRanges ranges strictly in front of *origRanges */
            while (nMinusRanges > 0
                    && minusRanges->end <= origRanges->start) {
                minusRanges++;      /* *minusRanges in front of *origRanges: move onto next *minusRanges */
                nMinusRanges--;
            }

            if (nMinusRanges > 0) {
                /* keep all origRanges ranges strictly in front of *minusRanges */
                while (nOrigRanges > 0
                        && origRanges->end <= minusRanges->start) {
                    *newRanges++ = *origRanges++;   /* *minusRanges beyond *origRanges: save *origRanges in *newRanges */
                    nOrigRanges--;
                    origSet->n_ranges++;
                }
            } else {
                /* no more minusRanges ranges to remove - save the rest of origRanges */
                while (nOrigRanges > 0) {
                    *newRanges++ = *origRanges++;
                    nOrigRanges--;
                    origSet->n_ranges++;
                }
            }
        } while (nMinusRanges > 0 && minusRanges->end <= origRanges->start); /* any more non-overlaps */

        /* when we get here either we're done, or we have overlap */
        if (nOrigRanges > 0) {
            if (minusRanges->start <= origRanges->start) {
                /* origRanges->start inside *minusRanges */
                if (minusRanges->end < origRanges->end) {
                    RangesetRefreshRange(origSet, origRanges->start,
                            minusRanges->end);
                    origRanges->start = minusRanges->end;  /* cut off front of original *origRanges */
                    minusRanges++;      /* dealt with this *minusRanges: move on */
                    nMinusRanges--;
                } else {
                    /* all *origRanges inside *minusRanges */
                    RangesetRefreshRange(origSet, origRanges->start,
                            origRanges->end);
                    origRanges++;       /* all of *origRanges can be skipped */
                    nOrigRanges--;
                }
            } else {
                /* minusRanges->start inside *origRanges: save front, adjust or skip rest */
                newRanges->start = origRanges->start;   /* save front of *origRanges in *newRanges */
                newRanges->end = minusRanges->start;
                newRanges++;
                origSet->n_ranges++;

                if (minusRanges->end < origRanges->end) {
                    /* all *minusRanges inside *origRanges */
                    RangesetRefreshRange(origSet, minusRanges->start,
                            minusRanges->end); 
                    origRanges->start = minusRanges->end; /* cut front of *origRanges upto end *minusRanges */
                    minusRanges++;      /* dealt with this *minusRanges: move on */
                    nMinusRanges--;
                } else {
                    /* minusRanges->end beyond *origRanges */
                    RangesetRefreshRange(origSet, minusRanges->start,
                            origRanges->end); 
                    origRanges++;       /* skip rest of *origRanges */
                    nOrigRanges--;
                }
            }
        }
    }

    /* finally, forget the old rangeset values, and reallocate the new ones */
    RangesFree(oldRanges);
    origSet->ranges = RangesRealloc(origSet->ranges, origSet->n_ranges);

    return origSet->n_ranges;
}


/* -------------------------------------------------------------------------- */

/*
** Get number of ranges in rangeset.
*/

int RangesetGetNRanges(Rangeset *rangeset)
{
    return rangeset ? rangeset->n_ranges : 0;
}


/* 
** Get information about rangeset.
*/
void RangesetGetInfo(Rangeset *rangeset, int *defined, int *label, 
        int *count, char **color, char **name, char **mode)
{
    if (rangeset == NULL) {
        *defined = False;
        *label = 0;
        *count = 0;
        *color = "";
        *name = "";
        *mode = "";
    }
    else {
        *defined = True;
        *label = (int)rangeset->label;
        *count = rangeset->n_ranges;
        *color = rangeset->color_name ? rangeset->color_name : "";
        *name = rangeset->name ? rangeset->name : "";
        *mode = rangeset->update_name;
    }
}

/* -------------------------------------------------------------------------- */

static Rangeset *rangesetFixMaxpos(Rangeset *rangeset, int ins, int del)
{
    rangeset->maxpos += ins - del;
    return rangeset;
}

/* -------------------------------------------------------------------------- */

/*
** Allocate and initialise, or empty and free a ranges set table.
*/

RangesetTable *RangesetTableAlloc(textBuffer *buffer)
{
    int i;
    RangesetTable *table = (RangesetTable *)NEditMalloc(sizeof (RangesetTable));

    if (!table)
	return table;

    table->buf = buffer;

    for (i = 0; i < N_RANGESETS; i++) {
	RangesetInit(&table->set[i], rangeset_labels[i], buffer);
	table->order[i] = (unsigned char)i;
	table->active[i] = 0;
	table->depth[i] = (unsigned char)i;
    }

    table->n_set = 0;
    table->list[0] = '\0';
    /* Range sets must be updated before the text display callbacks are
       called to avoid highlighted ranges getting out of sync. */
    BufAddHighPriorityModifyCB(buffer, RangesetBufModifiedCB, table);
    return table;
}

RangesetTable *RangesetTableFree(RangesetTable *table)
{
    int i;

    if (table) {
	BufRemoveModifyCB(table->buf, RangesetBufModifiedCB, table);
	for (i = 0; i < N_RANGESETS; i++)
	    RangesetEmpty(&table->set[i]);
	NEditFree(table);
    }
    return (RangesetTable *)0;
}

/*
** clone a ranges set.
*/
static void rangesetClone(Rangeset *destRangeset, Rangeset *srcRangeset)
{
    destRangeset->update_fn   = srcRangeset->update_fn;
    destRangeset->update_name = srcRangeset->update_name;
    destRangeset->maxpos      = srcRangeset->maxpos;
    destRangeset->last_index  = srcRangeset->last_index;
    destRangeset->n_ranges    = srcRangeset->n_ranges;
    destRangeset->color_set   = srcRangeset->color_set;
    destRangeset->color       = srcRangeset->color;

    if (srcRangeset->color_name) {
	destRangeset->color_name = (char*)NEditMalloc(strlen(srcRangeset->color_name) +1);
	strcpy(destRangeset->color_name, srcRangeset->color_name);
    }

    if (srcRangeset->name) {
	destRangeset->name = (char*)NEditMalloc(strlen(srcRangeset->name) + 1);
	strcpy(destRangeset->name, srcRangeset->name);
    }

    if (srcRangeset->ranges) {
	destRangeset->ranges = RangesNew(srcRangeset->n_ranges);
	memcpy(destRangeset->ranges, srcRangeset->ranges,
		srcRangeset->n_ranges * sizeof(Range));
    }
}

/*
** Create a new rangeset table in the destination buffer
** by cloning it from the source table.
**
** Returns the new table created.
*/
RangesetTable *RangesetTableClone(RangesetTable *srcTable,
        textBuffer *destBuffer)
{
    RangesetTable *newTable = NULL;
    int i;

    if (srcTable == NULL)
    	return NULL;
	
    newTable = RangesetTableAlloc(destBuffer);

    newTable->n_set = srcTable->n_set;
    memcpy(newTable->order, srcTable->order, 
	    sizeof(unsigned char) *N_RANGESETS);
    memcpy(newTable->active, srcTable->active, 
	    sizeof(unsigned char) *N_RANGESETS);
    memcpy(newTable->depth, srcTable->depth, 
	    sizeof(unsigned char) *N_RANGESETS);
    memcpy(newTable->list, srcTable->list, 
	    sizeof(unsigned char) *(N_RANGESETS + 1));

    for (i = 0; i < N_RANGESETS; i++)
	rangesetClone(&newTable->set[i], &srcTable->set[i]);
    
    return newTable;
}

/*
** Find a range set given its label in the table.
*/

int RangesetFindIndex(RangesetTable *table, int label, int must_be_active)
{
    int i;
    unsigned char *p_label;
    
    if(label == 0) {
       return -1;
    }

    if (table != NULL) {
	p_label = (unsigned char*)strchr((char*)rangeset_labels, label);
	if (p_label) {
	    i = p_label - rangeset_labels;
	    if (!must_be_active || table->active[i])
		return i;
	}
    }

    return -1;
}


/*
** Assign the range set table list.
*/

static void RangesetTableListSet(RangesetTable *table)
{
    int i;

    for (i = 0; i < table->n_set; i++)
	table->list[i] = rangeset_labels[(int)table->order[i]];
    table->list[table->n_set] = '\0';
}

/*
** Return true if label is a valid identifier for a range set.
*/

int RangesetLabelOK(int label)
{
    return strchr((char*)rangeset_labels, label) != NULL;
}

/*
** Helper routines for managing the order and depth tables.
*/

static int activateRangeset(RangesetTable *table, int active)
{
    int depth, i, j;

    if (table->active[active])
	return 0;			/* already active */

    depth = table->depth[active];

    /* we want to make the "active" set the most recent (lowest depth value):
       shuffle table->order[0..depth-1] to table->order[1..depth]
       readjust the entries in table->depth[] accordingly */
    for (i = depth; i > 0; i--) {
	j = table->order[i] = table->order[i - 1];
	table->depth[j] = i;
    }
    /* insert the new one: first in order, of depth 0 */
    table->order[0] = active;
    table->depth[active] = 0;

    /* and finally... */
    table->active[active] = 1;
    table->n_set++;

    RangesetTableListSet(table);

    return 1;
}

static int deactivateRangeset(RangesetTable *table, int active)
{
    int depth, n, i, j;

    if (!table->active[active])
	return 0;			/* already inactive */

    /* we want to start by swapping the deepest entry in order with active
       shuffle table->order[depth+1..n_set-1] to table->order[depth..n_set-2]
       readjust the entries in table->depth[] accordingly */
    depth = table->depth[active];
    n = table->n_set - 1;

    for (i = depth; i < n; i++) {
	j = table->order[i] = table->order[i + 1];
	table->depth[j] = i;
    }
    /* reinsert the old one: at max (active) depth */
    table->order[n] = active;
    table->depth[active] = n;

    /* and finally... */
    table->active[active] = 0;
    table->n_set--;

    RangesetTableListSet(table);

    return 1;
}


/*
** Return the number of rangesets that are inactive
*/

int nRangesetsAvailable(RangesetTable *table)
{
    return(N_RANGESETS - table->n_set);
}


/*
** Create a new empty rangeset
*/

int RangesetCreate(RangesetTable *table)
{
    int label;
    int setIndex;
    
    size_t firstAvailableIndex = strspn((char*)rangeset_labels, (char*)table->list);

    if(firstAvailableIndex >= sizeof(rangeset_labels))
        return 0;
    
    label = rangeset_labels[firstAvailableIndex];
    
    setIndex = RangesetFindIndex(table, label, 0);

    if (setIndex < 0)
        return 0;

    if (table->active[setIndex])
	return label;

    if (activateRangeset(table, setIndex))
	RangesetInit(&table->set[setIndex], 
             rangeset_labels[setIndex], table->buf);

    return label;
}
      
/*
** Forget the rangeset identified by label - clears it, renders it inactive.
*/

Rangeset *RangesetForget(RangesetTable *table, int label)
{
    int set_ind = RangesetFindIndex(table, label, 1);

    if (set_ind < 0)
	return (Rangeset *)0;

    if (deactivateRangeset(table, set_ind))
	RangesetEmpty(&table->set[set_ind]);

    return &table->set[set_ind];
}


      
/*
** Fetch the rangeset identified by label - initialise it if not active and
** make_active is true, and make it the most visible.
*/

Rangeset *RangesetFetch(RangesetTable *table, int label)
{
    int rangesetIndex = RangesetFindIndex(table, label, 0);

    if (rangesetIndex < 0)
	return (Rangeset *)NULL;

    if (table->active[rangesetIndex])
	return &table->set[rangesetIndex];
    else
        return (Rangeset *)NULL;
}


unsigned char *RangesetGetList(RangesetTable *table)
{
    return table ? table->list : (unsigned char *)"";
}


/* -------------------------------------------------------------------------- */

void RangesetTableUpdatePos(RangesetTable *table, int pos, int ins, int del)
{
    int i;
    Rangeset *p;

    if (!table || (ins == 0 && del == 0))
	return;

    for (i = 0; i < table->n_set; i++) {
	p = &table->set[(int)table->order[i]];
	p->update_fn(p, pos, ins, del);
    }
}

void RangesetBufModifiedCB(int pos, int nInserted, int nDeleted, int nRestyled,
	const char *deletedText, void *cbArg)
{
    RangesetTable *table = (RangesetTable *)cbArg;
    if ((nInserted != nDeleted) || BufCmp(table->buf, pos, nInserted, deletedText) != 0) {
        RangesetTableUpdatePos(table, pos, nInserted, nDeleted);
    }
}

/* -------------------------------------------------------------------------- */

/*
** Find the index of the first range in depth order which includes the position.
** Returns the index of the rangeset in the rangeset table + 1 if an including
** rangeset was found, 0 otherwise. If needs_color is true, "colorless" ranges
** will be skipped.
*/

int RangesetIndex1ofPos(RangesetTable *table, int pos, int needs_color)
{
    int i;
    Rangeset *rangeset;

    if (!table)
	return 0;

    for (i = 0; i < table->n_set; i++) {
	rangeset = &table->set[(int)table->order[i]];
	if (RangesetCheckRangeOfPos(rangeset, pos) >= 0) {
	    if (needs_color && rangeset->color_set >= 0 && rangeset->color_name)
		return table->order[i] + 1;
	}
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

/*
** Assign a color name to a rangeset via the rangeset table.
*/

int RangesetAssignColorName(Rangeset *rangeset, char *color_name)
{
    char *cp;

    if (color_name && color_name[0] == '\0')
	color_name = (char *)0;				/* "" invalid */

    /* store new color name value */
    if (color_name) {
	cp = (char*)NEditMalloc(strlen(color_name) + 1);
	strcpy(cp, color_name);
    }
    else
	cp = color_name;

    /* free old color name value */
    NEditFree(rangeset->color_name);

    rangeset->color_name = cp;
    rangeset->color_set = 0;

    rangesetRefreshAllRanges(rangeset);
    return 1;
}

/*
** Assign a name to a rangeset via the rangeset table.
*/

int RangesetAssignName(Rangeset *rangeset, char *name)
{
    char *cp;

    if (name && name[0] == '\0')
        name = (char *)0;

    /* store new name value */
    if (name) {
        cp = (char*)NEditMalloc(strlen(name) + 1);
        strcpy(cp, name);
    }
    else {
        cp = name;
    }

    /* free old name value */
    NEditFree(rangeset->name);

    rangeset->name = cp;

    return 1;
}

/*
** Assign a color pixel value to a rangeset via the rangeset table. If ok is
** false, the color_set flag is set to an invalid (negative) value.
*/

int RangesetAssignColorPixel(Rangeset *rangeset, Pixel color, int ok)
{
    rangeset->color_set = ok ? 1 : -1;
    rangeset->color = color;
    return 1;
}


/*
** Return the name, if any.
*/

char *RangesetGetName(Rangeset *rangeset)
{
    return rangeset->name;
}

/*
** Return the color validity, if any, and the value in *color.
*/

int RangesetGetColorValid(Rangeset *rangeset, Pixel *color)
{
    *color = rangeset->color;
    return rangeset->color_set;
}

/*
** Return the color name, if any.
*/

char *RangesetTableGetColorName(RangesetTable *table, int index)
{
    Rangeset *rangeset = &table->set[index];
    return rangeset->color_name;
}

/*
** Return the color color validity, if any, and the value in *color.
*/

int RangesetTableGetColorValid(RangesetTable *table, int index, Pixel *color)
{
    Rangeset *rangeset = &table->set[index];
    *color = rangeset->color;
    return rangeset->color_set;
}

/*
** Assign a color pixel value to a rangeset via the rangeset table. If ok is
** false, the color_set flag is set to an invalid (negative) value.
*/

int RangesetTableAssignColorPixel(RangesetTable *table, int index, Pixel color,
	int ok)
{
    Rangeset *rangeset = &table->set[index];
    rangeset->color_set = ok ? 1 : -1;
    rangeset->color = color;
    return 1;
}

/* -------------------------------------------------------------------------- */

#define is_start(i)	!((i) & 1)	/* true if i is even */
#define is_end(i)	((i) & 1)	/* true if i is odd */

/*
** Find the index of the first entry in the range set's ranges table (viewed as
** an int array) whose value is equal to or greater than pos. As a side effect,
** update the lasi_index value of the range set. Return's the index value. This
** will be twice p->n_ranges if pos is beyond the end.
*/

static int rangesetWeightedAtOrBefore(Rangeset *rangeset, int pos)
{
    int i, last, n, *rangeTable = (int *)rangeset->ranges;

    n = rangeset->n_ranges;
    if (n == 0)
	return 0;

    last = rangeset->last_index;

    if (last >= n || last < 0)
	last = 0;

    n *= 2;
    last *= 2;

    if (pos >= rangeTable[last])		/* ranges[last_index].start */
	i = weighted_at_or_before(rangeTable, last, n, pos); /* search end only */
    else
	i = weighted_at_or_before(rangeTable, 0, last, pos); /* search front only */

    rangeset->last_index = i / 2;

    return i;
}

/*
** Adjusts values in tab[] by an amount delta, perhaps moving them meanwhile.
*/

static int rangesetShuffleToFrom(int *rangeTable, int to, int from, int n, int delta)
{
    int end, diff = from - to;

    if (n <= 0)
	return 0;

    if (delta != 0) {
	if (diff > 0) {			/* shuffle entries down */
	    for (end = to + n; to < end; to++)
		rangeTable[to] = rangeTable[to + diff] + delta;
	}
	else if (diff < 0) {		/* shuffle entries up */
	    for (end = to, to += n; --to >= end;)
		rangeTable[to] = rangeTable[to + diff] + delta;
	}
	else {				/* diff == 0: just run through */
	    for (end = n; end--;)
		rangeTable[to++] += delta;
	}
    }
    else {
	if (diff > 0) {			/* shuffle entries down */
	    for (end = to + n; to < end; to++)
		rangeTable[to] = rangeTable[to + diff];
	}
	else if (diff < 0) {		/* shuffle entries up */
	    for (end = to, to += n; --to >= end;)
		rangeTable[to] = rangeTable[to + diff];
	}
	/* else diff == 0: nothing to do */
    }

    return n;
}

/*
** Functions to adjust a rangeset to include new text or remove old.
** *** NOTE: No redisplay: that's outside the responsability of these routines.
*/

/* "Insert/Delete": if the start point is in or at the end of a range
** (start < pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
*/

static Rangeset *rangesetInsDelMaintain(Rangeset *rangeset, int pos, int ins, int del)
{
    int i, j, n, *rangeTable = (int *)rangeset->ranges;
    int end_del, movement;

    n = 2 * rangeset->n_ranges;

    i = rangesetWeightedAtOrBefore(rangeset, pos);

    if (i == n)
	return rangesetFixMaxpos(rangeset, ins, del);	/* all beyond the end */

    end_del = pos + del;
    movement = ins - del;

    /* the idea now is to determine the first range not concerned with the
       movement: its index will be j. For indices j to n-1, we will adjust
       position by movement only. (They may need shuffling up or down, depending
       on whether ranges have been deleted or created by the change.) */
    j = i;
    while (j < n && rangeTable[j] <= end_del)	/* skip j to first ind beyond changes */
	j++;

    /* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
       accounting for inserts. */
    if (j > i)
	rangeTable[i] = pos + ins;

    /* If i and j both index starts or ends, just copy all the rangeTable[] values down
       by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
       before doing this. */

    if (is_start(i) != is_start(j))
	i++;

    rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

    n -= j - i;
    rangeset->n_ranges = n / 2;
    rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);

    /* final adjustments */
    return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Inclusive": if the start point is in, at the start of, or at the end of a
** range (start <= pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
** (Almost identical to rangesetInsDelMaintain().)
*/

static Rangeset *rangesetInclMaintain(Rangeset *rangeset, int pos, int ins, int del)
{
    int i, j, n, *rangeTable = (int *)rangeset->ranges;
    int end_del, movement;

    n = 2 * rangeset->n_ranges;

    i = rangesetWeightedAtOrBefore(rangeset, pos);

    if (i == n)
	return rangesetFixMaxpos(rangeset, ins, del);	/* all beyond the end */

    /* if the insert occurs at the start of a range, the following lines will
       extend the range, leaving the start of the range at pos. */

    if (is_start(i) && rangeTable[i] == pos && ins > 0)
	i++;

    end_del = pos + del;
    movement = ins - del;

    /* the idea now is to determine the first range not concerned with the
       movement: its index will be j. For indices j to n-1, we will adjust
       position by movement only. (They may need shuffling up or down, depending
       on whether ranges have been deleted or created by the change.) */
    j = i;
    while (j < n && rangeTable[j] <= end_del)	/* skip j to first ind beyond changes */
	j++;

    /* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
       accounting for inserts. */
    if (j > i)
	rangeTable[i] = pos + ins;

    /* If i and j both index starts or ends, just copy all the rangeTable[] values down
       by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
       before doing this. */

    if (is_start(i) != is_start(j))
	i++;

    rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

    n -= j - i;
    rangeset->n_ranges = n / 2;
    rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);

    /* final adjustments */
    return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Delete/Insert": if the start point is in a range (start < pos &&
** pos <= end), and the end of deletion is also in a range
** (start <= pos + del && pos + del < end) any text inserted will extend that
** range. Deletions appear to occur before insertions. This will never add new
** ranges.
*/

static Rangeset *rangesetDelInsMaintain(Rangeset *rangeset, int pos, int ins, int del)
{
    int i, j, n, *rangeTable = (int *)rangeset->ranges;
    int end_del, movement;

    n = 2 * rangeset->n_ranges;

    i = rangesetWeightedAtOrBefore(rangeset, pos);

    if (i == n)
	return rangesetFixMaxpos(rangeset, ins, del);	/* all beyond the end */

    end_del = pos + del;
    movement = ins - del;

    /* the idea now is to determine the first range not concerned with the
       movement: its index will be j. For indices j to n-1, we will adjust
       position by movement only. (They may need shuffling up or down, depending
       on whether ranges have been deleted or created by the change.) */
    j = i;
    while (j < n && rangeTable[j] <= end_del)	/* skip j to first ind beyond changes */
	j++;

    /* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
       accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
       text will belong to the range that rangeTable[j] closes; otherwise inserted
       text does not belong to a range.) */
    if (j > i)
	rangeTable[i] = (is_end(j)) ? pos + ins : pos;

    /* If i and j both index starts or ends, just copy all the rangeTable[] values down
       by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
       before doing this. */

    if (is_start(i) != is_start(j))
	i++;

    rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

    n -= j - i;
    rangeset->n_ranges = n / 2;
    rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);

    /* final adjustments */
    return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Exclusive": if the start point is in, but not at the end of, a range
** (start < pos && pos < end), and the end of deletion is also in a range
** (start <= pos + del && pos + del < end) any text inserted will extend that
** range. Deletions appear to occur before insertions. This will never add new
** ranges. (Almost identical to rangesetDelInsMaintain().)
*/

static Rangeset *rangesetExclMaintain(Rangeset *rangeset, int pos, int ins, int del)
{
    int i, j, n, *rangeTable = (int *)rangeset->ranges;
    int end_del, movement;

    n = 2 * rangeset->n_ranges;

    i = rangesetWeightedAtOrBefore(rangeset, pos);

    if (i == n)
	return rangesetFixMaxpos(rangeset, ins, del);	/* all beyond the end */

    /* if the insert occurs at the end of a range, the following lines will
       skip the range, leaving the end of the range at pos. */

    if (is_end(i) && rangeTable[i] == pos && ins > 0)
	i++;

    end_del = pos + del;
    movement = ins - del;

    /* the idea now is to determine the first range not concerned with the
       movement: its index will be j. For indices j to n-1, we will adjust
       position by movement only. (They may need shuffling up or down, depending
       on whether ranges have been deleted or created by the change.) */
    j = i;
    while (j < n && rangeTable[j] <= end_del)	/* skip j to first ind beyond changes */
	j++;

    /* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
       accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
       text will belong to the range that rangeTable[j] closes; otherwise inserted
       text does not belong to a range.) */
    if (j > i)
	rangeTable[i] = (is_end(j)) ? pos + ins : pos;

    /* If i and j both index starts or ends, just copy all the rangeTable[] values down
       by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
       before doing this. */

    if (is_start(i) != is_start(j))
	i++;

    rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

    n -= j - i;
    rangeset->n_ranges = n / 2;
    rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);

    /* final adjustments */
    return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Break": if the modification point pos is strictly inside a range, that range
** may be broken in two if the deletion point pos+del does not extend beyond the
** end. Inserted text is never included in the range.
*/

static Rangeset *rangesetBreakMaintain(Rangeset *rangeset, int pos, int ins, int del)
{
    int i, j, n, *rangeTable = (int *)rangeset->ranges;
    int end_del, movement, need_gap;

    n = 2 * rangeset->n_ranges;

    i = rangesetWeightedAtOrBefore(rangeset, pos);

    if (i == n)
	return rangesetFixMaxpos(rangeset, ins, del);	/* all beyond the end */

    /* if the insert occurs at the end of a range, the following lines will
       skip the range, leaving the end of the range at pos. */

    if (is_end(i) && rangeTable[i] == pos && ins > 0)
	i++;

    end_del = pos + del;
    movement = ins - del;

    /* the idea now is to determine the first range not concerned with the
       movement: its index will be j. For indices j to n-1, we will adjust
       position by movement only. (They may need shuffling up or down, depending
       on whether ranges have been deleted or created by the change.) */
    j = i;
    while (j < n && rangeTable[j] <= end_del)	/* skip j to first ind beyond changes */
	j++;

    if (j > i)
	rangeTable[i] = pos;

    /* do we need to insert a gap? yes if pos is in a range and ins > 0 */

    /* The logic for the next statement: if i and j are both range ends, range
       boundaries indicated by index values between i and j (if any) have been
       "skipped". This means that rangeTable[i-1],rangeTable[j] is the current range. We will
       be inserting in that range, splitting it. */

    need_gap = (is_end(i) && is_end(j) && ins > 0);

    /* if we've got start-end or end-start, skip rangeTable[i] */
    if (is_start(i) != is_start(j)) {	/* one is start, other is end */
	if (is_start(i)) {
	    if (rangeTable[i] == pos)
		rangeTable[i] = pos + ins;	/* move the range start */
	}
	i++;				/* skip to next index */
    }

    /* values rangeTable[j] to rangeTable[n-1] must be adjusted by movement and placed in
       position. */

    if (need_gap)
	i += 2;				/* make space for the break */

    /* adjust other position values: shuffle them up or down if necessary */
    rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

    if (need_gap) {			/* add the gap informations */
	rangeTable[i - 2] = pos;
	rangeTable[i - 1] = pos + ins;
    }

    n -= j - i;
    rangeset->n_ranges = n / 2;
    rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);

    /* final adjustments */
    return rangesetFixMaxpos(rangeset, ins, del);
}

/* -------------------------------------------------------------------------- */

/*
** Invert the rangeset (replace it with its complement in the range 0-maxpos).
** Returns the number of ranges if successful, -1 otherwise. Never adds more
** than one range.
*/

int RangesetInverse(Rangeset *rangeset)
{
    int *rangeTable;
    int n, has_zero, has_end;

    if (!rangeset)
	return -1;

    rangeTable = (int *)rangeset->ranges;

    if (rangeset->n_ranges == 0) {
        if (!rangeTable) {
            rangeset->ranges = RangesNew(1);
            rangeTable = (int *)rangeset->ranges;
        }
	rangeTable[0] = 0;
	rangeTable[1] = rangeset->maxpos;
	n = 2;
    }
    else {
	n = rangeset->n_ranges * 2;

	/* find out what we have */
        has_zero = (rangeTable[0] == 0);
	has_end = (rangeTable[n - 1] == rangeset->maxpos);

	/* fill the entry "beyond the end" with the buffer's length */
	rangeTable[n + 1] = rangeTable[n] = rangeset->maxpos;

	if (has_zero) {
	  /* shuffle down by one */
	  rangesetShuffleToFrom(rangeTable, 0, 1, n, 0);
	  n -= 1;
	}
	else {
	  /* shuffle up by one */
	  rangesetShuffleToFrom(rangeTable, 1, 0, n, 0);
	  rangeTable[0] = 0;
	  n += 1;
	}
	if (has_end)
	  n -= 1;
	else
	  n += 1;
    }

    rangeset->n_ranges = n / 2;
    rangeset->ranges = RangesRealloc((Range *)rangeTable, rangeset->n_ranges);

    RangesetRefreshRange(rangeset, 0, rangeset->maxpos);
    return rangeset->n_ranges;
}

/* -------------------------------------------------------------------------- */

/*
** Add the range indicated by the positions start and end. Returns the
** new number of ranges in the set.
*/

int RangesetAddBetween(Rangeset *rangeset, int start, int end)
{
    int i, j, n, *rangeTable = (int *)rangeset->ranges;

    if (start > end) {
	i = start;        /* quietly sort the positions */
	start = end;
	end = i;
    }
    else if (start == end) {
	return rangeset->n_ranges; /* no-op - empty range == no range */
    }
    
    n = 2 * rangeset->n_ranges;

    if (n == 0) {			/* make sure we have space */
	rangeset->ranges = RangesNew(1);
	rangeTable = (int *)rangeset->ranges;
	i = 0;
    }
    else
	i = rangesetWeightedAtOrBefore(rangeset, start);

    if (i == n) {			/* beyond last range: just add it */
	rangeTable[n] = start;
	rangeTable[n + 1] = end;
	rangeset->n_ranges++;
	rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);

	RangesetRefreshRange(rangeset, start, end);
	return rangeset->n_ranges;
    }

    j = i;
    while (j < n && rangeTable[j] <= end)	/* skip j to first ind beyond changes */
	j++;

    if (i == j) {
	if (is_start(i)) {
	    /* is_start(i): need to make a gap in range rangeTable[i-1], rangeTable[i] */
	    rangesetShuffleToFrom(rangeTable, i + 2, i, n - i, 0);	/* shuffle up */
	    rangeTable[i] = start;		/* load up new range's limits */
	    rangeTable[i + 1] = end;
	    rangeset->n_ranges++;		/* we've just created a new range */
	    rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);
	}
	else {
	    return rangeset->n_ranges;		/* no change */
	}
    }
    else {
	/* we'll be shuffling down */
	if (is_start(i))
	    rangeTable[i++] = start;
	if (is_start(j))
	     rangeTable[--j] = end;
	if (i < j)
	    rangesetShuffleToFrom(rangeTable, i, j, n - j, 0);
	n -= j - i;
	rangeset->n_ranges = n / 2;
	rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);
    }

    RangesetRefreshRange(rangeset, start, end);
    return rangeset->n_ranges;
}


/*
** Remove the range indicated by the positions start and end. Returns the
** new number of ranges in the set.
*/

int RangesetRemoveBetween(Rangeset *rangeset, int start, int end)
{
    int i, j, n, *rangeTable = (int *)rangeset->ranges;

    if (start > end) {
	i = start;        /* quietly sort the positions */
	start = end;
	end = i;
    }
    else if (start == end) {
	return rangeset->n_ranges; /* no-op - empty range == no range */
    }
    
    n = 2 * rangeset->n_ranges;

    i = rangesetWeightedAtOrBefore(rangeset, start);

    if (i == n)
	return rangeset->n_ranges;		/* beyond last range */

    j = i;
    while (j < n && rangeTable[j] <= end)	/* skip j to first ind beyond changes */
	j++;

    if (i == j) {
	/* removal occurs in front of rangeTable[i] */
	if (is_start(i))
	    return rangeset->n_ranges;		/* no change */
	else {
	    /* is_end(i): need to make a gap in range rangeTable[i-1], rangeTable[i] */
	    i--;			/* start of current range */
	    rangesetShuffleToFrom(rangeTable, i + 2, i, n - i, 0); /* shuffle up */
	    rangeTable[i + 1] = start;		/* change end of current range */
	    rangeTable[i + 2] = end;		/* change start of new range */
	    rangeset->n_ranges++;		/* we've just created a new range */
	    rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);
	}
    }
    else {
	/* removal occurs in front of rangeTable[j]: we'll be shuffling down */
	if (is_end(i))
	    rangeTable[i++] = start;
	if (is_end(j))
	     rangeTable[--j] = end;
	if (i < j)
	    rangesetShuffleToFrom(rangeTable, i, j, n - j, 0);
	n -= j - i;
	rangeset->n_ranges = n / 2;
	rangeset->ranges = RangesRealloc(rangeset->ranges, rangeset->n_ranges);
    }

    RangesetRefreshRange(rangeset, start, end);
    return rangeset->n_ranges;
}

/* -------------------------------------------------------------------------- */

