#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/lib/libmpfr.la" ]; then
    download_src "https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.1.tar.xz"
    [ -z "$FETCH_ONLY" ] || return 0

    cd "$DL_DIR"
    [ ! -d "mpfr-4.2.1" ] && tar -xf mpfr-4.2.1.tar.xz

    cd mpfr-4.2.1

    export CFLAGS="-O2 -fPIC"

    ./configure $ZCONFLAGS \
                --with-gmp="$TOOLDIR" \
                --enable-static \
                --disable-shared \

    zngmake -j"$JOBS"
    zngmake install
fi

