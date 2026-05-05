#!/usr/bin/env bash
set -e
SRCTREE=$1

mkdir -p "$SRCTREE/uki"

rm -f "$SRCTREE/uki/ramdisk.img"
truncate -s 16M "$SRCTREE/uki/ramdisk.img"
mformat  -i "$SRCTREE/uki/ramdisk.img" -F -v "ZNUBOOT" ::

# Directory layout
mmd -i "$SRCTREE/uki/ramdisk.img" ::/boot
mmd -i "$SRCTREE/uki/ramdisk.img" ::/EFI
mmd -i "$SRCTREE/uki/ramdisk.img" ::/EFI/BOOT

# limine.conf in root AND EFI/BOOT — Limine scans both locations
mcopy -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/configs/limine.conf" ::/limine.conf
mcopy -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/configs/limine.conf" ::/EFI/BOOT/limine.conf

# Kernel and initrd
mcopy -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/znu"                                          ::/boot/kernel.bin
mcopy -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/configs/iso_root/boot/initramfs.cpio"         ::/boot/initramfs.cpio

echo "  RAMDISK uki/ramdisk.img generated"

pushd "$SRCTREE/uki" > /dev/null
nim c --out:"$SRCTREE/uki/Znu.efi" wrapper.nim 2>&1
popd > /dev/null

echo "  MKUKI   uki/Znu.efi built successfully"
