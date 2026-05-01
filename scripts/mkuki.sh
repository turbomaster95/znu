#!/usr/bin/env bash
set -e

SRCTREE=$1

mkdir -p "$SRCTREE/uki"

# 1. Generate ramdisk.img
rm -f "$SRCTREE/uki/ramdisk.img"
truncate -s 16M "$SRCTREE/uki/ramdisk.img"

# Format as FAT32
mformat -i "$SRCTREE/uki/ramdisk.img" -F -v "UKI_RAMDISK" ::

# Create directories
mmd -i "$SRCTREE/uki/ramdisk.img" ::/boot

# Copy files
mcopy -i "$SRCTREE/uki/ramdisk.img" "$SRCTREE/znu" ::/boot/kernel.bin
mcopy -i "$SRCTREE/uki/ramdisk.img" "$SRCTREE/configs/iso_root/boot/initramfs.cpio" ::/boot/initramfs.cpio

echo "  RAMDISK uki/ramdisk.img generated"

# 2. Build the UKI wrapper
pushd "$SRCTREE/uki" > /dev/null
nim c --out:$SRCTREE/uki/Znu.efi wrapper.nim &> /dev/null
popd > /dev/null

echo "  MKUKI   uki/Znu.efi built successfully"
