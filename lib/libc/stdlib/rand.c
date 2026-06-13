#include <stdint.h>
#include <entropy.h>

drbg_ctx_t k_drbg;
static entropy_pool_t k_pool;
static int k_initialized = 0;

static void ensure_initialized(void) {
    if (!k_initialized) {
        entropy_pool_init(&k_pool);

        /* Gather hardware entropy */
        uint8_t hw_entropy[64];
        gather_hardware_entropy(hw_entropy, sizeof(hw_entropy));
        entropy_pool_add(&k_pool, hw_entropy, sizeof(hw_entropy));

        /* Add some timing jitter */
        uint32_t lo, hi;
        __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
        uint64_t tsc = ((uint64_t)hi << 32) | lo;
        entropy_pool_add(&k_pool, (uint8_t *)&tsc, sizeof(tsc));

        /* Extract seed material */
        uint8_t seed[64];
        uint8_t nonce[32];
        entropy_pool_extract(&k_pool, seed, sizeof(seed));
        entropy_pool_extract(&k_pool, nonce, sizeof(nonce));

        /* Instantiate DRBG */
        drbg_instantiate(&k_drbg, seed, sizeof(seed), 
                         nonce, sizeof(nonce),
                         (const uint8_t *)"znu_kernel_rng_v1", 18);

        memset(hw_entropy, 0, sizeof(hw_entropy));
        memset(seed, 0, sizeof(seed));
        memset(nonce, 0, sizeof(nonce));

        k_initialized = 1;
    }
}

void seed_from_text(const char *text) {
    ensure_initialized();

    size_t len = 0;
    while (text[len]) len++;

    /* Hash the text and add to entropy pool */
    uint8_t text_hash[32];
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)text, len);
    sha256_final(&ctx, text_hash);

    entropy_pool_add(&k_pool, text_hash, sizeof(text_hash));

    /* Re-seed DRBG with fresh entropy */
    uint8_t fresh_entropy[64];
    entropy_pool_extract(&k_pool, fresh_entropy, sizeof(fresh_entropy));
    drbg_reseed(&k_drbg, fresh_entropy, sizeof(fresh_entropy), NULL, 0);

    memset(text_hash, 0, sizeof(text_hash));
    memset(fresh_entropy, 0, sizeof(fresh_entropy));
    memset(&ctx, 0, sizeof(ctx));
}

void srand(unsigned int seed) {
    char seed_buf[32];
    unsigned int temp = seed;
    int idx = 0;

    if (temp == 0) {
        seed_buf[idx++] = '0';
    } else {
        while (temp > 0 && idx < 30) {
            seed_buf[idx++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    seed_buf[idx] = '\0';
    seed_from_text(seed_buf);
}

int rand(void) {
    ensure_initialized();

    uint32_t val;
    secure_random_bytes(&k_drbg, (uint8_t *)&val, sizeof(val));

    return (int)(val & 0x7FFFFFFF);
}

void seed_from_hardware(void) {
    ensure_initialized();

    uint8_t hw_entropy[128];
    gather_hardware_entropy(hw_entropy, sizeof(hw_entropy));

    entropy_pool_add(&k_pool, hw_entropy, sizeof(hw_entropy));

    uint8_t fresh[64];
    entropy_pool_extract(&k_pool, fresh, sizeof(fresh));
    drbg_reseed(&k_drbg, fresh, sizeof(fresh), NULL, 0);

    memset(hw_entropy, 0, sizeof(hw_entropy));
    memset(fresh, 0, sizeof(fresh));
}

void rand_reseed(void) {
    seed_from_hardware();
}

void random_bytes(uint8_t *out, size_t len) {
    ensure_initialized();
    secure_random_bytes(&k_drbg, out, len);
}

void entropy_garden(void) {
    uint64_t state_buf[24]; 
    int idx = 0;

    // TSC 
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    state_buf[idx++] = ((uint64_t)hi << 32) | lo;

    // Control Registers
    uint64_t cr0, cr2, cr3, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0)); state_buf[idx++] = cr0;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2)); state_buf[idx++] = cr2;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3)); state_buf[idx++] = cr3;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4)); state_buf[idx++] = cr4;

    // Processor Status & RFLAGS
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(rflags)); 
    state_buf[idx++] = rflags;

    // Memory Stack
    uint64_t rsp, rbp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp)); state_buf[idx++] = rsp;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp)); state_buf[idx++] = rbp;

    // Segment Selectors
    uint16_t cs, ds, ss, es, fs, gs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs)); state_buf[idx++] = cs;
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds)); state_buf[idx++] = ds;
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss)); state_buf[idx++] = ss;
    __asm__ volatile ("mov %%es, %0" : "=r"(es)); state_buf[idx++] = es;
    __asm__ volatile ("mov %%fs, %0" : "=r"(fs)); state_buf[idx++] = fs;
    __asm__ volatile ("mov %%gs, %0" : "=r"(gs)); state_buf[idx++] = gs;

    if (cpu_has_rdseed()) {
        uint64_t seed_val;
        if (rdseed64(&seed_val)) state_buf[idx++] = seed_val;
    }
    if (cpu_has_rdrand()) {
        uint64_t rand_val;
        if (rdrand64(&rand_val)) state_buf[idx++] = rand_val;
    }

    uint64_t local_rand_state = state_buf[0];
    if (local_rand_state == 0) {
        local_rand_state = 0x55AA55AA55AA55AAULL; // Fail-safe non-zero anchor
    }

    for (int i = 0; i < idx; i++) {
        local_rand_state ^= local_rand_state << 13;
        local_rand_state ^= local_rand_state >> 7;
        local_rand_state ^= local_rand_state << 17;
        int shift_amount = local_rand_state % 64;
        uint64_t dynamic_mask = local_rand_state;
        uint64_t rotated = (state_buf[i] >> shift_amount) | (state_buf[i] << (64 - shift_amount));
        state_buf[i] = rotated ^ dynamic_mask;
    }

    entropy_pool_add(&k_pool, (uint8_t *)state_buf, idx * sizeof(uint64_t));

    memset(state_buf, 0, sizeof(state_buf));
    local_rand_state = 0;
}
