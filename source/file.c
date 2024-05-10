/*******************************************************************************
*									       *
* file.c -- Nirvana Editor file i/o					       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
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
* May 10, 1991								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "file.h"
#include "filter.h"
#include "textBuf.h"
#include "text.h"
#include "window.h"
#include "preferences.h"
#include "undo.h"
#include "menu.h"
#include "tags.h"
#include "server.h"
#include "interpret.h"
#include "editorconfig.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/fileUtils.h"
#include "../util/getfiles.h"
#include "../util/printUtils.h"
#include "../util/utils.h"
#include "../util/nedit_malloc.h"
#include "../util/libxattr.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <iconv.h>
#include <locale.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#include <fcntl.h>

#include <Xm/Xm.h>
#include <Xm/ToggleB.h>
#include <Xm/FileSB.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#include <inttypes.h>

#define ENC_ERROR_LIST_LEN 8

typedef struct Locale {
    char *locale;
    char *encoding;
} Locale;

static Locale locales[] = {
    {"aa_DJ", "ISO8859-1"},
    {"af_ZA", "ISO8859-1"},
    {"an_ES", "ISO8859-15"},
    {"ar_AE", "ISO8859-6"},
    {"ar_BH", "ISO8859-6"},
    {"ar_DZ", "ISO8859-6"},
    {"ar_EG", "ISO8859-6"},
    {"ar_IQ", "ISO8859-6"},
    {"ar_JO", "ISO8859-6"},
    {"ar_KW", "ISO8859-6"},
    {"ar_LB", "ISO8859-6"},
    {"ar_LY", "ISO8859-6"},
    {"ar_MA", "ISO8859-6"},
    {"ar_OM", "ISO8859-6"},
    {"ar_QA", "ISO8859-6"},
    {"ar_SA", "ISO8859-6"},
    {"ar_SD", "ISO8859-6"},
    {"ar_SY", "ISO8859-6"},
    {"ar_TN", "ISO8859-6"},
    {"ar_YE", "ISO8859-6"},
    {"ast_ES", "ISO8859-15"},
    {"be_BY", "CP1251"},
    {"bg_BG", "CP1251"},
    {"br_FR", "ISO8859-15"},
    {"br_FR@euro", "ISO8859-15"},
    {"bs_BA", "ISO8859-2"},
    {"ca_AD", "ISO8859-15"},
    {"ca_ES", "ISO8859-15"},
    {"ca_ES@euro", "ISO8859-15"},
    {"ca_FR", "ISO8859-15"},
    {"ca_IT", "ISO8859-15"},
    {"cs_CZ", "ISO8859-2"},
    {"cy_GB", "ISO8859-14"},
    {"da_DK", "ISO8859-1"},
    {"de_AT", "ISO8859-15"},
    {"de_AT@euro", "ISO8859-15"},
    {"de_BE", "ISO8859-15"},
    {"de_BE@euro", "ISO8859-15"},
    {"de_CH", "ISO8859-1"},
    {"de_DE", "ISO8859-15"},
    {"de_DE@euro", "ISO8859-15"},
    {"de_LU", "ISO8859-15"},
    {"de_LU@euro", "ISO8859-15"},
    {"el_GR", "ISO8859-7"},
    {"el_CY", "ISO8859-7"},
    {"en_AU", "ISO8859-1"},
    {"en_BW", "ISO8859-1"},
    {"en_CA", "ISO8859-1"},
    {"en_DK", "ISO8859-1"},
    {"en_GB", "ISO8859-1"},
    {"en_HK", "ISO8859-1"},
    {"en_IE", "ISO8859-15"},
    {"en_IE@euro", "ISO8859-15"},
    {"en_NZ", "ISO8859-1"},
    {"en_PH", "ISO8859-1"},
    {"en_SG", "ISO8859-1"},
    {"en_US", "ISO8859-1"},
    {"en_ZA", "ISO8859-1"},
    {"en_ZW", "ISO8859-1"},
    {"es_AR", "ISO8859-1"},
    {"es_BO", "ISO8859-1"},
    {"es_CL", "ISO8859-1"},
    {"es_CO", "ISO8859-1"},
    {"es_CR", "ISO8859-1"},
    {"es_DO", "ISO8859-1"},
    {"es_EC", "ISO8859-1"},
    {"es_ES", "ISO8859-15"},
    {"es_ES@euro", "ISO8859-15"},
    {"es_GT", "ISO8859-1"},
    {"es_HN", "ISO8859-1"},
    {"es_MX", "ISO8859-1"},
    {"es_NI", "ISO8859-1"},
    {"es_PA", "ISO8859-1"},
    {"es_PE", "ISO8859-1"},
    {"es_PR", "ISO8859-1"},
    {"es_PY", "ISO8859-1"},
    {"es_SV", "ISO8859-1"},
    {"es_US", "ISO8859-1"},
    {"es_UY", "ISO8859-1"},
    {"es_VE", "ISO8859-1"},
    {"et_EE", "ISO8859-15"},
    {"et_EE.ISO8859-15", "ISO8859-15"},
    {"eu_ES", "ISO8859-15"},
    {"eu_ES@euro", "ISO8859-15"},
    {"fi_FI", "ISO8859-15"},
    {"fi_FI@euro", "ISO8859-15"},
    {"fo_FO", "ISO8859-1"},
    {"fr_BE", "ISO8859-15"},
    {"fr_BE@euro", "ISO8859-15"},
    {"fr_CA", "ISO8859-15"},
    {"fr_CH", "ISO8859-1"},
    {"fr_FR", "ISO8859-15"},
    {"fr_FR@euro", "ISO8859-15"},
    {"fr_LU", "ISO8859-15"},
    {"fr_LU@euro", "ISO8859-15"},
    {"ga_IE", "ISO8859-15"},
    {"ga_IE@euro", "ISO8859-15"},
    {"gd_GB", "ISO8859-15"},
    {"gl_ES", "ISO8859-15"},
    {"gl_ES@euro", "ISO8859-15"},
    {"gv_GB", "ISO8859-1"},
    {"he_IL", "ISO8859-8"},
    {"hr_HR", "ISO8859-2"},
    {"hsb_DE", "ISO8859-2"},
    {"hu_HU", "ISO8859-2"},
    {"hy_AM.ARMSCII-8", "ARMSCII-8"},
    {"id_ID", "ISO8859-1"},
    {"is_IS", "ISO8859-1"},
    {"it_CH", "ISO8859-1"},
    {"it_IT", "ISO8859-15"},
    {"it_IT@euro", "ISO8859-15"},
    {"iw_IL", "ISO8859-8"},
    {"ja_JP.EUC-JP", "EUC-JP"},
    {"ka_GE", "GEORGIAN-PS"},
    {"kk_KZ", "PT154"},
    {"kl_GL", "ISO8859-1"},
    {"ko_KR.EUC-KR", "EUC-KR"},
    {"ku_TR", "ISO8859-9"},
    {"kw_GB", "ISO8859-1"},
    {"lg_UG", "ISO8859-10"},
    {"lt_LT", "ISO8859-13"},
    {"lv_LV", "ISO8859-13"},
    {"mg_MG", "ISO8859-15"},
    {"mi_NZ", "ISO8859-13"},
    {"mk_MK", "ISO8859-5"},
    {"ms_MY", "ISO8859-1"},
    {"mt_MT", "ISO8859-3"},
    {"nb_NO", "ISO8859-1"},
    {"nl_BE", "ISO8859-15"},
    {"nl_BE@euro", "ISO8859-15"},
    {"nl_NL", "ISO8859-15"},
    {"nl_NL@euro", "ISO8859-15"},
    {"nn_NO", "ISO8859-1"},
    {"oc_FR", "ISO8859-1"},
    {"om_KE", "ISO8859-1"},
    {"pl_PL", "ISO8859-2"},
    {"pt_BR", "ISO8859-1"},
    {"pt_PT", "ISO8859-15"},
    {"pt_PT@euro", "ISO8859-15"},
    {"ro_RO", "ISO8859-2"},
    {"ru_RU.KOI8-R", "KOI8-R"},
    {"ru_RU", "ISO8859-5"},
    {"ru_UA", "KOI8-U"},
    {"sk_SK", "ISO8859-2"},
    {"sl_SI", "ISO8859-2"},
    {"so_DJ", "ISO8859-1"},
    {"so_KE", "ISO8859-1"},
    {"so_SO", "ISO8859-1"},
    {"sq_AL", "ISO8859-1"},
    {"st_ZA", "ISO8859-1"},
    {"sv_FI", "ISO8859-15"},
    {"sv_FI@euro", "ISO8859-15"},
    {"sv_SE", "ISO8859-1"},
    {"tg_TJ", "KOI8-T"},
    {"th_TH", "TIS-620"},
    {"tl_PH", "ISO8859-1"},
    {"tr_CY", "ISO8859-9"},
    {"tr_TR", "ISO8859-9"},
    {"uk_UA", "KOI8-U"},
    {"uz_UZ", "ISO8859-1"},
    {"wa_BE", "ISO8859-15"},
    {"wa_BE@euro", "ISO8859-15"},
    {"xh_ZA", "ISO8859-1"},
    {"yi_US", "CP1255"},
    {"zh_CN.GB18030", "GB18030"},
    {"zh_CN.GBK", "GBK"},
    {"zh_CN", "GB2312"},
    {"zh_HK", "BIG5-HKSCS"},
    {"zh_SG.GBK", "GBK"},
    {"zh_SG", "GB2312"},
    {"zh_TW.EUC-TW", "EUC-TW"},
    {"zh_TW", "BIG5"},
    {"zu_ZA", "ISO8859-1"},
    {NULL, NULL}
};

/* Maximum frequency in miliseconds of checking for external modifications.
   The periodic check is only performed on buffer modification, and the check
   interval is only to prevent checking on every keystroke in case of a file
   system which is slow to process stat requests (which I'm not sure exists) */
#define MOD_CHECK_INTERVAL 3000

static int doSave(WindowInfo *window, Boolean setEncAttr);
static void safeClose(WindowInfo *window);
static int doOpen(WindowInfo *window, const char *name, const char *path,
     const char *encoding, const char *filter_name, int flags);
static void backupFileName(WindowInfo *window, char *name, size_t len);
static int writeBckVersion(WindowInfo *window);
static int bckError(WindowInfo *window, const char *errString, const char *file);
static int fileWasModifiedExternally(WindowInfo *window);
static const char *errorString(void);
static void addWrapNewlines(WindowInfo *window);
static void setFormatCB(Widget w, XtPointer clientData, XtPointer callData);
static void addWrapCB(Widget w, XtPointer clientData, XtPointer callData);
static int cmpWinAgainstFile(WindowInfo *window, const char *fileName);
static int min(int i1, int i2);
static void modifiedWindowDestroyedCB(Widget w, XtPointer clientData,
    XtPointer callData);
static void forceShowLineNumbers(WindowInfo *window);


WindowInfo *EditNewFile(WindowInfo *inWindow, char *geometry, int iconic,
        const char *languageMode, const char *defaultPath)
{
    char name[MAXPATHLEN];
    WindowInfo *window;
    size_t pathlen;
    char *path;

    /*... test for creatability? */
    
    /* Find a (relatively) unique name for the new file */
    UniqueUntitledName(name);

    /* create new window/document */
    if (inWindow)
	window = CreateDocument(inWindow, name);
    else 
	window = CreateWindow(name, geometry, iconic);

    path = window->path;
    strcpy(window->filename, name);
    strcpy(path, (defaultPath && *defaultPath) ? defaultPath : GetCurrentDir());
    pathlen = strlen(window->path);
    
    /* do we have a "/" at the end? if not, add one */
    if (0 < pathlen && path[pathlen - 1] != '/' && pathlen < MAXPATHLEN - 1) {
        strcpy(&path[pathlen], "/");
    }
    
    SetWindowModified(window, FALSE);
    CLEAR_ALL_LOCKS(window->lockReasons);
    UpdateWindowReadOnly(window);
    UpdateStatsLine(window);
    UpdateWindowTitle(window);
    RefreshTabState(window);
    
    if (languageMode == NULL) 
    	DetermineLanguageMode(window, True);
    else
	SetLanguageMode(window, FindLanguageMode(languageMode), True);
	
    ShowTabBar(window, GetShowTabBar(window));

    if (iconic && IsIconic(window))
        RaiseDocument(window);
    else
        RaiseDocumentWindow(window);
	
    SortTabBar(window);
    return window;
}

