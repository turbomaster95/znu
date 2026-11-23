#ifndef CSEXP_H
#define CSEXP_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    CSEXP_ATOM = 1,
    CSEXP_LIST = 2
} csexp_type;

typedef struct csexp_node csexp_node;

struct csexp_node {
    csexp_type type;

    const uint8_t *data;
    size_t length;

    const uint8_t *hint;
    size_t hint_length;

    csexp_node *first_child;
    csexp_node *last_child;
    csexp_node *next;
};

typedef enum {
    CSEXP_OK = 0,
    CSEXP_ERR_ARGUMENT,
    CSEXP_ERR_EMPTY,
    CSEXP_ERR_UNEXPECTED_CHAR,
    CSEXP_ERR_UNEXPECTED_CLOSE,
    CSEXP_ERR_UNCLOSED_LIST,
    CSEXP_ERR_INVALID_LENGTH,
    CSEXP_ERR_NONCANONICAL_LENGTH,
    CSEXP_ERR_LENGTH_OVERFLOW,
    CSEXP_ERR_TRUNCATED_ATOM,
    CSEXP_ERR_TRAILING_DATA,
    CSEXP_ERR_NO_NODES,
    CSEXP_ERR_MAX_DEPTH,
    CSEXP_ERR_INVALID_DISPLAY_HINT
} csexp_error;

typedef struct {
    csexp_node *root;
    size_t nodes_used;
    size_t error_offset;
    csexp_error error;
} csexp_result;

csexp_error csexp_parse(const uint8_t *input, size_t input_len, csexp_node *nodes, size_t node_cap, csexp_node **stack, size_t stack_cap, csexp_result *result);

const char *csexp_error_string(csexp_error error);

static inline int csexp_bytes_eq(const uint8_t *data, size_t length, const char *text, size_t text_length) { size_t i; if ((data == NULL && length != 0) || text == NULL || length != text_length) return 0; for (i = 0; i < length; i++) if (data[i] != (uint8_t)text[i]) return 0; return 1; }

static inline int csexp_hint_eq(const csexp_node *node, const char *hint) { size_t n = 0; if (node == NULL || hint == NULL || node->hint == NULL) return 0; while (hint[n] != '\0') n++; return csexp_bytes_eq(node->hint, node->hint_length, hint, n); }

static inline int csexp_atom_eq(const csexp_node *node, const char *value) { size_t n = 0; if (node == NULL || node->type != CSEXP_ATOM || value == NULL) return 0; while (value[n] != '\0') n++; return csexp_bytes_eq(node->data, node->length, value, n); }

static inline int csexp_read_le(const csexp_node *node, const char *hint, size_t width, uint64_t *value) { size_t i; uint64_t v = 0; if (node == NULL || value == NULL || node->type != CSEXP_ATOM || node->length != width || !csexp_hint_eq(node, hint) || (width != 0 && node->data == NULL)) return 0; for (i = 0; i < width; i++) v |= (uint64_t)node->data[i] << (i * 8); *value = v; return 1; }

static inline int csexp_as_u8(const csexp_node *node, uint8_t *value) { uint64_t v; if (value == NULL || !csexp_read_le(node, "u8", 1, &v)) return 0; *value = (uint8_t)v; return 1; }
static inline int csexp_as_i8(const csexp_node *node, int8_t *value) { uint64_t v; if (value == NULL || !csexp_read_le(node, "i8", 1, &v)) return 0; *value = (int8_t)(uint8_t)v; return 1; }
static inline int csexp_as_u16(const csexp_node *node, uint16_t *value) { uint64_t v; if (value == NULL || !csexp_read_le(node, "u16", 2, &v)) return 0; *value = (uint16_t)v; return 1; }
static inline int csexp_as_i16(const csexp_node *node, int16_t *value) { uint64_t v; if (value == NULL || !csexp_read_le(node, "i16", 2, &v)) return 0; *value = (int16_t)(uint16_t)v; return 1; }
static inline int csexp_as_u32(const csexp_node *node, uint32_t *value) { uint64_t v; if (value == NULL || !csexp_read_le(node, "u32", 4, &v)) return 0; *value = (uint32_t)v; return 1; }
static inline int csexp_as_i32(const csexp_node *node, int32_t *value) { uint64_t v; if (value == NULL || !csexp_read_le(node, "i32", 4, &v)) return 0; *value = (int32_t)(uint32_t)v; return 1; }
static inline int csexp_as_u64(const csexp_node *node, uint64_t *value) { return csexp_read_le(node, "u64", 8, value); }
static inline int csexp_as_i64(const csexp_node *node, int64_t *value) { uint64_t v; if (value == NULL || !csexp_read_le(node, "i64", 8, &v)) return 0; *value = (v <= INT64_MAX) ? (int64_t)v : -1 - (int64_t)(UINT64_MAX - v); return 1; }
static inline int csexp_as_bool(const csexp_node *node, int *value) { if (node == NULL || value == NULL || node->type != CSEXP_ATOM || node->length != 1 || node->data == NULL || !csexp_hint_eq(node, "bool") || node->data[0] > 1) return 0; *value = node->data[0] != 0; return 1; }

static inline int csexp_as_f32(const csexp_node *node, float *value) { union { uint32_t u; float f; } bits; if (value == NULL || sizeof(float) != 4 || !csexp_hint_eq(node, "f32") || !csexp_as_u32(node, &bits.u)) return 0; *value = bits.f; return 1; }
static inline int csexp_as_f64(const csexp_node *node, double *value) { union { uint64_t u; double f; } bits; if (value == NULL || sizeof(double) != 8 || !csexp_hint_eq(node, "f64") || !csexp_as_u64(node, &bits.u)) return 0; *value = bits.f; return 1; }

#endif
