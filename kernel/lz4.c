#include <stdint.h>
#include <stddef.h>

static uint8_t* lz4_decompress_block(const uint8_t* source, uint8_t* dest, size_t isize) {
    const uint8_t* ip = source;
    const uint8_t* const iend = ip + isize;
    uint8_t* op = dest;

    while (ip < iend) {
        uint8_t token = *ip++;
        size_t lit_len = token >> 4;

        if (lit_len == 15) {
            uint8_t s;
            do { s = *ip++; lit_len += s; } while (s == 255);
        }

        for (size_t i = 0; i < lit_len; i++) *op++ = *ip++;

        if (ip >= iend) break;

        uint16_t offset = ip[0] | (ip[1] << 8);
        ip += 2;

        size_t mat_len = token & 0x0F;
        if (mat_len == 15) {
            uint8_t s;
            do { s = *ip++; mat_len += s; } while (s == 255);
        }
        mat_len += 4;

        uint8_t* ref = op - offset;
        for (size_t i = 0; i < mat_len; i++) *op++ = *ref++;
    }
    return op;
}

// The Main Frame Parser
int lz4_unframe(const uint8_t* source, uint8_t* dest, size_t input_size, size_t max_output) {
    const uint8_t* ip = source;
    const uint8_t* iend = source + input_size;
    uint8_t* op = dest;
    uint8_t* oend = dest + max_output;

    if (input_size < 7) return -1; // Minimum frame size

    // 1. Check Magic Number
    uint32_t magic = ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
    
    // Standard Frame: 0x184D2204
    // Legacy Frame:   0x184C2102
    
    if (magic == 0x184C2102) {
        ip += 4;
        while (ip < iend) {
            if (ip + 4 > iend) break;
            uint32_t block_size = ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
            ip += 4;
            if (block_size == 0) break; 
            if (ip + block_size > iend) return -3;
            
            op = lz4_decompress_block(ip, op, block_size);
            ip += block_size;
            if (op > oend) return -4;
        }
        return (int)(op - dest);
    }

    if (magic != 0x184D2204) return -1; 
    ip += 4;

    // 2. Parse Frame Descriptor
    uint8_t flg = *ip++;
    uint8_t bd  = *ip++; (void)bd; 
    
    if (flg & (1 << 3)) ip += 8; // Content Size present
    if (flg & (1 << 0)) ip += 4; // Dictionary ID present
    ip++; // Skip Header Checksum (HC)

    // 3. Parse Blocks
    while (ip < iend) {
        if (ip + 4 > iend) break;
        uint32_t block_size = ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
        ip += 4;

        if (block_size == 0) break; // End of blocks

        // Bit 31 is the "uncompressed" flag
        if (block_size & 0x80000000) {
            uint32_t real_size = block_size & 0x7FFFFFFF;
            if (ip + real_size > iend || op + real_size > oend) return -2;
            for(uint32_t i=0; i < real_size; i++) *op++ = *ip++;
        } else {
            if (ip + block_size > iend) return -3;
            // Decompress this block
            op = lz4_decompress_block(ip, op, block_size);
            ip += block_size;
            if (op > oend) return -4;
        }
        
        // Skip block checksum if flag is set
        if (flg & (1 << 4)) ip += 4;
    }

    return (int)(op - dest);
}