static void ApplyEditorConfig(WindowInfo *window, EditorConfig ec) {
    // apply editorconfig
    char *params[1];
    char numStr[25];

    if(ec.indent_size > 0 && ec.tab_width == 0) {
        ec.tab_width = ec.indent_size;
    }

    // indent style / tab width
    int indent_size_set = 0;
    if(ec.indent_style == EC_TAB) {
        params[0] = numStr;
        snprintf(numStr, 24, "%d", 1);
        XtCallActionProc(window->textArea, "set_use_tabs", NULL, params, 1);
    } else if(ec.indent_style == EC_SPACE) {
        int emTabDist = ec.indent_size;
        if(emTabDist == 0) {
            XtVaGetValues(window->textArea, textNemulateTabs, &emTabDist, NULL);
            if(emTabDist == 0) {
                emTabDist = BufGetTabDistance(window->buffer);
            }
        }

        params[0] = numStr;
        snprintf(numStr, 24, "%d", 0);
        XtCallActionProc(window->textArea, "set_use_tabs", NULL, params, 1);

        params[0] = numStr;
        snprintf(numStr, 24, "%d", emTabDist);
        XtCallActionProc(window->textArea, "set_em_tab_dist", NULL, params, 1);
        indent_size_set = 1;
    }

    if(ec.tab_width > 0) {
        params[0] = numStr;
        snprintf(numStr, 24, "%d", ec.tab_width);
        XtCallActionProc(window->textArea, "set_tab_dist", NULL, params, 1);
    }
    
    if(ec.indent_size > 0 && !indent_size_set) {
        params[0] = numStr;
        snprintf(numStr, 24, "%d", ec.indent_size);
        XtCallActionProc(window->textArea, "set_em_tab_dist", NULL, params, 1);
    }

    // newline
    switch(ec.end_of_line) {
        default: break;
        case EC_LF: {
            window->fileFormat = UNIX_FILE_FORMAT;
            break;
        }
        case EC_CR: {
            window->fileFormat = MAC_FILE_FORMAT;
            break;
        }
        case EC_CRLF: {
            window->fileFormat = DOS_FILE_FORMAT;
            break;
        }
    }
}

/*
** Open an existing file specified by name and path.  Use the window inWindow
** unless inWindow is NULL or points to a window which is already in use
** (displays a file other than Untitled, or is Untitled but modified).  Flags
** can be any of:
**
**	CREATE: 		If file is not found, (optionally) prompt the
**				user whether to create
**	SUPPRESS_CREATE_WARN	When creating a file, don't ask the user
**	PREF_READ_ONLY		Make the file read-only regardless
**
** If languageMode is passed as NULL, it will be determined automatically
** from the file extension or file contents.
**
** If bgOpen is True, then the file will be open in background. This
** works in association with the SetLanguageMode() function that has
** the syntax highlighting deferred, in order to speed up the file-
** opening operation when multiple files are being opened in succession. 
*/
WindowInfo *EditExistingFile(WindowInfo *inWindow, const char *name,
        const char *path, const char *encoding, const char *filter, int flags,
        char *geometry, int iconic, const char *languageMode, int tabbed,
        int bgOpen)
{
    WindowInfo *window;
    char fullname[MAXPATHLEN];
    
    /* first look to see if file is already displayed in a window */
    window = FindWindowWithFile(name, path);
    if (window != NULL) {
    	if (!bgOpen) {
	    if (iconic)
		RaiseDocument(window);
	    else
		RaiseDocumentWindow(window);
    	}	    
	return window;
    }
    
    /* If an existing window isn't specified; or the window is already
       in use (not Untitled or Untitled and modified), or is currently
       busy running a macro; create the window */
    if (inWindow == NULL) {
	window = CreateWindow(name, geometry, iconic);
    }
    else if (inWindow->filenameSet || inWindow->fileChanged ||
	    inWindow->macroCmdData != NULL) {
	if (tabbed) {
	    window = CreateDocument(inWindow, name);
    	}
	else {
	    window = CreateWindow(name, geometry, iconic);
	}
    }
    else {
    	/* open file in untitled document */
    	window = inWindow;
    	strcpy(window->path, path);
    	strcpy(window->filename, name); 
        if(encoding) {
            SetEncoding(window, encoding);
        } else {
            window->encoding[0] = '\0';
        }
        
        if (!iconic && !bgOpen) {
            RaiseDocumentWindow(window);
        }
    }
    
    // look for .editorconfig
    EditorConfig ec;
    if(GetEditorConfig()) {
        ec = EditorConfigGet(path, name);
    } else {
        memset(&ec, 0, sizeof(EditorConfig));
    }
    if(ec.charset && !encoding) {
        encoding = ec.charset;
        if(ec.bom != EC_BOM_UNSET) {
            window->bom = True;
        }
    }
    
    /* Open the file */
    if (!doOpen(window, name, path, encoding, filter, flags)) {
	/* The user may have destroyed the window instead of closing the 
	   warning dialog; don't close it twice */
	safeClose(window);
	
        if(ec.charset) {
            free(ec.charset);
        }
    	return NULL;
    }
    forceShowLineNumbers(window);

    /* Decide what language mode to use, trigger language specific actions */
    if (languageMode == NULL) 
    	DetermineLanguageMode(window, True);
    else
	SetLanguageMode(window, FindLanguageMode(languageMode), True);

    /* update tab label and tooltip */
    RefreshTabState(window);
    SortTabBar(window);
    ShowTabBar(window, GetShowTabBar(window));
    
    if (!bgOpen)
        RaiseDocument(window);

    /* Bring the title bar and statistics line up to date, doOpen does
       not necessarily set the window title or read-only status */
    UpdateWindowTitle(window);
    UpdateWindowReadOnly(window);
    UpdateStatsLine(window);
    
    /* Add the name to the convenience menu of previously opened files */
    strcpy(fullname, path);
    strcat(fullname, name);
    if(GetPrefAlwaysCheckRelTagsSpecs())
      	AddRelTagsFile(GetPrefTagFile(), path, TAG);
    AddToPrevOpenMenu(fullname);
    
    if(ec.found) {
        ApplyEditorConfig(window, ec);
    }
    
    if(ec.charset) {
        free(ec.charset);
    }
    
    return window;
}

void RevertToSaved(WindowInfo *window, char *newEncoding)
{
    char name[MAXPATHLEN], path[MAXPATHLEN];
    char *encoding;
    int i;
    int insertPositions[MAX_PANES], topLines[MAX_PANES];
    int horizOffsets[MAX_PANES];
    int openFlags = 0;
    Widget text;
    
    /* Can't revert untitled windows */
    if (!window->filenameSet)
    {
        DialogF(DF_WARN, window->shell, 1, "Error",
                "Window '%s' was never saved, can't re-read", "OK",
                window->filename);
        return;
    }
    
    /* save insert & scroll positions of all of the panes to restore later */
    for (i=0; i<=window->nPanes; i++) {
    	text = i==0 ? window->textArea : window->textPanes[i-1];
    	insertPositions[i] = TextGetCursorPos(text);
    	TextGetScroll(text, &topLines[i], &horizOffsets[i]);
    }

    /* re-read the file, update the window title if new file is different */
    strcpy(name, window->filename);
    strcpy(path, window->path);
    
    if(newEncoding) {
        encoding = newEncoding;
    } else if(window->encoding[0] != '\0') {
        encoding = window->encoding;
    } else {
        encoding = NULL;
    }
    
    
    RemoveBackupFile(window);
    ClearUndoList(window);
    openFlags |= IS_USER_LOCKED(window->lockReasons) && !IS_ENCODING_LOCKED(window->lockReasons) ? PREF_READ_ONLY : 0;
    if (!doOpen(window, name, path, encoding, window->filter, openFlags)) {
	/* This is a bit sketchy.  The only error in doOpen that irreperably
            damages the window is "too much binary data".  It should be
            pretty rare to be reverting something that was fine only to find
            that now it has too much binary data. */
        if (!window->fileMissing)
	    safeClose(window);
        else {
            /* Treat it like an externally modified file */
            window->lastModTime=0;
            window->fileMissing=FALSE;
        }
    	return;
    }
    forceShowLineNumbers(window);
    UpdateWindowTitle(window);
    UpdateWindowReadOnly(window);
    
    /* restore the insert and scroll positions of each pane */
    for (i=0; i<=window->nPanes; i++) {
    	text = i==0 ? window->textArea : window->textPanes[i-1];
	TextSetCursorPos(text, insertPositions[i]);
	TextSetScroll(text, topLines[i], horizOffsets[i]);
    }
}

/*
** Checks whether a window is still alive, and closes it only if so.
** Intended to be used when the file could not be opened for some reason.
** Normally the window is still alive, but the user may have closed the 
** window instead of the error dialog. In that case, we shouldn't close the 
** window a second time.
*/
static void safeClose(WindowInfo *window)
{
    WindowInfo* p = WindowList;
    while(p) {
        if (p == window) {
	    CloseWindow(window);
            return;
        }
        p = p->next;
    }
}

static char bom_utf8[3] = { (char)0xEF, (char)0xBB, (char)0xBF };
static char bom_utf16be[2] = { (char)0xFE, (char)0xFF };
static char bom_utf16le[2] = { (char)0xFF, (char)0xFE };
static char bom_utf32be[4] = { (char)0, (char)0, (char)0xFE, (char)0xFF };
static char bom_utf32le[4] = { (char)0xFF, (char)0xFE, (char)0, (char)0 };
static char bom_gb18030[4] = { (char)0x84, (char)0x31, (char)0x95, (char)0x33 };
static char bom_utfebcdic[4] = { (char)0xDD, (char)0x73, (char)0x66, (char)0x73 };

typedef size_t(*ConvertFunc)(iconv_t, char **, size_t *, char **, size_t *);

/*
 * a function with an iconv like interface but it just copies bytes
 */
size_t copyBytes(
        iconv_t cd,
        char **inbuf,
        size_t *inbytesleft,
        char **outbuf,
        size_t *outbytesleft)
{
    if(!inbuf || !outbuf) {
        return 0;
    }
    size_t in = *inbytesleft;
    size_t out = *outbytesleft;
    size_t cp = in > out ? out : in;
    memcpy(*outbuf, *inbuf, cp);
    *inbuf = (*inbuf) + cp;
    *outbuf = (*outbuf) + cp;
    *inbytesleft = in - cp;
    *outbytesleft = out - cp;
    return 0;
}


#define IO_BUFSIZE 2048

