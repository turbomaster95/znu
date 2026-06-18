#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znflex" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    tar -zxf "flex-2.6.4.tar.gz"
    cd flex-2.6.4

    ./configure $ZCONFLAGS --prefix="$TOOLDIR"
    zngmake -j"$JOBS"
    zngmake install

    mv "$TOOLDIR/bin/flex" "$TOOLDIR/bin/znflex"
    ln -sf "znflex" "$TOOLDIR/bin/flex"
fi
