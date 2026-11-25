#!/bin/bash

# 1. Environment & Path Logic
# $1 is passed as $(srctree) from the Makefile
SRCTREE=$1
if [ -z "$SRCTREE" ]; then
    SRCTREE="."
fi

ARCHFLAGS=$2

OBJTREE=$PWD
CONF="include/config/auto.conf"

if [ -f "$CONF" ]; then
    CROSS_COMPILE=$(grep "CONFIG_CROSS_COMPILE=" "$CONF" | cut -d'=' -f2 | tr -d '"')
fi

CC="${CROSS_COMPILE}gcc"
LD="${CROSS_COMPILE}ld"

if [ -z "$(command -v "$CC")" ]; then
    echo "Error: Compiler '$CC' not found in PATH."
    exit 1
fi

PROJECT_ROOT=$(realpath "$SRCTREE")
SRC=$(realpath "$SRCTREE/drivers/external/znet/core" 2>/dev/null)
znet_ROOT="$SRCTREE/drivers/external/znet"
INC="-I$SRCTREE/lib/libc/include -I$SRCTREE/include -I$SRCTREE/drivers/include -I$SRCTREE/drivers/external/include -I$SRCTREE/drivers/external/znet/include"
INTERNAL_INC=$($CC -print-file-name=include)
OUT="drivers/external/znet/.znet_out"
FINAL_OBJ="$OBJTREE/drivers/external/znet/znet_lib.o"

mkdir -p "$OUT"

# 2. Find core files
if [ -z "$SRC" ] || [ ! -d "$SRC" ]; then
    echo "  ERROR: Source directory ($SRCTREE/drivers/external/znet/core) not found."
    echo "  DEBUG: Current PWD: $PWD"
    exit 1
fi

FILES=$(find "$SRC" -maxdepth 3 -name "*.c")

# 3. Compile loop with Skip Logic
OBJ_LIST=""
NEW_FILES_COMPILED=false

for f in $FILES; do
    DISPATH="${f#$PROJECT_ROOT/}"
    obj_name=$(echo "$f" | tr '/' '_').o
    obj_path="$OUT/$obj_name"
    OBJ_LIST="$OBJ_LIST $obj_path"

    if [ -f "$obj_path" ]; then
        continue
    fi

    echo "  CC      $DISPATH"
    if $CC -c "$f" -o "$obj_path" $INC -isystem "$INTERNAL_INC" \
        -U__linux__ -w -U__unix__ \
        -include "$SRCTREE/lib/libc/include/string.h" \
        -include "$SRCTREE/lib/libc/include/ctype.h" \
        -D__is_libk \
        -ffreestanding -nostdinc -fno-stack-protector $ARCHFLAGS -fno-pic -fno-PIC -fno-PIE -fno-pie; then
        NEW_FILES_COMPILED=true
    else
        echo "  ERROR: Failed to compile $f"
        exit 1
    fi
done

# 4. Final Link
if [ -z "$OBJ_LIST" ]; then
    echo "  ERROR: No input files found for linking in $SRC"
    exit 1
fi

FINAL_DISPATH="${FINAL_OBJ#$PROJECT_ROOT/}"

if [ ! -f "$FINAL_OBJ" ] || [ "$NEW_FILES_COMPILED" = true ]; then
    echo "  LD      $FINAL_DISPATH"
    $LD -r $OBJ_LIST -o "$FINAL_OBJ"
else
    echo "  ZNET    $FINAL_DISPATH is up to date."
fi
