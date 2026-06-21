#!/bin/sh
set -e

# Scan the matrix to check if any single tool is missing
NEED_BUILD=0
for tool in nm strip objcopy objdump readelf size strings ar ranlib; do
    if [ ! -f "$TOOLDIR/bin/zn$tool" ]; then
        NEED_BUILD=1
        break
    fi
done

if [ "$NEED_BUILD" -eq 1 ]; then
    download_src "https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.xz"
    [ -z "$FETCH_ONLY" ] || return 0

    cd "$DL_DIR"
    [ ! -d "binutils-2.42" ] && tar -xf binutils-2.42.tar.xz

    cd binutils-2.42

    export CFLAGS="-O2 -I$TOOLDIR/include"
    export LDFLAGS="-L$TOOLDIR/lib"

    ./configure --prefix="$TOOLDIR" \
                --target=x86_64-elf \
                --disable-gas \
                --disable-ld \
                --disable-gprof \
                --disable-gprofng \
                --disable-shared \
                --disable-nls \
                --disable-dependency-tracking \
                $ZCONFLAGS

    zngmake -j"$JOBS" all-binutils
    zngmake install-binutils

    cd "$TOOLDIR/bin"
    for tool in nm strip objcopy objdump readelf size strings ar ranlib; do
        if [ -f "x86_64-elf-$tool" ]; then
            mv "x86_64-elf-$tool" "zn$tool"
        fi
    done

    rm -f x86_64-elf-addr2line x86_64-elf-cxxfilt x86_64-elf-elfedit
fi

