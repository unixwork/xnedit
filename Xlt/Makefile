# $Id: Makefile,v 1.1 2005/03/23 12:34:28 edg Exp $
SHELL=/bin/sh
#
# Makefile for NEdit text editor
#
# Targets are the suffixes of the system-specific makefiles in
# the makefiles/ directory.
# For example, to build NEdit for Solaris, give the command
#
#   make solaris
#
# This builds an intermediate library in the util/ directory,
# then builds the nedit and nc executables in the source/ directory.
#

all:
	@echo "Please specify target:"
	@echo "(For example, type \"make linux\" for a Linux system.)"
	@(cd ../makefiles && ls -C Makefile* | sed -e 's/Makefile.//g')

.DEFAULT:
	@- if [ -f ../makefiles/Makefile.$@ -a ! -f ./Makefile.$@ ];\
           then ln -s ../makefiles/Makefile.$@ .; fi
	@- $(MAKE) -f Makefile.$@ libXlt.a

clean:
	@- $(MAKE) -f Makefile.common clean