static int doOpen(WindowInfo *window, const char *name, const char *path,
     const char *encoding, const char *filter_name, int flags)
{
    char fullname[MAXPATHLEN];
    struct stat statbuf;
    int fileLen, readLen;
    char *fileString, *c;
    char buf[IO_BUFSIZE];
    FILE *fp = NULL;
    FileStream *stream = NULL;;
    int fd;
    int resp;
    int err;
    
    // make sure, encoding doesn't point to window->encoding
    char encoding_buffer[MAX_ENCODING_LENGTH];
    if(encoding == window->encoding) {
        size_t enclen = strlen(window->encoding);
        if(enclen > MAX_ENCODING_LENGTH) {
            enclen = MAX_ENCODING_LENGTH-1;
        }
        memcpy(encoding_buffer, window->encoding, enclen);
        encoding_buffer[enclen] = 0;
        encoding = encoding_buffer;
    }
    
    /* initialize lock reasons */
    CLEAR_ALL_LOCKS(window->lockReasons);
    
    /* Update the window data structure */
    strcpy(window->filename, name);
    strcpy(window->path, path);
    window->encoding[0] = '\0';
    window->filenameSet = TRUE;
    window->fileMissing = TRUE;

    /* Get the full name of the file */
    strcpy(fullname, path);
    strcat(fullname, name);
    
    /* Open the file */
#ifndef DONT_USE_ACCESS  
                   /* The only advantage of this is if you use clearcase,
    	    	      which messes up the mtime of files opened with r+,
		      even if they're never actually written.
		      To avoid requiring special builds for clearcase users,
		      this is now the default. */
    {
	if ((fp = fopen(fullname, "r")) != NULL) {
    	    if(access(fullname, W_OK) != 0)
                SET_PERM_LOCKED(window->lockReasons, TRUE);
#else
    fp = fopen(fullname, "rb+");
    if (fp == NULL) {
    	/* Error opening file or file is not writeable */
	fp = fopen(fullname, "rb");
	if (fp != NULL) {
	    /* File is read only */
            SET_PERM_LOCKED(window->lockReasons, TRUE);
#endif
	} else if (flags & CREATE && errno == ENOENT) {
	    /* Give option to create (or to exit if this is the only window) */
	    if (!(flags & SUPPRESS_CREATE_WARN)) {
                /* on Solaris 2.6, and possibly other OSes, dialog won't 
		   show if parent window is iconized. */
                RaiseShellWindow(window->shell, False);

                /* ask user for next action if file not found */
                if (WindowList == window && window->next == NULL) {
                    resp = DialogF(DF_WARN, window->shell, 3, "New File",
                            "Can't open %s:\n%s", "New File", "Cancel",
                            "Exit NEdit", fullname, errorString());
                } 
	        else {
                    resp = DialogF(DF_WARN, window->shell, 2, "New File",
                            "Can't open %s:\n%s", "New File", "Cancel", fullname,
                            errorString());
                }

                if (resp == 2) {
                    return FALSE;
                } 
                else if (resp == 3) {
                    exit(EXIT_SUCCESS);
                }
            }
        
            /* Test if new file can be created */
            if ((fd = creat(fullname, 0666)) == -1) {
                DialogF(DF_ERR, window->shell, 1, "Error creating File",
                        "Can't create %s:\n%s", "OK", fullname, errorString());
                return FALSE;
            } 
	    else {
                close(fd);
                remove(fullname);
            }

	    SetWindowModified(window, FALSE);
            if ((flags & PREF_READ_ONLY) != 0) {
                SET_USER_LOCKED(window->lockReasons, TRUE);
            }
	    UpdateWindowReadOnly(window);
	    return TRUE;
        }
	else {
            /* A true error */
            DialogF(DF_ERR, window->shell, 1, "Error opening File",
                    "Could not open %s%s:\n%s", "OK", path, name,
                    errorString());
            return FALSE;
        }
    }
     
    /* Get the length of the file, the protection mode, and the time of the
       last modification to the file */
    if (fstat(fileno(fp), &statbuf) != 0) {
        fclose(fp);
        window->filenameSet = FALSE; /* Temp. prevent check for changes. */
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Error opening %s", "OK", name);
        window->filenameSet = TRUE;
        return FALSE;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        fclose(fp);
        window->filenameSet = FALSE; /* Temp. prevent check for changes. */
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Can't open directory %s", "OK", name);
        window->filenameSet = TRUE;
        return FALSE;
    }

#ifdef S_ISBLK
    if (S_ISBLK(statbuf.st_mode)) {
        fclose(fp);
        window->filenameSet = FALSE; /* Temp. prevent check for changes. */
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Can't open block device %s", "OK", name);
        window->filenameSet = TRUE;
        return FALSE;
    }
#endif
    fileLen = statbuf.st_size;
    
    IOFilter* filter = GetFilterFromName(filter_name);
    char *filter_cmd = NULL;
    if(filter && filter->cmdin && strlen(filter->cmdin) > 0) {
        filter_cmd = filter->cmdin;
    }
    stream = filestream_open_r(window->shell, fp, filter_cmd);
    
    char *enc_attr = NULL;
    
    if(!encoding) {
        ssize_t attrlen = 0;
        enc_attr = xattr_get(fullname, "charset", &attrlen);
        /* enc_attr is NOT null-terminated */
        if(enc_attr) {
            if(attrlen > 0) {
                char *enc_attr_str = NEditMalloc(attrlen + 1);
                memcpy(enc_attr_str, enc_attr, attrlen);
                enc_attr_str[attrlen] = '\0';
                
                // trim the encoding string
                char *enc_trim = enc_attr_str;
                size_t etlen = attrlen;
                while(etlen > 0 && isspace(*enc_trim)) {
                    enc_trim++;
                    etlen--;
                }
                while(etlen > 0 && isspace(enc_trim[etlen-1])) {
                    etlen--;
                }
                enc_trim[etlen] = '\0';
                
                encoding = enc_trim;
                free(enc_attr);
                // keep the original pointer for NEditFree
                enc_attr = enc_attr_str;
            } else {
                free(enc_attr);
                enc_attr = NULL;
            }
        }
    }
    
    int checkBOM = 1;
    int checkEncoding = 0;
    if(encoding) {
        /* check if the encoding string starts with UTF */
        if(strlen(encoding) < 3) {
            /* no UTF encoding */
            checkBOM = 0;
        } else {
            char encpre[4];
            encpre[0] = encoding[0];
            encpre[1] = encoding[1];
            encpre[2] = encoding[2];
            encpre[3] = 0;
            if(strcasecmp(encpre, "UTF")) {
                // encoding doesn't start with "UTF" -> no BOM
                checkBOM = 0;
            }
        }
        if(!strcasecmp(encoding, "GB18030")) {
            checkBOM = 1; /* GB18030 is unicode and could have a BOM */
        }
    } else {
        /* file has no extended attributes, use locale charset */
        encoding = GetPrefDefaultCharset();
        checkEncoding = 1;
    }
    
    char *setEncoding = NULL;
    int hasBOM = 0;
    if(checkBOM) {
        /* read Byte Order Mark */
        int bom = 0;
        size_t r = filestream_read(buf, 4, stream);
        do {
            if(r >= 4) {
                bom = 4;
                if(!memcmp(buf, bom_utf32be, 4)) {
                    setEncoding = "UTF-32BE";
                    hasBOM = TRUE;
                    break;
                } else if(!memcmp(buf, bom_utf32le, 4)) {
                    setEncoding = "UTF-32LE";
                    hasBOM = TRUE;
                    break;
                } else if(!memcmp(buf, bom_gb18030, 4)) {
                    setEncoding = "GB18030";
                    hasBOM = TRUE;
                    break;
                } else if(!memcmp(buf, bom_utfebcdic, 4)) {
                    setEncoding = "UTF-EBCDIC";
                    hasBOM = TRUE;
                    break;
                }
            }
            if(r >= 3) {
                bom = 3;
                if(!memcmp(buf, bom_utf8, 3)) {
                    setEncoding = "UTF-8";
                    hasBOM = TRUE;
                    break;
                }
            }
            if(r >= 2) {
                bom = 2;
                if(!memcmp(buf, bom_utf16be, 2)) {
                    setEncoding = "UTF-16BE";
                    hasBOM = TRUE;
                    break;
                } else if(!memcmp(buf, bom_utf16le, 2)) {
                    setEncoding = "UTF-16LE";
                    hasBOM = TRUE;
                    break;
                }
            }
            bom = 0;
        } while (0);
        filestream_reset(stream, bom);
    }
    if(setEncoding) {
        encoding = setEncoding;
        checkEncoding = 0;
    }
    
    if(checkEncoding) {
        size_t r = filestream_read(buf, IO_BUFSIZE, stream);
        const char *newEnc = DetectEncoding(buf, r, encoding);
        if(newEnc && newEnc != encoding) {
            encoding = newEnc;
        }
        filestream_reset(stream, 0);
    }
    
    
    /* Allocate space for the whole contents of the file (unfortunately) */
    size_t strAlloc = fileLen;
    fileString = (char *)malloc(strAlloc + 1); /* +1 = space for null */
    if (fileString == NULL) {
        filestream_close(stream);
        window->filenameSet = FALSE; /* Temp. prevent check for changes. */
        DialogF(DF_ERR, window->shell, 1, "Error while opening File",
                "File is too large to edit", "OK");
        window->filenameSet = TRUE;
        if(enc_attr) {
            free(enc_attr);
        }
        return FALSE;
    }
    
    iconv_t ic = NULL;
    ConvertFunc strconv = copyBytes;
    if(encoding) {
        ic = iconv_open("UTF-8", encoding);
        if(ic == (iconv_t) -1) {
            filestream_close(stream);
            window->filenameSet = FALSE; /* Temp. prevent check for changes. */
            char *format = "File cannot be converted from %s to UTF8";
            size_t msglen = strlen(format) + strlen(encoding) + 4;
            char *msgbuf = NEditMalloc(msglen);
            snprintf(msgbuf, msglen, format, encoding);
            DialogF(DF_ERR, window->shell, 1, "Error while opening File",
                    msgbuf, "OK");
            NEditFree(msgbuf);
            window->filenameSet = TRUE;
            if(enc_attr) {
                free(enc_attr);
            }
            free(fileString);
            return FALSE;
        }
        strconv = (ConvertFunc)iconv;
        
        /* store encoding in window */
        SetEncoding(window, encoding);
        encoding = window->encoding[0] == '\0' ? window->encoding : NULL;
        
        if(enc_attr) {
            free(enc_attr);
        }
    }
    
    EncError *encErrors = NEditCalloc(ENC_ERROR_LIST_LEN, sizeof(EncError));
    size_t numEncErrors = 0;
    size_t allocEncErrors = ENC_ERROR_LIST_LEN;
    
    err = 0;
    int skipped = 0;
    size_t r = 0;
    readLen = 0;
    char *outStr = fileString;
    size_t prev = 0;
    while((r = filestream_read(buf+prev, IO_BUFSIZE-prev, stream)) > 0 && !err) {
        char *str = buf;
        size_t inleft = prev + r;
        size_t outleft = strAlloc - readLen;   
        prev = 0;  
        while(inleft > 0) { 
            size_t w = outleft;
            size_t rc = strconv(ic, &str, &inleft, &outStr, &outleft);
            w = w - outleft;
            readLen += w;
            
            if(rc == (size_t)-1) {
                /* iconv wants more bytes */
                int extendBuf = 0;
                switch(errno) {
                    default: err = 1; break;
                    case EILSEQ: {
                        if(inleft > 0) {
                            // replace with unicode replacement char
                            if(outleft < 3) {
                                // jump to extendBuf
                                // next strconv run will try to convert
                                // the same character, but this time
                                // we have the space to store the
                                // unicode replacement character
                                extendBuf = 1;
                                break;
                            }
                            
                            outStr[0] = 0xEF;
                            outStr[1] = 0xBF;
                            outStr[2] = 0xBD;
                            
                            // add unconvertible character to the error list
                            if(numEncErrors >= allocEncErrors) {
                                allocEncErrors += 16;
                                encErrors = NEditRealloc(encErrors, allocEncErrors * sizeof(EncError));
                            }
                            encErrors[numEncErrors].c = (unsigned char)*str;
                            encErrors[numEncErrors].pos = outStr - fileString;
                            numEncErrors++;
                            
                            
                            outStr += 3;
                            outleft -= 3;
                            readLen += 3;
                            
                            str++;
                            inleft--;
                            
                            skipped++;
                        }
                        break;
                    }
                    case EINVAL: {
                        memcpy(buf, str, inleft); 
                        prev = inleft;
                        inleft = 0;
                        break;
                    }
                    case E2BIG: {
                        extendBuf = 1;
                        break;
                    }
                }
                
                if(extendBuf) {
                    // either strconv needs more space, or
                    // the unicode replacement character couldn't be stored
                    // -> extend buffer
                    strAlloc += 512;
                    size_t outpos = outStr - fileString;
                    fileString = realloc(fileString, strAlloc + 1);
                    if(!fileString) {
                        err = 1;
                        break;
                    }
                    outStr = fileString + outpos;
                    outleft = strAlloc - readLen;
                }
                
                if(err) {
                    break;
                }
            }
        }
    }
    
    if(ic) {
        iconv_close(ic);
    }
    
    SET_ENCODING_LOCKED(window->lockReasons, FALSE);
    
    int show_err = TRUE;
    int show_infobar = FALSE;
    int lock_enc_error = FALSE;
    if(skipped > 0) {
        /*
        window->filenameSet = FALSE; // Temp. prevent check for changes.
        int btn = DialogF(DF_WARN, window->shell, 2, "Encoding warning",
                "%d non-convertible characters skipped\n"
    		"Open anyway?", "NO", "YES",
                skipped);
        window->filenameSet = TRUE;
        if(btn == 1) {
            show_err = FALSE;
            err = TRUE;
        }
        */
        
        char *lockmsg = "";
        if(GetPrefLockEncodingError()) {
            lockmsg = ": file locked to prevent accidental changes";
            flags = flags | PREF_READ_ONLY;
            lock_enc_error = TRUE;
        }
        
        char msgbuf[256];
        snprintf(msgbuf, 256, "%d non-convertible characters skipped%s", skipped, lockmsg);
        
        show_infobar = TRUE;
        SetEncodingInfoBarLabel(window, msgbuf);
        SetEncErrors(window, encErrors, numEncErrors);
    } else {
        SetEncodingInfoBarLabel(window, "No conversion errors");
        SetEncErrors(window, NULL, 0);
        NEditFree(encErrors);
    }
    
    if (err || ferror(fp)) {
        filestream_close(stream);
        window->filenameSet = FALSE; /* Temp. prevent check for changes. */
        if(show_err) {
             DialogF(DF_ERR, window->shell, 1, "Error while opening File",
                "Error reading %s:\n%s", "OK", name, errorString());
        }
        window->filenameSet = TRUE;
        NEditFree(fileString);
        return FALSE;
    }
    fileString[readLen] = 0;
    
    /* Close the file */
    if (filestream_close(stream) != 0) {
        /* unlikely error */
        DialogF(DF_WARN, window->shell, 1, "Error while opening File",
                "Unable to close file", "OK");
        /* we read it successfully, so continue */
    }
    
    SetFilter(window, filter_name);
    
    /* bom in the window */
    window->bom = hasBOM;

    /* Any errors that happen after this point leave the window in a 
        "broken" state, and thus RevertToSaved will abandon the window if
        window->fileMissing is FALSE and doOpen fails. */    
    window->fileMode = statbuf.st_mode;
    window->fileUid = statbuf.st_uid;
    window->fileGid = statbuf.st_gid;
    window->lastModTime = statbuf.st_mtime;
    window->device = statbuf.st_dev;
    window->inode = statbuf.st_ino;
    window->fileMissing = FALSE;

    /* Detect and convert DOS and Macintosh format files */
    if (GetPrefForceOSConversion()) {
        window->fileFormat = FormatOfFile(fileString);
        if (window->fileFormat == DOS_FILE_FORMAT) {
            ConvertFromDosFileString(fileString, &readLen, NULL);
        } else if (window->fileFormat == MAC_FILE_FORMAT) {
            ConvertFromMacFileString(fileString, readLen);
        }
    }
    
    /* Display the file contents in the text widget */
    window->ignoreModify = True;
    BufSetAll(window->buffer, fileString);
    window->ignoreModify = False;
    
    /* Check that the length that the buffer thinks it has is the same
       as what we gave it.  If not, there were probably nuls in the file.
       Substitute them with another character.  If that is impossible, warn
       the user, make the file read-only, and force a substitution */
    if (window->buffer->length != readLen) {
        if (!BufSubstituteNullChars(fileString, readLen, window->buffer)) {
            resp = DialogF(DF_ERR, window->shell, 2, "Error while opening File",
                    "Too much binary data in file.  You may view\n"
                    "it, but not modify or re-save its contents.", "View",
                    "Cancel");
            if (resp == 2) {
                return FALSE;
            }

            SET_TMBD_LOCKED(window->lockReasons, TRUE);
            for (c = fileString; c < &fileString[readLen]; c++) {
                if (*c == '\0') {
                    *c = (char) 0xfe;
                }
            }
            window->buffer->nullSubsChar = (char) 0xfe;
        }
        window->ignoreModify = True;
        BufSetAll(window->buffer, fileString);
        window->ignoreModify = False;
    }

    /* Release the memory that holds fileString */
    NEditFree(fileString);

    /* Set window title and file changed flag */
    if ((flags & PREF_READ_ONLY) != 0) {
        SET_USER_LOCKED(window->lockReasons, TRUE);
        SET_ENCODING_LOCKED(window->lockReasons, lock_enc_error);
    }
    if (IS_PERM_LOCKED(window->lockReasons)) {
	window->fileChanged = FALSE;
	UpdateWindowTitle(window);
    } else {
	SetWindowModified(window, FALSE);
	if (IS_ANY_LOCKED(window->lockReasons)) {
	    UpdateWindowTitle(window);
        }
    }
    UpdateWindowReadOnly(window);
    
    // show infobar, if needed
    if(show_infobar) {
        ShowEncodingInfoBar(window, 1);
    }
    
    return TRUE;
}   

int IncludeFile(WindowInfo *window, const char *name)
{
    struct stat statbuf;
    int fileLen, readLen;
    char *fileString;
    FILE *fp = NULL;

    /* Open the file */
    fp = fopen(name, "rb");
    if (fp == NULL)
    {
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Could not open %s:\n%s", "OK", name, errorString());
        return FALSE;
    }
    
    /* Get the length of the file */
    if (fstat(fileno(fp), &statbuf) != 0)
    {
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Error opening %s", "OK", name);
        fclose(fp);
        return FALSE;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Can't open directory %s", "OK", name);
        fclose(fp);
        return FALSE;
    }
    fileLen = statbuf.st_size;
 
    /* allocate space for the whole contents of the file */
    fileString = (char *)NEditMalloc(fileLen+1);  /* +1 = space for null */
    if (fileString == NULL)
    {
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "File is too large to include", "OK");
        fclose(fp);
        return FALSE;
    }

    /* read the file into fileString and terminate with a null */
    readLen = fread(fileString, sizeof(char), fileLen, fp);
    if (ferror(fp))
    {
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Error reading %s:\n%s", "OK", name, errorString());
        fclose(fp);
        NEditFree(fileString);
        return FALSE;
    }
    fileString[readLen] = 0;
    
    /* Detect and convert DOS and Macintosh format files */
    switch (FormatOfFile(fileString)) {
        case DOS_FILE_FORMAT:
            ConvertFromDosFileString(fileString, &readLen, NULL);
            break;
        case MAC_FILE_FORMAT:
            ConvertFromMacFileString(fileString, readLen);
            break;
        default:
            /*  Default is Unix, no conversion necessary.  */
            break;
    }
    
    /* If the file contained ascii nulls, re-map them */
    if (!BufSubstituteNullChars(fileString, readLen, window->buffer))
    {
        DialogF(DF_ERR, window->shell, 1, "Error opening File",
                "Too much binary data in file", "OK");
    }
 
    /* close the file */
    if (fclose(fp) != 0)
    {
        /* unlikely error */
        DialogF(DF_WARN, window->shell, 1, "Error opening File",
                "Unable to close file", "OK");
        /* we read it successfully, so continue */
    }
    
    /* insert the contents of the file in the selection or at the insert
       position in the window if no selection exists */
    if (window->buffer->primary.selected)
    	BufReplaceSelected(window->buffer, fileString);
    else
    	BufInsert(window->buffer, TextGetCursorPos(window->lastFocus),
    		fileString);

    /* release the memory that holds fileString */
    NEditFree(fileString);

    return TRUE;
}

/*
** Close all files and windows, leaving one untitled window
*/
int CloseAllFilesAndWindows(void)
{
    while (WindowList->next != NULL || 
    		WindowList->filenameSet || WindowList->fileChanged) {
        /*
         * When we're exiting through a macro, the document running the 
         * macro does not disappear from the list, so we could get stuck
         * in an endless loop if we try to close it. Therefore, we close
         * other documents first. (Note that the document running the macro
         * may get closed because it is in the same window as another 
         * document that gets closed, but it won't disappear; it becomes
         * Untitled.)
         */     
        if (WindowList == MacroRunWindow() && WindowList->next != NULL) {
            if (!CloseAllDocumentInWindow(WindowList->next)) {
                return False;
            }
        }
        else {
            if (!CloseAllDocumentInWindow(WindowList)) {
                return False;
            }
        }
    }

    return TRUE;
}

int CloseFileAndWindow(WindowInfo *window, int preResponse)
{ 
    int response, stat;
    
    /* Make sure that the window is not in iconified state */
    if (window->fileChanged)
    	RaiseDocumentWindow(window);

    /* If the window is a normal & unmodified file or an empty new file, 
       or if the user wants to ignore external modifications then
       just close it.  Otherwise ask for confirmation first. */
    if (!window->fileChanged && 
            /* Normal File */
            ((!window->fileMissing && window->lastModTime > 0) || 
            /* New File*/
             (window->fileMissing && window->lastModTime == 0) ||
            /* File deleted/modified externally, ignored by user. */
            !GetPrefWarnFileMods()))
    {
        CloseWindow(window);
        /* up-to-date windows don't have outstanding backup files to close */
    } else
    {
        if (preResponse == PROMPT_SBC_DIALOG_RESPONSE)
        {
            response = DialogF(DF_WARN, window->shell, 3, "Save File",
            "Save %s before closing?", "Yes", "No", "Cancel", window->filename);
        } else
        {
            response = preResponse;
        }

        if (response == YES_SBC_DIALOG_RESPONSE)
        {
            /* Save */
            stat = SaveWindow(window);
            if (stat)
            {
                CloseWindow(window);
            } else
            {
                return FALSE;
            }
        } else if (response == NO_SBC_DIALOG_RESPONSE)
        {
            /* Don't Save */
            RemoveBackupFile(window);
            CloseWindow(window);
        } else /* 3 == Cancel */
        {
            return FALSE;
        }
    }
    return TRUE;
}

int SaveWindow(WindowInfo *window)
{
    int stat;
    
    /* Try to ensure our information is up-to-date */
    CheckForChangesToFile(window);
    
    /* Return success if the file is normal & unchanged or is a
        read-only file. */
    if ( (!window->fileChanged && !window->fileMissing && 
            window->lastModTime > 0) || 
            IS_ANY_LOCKED_IGNORING_PERM(window->lockReasons))
    	return TRUE;
    /* Prompt for a filename if this is an Untitled window */
    if (!window->filenameSet)
    	return SaveWindowAs(window, NULL);

    /* Check for external modifications and warn the user */
    if (GetPrefWarnFileMods() && fileWasModifiedExternally(window))
    {
        stat = DialogF(DF_WARN, window->shell, 2, "Save File",
        "%s has been modified by another program.\n\n"
        "Continuing this operation will overwrite any external\n"
        "modifications to the file since it was opened in NEdit,\n"
        "and your work or someone else's may potentially be lost.\n\n"
        "To preserve the modified file, cancel this operation and\n"
        "use Save As... to save this file under a different name,\n"
        "or Revert to Saved to revert to the modified version.",
        "Continue", "Cancel", window->filename);
        if (stat == 2)
        {
            /* Cancel and mark file as externally modified */
            window->lastModTime = 0;
            window->fileMissing = FALSE;
            return FALSE;
        }
    }
    
    if (writeBckVersion(window))
    	return FALSE;
    stat = doSave(window, 0);
    if (stat) 
        RemoveBackupFile(window);
    
    return stat;
}
    
int SaveWindowAs(WindowInfo *window, FileSelection *file)
{
    int response, retVal, fileFormat;
    char filename[MAXPATHLEN], pathname[MAXPATHLEN];
    WindowInfo *otherWindow;
    char fullname[MAXPATHLEN];
    
    /* Get the new name for the file */
    FileSelection newFile;
    if (!file) {
        memset(&newFile, 0, sizeof(FileSelection));
        newFile.setenc = True;
        newFile.encoding = strlen(window->encoding) > 0 ? window->encoding : NULL;
        newFile.format = window->fileFormat;
        newFile.writebom = window->bom;
        
	response = PromptForNewFile(window, "Save File As", &newFile, &fileFormat);
	if (response != GFN_OK)
    	    return FALSE;
        window->bom = newFile.writebom;
	window->fileFormat = newFile.format;
        SetFilter(window, newFile.filter);
        size_t pathlen = strlen(newFile.path);
        if(pathlen >= MAXPATHLEN) {
            fprintf(stderr, "Error: Path too long\n");
            NEditFree(newFile.path);
            return FALSE;
        }
        memcpy(fullname, newFile.path, pathlen);
        fullname[pathlen] = '\0';
        NEditFree(newFile.path);
        
        if(newFile.encoding) {
            if(!strcmp(newFile.encoding, "UTF-8")) {
                window->encoding[0] = '\0';
            } else {
                SetEncoding(window, newFile.encoding);
            }
        }
        
        file = &newFile;        
    } else
    {
        strcpy(fullname, file->path);
        if(!strcmp(file->encoding, "UTF-8")) {
            window->encoding[0] = '\0';
        } else {
            SetEncoding(window, file->encoding);
        }
        window->bom = file->writebom;
        window->fileFormat = file->format;
        SetFilter(window, file->filter);
    }

    if (1 == NormalizePathname(fullname))
    {
        return False;
    }
    
    /* Add newlines if requested */
    if (file->addwrap)
    	addWrapNewlines(window);
    
    if (ParseFilename(fullname, filename, pathname) != 0) {
        return FALSE;
    }

    /* If the requested file is this file, just save it and return */
    if (!strcmp(window->filename, filename) &&
    	    !strcmp(window->path, pathname)) {
	if (writeBckVersion(window))
    	    return FALSE;
	return doSave(window, file->setxattr);
    }
    
    /* If the file is open in another window, make user close it.  Note that
       it is possible for user to close the window by hand while the dialog
       is still up, because the dialog is not application modal, so after
       doing the dialog, check again whether the window still exists. */
    otherWindow = FindWindowWithFile(filename, pathname);
    if (otherWindow != NULL)
    {
        response = DialogF(DF_WARN, window->shell, 2, "File open",
        "%s is open in another XNEdit window", "Cancel",
        "Close Other Window", filename);

        if (response == 1)
        {
            return FALSE;
        }
        if (otherWindow == FindWindowWithFile(filename, pathname))
        {
            if (!CloseFileAndWindow(otherWindow, PROMPT_SBC_DIALOG_RESPONSE))
            {
                return FALSE;
            }
        }
    }
    
    /* Destroy the file closed property for the original file */
    DeleteFileClosedProperty(window);

    /* Change the name of the file and save it under the new name */
    RemoveBackupFile(window);
    strcpy(window->filename, filename);
    strcpy(window->path, pathname);
    window->fileMode = 0;
    window->fileUid = 0;
    window->fileGid = 0;
    CLEAR_ALL_LOCKS(window->lockReasons);
    retVal = doSave(window, file->setxattr);
    UpdateWindowReadOnly(window);
    RefreshTabState(window);
    
    /* Add the name to the convenience menu of previously opened files */
    AddToPrevOpenMenu(fullname);
    
    /*  If name has changed, language mode may have changed as well, unless
        it's an Untitled window for which the user already set a language
        mode; it's probably the right one.  */
    if (PLAIN_LANGUAGE_MODE == window->languageMode || window->filenameSet) {
        DetermineLanguageMode(window, False);
    }
    window->filenameSet = True;
    
    /* Update the stats line and window title with the new filename */
    UpdateWindowTitle(window);
    UpdateStatsLine(window);

    SortTabBar(window);
    return retVal;
}

static int getBOM(char *encoding, char **bom)
{
    int len = 0;
    *bom = NULL;
    if(!encoding || strlen(encoding) == 0 || !strcasecmp(encoding, "UTF-8")) {
        *bom = bom_utf8;
        len = 3;
    } else if(!strcasecmp(encoding, "UTF-16BE")) {
        *bom = bom_utf16be;
        len = 2;
    } else if(!strcasecmp(encoding, "UTF-16LE")) {
        *bom = bom_utf16le;
        len = 2;
    } else if(!strcasecmp(encoding, "UTF-32BE")) {
        *bom = bom_utf32be;
        len = 4;
    } else if(!strcasecmp(encoding, "UTF-32LE")) {
        *bom = bom_utf32le;
        len = 4;
    } else if(!strcasecmp(encoding, "GB18030")) {
        *bom = bom_gb18030;
        len = 4;
    } else if(!strcasecmp(encoding, "UTF-EBCDIC")) {
        *bom = bom_utfebcdic;
        len = 4;
    }
    return len;
}

static int doSave(WindowInfo *window, Boolean setEncAttr)
{
    char *fileString = NULL;
    char fullname[MAXPATHLEN];
    struct stat statbuf;
    FILE *fp;
    int fileLen, result;
    
    iconv_t ic = NULL;
    ConvertFunc strconv = copyBytes;
    if(strlen(window->encoding) > 0) {
        ic = iconv_open(window->encoding, "UTF-8");
        if(ic == (iconv_t) -1) {
            DialogF(DF_ERR, window->shell, 1, "Error saving File",
                "The text cannot be converted to %s", "OK", window->encoding);
            return FALSE;
        }
        strconv = (ConvertFunc)iconv;
    }

    /* Get the full name of the file */
    strcpy(fullname, window->path);
    strcat(fullname, window->filename);

    /*  Check for root and warn him if he wants to write to a file with
        none of the write bits set.  */
    if ((0 == getuid())
            && (0 == stat(fullname, &statbuf))
            && !(statbuf.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)))
    {
        result = DialogF(DF_WARN, window->shell, 2, "Writing Read-only File",
                "File '%s' is marked as read-only.\n"
                "Do you want to save anyway?",
                "Save", "Cancel", window->filename);
        if (1 != result)
        {
            return True;
        }
    }


    /* add a terminating newline if the file doesn't already have one for
       Unix utilities which get confused otherwise 
       NOTE: this must be done _before_ we create/open the file, because the
             (potential) buffer modification can trigger a check for file
             changes. If the file is created for the first time, it has
             zero size on disk, and the check would falsely conclude that the
             file has changed on disk, and would pop up a warning dialog */
    if (BufGetCharacter(window->buffer, window->buffer->length - 1) != '\n'
            && window->buffer->length != 0
            && GetPrefAppendLF())
    {
        BufInsert(window->buffer, window->buffer->length, "\n");
    }
    
    /* open the file */
    fp = fopen(fullname, "wb");
    if (fp == NULL)
    {
        result = DialogF(DF_WARN, window->shell, 2, "Error saving File",
                "Unable to save %s:\n%s\n\nSave as a new file?",
                "Save As...", "Cancel",
        window->filename, errorString());

        if (result == 1)
        {
            return SaveWindowAs(window, NULL);
        }
        return FALSE;
    }
    
    /* get the text buffer contents and its length */
    fileString = BufGetAll(window->buffer);
    fileLen = window->buffer->length;
    
    /* If null characters are substituted for, put them back */
    BufUnsubstituteNullChars(fileString, window->buffer);
    
    /* If the file is to be saved in DOS or Macintosh format, reconvert */
    if (window->fileFormat == DOS_FILE_FORMAT)
    {
        if (!ConvertToDosFileString(&fileString, &fileLen))
        {
            DialogF(DF_ERR, window->shell, 1, "Out of Memory",
                    "Out of memory!  Try\nsaving in Unix format", "OK");
            fclose(fp);
            return FALSE;
        }
    } else if (window->fileFormat == MAC_FILE_FORMAT)
    { 
        ConvertToMacFileString(fileString, fileLen);
    }

    /* write to the file */
    IOFilter *filter = GetFilterFromName(window->filter);
    char *filter_cmd = NULL;
    if(filter && filter->cmdout && strlen(filter->cmdout) > 0) {
        filter_cmd = filter->cmdout;
    }
    FileStream *stream = filestream_open_w(window->shell, fp, filter_cmd);
    
    /* write bom if requsted */
    if(window->bom) {
        char *bom;
        int bomLen = getBOM(window->encoding, &bom);
        if(bomLen > 0) {
            if(filestream_write(bom, bomLen, stream) != bomLen) {
                fileLen = 0;
            }
        }
    }
    
    /* convert text if required and write it to the file */
    int skipped = 0;
    int nonreversible = 0;
    int unerr = 0;
    char buf[IO_BUFSIZE];
    char *in = fileString;
    size_t inleft = fileLen;
    while(in) {
        char *out = buf;
        size_t outleft = IO_BUFSIZE;
        size_t w = outleft;
        if (inleft == 0) {
            /* be sure to flush out any partially converted input */
            in = NULL;
        }
        size_t rc = strconv(ic, &in, &inleft, &out, &outleft);
        w -= outleft;
        
        if(w > 0) {
            filestream_write(buf, w, stream);
        }
        
        if(rc == (size_t)-1) {
            size_t skip;
            switch (errno) {
            case EILSEQ:
            case EINVAL:
                /* An invalid multibyte sequence is encountered in the input */
                skip = Utf8CharLen((const unsigned char*)in);
                ++skipped;
                in += skip;
                if(inleft >= skip) {
                    inleft -= skip;
                } else {
                    inleft = 0;
                }
                break;
            case E2BIG:
                /* Conversion succeeded but output buffer is full */
                break;
            default:
            	/* Unknown error encountered */
            	++unerr;
            }
        }

        if (inleft == 0) {
        	/* add # of nonreversible conversions */
        	nonreversible += rc;
        }
    }

    unsigned int eresp = 0;
    if (skipped > 0 || nonreversible > 0 || unerr > 0) {
    	eresp = DialogF(DF_WARN, window->shell, 2, "Encoding warning",
                "%d non-convertible characters skipped\n"
    			"%d non-reversible characters encountered\n"
    			"%d unknown errors occurred\n"
    			"Save anyway?", "YES", "NO",
                skipped, nonreversible, unerr);
    }
    
    if(ic) {
        iconv_close(ic);
    }
    
    if (ferror(fp))
    {
        DialogF(DF_ERR, window->shell, 1, "Error saving File",
                "%s not saved:\n%s", "OK", window->filename, errorString());
    }

    if (ferror(fp) || eresp == 2) {
        filestream_close(stream);
        remove(fullname);
        NEditFree(fileString);
        return FALSE;
    }
    
    if(setEncAttr) {
        size_t encLen = strlen(window->encoding);
        char *encStr;
        char *encCopy = NULL;
        if(encLen == 0) {
            encStr = "utf-8";
            encLen = 5;
        } else {
            encCopy = NEditStrdup(window->encoding);
            for(int i=0;i<encLen;i++) {
                encCopy[i] = tolower(encCopy[i]);
            }
            encStr = encCopy;
        }
        
        if(xattr_set(fullname, "charset", encStr, encLen)) {
            perror("xattr_set failed");
        }
        
        if(encCopy) {
            NEditFree(encCopy);
        }
    } else {
        ssize_t len = 0;
        char *fileAttr = xattr_get(fullname, "charset", &len);
        if(fileAttr) {
            size_t winEncLen = strlen(window->encoding);
            size_t cmpLen = winEncLen > len ? len : winEncLen;
            if(len != winEncLen || memcmp(fileAttr, window->encoding, cmpLen)) {
                if(xattr_remove(fullname, "charset")) {
                    DialogF(DF_ERR, window->shell, 1, "Error saving File",
                            "Cannot remove previous charset attribute");
                }
            }
            free(fileAttr);
        }
    }
    
    /* close the file */
    if (filestream_close(stream) != 0)
    {
        DialogF(DF_ERR, window->shell, 1, "Error closing File",
                "Error closing file:\n%s", "OK", errorString());
        NEditFree(fileString);
        return FALSE;
    }

    /* free the text buffer copy returned from XmTextGetString */
    NEditFree(fileString);

    /* success, file was written */
    SetWindowModified(window, FALSE);
    
    /* update the modification time */
    if (stat(fullname, &statbuf) == 0) {
	window->lastModTime = statbuf.st_mtime;
        window->fileMissing = FALSE;
        window->device = statbuf.st_dev;
        window->inode = statbuf.st_ino;
    } else {
        /* This needs to produce an error message -- the file can't be 
            accessed! */
	window->lastModTime = 0;
        window->fileMissing = TRUE;
        window->device = 0;
        window->inode = 0;
    }

    return TRUE;
}

