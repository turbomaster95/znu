#include <stdlib.h>
#include <stdint.h>

__attribute__((weak)) void* __emutls_get_address(void* obj) {
    (void)obj;
    return (void*)0;
}

struct ubsan_source_location {
    const char *filename;
    uint32_t line;
    uint32_t column;
};

struct ubsan_type_descriptor {
    uint16_t type_kind;
    uint16_t type_info;
    char type_name[];
};

struct ubsan_type_mismatch_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
    uint8_t alignment;
    uint8_t type_check_kind;
};

struct ubsan_overflow_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_shift_out_of_bounds_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *lhs_type;
    struct ubsan_type_descriptor *rhs_type;
};

struct ubsan_out_of_bounds_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *array_type;
    struct ubsan_type_descriptor *index_type;
};

struct ubsan_invalid_value_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_unreachable_data {
    struct ubsan_source_location location;
};

struct ubsan_function_type_mismatch_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_float_cast_overflow_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *from_type;
    struct ubsan_type_descriptor *to_type;
};

static void ubsan_log_header(const char *err, struct ubsan_source_location *loc) {
    debugln("\n[UBSAN FATAL ERROR]\n");
    debugln("  Condition: %s\n", err);
    debugln("  Location : %s:%d:%d\n", loc->filename, loc->line, loc->column);
}

void __ubsan_handle_type_mismatch_v1_abort(struct ubsan_type_mismatch_data *data, uintptr_t ptr) {
    ubsan_log_header("Type Mismatch", &data->location);
    debugln("  Object Address: %p\n", ptr);
    debugln("  Expected Type : %s\n", data->type->type_name);
    debugln("  Alignment Required: %d bytes\n", 1 << data->alignment);
    
    const char *kinds[] = {"load of", "store to", "reference binding to", "member access within", "member call on", "constructor call on", "downcast of", "downcast of", "upcast of", "cast from non-virtual base to derived of", "dynamic operation on"};
    if (data->type_check_kind < 11) 
        debugln("  Operation: %s\n", kinds[data->type_check_kind]);

    panic("[ubsan] Type Mismatch");
}

void __ubsan_handle_shift_out_of_bounds_abort(struct ubsan_shift_out_of_bounds_data *data, uintptr_t lhs, uintptr_t rhs) {
    ubsan_log_header("Shift Out of Bounds", &data->location);
    debugln("  LHS Type : %s (Value: %p)\n", data->lhs_type->type_name, lhs);
    debugln("  RHS Type : %s (Shift Amount: %d)\n", data->rhs_type->type_name, (int)rhs);
    
    panic("[ubsan] Shift Out of Bounds");
}

void __ubsan_handle_out_of_bounds_abort(struct ubsan_out_of_bounds_data *data, uintptr_t index) {
    ubsan_log_header("Index Out of Bounds", &data->location);
    debugln("  Array Type: %s\n", data->array_type->type_name);
    debugln("  Attempted Index: %d\n", (int)index);
    
    panic("[ubsan] Out of Bounds");
}

void __ubsan_handle_add_overflow_abort(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    ubsan_log_header("Addition Overflow", &data->location);
    debugln("  Type : %s\n", data->type->type_name);
    debugln("  Values: %d + %d overflows\n", (int)lhs, (int)rhs);
    panic("[ubsan] Arithmetic Overflow");
}

void __ubsan_handle_pointer_overflow_abort(struct ubsan_overflow_data *data, uintptr_t base, uintptr_t result) {
    ubsan_log_header("Pointer Overflow", &data->location);
    debugln("  Base   : %p\n", base);
    debugln("  Result : %p\n", result);
    panic("[ubsan] Pointer Overflow");
}

void __ubsan_handle_sub_overflow_abort(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    ubsan_log_header("Subtraction Overflow", &data->location);
    panic("[ubsan] Arithmetic Overflow");
}

void __ubsan_handle_mul_overflow_abort(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    ubsan_log_header("Multiplication Overflow", &data->location);
    panic("[ubsan] Arithmetic Overflow");
}

void __ubsan_handle_negate_overflow_abort(void* data, void* location) {
    panic("[ubsan] Negate Overflow");
}
void __ubsan_handle_divrem_overflow_abort(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    ubsan_log_header("Division Overflow", &data->location);
    panic("[ubsan] Arithmetic Overflow");
}

void __ubsan_handle_load_invalid_value_abort(void *data, uintptr_t lhs, uintptr_t rhs) {
    ubsan_log_header("Invalid Load Value", &((struct ubsan_invalid_value_data *)data)->location);
    panic("[ubsan] Invalid Value Loaded");
}

void __ubsan_handle_builtin_unreachable(struct ubsan_unreachable_data *data) {
    ubsan_log_header("Builtin Unreachable", &data->location);
    debugln("  Code path marked as unreachable was executed\n");
    panic("[ubsan] builtin unreachable");
}

void __ubsan_handle_function_type_mismatch_abort(struct ubsan_function_type_mismatch_data *data, uintptr_t fn_ptr) {
    ubsan_log_header("Function Type Mismatch", &data->location);
    debugln("  Expected Function Type: %s\n", data->type->type_name);
    debugln("  Function Pointer: %p\n", fn_ptr);
    panic("[ubsan] function type mismatch");
}

void __ubsan_handle_float_cast_overflow_abort(struct ubsan_float_cast_overflow_data *data, uintptr_t from) {
    (void)from;
    ubsan_log_header("Float Cast Overflow", &data->location);
    if (data->from_type && data->to_type) {
        debugln("  From Type : %s\n", data->from_type->type_name);
        debugln("  To Type   : %s\n", data->to_type->type_name);
    }
    panic("[ubsan] Float Cast Overflow");
}

void __ubsan_handle_float_cast_overflow(struct ubsan_float_cast_overflow_data *data, uintptr_t from) {
    __ubsan_handle_float_cast_overflow_abort(data, from);
}

void __ubsan_handle_type_mismatch_v1(struct ubsan_type_mismatch_data *data, uintptr_t ptr) {
    __ubsan_handle_type_mismatch_v1_abort(data, ptr);
}

void __ubsan_handle_shift_out_of_bounds(struct ubsan_shift_out_of_bounds_data *data, uintptr_t lhs, uintptr_t rhs) {
    __ubsan_handle_shift_out_of_bounds_abort(data, lhs, rhs);
}

void __ubsan_handle_out_of_bounds(struct ubsan_out_of_bounds_data *data, uintptr_t index) {
    __ubsan_handle_out_of_bounds_abort(data, index);
}

void __ubsan_handle_add_overflow(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    __ubsan_handle_add_overflow_abort(data, lhs, rhs);
}

void __ubsan_handle_sub_overflow(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    __ubsan_handle_sub_overflow_abort(data, lhs, rhs);
}

void __ubsan_handle_mul_overflow(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    __ubsan_handle_mul_overflow_abort(data, lhs, rhs);
}

void __ubsan_handle_divrem_overflow(struct ubsan_overflow_data *data, uintptr_t lhs, uintptr_t rhs) {
    __ubsan_handle_divrem_overflow_abort(data, lhs, rhs);
}

void __ubsan_handle_negate_overflow(void *data, void *location) {
    __ubsan_handle_negate_overflow_abort(data, location);
}

void __ubsan_handle_pointer_overflow(struct ubsan_overflow_data *data, uintptr_t base, uintptr_t result) {
    __ubsan_handle_pointer_overflow_abort(data, base, result);
}
