#!/usr/bin/env bash

RED='\033[1;31m'
NC='\033[0m'
BLUE='\033[1;36m'

# Setup directories
SRCTREE="$1"
LDP="$2"

if [ -z "$SRCTREE" ]; then
    printf "${RED}Error:${NC} SRCTREE not provided.\n"
    echo ""
    echo "Usage: $0 <srctree> <ld-name>"
    echo ""
    echo "Example: $0 \$(pwd) ld.lld"
    exit 1
fi

if [ -z "$LDP" ]; then
    printf "${RED}Error:${NC} LD's name not provided.\n"
    echo ""
    echo "Usage: $0 <srctree> <ld-name>"
    echo ""
    echo "Example: $0 \$(pwd) ld.lld"
    exit 1
fi

if ! command -v "$LDP" > /dev/null 2>&1; then
    printf "${RED}Error: The linker '$LDP' was not found in your PATH.${NC}\n"
    exit 1
fi

TEMP_DIR="$SRCTREE/scripts/temp"
mkdir -p "$TEMP_DIR"
# Clean up previous runs
rm -f "$TEMP_DIR"/*.o

FINAL_OBJ="$TEMP_DIR/legal.o"
rm -f "$FINAL_OBJ"

OBJCOPY_FLAGS="-I binary -O elf64-x86-64 -B i386"
SECTION_NAME=".legal"

compile_to_obj() {
    local input_txt=$1
    local output_obj=$2
    objcopy $OBJCOPY_FLAGS --rename-section .data=$SECTION_NAME "$input_txt" "$output_obj"
    # Set flags so the section is included in the final binary
    objcopy --set-section-flags $SECTION_NAME=alloc,load,contents,readonly "$output_obj"
}

# --- Handle Root Licenses ---
ROOT_COMBINED="$TEMP_DIR/znu_root.txt"
> "$ROOT_COMBINED"

find "$SRCTREE" -maxdepth 1 -type f \( -iname "LICENSE*" -o -iname "COPYING*" -o -iname "NOTICE*" \) | while read -r root_file; do
    # Skip the NOTICE file inside scripts/temp if it exists
    [[ "$root_file" == *"$TEMP_DIR"* ]] && continue
    echo "--- ROOT: $(basename "$root_file") ---" >> "$ROOT_COMBINED"
    cat "$root_file" >> "$ROOT_COMBINED"
    echo -e "\n" >> "$ROOT_COMBINED"
done

if [ -s "$ROOT_COMBINED" ]; then
    compile_to_obj "$ROOT_COMBINED" "$TEMP_DIR/znu_root.o"
    rm "$ROOT_COMBINED"
fi

# --- Handle Locations from NOTICE ---
paths=$(grep "Location:" "$SRCTREE/NOTICE" | sed -n 's/.*\[\(.*\)\].*/\1/p')

for path in $paths; do
    # Only proceed if the path is a directory; ignore files (like md5.h)
    if [ -d "$path" ]; then
        folder_name=$(basename "$path")
        sub_combined="$TEMP_DIR/${folder_name}.txt"
        sub_obj="$TEMP_DIR/${folder_name}.o"
        
        > "$sub_combined"
        
        # Look for legal files inside the submodule folder
        find "$path" -maxdepth 1 -type f \( -iname "LICENSE*" -o -iname "COPYING*" -o -iname "NOTICE*" \) | while read -r legal_file; do
            echo "--- SUBMODULE ($folder_name): $(basename "$legal_file") ---" >> "$sub_combined"
            cat "$legal_file" >> "$sub_combined"
            echo -e "\n" >> "$sub_combined"
        done

        if [ -s "$sub_combined" ]; then
            compile_to_obj "$sub_combined" "$sub_obj"
            rm "$sub_combined"
        else
            # Remove empty file if no legal docs found in this folder
            rm -f "$sub_combined"
        fi
    fi
done

# Check if there are actually any objects to merge
if ls "$TEMP_DIR"/*.o >/dev/null 2>&1; then
    $LDP -r "$TEMP_DIR"/*.o -o "$FINAL_OBJ"
    
    if [ $? -eq 0 ]; then
        echo "  GEN     scripts/temp/legal.o"
    else
        printf "${RED}Error:${NC} Failed to merge objects.\n"
        exit 1
    fi
else
    printf "${BLUE}Notice:${NC} No legal objects generated to merge.\n"
fi