/*
** Create a backup file for the current window.  The name for the backup file
** is generated using the name and path stored in the window and adding a
** tilde (~) on UNIX and underscore (_) on VMS to the beginning of the name.  
*/
int WriteBackupFile(WindowInfo *window)
{
    char *fileString = NULL;
    char name[MAXPATHLEN];
    FILE *fp;
    int fd, fileLen;
    
    /* Generate a name for the autoSave file */
    backupFileName(window, name, sizeof(name));

    /* remove the old backup file.
       Well, this might fail - we'll notice later however. */
    remove(name);
    
    /* open the file, set more restrictive permissions (using default
        permissions was somewhat of a security hole, because permissions were
        independent of those of the original file being edited */
    if ((fd = open(name, O_CREAT|O_EXCL|O_WRONLY, S_IRUSR | S_IWUSR)) < 0
            || (fp = fdopen(fd, "w")) == NULL)
    {
        DialogF(DF_WARN, window->shell, 1, "Error writing Backup",
                "Unable to save backup for %s:\n%s\n"
                "Automatic backup is now off", "OK", window->filename,
                errorString());
        window->autoSave = FALSE;
        SetToggleButtonState(window, window->autoSaveItem, FALSE, FALSE);
        return FALSE;
    }

    /* get the text buffer contents and its length */
    fileString = BufGetAll(window->buffer);
    fileLen = window->buffer->length;
    
    /* If null characters are substituted for, put them back */
    BufUnsubstituteNullChars(fileString, window->buffer);
    
    /* add a terminating newline if the file doesn't already have one */
    if (fileLen != 0 && fileString[fileLen-1] != '\n')
    	fileString[fileLen++] = '\n'; 	 /* null terminator no longer needed */
    
    /* write out the file */
    fwrite(fileString, sizeof(char), fileLen, fp);
    if (ferror(fp))
    {
        DialogF(DF_ERR, window->shell, 1, "Error saving Backup",
                "Error while saving backup for %s:\n%s\n"
                "Automatic backup is now off", "OK", window->filename,
                errorString());
        fclose(fp);
        remove(name);
        NEditFree(fileString);
        window->autoSave = FALSE;
        return FALSE;
    }
    
    /* close the backup file */
    if (fclose(fp) != 0) {
	NEditFree(fileString);
	return FALSE;
    }

    /* Free the text buffer copy returned from XmTextGetString */
    NEditFree(fileString);

    return TRUE;
}

