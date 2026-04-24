#!/bin/bash

# 1. Environment & Path Logic
# $1 is passed as $(srctree) from the Makefile
SRCTREE=$1
if [ -z "$SRCTREE" ]; then
    SRCTREE="."
fi

# In Kbuild, we execute from the OBJTREE (the build folder)
OBJTREE=$PWD
CONF="include/config/auto.conf"

if [ -f "$CONF" ]; then
    # Parse CONFIG_CROSS_COMPILE and strip quotes
    CROSS_COMPILE=$(grep "CONFIG_CROSS_COMPILE=" "$CONF" | cut -d'=' -f2 | tr -d '"')
fi

CC="${CROSS_COMPILE}gcc"
LD="${CROSS_COMPILE}ld"

if [ -z "$(command -v "$CC")" ]; then
    echo "Error: Compiler '$CC' not found in PATH."
    exit 1
fi

# 2. Feature Test (Performed in current build directory)
TMP_C="lib/tmp_feature_test.c"
TMP_O="lib/tmp_feature_test.o"

cat << 'EOF' > "$TMP_C"
#ifndef __x86_64__
#error "Target is not x86_64"
#endif
void _start(void) {}
EOF

if ! $CC -c "$TMP_C" -o "$TMP_O" -ffreestanding -nostdinc >/dev/null 2>&1; then
    echo "  ERROR: Compiler $CC failed x86_64 check."
    rm -f "$TMP_C" "$TMP_O"
    exit 1
fi
rm -f "$TMP_C" "$TMP_O"

# 3. Directory Definitions
# Resolve absolute path for SRC to prevent find errors
PROJECT_ROOT=$(realpath "$SRCTREE")
SRC=$(realpath "$SRCTREE/lib/uacpi/source" 2>/dev/null)
INC="-I$SRCTREE/lib/uacpi/include -I$SRCTREE/lib/libc/include"
INTERNAL_INC=$($CC -print-file-name=include)
OUT="lib/uacpi/uacpi_out"
FINAL_OBJ="$OBJTREE/lib/uacpi_lib.o"

mkdir -p "$OUT"

# 4. Find core files in the Source Tree
if [ -z "$SRC" ] || [ ! -d "$SRC" ]; then
    echo "  ERROR: Source directory ($SRCTREE/lib/uacpi/source) not found."
    echo "  DEBUG: Current PWD: $PWD"
    exit 1
fi

# Use find on the absolute path
FILES=$(find "$SRC" -maxdepth 3 -name "*.c")

# 5. Compile loop with Skip Logic
OBJ_LIST=""
NEW_FILES_COMPILED=false

for f in $FILES; do
    DISPATH="${f#$PROJECT_ROOT/}"
    # Create unique object name by converting path separators to underscores
    obj_name=$(echo "$f" | tr '/' '_').o
    obj_path="$OUT/$obj_name"
    OBJ_LIST="$OBJ_LIST $obj_path"

    # FEATURE: Skip if individual .o already exists
    if [ -f "$obj_path" ]; then
        continue
    fi

    echo "  ACC     $DISPATH"
    if $CC -c "$f" -o "$obj_path" $INC -isystem "$INTERNAL_INC" \
        -U__linux__ -w -U__unix__ \
        -include "$SRCTREE/lib/libc/include/string.h" \
        -include "$SRCTREE/lib/libc/include/ctype.h" \
        -D_GNU_EFI -D_EFI64 -DACPI_MACHINE_WIDTH=64 \
        -DACPI_USE_SYSTEM_CLIBRARY=0 \
        -DACPI_USE_STANDARD_HEADERS=0 \
        -ffreestanding -nostdinc -fno-stack-protector -mcmodel=kernel -mno-red-zone; then
        NEW_FILES_COMPILED=true
    else
        echo "  ERROR: Failed to compile $f"
        exit 1
    fi
done

# 6. Final Link Logic
if [ -z "$OBJ_LIST" ]; then
    echo "  ERROR: No input files found for linking in $SRC"
    exit 1
fi


FINAL_DISPATH="${FINAL_OBJ#$PROJECT_ROOT/}"

if [ ! -f "$FINAL_OBJ" ] || [ "$NEW_FILES_COMPILED" = true ]; then
    echo "  LD      $FINAL_DISPATH"
    $LD -r $OBJ_LIST -o "$FINAL_OBJ"
else
    echo "  ACPI    $FINAL_DISPATH is up to date."
fi
