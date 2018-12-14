/*******************************************************************************
*                                                                              *
* system.h -- Compile Time Configuration Header File                           *
*                                                                              *
* Copyright (C) 2001 Scott Tringali                                            *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
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
* July 23, 2001                                                                *
*                                                                              *
* Written by Scott Tringali, http://www.tringali.org                           *
*                                                                              *
*******************************************************************************/


#ifndef NEDIT_SYSTEM_H_INCLUDED
#define NEDIT_SYSTEM_H_INCLUDED

/*
   Determine which machine we were compiled with.  This isn't as accurate
   as calling uname(), which is preferred.  However, this gets us very close
   for a majority of the machines out there, and doesn't require any games
   with make.

   A better, but trickier solution, is to run uname at compile time, capture
   the string, and place it in the executable.
      
   Please update this with the proper symbols for your compiler/CPU.  It
   may take a little sleuthing to find out what the correct symbol is.
   Better compilers/OSs document the symbols they define, but not all do.
   Usually, the correct ones are prepended with an _ or __, as this is
   namespace is reserved by ANSI C for the compiler implementation.
   
   The order is important for the x86 macros.  Some compilers will
   simultanenously define __i386 and __pentium, so we pick the highest one.

   Some of the info below derived from these excellent references: 

   http://www.fortran-2000.com/ArnaudRecipes/Version.html
   http://predef.sourceforge.net/
*/

#if defined(__alpha) || defined (_M_ALPHA)
#   define COMPILE_MACHINE "Alpha"

#elif defined(__mips)
#   define COMPILE_MACHINE "MIPS"

#elif defined(__sparc)
#   define COMPILE_MACHINE "Sparc"

#elif defined(__sparcv9)
#   define COMPILE_MACHINE "Sparc64"

#elif defined(__hppa)
#   define COMPILE_MACHINE "PA-RISC"

#elif defined(__ALTIVEC__)
#   define COMPILE_MACHINE "PowerPC Altivec"

#elif defined(__POWERPC__) || defined(__ppc__) || defined(__powerpc__) || defined(_POWER)
#   define COMPILE_MACHINE "PowerPC"

#elif defined(__x86_64) || defined(_x86_64)
#   define COMPILE_MACHINE "x86-64"

#elif defined(__IA64) || defined(__ia64)
#   define COMPILE_MACHINE "IA64"

#elif defined(__k6) || defined(__k6__)
#   define COMPILE_MACHINE "K6"

#elif defined(__athlon) || defined(__athlon__)
#   define COMPILE_MACHINE "Athlon"

#elif defined(__pentium4) || defined(__pentium4__)
#   define COMPILE_MACHINE "Pentium IV"

#elif defined(__pentium3) || defined(__pentium3__)
#   define COMPILE_MACHINE "Pentium III"

#elif defined(__pentium2) || defined(__pentium2__)
#   define COMPILE_MACHINE "Pentium II"

#elif defined(__pentiumpro) || defined(__pentiumpro__)
#   define COMPILE_MACHINE "Pentium Pro"

#elif defined(__pentium) || defined(__pentium__)
#   define COMPILE_MACHINE "Pentium"

#elif defined(__i486) || defined(__i486__)
#   define COMPILE_MACHINE "486"

#elif defined(__i386) || defined(__i386__)
#   define COMPILE_MACHINE "386"

#elif defined(_M_IX86) || defined(_X86_) || defined (__x86__)
#   define COMPILE_MACHINE "x86"

#elif defined(__VAX)
#   define COMPILE_MACHINE "VAX"        /* Untested, please verify */

#else	
#   define COMPILE_MACHINE "Unknown"
#endif



#if defined(__osf__)
#   define COMPILE_OS "Tru64/Digital Unix"

#elif defined(__sun)
#   define COMPILE_OS "Solaris"

#elif defined(__hpux)
#   define COMPILE_OS "HP/UX"

#elif defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#   define COMPILE_OS "Win32"

#elif defined(__sgi)
#   define COMPILE_OS "IRIX"

#elif defined(__Lynx__)
#   define COMPILE_OS "Lynx"

#elif defined(__linux__)
#   define COMPILE_OS "Linux"