/*
** Remove the backup file associated with this window
*/
void RemoveBackupFile(WindowInfo *window)
{
    char name[MAXPATHLEN];
    
    /* Don't delete backup files when backups aren't activated. */
    if (window->autoSave == FALSE)
        return;
      
    backupFileName(window, name, sizeof(name));
    remove(name);
}

/*
** Generate the name of the backup file for this window from the filename
** and path in the window data structure & write into name
*/
static void backupFileName(WindowInfo *window, char *name, size_t len)
{
    char bckname[MAXPATHLEN];
    if (window->filenameSet)
    {
        sprintf(name, "%s~%s", window->path, window->filename);
    } else
    {
        strcpy(bckname, "~");
        strncat(bckname, window->filename, MAXPATHLEN - 1);
        PrependHome(bckname, name, len);
    }
}

/*
** If saveOldVersion is on, copies the existing version of the file to
** <filename>.bck in anticipation of a new version being saved.  Returns
** True if backup fails and user requests that the new file not be written.
*/
static int writeBckVersion(WindowInfo *window)
{
    char fullname[MAXPATHLEN], bckname[MAXPATHLEN];
    struct stat statbuf;
    int in_fd, out_fd;
    char *io_buffer;
#define IO_BUFFER_SIZE ((size_t)(1024*1024))

    /* Do only if version backups are turned on */
    if (!window->saveOldVersion) {
    	return False;
    }
    
    /* Get the full name of the file */
    strcpy(fullname, window->path);
    strcat(fullname, window->filename);
    
    /* Generate name for old version */
    if ((strlen(fullname) + 5) > (size_t) MAXPATHLEN) {
        return bckError(window, "file name too long", window->filename);
    }
    if(snprintf(bckname, MAXPATHLEN, "%s.bck", fullname) >= MAXPATHLEN) {
        return FALSE;
    }

    /* Delete the old backup file */
    /* Errors are ignored; we'll notice them later. */
    remove(bckname);

    /* open the file being edited.  If there are problems with the
       old file, don't bother the user, just skip the backup */
    in_fd = open(fullname, O_RDONLY);
    if (in_fd<0) {
    	return FALSE;
    }

    /* Get permissions of the file.
       We preserve the normal permissions but not ownership, extended
       attributes, et cetera. */
    if (fstat(in_fd, &statbuf) != 0) {
        close(in_fd);
	return FALSE;
    }

    /* open the destination file exclusive and with restrictive permissions. */
    out_fd = open(bckname, O_CREAT|O_EXCL|O_TRUNC|O_WRONLY, S_IRUSR | S_IWUSR);
    if (out_fd < 0) {
        close(in_fd);
        return bckError(window, "Error open backup file", bckname);
    }

    /* Set permissions on new file */
    if (fchmod(out_fd, statbuf.st_mode) != 0) {
        close(in_fd);
        close(out_fd);
        remove(bckname);
        return bckError(window, "fchmod() failed", bckname);
    }

    /* Allocate I/O buffer */
    io_buffer = (char*) NEditMalloc(IO_BUFFER_SIZE);
    if (NULL == io_buffer) {
        close(in_fd);
        close(out_fd);
        remove(bckname);
	return bckError(window, "out of memory", bckname);
    }

    /* copy loop */
    for(;;) {
        ssize_t bytes_read;
        ssize_t bytes_written;
        bytes_read = read(in_fd, io_buffer, IO_BUFFER_SIZE);

        if (bytes_read < 0) {
            close(in_fd);
            close(out_fd);
            remove(bckname);
            free(io_buffer);
            return bckError(window, "read() error", window->filename);
        }

        if (0 == bytes_read) {
            break; /* EOF */
        }

        /* write to the file */
        bytes_written = write(out_fd, io_buffer, (size_t) bytes_read);
        if (bytes_written != bytes_read) {
            close(in_fd);
            close(out_fd);
            remove(bckname);
            free(io_buffer);
            return bckError(window, errorString(), bckname);
        }
    }

    /* close the input and output files */
    close(in_fd);
    close(out_fd);

    free(io_buffer);

    return FALSE;
}

