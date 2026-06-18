#!/bin/sh
set -e

TOP_DIR="$(cd "$(dirname "$0")" && pwd)"
export TOOLDIR="$TOP_DIR/tools/obj/tooldir"
export DIRTOOL="$TOP_DIR/tools"
export DL_DIR="$TOP_DIR/tools/obj/downloads"
export JOBS=5
export MFLAGS="INREPO=yes LZ4=znlz4 NASM=znnasm"

mkdir -p "$TOOLDIR/bin" "$DL_DIR"
. "$DIRTOOL/env.sh"

case "$1" in
    tools)
        sh "$TOP_DIR/tools/tool.sh"
        ;;
    kernel)
        zngmake $MFLAGS -j"$JOBS"
        ;;
    clean)
        zngmake $MFLAGS clean
        ;;
    *)
        sh "$TOP_DIR/tools/tool.sh"
        zngmake $MFLAGS -j"$JOBS"
        ;;
esac
