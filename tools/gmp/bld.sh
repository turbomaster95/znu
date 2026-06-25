#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/lib/libgmp.so" ]; then
    download_src "https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz"
    [ -z "$FETCH_ONLY" ] || return 0

    cd "$DL_DIR"
    [ ! -d "gmp-6.3.0" ] && tar -xf gmp-6.3.0.tar.xz

    cd gmp-6.3.0

    export CFLAGS="-O2 -fPIC"

    # M4 environment variable ensures it grabs the compiled m4 binary
    export M4="$TOOLDIR/bin/znm4"

    ./configure $ZCONFLAGS \
                --enable-static \
                --disable-shared

    zngmake -j"$JOBS"
    zngmake install
fi

