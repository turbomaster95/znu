import sys
from pathlib import Path

if len(sys.argv) < 3:
    print("Provide a PSF1 file as 1st input!")
    print("Provide kernel srcdir as 2nd input!")
    exit(1)

file = sys.argv[1]
srctree = sys.argv[2]

with open(file, "rb") as f:
    data = f.read()

if data[0] != 0x36 or data[1] != 0x04:
    raise ValueError("Not a valid PSF1 font file!")

headersize = 4
glyph_height = data[3]
bytes_per_glyph = glyph_height

raw_glyphs = data[headersize : headersize + (256 * bytes_per_glyph)]

gfile = srctree + "/include/generated/kfont.h"

def bin2c(input: str, output_file: str):
    data = input

    var_name = "kfont"

    # Format bytes as 0x00, 12 per line
    hex_bytes = [f"0x{b:02x}" for b in data]
    lines = [", ".join(hex_bytes[i:i + 12]) for i in range(0, len(hex_bytes), 12)]
    formatted_data = ",\n    ".join(lines)
    
    header_guard = f"{var_name.upper()}_H"
    content = f"""#ifndef {header_guard}
#define {header_guard}

#include <stddef.h>

// DO NOT EDIT MANUALLY! SWAP THE PSF FONT FROM IT'S LOCATION AT:
// Generated from {file}

const unsigned char {var_name}[] = {{
    {formatted_data}
}};
const size_t {var_name}_len = {len(data)};

#endif // {header_guard}
"""
    Path(output_file).write_text(content)
    print(content)

bin2c(raw_glyphs, gfile)
