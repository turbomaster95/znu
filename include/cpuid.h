#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>

#define CPUID_GETVENDORSTRING         0x00000000
#define CPUID_GETFEATURES             0x00000001 /* Family, Model, Stepping, and Feature Flags */
#define CPUID_GETCACHETLBINFO         0x00000002
#define CPUID_GETSERIALNUMBER         0x00000003
#define CPUID_GETDETERMINISTICCACHE   0x00000004 /* Subleaves: Cache index */
#define CPUID_GETMONITORLEWAIT        0x00000005
#define CPUID_GETTHERMALPOWERMGMT     0x00000006
#define CPUID_GETSTRUCTFEATUREFLAGS   0x00000007 /* Subleaves: 0=Max, 1=Features */
#define CPUID_GETARCHPERFMON          0x0000000A
#define CPUID_GETTOPOLOGY             0x0000000B /* Extended Topology Enumeration */
#define CPUID_GETXSAVEAREASTATE       0x0000000D /* Subleaves: State components */
#define CPUID_GETRDTMONITORING        0x0000000F /* Resource Director Technology */
#define CPUID_GETRDTALLOCATION        0x00000010
#define CPUID_GETSGXCAPABILITIES      0x00000012 /* Intel Software Guard Extensions */
#define CPUID_GETINTELPT              0x00000014 /* Intel Processor Trace */
#define CPUID_GETTSC_INFO             0x00000015 /* Nominal Core Crystal Clock */
#define CPUID_GETPROCESSOR_FREQ       0x00000016 /* Base/Max/Bus frequencies */
#define CPUID_GETSOC_VENDOR           0x00000017
#define CPUID_GETDETERMINISTICTOPOLOGY 0x0000001F /* V2 Extended Topology */

#define CPUID_HYPERVISOR_INTERFACE    0x40000000
#define CPUID_HYPERVISOR_FEATURES     0x40000001

#define CPUID_EXT_MAX_FUNCTION        0x80000000
#define CPUID_EXT_FEATURES            0x80000001 /* AMD-specific and extended features */
#define CPUID_EXT_BRAND_STRING_1      0x80000002
#define CPUID_EXT_BRAND_STRING_2      0x80000003
#define CPUID_EXT_BRAND_STRING_3      0x80000004
#define CPUID_EXT_L1_CACHE_TLB        0x80000005
#define CPUID_EXT_L2_L3_CACHE_TLB     0x80000006
#define CPUID_EXT_ADV_POWER_MGMT      0x80000007
#define CPUID_EXT_ADDR_SIZE           0x80000008 /* Physical/Virtual address sizes */
#define CPUID_EXT_SVM_FEATURES        0x8000000A /* Secure Virtual Machine (AMD) */
#define CPUID_EXT_L1_TLB_1GB_PAGES    0x80000019
#define CPUID_EXT_PERF_OPTIMIZATIONS  0x8000001A
#define CPUID_EXT_INSTRUCTION_SET     0x8000001B
#define CPUID_EXT_LWP                 0x8000001C /* Lightweight Profiling */
#define CPUID_EXT_CACHE_PROPERTIES    0x8000001D
#define CPUID_EXT_TOPOLOGY            0x8000001E /* AMD Topology */
#define CPUID_EXT_MEM_ENCRYPTION      0x8000001F /* AMD SME/SEV */

typedef struct {
    uint32_t eax, ebx, ecx, edx;
} cpuid_res_t;

/**
 * @brief Executes the CPUID instruction.
 * 
 * @param leaf The primary leaf (EAX).
 * @param subleaf The secondary leaf (ECX), required for leaves like 0x04, 0x07, 0x0D.
 */
static inline cpuid_res_t cpuid_query(uint32_t leaf, uint32_t subleaf) {
    cpuid_res_t res;
    /* We use volatile because CPUID depends on the hardware state, 
       and we must preserve EBX because it's used by the PIC in some 32-bit environments. */
    __asm__ volatile (
        "cpuid"
        : "=a" (res.eax), "=b" (res.ebx), "=c" (res.ecx), "=d" (res.edx)
        : "a" (leaf), "c" (subleaf)
    );
    return res;
}

static int cpuid_has_feature(uint32_t leaf, uint32_t reg_offset, uint32_t bit) {
    cpuid_res_t res = cpuid_query(leaf, 0);
    uint32_t *regs = (uint32_t*)&res;
    return (regs[reg_offset] >> bit) & 1;
}

#endif
