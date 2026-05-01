#!/usr/bin/env bash
set -e

GNU_EFI_DIR="$(realpath $1)/scripts/gnu-efi"
BIN_DIR="$GNU_EFI_DIR/x86_64"

# These are the files we verified in your 'ls' and 'tree' output
REQUIRED_FILES=(
    "gnuefi/libgnuefi.a"
    "gnuefi/crt0-efi-x86_64.o"
    "lib/libefi.a"
)

# 1. THE SHORT-CIRCUIT: Check if everything is already built
ALL_PRESENT=true
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$BIN_DIR/$file" ]; then
        ALL_PRESENT=false
        break
    fi
done

if [ "$ALL_PRESENT" = true ]; then
    # Skip silently or with a quick note to stderr
    echo "  CHECK   gnu-efi (already built)" >&2
fi

# Use CROSS_COMPILE if set, otherwise default to x86_64-elf-
PREFIX=${CROSS_COMPILE:-x86_64-elf-}

pushd "$GNU_EFI_DIR" > /dev/null
    
# -z norelro fixes the ld.lld strictness we saw earlier
echo "about to make"
echo PATH="$PATH" CROSS_COMPILE="$CROSS_COMPILE" ARCH=x86_64 CC="${PREFIX}gcc" AS="${PREFIX}as" LD="${PREFIX}ld" OBJCOPY="${PREFIX}objcopy" LDFLAGS="-z norelro"
env -i PATH="$PATH" CROSS_COMPILE="$CROSS_COMPILE" make ARCH=x86_64 CC="${PREFIX}gcc" AS="${PREFIX}as" LD="${PREFIX}ld" OBJCOPY="${PREFIX}objcopy" LDFLAGS="-z norelro" -j$(nproc 2>/dev/null || echo 4)
popd > /dev/null
