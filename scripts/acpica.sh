#!/bin/bash
# scripts/build_acpica.sh

# 1. Setup paths
CC="/data/data/com.termux/files/home/opt/cross-x64/bin/x86_64-elf-gcc"
LD="/data/data/com.termux/files/home/opt/cross-x64/bin/x86_64-elf-ld"

SRC="lib/acpica/source/components"
INC="-Ilib/acpica/source/include -Ilib/libc/include"
# Find internal compiler headers for stdarg.h
INTERNAL_INC=$($CC -print-file-name=include)
OUT="lib/acpica_out"
mkdir -p $OUT

# 2. Define the core files we need (Minimalist List)
FILES=$(find $SRC -maxdepth 2 -not -path '*/debugger*' -not -path '*/disassembler*' -name "*.c" | grep -vE "db|dump|trace|help|os")

# 3. Compile each file into its own .o
OBJ_LIST=""
for f in $FILES; do
    # Create a unique name to avoid path collisions in the output dir
    obj_name=$(echo $f | tr '/' '_').o
    
    # Compile with the same flags your kernel uses
    # We define _GNU_EFI to bypass the Linux host headers
    echo "CC $f"
    $CC -c $f -o $OUT/$obj_name $INC -isystem $INTERNAL_INC \
    -U__linux__ -w -U__unix__ -include lib/libc/include/string.h -include lib/libc/include/ctype.h \
    -D_GNU_EFI -D_EFI64 -DACPI_MACHINE_WIDTH=64 \
    -DACPI_USE_SYSTEM_CLIBRARY=0 \
    -DACPI_USE_STANDARD_HEADERS=0 \
    -ffreestanding -nostdinc -fno-stack-protector -mcmodel=kernel -mno-red-zone

    OBJ_LIST="$OBJ_LIST $OUT/$obj_name"
done

# 4. Use the Linker to merge them into one relocatable object
# -r means "Relocatable", so it doesn't need a 'main' function
$LD -r $OBJ_LIST -o lib/acpica_lib.o
