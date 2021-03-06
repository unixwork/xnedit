#!/bin/sh
# this script create the environment to run XNEdit from Cygwin
echo "Starting 'xnc.sh' ..."
echo Bash param:$1

#pwd
export XNEditDir=`pwd`
echo PATH=$PATH
# to find the cygwin1.dll, shell commands, X libraries
export PATH="$XNEditDir/usr/bin:$PATH"
echo PATH=$PATH

#if (test "" = "$PROGRAMFILES") then
#   export PROGRAMFILES="D:\installer"
#fi
#echo PROGRAMFILES=$PROGRAMFILES
#export ProgramFiles=`./usr/bin/cygpath -u "$PROGRAMFILES"`
#echo ProgramFiles=$ProgramFiles

# specify a home directory, XNEdit needs it to store its preference files
# maybe used also to find fonts directory as specified in 'fonts.conf'
echo HOME=$HOME
#export HOME="$ProgramFiles/xnedit_64bit"
#export HOME=/cygdrive/d/installer/xnedit_64bit
if (test "" = "$HOME") then
   #export HOME=`cygpath -H`/$USERNAME
   export HOME=`cygpath -u $USERPROFILE`
fi
echo HOME=$HOME

# this is a new variable. when set, XNEdit stores its preference files
# in this directory under the new names `nedit.rc' (previous name `.nedit')
# and `autoload.nm' (previous name `.neditmacro')
#export XNEDIT_HOME="$ProgramFiles/xnedit_64bit"
#export XNEDIT_HOME=/cygdrive/d/installer/xnedit_64bit
export XNEDIT_HOME=$XNEditDir/.xnedit
echo XNEDIT_HOME=$XNEDIT_HOME

# to find the display
#export DISPLAY=localhost:0.0
export DISPLAY=:0
echo DISPLAY=$DISPLAY

# for the keyboard, isn't always necessary
#export XKEYSYMDB="$ProgramFiles/xnedit_64bit/cygwin/bin/xkeysymdb"
#export XKEYSYMDB=/cygdrive/d/installer/xnedit_64bit/xkeysymdb
export XKEYSYMDB=$XNEditDir/xkeysymdb
echo XKEYSYMDB=$XKEYSYMDB

# used to find 'fonts.conf'
#export FONTCONFIG_PATH="/cygdrive/d/installer/xnedit_64bit"
export FONTCONFIG_PATH=$XNEditDir
echo FONTCONFIG_PATH=$FONTCONFIG_PATH

# fonts.conf: <dir prefix="xdg">fonts</dir>
# fonts are loaded from $XDG_DATA_HOME/fonts
#export XDG_DATA_HOME="/cygdrive/d/installer/xnedit_64bit"
export XDG_DATA_HOME=$XNEditDir
echo XDG_DATA_HOME=$XDG_DATA_HOME

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
  echo Run Xnedit with a parameter ...
  xnedit "$filename" &
else
  echo Run Xnedit without parameters ...
  xnedit &
fi
