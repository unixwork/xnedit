SHELL=/bin/sh
PREFIX=/usr
#
# Makefile for XNEdit text editor
#
# Targets are the suffixes of the system-specific makefiles in
# the makefiles/ directory.
# For example, to build XNEdit for Solaris, give the command
#
#   make solaris
#
# This builds an intermediate library in the util/ directory,
# then builds the xnedit and xnc executables in the source/ directory.
#

all:
	@echo "Please specify target:"
	@echo "(For example, type \"make linux\" for a Linux system.)"
	@(cd makefiles && ls -C Makefile* | sed -e 's/Makefile.//g')

.DEFAULT:
	@- (cd Microline/XmL;   if [ -f ../../makefiles/Makefile.$@ -a ! -f ./Makefile.$@ ];\
	   then ln -s ../../makefiles/Makefile.$@ .; fi)
	@- (cd Xlt;   if [ -f ../makefiles/Makefile.$@ -a ! -f ./Makefile.$@ ];\
	   then ln -s ../makefiles/Makefile.$@ .; fi)
	@- (cd util;   if [ -f ../makefiles/Makefile.$@ -a ! -f ./Makefile.$@ ];\
	   then ln -s ../makefiles/Makefile.$@ .; fi)
	@- (cd source; if [ -f ../makefiles/Makefile.$@ -a ! -f ./Makefile.$@ ];\
	   then ln -s ../makefiles/Makefile.$@ .; fi)

	(cd util; \
	    $(MAKE) -f Makefile.$@ libNUtil.a)
	(cd Xlt;    $(MAKE) -f Makefile.$@ libXlt.a)
	(cd Microline/XmL;    $(MAKE) -f Makefile.$@ libXmL.a)
	(cd source; $(MAKE) -f Makefile.$@ xnedit xnc)
	@source/xnedit -V

# This should not be in the default build, as users may not have Perl
# installed.  This is only interesting to developers.
docs:
	(cd doc; $(MAKE) all)

# We need a "dev-all" target that builds the docs plus binaries, but
# that doesn't work since we require the user to specify the target.  More
# thought is needed

clean:
	(cd util;   $(MAKE) -f Makefile.common clean)
	(cd Xlt;    $(MAKE) -f Makefile.common clean)
	(cd Microline/XmL;    $(MAKE) -f Makefile.common clean)
	(cd source; $(MAKE) -f Makefile.common clean)

realclean: clean
	(cd doc;    $(MAKE) clean)
#
# install binaries and other resources to $(PREFIX)
#
INSTALL_FILES=source/xnedit source/xnc
install: $(INSTALL_FILES)
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/icons
	mkdir -p $(PREFIX)/share/applications
	rm -f $(PREFIX)/bin/xnedit
	rm -f $(PREFIX)/bin/xnc
	cp source/xnedit $(PREFIX)/bin/xnedit
	cp source/xnc $(PREFIX)/bin/xnc
	cp resources/desktop/xnedit.png $(PREFIX)/share/icons/xnedit.png
	sed s:%PREFIX%:$(PREFIX):g resources/desktop/xnedit.desktop.template > $(PREFIX)/share/applications/xnedit.desktop

#
# The following is for creating binary packages of NEdit.
#
RELEASE=xnedit-1.0-`uname -s`-`uname -m`
BINDIST-FILES=source/xnedit source/xnc README LICENSE ReleaseNotes doc/xnedit.txt doc/xnedit.html doc/xnedit.man doc/xnc.man doc/faq.txt resources/desktop/xnedit.desktop resources/desktop/xnedit.png

dist-bin: $(BINDIST-FILES)
	rm -rf $(RELEASE)
	mkdir -p $(RELEASE)
	cp $(BINDIST-FILES) $(RELEASE)/
	strip $(RELEASE)/xnedit $(RELEASE)/xnc
	chmod 555 $(RELEASE)/xnedit $(RELEASE)/xnc
	tar cf $(RELEASE).tar $(RELEASE)
	-gzip -9 -c $(RELEASE).tar > $(RELEASE).tar.gz
	-bzip2 -9 -c $(RELEASE).tar > $(RELEASE).tar.bz2
	rm -rf $(RELEASE) $(RELEASE).tar
