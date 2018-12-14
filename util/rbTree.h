/*******************************************************************************
*                                                                              *
* rbTree.h -- Nirvana Editor Red-Black Balanced Binary Tree Header File        *
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

#ifndef NEDIT_RBTREE_H_INCLUDED
#define NEDIT_RBTREE_H_INCLUDED

typedef struct rbTreeNode {
    struct rbTreeNode *left;   /* left child */
    struct rbTreeNode *right;  /* right child */
    struct rbTreeNode *parent; /* parent */
    int color;                 /* node color (rbTreeNodeBlack, rbTreeNodeRed) */
} rbTreeNode;

typedef int (*rbTreeCompareNodeCB)(rbTreeNode *left, rbTreeNode *right);
typedef rbTreeNode *(*rbTreeAllocateNodeCB)(rbTreeNode *src);
typedef rbTreeNode *(*rbTreeAllocateEmptyNodeCB)(void);
typedef void (*rbTreeDisposeNodeCB)(rbTreeNode *src);
typedef int (*rbTreeCopyToNodeCB)(rbTreeNode *dst, rbTreeNode *src);

rbTreeNode *rbTreeBegin(rbTreeNode *base);
rbTreeNode *rbTreeReverseBegin(rbTreeNode *base);
rbTreeNode *rbTreeFind(rbTreeNode *base, rbTreeNode *searchNode,
                        rbTreeCompareNodeCB compareRecords);
rbTreeNode *rbTreeInsert(rbTreeNode *base, rbTreeNode *searchNode,
                            rbTreeCompareNodeCB compareRecords,
                            rbTreeAllocateNodeCB allocateNode,
                            rbTreeCopyToNodeCB copyToNode);
rbTreeNode *rbTreeUnlinkNode(rbTreeNode *base, rbTreeNode *z);
void rbTreeDeleteNode(rbTreeNode *base, rbTreeNode *foundNode,
                    rbTreeDisposeNodeCB disposeNode);
int rbTreeDelete(rbTreeNode *base, rbTreeNode *searchNode,
                    rbTreeCompareNodeCB compareRecords,
                    rbTreeDisposeNodeCB disposeNode);
rbTreeNode *rbTreeNext(rbTreeNode *x);
rbTreeNode *rbTreePrevious(rbTreeNode *x);
int rbTreeSize(rbTreeNode *base);
rbTreeNode *rbTreeNew(rbTreeAllocateEmptyNodeCB allocateEmptyNode);
void rbTreeDispose(rbTreeNode *base, rbTreeDisposeNodeCB disposeNode);

#endif /* NEDIT_RBTREE_H_INCLUDED */
