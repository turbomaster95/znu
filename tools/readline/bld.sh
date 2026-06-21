#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/lib/libreadline.a" ]; then
    download_src "https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz"
    [ -z "$FETCH_ONLY" ] || return 0

    cd "$DL_DIR"
    [ ! -d "readline-8.2" ] && tar -xzf readline-8.2.tar.gz

    cd readline-8.2

    export CFLAGS="-O2"

    ./configure --prefix="$TOOLDIR" \
                --enable-static \
                --disable-shared \
                $ZCONFLAGS

    zngmake -j"$JOBS"
    zngmake install
fi

