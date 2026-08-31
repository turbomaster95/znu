#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>

struct zfilter_prog;
struct zvm_ops;

struct zfilter_ctx {
    void  *data;      // Buffer/Packet/Metadata pointer
    size_t len;       // Length of context buffer
    uint32_t    hook_type;
};

struct zvm_ops {
    const char *name;
    int (*verify)(const void *code, size_t len);
    int (*init)(struct zfilter_prog *prog, const void *code, size_t len);
    uint32_t (*run)(struct zfilter_prog *prog, struct zfilter_ctx *ctx);
    void (*cleanup)(struct zfilter_prog *prog);
};

struct zfilter_prog {
    uint32_t id;
    const struct zvm_ops *ops;    // Pointer to driver implementation
    void *opaque_bytecode;        // Engine-specific instructions / JIT code
    size_t code_len;
    void *vm_priv;                // Private engine state / allocated context
};

int register_zfilter(const struct zvm_ops *ops);
struct zfilter_prog *zfilter_create(const char *engine_name, const void *code, size_t len);
uint32_t zfilter_execute(struct zfilter_prog *prog, void *data, size_t len, uint32_t hook_type);
void zfilter_destroy(struct zfilter_prog *prog);

#endif // FILTER_H
