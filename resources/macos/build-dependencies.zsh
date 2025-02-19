#!/bin/zsh

BUILD_ROOT=$(pwd)
BUILD_DIR=$BUILD_ROOT/build
BUILD_INSTALL=$BUILD_ROOT/install

LIB_DIR=$BUILD_INSTALL/lib

export PKG_CONFIG_PATH=$BUILD_INSTALL/lib/pkgconfig:$BUILD_INSTALL/share/pkgconfig

rm -Rf $BUILD_DIR
rm -Rf $BUILD_INSTALL

src=(
	"https://www.x.org/releases/individual/proto/xcb-proto-1.17.0.tar.gz"
	"https://www.x.org/releases/individual/xcb/libpthread-stubs-0.5.tar.gz"
	"https://www.x.org/releases/individual/proto/xproto-7.0.31.tar.bz2"
	"https://www.x.org/releases/individual/lib/libXau-1.0.12.tar.gz"
	"https://www.x.org/releases/individual/lib/libXdmcp-1.1.5.tar.gz"
	"https://www.x.org/releases/individual/lib/libxcb-1.17.0.tar.gz"
	"https://www.x.org/releases/individual/proto/xextproto-7.3.0.tar.bz2"
	"https://www.x.org/releases/individual/lib/xtrans-1.5.2.tar.gz"
	"https://www.x.org/releases/individual/proto/kbproto-1.0.7.tar.bz2"
	"https://www.x.org/releases/individual/proto/inputproto-2.3.tar.bz2"
	"https://www.x.org/releases/individual/lib/libX11-1.8.11.tar.gz"
	"https://www.x.org/releases/individual/proto/renderproto-0.11.tar.bz2"
	"https://www.x.org/releases/individual/lib/libXrender-0.9.12.tar.gz"
	"https://www.zlib.net/zlib-1.3.1.tar.gz"
	"https://ftp-osl.osuosl.org/pub/libpng/src/libpng16/libpng-1.6.34.tar.gz"
	"https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.gz"
	"https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.16.0.tar.xz"
	"https://www.x.org/releases/individual/lib/libXft-2.3.8.tar.gz"
	"https://www.x.org/releases/individual/lib/libICE-1.1.2.tar.gz"
	"https://www.x.org/releases/individual/lib/libSM-1.2.5.tar.gz"
	"https://www.x.org/releases/individual/lib/libXt-1.3.1.tar.gz"
	"https://www.x.org/releases/individual/lib/libXext-1.3.6.tar.gz"
	"https://www.x.org/releases/individual/lib/libXmu-1.2.1.tar.gz"
	"https://www.x.org/releases/individual/data/xbitmaps-1.1.3.tar.gz"
	)

src_motif="https://pkg.unixwork.de/src/motif-2.3.8.tar.gz"

mkdir -p $BUILD_DIR

echo "enter build dir $BUILD_DIR"
echo 
cd $BUILD_DIR

for url in "${src[@]}"; do
	echo "------------------------------------------------------"
	echo 
    echo "## download $url"
    curl -L -O $url

    dlfile=$(basename $url)
    dirname=${dlfile%.tar.*}
    # BSD/macOS tar should just work with tar.gz or tar.xz
    tar xvf $dlfile

    echo
    echo "##enter $BUILD_DIR/$dirname"
    cd $dirname
    if [ $? -ne 0 ]; then
    	echo "abort"
    	exit 1
    fi

    echo "### build $dirname"

    ./configure --prefix=$BUILD_INSTALL --sysconfdir=/tmp/xnedit/etc --localstatedir=/tmp/xnedit/var 
    if [ $? -ne 0 ]; then
    	echo "abort"
    	exit 1
    fi

    make
	if [ $? -ne 0 ]; then
    	echo "abort"
    	exit 1
    fi

    make install
    if [ $? -ne 0 ]; then
    	echo "abort"
    	exit 1
    fi

    echo
    echo "## leave $BUILD_DIR/$dirname"
    cd ..
done


echo
echo "build motif"
echo "------------------------------------------------------"
echo

curl -L -O $src_motif
dlfile=$(basename $src_motif)
dirname=${dlfile%.tar.*}
tar xvfz $dlfile

cd $dirname

# apply patches
echo "## apply patches"
for file in ../../patches/*; do
	patch -p1 < $file
done
echo "## build"


# compile motif

./configure --prefix=$BUILD_INSTALL
if [ $? -ne 0 ]; then
	echo "abort"
	exit 1
fi

make
if [ $? -ne 0 ]; then
	echo "abort"
	exit 1
fi

make install
if [ $? -ne 0 ]; then
	echo "abort"
	exit 1
fi

echo
echo "update dylib IDs"
echo "------------------------------------------------------"
echo

for file in $(find $LIB_DIR -path "*.dylib" -type f); do
	id=$(otool -L $file | sed '2q;d' | awk '{print $1}')
	newid=${id/$LIB_DIR/@rpath}
	echo install_name_tool -id $newid $file
	install_name_tool -id $newid $file
	if [ $? -ne 0 ]; then
    	echo "abort"
    	exit 1
    fi

	for dylib in $(otool -L $file | sed 1,2d | awk '{print $1}' | grep $LIB_DIR); do
		echo install_name_tool -change $dylib ${dylib/$LIB_DIR/@rpath} $file
		install_name_tool -change $dylib ${dylib/$LIB_DIR/@rpath} $file
		if [ $? -ne 0 ]; then
	    	echo "abort"
	    	exit 1
	    fi
	done
done

