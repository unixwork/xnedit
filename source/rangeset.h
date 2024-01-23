/*******************************************************************************
*									       *
* rangeset.h	 -- Nirvana Editor rangest header			       *
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
#ifndef rangeset_h_DEFINED
#define rangeset_h_DEFINED

#include <Xm/Xm.h>
#include <X11/Xft/Xft.h>

#define N_RANGESETS 63

typedef struct _Range Range;
typedef struct _Rangeset Rangeset;

void RangesetRefreshRange(Rangeset *rangeset, ssize_t start, ssize_t end);
void RangesetEmpty(Rangeset *rangeset);
void RangesetInit(Rangeset *rangeset, int label, textBuffer *buf);
int RangesetChangeModifyResponse(Rangeset *rangeset, char *name);
int RangesetFindRangeNo(Rangeset *rangeset, int index, ssize_t *start, ssize_t *end);
int RangesetFindRangeOfPos(Rangeset *rangeset, ssize_t pos, ssize_t incl_end);
int RangesetCheckRangeOfPos(Rangeset *rangeset, ssize_t pos);
int RangesetInverse(Rangeset *p);
int RangesetAdd(Rangeset *origSet, Rangeset *plusSet);
int RangesetAddBetween(Rangeset *rangeset, ssize_t start, ssize_t end);
int RangesetRemove(Rangeset *origSet, Rangeset *minusSet);
int RangesetRemoveBetween(Rangeset *rangeset, ssize_t start, ssize_t end);
int RangesetGetNRanges(Rangeset *rangeset);
void RangesetGetInfo(Rangeset *rangeset, int *defined, int *label, 
        int *count, char **color, char **name, char **mode);
RangesetTable *RangesetTableAlloc(textBuffer *buf);
RangesetTable *RangesetTableFree(RangesetTable *table);
RangesetTable *RangesetTableClone(RangesetTable *srcTable,
        textBuffer *destBuffer);
int RangesetFindIndex(RangesetTable *table, int label, int must_be_active);
int RangesetLabelOK(int label);
int RangesetCreate(RangesetTable *table);
int nRangesetsAvailable(RangesetTable *table);
Rangeset *RangesetForget(RangesetTable *table, int label);
Rangeset *RangesetFetch(RangesetTable *table, int label);
unsigned char * RangesetGetList(RangesetTable *table);
void RangesetTableUpdatePos(RangesetTable *table, ssize_t pos, ssize_t n_ins, ssize_t n_del);
void RangesetBufModifiedCB(ssize_t pos, ssize_t nInserted, ssize_t nDeleted, ssize_t nRestyled,
	const char *deletedText, void *cbArg);
int RangesetIndex1ofPos(RangesetTable *table, ssize_t pos, int needs_color);
int RangesetAssignColorName(Rangeset *rangeset, char *color_name);
int RangesetAssignColorPixel(Rangeset *rangeset, XftColor color, int ok);
char *RangesetGetName(Rangeset *rangeset);
int RangesetAssignName(Rangeset *rangeset, char *name);
int RangesetGetColorValid(Rangeset *rangeset, XftColor *color);
char *RangesetTableGetColorName(RangesetTable *table, int index);
int RangesetTableGetColorValid(RangesetTable *table, int index, XftColor *color, Rangeset **rs);
Rangeset* RangesetTableAssignColorPixel(RangesetTable *table, int index, XftColor color,
	int ok);
XftColor* RangesetGetColor(Rangeset *rangeset);

#endif /* rangeset_h_DEFINED */
