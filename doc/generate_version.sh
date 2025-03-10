#!/bin/sh
#
# Copyright 2023 Olaf Wintermann
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#


LANG=C
export LANG

XNEDIT_RELEASE_DATE=`date +'%b %e, %Y'`
XNEDIT_VERSION="XNEdit release of $XNEDIT_RELEASE_DATE"
XNEDIT_GIT_DEF=

# release tarballs usually don't contain the git repository
if [ -d "../.git" ]; then
    # try to get the git revision hash and date
    while true
    do
        # check if git command exists
        which git > /dev/null
        if [ $? -ne 0 ]; then
            echo "git not found"
            break
        fi
        
        # get current revision
        REV_SHORT=`git rev-parse --short HEAD`
        if [ $? -ne 0 ]; then
            break
        fi
        REV_LONG=`git rev-parse HEAD`
        
        # get revision date
        # REV_DATE=`git show -s --format=%cs $REV_LONG`
        REV_DATE=`git show -s --date=short --pretty=format:%cd $REV_LONG`
        
        if [ -z "$REV_SHORT" ]; then
            break;
        fi
        if [ -z "$REV_DATE" ]; then
            break
        fi
        
        XNEDIT_VERSION="XNEdit rev $REV_SHORT $REV_DATE"
        XNEDIT_GIT_DEF="#define XNEDIT_GIT_REV"
        
        break
    done
fi

# this script can generate a header file, if a filename is provided
if [ ! -z "$1" ]; then

	if [ -f "../doc/xnedit-release" ]; then
		XNEDIT_VERSION=`cat ../doc/xnedit-release`
	fi

    cat > .version.h << __EOF__
/* auto-generated by generate_version.sh */

$XNEDIT_GIT_DEF

#define XNEDIT_VERSION "$XNEDIT_VERSION"

__EOF__
    
    # don't refresh the file, if it hasn't changed
    if [ -f "$1" ]; then
        diff $1 .version.h > /dev/null 2>&1
        if  [ $? -ne 0 ]; then
		    echo "generate $1"
            cp -f .version.h $1
        fi
    else
        echo "generate $1"
        cp -f .version.h $1
    fi
    rm -f .version.h

else
    # script called by doc makefile
    echo $XNEDIT_VERSION
fi





