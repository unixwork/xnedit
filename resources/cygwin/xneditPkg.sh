#!/bin/bash
# xneditPkg.sh: Copyright 2021-2023 Valerio Messina GNU GPL v2+
# xneditPkg.sh is part of XNEdit multi-purpose text editor:
# https://github.com/unixwork/xnedit a fork of Nedit http://www.nedit.org
# XNEdit is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# XNEdit is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with XNEdit. If not, see <http://www.gnu.org/licenses/>.

# xneditPkg.sh: create a Win portable package for XNEdit with all dependencies
# Note: must be run in Cygwin, does not work in MinGw/MSYS2/Unix
# Note: if need a different installation path than $PROGRAMFILES change 'pkg'
#       eg. "/cygdrive/c/installer/xnedit"
# Note: default assume jut built binaries path is "../../source", to change
# you can force the path filling 'bin', eg. bin="$HOME/c/xnedit/source"
bin=""
pkg="/cygdrive/d/installer/xnedit"
pkg="/cygdrive/d/ProgramFiles/xnedit"
pkg="$PROGRAMFILES/xnedit"
dbg=0 # set to 1 to have debug prints and not stripped files

ver="v0.04.01 2023/09/07"
echo "xneditPkg.sh $ver create a Win package for XNEdit"
# check for external dependancy compliance
flag=0
for extCmd in awk bash basename cat ctags cygstart dirname dos2unix grep mktemp realpath sed sleep strip test uname which ; do
   #echo $extCmd
   exist=`which $extCmd 2> /dev/null`
   if (test "" = "$exist") then
      echo "Required external dependancy: "\"$extCmd\"" unsatisfied!"
      flag=1
   fi
done
if (test "$flag" = 1) then
   echo "ERROR: Install the required packages and retry. Exit"
   exit
fi

# check the running env
os=`uname -o`
if (test "$os" != "Cygwin") then
   echo "ERROR: You are not on Cygwin"
   exit
fi
cpu=`uname -m`
if (test "$cpu" = "x86_64") then
   bit=64
fi
if (test "$cpu" = "i686") then 
   bit=32
fi

relPath=`dirname $0`
scriptPath=`realpath "$relPath"`
# check the distribution package has all needed files
for file in cygspecial.nm winclip.nm xnc.sh xnedit.ico xnedit.nsi ; do
   if (! test -e "$scriptPath/$file") then
      echo "ERROR: Cannot find: $scriptPath/$file"
      exit
   fi
done
src=`realpath "$scriptPath/../.."` # git root
if (test "" = "$bin") then
   bin="$src/source" # where binary goes with make
fi
if (! test -e "$bin") then
   echo "ERROR: Cannot find source/bin directory: $bin"
   exit
fi
root="/usr" # where to pick dependencies
pkg=`cygpath -u "$pkg"`
pkg="$pkg"_$bit"bit"
pkgWin=`cygpath -w "$pkg"`
xnVer=`grep NEditVersion $bin/help_data.h | awk -F'"' '{print $2}' | sed 's/XNEdit //' | awk '{print $1,$2,$3,$4,$5}' | sed 's/\\\n/ /g' | sed 's/ *$//g' | sed 's/,//g' | sed 's/ /_/g'`

echo "XNEdit script files: $scriptPath"
echo "XNEdit binaries    : $bin"
echo "Dependencies from  : $root"
echo "Package created in : $pkg"
echo "Package created in : $pkgWin"
echo "Debug is (1=active): $dbg"
read -p "Press Return to start ..."
echo ""
echo "Packaging 'XNEdit' v$xnVer for Win$bit ..."
cd "$bin"

rm -rf "$pkg" 2> /dev/null # remove prev installation

# create destination directory
tempname=$(basename `mktemp -u`)
echo "$pkgWin" > "$scriptPath/$tempname.sh"
prog=`grep -F "$PROGRAMFILES" "$scriptPath/$tempname.sh"`
if (test "" = "$prog") then # not protected path
   mkdir -p "$pkg/cygroot/bin"
