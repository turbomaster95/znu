#!/usr/bin/env bash
set -e

SRCTREE="${1:-.}"
LIMINE_DIR="$SRCTREE/scripts/limine"
BIN_DIR="$LIMINE_DIR/bin"

# Define the absolute minimum files needed for Znu to boot BIOS/UEFI
REQUIRED_FILES=(
    "limine"
    "limine-bios.sys"
    "limine-bios-cd.bin"
    "BOOTX64.EFI"
)

# 1. THE SHORT-CIRCUIT: Check if all required files exist
ALL_PRESENT=true
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$BIN_DIR/$file" ]; then
        ALL_PRESENT=false
        break
    fi
done

if [ "$ALL_PRESENT" = true ]; then
    echo "Limine binaries already built. Skipping."
    exit 0
fi

# 2. Submodule check (only if we are missing files)
if [ ! -f "$LIMINE_DIR/bootstrap" ]; then
    echo "Limine source missing. Initializing submodule..."
    git submodule update --init --recursive
fi

# 3. Build logic
echo "Building Limine..."
pushd "$LIMINE_DIR" > /dev/null
    unset LDFLAGS CFLAGS
    
    # Check for Makefile to avoid redundant config
    if [ ! -f "Makefile" ]; then
        ./bootstrap
        ./configure --enable-bios --enable-bios-cd --enable-uefi-x86-64 --enable-uefi-cd CC="gcc"
    fi

    # Build and only show errors
    make -j$(nproc 2>/dev/null || echo 1)
popd > /dev/null

echo "Limine build complete."
