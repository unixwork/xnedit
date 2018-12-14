/*******************************************************************************
*									       *
* vmsUtils.c - Utility routines for VMS systems.			       *
*									       *
*	       This file contains the following functions:		       *
*									       *
*	 StrDescToNul      - Convert VMS String Descriptor to C-like null-     *
*			       terminated string.  Returns the address of      *
*			       the malloc'd string.  (Call FreeNulStr() when   *
*			       done with the string.)			       *
*	 NulStrToDesc	   - Convert C-like null-terminated string to VMS      *
*			       String Descriptor.  Returns the address of      *
*			       the malloc'd descriptor, which should be free'd *
*			       when done by calling FreeStrDesc().  	       *
*	 NulStrWrtDesc	   - Convert C-like null-terminated string to VMS      *
*			       String Descriptor for writing into.  The C      *
*			       String should already be allocated and the      *
*			       length passed as the second parameter.  Returns *
*			       the address of the malloc'd descriptor, which   *
*			       should be free'd when done via FreeStrDesc().   *
*	 FreeNulStr	   - Frees null-terminated strings created by 	       *
*			       StrDescToNul(). 				       *
*	 FreeStrDesc 	   - Frees VMS String Descriptors created by 	       *
*			       NulStrToDesc() and NulStrWrtDesc(). 	       *
*	 ConvertVMSCommandLine	- Convert an argument vector representing a    *
*			       VMS-style command line to something Unix-like.  *
*			       Limitations: no abbreviations, some syntax      *
*			       information is lost so some errors will yield   *
*			       strange results.				       * 
*									       *
*	 rint   	   - Returns the integer (represented as a double      *
*			       precision number) nearest its double argument.  *
*									       *
*	 VMSFileScan   	   - Calls LIB$FILE_SCAN for filenames on VMS systems  *
*									       *
*	 VMSFileScanDone   - Ends LIB$FILE_SCAN context & frees memory used    *
*                                                                              *
*        ProcAlive 	   - See if a process (identified by pID) is still     *
*			       alive on VMS.				       *
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
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.							       *
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* February 22, 1993							       *
*									       *
* Written by Joy Kyriakopulos						       *
*									       *
*******************************************************************************/

#ifdef VMS

#include stdio
#include string
#include stdlib
#include ctype
#include errno
#include math
#include unixio
#include fab
#include nam
#include rmsdef
#include ssdef
#include starlet
#include lib$routines
#include jpidef
#include descrip
#include "vmsUtils.h"

/* Maximum number and length of arguments for ConvertVMSCommandLine */
#define MAX_ARGS 100
#define MAX_CMD_LENGTH 256
/* Maximum number of files handled by VMSFileScan */
#define MAX_NUM_FILES 100

static void successRtn(struct FAB *dirFab);	/* VMSFileScan */
static void errRtn(struct FAB *dirFab);		/* VMSFileScan */

static void addArgChar(int *argc, char *argv[], char c);

char *StrDescToNul(struct dsc$descriptor_s *vmsString)
{
    char *str;
    
    if (vmsString->dsc$b_dtype != DSC$K_DTYPE_T || vmsString->dsc$b_class !=
    	    DSC$K_CLASS_S)
    	fprintf(stderr,"Warning from StrDescToNul: descriptor class/type = %d/%d\n%s",
    	    vmsString->dsc$b_class, vmsString->dsc$b_dtype,
    	    "               Expecting 1/14\n");
    str = malloc(vmsString->dsc$w_length + 1);
    strncpy(str, vmsString->dsc$a_pointer, vmsString->dsc$w_length);
    str[vmsString->dsc$w_length + 1] = '\0';
    return str;
}

struct dsc$descriptor_s *NulStrToDesc(char *nulTString)
{
    struct dsc$descriptor_s *vmsString;
    int strLen;
    char *tmp;
    