else # protected path
   # cygstart --action=runas do not work when args has spaces, so ...
   echo "/usr/bin/mkdir -p \"$pkg/cygroot/bin\"" > "$scriptPath/$tempname.sh"
   cygstart --action=runas /bin/bash "$scriptPath/$tempname.sh"
   sleep 2
fi
if (test "$dbg" != "1") then
   rm "$scriptPath/$tempname.sh"
fi
if (! test -e "$pkg/cygroot/bin") then
   echo "ERROR: Cannot create destination directory: $pkg"
   exit
fi

# copy main binaries
cp xnedit.exe "$pkg/cygroot/bin"
if (test "$dbg" != "1") then
   strip "$pkg/cygroot/bin/xnedit.exe"
fi
cp xnc.exe "$pkg/cygroot/bin"
if (test "$dbg" != "1") then
   strip "$pkg/cygroot/bin/xnc.exe"
fi

# direct dependencies of xnedit and xnc
cygver=`uname -s`
for file in cygX11-6.dll cygwin1.dll cygxcb-1.dll cygXau-6.dll cygXdmcp-6.dll cygXft-2.dll cygfontconfig-1.dll cygexpat-1.dll cygfreetype-6.dll cygbrotlidec-1.dll cygbrotlicommon-1.dll cygbz2-1.dll cygpng16-16.dll cygz.dll cygintl-8.dll cygiconv-2.dll cyguuid-1.dll cygXrender-1.dll cygXm-4.dll cygjpeg-8.dll cygXext-6.dll cygXmu-6.dll cygXt-6.dll cygICE-6.dll cygSM-6.dll cygXpm-4.dll cygpcre-1.dll ; do
   if (test "$cygver" = "CYGWIN_NT-5.1" && test "$file" = "cygbrotlidec-1.dll") then continue ; fi
   if (test "$cygver" = "CYGWIN_NT-5.1" && test "$file" = "cygbrotlicommon-1.dll") then continue ; fi
   cp "$root/bin/$file" "$pkg/cygroot/bin"
   if (test "$file" = "cygwin1.dll") then continue ; fi
   if (test "$dbg" != "1") then
      strip "$pkg/cygroot/bin/$file"
   fi
done
if (test "$bit" = "64") then
   cp "$root/bin/cyggcc_s-seh-1.dll" "$pkg/cygroot/bin"
   if (test "$dbg" != "1") then
      strip "$pkg/cygroot/bin/cyggcc_s-seh-1.dll"
   fi
fi
if (test "$bit" = "32") then
   cp "$root/bin/cyggcc_s-1.dll" "$pkg/cygroot/bin"
   if (test "$dbg" != "1") then
      strip "$pkg/cygroot/bin/cyggcc_s-1.dll"
   fi
fi

# XNEdit config
mkdir -p "$pkg/.xnedit"
cp "$HOME/.xnedit/nedit.rc" "$pkg/.xnedit"
echo '#load_macro_file(getenv("ProgramFiles") "/.xnedit/cygspecial.nm")' > "$pkg/.xnedit/autoload.nm"
echo 'load_macro_file(getenv("XNEditDir") "/.xnedit/cygspecial.nm")' > "$pkg/.xnedit/autoload.nm"
cp "$scriptPath/cygspecial.nm" "$pkg/.xnedit"
# Note: as now do not copy winclip.nm
cp "$scriptPath/xnedit.ico" "$pkg"

# other needed files
for file in ctags.exe test.exe sleep.exe echo.exe date.exe ls.exe wc.exe unexpand.exe tr.exe sort.exe nl.exe expand.exe cat.exe grep.exe diff.exe cygpath.exe gawk.exe cygsigsegv-2.dll; do
   cp -P "$root/bin/$file" "$pkg/cygroot/bin"
   if (test "$dbg" != "1") then
      strip "$pkg/cygroot/bin/$file"
   fi