/*
** Error processing for writeBckVersion, gives the user option to cancel
** the subsequent save, or continue and optionally turn off versioning
*/
static int bckError(WindowInfo *window, const char *errString, const char *file)
{
    int resp;

    resp = DialogF(DF_ERR, window->shell, 3, "Error writing Backup",
            "Couldn't write .bck (last version) file.\n%s: %s", "Cancel Save",
            "Turn off Backups", "Continue", file, errString);
    if (resp == 1)
    	return TRUE;
    if (resp == 2) {
    	window->saveOldVersion = FALSE;
    	SetToggleButtonState(window, window->saveLastItem, FALSE, FALSE);
    }
    return FALSE;
}

void PrintWindow(WindowInfo *window, int selectedOnly)
{
    textBuffer *buf = window->buffer;
    selection *sel = &buf->primary;
    char *fileString = NULL;
    int fileLen;
    
    /* get the contents of the text buffer from the text area widget.  Add
       wrapping newlines if necessary to make it match the displayed text */
    if (selectedOnly) {
    	if (!sel->selected) {
    	    XBell(TheDisplay, 0);
	    return;
	}
	if (sel->rectangular) {
    	    fileString = BufGetSelectionText(buf);
    	    fileLen = strlen(fileString);
    	} else
    	    fileString = TextGetWrapped(window->textArea, sel->start, sel->end,
    	    	    &fileLen);
    } else
    	fileString = TextGetWrapped(window->textArea, 0, buf->length, &fileLen);
    
    /* If null characters are substituted for, put them back */
    BufUnsubstituteNullChars(fileString, buf);

        /* add a terminating newline if the file doesn't already have one */
    if (fileLen != 0 && fileString[fileLen-1] != '\n')
    	fileString[fileLen++] = '\n'; 	 /* null terminator no longer needed */
    
    /* Print the string */
    PrintString(fileString, fileLen, window->shell, window->filename);

    /* Free the text buffer copy returned from XmTextGetString */
    NEditFree(fileString);
}

/*
** Print a string (length is required).  parent is the dialog parent, for
** error dialogs, and jobName is the print title.
*/
void PrintString(const char *string, int length, Widget parent, const char *jobName)
{
    char tmpFileName[L_tmpnam];    /* L_tmpnam defined in stdio.h */
    FILE *fp;
    int fd;

    /* Generate a temporary file name */
    /*  If the glibc is used, the linker issues a warning at this point. This is
	very thoughtful of him, but does not apply to NEdit. The recommended
	replacement mkstemp(3) uses the same algorithm as NEdit, namely
	    1. Create a filename
	    2. Open the file with the O_CREAT|O_EXCL flags
	So all an attacker can do is a DoS on the print function. */
#ifdef __GLIBC__
    mkstemp(tmpFileName);
#else
    tmpnam(tmpFileName);
#endif
    
    /* open the temporary file */
    if ((fd = open(tmpFileName, O_CREAT|O_EXCL|O_WRONLY, S_IRUSR | S_IWUSR)) < 0 || (fp = fdopen(fd, "w")) == NULL)
    {
        DialogF(DF_WARN, parent, 1, "Error while Printing",
                "Unable to write file for printing:\n%s", "OK",
                errorString());
        return;
    }
    
    /* write to the file */
    fwrite(string, sizeof(char), length, fp);
    if (ferror(fp))
    {
        DialogF(DF_ERR, parent, 1, "Error while Printing",
                "%s not printed:\n%s", "OK", jobName, errorString());
        fclose(fp); /* should call close(fd) in turn! */
        remove(tmpFileName);
        return;
    }
    
    /* close the temporary file */
    if (fclose(fp) != 0)
    {
        DialogF(DF_ERR, parent, 1, "Error while Printing",
                "Error closing temp. print file:\n%s", "OK",
                errorString());
        remove(tmpFileName);
        return;
    }

    /* Print the temporary file, then delete it and return success */
    PrintFile(parent, tmpFileName, jobName);
    remove(tmpFileName);
    return;
}

/*
** Wrapper for GetExistingFilename which uses the current window's path
** (if set) as the default directory.
*/
int PromptForExistingFile(WindowInfo *window, char *prompt, FileSelection *file)
{   
    char *savedDefaultDir;
    int retVal;
    
    /* Temporarily set default directory to window->path, prompt for file,
       then, if the call was unsuccessful, restore the original default
       directory */
    savedDefaultDir = GetFileDialogDefaultDirectory();
    if (*window->path != '\0')
    	SetFileDialogDefaultDirectory(window->path);
    retVal = GetExistingFilename(window->shell, prompt, file);
    if (retVal != GFN_OK)
    	SetFileDialogDefaultDirectory(savedDefaultDir);

    NEditFree(savedDefaultDir);

    return retVal;
}

