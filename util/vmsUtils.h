/*******************************************************************************
*                                                                              *
* vmsUtils.h -- Nirvana Editor VMS Utilities Header File                       *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
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

#ifndef NEDIT_VMSUTILS_H_INCLUDED
#define NEDIT_VMSUTILS_H_INCLUDED

#ifdef VMS
#ifndef __DESCRIP_LOADED
#include descrip
#endif /*__DESCRIP_LOADED*/

#define INCLUDE_FNF 0
#define EXCLUDE_FNF 1
#define NOT_ERR_FNF 2

char *StrDescToNul(struct dsc$descriptor_s *vmsString);
struct dsc$descriptor_s *NulStrToDesc(char *nulTString);
struct dsc$descriptor_s *NulStrWrtDesc(char *nulTString, int strLen);
void FreeNulStr(char *nulTString);
void FreeStrDesc(struct dsc$descriptor_s *vmsString);
double rint(double dnum);
void ConvertVMSCommandLine(int *argc, char **argv[]);
int VMSFileScan(char *dirname, char *(*namelist[]), int (*select)(), int fnf);
void VMSFileScanDone(void);
int ProcAlive(const unsigned int pID);

#endif /*VMS*/

#endif /* NEDIT_VMSUTILS_H_INCLUDED */
