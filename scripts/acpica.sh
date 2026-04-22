#!/bin/bash

CONF="include/config/auto.conf"

if [ -f "$CONF" ]; then
    # Parse CONFIG_CROSS_COMPILE and strip quotes
    CROSS_COMPILE=$(grep "CONFIG_CROSS_COMPILE=" "$CONF" | cut -d'=' -f2 | tr -d '"')
fi

# If not found in config, fallback to environment or empty
CC="${CROSS_COMPILE}gcc"
LD="${CROSS_COMPILE}ld"

# Final safety check: if CC is just "gcc", make sure it's actually what we want
if [ -z "$(command -v "$CC")" ]; then
    echo "Error: Compiler '$CC' not found in PATH."
    exit 1
fi

TMP_C="lib/tmp_feature_test.c"
TMP_O="lib/tmp_feature_test.o"

cat << 'EOF' > "$TMP_C"
#ifndef __x86_64__
#error "Target is not x86_64"
#endif
void _start(void) {}
EOF

# Try to compile. Redirect stderr to /dev/null so it stays 'quiet' unless it fails.
if ! $CC -c "$TMP_C" -o "$TMP_O" -ffreestanding -nostdinc >/dev/null 2>&1; then
    echo "  ERROR: Compiler $CC failed x86_64 check."
    rm -f "$TMP_C" "$TMP_O"
    exit 1
fi

# Cleanup the test files
rm -f "$TMP_C" "$TMP_O"

SRC="lib/acpica/source/components"
INC="-Ilib/acpica/source/include -Ilib/libc/include"
INTERNAL_INC=$($CC -print-file-name=include)
OUT="lib/acpica_out"
FINAL_OBJ="lib/acpica_lib.o"

mkdir -p $OUT

# 1. Find core files
FILES=$(find $SRC -maxdepth 2 -not -path '*/debugger*' -not -path '*/disassembler*' -name "*.c" | grep -vE "db|dump|trace|help|os")

# 2. Compile loop
OBJ_LIST=""
for f in $FILES; do
    obj_name=$(echo $f | tr '/' '_').o
    obj_path="$OUT/$obj_name"
    OBJ_LIST="$OBJ_LIST $obj_path"

    # Skip ONLY if the file exists. 
    # This ignores timestamps (won't recompile even if you edit the .c)
    if [ -f "$obj_path" ]; then
        continue
    fi

    echo "  ACC     $f"
    $CC -c $f -o "$obj_path" $INC -isystem "$INTERNAL_INC" \
    -U__linux__ -w -U__unix__ -include lib/libc/include/string.h -include lib/libc/include/ctype.h \
    -D_GNU_EFI -D_EFI64 -DACPI_MACHINE_WIDTH=64 \
    -DACPI_USE_SYSTEM_CLIBRARY=0 \
    -DACPI_USE_STANDARD_HEADERS=0 \
    -ffreestanding -nostdinc -fno-stack-protector -mcmodel=kernel -mno-red-zone
done

# 3. Final Link check
# We link if the final library is missing.
if [ ! -f "$FINAL_OBJ" ]; then
    echo "  LD      $FINAL_OBJ"
    $LD -r $OBJ_LIST -o "$FINAL_OBJ"
else
    echo "  ACPI    $FINAL_OBJ already exists, skipping link."
fi

