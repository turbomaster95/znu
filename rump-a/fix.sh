#!/usr/bin/env bash
PREFIX="x86_64-elf-"

mkdir -p tmp_extract
cp librumpy.a tmp_extract/
cd tmp_extract

# Extract everything
"${PREFIX}ar" x librumpy.a
rm librumpy.a

# Convert every single object's properties
for obj in *.o; do
    # Force the object into the kernel/large code model space 
    # by flagging code sections explicitly
    "${PREFIX}objcopy" --set-section-flags .text=alloc,code,load \
                       --set-section-flags .rodata=alloc,data,load \
                       "$obj"
done

# Repack into a fixed archive
"${PREFIX}ar" rcs ../librump_fixed.a *.o

cd ..
rm -rf tmp_extract
echo "Generated librump_fixed.a"