done
mv "$pkg/cygroot/bin/gawk.exe" "$pkg/cygroot/bin/awk.exe" # NSIS can't process links

# files needed to run 'bash.exe xnc.sh'
for file in bash.exe sed.exe dos2unix.exe cygreadline7.dll cygncursesw-10.dll ; do
   cp "$root/bin/$file" "$pkg/cygroot/bin"
   if (test "$dbg" != "1") then
      strip "$pkg/cygroot/bin/$file"
   fi
done
mkdir -p "$pkg/cygroot/tmp" # avoid "bash.exe: warning: could not find /tmp, please create!"

# files needed to run Shell menu commands:
for file in cygmpfr-6.dll cyggmp-10.dll; do
   if (test "$cygver" = "CYGWIN_NT-5.1" && test "$file" = "cygmpfr-6.dll") then continue ; fi
   cp "$root/bin/$file" "$pkg/cygroot/bin"
   if (test "$dbg" != "1") then
      strip "$pkg/cygroot/bin/$file"
   fi
done

# create the fonts directory and fonts.conf file
mkdir -p "$pkg/cygroot/usr/share/fonts/dejavu"
cp /usr/share/fonts/dejavu/DejaVuSansMono*.ttf "$pkg/cygroot/usr/share/fonts/dejavu"
cat /etc/fonts/fonts.conf | grep -vF "</fontconfig>" > "$pkg/$tempname.conf"
cat "$pkg/$tempname.conf" | sed 's#<dir>~/\.fonts</dir>#<dir>~/fonts</dir><dir>'"$pkg"'/fonts</dir>#' > "$pkg/fonts.conf"
rm "$pkg/$tempname.conf"
echo "<!--"                                                              >> "$pkg/fonts.conf"
echo "  Accept  'monospace' alias, replacing it with 'DejaVu Sans Mono'" >> "$pkg/fonts.conf"
echo "-->"                                                               >> "$pkg/fonts.conf"
echo "	<match target=\"pattern\">"                                     >> "$pkg/fonts.conf"
echo "		<test qual=\"any\" name=\"family\">"                         >> "$pkg/fonts.conf"
echo "			<string>monospace</string>"                               >> "$pkg/fonts.conf"
echo "		</test>"                                                     >> "$pkg/fonts.conf"
echo "		<edit name=\"family\" mode=\"assign\" binding=\"same\">"     >> "$pkg/fonts.conf"
echo "			<string>DejaVu Sans Mono</string>"                        >> "$pkg/fonts.conf"
echo "		</edit>"                                                     >> "$pkg/fonts.conf"
echo "	</match>"                                                       >> "$pkg/fonts.conf"
echo "</fontconfig>"                                                     >> "$pkg/fonts.conf"

# XNEdit need to input non ASCII characters
mkdir -p "$pkg/cygroot/usr/share/x11/locale"
cp -a /usr/share/x11/locale "$pkg/cygroot/usr/share/x11"

# used to hide the Windows console
echo 'CreateObject("Wscript.Shell").Run "" & WScript.Arguments(0) & "", 0, False' > "$pkg/hide.vbs"
unix2dos -q "$pkg/hide.vbs"

# create the run batch and script file:
echo "@ECHO OFF"                         >  "$pkg/xnedit.bat"
echo "rem This Batch file start XNEdit"  >> "$pkg/xnedit.bat"
if (test "$dbg" = "1") then
   echo "echo Starting 'xnedit.bat' ..." >> "$pkg/xnedit.bat"
   echo "echo Batch par1:%1"             >> "$pkg/xnedit.bat"
   echo "echo Batch par2:%2"             >> "$pkg/xnedit.bat"
   echo "cd"                             >> "$pkg/xnedit.bat"
