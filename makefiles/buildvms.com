$! $Id: buildvms.com,v 1.2 2006/09/27 11:51:44 michaelsmith Exp $
$!
$! VMS procedure to compile and link modules for NEdit
$!
$!
$! In case of problems with the install you might contact me at
$! zinser@zinser.no-ip.info(preferred) or
$! zinser@sysdev.deutsche-boerse.com (work)
$!
$! Make procedure history for NEdit
$!
$!------------------------------------------------------------------------------
$! Version history
$! 0.01 20040229 First version to receive a number
$! 0.02 20041109 Init s_case for case sensitive shareable images
$! 0.03 20041229 Some more config info (tconfig, conf_check_string)
$! 0.04 20050105 Add check for MMS/MMK, does not hurt even for pure DCL build
$! 0.05 20060105 Merge in improvements by Michael Smith from NEdit CVS
$!
$ ON ERROR THEN GOTO err_exit
$ true  = 1
$ false = 0
$ tmpnam = "temp_" + f$getjpi("","pid")
$ tt = tmpnam + ".txt"
$ tc = tmpnam + ".c"
$ th = tmpnam + ".h"
$ define tconfig 'th'
$ its_decc = false
$ its_vaxc = false
$ its_gnuc = false
$ s_case   = False
$ ver_no   = ""
$!
$! Setup variables holding "config" information
$!
$ Make    = ""
$ ccopt   = ""
$ lopts   = ""
$ dnsrl   = ""
$ aconf_in_file = "config.hin"
$ conf_check_string = ""
$ name    = "NEdit"
$!
$ gosub find_ver
$!
$ whoami = f$parse(f$enviornment("Procedure"),,,,"NO_CONCEAL")
$ mydef  = F$parse(whoami,,,"DEVICE")
$ mydir  = f$parse(whoami,,,"DIRECTORY") - "]["
$ myproc = f$parse(whoami,,,"Name") + f$parse(whoami,,,"type")
$ xmldir = mydef + mydir - ".MAKEFILES]" + ".MICROLINE.XML]"
$ startdir = mydef + mydir - ".MAKEFILES]" +"]"
$ target   = ""
$!
$! Check for MMK/MMS
$!
$ If F$Search ("Sys$System:MMS.EXE") .nes. "" Then Make = "MMS"
$ If F$Type (MMK) .eqs. "STRING" Then Make = "MMK"
$!
$ gosub check_opts
$!
$! Default is to build NEdit. 
$! If requested do cleanup, create dist, then exit
$!
$ if (target .nes. "")
$ then
$   gosub 'target'
$   exit
$ endif
$ gosub check_compiler
$!
$ if (its_decc) then ccopt = ccopt + "/prefix=all"
$ if f$length(make) .gt. 0
$ then
$   gosub crea_mms
$   set def [.util]
$   'make'
$   set def [-.microline.xml]
$   'make'
$   set def [--.xlt]
$   'make'
$   set def [-.source]
$   'make'
$   set def [-]
$ else
$   DEFINE SYS DECC$LIBRARY_INCLUDE
$   DEFINE XM DECW$INCLUDE
$   DEFINE X11 DECW$INCLUDE
$   DEFINE XML 'xmldir'
$   set def [.util]
$   if f$search("LIBNUTIL.OLB") .eqs. "" then library/create/object libNUtil.olb
$   if f$search("VMSUTILS.OLB") .eqs. "" then library/create/object vmsutils.olb
$   cflags = ccopt
$   call COMPILE CLEARCASE   libNUtil						  
$   call COMPILE DIALOGF     libNUtil						  
$   call COMPILE FILEUTILS   libNUtil 					  
$   call COMPILE GETFILES    libNUtil						  
$   call COMPILE MISC	     libNUtil					  
$   call COMPILE PREFFILE    libNUtil						  
$   call COMPILE PRINTUTILS  libNUtil					  
$   call COMPILE FONTSEL     libNUtil						  
$   call COMPILE MANAGEDLIST libNUtil					  
$   call COMPILE MOTIF       libNUtil					  
$   call COMPILE UTILS	     libNUtil					  
$   call COMPILE VMSUTILS    vmsutils						  
$   set def [-.microline.xml]
$   if f$search("libXmL.OLB") .eqs. "" then library/create/object libXmL.olb
$   call COMPILE Folder   libXmL
$   call COMPILE Grid     libXmL
$   call COMPILE GridUtil libXmL
$   call COMPILE Progress libXmL
$   call COMPILE Tree     libXmL
$   call COMPILE XmL      libXmL
$   set def [--.xlt]
$   cflags = ccopt + "/include=[]"
$   if f$search("libXlt.OLB") .eqs. "" then library/create/object libXlt.olb
$   call COMPILE BubbleButton libXlt
$   call COMPILE SlideC       libXlt
$   set def [-.source]
$   cflags = ccopt + "/DEFINE=(USE_ACCESS)/include=[-.xlt]"
$   call COMPILE SELECTION
$   call COMPILE FILE
$   call COMPILE HELP
$   call COMPILE MENU
$   call COMPILE NEDIT
$   call COMPILE PREFERENCES
$   call COMPILE REGULAREXP
$   call COMPILE SEARCH
$   call COMPILE SHIFT
$   call COMPILE TAGS
$   call COMPILE UNDO
$   call COMPILE WINDOW
$   call COMPILE USERCMDS
$   call COMPILE MACRO
$   call COMPILE TEXT
$   call COMPILE TEXTSEL
$   call COMPILE TEXTDISP
$   call COMPILE TEXTBUF
$   call COMPILE TEXTDRAG
$   call COMPILE SERVER
$   call COMPILE HIGHLIGHT
$   call COMPILE HIGHLIGHTDATA
$   call COMPILE INTERPRET
$   call COMPILE SMARTINDENT
$   call COMPILE REGEXCONVERT
$   call COMPILE RBTREE
$   call COMPILE WINDOWTITLE
$   call COMPILE LINKDATE
$   call COMPILE CALLTIPS
$   call COMPILE RANGESET
$   call COMPILE SERVER_COMMON
$   !
$   if f$search("PARSE.C") .nes. "" then DELETE PARSE.C;*
$   COPY PARSE_NOYACC.C PARSE.C
$   call COMPILE PARSE
$   !
$   call COMPILE NC
$   OBJS :=	  nedit, file, menu, window, selection, search, undo, shift, -
    	  help, preferences, tags, userCmds, regularExp, macro, text, -
    	  textSel, textDisp, textBuf, textDrag, server, highlight, -
    	  highlightData, interpret, parse, smartIndent, regexconvert, -
    	  rbTree, windowtitle, linkdate, calltips, rangeset, server_common