/*
** Wrapper for HandleCustomNewFileSB which uses the current window's path
** (if set) as the default directory, and asks about embedding newlines
** to make wrapping permanent.
*/
int PromptForNewFile(WindowInfo *window, char *prompt, FileSelection *file,
    	int *fileFormat)
{
    int retVal;
    char *savedDefaultDir;
    
    *fileFormat = window->fileFormat;
    
    /* Temporarily set default directory to window->path, prompt for file,
       then, if the call was unsuccessful, restore the original default
       directory */
    savedDefaultDir = GetFileDialogDefaultDirectory();
    if (*window->path != '\0')
    	SetFileDialogDefaultDirectory(window->path);
    
#ifdef MOTIF_FILEDIALOG
    Arg args[20];
    XmString s1, s2;
    Widget fileSB, wrapToggle;
    Widget formatForm, formatBtns, unixFormat, dosFormat, macFormat;
    
    /* Present a file selection dialog with an added field for requesting
       long line wrapping to become permanent via inserted newlines */
    int n = 0;
    XtSetArg(args[n],
            XmNselectionLabelString,
            s1 = XmStringCreateLocalized("New File Name:")); n++;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;
    XtSetArg(args[n],
            XmNdialogTitle,
            s2 = XmStringCreateSimple(prompt)); n++;
    fileSB = CreateFileSelectionDialog(window->shell,"FileSelect",args,n);
    XmStringFree(s1);
    XmStringFree(s2);
    formatForm = XtVaCreateManagedWidget("formatForm", xmFormWidgetClass,
	    fileSB, NULL);
    formatBtns = XtVaCreateManagedWidget("formatBtns",
            xmRowColumnWidgetClass, formatForm,
            XmNradioBehavior, XmONE_OF_MANY,
            XmNorientation, XmHORIZONTAL,
            XmNpacking, XmPACK_TIGHT,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            NULL);
    XtVaCreateManagedWidget("formatBtns", xmLabelWidgetClass, formatBtns,
	    XmNlabelString, s1=XmStringCreateSimple("Format:"), NULL);
    XmStringFree(s1);
    unixFormat = XtVaCreateManagedWidget("unixFormat",
            xmToggleButtonWidgetClass, formatBtns,
            XmNlabelString, s1 = XmStringCreateSimple("Unix"),
            XmNset, *fileFormat == UNIX_FILE_FORMAT,
            XmNuserData, (XtPointer)UNIX_FILE_FORMAT,
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNmnemonic, 'U',
            NULL);
    XmStringFree(s1);
    XtAddCallback(unixFormat, XmNvalueChangedCallback, setFormatCB,
    	    fileFormat);
    dosFormat = XtVaCreateManagedWidget("dosFormat",
            xmToggleButtonWidgetClass, formatBtns,
            XmNlabelString, s1 = XmStringCreateSimple("DOS"),
            XmNset, *fileFormat == DOS_FILE_FORMAT,
            XmNuserData, (XtPointer)DOS_FILE_FORMAT,
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNmnemonic, 'O',
            NULL);
    XmStringFree(s1);
    XtAddCallback(dosFormat, XmNvalueChangedCallback, setFormatCB,
    	    fileFormat);
    macFormat = XtVaCreateManagedWidget("macFormat",
            xmToggleButtonWidgetClass, formatBtns,
            XmNlabelString, s1 = XmStringCreateSimple("Macintosh"),
            XmNset, *fileFormat == MAC_FILE_FORMAT,
            XmNuserData, (XtPointer)MAC_FILE_FORMAT,
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNmnemonic, 'M',
            NULL);
    XmStringFree(s1);
    XtAddCallback(macFormat, XmNvalueChangedCallback, setFormatCB,
    	    fileFormat);
    if (window->wrapMode == CONTINUOUS_WRAP) {
	wrapToggle = XtVaCreateManagedWidget("addWrap",
                xmToggleButtonWidgetClass, formatForm,
                XmNlabelString, s1 = XmStringCreateSimple("Add line breaks where wrapped"),
                XmNalignment, XmALIGNMENT_BEGINNING,
                XmNmnemonic, 'A',
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, formatBtns,
                XmNleftAttachment, XmATTACH_FORM,
                NULL);
	XtAddCallback(wrapToggle, XmNvalueChangedCallback, addWrapCB,
    	    	&file->addwrap);
	XmStringFree(s1);
    }
    file->addwrap = False;
    XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_LABEL),
            XmNmnemonic, 'l',
            XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_TEXT),
            NULL);
    XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_DIR_LIST_LABEL),
            XmNmnemonic, 'D',
            XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_DIR_LIST),
            NULL);
    XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_LIST_LABEL),
            XmNmnemonic, 'F',
            XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_LIST),
            NULL);
    XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_SELECTION_LABEL),
            XmNmnemonic, prompt[strspn(prompt, "lFD")],
            XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_TEXT),
            NULL);
    AddDialogMnemonicHandler(fileSB, FALSE);
    RemapDeleteKey(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_TEXT));
    RemapDeleteKey(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_TEXT));
    char fullname[MAXPATHLEN];
    retVal = HandleCustomNewFileSB(fileSB, fullname,
    	    window->filenameSet ? window->filename : NULL);
    if(retVal == GFN_OK) {
        file->path = NEditStrdup(fullname);
        if(!file->encoding) {
            file->encoding = "";
        }
    }
#else
    char *prevPath = NULL;
    if(window->path[0] != '\0' && window->filename[0] != '\0' && window->filenameSet) {
        size_t plen = strlen(window->path);
        size_t nlen = strlen(window->filename);
        prevPath = NEditMalloc(plen + nlen + 2);
        memcpy(prevPath, window->path, plen);
        if(window->path[plen-1] != '/') {
            prevPath[plen] = '/';
            plen++;
        }
        memcpy(prevPath+plen, window->filename, nlen);
        prevPath[plen+nlen] = '\0';
    }
    
    file->path = prevPath;
    retVal = GetNewFilename(window->shell, prompt, file, "");
    if(prevPath) {
        NEditFree(prevPath);
    }
#endif

    if (retVal != GFN_OK)
    	SetFileDialogDefaultDirectory(savedDefaultDir);

    NEditFree(savedDefaultDir);

    return retVal;
}

/*
** Find a name for an untitled file, unique in the name space of in the opened
** files in this session, i.e. Untitled or Untitled_nn, and write it into
** the string "name".
*/
void UniqueUntitledName(char *name)
{
    WindowInfo *w;
    int i;

   for (i=0; i<INT_MAX; i++) {
    	if (i == 0)
    	    sprintf(name, "Untitled");
    	else
    	    sprintf(name, "Untitled_%d", i);
	for (w=WindowList; w!=NULL; w=w->next)
     	    if (!strcmp(w->filename, name))
    	    	break;
    	if (w == NULL)
    	    break;
    }
}

/*
** Callback that guards us from trying to access a window after it has 
** been destroyed while a modal dialog is up.
*/
static void modifiedWindowDestroyedCB(Widget w, XtPointer clientData, 
    XtPointer callData)
{
    *(Bool*)clientData = TRUE;
}

/*
** Check if the file in the window was changed by an external source.
** and put up a warning dialog if it has.
*/
void CheckForChangesToFile(WindowInfo *window)
{
    static WindowInfo* lastCheckWindow = NULL;
    static Time lastCheckTime = 0;
    char fullname[MAXPATHLEN];
    struct stat statbuf;
    Time timestamp;
    FILE *fp;
    int resp, silent = 0;
    XWindowAttributes winAttr;
    Boolean windowIsDestroyed = False;
    
    if(!window->filenameSet)
        return;

    /* If last check was very recent, don't impact performance */
    timestamp = XtLastTimestampProcessed(XtDisplay(window->shell));
    if (window == lastCheckWindow &&
            timestamp - lastCheckTime < MOD_CHECK_INTERVAL)
        return;
    lastCheckWindow = window;
    lastCheckTime = timestamp;

    /* Update the status, but don't pop up a dialog if we're called
       from a place where the window might be iconic (e.g., from the
       replace dialog) or on another desktop.
       
       This works, but I bet it costs a round-trip to the server.
       Might be better to capture MapNotify/Unmap events instead. 

       For tabs that are not on top, we don't want the dialog either,
       and we don't even need to contact the server to find out. By
       performing this check first, we avoid a server round-trip for
       most files in practice. */
    if (!IsTopDocument(window))
        silent = 1;
    else {
        XGetWindowAttributes(XtDisplay(window->shell),
                             XtWindow(window->shell),
                             &winAttr);

        if (winAttr.map_state != IsViewable)
            silent = 1;
    }

    /* Get the file mode and modification time */
    strcpy(fullname, window->path);
    strcat(fullname, window->filename);
    if (stat(fullname, &statbuf) != 0) {
        /* Return if we've already warned the user or we can't warn him now */
        if (window->fileMissing || silent) {
            return;
        }

        /* Can't stat the file -- maybe it's been deleted.
           The filename is now invalid */
        window->fileMissing = TRUE;
        window->lastModTime = 1;
        window->device = 0;
        window->inode = 0;

        /* Warn the user, if they like to be warned (Maybe this should be its
            own preference setting: GetPrefWarnFileDeleted()) */
        if (GetPrefWarnFileMods()) {
            char* title;
            char* body;

            /* See note below about pop-up timing and XUngrabPointer */
            XUngrabPointer(XtDisplay(window->shell), timestamp);
            
            /* If the window (and the dialog) are destroyed while the dialog
               is up (typically closed via the window manager), we should
               avoid accessing the window afterwards. */
            XtAddCallback(window->shell, XmNdestroyCallback,
                          modifiedWindowDestroyedCB, &windowIsDestroyed);

            /*  Set title, message body and button to match stat()'s error.  */
            switch (errno) {
                case ENOENT:
                    /*  A component of the path file_name does not exist.  */
                    title = "File not Found";
                    body = "File '%s' (or directory in its path)\n"
                            "no longer exists.\n"
                            "Another program may have deleted or moved it.";
                    resp = DialogF(DF_ERR, window->shell, 2, title, body,
                            "Save", "Cancel", window->filename);
                    break;
                case EACCES:
                    /*  Search permission denied for a path component. We add
                        one to the response because Re-Save wouldn't really
                        make sense here.  */
                    title = "Permission Denied";
                    body = "You no longer have access to file '%s'.\n"
                            "Another program may have changed the permissions of\n"
                            "one of its parent directories.";
                    resp = 1 + DialogF(DF_ERR, window->shell, 1, title, body,
                            "Cancel", window->filename);
                    break;
                default:
                    /*  Everything else. This hints at an internal error (eg.
                        ENOTDIR) or at some bad state at the host.  */
                    title = "File not Accessible";
                    body = "Error while checking the status of file '%s':\n"
                            "    '%s'\n"
                            "Please make sure that no data is lost before closing\n"
                            "this window.";
                    resp = DialogF(DF_ERR, window->shell, 2, title, body,
                            "Save", "Cancel", window->filename,
                            errorString());
                    break;
            }
            
            if (!windowIsDestroyed) {
                XtRemoveCallback(window->shell, XmNdestroyCallback,
                                 modifiedWindowDestroyedCB, &windowIsDestroyed);
            }

            switch (resp) {
                case 1:
                    SaveWindow(window);
                    break;
                /*  Good idea, but this leads to frequent crashes, see
                    SF#1578869. Reinsert this if circumstances change by
                    uncommenting this part and inserting a "Close" button
                    before each Cancel button above.
                case 2:
                    CloseWindow(window);
                    return;
                */
            }
        }

        /* A missing or (re-)saved file can't be read-only. */
        /*  TODO: A document without a file can be locked though.  */
        /* Make sure that the window was not destroyed behind our back! */
        if (!windowIsDestroyed) {
            SET_PERM_LOCKED(window->lockReasons, False);
            UpdateWindowTitle(window);
            UpdateWindowReadOnly(window);
        }
        return;
    }
    
    /* Check that the file's read-only status is still correct (but
       only if the file can still be opened successfully in read mode) */
    if (window->fileMode != statbuf.st_mode ||
        window->fileUid != statbuf.st_uid ||
        window->fileGid != statbuf.st_gid) {
        window->fileMode = statbuf.st_mode;
        window->fileUid = statbuf.st_uid;
        window->fileGid = statbuf.st_gid;
        if ((fp = fopen(fullname, "r")) != NULL) {
            int readOnly;
            fclose(fp);
#ifndef DONT_USE_ACCESS 
            readOnly = access(fullname, W_OK) != 0;
#else
            if (((fp = fopen(fullname, "r+")) != NULL)) {
                readOnly = FALSE;
                fclose(fp);
            } else
                readOnly = TRUE;
#endif
            if (IS_PERM_LOCKED(window->lockReasons) != readOnly) {
                SET_PERM_LOCKED(window->lockReasons, readOnly);
                UpdateWindowTitle(window);
                UpdateWindowReadOnly(window);
            }
        }
    }

    /* Warn the user if the file has been modified, unless checking is
       turned off or the user has already been warned.  Popping up a dialog
       from a focus callback (which is how this routine is usually called)
       seems to catch Motif off guard, and if the timing is just right, the
       dialog can be left with a still active pointer grab from a Motif menu
       which is still in the process of popping down.  The workaround, below,
       of calling XUngrabPointer is inelegant but seems to fix the problem. */
    if (!silent && 
            ((window->lastModTime != 0 && 
              window->lastModTime != statbuf.st_mtime) ||
             window->fileMissing)  ){
        window->lastModTime = 0;        /* Inhibit further warnings */
        window->fileMissing = FALSE;
        if (!GetPrefWarnFileMods())
            return;
        if (GetPrefWarnRealFileMods() &&
	    !cmpWinAgainstFile(window, fullname)) {
	    /* Contents hasn't changed. Update the modification time. */
	    window->lastModTime = statbuf.st_mtime;
	    return;
	}
        XUngrabPointer(XtDisplay(window->shell), timestamp);
        if (window->fileChanged)
            resp = DialogF(DF_WARN, window->shell, 2,
                    "File modified externally",
                    "%s has been modified by another program.  Reload?\n\n"
                    "WARNING: Reloading will discard changes made in this\n"
                    "editing session!", "Reload", "Cancel", window->filename);
        else
            resp = DialogF(DF_WARN, window->shell, 2,
                    "File modified externally",
                    "%s has been modified by another\nprogram.  Reload?",
                    "Reload", "Cancel", window->filename);
        if (resp == 1)
            RevertToSaved(window, NULL);
    }
}

