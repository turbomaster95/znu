#!/bin/bash
set -e

SRCTREE=$1
ISO_NAME=$2

if [ -z "$SRCTREE" ]; then
    echo "Usage: $0 <srctree> <iso_name>"
    exit 1
fi

llvm-objcopy --strip-all --strip-unneeded --strip-debug --strip-all-gnu oxus ox

cp ox "$SRCTREE/configs/iso_root/boot/kernel.bin"

xorriso -as mkisofs \
  -b boot/limine/limine-bios-cd.bin \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  -R -o "$ISO_NAME" "$SRCTREE/configs/iso_root"

# 4. Install Limine BIOS stages (Looking in SRCTREE for the binary)
"$SRCTREE/scripts/limine/bin/limine" bios-install "$ISO_NAME"

echo "ISO built: $ISO_NAME"

