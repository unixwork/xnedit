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
	@ echo "   make VERSION='NEdit 5.3' all"
	@ echo ""
	@ echo "When the version is not specified, the default value will"
	@ echo "be 'NEdit release of <currentDate>'"
	@ echo ""
	
.version:
	@ if [ "$(VERSION)" = "" ]; then \
	      echo "NEdit release of `date +'%b %e, %Y'`" > .version; \
	  else \
	      echo "$(VERSION)" > .version; \
	  fi
help: ../source/help_topic.h ../source/help_data.h setext

../source/help_topic.h ../source/help_data.h: .version help.etx setext
	@ echo "Creating NEdit help code `cat .version`"
	@ ./setext -m -v version="`cat .version`" help.etx
	@ mv -f help_topic.h ../source
	@ mv -f help_data.h ../source

nedit.html: .version help.etx setext
	@ echo "Creating NEdit HTML documentation..."
	@ ./setext -v version="`cat .version`" help.etx nedit.html

html/nedit.html: .version help.etx setext
	@ echo "Creating NEdit HTML website documentation..."
	@ if [ ! -d html ]; then mkdir html; fi
	@ if [ -d html ]; then \
	      cd html; \
	      ../setext -S -v version="`cat ../.version`" ../help.etx nedit.html; \
	  else \
	      echo "** Unable to create html directory to hold NEdit documentation"; \
	  fi

nedit.doc: .version help.etx setext
	@ echo "Creating NEdit plain text documenation..."
	@ ./setext -c NEDITDOC -v version="`cat .version`" help.etx nedit.doc

nc.man: .version
nedit.man: .version
.pod.man:
	@ echo "Creating $* man page..."
	@ which pod2man > /dev/null 2>&1 || ( echo "Sorry, you need pod2man." && exit 1 )
	@ pod2man --release="`cat .version`" --center="NEdit documentation" $*.pod > $@ 

doc: .version nedit.doc nedit.html html/nedit.html

man: .version nedit.man nc.man

all: help doc man

clean:
	@ echo "Removing generated NEdit documentation..."
	@ rm -rf help nedit.html html nedit.doc .version nedit.man nc.man

#
# FAQ targets. Requires an XSLT parser and processor, such as Java with the
# XP and XT packages (http://www.jclark.com/xml/)
# Override the XSLT variable at the command line as follows:
#
#   make XSLT=<command to process the xsl files> faq
#
# Example: make XSLT="java -classpath xp.jar:xt.jar com.jclark.xsl.sax.Driver" faq
#
XSLT=undefined

faq: check_xsl faq.txt html/faq/index.shtml

faq.txt: faq.xml faq-txt.xsl faq-txt-pass2.xsl faq-txt.awk
	@ echo Building FAQ - text version
	@ rm -f faq-txt.html faq-txt-tmp.txt faq.txt
	@ ${XSLT} faq.xml faq-txt.xsl faq-txt.html
	@ ${XSLT} faq-txt.html faq-txt-pass2.xsl faq-txt-tmp.txt
	@ awk -f faq-txt.awk < faq-txt-tmp.txt > faq.txt
	@ rm -f faq-txt-tmp.txt

html/faq/index.shtml: faq.xml faq-txt.xsl
	@ echo Building FAQ - HTML version
	@ rm -f html/faq/*.shtml
	@ if [ ! -d html ]; then mkdir html; fi
	@ if [ ! -d html/faq ]; then mkdir html/faq; fi
	@ ${XSLT} faq.xml faq.xsl html/faq/index.shtml

check_xsl:
	@ if [ "x${XSLT}" = "xundefined" ]; \
          then\
	    echo "To rebuild the FAQ, an XSLT parser and processer are required.";\
            echo "For instance, a Java runtime environment and the XT and XP";\
	    echo "packages (http://www.jclark.com/xml/) can be used.";\
	    echo "Then type:";\
            echo "";\
	    echo "   make XSLT=<command to process the xsl files> faq";\
            echo "";\
	    echo "For instance:";\
            echo "";\
            echo "   make XSLT=\"java -classpath xp.jar:xt.jar com.jclark.xsl.sax.Driver\" faq";\
            echo "";\
	    exit 1;\
          fi

FAQFILES = faq-txt.html faq-txt.xsl faq-txt-pass2.xsl faq-txt.awk\
           faq-txt.dtd README.FAQ faq.txt Makefile faq.xml faq.xsl\
           faq.dtd
faq-dist:
	@ echo "Building faq.tar.gz";\
          rm -f faq.tar.gz;\
	  DISTFILES="${FAQFILES} `ls html/faq/*.shtml`";\
	  tar cv $${DISTFILES} | gzip -c > faq.tar.gz;\
	  echo "Building faq.zip";\
	  rm -f faq.zip;\
	  zip faq.zip $${DISTFILES};\