$   LINK 'lopts' 'OBJS', NEDIT_OPTIONS_FILE/OPT, -
                          [-.microline.xml]libxml/lib, [-.xlt]libXlt/lib, -
    			  [-.util]vmsUtils/lib, libNUtil/lib
$   LINK 'lopts' nc, server_common.obj, NEDIT_OPTIONS_FILE/OPT, -
    			  [-.util]vmsUtils/lib, libNUtil/lib
$   set def 'startdir'
$ endif
$ if f$type(dnrsl) .eqs. "STRING" then -
    define decc$no_rooted_search_lists 'dnrsl'
$ exit
$CC_ERR:
$ write sys$output "C compiler required to build ''name'"
$ goto err_exit
$ERR_EXIT:
$ if f$type(dnrsl) .eqs. "STRING" then -
    define decc$no_rooted_search_lists 'dnrsl'
$ close/nolog ver_h
$ write sys$output "Error building ''name'. Exiting..."
$ exit 2
$!------------------------------------------------------------------------------
$!
$! If MMS/MMK are available dump out the descrip.mms if required
$!
$CREA_MMS:
$ write sys$output "Creating [.util]descrip.mms..."
$ create [.util]descrip.mms
$ open/append out [.util]descrip.mms
$ write out "CFLAGS=", ccopt
$ copy sys$input: out
$ deck
CC=cc
AR=lib
.FIRST
        DEFINE SYS DECC$LIBRARY_INCLUDE
        DEFINE XM DECW$INCLUDE
        DEFINE X11 DECW$INCLUDE