    strLen = strlen(nulTString);
    if (strLen > 32767)
    	fprintf(stderr,"Warning from NulStrToDesc: string > 32767 bytes\n");
    vmsString = malloc(sizeof(struct dsc$descriptor_s) + strLen + 1);
    vmsString->dsc$a_pointer = ((char *)vmsString) 
				+ sizeof(struct dsc$descriptor_s);
    tmp = strcpy(vmsString->dsc$a_pointer, nulTString);
    vmsString->dsc$w_length = strLen;
    vmsString->dsc$b_dtype = DSC$K_DTYPE_T;
    vmsString->dsc$b_class = DSC$K_CLASS_S;
    return vmsString;
}

struct dsc$descriptor_s *NulStrWrtDesc(char *nulTString, int strLen)
{
    struct dsc$descriptor_s *vmsString;
    
    if (strLen > 32767)
    	fprintf(stderr,"Warning from NulStrToDesc: string > 32767 bytes\n");
    memset(nulTString, 0, strLen);
    /*bzero(nulTString, strLen);*/
    vmsString = malloc(sizeof(struct dsc$descriptor_s));
    vmsString->dsc$a_pointer = nulTString;
    vmsString->dsc$w_length = strLen;
    vmsString->dsc$b_dtype = DSC$K_DTYPE_T;
    vmsString->dsc$b_class = DSC$K_CLASS_S;
    return vmsString;
}

void FreeNulStr(char *nulTString)
{
    free(nulTString);
}

void FreeStrDesc(struct dsc$descriptor_s *vmsString)
{
    if (vmsString->dsc$b_dtype != DSC$K_DTYPE_T || vmsString->dsc$b_class !=
    	    DSC$K_CLASS_S)
    	fprintf(stderr,"Warning from FreeStrDesc: descriptor class/type = %d/%d\n%s",
    	    vmsString->dsc$b_class, vmsString->dsc$b_dtype,
    	    "               Expecting 1/14\n");
    free(vmsString);
}

#if !(defined __ALPHA && (defined _XOPEN_SOURCE_EXTENDED || !defined _ANSI_C_SOURCE))
double rint(double dnum)
{
    return floor(dnum + 0.5);
}
#endif

/*
** Re-read the command line and convert it from VMS style to unix style.
** Replaces argv and argc with Unix-correct versions.  This is
** a poor solution to parsing VMS command lines because some information
** is lost and some elements of the syntax are not checked.  Users also
** can't abbreviate qualifiers as is customary under VMS.
*/
void ConvertVMSCommandLine(int *argc, char **argv[])
{
    int i;
    short cmdLineLen;
    char *c, cmdLine[MAX_CMD_LENGTH], *oldArg0;
    struct dsc$descriptor_s *cmdLineDesc;

    /* get the command line (don't use the old argv and argc because VMS
       has removed the quotes and altered the line somewhat */
    cmdLineDesc = NulStrWrtDesc(cmdLine, MAX_CMD_LENGTH);
    lib$get_foreign(cmdLineDesc, 0, &cmdLineLen, 0);
    FreeStrDesc(cmdLineDesc);
    
    /* begin a new argv and argc, but preserve the original argv[0]
       which is not returned by lib$get_foreign */
    oldArg0 = (*argv)[0];
    *argv = (char **)malloc(sizeof(char *) * MAX_ARGS);
    (*argv)[0] = oldArg0;
    *argc = 1;

    /* scan all of the text on the command line, reconstructing the arg list */
    for (i=0, c=cmdLine; i<cmdLineLen; c++, i++) {
	if (*c == '\t') {
	    addArgChar(argc, *argv, ' ');
	} else if (*c == '/') {
	    addArgChar(argc, *argv, ' ');
	    addArgChar(argc, *argv, '-');
	} else if (*c == '=') {
	    addArgChar(argc, *argv, ' ');
	} else if (*c == ',') {
	    addArgChar(argc, *argv, ' ');
	} else {
	    addArgChar(argc, *argv, *c);
	}
    }
    addArgChar(argc, *argv, ' ');
}

