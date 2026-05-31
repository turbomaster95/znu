#!/usr/bin/env bash
set -e

SRCTREE=$1
IMG_NAME=$2

if [ -z "$SRCTREE" ] || [ -z "$IMG_NAME" ]; then
    echo "Usage: $0 <srctree> <img_name>"
    exit 1
fi

# --- Termux/Android Native Width-Aware iconv Shim ---
SHIM_SO="$SRCTREE/scripts/.mtools_shim.so"

# Clear out the old dumb-shim to force a recompile
rm -f "$SHIM_SO"

echo "[*] Compiling width-aware mtools compatibility shim..."
cat << 'EOF' > "$SRCTREE/scripts/.shim.c"
#include <stddef.h>
#include <stdint.h>

typedef void* iconv_t;

// Detect if mtools is asking for wide-character (UTF-32/UCS-4/WCHAR_T) conversions
static int is_wide(const char *str) {
    if (!str) return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A'); // Lowercase conversion
        if (c == 'w' && (str[i+1] == 'c' || str[i+1] == 'C')) return 1; // "wchar"
        if (c == 'u' && (str[i+1] == 'c' || str[i+1] == 'C' || str[i+1] == 't' || str[i+1] == 'T')) return 1; // "ucs" or "utf-32"
    }
    return 0;
}

iconv_t iconv_open(const char *tocode, const char *fromcode) {
    if (is_wide(fromcode)) return (iconv_t)2; // Mode 2: WCHAR -> BYTE
    if (is_wide(tocode))   return (iconv_t)3; // Mode 3: BYTE -> WCHAR
    return (iconv_t)1;                        // Mode 1: BYTE -> BYTE
}

int iconv_close(iconv_t cd) { return 0; }

size_t iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
    if (!inbuf || !*inbuf || !outbuf || !*outbuf) return 0;
    
    uintptr_t mode = (uintptr_t)cd;
    char *in = *inbuf;
    char *out = *outbuf;
    size_t in_len = *inbytesleft;
    size_t out_len = *outbytesleft;
    
    if (mode == 2) { 
        // WCHAR (4 bytes) to BYTE (1 byte) -> Strip trailing zeroes for ASCII
        while (in_len >= 4 && out_len >= 1) {
            *out = *in; 
            out++; out_len--;
            in += 4; in_len -= 4;
        }
    } else if (mode == 3) { 
        // BYTE (1 byte) to WCHAR (4 bytes) -> Pad out layout with zeroes
        while (in_len >= 1 && out_len >= 4) {
            out[0] = *in;
            out[1] = 0; out[2] = 0; out[3] = 0;
            out += 4; out_len -= 4;
            in++; in_len--;
        }
    } else { 
        // Standard BYTE to BYTE (1:1 raw copy for standard pathnames)
        size_t len = (in_len < out_len) ? in_len : out_len;
        for (size_t i = 0; i < len; i++) out[i] = in[i];
        in += len; out += len;
        in_len -= len; out_len -= len;
    }
    
    *inbuf = in;
    *outbuf = out;
    *inbytesleft = in_len;
    *outbytesleft = out_len;
    return 0;
}
EOF

clang -shared -fPIC "$SRCTREE/scripts/.shim.c" -o "$SHIM_SO"
rm -f "$SRCTREE/scripts/.shim.c"

export LD_PRELOAD="$SHIM_SO"
# -----------------------------------------------------

rm -f "$IMG_NAME"
truncate -s 128M "$IMG_NAME"

# Partitioning
sgdisk -Z "$IMG_NAME" 2>/dev/null
sgdisk -n 1:2048:+1M -t 1:ef02 -c 1:"BIOS_BOOT" "$IMG_NAME" 2>/dev/null
sgdisk -n 2:4096:0 -t 2:ef00 -c 2:"ZNU_ESP" "$IMG_NAME" 2>/dev/null

# Format and build image completely natively
MTOOLS_SKIP_CHECK=1 mformat -i "$IMG_NAME"@@2M -F ::
MTOOLS_SKIP_CHECK=1 mcopy -o -i "$IMG_NAME"@@2M -s "$SRCTREE/configs/iso_root/"* ::/
MTOOLS_SKIP_CHECK=1 mcopy -o -i "$IMG_NAME"@@2M -s znu ::/boot/kernel.bin

# Deploy Limine bootloader
"$SRCTREE/scripts/limine/bin/limine" bios-install "$IMG_NAME"

echo "Image built successfully: $IMG_NAME"