OBJS = clearcase.obj, DialogF.obj, getfiles.obj, printUtils.obj, misc.obj,\
       fileUtils.obj, prefFile.obj, fontsel.obj, managedlist.obj, utils.obj,\
       motif.obj

all : libNUtil.olb VMSUTILS.olb
        sh def

libNUtil.olb : $(OBJS)
        $(AR) /CREATE/OBJ libNUtil.olb $(OBJS)

VMSUTILS.olb : VMSUTILS.obj
        $(AR) /CREATE/OBJ VMSUTILS VMSUTILS
$ eod
$ close out
$ write sys$output "Creating [.microline.xml]descrip.mms..."
$ create [.microline.xml]descrip.mms
$ open/append out [.microline.xml]descrip.mms
$ write out "CFLAGS=", ccopt
$ copy sys$input: out
$ deck
CC=cc
AR=lib
.FIRST
        DEFINE SYS DECC$LIBRARY_INCLUDE
        DEFINE XM DECW$INCLUDE
        DEFINE X11 DECW$INCLUDE
$ eod
$ write out "        DEFINE XML ", xmldir
$ copy sys$input: out
$ deck

OBJS =  Folder.obj, Grid.obj, GridUtil.obj, Progress.obj, Tree.obj, XmL.obj

all : libXmL.olb
        sh def

libXmL.olb : $(OBJS)
        $(AR) /CREATE/OBJ libXmL.olb $(OBJS)
$ eod
$ close out
$ write sys$output "Creating [.xlt]descrip.mms..."
$ create [.xlt]descrip.mms
$ open/append out [.xlt]descrip.mms
$ write out "CFLAGS=", ccopt, "/include=[]" 
$ copy sys$input: out
$ deck
CC=cc
AR=lib

.FIRST
        DEFINE SYS DECC$LIBRARY_INCLUDE
        DEFINE XM DECW$INCLUDE
        DEFINE X11 DECW$INCLUDE

OBJS = BubbleButton.obj, SlideC.obj

all : libXlt.olb
        sh def

libXlt.olb : $(OBJS)
        $(AR) /CREATE/OBJ libXlt.olb $(OBJS)
$ eod
$ close out
$ write sys$output "Creating [.source]descrip.mms..."
$ create [.source]descrip.mms
$ open/append out [.source]descrip.mms
$ write out "CFLAGS=", ccopt, "/define=(USE_ACCESS)/include=[-.xlt]"
$ write out "LFLAGS=", lopts
$ copy sys$input: out
$ deck
#
# Makefile for VMS/MMS
#

CC=cc

.FIRST
        DEFINE SYS DECC$LIBRARY_INCLUDE
        DEFINE XM DECW$INCLUDE
        DEFINE X11 DECW$INCLUDE
$ eod
$ write out "        DEFINE XML ", xmldir
$ copy sys$input: out
$ deck
        copy parse_noyacc.c parse.c

SRCS =  nedit.c selection.c file.c help.c menu.c preferences.c regularExp.c\
        search.c shift.c tags.c undo.c window.c userCmds.c macro.c text.c\
        textSel.c textDisp.c textBuf.c textDrag.c server.c highlight.c\
        highlightData.c interpret.c smartIndent.c parse.c nc.c regexconvert.c\
        rbtree.c linkdate.c windowTitle.c calltips.c\
        rangeset.c, server_common.c

OBJS =  selection.obj, file.obj, help.obj, menu.obj, preferences.obj, \
        regularExp.obj, search.obj, shift.obj, tags.obj, undo.obj, window.obj,\
        userCmds.obj, macro.obj, text.obj, textSel.obj, textDisp.obj,\
        textBuf.obj, textDrag.obj, server.obj, highlight.obj,\
        highlightData.obj, interpret.obj, smartIndent.obj, parse.obj,\
        regexconvert.obj, rbtree.obj, linkdate.obj, windowTitle.obj, \
        calltips.obj, rangeset.obj, server_common.obj

NEOBJS = nedit.obj

NCOBJS = nc.obj

all : nedit.exe nc.exe
      @ write sys$output "Nedit build completed"