/*
** Accumulate characters in an argv argc style argument list.  Spaces
** mean start a new argument and extra ones are ignored.  Argument strings
** are accumulated internally in the routine and not flushed into argv
** until a space is recieved (so a final space is needed to finish argv).
*/
static void addArgChar(int *argc, char *argv[], char c)
{
    static char str[MAX_CMD_LENGTH];
    static char *strPtr = str;
    static int inQuoted = FALSE, preserveQuotes = FALSE;
    int strLen;

    if (c == ' ') {
	if (strPtr == str) {	/* don't form empty arguments */
	    return;
	} else if (inQuoted) {	/* preserve spaces inside of quoted strings */
	    *strPtr++ = c;
	} else {		/* flush the accumulating argument */
	    strLen = strPtr - str;
	    argv[*argc] = (char *)malloc(sizeof(char) * (strLen + 1));
	    strncpy(argv[*argc], str, strLen);
	    argv[*argc][strLen] = '\0';
	    (*argc)++;
	    strPtr = str;
	    inQuoted = FALSE;
	}
    } else if (c == '"') {	/* note when quoted strings begin and end */
    	if (inQuoted) {
    	    inQuoted = FALSE;
    	} else {
    	    preserveQuotes = (strPtr != str);	/* remove quotes around the  */
    	    inQuoted = TRUE;			/* outsides of arguments but */
    	}					/* preserve internal ones so */
    	if (preserveQuotes)			/* access strings will work  */
    	    *strPtr++ = c;
    } else if (inQuoted) {
	*strPtr++ = c;
    } else {
    	*strPtr++ = tolower(c);
    }
}
	
/*
 * VMSFileScan -- Routine to call LIB$FILE_SCAN for filenames on VMS systems 
 *
 *	Returns: integer value >= 0, the number of files returned in namelist
 *			       = -1, an error was returned from LIB$FILE_SCAN
 *					and the error is printed on stderr
 *
 *	Parameters:
 *
 *	    dirname:  input file specification (can include wildcards)
 *	    namelist: array of pointers to the expanded file specs
 *		         (free each string and the table of pointers when done)
 *	    select:   user supplied function (or NULL) to call to select
 *		         which filenames are to be included in dirname array.
 *		         If NULL, all filenames will be included.
 *	    fnf:      specify INCLUDE_FNF, EXCLUDE_FNF, or NOT_ERR_FNF 
 *			 INCLUDE_FNF: the resultant file specification is 
 *				passed on to select() routine for returning
 *				in namelist, even though the file doesn't exist
 *			 EXCLUDE_FNF: return -1 (error) if dirname doesn't exist
 *			 NOT_ERR_FNF: return 0 (no error) if no files found
 *
 *	Call VMSFileScanDone() to free memory used by the FAB and RAB and clear
 *	sticky filename defaults for another scanning sequence.
 */
static char **Namelist;
static int NumFilesFound;
static int Fnf;
static int Context = 0;
static int (*SelectRoutine)();		/* saves select fcn for successRtn    */
static struct FAB *DirFAB = NULL;
static struct NAM *DirNAM = NULL;

int VMSFileScan(char *dirname, char *(*namelist[]), int (*select)(), int fnf)
{
    char result_name[NAM$C_MAXRSS+1];	/* array for resulting file spec      */
    char expanded_name[NAM$C_MAXRSS+1];	/* array for expanded file spec	      */
    int stat;

    if (DirFAB == NULL) {
    	DirFAB = (struct FAB *) malloc(sizeof(struct FAB));
    	DirNAM = (struct NAM *) malloc(sizeof(struct NAM));
	*DirFAB = cc$rms_fab;		/* initialize FAB with default values */
	*DirNAM = cc$rms_nam;		/*     "      NAMe block "  "   "     */
	DirFAB->fab$l_nam = DirNAM;	/* point FAB to NAM block	      */
	DirFAB->fab$l_dna = "*.";	/* default is no extension            */
	DirFAB->fab$b_dns = 2;
	DirNAM->nam$b_ess = sizeof(expanded_name) - 1;
        DirNAM->nam$b_rss = sizeof(result_name) - 1;
    }

    DirFAB->fab$l_fna = dirname;	/* wildcard spec for LIB$FILE_SCAN    */
    DirFAB->fab$b_fns = strlen(dirname);
    DirNAM->nam$l_esa = &expanded_name[0]; /* expanded file specs ret'nd here */
    DirNAM->nam$l_rsa = &result_name[0];   /* resultant file specs ret'nd here */
    SelectRoutine = select;
    NumFilesFound = 0;
    Fnf = fnf;
    Namelist = malloc(sizeof(char *) * MAX_NUM_FILES);
    *namelist = 0;
    stat = lib$file_scan(DirFAB, successRtn, errRtn, &Context); 

    if (stat != RMS$_NORMAL && stat != RMS$_FNF && stat != RMS$_NMF) {
	fprintf(stderr, "Error calling LIB$FILE_SCAN: %s\n",
		strerror(EVMSERR, stat));
	return -1;
    }
    if (stat == RMS$_FNF && Fnf == EXCLUDE_FNF)
	return -1;
    *namelist = Namelist;
    return NumFilesFound;
}

