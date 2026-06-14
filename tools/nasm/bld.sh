#!/bin/sh
set -e
if [ ! -f "$TOOLDIR/bin/znnasm" ]; then
    download_src "https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz"
    [ -z "$FETCH_ONLY" ] || return 0
    
    cd "$DL_DIR"
    [ ! -d "nasm-2.16.03" ] && tar -xf nasm-2.16.03.tar.xz
    
    cd nasm-2.16.03
    ./configure --prefix="$TOOLDIR"
    zngmake -j"$JOBS" && zngmake install
    mv "$TOOLDIR/bin/nasm" "$TOOLDIR/bin/znnasm"
    mv "$TOOLDIR/bin/ndisasm" "$TOOLDIR/bin/znndisasm"
fi
