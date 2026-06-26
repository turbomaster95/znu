#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/zncpio" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://ftp.gnu.org/gnu/cpio/cpio-2.15.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    tar -zxf "cpio-2.15.tar.gz"
    cd cpio-2.15

    znpatch -p1 < "$TOOLS_DIR/cpio/gcc15.patch"

    ./configure $ZCONFLAGS --prefix="$TOOLDIR" \
		--enable-largefile \
		--disable-mt \
		--disable-rpath \
		--disable-nls

    zngmake -j"$JOBS"
    zngmake install

    if [ -f "$TOOLDIR/bin/cpio" ]; then
        mv "$TOOLDIR/bin/cpio" "$TOOLDIR/bin/zncpio"
    fi
fi
