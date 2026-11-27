extern "C" {
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
}

struct ubsan_source_location {
    const char* filename;
    uint32_t line;
    uint32_t column;
};

struct ubsan_type_descriptor {
    uint16_t type_kind;
    uint16_t type_info;
    char type_name[1];
};

extern "C" {
void ubsan_log_header(const char* condition, const ubsan_source_location* loc);
}

#include <cxx/typeinfo>

namespace __cxxabiv1 {
class __class_type_info;
extern "C" void* __dynamic_cast(
    const void* sub,
    const __class_type_info* src,
    const __class_type_info* dst,
    ptrdiff_t src2dst_offset
);
}

struct ubsan_dynamic_type_cache_miss_data {
    ubsan_source_location location;
    const ubsan_type_descriptor* type;
    void* type_info;
    uint8_t type_check_kind;
};

#define UBSAN_VPTR_CACHE_SIZE 128
extern "C" uintptr_t __ubsan_vptr_type_cache[UBSAN_VPTR_CACHE_SIZE] = {0};

extern "C" void __ubsan_handle_dynamic_type_cache_miss_abort(
    ubsan_dynamic_type_cache_miss_data* data,
    uintptr_t ptr,
    uintptr_t hash
) {
    (void)hash;
    ubsan_log_header("C++ Dynamic Type Cache Miss (vptr)", &data->location);
    debugln("  Object Address: %p\n", (void*)ptr);
    if (ptr) {
        debugln("  Actual vptr   : %p\n", (void*)(*reinterpret_cast<const uintptr_t*>(ptr)));
    }
    debugln("  Check Kind    : %u\n", (unsigned)data->type_check_kind);
    if (data->type) {
        debugln("  Expected Type : %s\n", data->type->type_name);
    }
    panic("[ubsan] Dynamic Type Cache Miss");
}

extern "C" void __ubsan_handle_dynamic_type_cache_miss(
    ubsan_dynamic_type_cache_miss_data* data,
    uintptr_t ptr,
    uintptr_t hash
) {
    if (!ptr) return;

    uintptr_t vptr = *reinterpret_cast<const uintptr_t*>(ptr);
    if (vptr < 0xffffffff80000000ULL) {
        __ubsan_handle_dynamic_type_cache_miss_abort(data, ptr, hash);
        return;
    }

    const auto* expected_type = static_cast<const __cxxabiv1::__class_type_info*>(data->type_info);
    void* cast_res = __cxxabiv1::__dynamic_cast(
        reinterpret_cast<const void*>(ptr),
        expected_type,
        expected_type,
        -1
    );

    if (!cast_res) {
        __ubsan_handle_dynamic_type_cache_miss_abort(data, ptr, hash);
        return;
    }

    __ubsan_vptr_type_cache[hash % UBSAN_VPTR_CACHE_SIZE] = hash;
}