static void successRtn(struct FAB *dirFab)
{
    if (NumFilesFound >= MAX_NUM_FILES)
	return;

    /* terminate filename string with a null to pass to user's select routine */
    dirFab->fab$l_nam->nam$l_rsa[dirFab->fab$l_nam->nam$b_rsl] = '\0';

    /* if user's select routine returns value != 0, then put into name list */
    if (SelectRoutine == NULL || 
		(*SelectRoutine)(dirFab->fab$l_nam->nam$l_rsa)) {
	++NumFilesFound;
	Namelist[NumFilesFound-1] = malloc(dirFab->fab$l_nam->nam$b_rsl+1);
	strcpy(Namelist[NumFilesFound-1], dirFab->fab$l_nam->nam$l_rsa);
	/* printf("File: %s included\n", dirFab->fab$l_nam->nam$l_rsa); */
	
    }
}

static void errRtn(struct FAB *dirFab)
{
    if (dirFab->fab$l_sts == RMS$_FNF && Fnf == INCLUDE_FNF)
	successRtn(dirFab);	   /* return filename even tho' doesn't exist */
    else if (dirFab->fab$l_sts != RMS$_FNF || (dirFab->fab$l_sts == RMS$_FNF
    		&& Fnf != NOT_ERR_FNF))
	fprintf(stderr, "Error - %s:  %s\n", strerror(EVMSERR,
		 dirFab->fab$l_sts), dirFab->fab$l_fna);
}

void VMSFileScanDone(void)
{
    if (DirFAB != NULL) {
    	int s;
	if ((s=lib$file_scan_end(DirFAB, &Context)) != RMS$_NORMAL 
			&& s != SS$_NORMAL)
    	    fprintf(stderr, "Error calling LIB$FILE_SCAN_END: %s\n",
    	    		strerror(EVMSERR,s));
	free(DirNAM);
	DirNAM = NULL;
	free(DirFAB);
	DirFAB = NULL;
    }
}

/*
 * ProcAlive: see if a process (identified by pID) is still alive on VMS.
 *
 *    Returns:  1 - process exists
 *	        0 - process does not exist
 *	       -1 - error getting process info
 */
int ProcAlive(const unsigned int pID)
{
    int jpiStat;
    short retLen;
    char userName[13];			/* 12 plus 1 for ending null */
    struct getJPIdescriptor {
	short bufLength;
	short itemCode;
	char  *bufAddr;
	short *retLenAddr;
	int   *endList;
    } getJPID;

    getJPID.bufLength  = 12;		/* (max) size of user name */
    getJPID.itemCode   = JPI$_USERNAME;
    getJPID.bufAddr    = userName;
    getJPID.retLenAddr = &retLen;
    getJPID.endList    = 0;
    jpiStat = sys$getjpiw(1,&pID,0,&getJPID,0,0,0);
    /* printf("in ProcAlive - jpiStat = %d, pid = %X\n", jpiStat, pID); */
    if (jpiStat == SS$_NORMAL || jpiStat == SS$_NOPRIV 
		|| jpiStat == SS$_SUSPENDED)
	return 1;			/* process exists	  */
    if (jpiStat == SS$_NONEXPR)
	return 0;			/* process does not exist */
    fprintf(stderr, "Error calling GETJPI in ProcAlive.  Status = %d\n",
	    jpiStat);
    return -1;				/* error		  */
}


#endif /*VMS*/
