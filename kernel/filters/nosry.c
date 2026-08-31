#include <kernel/initcall.h>
#include <filter.h>
#include <stdio.h>
#include <stddef.h>

#define NOSRY_GLUE_GLOBAL
#include "external/nosry/vm.h"
#include "external/nosry/verify.h"

static int nosry_verify(const void *code, size_t len) {
    if (!code || len == 0) {
        debugln("[nosryf] Verification failed: NULL or zero-length buffer\n");
        return -VM_VERIFY_NULL_PROGRAM;
    }

    VerifierReport report = VM_verify((const uint8_t *)code, len);

    switch (report.error) {
        case VM_VERIFY_OK:
            debugln("[nosryf] Verification succeeded (VM_VERIFY_OK)\n");
            return 0;

        case VM_VERIFY_NULL_PROGRAM:
            debugln("[nosryf] Verification failed: NULL program buffer (code %d)\n", report.error);
            break;

        case VM_VERIFY_BAD_OPCODE:
            debugln("[nosryf] Verification failed: Unknown or unhandled opcode encountered (code %d)\n", report.error);
            break;

        case VM_VERIFY_BAD_DEST_REGISTER:
            debugln("[nosryf] Verification failed: Destination Register index out of bounds (code %d)\n", report.error);
            break;

        case VM_VERIFY_BAD_SRC_REGISTER:
            debugln("[nosryf] Verification failed: Source Register index out of bounds (code %d)\n", report.error);
            break;

        case VM_VERIFY_BACKWARD_JUMP:
            debugln("[nosryf] Verification failed: Backward jump/loop prohibited (code %d)\n", report.error);
            break;

        case VM_VERIFY_DIVIDE_BY_ZERO:
            debugln("[nosryf] Verification failed: Division by zero immediate detected (code %d)\n", report.error);
            break;

        case VM_VERIFY_CALL_DEPTH_UNPROVEN:
            debugln("[nosryf] Verification failed: Call stack depth exceeded limit (code %d)\n", report.error);
            break;

        case VM_VERIFY_NO_HALT:
            debugln("[nosryf] Verification failed: No HALT found (code %d)\n", report.error);
            break;

        default:
            debugln("[nosryf] Verification failed: Unrecognized verifier status (code %d)\n", report.error);
            break;
    }

    return -(int)report.error;
}

static int nosry_init(struct zfilter_prog *prog, const void *code, size_t len) {
    Inst *prog_copy = kmalloc(len);
    if (!prog_copy) return -1;

    memcpy(prog_copy, code, len);
    prog->opaque_bytecode = prog_copy;
    return 0;
}

static uint32_t nosry_run(struct zfilter_prog *prog, struct zfilter_ctx *ctx) {
    VM vm;
    Memory mem;

    VM_reset(&vm, &mem);

    // Map kernel filter context into VM RAM
    if (ctx->data && ctx->len <= RAM_SIZE) {
        memcpy(mem.ram, ctx->data, ctx->len);
    }

    size_t inst_count = prog->code_len / sizeof(Inst);
    VM_run(&vm, (const Inst *)prog->opaque_bytecode, inst_count, &mem);

    // Return register R0 as standard filter decision output
    return vm.regs[0];
}

static void nosry_cleanup(struct zfilter_prog *prog) {
    if (prog->opaque_bytecode) {
        kfree(prog->opaque_bytecode);
    }
}

static const struct zvm_ops nosry_vm_ops = {
    .name = "nosry",
    .verify = nosry_verify,
    .init = nosry_init,
    .run = nosry_run,
    .cleanup = nosry_cleanup,
};

void init_nf_engine(void) {
    register_zfilter(&nosry_vm_ops);
}

void exit_nf_engine(void) {
    // nothing here atm
}

module_init(init_nf_engine);
module_exit(exit_nf_engine);

