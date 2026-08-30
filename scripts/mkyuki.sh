#!/usr/bin/env bash
set -e

SRCTREE=$1
MFORMAT=$2
MCOPY=$3
MMD=$4

mkdir -p "$SRCTREE/yuki"

rm -f "$SRCTREE/yuki/ramdisk.img"
truncate -s 16M "$SRCTREE/yuki/ramdisk.img"
$MFORMAT  -i "$SRCTREE/yuki/ramdisk.img" -F -v "ZNUBOOT" ::

# Directory layout
$MMD -i "$SRCTREE/yuki/ramdisk.img" ::/boot
$MMD -i "$SRCTREE/yuki/ramdisk.img" ::/EFI
$MMD -i "$SRCTREE/yuki/ramdisk.img" ::/EFI/BOOT

# limine.conf in root AND EFI/BOOT — Limine scans both locations
$MCOPY -i "$SRCTREE/yuki/ramdisk.img" \
    "$SRCTREE/configs/limine.conf" ::/limine.conf
$MCOPY -i "$SRCTREE/yuki/ramdisk.img" \
    "$SRCTREE/configs/limine.conf" ::/EFI/BOOT/limine.conf

# Kernel and initrd
$MCOPY -i "$SRCTREE/yuki/ramdisk.img" \
    "$SRCTREE/znu"                                          ::/boot/kernel.bin
$MCOPY -i "$SRCTREE/yuki/ramdisk.img" \
    "$SRCTREE/configs/iso_root/boot/initramfs.cpio"         ::/boot/initramfs.cpio

echo "  RAMDISK yuki/ramdisk.img generated"

pushd "$SRCTREE/yuki" > /dev/null
nim c --out:"$SRCTREE/yuki/Znu.efi" wrapper.nim 2>&1
popd > /dev/null

echo "  MKYUKI  yuki/Znu.efi built successfully"
