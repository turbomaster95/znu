#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znxorriso" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://ftp.gnu.org/gnu/xorriso/xorriso-1.5.6.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    tar -zxf "xorriso-1.5.6.tar.gz"
    cd xorriso-1.5.6

    sed -i 's/wait3(NULL,WNOHANG,NULL)/waitpid(-1,NULL,WNOHANG)/g' xorriso/parse_exec.c
    sed -i '1s/^/#include <sys\/types.h>\n/' libisofs/rockridge.h

    touch xorriso/*.info

    ./configure --prefix="$TOOLDIR" $ZCONFLAGS \
        --disable-nls \
        --disable-libacl \
        --disable-xattr

    zngmake -j"$JOBS"
    zngmake install

    mv "$TOOLDIR/bin/xorriso" "$TOOLDIR/bin/znxorriso"
fi

