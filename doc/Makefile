# Makefile for NEdit text editor documentation
#
#  $Id: Makefile,v 1.6 2003/11/24 17:41:34 edg Exp $
#
#  NEdit help documentation and internal help code using one common
#  source, help.etx, a Structure Enhanced TEXT document.
#
.SUFFIXES: .man .pod

instructions:
	@ echo ""
	@ echo "This make file is intended for NEdit developers only."
	@ echo ""
	@ echo "It uses a perl program (setext) to extract the various forms"
	@ echo "of the NEdit help documentation and internal help code using"
	@ echo "one common source, help.etx, a Structure Enhanced TEXT document."
	@ echo ""
	@ echo "Additionally, it generates the nedit and nc man pages."
	@ echo "To generate those you need to have perl and pod2man installed."
	@ echo ""
	@ echo "The following are the main targets which generate files for"
	@ echo "NEdit development. They are:"
	@ echo ""
	@ echo "  help - generates NEdit help code (help_topic.h, help_data.h)"
	@ echo "  doc  - generates various forms of NEdit documentation"
	@ echo "  man  - generates the nedit and nc man pages"
	@ echo "  all  - generates all the files"
	@ echo ""
	@ echo "Remember to specify the VERSION macro on the make command"
	@ echo "or as an environment variable so that the NEdit version"
	@ echo "gets placed appropriately. For example, the following"
	@ echo "command creates all the files for NEdit version 5.3"
	@ echo ""
	@ echo "   make VERSION='XNEdit 1.5' all"
	@ echo ""
	@ echo "When the version is not specified, the default value will"
	@ echo "be 'XNEdit rev <gitRev>'"
	@ echo ""
	
.version:
	@ if [ "$(VERSION)" = "" ]; then \
	      ./generate_version.sh > .version; \
	  else \
	      echo "$(VERSION)" > .version; \
	  fi
help: ../source/help_topic.h ../source/help_data.h setext

../source/help_topic.h ../source/help_data.h: .version help.etx setext
	@ echo "Creating XNEdit help code `cat .version`"
	@ ./setext -m -v version="`cat .version`" help.etx
	@ mv -f help_topic.h ../source
	@ mv -f help_data.h ../source
	@ if [ ! -z "$(VERSION)" ]; then \
		echo "#define XNEDIT_IS_RELEASE" >> ../source/help_data.h ; \
	  fi

xnedit.html: .version help.etx setext
	@ echo "Creating XNEdit HTML documentation..."
	@ ./setext -v version="`cat .version`" help.etx xnedit.html

html/xnedit.html: .version help.etx setext
	@ echo "Creating XNEdit HTML website documentation..."
	@ if [ ! -d html ]; then mkdir html; fi
	@ if [ -d html ]; then \
	      cd html; \
	      ../setext -S -v version="`cat ../.version`" ../help.etx xnedit.html; \
	  else \
	      echo "** Unable to create html directory to hold XNEdit documentation"; \
	  fi

xnedit.txt: .version help.etx setext
	@ echo "Creating XNEdit plain text documenation..."
	@ ./setext -c NEDITDOC -v version="`cat .version`" help.etx xnedit.txt

xnc.man: .version
xnedit.man: .version
.pod.man:
	@ echo "Creating $* man page..."
	@ which pod2man > /dev/null 2>&1 || ( echo "Sorry, you need pod2man." && exit 1 )
	@ pod2man --release="`cat .version`" --center="XNEdit documentation" $*.pod > $@ 

doc: .version xnedit.txt xnedit.html html/xnedit.html

man: .version xnedit.man xnc.man

all: help doc man

clean:
	@ echo "Removing generated XNEdit documentation..."
	@ rm -rf help xnedit.html html xnedit.txt .version xnedit.man xnc.man

