#!/usr/bin/env bash
set -e

SRCTREE=$1
IMG_NAME=$2
XORRISO=$3

if [ -z "$SRCTREE" ] || [ -z "$IMG_NAME" ]; then
    echo "Usage: $0 <srctree> <img_name>"
    exit 1
fi

ISO_ROOT="$SRCTREE/configs/iso_root"
mkdir -p "$ISO_ROOT/boot"

# Pre-stage the kernel binary
cp -f znu "$ISO_ROOT/boot/kernel.bin"

build_xorriso() {
    local OUT_FILE=$1
    local MODE=$2

    # Array to track dynamic xorriso arguments
    local OPTS=("-as" "mkisofs")

    # --- BIOS SETUP ---
    if [ "$MODE" = "multi" ] || [ "$MODE" = "bios" ]; then
        # Check where limine-bios-cd.bin is located and copy it to ISO root boot folder
        if [ -f "$SRCTREE/scripts/limine/bin/limine-bios-cd.bin" ]; then
            cp -f "$SRCTREE/scripts/limine/bin/limine-bios-cd.bin" "$ISO_ROOT/boot/"
        else
            echo "[-] Error: limine-bios-cd.bin not found in scripts/limine!" >&2
            exit 1
        fi
        
        # Keep limine-bios.sys in root/boot if it's currently sitting in root/boot/limine
        if [ -f "$ISO_ROOT/boot/limine/limine-bios.sys" ] && [ ! -f "$ISO_ROOT/boot/limine-bios.sys" ]; then
            cp -f "$ISO_ROOT/boot/limine/limine-bios.sys" "$ISO_ROOT/boot/"
        fi

        OPTS+=("-b" "boot/limine-bios-cd.bin")
        OPTS+=("-no-emul-boot")
        OPTS+=("-boot-load-size" "4")
        OPTS+=("-boot-info-table")
    fi

    # --- UEFI SETUP ---
    if [ "$MODE" = "multi" ] || [ "$MODE" = "uefi" ]; then
        # Find BOOTX64.EFI from your limine build directory and stage it dynamically
        local EFI_SRC=""
        if [ -f "$SRCTREE/scripts/limine/share/limine/BOOTX64.EFI" ]; then
            EFI_SRC="$SRCTREE/scripts/limine/share/limine/BOOTX64.EFI"
        elif [ -f "$SRCTREE/scripts/limine/BOOTX64.EFI" ]; then
            EFI_SRC="$SRCTREE/scripts/limine/BOOTX64.EFI"
        fi

        if [ -n "$EFI_SRC" ]; then
            mkdir -p "$ISO_ROOT/EFI/BOOT"
            cp -f "$EFI_SRC" "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI"
            
            # Only add El Torito UEFI alt boot configuration if we actually have the binary file
            if [ "$MODE" = "multi" ]; then
                OPTS+=("-eltorito-alt-boot")
            fi
            OPTS+=("-e" "EFI/BOOT/BOOTX64.EFI")
            OPTS+=("-no-emul-boot")
        else
            # Fallback handling if building UEFI component but missing the payload binary
            if [ "$MODE" = "uefi" ]; then
                echo "[-] Error: BOOTX64.EFI missing. Cannot compile explicit UEFI ISO." >&2
                exit 1
            elif [ "$MODE" = "multi" ]; then
                echo "[!] Warning: BOOTX64.EFI missing. Compiling Multi-ISO variant downshifted to BIOS-only mode." >&2
            fi
        fi
    fi

    OPTS+=("-o" "$OUT_FILE")
    OPTS+=("$ISO_ROOT")

    rm -f "$OUT_FILE"
    
    # Run xorriso cleanly without suppressing errors so you can see warnings
    $XORRISO "${OPTS[@]}"

    # Run the structural post-install logic if BIOS execution layers exist
    if [ "$MODE" = "multi" ] || [ "$MODE" = "bios" ]; then
        "$SRCTREE/scripts/limine/bin/limine" bios-install "$OUT_FILE"
    fi

    echo "[+] ISO built successfully: $OUT_FILE"
}

if [ "$CONFIG_ISO_MULTI" = "y" ]; then
    build_xorriso "$IMG_NAME.iso" "multi"
fi

if [ "$CONFIG_ISO_BIOS" = "y" ]; then
    build_xorriso "$IMG_NAME.bios.iso" "bios"
fi

if [ "$CONFIG_ISO_UEFI" = "y" ]; then
    build_xorriso "$IMG_NAME.uefi.iso" "uefi"
fi

