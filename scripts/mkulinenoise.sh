#!/bin/bash

# 1. Environment & Path Logic
SRCTREE=$1
if [ -z "$SRCTREE" ]; then
    SRCTREE="."
fi

OBJTREE=$PWD
CONF="include/config/auto.conf"

if [ -f "$CONF" ]; then
    CROSS_COMPILE=$(grep "CONFIG_CROSS_COMPILE=" "$CONF" | cut -d'=' -f2 | tr -d '"')
fi

CC="${CROSS_COMPILE}gcc"
LD="${CROSS_COMPILE}ld"

PROJECT_ROOT=$(realpath "$SRCTREE")
ISOCLINE_SRC="$PROJECT_ROOT/lib/ulibc/third-party/linenoise/linenoise.c"
INC="-I$SRCTREE/lib/ulibc/include -I$SRCTREE/lib/ulibc/third-party/linenoise"
INTERNAL_INC=$($CC -print-file-name=include)
FINAL_OBJ="$OBJTREE/lib/ulibc/third-party/linenoise/linenoise_lib.o"

if [ ! -f "$ISOCLINE_SRC" ]; then
    echo "  ERROR: linenoise.c not found at $ISOCLINE_SRC"
    exit 1
fi

DISPATH="${ISOCLINE_SRC#$PROJECT_ROOT/}"
FINAL_DISPATH="${FINAL_OBJ#$PROJECT_ROOT/}"

echo "  CC      $DISPATH"
if $CC -c "$ISOCLINE_SRC" -o "$FINAL_OBJ" $INC -isystem "$INTERNAL_INC" \
    -ffreestanding -nostdinc -fno-stack-protector \
    -DSIZE_MAX=0xFFFFFFFFFFFFFFFF \
    -Datexit\(x\)=0 \
    -Dsscanf=sscanf \
    -D_POSIX_C_SOURCE=200809L \
    -w \
    -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2; then
    
    echo "  DONE    $FINAL_DISPATH"
else
    echo "  ERROR: Failed to compile $ISOCLINE_SRC"
    exit 1
fi

#    -I"$SRCTREE/lib/libc/include/" 