#elif defined(_AIX)
#   define COMPILE_OS "AIX"

#elif defined(__VMS)                    /* Untested, please verify */
#   define COMPILE_OS "VMS"

#elif defined(__FreeBSD__)
#   define COMPILE_OS "FreeBSD"

#elif defined(__OpenBSD__)              /* Untested, please verify */
#   define COMPILE_OS "OpenBSD"

#elif defined(__NetBSD__)               /* Untested, please verify */
#   define COMPILE_OS "NetBSD"

#elif defined(__bsdi)                   /* Untested, please verify */
#   define COMPILE_OS "BSDI"

#elif defined(__ultrix)                 /* Untested, please verify */
#   define COMPILE_OS "Ultrix"

#elif defined(__EMX__)                  /* I think this should be __OS2__ */
#   define COMPILE_OS "OS/2"

#elif defined(__APPLE__) || defined(__MACOSX__)
#   define COMPILE_OS "MacOS X"

#elif defined(__UNIXWARE__)
#   define COMPILE_OS "UnixWare"

#elif defined(__unix__)                 /* Unknown Unix, next to last */
#   define COMPILE_OS "Unix"

#else
#   define COMPILE_OS "Unknown"
#endif



#if defined (__DECC)
#   define COMPILE_COMPILER "DEC C"

#elif defined (__DECCXX)
#   define COMPILE_COMPILER "DEC C++"

#elif defined (__APOGEE)
#   define COMPILE_COMPILER "Apogee"

#elif defined (__SUNPRO_C)
#   define COMPILE_COMPILER "Sun Studio C"    /* aka Sun WorkShop, Sun ONE studio, Forte, Sun Studio, ARGH! */

#elif defined (__SUNPRO_CC)
#   define COMPILE_COMPILER "Sun Studio C++"  /* aka Sun WorkShop, Sun ONE studio, Forte, Sun Studio, ARGH! */

#elif defined (__LCC__)
#   define COMPILE_COMPILER "LCC"

#elif defined (_MSC_VER)
#   define COMPILE_COMPILER "Microsoft C"

#elif defined (__BORLANDC__)
#   define COMPILE_COMPILER "Borland C"

#elif defined (__sgi) && defined (_COMPILER_VERSION)
#   define COMPILE_COMPILER "SGI MipsPro"

#elif defined (__xlC__)                       /* Unix version of IBM C */
#   define COMPILE_COMPILER "IBM xlC"

#elif defined (__IBMC__)
#   define COMPILE_COMPILER "IBM C"           /* PC (OS/2, Windows) versions */

#elif defined (__HP_cc)
#   define COMPILE_COMPILER "HP cc"

#elif defined (__HP_aCC)
#   define COMPILE_COMPILER "HP aCC"

#elif defined (__KCC)
#   define COMPILE_COMPILER "KAI C++"

#elif defined (__MWERKS__)
#   define COMPILE_COMPILER "Metrowerks CodeWarrior"

#elif defined (__WATCOMC__)
#   define COMPILE_COMPILER "Watcom C/C++"

#elif defined (__INTEL_COMPILER)
#   define COMPILE_COMPILER "Intel C++"

#elif defined (__clang__)
#   define COMPILE_COMPILER "Clang"

#elif defined (__llvm__)
#   define COMPILE_COMPILER "LLVM"

/* GCC needs to be near the bottom of this list, because Intel and Clang have GCC 
   command-line emulation modes. */
   
#elif defined(__GNUC__)
#   define COMPILE_COMPILER "GNU C"

/* The next few entries are last-ditch efforts to guess the compiler, if
   no compiler macro exists.  These need to be at the end of the list,
   after all the compilers we really recognize. */

#elif defined (__hpux)                        /* HP has no indentifier, so guessing here */
#   define COMPILE_COMPILER "HP C [?]"

#elif defined (__sgi)                         /* Same for old versions of SGI cc */
#   define COMPILE_COMPILER "SGI MipsPro [?]"

#else
#   define COMPILE_COMPILER "Unknown"         /* Must be last */
#endif

#endif /* NEDIT_SYSTEM_H_INCLUDED */