nedit.exe : $(NEOBJS) $(OBJS)
        link/exe=nedit.exe  $(NEOBJS),$(OBJS), NEDIT_OPTIONS_FILE/OPT, -
        [-.microline.xml]libxml/lib, [-.xlt]libXlt/lib, -
        [-.util]vmsUtils/lib, libNUtil.olb/lib

nc.exe : $(NCOBJS)
        LINK $(NCOBJS), NEDIT_OPTIONS_FILE/OPT, -
        [-.util]vmsUtils/lib,libNUtil.olb/lib
$ eod
$ close out
$ return
$!------------------------------------------------------------------------------
$!
$! Check command line options and set symbols accordingly
$!
$!------------------------------------------------------------------------------
$! Version history
$! 0.01 20041206 First version to receive a number
$ CHECK_OPTS:
$ i = 1
$ OPT_LOOP:
$ if i .lt. 9
$ then
$   cparm = f$edit(p'i',"upcase")
$!
$! Check if parameter actually contains something
$!
$   if f$edit(cparm,"trim") .nes. ""
$   then
$     if cparm .eqs. "DEBUG"
$     then
$       ccopt = ccopt + "/noopt/deb"
$       lopts = lopts + "/deb"
$     endif
$     if f$locate("CCOPT=",cparm) .lt. f$length(cparm)
$     then
$       start = f$locate("=",cparm) + 1
$       len   = f$length(cparm) - start
$       ccopt = ccopt + f$extract(start,len,cparm)
$       if f$locate("AS_IS",f$edit(ccopt,"UPCASE")) .lt. f$length(ccopt) -
          then s_case = true
$     endif
$     if cparm .eqs. "LINK" then linkonly = true
$     if f$locate("LOPTS=",cparm) .lt. f$length(cparm)
$     then
$       start = f$locate("=",cparm) + 1
$       len   = f$length(cparm) - start
$       lopts = lopts + f$extract(start,len,cparm)
$     endif
$     if f$locate("CC=",cparm) .lt. f$length(cparm)
$     then
$       start  = f$locate("=",cparm) + 1
$       len    = f$length(cparm) - start
$       cc_com = f$extract(start,len,cparm)
        if (cc_com .nes. "DECC") .and. -
           (cc_com .nes. "VAXC") .and. -
           (cc_com .nes. "GNUC")
$       then
$         write sys$output "Unsupported compiler choice ''cc_com' ignored"
$         write sys$output "Use DECC, VAXC, or GNUC instead"
$       else
$         if cc_com .eqs. "DECC" then its_decc = true
$         if cc_com .eqs. "VAXC" then its_vaxc = true
$         if cc_com .eqs. "GNUC" then its_gnuc = true
$       endif
$     endif
$     if f$locate("MAKE=",cparm) .lt. f$length(cparm)
$     then
$       start  = f$locate("=",cparm) + 1
$       len    = f$length(cparm) - start
$       mmks = f$extract(start,len,cparm)
$       if (mmks .eqs. "MMK") .or. (mmks .eqs. "MMS")
$       then
$         make = mmks
$       else
$         write sys$output "Unsupported make choice ''mmks' ignored"
$         write sys$output "Use MMK or MMS instead"
$       endif
$     endif
$     if cparm .eqs. "CLEAN" then target = "CLEAN"
$     if cparm .eqs. "DISTBIN" then target = "DISTBIN"
$   endif
$   i = i + 1
$   goto opt_loop
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Look for the compiler used
$!
$! Version history
$! 0.01 20040223 First version to receive a number
$! 0.02 20040229 Save/set value of decc$no_rooted_search_lists
$CHECK_COMPILER:
$ if (.not. (its_decc .or. its_vaxc .or. its_gnuc))
$ then
$   its_decc = (f$search("SYS$SYSTEM:DECC$COMPILER.EXE") .nes. "")
$   its_vaxc = .not. its_decc .and. (F$Search("SYS$System:VAXC.Exe") .nes. "")
$   its_gnuc = .not. (its_decc .or. its_vaxc) .and. (f$trnlnm("gnu_cc") .nes. "")
$ endif
$!
$! Exit if no compiler available
$!
$ if (.not. (its_decc .or. its_vaxc .or. its_gnuc))
$ then goto CC_ERR
$ else
$   if its_decc
$   then
$     write sys$output "CC compiler check ... Compaq C"
$     if f$trnlnm("decc$no_rooted_search_lists") .nes. ""
$     then
$       dnrsl = f$trnlnm("decc$no_rooted_search_lists")
$     endif
$     define decc$no_rooted_search_lists 1
$   else
$     if its_vaxc then write sys$output "CC compiler check ... VAX C"
$     if its_gnuc then write sys$output "CC compiler check ... GNU C"
$     if f$trnlnm(topt) then write topt "sys$share:vaxcrtl.exe/share"
$     if f$trnlnm(optf) then write optf "sys$share:vaxcrtl.exe/share"
$   endif
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Cleanup files created during compilation
$!
$CLEAN:
$ delete/noconfirm/log [.util]*.obj;*,[.util]*.olb;*,[.util]*.mms;*
$ delete/noconfirm/log [.Xlt]*.obj;*,[.Xlt]*.olb;*,[.Xlt]*.mms;*
$ delete/noconfirm/log [.Microline.XmL]*.obj;*,[.Microline.XmL]*.olb;*,-
                       [.Microline.XmL]*.mms;*
