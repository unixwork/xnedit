XNEdit Version 1.4, January 2022
================================

XNEdit is a multi-purpose text editor for the X Window System, which combines
a standard, easy to use, graphical user interface with the thorough
functionality and stability required by users who edit text eight hours a day.
It provides intensive support for development in a wide variety of languages,
text processors, and other tools, but at the same time can be used productively
by just about anyone who needs to edit text.

XNEdit is a fork of the Nirvana Editor (NEdit) and provides new functionality
like antialiased text rendering and support for unicode.

BUILDING XNEDIT
---------------

Pre-built executables will be available for many operating system. Check out
the [XNEdit web page](https://www.unixwork.de/xnedit/).

If you have downloaded a pre-built executable you can skip ahead to the section
called INSTALLATION. Otherwise, the requirements to build XNEdit from the
sources are:

 - ANSI C99 compiler
 - make utility (eg, GNU make)
 - X11 development stuff (headers, libraries)
 - Xrender and Xft
 - Xpm
 - iconv (*BSD, cygwin)
 - Fontconfig
 - Motif 2.0 or above 
 - libpcre
 
Optionally one may use:
 
 - yacc (or GNU bison)

To build XNEdit from source, run make from XNEdit's root directory and specify
the build-configuration:

    make <build-config>

Available configurations are:
    
 - linux
 - solaris
 - freebsd
 - netbsd
 - openbsd
 - macosx
 - generic

If everything works properly, this will produce two executables called
'xnedit' and 'xnc' in the directory called 'source'.

INSTALLATION
------------

To install the XNEdit binaries and desktop integration files, run as root:

    make install

This will install the binaries to /usr/bin and desktop integration files to
`/usr/share`. The desktop integration consists of a starter file `xnedit.desktop`
and the icon `xnedit.png`.

To install to another destination, specify a path prefix with

    make install PREFIX=/path  

To install just the binaries, copy the files `xnedit` and `xnc` from the
`source` directory to your path.

RUNNING XNEDIT
--------------

To run XNEdit, simply type `xnedit`, optionally followed by the name of a file
or files to edit. On-line help is available from the pulldown menu on the far
right of the menu bar. For more information on the syntax of the xnedit command
line, look under the heading of "XNEdit Command Line".

The recommended way to use XNEdit, though, is in client/server mode, invoked by
the xnc executable. It allows you to edit multiple files within the same
instance of XNEdit (but still in multiple windows). This saves memory (only one
process keeps running), and enables additional functionality (such as find &
replace across multiple windows). See "Server Mode and xnc" in the help menu
for more information.

COMPATIBILITY WITH PREVIOUS VERSIONS
------------------------------------

### Upgrading from NEdit 5.7 to XNEdit 1.0

XNEdit uses the same preferences format as NEdit 5.7. Also the X-Resources have
the same appname `nedit`. Therefore NEdit's settings are compatible with
XNEdit, except the font configuration.

XNEdit's default configuration directory is `~/.xnedit`. To use your
existing NEdit settings in XNEdit, copy the content from `~/.nedit/` to
`~/.xnedit` or just rename the `~/.nedit` directory.

After that, you need to adjust the font settings. To use the default XNEdit
font settings, delete the following lines from the nedit.rc file:

    nedit.textFont: ...
    nedit.boldHighlightFont: ...
    nedit.italicHighlightFont: ...
    nedit.boldItalicHighlightFont: ...

Or alternatively, adjust the fonts directly in XNEdit.

### Upgrading from XNEdit 0.9 to 1.0

XNEdit 0.9 uses `xnedit` as application name for X-Resources. Version 1.0
switched back to `nedit` to improve compatibility with NEdit.

To convert XNEdit 0.9 settings to the new version, you have to rename all
resource strings in the `nedit.rc` file. Replace `xnedit` with `nedit`.

DOCUMENTATION
-------------

More information is available from XNEdit's on-line help system, the man-pages
and from the [web page](https://www.unixwork.de/xnedit/) for XNEdit.

SUPPORT
-------

Bug reports and feature requests can be issued at github or sourceforge,
or contact me via email.

 - https://github.com/unixwork/xnedit/issues
 - https://sourceforge.net/projects/xnedit/
 - Olaf Wintermann <olaf.wintermann@gmail.com>

AUTHORS
-------

XNEdit author: Olaf Wintermann

NEdit was written by Mark Edel, Joy Kyriakopulos, Christopher Conrad,
Jim Clark, Arnulfo Zepeda-Navratil, Suresh Ravoor, Tony Balinski, Max
Vohlken, Yunliang Yu, Donna Reid, Arne Førlie, Eddy De Greef, Steve
LoBasso, Alexander Mai, Scott Tringali, Thorsten Haude, Steve Haehn,
Andrew Hood, Nathaniel Gray, and TK Soh.

The regular expression matching routines used in NEdit are adapted (with
permission) from original code written by Henry Spencer at the University of
Toronto.

The Microline widgets are inherited from the Mozilla project.

Syntax highlighting patterns and smart indent macros were contributed
by: Simon T. MacDonald,  Maurice Leysens, Matt Majka, Alfred Smeenk,
Alain Fargues, Christopher Conrad, Scott Markinson, Konrad Bernloehr,
Ivan Herman, Patrice Venant, Christian Denat, Philippe Couton, Max Vohlken, 
Markus Schwarzenberg, Himanshu Gohel, Steven C. Kapp, Michael Turomsha, 
John Fieber, Chris Ross, Nathaniel Gray, Joachim Lous, Mike Duigou, 
Seak Teng-Fong, Joor Loohuis, Mark Jones, and Niek van den Berg.

LICENSE
-------

XNEdit may be freely distributed under the terms of the GNU
General Public License

See the LICENSE file for more informations.