fi
echo "IF '%1'=='' GOTO NoParam"          >> "$pkg/xnedit.bat"
echo ":Loop"                             >> "$pkg/xnedit.bat"
echo "IF '%1'=='' GOTO End"              >> "$pkg/xnedit.bat"
if (test "$dbg" != "1") then
   echo 'C:\Windows\System32\wscript.exe "hide.vbs" "cygroot\bin\bash.exe xnc.sh "%1""' >> "$pkg/xnedit.bat"
else
   echo "cygroot\bin\bash.exe xnc.sh %1" >> "$pkg/xnedit.bat"
fi
echo "SHIFT"                             >> "$pkg/xnedit.bat"
echo "GOTO Loop"                         >> "$pkg/xnedit.bat"
echo ":NoParam"                          >> "$pkg/xnedit.bat"
if (test "$dbg" != "1") then
   echo 'C:\Windows\System32\wscript.exe "hide.vbs" "cygroot\bin\bash.exe xnc.sh"' >> "$pkg/xnedit.bat"
else
   echo "cygroot\bin\bash.exe xnc.sh"    >> "$pkg/xnedit.bat"
fi
echo ":End"                              >> "$pkg/xnedit.bat"
#echo "pause"                            >> "$pkg/xnedit.bat"
unix2dos -q "$pkg/xnedit.bat"
cp "$scriptPath/xnc.sh" "$pkg" # run script

# copy text files
cd "$src"
cp LICENSE README.md ReleaseNotes CHANGELOG "$pkg"

# create the Windows shortcut in $pkgWin
cd "$scriptPath"
echo "Set oWS = WScript.CreateObject(\"WScript.Shell\")"  >  link.vbs
echo "sLinkFile = \"$pkgWin\XNEdit.lnk\""                 >> link.vbs
echo "Set oLink = oWS.CreateShortcut(sLinkFile)"          >> link.vbs
echo "    oLink.TargetPath = \"$pkgWin\xnedit.bat\""      >> link.vbs
echo " '  oLink.Arguments = \"\""                         >> link.vbs
echo " '  oLink.Description = \"XNEdit\""                 >> link.vbs
echo " '  oLink.HotKey = \"CTRL+ALT+SHIFT+N\""            >> link.vbs
echo "    oLink.IconLocation = \"$pkgWin\xnedit.ico, 0\"" >> link.vbs
echo " '  oLink.WindowStyle = \"1\""                      >> link.vbs
echo "    oLink.WorkingDirectory = \"$pkgWin\""           >> link.vbs
echo "oLink.Save"                                         >> link.vbs
unix2dos -q link.vbs
wscript.exe link.vbs
if (test "$dbg" != "1") then
   rm link.vbs
fi

# copy NSIS install script
cat xnedit.nsi | sed 's/XNEditM\.m\.d/XNEdit'_$xnVer'_/' > $tempname.nsi
cat $tempname.nsi | sed 's/winXX_setup/win'$bit'_setup/' > "$pkg/xnedit.nsi"
rm $tempname.nsi
if (test "$bit" = "32") then
   cat "$pkg/xnedit.nsi" | sed 's/PROGRAMFILES64/PROGRAMFILES/' > $tempname.nsi
   mv $tempname.nsi "$pkg/xnedit.nsi"
fi
unix2dos -q "$pkg/xnedit.nsi"

if (test "$cygver" != "CYGWIN_NT-5.1") then
   icacls "$pkgWin\\xnedit.bat" /reset /q > /dev/null # fix Cygwin file permissions
else # we are on WinXP
   echo "Setting ACL for files ..."
   ##echo y| cacls "$pkgWin\\xnedit.bat" /t /c /p everyone:f # do not work on localizations
   #subinacl /file "$pkgWin" /setowner=$USER > /dev/null
   #subinacl /noverbose /subdirectories "$pkgWin\*" /setowner=$USER > /dev/null
   #cscript XCACLS.vbs "$pkgWin" /q /f /s /t /p everyone:f > /dev/null
fi

echo Done
