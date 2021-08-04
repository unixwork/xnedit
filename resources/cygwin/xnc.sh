#!/bin/sh
# xnc.sh: Copyright 2021 Valerio Messina GNU GPL v2+
# xnc.sh is part of XNEdit multi-purpose text editor:
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

# xnc.sh: create the environment to run XNEdit out of Cygwin
# Note: to start XNedit run 'xnedit.bat' instead of this
ver="v0.02.0 2021/08/04"
echo "Starting 'xnc.sh' ..."
echo "Bash par1:$1"

export XNEditDir=`pwd`
echo cwd:$XNEditDir
echo "PATH=$PATH"
# to find the cygwin1.dll, shell commands, X libraries
export PATH="$XNEditDir/cygroot/bin:$PATH"
echo "PATH=$PATH"

#if (test "" = "$PROGRAMFILES") then
#   export PROGRAMFILES="D:\installer"
#fi
#echo "PROGRAMFILES=$PROGRAMFILES"
#export ProgramFiles=`./cygroot/bin/cygpath -u "$PROGRAMFILES"`
#echo "ProgramFiles=$ProgramFiles"

# specify a home directory, XNEdit needs it to store its preference files
# maybe used also to find fonts directory as specified in 'fonts.conf'
echo "HOME=$HOME"
#export HOME="$ProgramFiles/xnedit_64bit"
#export HOME=/cygdrive/c/installer/xnedit_64bit
if (test "" = "$HOME") then
   #export HOME=`cygpath -H`/$USERNAME
   export HOME=`cygpath -u $USERPROFILE`
fi
echo "HOME=$HOME"

# this is a new variable. when set, XNEdit stores its preference files
# in this directory under the new names `nedit.rc' (previous name `.nedit')
# and `autoload.nm' (previous name `.neditmacro')
#export XNEDIT_HOME="$ProgramFiles/xnedit_64bit"
#export XNEDIT_HOME=/cygdrive/c/installer/xnedit_64bit
export XNEDIT_HOME=$XNEditDir/.xnedit
echo "XNEDIT_HOME=$XNEDIT_HOME"

# to find the display
export DISPLAY=:0
echo "DISPLAY=$DISPLAY"

# for the keyboard, isn't always necessary
#export XKEYSYMDB="$ProgramFiles/xnedit_64bit/cygroot/bin/xkeysymdb"
#export XKEYSYMDB=/cygdrive/c/installer/xnedit_64bit/xkeysymdb
#export XKEYSYMDB=$XNEditDir/xkeysymdb
#echo "XKEYSYMDB=$XKEYSYMDB"

# used to find 'fonts.conf'
#export FONTCONFIG_PATH="/cygdrive/c/installer/xnedit_64bit"
export FONTCONFIG_PATH=$XNEditDir
echo "FONTCONFIG_PATH=$FONTCONFIG_PATH"

# fonts.conf: <dir prefix="xdg">fonts</dir>
# fonts are loaded from $XDG_DATA_HOME/fonts
#export XDG_DATA_HOME="/cygdrive/c/installer/xnedit_64bit"
export XDG_DATA_HOME=$XNEditDir/cygroot/usr/share
echo "XDG_DATA_HOME=$XDG_DATA_HOME"

export LANG=C
export LC_ALL=C

# if you bind XNEdit to file extensions so can start it by double-click on docs
# you should start your X server here to make sure it start before XNEdit!
# For example
#/cygdrive/c/mix_95/xs &
#/cygdrive/c/cygwin/bin/waimea &
#/usr/bin/startxwin &

# handling calls with multiple files, files and path names with empty spaces and
# upper-case - lower-case difference
if [ "$1" != "" ]
then
  filename="$*" # all args
  # since under Windows there is no difference between small and capital
  # letters, but XNEdit sees a difference internally, the file name and
  # path get translated to all lower-case
  filename=`cygpath -u "$filename" | tr A-Z a-z`
  echo "Run XNEdit with a parameter ..."
  xnedit "$filename" &
else
  echo "Run XNEdit without parameters ..."
  xnedit &
fi
