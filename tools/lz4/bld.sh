#!/bin/sh
set -e
if [ ! -f "$TOOLDIR/bin/znlz4" ]; then
    download_src "https://github.com/lz4/lz4/archive/refs/tags/v1.9.4.tar.gz"
    [ -z "$FETCH_ONLY" ] || return 0
    
    cd "$DL_DIR"
    [ ! -d "lz4-1.9.4" ] && tar -xzf v1.9.4.tar.gz
    
    cd lz4-1.9.4
    zngmake -j"$JOBS"
    zngmake PREFIX="$TOOLDIR" install
    mv "$TOOLDIR/bin/lz4" "$TOOLDIR/bin/znlz4"
fi
