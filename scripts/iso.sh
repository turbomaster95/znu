#!/bin/bash
set -e

# Compile kernel
# (cd src && bash build.sh) || true

# Strip things down
objcopy --strip-all --strip-unneeded --strip-debug --strip-all-gnu oxus ox

# Copy kernel to ISO root
cp ox configs/iso_root/boot/kernel.bin

# Build bootable ISO
xorriso -as mkisofs \
  -b boot/limine/limine-bios-cd.bin \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  -R -o image.iso configs/iso_root

# Install Limine BIOS stages
scripts/limine/bin/limine bios-install image.iso

echo "ISO built: image.iso"
