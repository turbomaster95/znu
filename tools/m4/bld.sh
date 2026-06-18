#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znm4" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    tar -zxf "m4-1.4.19.tar.gz"
    cd m4-1.4.19

    ./configure $ZCONFLAGS --prefix="$TOOLDIR" CFLAGS="-Wno-attributes -std=gnu11"

    zngmake -j"$JOBS" CFLAGS="-Wno-attributes -std=gnu11 -Wno-int-conversion"
    zngmake install

    mv "$TOOLDIR/bin/m4" "$TOOLDIR/bin/znm4"
    ln -sf "znm4" "$TOOLDIR/bin/m4"
fi
