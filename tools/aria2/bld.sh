#!/bin/sh
set -e
if [ ! -f "$TOOLDIR/bin/znaria2c" ]; then
    download_src "https://github.com/aria2/aria2/releases/download/release-1.37.0/aria2-1.37.0.tar.xz"
    [ -z "$FETCH_ONLY" ] || return 0

    cd "$DL_DIR"
    [ ! -d "aria2-1.37.0" ] && tar -xf aria2-1.37.0.tar.xz
    
    cd aria2-1.37.0
    
    ./configure --prefix="$TOOLDIR" \
                --disable-nls \
                --disable-dependency-tracking
                
    zngmake -j"$JOBS"
    zngmake install
    
    mv "$TOOLDIR/bin/aria2c" "$TOOLDIR/bin/znaria2c"
fi