/*
** Return true if the file displayed in window has been modified externally
** to nedit.  This should return FALSE if the file has been deleted or is
** unavailable.
*/
static int fileWasModifiedExternally(WindowInfo *window)
{    
    char fullname[MAXPATHLEN];
    struct stat statbuf;
    
    if(!window->filenameSet)
        return FALSE;
    /* if (window->lastModTime == 0)
	return FALSE; */
    strcpy(fullname, window->path);
    strcat(fullname, window->filename);
    if (stat(fullname, &statbuf) != 0)
        return FALSE;
    if (window->lastModTime == statbuf.st_mtime)
	return FALSE;
    if (GetPrefWarnRealFileMods() &&
	!cmpWinAgainstFile(window, fullname)) {
	return FALSE;
    }
    return TRUE;
}

/*
** Check the read-only or locked status of the window and beep and return
** false if the window should not be written in.
*/
int CheckReadOnly(WindowInfo *window)
{
    if (IS_ANY_LOCKED(window->lockReasons)) {
    	XBell(TheDisplay, 0);
	return True;
    }
    return False;
}

/*
** Wrapper for strerror
*/
static const char *errorString(void)
{
    return strerror(errno);
}


/*
** Callback procedure for File Format toggle buttons.  Format is stored
** in userData field of widget button
*/
static void setFormatCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (XmToggleButtonGetState(w)) {
        XtPointer userData;
        XtVaGetValues(w, XmNuserData, &userData, NULL);
        *(int*) clientData = (int) (intptr_t) userData;
    }
}

/*
** Callback procedure for toggle button requesting newlines to be inserted
** to emulate continuous wrapping.
*/
static void addWrapCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int resp;
    int *addWrap = (int *)clientData;
    
    if (XmToggleButtonGetState(w))
    {
        resp = DialogF(DF_WARN, w, 2, "Add Wrap",
                "This operation adds permanent line breaks to\n"
                "match the automatic wrapping done by the\n"
                "Continuous Wrap mode Preferences Option.\n\n"
                "*** This Option is Irreversable ***\n\n"
                "Once newlines are inserted, continuous wrapping\n"
                "will no longer work automatically on these lines", "OK",
                "Cancel");
        if (resp == 2)
        {
            XmToggleButtonSetState(w, False, False);
            *addWrap = False;
        } else
        {
            *addWrap = True;
        }
    } else
    {
        *addWrap = False;
    }
}

/*
** Change a window created in NEdit's continuous wrap mode to the more
** conventional Unix format of embedded newlines.  Indicate to the user
** by turning off Continuous Wrap mode.
*/
static void addWrapNewlines(WindowInfo *window)
{
    int fileLen, i, insertPositions[MAX_PANES], topLines[MAX_PANES];
    int horizOffset;
    Widget text;
    char *fileString;
	
    /* save the insert and scroll positions of each pane */
    for (i=0; i<=window->nPanes; i++) {
    	text = i==0 ? window->textArea : window->textPanes[i-1];
    	insertPositions[i] = TextGetCursorPos(text);
    	TextGetScroll(text, &topLines[i], &horizOffset);
    }

    /* Modify the buffer to add wrapping */
    fileString = TextGetWrapped(window->textArea, 0,
    	    window->buffer->length, &fileLen);
    BufSetAll(window->buffer, fileString);
    NEditFree(fileString);

    /* restore the insert and scroll positions of each pane */
    for (i=0; i<=window->nPanes; i++) {
    	text = i==0 ? window->textArea : window->textPanes[i-1];
	TextSetCursorPos(text, insertPositions[i]);
	TextSetScroll(text, topLines[i], 0);
    }

    /* Show the user that something has happened by turning off
       Continuous Wrap mode */
    SetToggleButtonState(window, window->continuousWrapItem, False, True);
}

/* 
 * Number of bytes read at once by cmpWinAgainstFile
 */
#define PREFERRED_CMPBUF_LEN 32768

/* 
 * Check if the contens of the textBuffer *buf is equal 
 * the contens of the file named fileName. The format of
 * the file (UNIX/DOS/MAC) is handled properly.
 *
 * Return values
 *   0: no difference found
 *  !0: difference found or could not compare contents.
 */
static int cmpWinAgainstFile(WindowInfo *window, const char *fileName)
{
    char    fileString[PREFERRED_CMPBUF_LEN + 2];
    struct  stat statbuf;
    int     fileLen, restLen, nRead, bufPos, rv, offset, filePos;
    char    pendingCR = 0;
    int	    fileFormat = window->fileFormat;
    char    message[MAXPATHLEN+50];
    textBuffer *buf = window->buffer;
    FILE   *fp;

    fp = fopen(fileName, "r");
    if (!fp)
        return (1);
    if (fstat(fileno(fp), &statbuf) != 0) {
        fclose(fp);
        return (1);
    }

    fileLen = statbuf.st_size;
    /* For DOS files, we can't simply check the length */
    if (fileFormat != DOS_FILE_FORMAT) {
	if (fileLen != buf->length) {
	    fclose(fp);
	    return (1);
        }
    } else {
	/* If a DOS file is smaller on disk, it's certainly different */
	if (fileLen < buf->length) {
	    fclose(fp);
	    return (1);
	}
    }
    
    /* For large files, the comparison can take a while. If it takes too long,
       the user should be given a clue about what is happening. */
    sprintf(message, "Comparing externally modified %s ...", window->filename);
    restLen = min(PREFERRED_CMPBUF_LEN, fileLen);
    bufPos = 0;
    filePos = 0;
    while (restLen > 0) {
        AllWindowsBusy(message);
        if (pendingCR) {
           fileString[0] = pendingCR;
           offset = 1;
        } else {
           offset = 0;
        }

        nRead = fread(fileString+offset, sizeof(char), restLen, fp);
        if (nRead != restLen) {
            fclose(fp);
            AllWindowsUnbusy();
            return (1);
        }
        filePos += nRead;
        
        nRead += offset;

        /* check for on-disk file format changes, but only for the first hunk */
        if (bufPos == 0 && fileFormat != FormatOfFile(fileString)) {
            fclose(fp);
            AllWindowsUnbusy();
            return (1);
        }

        if (fileFormat == MAC_FILE_FORMAT)
            ConvertFromMacFileString(fileString, nRead);
        else if (fileFormat == DOS_FILE_FORMAT)
            ConvertFromDosFileString(fileString, &nRead, &pendingCR);

        /* Beware of 0 chars ! */
        BufSubstituteNullChars(fileString, nRead, buf);
        rv = BufCmp(buf, bufPos, nRead, fileString);
        if (rv) {
            fclose(fp);
            AllWindowsUnbusy();
            return (rv);
        }
        bufPos += nRead;
        restLen = min(fileLen - filePos, PREFERRED_CMPBUF_LEN);
    }
    AllWindowsUnbusy();
    fclose(fp);
    if (pendingCR) {
	rv = BufCmp(buf, bufPos, 1, &pendingCR);
	if (rv) {
	    return (rv);
	}
	bufPos += 1;
    }
    if (bufPos != buf->length) { 
	return (1);
    }
    return (0);
}

/*
** Force ShowLineNumbers() to re-evaluate line counts for the window if line
** counts are required.
*/
static void forceShowLineNumbers(WindowInfo *window)
{
    Boolean showLineNum = window->showLineNumbers;
    if (showLineNum) {
        window->showLineNumbers = False;
        ShowLineNumbers(window, showLineNum);
    }
}

static int min(int i1, int i2)
{
    return i1 <= i2 ? i1 : i2;
}

static char * GetDefaultEncoding(void) {
    char *lc = setlocale (LC_ALL, "");
    char *d = strchr(lc, '.');
    if(d) {
        *d = 0;
    }
    
    int i = 0;
    while(locales[i].locale) {
        if(!strcmp(locales[i].locale, lc)) {
            return locales[i].encoding;
        }
        i++;
    }
    
    return "ISO8859-1";
}

const char * DetectEncoding(const char *buf, size_t len, const char *def) {
    int utf8Err = 0; // number of utf8 encoding errors
    int utf8Mb = 0;  // number of multibyte characters 
    
    const unsigned char *u = (const unsigned char*)buf;
    int charLen = 0; // length of char - 1
    for(size_t i=0;i<len;i++) {
        unsigned char c = u[i];
        if(charLen == 0) {
            if(c >= 240) {
                charLen = 3;
            } else if(c >= 224) {
                charLen = 2;
            } else if(c > 192) {
                charLen = 1;
            }
        } else {
            if((c & 192) == 128) {
                if(--charLen == 0) {
                    utf8Mb++;
                }
            } else {
                utf8Err++;
                charLen = 0;
            }
        }
    }  
    
    if(utf8Err == 0 || utf8Mb - utf8Err > 2) {
        return "UTF-8";
    }
    
    char defbuf[4];
    memset(defbuf, 0, 4);
    if(def && strlen(def) > 3) {
        memcpy(defbuf, def, 3);
    }
    if(!strcasecmp(defbuf, "utf")) {
        return GetDefaultEncoding();
    }
    
    return def;
}
