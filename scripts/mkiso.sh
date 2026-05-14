#!/usr/bin/env bash
set -e

SRCTREE=$1
IMG_NAME=$2

if [ -z "$SRCTREE" ] || [ -z "$IMG_NAME" ]; then
    echo "Usage: $0 <srctree> <img_name>"
    exit 1
fi

# llvm-objcopy --strip-all --strip-unneeded --strip-debug znus znu

rm -f "$IMG_NAME"

truncate -s 128M "$IMG_NAME"

sgdisk -Z "$IMG_NAME"
sgdisk -n 1:2048:+1M -t 1:ef02 -c 1:"BIOS_BOOT" "$IMG_NAME"
sgdisk -n 2:4096:0 -t 2:ef00 -c 2:"ZNU_ESP" "$IMG_NAME"

mformat -i "$IMG_NAME"@@2M -F -v "ZNU_BOOT" ::

mcopy -o -i "$IMG_NAME"@@2M -s "$SRCTREE/configs/iso_root/"* ::/
mcopy -o -i "$IMG_NAME"@@2M -s znu ::/boot/kernel.bin

"$SRCTREE/scripts/limine/bin/limine" bios-install "$IMG_NAME"

echo "Image built: $IMG_NAME"
