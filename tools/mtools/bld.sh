#!/bin/sh
set -e

if [ ! -f "$TOOLDIR/bin/znmformat" ] || [ ! -f "$TOOLDIR/bin/znmcopy" ] || [ ! -f "$TOOLDIR/bin/znmmd" ]; then
    if [ "$FETCH_ONLY" = "yes" ]; then
        download_src "https://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.gz"
        exit 0
    fi

    cd "$DL_DIR"
    rm -rf mtools-4.0.49
    tar -zxf "mtools-4.0.49.tar.gz"
    cd mtools-4.0.49

    patch -p1 -f < "$TOOLS_DIR/mtools/shim.patch"

    sed -i 's/strtonum.o @FLOPPYD_IO_OBJ@ @XDF_IO_OBJ@/strtonum.o android_shim.o @FLOPPYD_IO_OBJ@ @XDF_IO_OBJ@/' Makefile.in

    ./configure $ZCONFLAGS

    zngmake -j"$JOBS"
    zngmake install

    # Prefix tools to prevent collision
    for cmd in mformat mcopy mmd; do
        mv "$TOOLDIR/bin/$cmd" "$TOOLDIR/bin/zn$cmd"
    done
fi
