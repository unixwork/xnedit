/************************************************************************
 *                                                                      *
 * dragAndDrop.c -- CDE drag and drop functions to provide              *
 *                  basic support for file drops.                       *
 *                                                                      *
 * Originally Written By Fredrik Jönsson                                *
 * Modified by Per Grahn                                                *
 ************************************************************************/
/* $Id$ */

#ifndef DRAGANDDROP_H
#define DRAGANDDROP_H

void neditDropInit (Widget, char *geometry);
void neditDropWidget (Widget);
void neditTransferCallback (Widget, XtPointer, XtPointer);
void fileTransferCallback (Widget, XtPointer, XtPointer);
void dataTransferCallback (Widget, XtPointer, XtPointer);

#endif /* DRAGANDDROP_H */
