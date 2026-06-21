#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/zngawk" ]; then
    download_src "https://ftp.gnu.org/gnu/gawk/gawk-5.3.0.tar.gz"
    [ -z "$FETCH_ONLY" ] || return 0

    cd "$DL_DIR"
    [ ! -d "gawk-5.3.0" ] && tar -xzf gawk-5.3.0.tar.gz

    cd gawk-5.3.0

    export CFLAGS="-std=gnu17 -O2 -I$TOOLDIR/include"
    export LDFLAGS="-L$TOOLDIR/lib"

    ./configure --prefix="$TOOLDIR" \
                --with-readline="$TOOLDIR" \
                --with-mpfr="$TOOLDIR" --disable-extensions \
		--disable-pma \
                $ZCONFLAGS

    zngmake -j"$JOBS"
    zngmake install

    if [ -f "$TOOLDIR/bin/gawk" ]; then
        mv "$TOOLDIR/bin/gawk" "$TOOLDIR/bin/zngawk"
    fi

    rm -f "$TOOLDIR/bin/awk" "$TOOLDIR/bin/gawk-5.3.0"
fi

