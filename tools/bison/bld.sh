#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znbison" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    tar -zxf "bison-3.8.2.tar.gz"
    cd bison-3.8.2

    ./configure $ZCONFLAGS --prefix="$TOOLDIR" \
              ac_cv_func_ffsl=yes ac_cv_func_ffsll=yes

    zngmake -j"$JOBS"
    zngmake install

    mv "$TOOLDIR/bin/bison" "$TOOLDIR/bin/znbison"
    ln -sf "znbison" "$TOOLDIR/bin/bison"
    ln -sf "znbison" "$TOOLDIR/bin/yacc"
fi
