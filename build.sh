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

Usage: build.sh [-hu] [-j njob] [-T tools] operation [...]

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

OPERATIONS="$*"
if [ "$OPERATIONS" = "help" ] || [ -z "$OPERATIONS" ]; then
    show_help
    exit 1
fi

if [ -n "$TOOLDIR_OVERRIDE" ]; then
    export TOOLDIR="$TOOLDIR_OVERRIDE"
else
    export TOOLDIR="$TOP_DIR/tools/obj/tooldir"
fi
export DL_DIR="$TOP_DIR/tools/obj/downloads"
export MFLAGS="INREPO=yes LZ4=znlz4 NASM=znnasm"

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
    fi
}

for op in $OPERATIONS; do
    case "$op" in
        clean)
            if [ -x "$TOOLDIR/bin/zngmake" ]; then
                zngmake $MFLAGS clean
            else
                rm -rf "$TOP_DIR/tools/obj"
            fi
            ;;
        tools)
            sh "$DIRTOOL/tool.sh"
            ;;
        kernel)
            execute_tools
            zngmake $MFLAGS -j"$JOBS"
            ;;
        build)
            execute_tools
            zngmake $MFLAGS -j"$JOBS"
            ;;
        *)
            echo "Unknown operation: $op" >&2
            exit 1
            ;;
    esac
done

echo "===> build.sh ended:      $(date)"
