#!/bin/env bash
set -e

# Use the first argument as the source tree path, default to current dir
SRCTREE="${1:-.}"
FLAGS="$2"
LIMINE_DIR="$SRCTREE/scripts/limine"

JOBS=$(echo "$FLAGS" | grep -oP '(?<=-j)\d+' || echo "")

if [ -z "$JOBS" ]; then
    # If -j was passed without a number (infinite jobs)
    # or not passed at all, check for the standalone '-j' flag
    if [[ "$FLAGS" == *"-j"* ]]; then
        JOBS="" # Let make handle it
    else
        JOBS="1" # Default to single-threaded if not specified
    fi
fi

# 1. Check if the submodule is actually there
if [ ! -f "$LIMINE_DIR/bootstrap" ]; then
    echo "Limine submodule not found at $LIMINE_DIR."
    echo "Attempting to initialize..."
    git submodule update --init --recursive
fi

# 2. Build Limine
# We use -C to run make inside the directory
echo "Building Limine binaries..."
pushd "$LIMINE_DIR"
unset LDFLAGS
unset CFLAGS
if [ ! -f "$LIMINE_DIR/Makefile" ]; then
    ./bootstrap
    ./configure --enable-bios --enable-bios-cd --enable-bios-pxe \
        --host=x86_64-elf CC="gcc" \
        TOOLCHAIN_FOR_TARGET="x86_64-elf-" \
        LD_FOR_TARGET="x86_64-elf-ld"
fi

if [ ! -f "$LIMINE_DIR/bin/limine-bios-cd.bin" ]; then
    if [ -n "$JOBS" ]; then
        make -j"$JOBS" > /dev/null
    else
        make -j > /dev/null
    fi
else
    echo "Limine already compiled, skipping build."
fi
popd

# 3. Verify the build produced the necessary BIOS binary
if [ ! -f "$LIMINE_DIR/bin/limine-bios-cd.bin" ]; then
    echo "Error: Limine build failed to produce BIOS binaries."
    exit 1
fi
