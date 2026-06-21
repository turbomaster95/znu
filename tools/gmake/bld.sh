#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/zngmake" ]; then
    download_src "https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz"
    [ -z "$FETCH_ONLY" ] || return 0
    
    cd "$DL_DIR"
    [ ! -d "make-4.4.1" ] && tar -xzf make-4.4.1.tar.gz
    
    cd make-4.4.1

    patch -p1 -f < "$TOOLS_DIR/gmake/znu.patch"

    export CFLAGS="-std=gnu17 -O2"

    ./configure --prefix="$TOOLDIR" $ZCONFLAGS

    sh ./build.sh
    cp ./make "$TOOLDIR/bin/zngmake"
fi
