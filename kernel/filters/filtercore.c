#include <filter.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>

#define MAX_VM_ENGINES 8

static const struct zvm_ops *registered_engines[MAX_VM_ENGINES];
static size_t engine_count = 0;

int register_zfilter(const struct zvm_ops *ops) {
    if (!ops || !ops->name || !ops->run || engine_count >= MAX_VM_ENGINES) {
        return -1;
    }
    registered_engines[engine_count++] = ops;
    debugln("[zfilter] Registered VM engine driver: %s\n", ops->name);
    return 0;
}

struct zfilter_prog *zfilter_create(const char *engine_name, const void *code, size_t len) {
    const struct zvm_ops *target_ops = NULL;

    for (size_t i = 0; i < engine_count; i++) {
        if (strcmp(registered_engines[i]->name, engine_name) == 0) {
            target_ops = registered_engines[i];
            break;
        }
    }
    if (!target_ops) return NULL;

    if (target_ops->verify && target_ops->verify(code, len) != 0) {
        debugln("[zfilter] Bytecode verification failed for engine: %s\n", engine_name);
        return NULL;
    }
 
    struct zfilter_prog *prog = kmalloc(sizeof(struct zfilter_prog));
    if (!prog) return NULL;

    prog->ops = target_ops;
    prog->code_len = len;

    if (target_ops->init && target_ops->init(prog, code, len) != 0) {
        kfree(prog);
        return NULL;
    }

    return prog;
}

uint32_t zfilter_execute(struct zfilter_prog *prog, void *data, size_t len, uint32_t hook_type) {
    if (!prog || !prog->ops || !prog->ops->run) return 0;

    struct zfilter_ctx ctx = {
        .data = data,
        .len = len,
        .hook_type = hook_type
    };

    return prog->ops->run(prog, &ctx);
}

void zfilter_destroy(struct zfilter_prog *prog) {
    if (!prog) return;
    if (prog->ops->cleanup) {
        prog->ops->cleanup(prog);
    }
    kfree(prog);
}
