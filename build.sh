#!/bin/sh
set -e

export TOP_DIR="$(cd "$(dirname "$0")" && pwd)"
export DIRTOOL="$TOP_DIR/tools"
export TOBJDIR="$TOP_DIR/tools/obj"

export JOBS=5
TOOLDIR_OVERRIDE=""
UPDATE_MODE=false

show_help() {
    cat << EOF
build.sh: Missing operation to perform.

Usage: build.sh [-hu] [-j njob] [-T tools] operation [MAKE_ARGS...]

Build operations (all imply "tools"):
    build               Run tools baseline and compile kernel.
    kernel              Build the Znu kernel.
    tools               Build and install host compilation tools.
    clean               Clean build artifacts.

Options:
    -h             Print this help message.
    -j njob        Run up to njob jobs in parallel.
    -T tools       Override target output TOOLDIR layout path.
    -u             Skip clean routines before execution (MKUPDATE=yes).
EOF
}

while getopts "a:hj:m:N:T:u" opt; do
    case "$opt" in
        h) show_help; exit 0 ;;
        j) export JOBS="$OPTARG" ;;
        T) TOOLDIR_OVERRIDE="$OPTARG" ;;
        u) UPDATE_MODE=true ;;
        *) show_help; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

OPERATIONS=""
MAKE_ARGS=""

for arg in "$@"; do
    case "$arg" in
        build|kernel|tools|clean)
            OPERATIONS="$OPERATIONS $arg"
            ;;
        *)
            # Anything else (like V=1, -k, etc.) gets forwarded to make
            MAKE_ARGS="$MAKE_ARGS $arg"
            ;;
    esac
done

if [ -z "$OPERATIONS" ]; then
    show_help
    exit 1
fi

if [ -n "$TOOLDIR_OVERRIDE" ]; then
    export TOOLDIR="$TOOLDIR_OVERRIDE"
else
    export TOOLDIR="$TOP_DIR/tools/obj/tooldir"
fi
export DL_DIR="$TOP_DIR/tools/obj/downloads"
export MFLAGS="LZ4=znlz4 NASM=znnasm XORRISO=znxorriso MCOPY=znmcopy MFORMAT=znmformat MMD=znmmd CPIO=zncpio FLEX=znflex BISON=znbison M4=znm4 AWK=zngawk COMPAR=znar"

echo "===> build.sh command:    $0 $*"
echo "===> build.sh started:    $(date)"
echo "===> Znu version:         0.0.1"
echo "===> Build platform:      $(uname -s) $(uname -r) $(uname -m)"
echo "===> HOST_SH:             ${SHELL:-/bin/sh}"

mkdir -p "$TOOLDIR/bin" "$DL_DIR"
[ -f "$DIRTOOL/env.sh" ] && . "$DIRTOOL/env.sh"

execute_tools() {
    if [ "$UPDATE_MODE" = "false" ]; then
        rm -rf "$TOP_DIR/tools/obj/build" 2>/dev/null || true
    fi
    if [ ! -x "$TOOLDIR/bin/zngmake" ]; then
        echo "===> No \$TOOLDIR/bin/zngmake, needs building."
        sh "$DIRTOOL/tool.sh"
        return 0
    fi
    sh "$DIRTOOL/tool.sh"
}

for op in $OPERATIONS; do
    case "$op" in
        clean)
            if [ -x "$TOOLDIR/bin/zngmake" ]; then
                zngmake $MFLAGS $MAKE_ARGS clean
            else
                rm -rf "$TOP_DIR/tools/obj"
            fi
            ;;
        tools)
            sh "$DIRTOOL/tool.sh"
            ;;
        kernel)
            execute_tools
            zngmake $MFLAGS $MAKE_ARGS -j"$JOBS"
            ;;
        build)
            execute_tools
            zngmake $MFLAGS $MAKE_ARGS -j"$JOBS"
            ;;
    esac
done

echo "===> build.sh ended:      $(date)"

