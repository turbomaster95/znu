#!/usr/bin/env bash
set -e

SRCTREE=$1
MFORMAT=$2
MCOPY=$3
MMD=$4

mkdir -p "$SRCTREE/uki"

rm -f "$SRCTREE/uki/ramdisk.img"
truncate -s 16M "$SRCTREE/uki/ramdisk.img"
$MFORMAT  -i "$SRCTREE/uki/ramdisk.img" -F -v "ZNUBOOT" ::

# Directory layout
$MMD -i "$SRCTREE/uki/ramdisk.img" ::/boot
$MMD -i "$SRCTREE/uki/ramdisk.img" ::/EFI
$MMD -i "$SRCTREE/uki/ramdisk.img" ::/EFI/BOOT

# limine.conf in root AND EFI/BOOT — Limine scans both locations
$MCOPY -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/configs/limine.conf" ::/limine.conf
$MCOPY -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/configs/limine.conf" ::/EFI/BOOT/limine.conf

# Kernel and initrd
$MCOPY -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/znu"                                          ::/boot/kernel.bin
$MCOPY -i "$SRCTREE/uki/ramdisk.img" \
    "$SRCTREE/configs/iso_root/boot/initramfs.cpio"         ::/boot/initramfs.cpio

echo "  RAMDISK uki/ramdisk.img generated"

pushd "$SRCTREE/uki" > /dev/null
nim c --out:"$SRCTREE/uki/Znu.efi" wrapper.nim 2>&1
popd > /dev/null

echo "  MKUKI   uki/Znu.efi built successfully"
