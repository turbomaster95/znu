#!/usr/bin/env bash
set -e

SRCTREE=$1
IMG_NAME=$2 # Base name passed from Makefile (e.g., "Znu")

if [ -z "$SRCTREE" ] || [ -z "$IMG_NAME" ]; then
    echo "Usage: $0 <srctree> <img_name>"
    exit 1
fi

ISO_ROOT="$SRCTREE/configs/iso_root"
# Ensure the kernel binary is staged inside the iso_root before running xorriso
cp -f znu "$ISO_ROOT/boot/kernel.bin"

# Fetch Limine's system boot sector path
LIMINE_SYS="$SRCTREE/scripts/limine/share/limine/limine-bios.sys"

build_xorriso() {
    local OUT_FILE=$1
    local MODE=$2 # "multi", "bios", "uefi"
    
    local OPTS=()

    # Base xorriso options for standard ISO generation
    OPTS+=("-as" "mkisofs")
    OPTS+=("-b" "boot/limine-bios-cd.bin") # Path inside iso_root or relative
    OPTS+=("-no-emul-boot")
    OPTS+=("-boot-load-size" "4")
    OPTS+=("-boot-info-table")

    # Handle BIOS/MBR Boot options
    if [ "$MODE" = "multi" ] || [ "$MODE" = "bios" ]; then
        # We assume limine-bios-cd.bin is copied or available in your staging root
        # If limine-bios-cd.bin isn't in your iso_root, make sure to stage it there first!
        if [ -f "$SRCTREE/scripts/limine/share/limine/limine-bios-cd.bin" ]; then
            mkdir -p "$ISO_ROOT/boot"
            cp -f "$SRCTREE/scripts/limine/share/limine/limine-bios-cd.bin" "$ISO_ROOT/boot/"
        fi
    fi

    # Handle UEFI Boot options via MBR El Torito Alt-Boot
    if [ "$MODE" = "multi" ] || [ "$MODE" = "uefi" ]; then
        OPTS+=("-eltorito-alt-boot")
        OPTS+=("-e" "EFI/BOOT/BOOTX64.EFI") # Point directly to the EFI executable inside ISO
        OPTS+=("-no-emul-boot")
    fi

    # Output details
    OPTS+=("-o" "$OUT_FILE")
    OPTS+=("$ISO_ROOT")

    # Run xorriso safely
    rm -f "$OUT_FILE"
    xorriso "${OPTS[@]}" 2>/dev/null

    # For BIOS or Multi targets, we must run the limine deployment post-processing step
    if [ "$MODE" = "multi" ] || [ "$MODE" = "bios" ]; then
        "$SRCTREE/scripts/limine/bin/limine" bios-install "$OUT_FILE"
    fi

    echo "ISO built successfully via xorriso: $OUT_FILE"
}

# Process paths depending on Makefile variables
if [ "$CONFIG_ISO_MULTI" = "y" ]; then
    build_xorriso "$IMG_NAME.iso" "multi"
fi

if [ "$CONFIG_ISO_BIOS" = "y" ]; then
    build_xorriso "$IMG_NAME.bios.iso" "bios"
fi

if [ "$CONFIG_ISO_UEFI" = "y" ]; then
    build_xorriso "$IMG_NAME.uefi.iso" "uefi"
fi

