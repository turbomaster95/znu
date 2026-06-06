#!/bin/bash

# Target archives to grab undefined values from
TARGET_LIBS="kernel/librumpy.a"
OUTPUT_FILE="rump_stubs.c"

echo "/* Automatically generated clean stubs for kernel building */" > "$OUTPUT_FILE"
echo "#include <stddef.h>" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "[*] Scanning archives for undefined symbols..."

# Combine and stream the nm output safely
nm -u $TARGET_LIBS 2>/dev/null | \
sed -E 's/^[[:space:]]+U[[:space:]]+//' | \
awk '
  # 1. Skip lines containing archive markers, paths, object filenames or colons
  /\.o/   { next }
  /:/     { next }
  /^[[:space:]]*$/ { next }
  
  # 2. Skip standard compiler intrinsics or stack protectors
  /^__/   { next }
  
  # 3. Handle duplicates! Check if we have seen this symbol name already
  seen[$1]++ { next }
  
  # 4. If it survives, format it as a clean unique function declaration
  { 
    print "__attribute__((weak)) long " $1 "() { /* Dummy Stub */ return 0; }" 
  }
' >> "$OUTPUT_FILE"

TOTAL_STUBS=$(grep -c "Dummy Stub" "$OUTPUT_FILE")
echo "[SUCCESS] Generated $TOTAL_STUBS UNIQUE clean stubs in $OUTPUT_FILE!"
