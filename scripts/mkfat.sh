#!/usr/bin/env bash

# Exit immediately if any command fails
set -e

SRCTREE="$1"

# 1. Create the blank 100 MiB disk image inside the SRCTREE directory
dd if=/dev/zero of="${SRCTREE}/fat32.img" bs=1M count=100

# 2. Partition the image using fdisk
fdisk "${SRCTREE}/fat32.img" <<EOF
o
n
p
1
2048

t
c
w
EOF

# 3. Allocate space for the temporary partition
dd if=/dev/zero of=partition.fat bs=512 count=202752

# 4. Format the temporary partition file as FAT32
mkfs.vfat -F 32 partition.fat

# 5. Copy the hello binary from the workspace path into the FAT32 filesystem
mcopy -i partition.fat "${SRCTREE}/configs/sysroot/bin/hello" ::hello

# 6. Flash the partition back into the main disk image at the 1 MiB offset
dd if=partition.fat of="${SRCTREE}/fat32.img" bs=512 seek=2048 conv=notrunc

# 7. Clean up the temporary partition file
rm partition.fat

