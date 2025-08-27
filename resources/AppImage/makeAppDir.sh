#!/bin/bash
# makeAppDir.sh: this script generate the AppDir for a binary
#
# Copyright 2023 Valerio Messina
# makeAppDir.sh is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# makeAppDir.sh is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with makeAppDir.sh. If not, see <http://www.gnu.org/licenses/>.
#
# Syntax: $ makeAppDir.sh [-y] XNEdit source/xnedit resources/desktop Linux [32|64]
#
ver="2025-08-26"
echo "makeAppDir.sh v.${ver}: generating the AppDir for a binary"
flag=0 # check for common external dependancy compliance
for extCmd in basename chmod cp getconf ls mkdir mv rm uname wget ; do
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
if [[ "$1" = "-y" ]]; then
   batch=1
   shift
fi
if (test "" = "$4") then
   echo "makeAppDir.sh ERROR: need the appName"
   echo "Syntax: $ makeAppDir.sh [-y] appName path/binary pathRes Linux|Win [32|64]"
   echo "          -y for bath execution, no confirmation"
   exit
fi
APP=$1 # eg. XNEdit
BIN=$2 # eg. source/bin
NAME=`basename $BIN`
#echo "BIN:$BIN NAME:$NAME"
RES=$3 # eg. resources/desktop
PKG=$4
if (! test -s "$BIN") then
   echo "makeAppDir.sh ERROR: binary not valid/exist"
   echo "Syntax: $ makeAppDir.sh [-y] appName path/binary pathRes Linux|Win [32|64]"
   echo "          -y for bath execution, no confirmation"
   exit
fi
if (test "$PKG" != "Linux" && test "$PKG" != "Win") then
   echo "makeAppDir.sh ERROR: unsupported target platform $PKG"
   echo "Syntax: $ makeAppDir.sh [-y] appName path/binary pathRes Linux|Win [32|64]"
   echo "          -y for bath execution, no confirmation"
   exit
fi
if (test "" = "$5") then
   BIT=$(getconf LONG_BIT)
else
   BIT=$5
fi
CPU=`uname -m`
if (test "$CPU" = "x86_64" && test "$BIT" = "32") then # built on 64 bit host with 32 bit target
   CPU=i686
fi
OS=`uname` # current OS
if (test "$OS" != "Darwin") then
   OS=`uname -o`  # Msys or GNU/Linux, illegal on macOS
fi
if (test "$PKG" = "Win") then
   EXT=".exe"
fi
DIR="AppDir"
DATE=`date -I`

echo "OS  : $OS"
echo "CPU : $CPU"
echo "BIT : $BIT"
echo "APP : $APP"
echo "BIN : $BIN"
echo "NAME: $NAME"
echo "RES : $RES"
echo "DIR : $DIR"
echo "PKG : $PKG"
echo "DATE: $DATE"
if (test "$batch" != "1") then
   read -p "Press RETURN to proceed ..."
fi
echo ""

echo "makeAppDir.sh: generating $APP $NAME $PKG $CPU ${BIT}-bit in ${DIR} ..."
rm -rf ${DIR}
mkdir -p ${DIR}/usr/bin
cp -a CHANGELOG LICENSE README.md ReleaseNotes ${DIR} # text files
#cp -a ${RES}/${NAME}.png ${DIR} # icon file
#cp -a ${RES}/${NAME}.desktop ${DIR} # freedesktop file
cp -a ${BIN} ${DIR}/usr/bin # binaries
cp -a source/xnc${EXT} ${DIR}/usr/bin # binaries
if (test "$PKG" = "Linux" && (test "$CPU" = "x86_64" || test "$CPU" = "i686")) then # skip on ARM&RISC-V
   if (test "$BIT" = "64") then
      echo "makeAppDir.sh: Generating the AppDir for $BIN ..."
      if (! test -x linuxdeploy-x86_64.AppImage) then
         wget -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" 2> wgetLog$DATE.txt
         chmod +x linuxdeploy-x86_64.AppImage
      fi
      ./linuxdeploy-x86_64.AppImage -e ${BIN} --appdir ${DIR} -i ${RES}/${NAME}.png -d ${RES}/${NAME}.desktop > linuxDeployLog$DATE.txt
      cwd=`pwd`
      cd ${DIR}
      rm AppRun
      ln -s usr/bin/xnedit AppRun
      cd "$cwd"
      echo "makeAppDir.sh: Generating the AppImage for $BIN ..."
      ./linuxdeploy-x86_64.AppImage --appdir ${DIR} --output appimage >> linuxDeployLog$DATE.txt
      echo ""
      echo "makeAppDir.sh: Generated AppImage:"
      mv ${APP}-*x86_64.AppImage ${APP}-${DATE}_x86_64.AppImage
      ls -l ${APP}-*_x86_64.AppImage
      #mv ${APP}-*_x86_64.AppImage ../${APP}_x86_64.AppImage
      #rm linuxdeploy-x86_64.AppImage
   fi
   if (test "$BIT" = "32") then
      echo "makeAppDir.sh: Generating the AppDir for $BIN ..."
      if (! test -x linuxdeploy-i386.AppImage) then
         wget -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-i386.AppImage" 2> wgetLog$DATE.txt
         chmod +x linuxdeploy-i386.AppImage
      fi
      ./linuxdeploy-i386.AppImage -e ${BIN} --appdir ${DIR} -i ${RES}/${NAME}.png -d ${RES}/${NAME}.desktop > linuxDeployLog$DATE.txt
      cwd=`pwd`
      cd ${DIR}
      rm AppRun
      ln -s usr/bin/xnedit AppRun
      cd "$cwd"
      echo "makeAppDir.sh: Generating the AppImage for $BIN ..."
      ./linuxdeploy-i386.AppImage --appdir ${DIR} --output appimage >> linuxDeployLog$DATE.txt
      echo ""
      mv ${APP}-*i386.AppImage ${APP}-${DATE}_i386.AppImage
      echo "makeAppDir.sh: Generated AppImage:"
      ls -l ${APP}-*_i386.AppImage
      #mv ${APP}-*_i386.AppImage ../${APP}_i386.AppImage
      #rm linuxdeploy-i386.AppImage
   fi
   #rm linuxDeployLog$DATE.txt
   echo "Done"
fi
#rm -rf ${DIR}