$ delete/noconfirm/log [.source]*.obj;*,[.source]*.exe;*,[.source]*.mms;*
$ return
$!------------------------------------------------------------------------------
$!
$! Create backup saveset with binaries and other files needed to run NEdit
$!
$DISTBIN:
$ on error then continue
$ arch = f$getsyi("arch_name")
$ name_ver = -
        f$fao("!AS!AS_!AS",name,f$element(0,".",ver_no),f$element(0,".",ver_no))
$ purge/log [...]
$ delete 'name_ver'-vms-'arch'.bck;*
$ create/directory [.'name_ver']
$ set protection=w:rwed [-.'name_ver'...]*.*;*
$ delete [.'name_ver'...]*.*;*
$ delete [.'name_ver'...]*.*;*
$ delete [.'name_ver'...]*.*;*
$ delete [.'name_ver'...]*.*;*
$ delete [.'name_ver'...]*.*;*
$ delete [.'name_ver']*.*;*
$ create/directory [.'name_ver']
$ copy [.source]nedit.exe [.'name_ver']
$ copy [.source]nc.exe    [.'name_ver']
$ copy README             [.'name_ver']
$ copy COPYRIGHT          [.'name_ver']
$ copy ReleaseNotes       [.'name_ver']
$ copy [.doc]NEDIT.DOC    [.'name_ver']
$ copy [.doc]NEDIT.HTML   [.'name_ver']
$ copy [.doc]NEDIT.MAN    [.'name_ver']
$ copy [.doc]NC.MAN       [.'name_ver']
$ copy [.doc]FAQ.TXT      [.'name_ver']
$ backup/log [.'name_ver'...] 'name_ver'-vms-'arch'.bck/save_set
$ return
$!------------------------------------------------------------------------------
$!
$! Check and find version of NEdit
$!
$FIND_VER:
$ open/read ver_h [.source]help_data.h
$FVLOOP:
$ read/end=end_fvloop ver_h line
$ if f$locate("NEditVersion",line) .nes. f$length(line)
$ then
$  ver_no = f$element(0,"\",f$element(1," ",f$element(1,"""",line)))
$  goto end_fvloop
$ endif
$ goto fvloop
$END_FVLOOP:
$ close ver_h
$ return
$!------------------------------------------------------------------------------
$!
$! Subroutine to compile a source file
$!
$compile: subroutine
$ crdt = F$CVTIME(F$FILE_ATTRIBUTES("''p1'.c","RDT"))
$ ordt = ""
$ if f$search("''p1'.obj") .nes. "" then - 
     ordt = F$CVTIME(F$FILE_ATTRIBUTES("''p1'.obj","RDT"))
$ if "''crdt'" .gts. "''ordt'"
$ then
$   write sys$output "Compiling ", p1
$   cc 'cflags' /object='p1' 'p1'.c
$   if p2 .nes."" then  library/replace/object 'p2'.olb 'p1'.obj
$ endif
$ exit
$ endsubroutine
$!------------------------------------------------------------------------------
