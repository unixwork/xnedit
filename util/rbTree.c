/*******************************************************************************
*                                                                              *
* rbTree.c -- Nirvana editor red-black balanced binary tree                    *
*                                                                              *
* Copyright (C) 2001 Mark Edel                                                 *
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

/*
** rbTree is a red-black balanced binary tree
** the base node holds the leftmost, rightmost and root pointers
** and a node count
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "rbTree.h"
#include "../util/nedit_malloc.h"

#include <stdlib.h>
#include <string.h>
/*#define RBTREE_TEST_CODE*/
#ifdef RBTREE_TEST_CODE
#include <stdio.h>
#endif

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif


#define rbTreeNodeRed       0
#define rbTreeNodeBlack     1

/*
** rotate a node left
*/
static void rotateLeft(rbTreeNode *x, rbTreeNode **root)
{
    rbTreeNode *y = x->right;
    x->right = y->left;
    if (y->left != NULL) {
        y->left->parent = x;
    }
    y->parent = x->parent;

    if (x == *root) {
        *root = y;
    }
    else if (x == x->parent->left) {
        x->parent->left = y;
    }
    else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

/*
** rotate a node right
*/
static void rotateRight(rbTreeNode *x, rbTreeNode **root)
{
    rbTreeNode *y = x->left;
    x->left = y->right;
    if (y->right != NULL) {
        y->right->parent = x;
    }
    y->parent = x->parent;

    if (x == *root) {
        *root = y;
    }
    else if (x == x->parent->right) {
        x->parent->right = y;
    }
    else {
        x->parent->left = y;
    }
    y->right = x;
    x->parent = y;
}

/*
** balance tree after an insert of node x
*/
static void insertBalance(rbTreeNode *x, rbTreeNode **root)
{
  x->color = rbTreeNodeRed;
  while (x != *root && x->parent->color == rbTreeNodeRed) {
    if (x->parent == x->parent->parent->left) {
      rbTreeNode *y = x->parent->parent->right;
      if (y && y->color == rbTreeNodeRed) {
        x->parent->color = rbTreeNodeBlack;
        y->color = rbTreeNodeBlack;
        x->parent->parent->color = rbTreeNodeRed;
        x = x->parent->parent;
      }
      else {
        if (x == x->parent->right) {
          x = x->parent;
          rotateLeft(x, root);
        }
        x->parent->color = rbTreeNodeBlack;
        x->parent->parent->color = rbTreeNodeRed;
        rotateRight(x->parent->parent, root);
      }
    }
    else {
      rbTreeNode *y = x->parent->parent->left;
      if (y && y->color == rbTreeNodeRed) {
        x->parent->color = rbTreeNodeBlack;
        y->color = rbTreeNodeBlack;
        x->parent->parent->color = rbTreeNodeRed;
        x = x->parent->parent;
      }
      else {
        if (x == x->parent->left) {
          x = x->parent;
          rotateRight(x, root);
        }
        x->parent->color = rbTreeNodeBlack;
        x->parent->parent->color = rbTreeNodeRed;
        rotateLeft(x->parent->parent, root);
      }
    }
  }
  (*root)->color = rbTreeNodeBlack;
}

/*
** returns the leftmost node (the beginning of the sorted list)
*/
rbTreeNode *rbTreeBegin(rbTreeNode *base)
{
    return(base->left);
}

/*
** returns the rightmost node (the end of the sorted list)
*/
rbTreeNode *rbTreeReverseBegin(rbTreeNode *base)
{
    return(base->right);
}

/*
** search for a node and return it's pointer, NULL if not found
*/
rbTreeNode *rbTreeFind(rbTreeNode *base, rbTreeNode *searchNode,
                        rbTreeCompareNodeCB compareRecords)
{
    rbTreeNode *foundNode = NULL;
    rbTreeNode *current = base->parent;
    while(current != NULL) {
        int compareResult = compareRecords(searchNode, current);

        if (compareResult < 0) {
            current = current->left;
        }
        else if (compareResult > 0) {
            current = current->right;
        }
        else {
            foundNode = current;
            current = NULL;
        }
    }
    return(foundNode);
}

/*
** insert a node into the tree and rebalance it
** if a duplicate is found copy the new data over it
** returns the new node
*/
rbTreeNode *rbTreeInsert(rbTreeNode *base, rbTreeNode *searchNode,
                            rbTreeCompareNodeCB compareRecords,
                            rbTreeAllocateNodeCB allocateNode,
                            rbTreeCopyToNodeCB copyToNode)
{
    rbTreeNode *current, *parent, *x;
    int fromLeft = 0, foundMatch = 0;

    current = base->parent;
    parent = NULL;
    x = NULL;
    while(current != NULL) {
        int compareResult = compareRecords(searchNode, current);

        if (compareResult < 0) {
            parent = current;
            current = current->left;
            fromLeft = 1;
        }
        else if (compareResult > 0) {
            parent = current;
            current = current->right;
            fromLeft = 0;
        }
        else {
            x = current;
            if (!copyToNode(x, searchNode)) {
                x = NULL;
            }
            current = NULL;
            foundMatch = 1;
        }
    }

    if (!foundMatch) {
        x = allocateNode(searchNode);
        if (x) {
            x->parent = parent;
            x->left = NULL;
            x->right = NULL;
            x->color = rbTreeNodeRed;

            if (parent) {
                if (fromLeft) {
                    parent->left = x;
                }
                else {
                    parent->right = x;
                }
            }
            else {
                base->parent = x;
            }
            ++(base->color);
            if (x->parent == base->left && (x->parent == NULL || x->parent->left == x)) {
                base->left = x;
            }
            if (x->parent == base->right && (x->parent == NULL || x->parent->right == x)) {
                base->right = x;
            }
            insertBalance(x, &base->parent);
        }
    }
    return(x);
}

/*
** unlink a node from the tree and rebalance it.
** returns the removed node pointer
*/
rbTreeNode *rbTreeUnlinkNode(rbTreeNode *base, rbTreeNode *z)
{
    int swapColor;
    rbTreeNode *x, *y, *x_parent;

    y = z;
    if (y->left == NULL) {
        x = y->right;
        if (y == base->left) {
            base->left = rbTreeNext(y);
        }
    }
    else {
        if (y->right == NULL) {
            x = y->left;
            if (y == base->right) {
                base->right = rbTreePrevious(y);
            }
        }
        else {
            y = y->right;
            while (y->left != NULL) {
                y = y->left;
            }
            x = y->right;
        }
    }
    if (y != z) {
        z->left->parent = y;
        y->left = z->left;
        if (y != z->right) {
            x_parent = y->parent;
            if (x != NULL) {
                x->parent = y->parent;
            }
            y->parent->left = x;
            y->right = z->right;
            z->right->parent = y;
        }
        else {
            x_parent = y;
        }
        if (base->parent == z) {
            base->parent = y;
        }
        else if (z->parent->left == z) {
            z->parent->left = y;
        }
        else {
            z->parent->right = y;
        }
        y->parent = z->parent;

        swapColor = y->color;
        y->color = z->color;
        z->color = swapColor;

        y = z;
    }
    else {
        x_parent = y->parent;
        if (x != NULL) {
            x->parent = y->parent;
        }
        if (base->parent == z) {
            base->parent = x;
        }
        else {
            if (z->parent->left == z) {
                z->parent->left = x;
            }
            else {
                z->parent->right = x;
            }
        }
    }

    --(base->color);

    if (y->color != rbTreeNodeRed) { 
        while (x != base->parent && (x == NULL || x->color == rbTreeNodeBlack)) {
            if (x == x_parent->left) {
                rbTreeNode *w = x_parent->right;
                if (w->color == rbTreeNodeRed) {
                    w->color = rbTreeNodeBlack;
                    x_parent->color = rbTreeNodeRed;
                    rotateLeft(x_parent, &base->parent);
                    w = x_parent->right;
                }
                if ((w->left == NULL || 
                w->left->color == rbTreeNodeBlack) &&
                (w->right == NULL || 
                w->right->color == rbTreeNodeBlack)) {

                    w->color = rbTreeNodeRed;
                    x = x_parent;
                    x_parent = x_parent->parent;
                } else {
                    if (w->right == NULL || 
                    w->right->color == rbTreeNodeBlack) {

                        if (w->left) {
                            w->left->color = rbTreeNodeBlack;
                        }
                        w->color = rbTreeNodeRed;
                        rotateRight(w, &base->parent);
                        w = x_parent->right;
                    }
                    w->color = x_parent->color;
                    x_parent->color = rbTreeNodeBlack;
                    if (w->right) {
                        w->right->color = rbTreeNodeBlack;
                    }
                    rotateLeft(x_parent, &base->parent);
                    break;
                }
            }
            else {
                rbTreeNode *w = x_parent->left;
                if (w->color == rbTreeNodeRed) {
                    w->color = rbTreeNodeBlack;
                    x_parent->color = rbTreeNodeRed;
                    rotateRight(x_parent, &base->parent);
                    w = x_parent->left;
                }
                if ((w->right == NULL || 
                    w->right->color == rbTreeNodeBlack) &&
                    (w->left == NULL || 
                    w->left->color == rbTreeNodeBlack)) {

                    w->color = rbTreeNodeRed;
                    x = x_parent;
                    x_parent = x_parent->parent;
                }
                else {
                    if (w->left == NULL || 
                        w->left->color == rbTreeNodeBlack) {

                        if (w->right) {
                            w->right->color = rbTreeNodeBlack;
                        }
                        w->color = rbTreeNodeRed;
                        rotateLeft(w, &base->parent);
                        w = x_parent->left;
                    }
                    w->color = x_parent->color;
                    x_parent->color = rbTreeNodeBlack;
                    if (w->left) {
                        w->left->color = rbTreeNodeBlack;
                    }
                    rotateRight(x_parent, &base->parent);
                    break;
                }
            }
        }
        if (x) {
            x->color = rbTreeNodeBlack;
        }
    }
    return(y);
}

/*
** delete an already found node and dispose it
*/
void rbTreeDeleteNode(rbTreeNode *base, rbTreeNode *foundNode,
                    rbTreeDisposeNodeCB disposeNode)
{
    disposeNode(rbTreeUnlinkNode(base, foundNode));
}

/*
** search for a node and remove it from the tree
** disposing the removed node
** returns 1 if a node was found, otherwise 0
*/
int rbTreeDelete(rbTreeNode *base, rbTreeNode *searchNode,
                    rbTreeCompareNodeCB compareRecords,
                    rbTreeDisposeNodeCB disposeNode)
{
    int foundNode = 0;
    rbTreeNode *z;

    z = rbTreeFind(base, searchNode, compareRecords);
    if (z != NULL) {
        rbTreeDeleteNode(base, z, disposeNode);
        foundNode = 1;
    }
    return(foundNode);
}

/*
** move an iterator foreward one element
** note that a valid pointer must be passed,
** passing NULL will result in unpredictable results
*/
rbTreeNode *rbTreeNext(rbTreeNode *x)
{
    if (x->right != NULL) {
        x = x->right;
        while (x->left != NULL) {
            x = x->left;
        }
    }
    else {
        rbTreeNode *fromX;
        do {
            fromX = x;
            x = x->parent;
        } while (x && fromX == x->right);
    }
    return(x);
}

/*
** move an iterator back one element
** note that a valid pointer must be passed,
** passing NULL will result in unpredictable results
*/
rbTreeNode *rbTreePrevious(rbTreeNode *x)
{
    if (x->left != NULL) {
        x = x->left;
        while (x->right != NULL) {
            x = x->right;
        }
    }
    else {
        rbTreeNode *fromX;
        do {
            fromX = x;
            x = x->parent;
        } while (x && fromX == x->left);
    }
    return(x);
}

/*
** returns the number of real data nodes in the tree, not counting
** the base node since it contains no data
*/
int rbTreeSize(rbTreeNode *base)
{
    return(base->color);
}

/*
** Allocate a new red-black tree using an empty node to hold pointers
*/
rbTreeNode *rbTreeNew(rbTreeAllocateEmptyNodeCB allocateEmptyNode)
{
    rbTreeNode *rootStorage = allocateEmptyNode();
    if (rootStorage) {
        rootStorage->left = NULL;   /* leftmost node */
        rootStorage->right = NULL;  /* rightmost node */
        rootStorage->parent = NULL; /* root node */
        rootStorage->color = 0;     /* node count */
    }
    return(rootStorage);
}

/*
** iterate through all nodes, unlinking and disposing them
** extra effort is made to maintain all links, the size, and
** leftmost/rightmost pointers, so that the tree can be dumped
** when debugging problems. We could probably ifdef some of this
** since it goes unused most of the time
** the tree is not kept balanced since all nodes will be removed
*/
void rbTreeDispose(rbTreeNode *base, rbTreeDisposeNodeCB disposeNode)
{
    rbTreeNode *iter = rbTreeBegin(base);
    while (iter != NULL) {
        rbTreeNode *nextIter = rbTreeNext(iter);

        if (iter->parent) {
            if (iter->parent->left == iter) {
                iter->parent->left = iter->right;
            }
            else {
                iter->parent->right = iter->right;
            }
        }
        if (iter->right != NULL) {
            iter->right->parent = iter->parent;
        }
        base->left = nextIter;
        if (base->right == iter) {
            base->right = NULL;
        }
        --(base->color);
        if (base->parent == iter) {
            base->parent = nextIter;
        }
        disposeNode(iter);

        iter = nextIter;
    }
    disposeNode(base);
}

#ifdef RBTREE_TEST_CODE
/* ================================================================== */

/*
** code to test basic stuff of tree routines
*/

typedef struct TestNode {
    rbTreeNode      nodePointers; /* MUST BE FIRST MEMBER */
    char *str;
    char *key;
} TestNode;


static int rbTreeCompareNode_TestNode(rbTreeNode *left, rbTreeNode *right)
{
    return(strcmp(((TestNode *)left)->key, ((TestNode *)right)->key));
}

static rbTreeNode *rbTreeAllocateNode_TestNode(rbTreeNode *src)
{
    TestNode *newNode = NEditMalloc(sizeof(TestNode));
    if (newNode) {
        newNode->str = NEditMalloc(strlen(((TestNode *)src)->str) + 1);
        if (newNode->str) {
            strcpy(newNode->str, ((TestNode *)src)->str);
            
            newNode->key = NEditMalloc(strlen(((TestNode *)src)->key) + 1);
            if (newNode->key) {
                strcpy(newNode->key, ((TestNode *)src)->key);
            }
            else {
                NEditFree(newNode->str);
                newNode->str = NULL;

                NEditFree(newNode);
                newNode = NULL;
            }
        }
        else {
            NEditFree(newNode);
            newNode = NULL;
        }
    }
    return((rbTreeNode *)newNode);
}

rbTreeNode *rbTreeAllocateEmptyNodeCB_TestNode(void)
{
    TestNode *newNode = NEditMalloc(sizeof(TestNode));
    if (newNode) {
        newNode->str = NULL;
        newNode->key = NULL;
    }
    return((rbTreeNode *)newNode);
}

static void rbTreeDisposeNode_TestNode(rbTreeNode *src)
{
    if (src) {
        if (((TestNode *)src)->str) {
            NEditFree(((TestNode *)src)->str);
            ((TestNode *)src)->str = NULL;
        }
        if (((TestNode *)src)->key) {
            NEditFree(((TestNode *)src)->key);
            ((TestNode *)src)->key = NULL;
        }
        src->left = (void *)-1;
        src->right = (void *)-1;
        src->parent = (void *)-1;
        src->color = rbTreeNodeBlack;

        NEditFree(src);
    }
}

static int rbTreeCopyToNode_TestNode(rbTreeNode *dst, rbTreeNode *src)
{
    TestNode newValues;
    int copiedOK = 0;
    
    newValues.str = NEditMalloc(strlen(((TestNode *)src)->str) + 1);
    if (newValues.str) {
        strcpy(newValues.str, ((TestNode *)src)->str);
        
        newValues.key = NEditMalloc(strlen(((TestNode *)src)->key) + 1);
        if (newValues.key) {
            strcpy(newValues.key, ((TestNode *)src)->key);
            
            ((TestNode *)dst)->str = newValues.str;
            ((TestNode *)dst)->key = newValues.key;
            copiedOK = 1;
        }
        else {
            NEditFree(newValues.str);
            newValues.str = NULL;
        }
    }
    return(copiedOK);
}

static void DumpTree(rbTreeNode *base)
{
    rbTreeNode *newNode;
    
    newNode = rbTreeBegin(base);
    while (newNode != NULL) {
        rbTreeNode *nextNode = rbTreeNext(newNode);
        
        printf("[%s] = \"%s\"\n", ((TestNode *)newNode)->key, ((TestNode *)newNode)->str);
        printf("[%x] l[%x] r[%x] p[%x] <%s>\n", (int)newNode, (int)newNode->left, (int)newNode->right, (int)newNode->parent, ((newNode->color == rbTreeNodeBlack)?"Black":"Red"));
        
        newNode = nextNode;
    }
}

int main(int argc, char **argv)
{
    rbTreeNode *base, *newNode;
    TestNode searchNode;
    char tmpkey[20], tmpValue[40];
    int i;
    
    searchNode.key = tmpkey;
    searchNode.str = tmpValue;

    base = rbTreeNew(rbTreeAllocateEmptyNodeCB_TestNode);
    if (!base) {
        printf("Failed New!!!\n");
        exit(1);
    }
    for (i = 0; i < 100; ++i) {
        sprintf(tmpkey, "%d", i);
        sprintf(tmpValue, "<%d>", i * i);
        
        newNode = rbTreeInsert(base, (rbTreeNode *)&searchNode,
                            rbTreeCompareNode_TestNode,
                            rbTreeAllocateNode_TestNode,
                            rbTreeCopyToNode_TestNode);
        if (!newNode) {
            printf("Failed!!!\n");
            exit(1);
        }
    }
    
    newNode = rbTreeBegin(base);
    while (newNode != NULL) {
        rbTreeNode *nextNode = rbTreeNext(newNode);
        
        printf("[%s] = \"%s\"\n", ((TestNode *)newNode)->key, ((TestNode *)newNode)->str);
        
        if (strlen(((TestNode *)newNode)->str) < 7) {
            int didDelete;
            
            printf("Deleting [%s]\n", ((TestNode *)newNode)->key);
            didDelete = rbTreeDelete(base, newNode,
                    rbTreeCompareNode_TestNode, rbTreeDisposeNode_TestNode);
            printf("delete result = %d\n", didDelete);
        }

        newNode = nextNode;
    }

    printf("Tree Size = %d\n", rbTreeSize(base));
    printf("\n++++++++++++++++\n");
    DumpTree(base);
    printf("\n++++++++++++++++\n");

    rbTreeDispose(base, rbTreeDisposeNode_TestNode);

    printf("\nDone.\n");
    return(0);
}
#endif
