#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t eax, ebx, ecx, edx;
} cpuid_res_t;

static inline cpuid_res_t cpuid_query(uint32_t leaf, uint32_t subleaf) {
    cpuid_res_t res;
    __asm__ volatile (
        "cpuid"
        : "=a" (res.eax), "=b" (res.ebx), "=c" (res.ecx), "=d" (res.edx)
        : "a" (leaf), "c" (subleaf)
        : "memory"
    );
    return res;
}

static inline bool cpuid_leaf_supported(uint32_t leaf) {
    if (leaf <= 0x00000000) return true;
    if (leaf < 0x80000000) {
        cpuid_res_t res = cpuid_query(0, 0);
        return leaf <= res.eax;
    }
    cpuid_res_t res = cpuid_query(0x80000000, 0);
    return leaf <= res.eax;
}

#define CPUID_VENDOR_STRING             0x00000000
#define CPUID_FEATURES                  0x00000001
#define CPUID_CACHE_TLB_INFO            0x00000002
#define CPUID_PROCESSOR_SERIAL          0x00000003
#define CPUID_DETERMINISTIC_CACHE       0x00000004
#define CPUID_MONITOR_MWAIT             0x00000005
#define CPUID_THERMAL_POWER_MGMT        0x00000006
#define CPUID_STRUCTURED_FEATURES       0x00000007
#define CPUID_DIRECT_CACHE_ACCESS       0x00000009
#define CPUID_ARCH_PERF_MON             0x0000000A
#define CPUID_EXTENDED_TOPOLOGY         0x0000000B
#define CPUID_XSAVE_FEATURES            0x0000000D
#define CPUID_RDT_MONITORING            0x0000000F
#define CPUID_RDT_ALLOCATION            0x00000010
#define CPUID_SGX_CAPABILITIES          0x00000012
#define CPUID_INTEL_TRACE               0x00000014
#define CPUID_TSC_INFO                  0x00000015
#define CPUID_PROCESSOR_FREQUENCY       0x00000016
#define CPUID_SOC_VENDOR                0x00000017
#define CPUID_HYBRID_INFO               0x0000001A
#define CPUID_PCONFIG_INFO              0x0000001B
#define CPUID_V2_EXTENDED_TOPOLOGY      0x0000001F

/* Hypervisor Leaves */
#define CPUID_HYPERVISOR_INTERFACE      0x40000000
#define CPUID_HYPERVISOR_FEATURES       0x40000001

/* Extended Leaves */
#define CPUID_EXT_MAX_FUNCTION          0x80000000
#define CPUID_EXT_FEATURES              0x80000001
#define CPUID_EXT_BRAND_STRING_1        0x80000002
#define CPUID_EXT_BRAND_STRING_2        0x80000003
#define CPUID_EXT_BRAND_STRING_3        0x80000004
#define CPUID_EXT_L1_CACHE_TLB          0x80000005
#define CPUID_EXT_L2_L3_CACHE_TLB       0x80000006
#define CPUID_EXT_ADV_POWER_MGMT        0x80000007
#define CPUID_EXT_ADDR_SIZE             0x80000008
#define CPUID_EXT_SVM_FEATURES          0x8000000A
#define CPUID_EXT_PERF_OPTIMIZATIONS    0x8000001A
#define CPUID_EXT_INSTRUCTION_SET       0x8000001B
#define CPUID_EXT_LWP                   0x8000001C
#define CPUID_EXT_CACHE_PROPERTIES      0x8000001D
#define CPUID_EXT_TOPOLOGY              0x8000001E
#define CPUID_EXT_MEM_ENCRYPTION        0x8000001F
#define CPUID_EXT_PASID_FEATURES        0x80000020
#define CPUID_EXT_SVM_FEATURES_V2       0x80000021

/* Leaf 0x01 EDX */
#define CPUID_FEAT_EDX_FPU              0
#define CPUID_FEAT_EDX_VME              1
#define CPUID_FEAT_EDX_DE               2
#define CPUID_FEAT_EDX_PSE              3
#define CPUID_FEAT_EDX_TSC              4
#define CPUID_FEAT_EDX_MSR              5
#define CPUID_FEAT_EDX_PAE              6
#define CPUID_FEAT_EDX_MCE              7
#define CPUID_FEAT_EDX_CX8              8
#define CPUID_FEAT_EDX_APIC             9
#define CPUID_FEAT_EDX_SEP              11
#define CPUID_FEAT_EDX_MTRR             12
#define CPUID_FEAT_EDX_PGE              13
#define CPUID_FEAT_EDX_MCA              14
#define CPUID_FEAT_EDX_CMOV             15
#define CPUID_FEAT_EDX_PAT              16
#define CPUID_FEAT_EDX_PSE36            17
#define CPUID_FEAT_EDX_CLFSH            19
#define CPUID_FEAT_EDX_DS               21
#define CPUID_FEAT_EDX_ACPI             22
#define CPUID_FEAT_EDX_MMX              23
#define CPUID_FEAT_EDX_FXSR             24
#define CPUID_FEAT_EDX_SSE              25
#define CPUID_FEAT_EDX_SSE2             26
#define CPUID_FEAT_EDX_SS               27
#define CPUID_FEAT_EDX_HTT              28
#define CPUID_FEAT_EDX_TM               29
#define CPUID_FEAT_EDX_PBE              31

/* Leaf 0x01 ECX */
#define CPUID_FEAT_ECX_SSE3             0
#define CPUID_FEAT_ECX_PCLMULQDQ        1
#define CPUID_FEAT_ECX_DTES64           2
#define CPUID_FEAT_ECX_MONITOR          3
#define CPUID_FEAT_ECX_DS_CPL           4
#define CPUID_FEAT_ECX_VMX              5
#define CPUID_FEAT_ECX_SMX              6
#define CPUID_FEAT_ECX_EST              7
#define CPUID_FEAT_ECX_TM2              8
#define CPUID_FEAT_ECX_SSSE3            9
#define CPUID_FEAT_ECX_CID              10
#define CPUID_FEAT_ECX_SDBG             11
#define CPUID_FEAT_ECX_FMA              12
#define CPUID_FEAT_ECX_CX16             13
#define CPUID_FEAT_ECX_XTPR             14
#define CPUID_FEAT_ECX_PDCM             15
#define CPUID_FEAT_ECX_PCID             17
#define CPUID_FEAT_ECX_DCA              18
#define CPUID_FEAT_ECX_SSE4_1           19
#define CPUID_FEAT_ECX_SSE4_2           20
#define CPUID_FEAT_ECX_X2APIC           21
#define CPUID_FEAT_ECX_MOVBE            22
#define CPUID_FEAT_ECX_POPCNT           23
#define CPUID_FEAT_ECX_TSC_DEADLINE     24
#define CPUID_FEAT_ECX_AES              25
#define CPUID_FEAT_ECX_XSAVE            26
#define CPUID_FEAT_ECX_OSXSAVE          27
#define CPUID_FEAT_ECX_AVX              28
#define CPUID_FEAT_ECX_F16C             29
#define CPUID_FEAT_ECX_RDRAND           30
#define CPUID_FEAT_ECX_HYPERVISOR       31

/* Leaf 0x07 EBX */
#define CPUID_FEAT_7_EBX_FSGSBASE       0
#define CPUID_FEAT_7_EBX_TSC_ADJUST     1
#define CPUID_FEAT_7_EBX_SGX            2
#define CPUID_FEAT_7_EBX_BMI1           3
#define CPUID_FEAT_7_EBX_HLE            4
#define CPUID_FEAT_7_EBX_AVX2           5
#define CPUID_FEAT_7_EBX_SMEP           7
#define CPUID_FEAT_7_EBX_BMI2           8
#define CPUID_FEAT_7_EBX_ERMS           9
#define CPUID_FEAT_7_EBX_INVPCID        10
#define CPUID_FEAT_7_EBX_RTM            11
#define CPUID_FEAT_7_EBX_RDT_M          12
#define CPUID_FEAT_7_EBX_MPX            14
#define CPUID_FEAT_7_EBX_RDT_A          15
#define CPUID_FEAT_7_EBX_AVX512F        16
#define CPUID_FEAT_7_EBX_AVX512DQ       17
#define CPUID_FEAT_7_EBX_RDSEED         18
#define CPUID_FEAT_7_EBX_ADX            19
#define CPUID_FEAT_7_EBX_SMAP           20
#define CPUID_FEAT_7_EBX_AVX512IFMA     21
#define CPUID_FEAT_7_EBX_CLFLUSHOPT     23
#define CPUID_FEAT_7_EBX_CLWB           24
#define CPUID_FEAT_7_EBX_INTEL_PT       25
#define CPUID_FEAT_7_EBX_AVX512PF       26
#define CPUID_FEAT_7_EBX_AVX512ER       27
#define CPUID_FEAT_7_EBX_AVX512CD       28
#define CPUID_FEAT_7_EBX_SHA            29
#define CPUID_FEAT_7_EBX_AVX512BW       30
#define CPUID_FEAT_7_EBX_AVX512VL       31

/* Leaf 0x07 ECX */
#define CPUID_FEAT_7_ECX_PREFETCHWT1    0
#define CPUID_FEAT_7_ECX_AVX512VBMI     1
#define CPUID_FEAT_7_ECX_UMIP           2
#define CPUID_FEAT_7_ECX_PKU            3
#define CPUID_FEAT_7_ECX_OSPKE          4
#define CPUID_FEAT_7_ECX_WAITPKG        5
#define CPUID_FEAT_7_ECX_AVX512VBMI2    6
#define CPUID_FEAT_7_ECX_CET_SS         7
#define CPUID_FEAT_7_ECX_GFNI           8
#define CPUID_FEAT_7_ECX_VAES           9
#define CPUID_FEAT_7_ECX_VPCLMULQDQ     10
#define CPUID_FEAT_7_ECX_AVX512VNNI     11
#define CPUID_FEAT_7_ECX_AVX512BITALG   12
#define CPUID_FEAT_7_ECX_AVX512VPOPCNTDQ 14
#define CPUID_FEAT_7_ECX_FZM            15
#define CPUID_FEAT_7_ECX_FSRS           16
#define CPUID_FEAT_7_ECX_FSRCS          17
#define CPUID_FEAT_7_ECX_PKRS           18
#define CPUID_FEAT_7_ECX_AVX512_4VNNIW  21
#define CPUID_FEAT_7_ECX_AVX512_4FMAPS  22
#define CPUID_FEAT_7_ECX_FSRM           23
#define CPUID_FEAT_7_ECX_UINTR          24
#define CPUID_FEAT_7_ECX_AVX512_VP2INTERSECT 25
#define CPUID_FEAT_7_ECX_SRBDS_CTRL     27
#define CPUID_FEAT_7_ECX_MD_CLEAR       28
#define CPUID_FEAT_7_ECX_RTM_ALWAYS_ABORT 29
#define CPUID_FEAT_7_ECX_TSX_LDTRK      30
#define CPUID_FEAT_7_ECX_SERIALIZE      31

/* Leaf 0x07 EDX */
#define CPUID_FEAT_7_EDX_AVX512_BF16    5
#define CPUID_FEAT_7_EDX_AMX_BF16       22
#define CPUID_FEAT_7_EDX_AMX_TILE       24
#define CPUID_FEAT_7_EDX_AMX_INT8       25
#define CPUID_FEAT_7_EDX_IBT            20
#define CPUID_FEAT_7_EDX_SHSTK          19
#define CPUID_FEAT_7_EDX_KL             23
#define CPUID_FEAT_7_EDX_WIDEKL         24
#define CPUID_FEAT_7_EDX_HRESET         22
#define CPUID_FEAT_7_EDX_AVX_VNNI_VINT8 23
#define CPUID_FEAT_7_EDX_AVX512_FP16    23

/* Leaf 0x80000001 ECX (AMD) */
#define CPUID_FEAT_80000001_ECX_LAHF_LM     0
#define CPUID_FEAT_80000001_ECX_CMP_LEGACY  1
#define CPUID_FEAT_80000001_ECX_SVM         2
#define CPUID_FEAT_80000001_ECX_EXT_APIC    3
#define CPUID_FEAT_80000001_ECX_CR8_LEGACY  4
#define CPUID_FEAT_80000001_ECX_LZCNT       5
#define CPUID_FEAT_80000001_ECX_SSE4A       6
#define CPUID_FEAT_80000001_ECX_MISALIGNSSE 7
#define CPUID_FEAT_80000001_ECX_3DNOWPREF  8
#define CPUID_FEAT_80000001_ECX_OSVW       9
#define CPUID_FEAT_80000001_ECX_IBS        10
#define CPUID_FEAT_80000001_ECX_XOP        11
#define CPUID_FEAT_80000001_ECX_SKINIT     12
#define CPUID_FEAT_80000001_ECX_WDT        13
#define CPUID_FEAT_80000001_ECX_LWP       15
#define CPUID_FEAT_80000001_ECX_FMA4       16
#define CPUID_FEAT_80000001_ECX_TCE        17
#define CPUID_FEAT_80000001_ECX_NODEID_MSR 19
#define CPUID_FEAT_80000001_ECX_TBM        21
#define CPUID_FEAT_80000001_ECX_TOPOEXT    22
#define CPUID_FEAT_80000001_ECX_PERFCTREXT_CORE 23
#define CPUID_FEAT_80000001_ECX_PERFCTREXT_NB 24
#define CPUID_FEAT_80000001_ECX_DBX        26
#define CPUID_FEAT_80000001_ECX_PERFTSC    27
#define CPUID_FEAT_80000001_ECX_PCX_L2I    28
#define CPUID_FEAT_80000001_ECX_MONITORX   29
#define CPUID_FEAT_80000001_ECX_ADDR_MASK_EXT 30

/* Leaf 0x80000001 EDX (AMD) */
#define CPUID_FEAT_80000001_EDX_FPU        0
#define CPUID_FEAT_80000001_EDX_VME        1
#define CPUID_FEAT_80000001_EDX_DE         2
#define CPUID_FEAT_80000001_EDX_PSE        3
#define CPUID_FEAT_80000001_EDX_TSC        4
#define CPUID_FEAT_80000001_EDX_MSR        5
#define CPUID_FEAT_80000001_EDX_PAE        6
#define CPUID_FEAT_80000001_EDX_MCE        7
#define CPUID_FEAT_80000001_EDX_CX8        8
#define CPUID_FEAT_80000001_EDX_APIC       9
#define CPUID_FEAT_80000001_EDX_SYSCALL    11
#define CPUID_FEAT_80000001_EDX_MTRR       12
#define CPUID_FEAT_80000001_EDX_PGE        13
#define CPUID_FEAT_80000001_EDX_MCA        14
#define CPUID_FEAT_80000001_EDX_CMOV       15
#define CPUID_FEAT_80000001_EDX_PAT        16
#define CPUID_FEAT_80000001_EDX_PSE36      17
#define CPUID_FEAT_80000001_EDX_NX         20
#define CPUID_FEAT_80000001_EDX_MMXEXT     22
#define CPUID_FEAT_80000001_EDX_MMX        23
#define CPUID_FEAT_80000001_EDX_FXSR       24
#define CPUID_FEAT_80000001_EDX_FFXSR      25
#define CPUID_FEAT_80000001_EDX_PAGE1GB    26
#define CPUID_FEAT_80000001_EDX_RDTSCP     27
#define CPUID_FEAT_80000001_EDX_LM         29
#define CPUID_FEAT_80000001_EDX_3DNOWEXT   30
#define CPUID_FEAT_80000001_EDX_3DNOW      31

typedef enum {
    FPU_SUPPORT = 0,
    VME_SUPPORT, DE_SUPPORT, PSE_SUPPORT, TSC_SUPPORT, MSR_SUPPORT,
    PAE_SUPPORT, MCE_SUPPORT, CMPXCHG8B_SUPPORT, APIC_SUPPORT, SEP_SUPPORT,
    MTRR_SUPPORT, PGE_SUPPORT, MCA_SUPPORT, CMOV_SUPPORT, PAT_SUPPORT,
    PSE36_SUPPORT, CLFLUSH_SUPPORT, DS_SUPPORT, ACPI_SUPPORT, MMX_SUPPORT,
    FXSR_SUPPORT, SSE_SUPPORT, SSE2_SUPPORT, SS_SUPPORT, HTT_SUPPORT,
    TM_SUPPORT, PBE_SUPPORT,

    SSE3_SUPPORT, PCLMULQDQ_SUPPORT, DTES64_SUPPORT, MONITOR_SUPPORT,
    DS_CPL_SUPPORT, VMX_SUPPORT, SMX_SUPPORT, EST_SUPPORT, TM2_SUPPORT,
    SSSE3_SUPPORT, CID_SUPPORT, SDBG_SUPPORT, FMA_SUPPORT, CMPXCHG16B_SUPPORT,
    XTPR_SUPPORT, PDCM_SUPPORT, PCID_SUPPORT, DCA_SUPPORT, SSE4_1_SUPPORT,
    SSE4_2_SUPPORT, X2APIC_SUPPORT, MOVBE_SUPPORT, POPCNT_SUPPORT,
    TSC_DEADLINE_SUPPORT, AESNI_SUPPORT, XSAVE_SUPPORT, OSXSAVE_SUPPORT,
    AVX_SUPPORT, F16C_SUPPORT, RDRAND_SUPPORT, HYPERVISOR_PRESENT,

    FSGSBASE_SUPPORT, TSC_ADJUST_SUPPORT, SGX_SUPPORT, BMI1_SUPPORT,
    HLE_SUPPORT, AVX2_SUPPORT, SMEP_SUPPORT, BMI2_SUPPORT, ERMS_SUPPORT,
    INVPCID_SUPPORT, RTM_SUPPORT, RDT_M_SUPPORT, MPX_SUPPORT, RDT_A_SUPPORT,
    AVX512F_SUPPORT, AVX512DQ_SUPPORT, RDSEED_SUPPORT, ADX_SUPPORT,
    SMAP_SUPPORT, AVX512IFMA_SUPPORT, CLFLUSHOPT_SUPPORT, CLWB_SUPPORT,
    INTEL_PT_SUPPORT, AVX512PF_SUPPORT, AVX512ER_SUPPORT, AVX512CD_SUPPORT,
    SHA_SUPPORT, AVX512BW_SUPPORT, AVX512VL_SUPPORT,

    PREFETCHWT1_SUPPORT, AVX512VBMI_SUPPORT, UMIP_SUPPORT, PKU_SUPPORT,
    OSPKE_SUPPORT, WAITPKG_SUPPORT, AVX512VBMI2_SUPPORT, CET_SS_SUPPORT,
    GFNI_SUPPORT, VAES_SUPPORT, VPCLMULQDQ_SUPPORT, AVX512VNNI_SUPPORT,
    AVX512BITALG_SUPPORT, AVX512VPOPCNTDQ_SUPPORT, FZM_SUPPORT, FSRS_SUPPORT,
    FSRCS_SUPPORT, PKRS_SUPPORT, AVX512_4VNNIW_SUPPORT, AVX512_4FMAPS_SUPPORT,
    FSRM_SUPPORT, UINTR_SUPPORT, AVX512_VP2INTERSECT_SUPPORT,
    SRBDS_CTRL_SUPPORT, MD_CLEAR_SUPPORT, RTM_ALWAYS_ABORT_SUPPORT,
    TSX_LDTRK_SUPPORT, SERIALIZE_SUPPORT,

    AVX512_BF16_SUPPORT, AMX_BF16_SUPPORT, AMX_TILE_SUPPORT, AMX_INT8_SUPPORT,
    IBT_SUPPORT, SHSTK_SUPPORT, KL_SUPPORT, WIDEKL_SUPPORT, HRESET_SUPPORT,
    AVX_VNNI_VINT8_SUPPORT, AVX512_FP16_SUPPORT,

    LAHF_LM_SUPPORT, CMP_LEGACY_SUPPORT, SVM_SUPPORT, EXT_APIC_SUPPORT,
    CR8_LEGACY_SUPPORT, LZCNT_SUPPORT, SSE4A_SUPPORT, MISALIGNSSE_SUPPORT,
    PREFETCHW_SUPPORT, OSVW_SUPPORT, IBS_SUPPORT, XOP_SUPPORT, SKINIT_SUPPORT,
    WDT_SUPPORT, LWP_SUPPORT, FMA4_SUPPORT, TCE_SUPPORT, NODEID_MSR_SUPPORT,
    TBM_SUPPORT, TOPOEXT_SUPPORT, PERFCTREXT_CORE_SUPPORT, PERFCTREXT_NB_SUPPORT,
    DBX_SUPPORT, PERFTSC_SUPPORT, PCX_L2I_SUPPORT, MONITORX_SUPPORT,
    ADDR_MASK_EXT_SUPPORT, SYSCALL_SUPPORT, NX_SUPPORT, MMXEXT_SUPPORT,
    FFXSR_SUPPORT, PAGE1GB_SUPPORT, RDTSCP_SUPPORT, LM_SUPPORT,
    THREE_DNOWEXT_SUPPORT, THREE_DNOW_SUPPORT,

    STEPPING_ID, MODEL_ID, FAMILY_ID, PROCESSOR_TYPE,
    EXTENDED_MODEL_ID, EXTENDED_FAMILY_ID,
    LOGICAL_PROCESSORS_PER_PACKAGE, INITIAL_APIC_ID,
    BASE_FREQUENCY_MHZ, MAX_FREQUENCY_MHZ, BUS_FREQUENCY_MHZ,
    PHYSICAL_ADDRESS_BITS, VIRTUAL_ADDRESS_BITS,
    PMU_VERSION, PMU_GP_COUNTERS, PMU_COUNTER_BITS,
    MAX_STANDARD_LEAF, MAX_EXTENDED_LEAF,
    IS_HYBRID, CORE_TYPE, NATIVE_MODEL_ID,

    VENDOR_STRING, BRAND_STRING, MICROARCHITECTURE,

    CPU_HAS_QUERY_COUNT
} cpu_has_query_t;

static inline int __cpuid_check_feature(uint32_t leaf, uint32_t subleaf, 
                                         int reg_idx, uint32_t bit) {
    if (!cpuid_leaf_supported(leaf)) return 0;
    cpuid_res_t res = cpuid_query(leaf, subleaf);
    uint32_t regs[4] = { res.eax, res.ebx, res.ecx, res.edx };
    return (regs[reg_idx] >> bit) & 1;
}

static inline uint32_t __cpuid_extract_field(uint32_t leaf, uint32_t subleaf,
                                              int reg_idx, int hi_bit, int lo_bit) {
    if (!cpuid_leaf_supported(leaf)) return 0;
    cpuid_res_t res = cpuid_query(leaf, subleaf);
    uint32_t regs[4] = { res.eax, res.ebx, res.ecx, res.edx };
    uint32_t mask = (1U << (hi_bit - lo_bit + 1)) - 1;
    return (regs[reg_idx] >> lo_bit) & mask;
}

static inline void __cpuid_get_vendor(char *buf) {
    cpuid_res_t res = cpuid_query(0, 0);
    uint32_t vendor[4];
    vendor[0] = res.ebx;
    vendor[1] = res.edx;
    vendor[2] = res.ecx;
    vendor[3] = 0;
    memcpy(buf, vendor, 13);
    buf[12] = '\0';
}

static inline void __cpuid_get_brand(char *buf) {
    buf[0] = '\0';
    if (!cpuid_leaf_supported(0x80000004)) return;
    cpuid_res_t r2 = cpuid_query(0x80000002, 0);
    cpuid_res_t r3 = cpuid_query(0x80000003, 0);
    cpuid_res_t r4 = cpuid_query(0x80000004, 0);
    memcpy(buf + 0,  &r2, 16);
    memcpy(buf + 16, &r3, 16);
    memcpy(buf + 32, &r4, 16);
    buf[48] = '\0';
    char *p = buf;
    while (*p == ' ') p++;
    if (p != buf) memmove(buf, p, strlen(p) + 1);
}

static inline uintptr_t cpu_has(cpu_has_query_t query) {
    static __thread char __str_buf[64];

    switch (query) {
        /* Leaf 0x01 EDX */
        case FPU_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_FPU);
        case VME_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_VME);
        case DE_SUPPORT:        return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_DE);
        case PSE_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_PSE);
        case TSC_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_TSC);
        case MSR_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_MSR);
        case PAE_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_PAE);
        case MCE_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_MCE);
        case CMPXCHG8B_SUPPORT: return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_CX8);
        case APIC_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_APIC);
        case SEP_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_SEP);
        case MTRR_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_MTRR);
        case PGE_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_PGE);
        case MCA_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_MCA);
        case CMOV_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_CMOV);
        case PAT_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_PAT);
        case PSE36_SUPPORT:     return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_PSE36);
        case CLFLUSH_SUPPORT:   return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_CLFSH);
        case DS_SUPPORT:        return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_DS);
        case ACPI_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_ACPI);
        case MMX_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_MMX);
        case FXSR_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_FXSR);
        case SSE_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_SSE);
        case SSE2_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_SSE2);
        case SS_SUPPORT:        return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_SS);
        case HTT_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_HTT);
        case TM_SUPPORT:        return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_TM);
        case PBE_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 3, CPUID_FEAT_EDX_PBE);

        /* Leaf 0x01 ECX */
        case SSE3_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_SSE3);
        case PCLMULQDQ_SUPPORT: return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_PCLMULQDQ);
        case DTES64_SUPPORT:    return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_DTES64);
        case MONITOR_SUPPORT:   return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_MONITOR);
        case DS_CPL_SUPPORT:    return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_DS_CPL);
        case VMX_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_VMX);
        case SMX_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_SMX);
        case EST_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_EST);
        case TM2_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_TM2);
        case SSSE3_SUPPORT:     return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_SSSE3);
        case CID_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_CID);
        case SDBG_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_SDBG);
        case FMA_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_FMA);
        case CMPXCHG16B_SUPPORT: return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_CX16);
        case XTPR_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_XTPR);
        case PDCM_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_PDCM);
        case PCID_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_PCID);
        case DCA_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_DCA);
        case SSE4_1_SUPPORT:    return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_SSE4_1);
        case SSE4_2_SUPPORT:    return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_SSE4_2);
        case X2APIC_SUPPORT:    return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_X2APIC);
        case MOVBE_SUPPORT:     return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_MOVBE);
        case POPCNT_SUPPORT:    return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_POPCNT);
        case TSC_DEADLINE_SUPPORT: return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_TSC_DEADLINE);
        case AESNI_SUPPORT:     return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_AES);
        case XSAVE_SUPPORT:     return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_XSAVE);
        case OSXSAVE_SUPPORT:   return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_OSXSAVE);
        case AVX_SUPPORT:       return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_AVX);
        case F16C_SUPPORT:      return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_F16C);
        case RDRAND_SUPPORT:    return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_RDRAND);
        case HYPERVISOR_PRESENT: return __cpuid_check_feature(0x00000001, 0, 2, CPUID_FEAT_ECX_HYPERVISOR);

        /* Leaf 0x07 EBX */
        case FSGSBASE_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_FSGSBASE);
        case TSC_ADJUST_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_TSC_ADJUST);
        case SGX_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_SGX);
        case BMI1_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_BMI1);
        case HLE_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_HLE);
        case AVX2_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX2);
        case SMEP_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_SMEP);
        case BMI2_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_BMI2);
        case ERMS_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_ERMS);
        case INVPCID_SUPPORT:   return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_INVPCID);
        case RTM_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_RTM);
        case RDT_M_SUPPORT:     return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_RDT_M);
        case MPX_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_MPX);
        case RDT_A_SUPPORT:     return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_RDT_A);
        case AVX512F_SUPPORT:   return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512F);
        case AVX512DQ_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512DQ);
        case RDSEED_SUPPORT:    return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_RDSEED);
        case ADX_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_ADX);
        case SMAP_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_SMAP);
        case AVX512IFMA_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512IFMA);
        case CLFLUSHOPT_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_CLFLUSHOPT);
        case CLWB_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_CLWB);
        case INTEL_PT_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_INTEL_PT);
        case AVX512PF_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512PF);
        case AVX512ER_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512ER);
        case AVX512CD_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512CD);
        case SHA_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_SHA);
        case AVX512BW_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512BW);
        case AVX512VL_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 1, CPUID_FEAT_7_EBX_AVX512VL);

        /* Leaf 0x07 ECX */
        case PREFETCHWT1_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_PREFETCHWT1);
        case AVX512VBMI_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512VBMI);
        case UMIP_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_UMIP);
        case PKU_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_PKU);
        case OSPKE_SUPPORT:     return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_OSPKE);
        case WAITPKG_SUPPORT:   return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_WAITPKG);
        case AVX512VBMI2_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512VBMI2);
        case CET_SS_SUPPORT:    return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_CET_SS);
        case GFNI_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_GFNI);
        case VAES_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_VAES);
        case VPCLMULQDQ_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_VPCLMULQDQ);
        case AVX512VNNI_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512VNNI);
        case AVX512BITALG_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512BITALG);
        case AVX512VPOPCNTDQ_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512VPOPCNTDQ);
        case FZM_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_FZM);
        case FSRS_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_FSRS);
        case FSRCS_SUPPORT:     return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_FSRCS);
        case PKRS_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_PKRS);
        case AVX512_4VNNIW_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512_4VNNIW);
        case AVX512_4FMAPS_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512_4FMAPS);
        case FSRM_SUPPORT:      return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_FSRM);
        case UINTR_SUPPORT:     return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_UINTR);
        case AVX512_VP2INTERSECT_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_AVX512_VP2INTERSECT);
        case SRBDS_CTRL_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_SRBDS_CTRL);
        case MD_CLEAR_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_MD_CLEAR);
        case RTM_ALWAYS_ABORT_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_RTM_ALWAYS_ABORT);
        case TSX_LDTRK_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_TSX_LDTRK);
        case SERIALIZE_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 2, CPUID_FEAT_7_ECX_SERIALIZE);

        /* Leaf 0x07 EDX */
        case AVX512_BF16_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_AVX512_BF16);
        case AMX_BF16_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_AMX_BF16);
        case AMX_TILE_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_AMX_TILE);
        case AMX_INT8_SUPPORT:  return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_AMX_INT8);
        case IBT_SUPPORT:       return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_IBT);
        case SHSTK_SUPPORT:     return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_SHSTK);
        case KL_SUPPORT:        return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_KL);
        case WIDEKL_SUPPORT:    return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_WIDEKL);
        case HRESET_SUPPORT:    return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_HRESET);
        case AVX_VNNI_VINT8_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_AVX_VNNI_VINT8);
        case AVX512_FP16_SUPPORT: return __cpuid_check_feature(0x00000007, 0, 3, CPUID_FEAT_7_EDX_AVX512_FP16);

        /* Leaf 0x80000001 ECX (AMD) */
        case LAHF_LM_SUPPORT:   return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_LAHF_LM);
        case CMP_LEGACY_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_CMP_LEGACY);
        case SVM_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_SVM);
        case EXT_APIC_SUPPORT:  return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_EXT_APIC);
        case CR8_LEGACY_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_CR8_LEGACY);
        case LZCNT_SUPPORT:     return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_LZCNT);
        case SSE4A_SUPPORT:     return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_SSE4A);
        case MISALIGNSSE_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_MISALIGNSSE);
        case PREFETCHW_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_3DNOWPREF);
        case OSVW_SUPPORT:      return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_OSVW);
        case IBS_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_IBS);
        case XOP_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_XOP);
        case SKINIT_SUPPORT:    return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_SKINIT);
        case WDT_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_WDT);
        case LWP_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_LWP);
        case FMA4_SUPPORT:      return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_FMA4);
        case TCE_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_TCE);
        case NODEID_MSR_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_NODEID_MSR);
        case TBM_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_TBM);
        case TOPOEXT_SUPPORT:   return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_TOPOEXT);
        case PERFCTREXT_CORE_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_PERFCTREXT_CORE);
        case PERFCTREXT_NB_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_PERFCTREXT_NB);
        case DBX_SUPPORT:       return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_DBX);
        case PERFTSC_SUPPORT:   return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_PERFTSC);
        case PCX_L2I_SUPPORT:   return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_PCX_L2I);
        case MONITORX_SUPPORT:  return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_MONITORX);
        case ADDR_MASK_EXT_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 2, CPUID_FEAT_80000001_ECX_ADDR_MASK_EXT);

        /* Leaf 0x80000001 EDX (AMD) */
        case SYSCALL_SUPPORT:   return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_SYSCALL);
        case NX_SUPPORT:        return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_NX);
        case MMXEXT_SUPPORT:    return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_MMXEXT);
        case FFXSR_SUPPORT:     return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_FFXSR);
        case PAGE1GB_SUPPORT:   return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_PAGE1GB);
        case RDTSCP_SUPPORT:    return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_RDTSCP);
        case LM_SUPPORT:        return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_LM);
        case THREE_DNOWEXT_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_3DNOWEXT);
        case THREE_DNOW_SUPPORT: return __cpuid_check_feature(0x80000001, 0, 3, CPUID_FEAT_80000001_EDX_3DNOW);

        case STEPPING_ID:
            return __cpuid_extract_field(0x00000001, 0, 0, 3, 0);
        case MODEL_ID:
            return __cpuid_extract_field(0x00000001, 0, 0, 7, 4);
        case FAMILY_ID:
            return __cpuid_extract_field(0x00000001, 0, 0, 11, 8);
        case PROCESSOR_TYPE:
            return __cpuid_extract_field(0x00000001, 0, 0, 13, 12);
        case EXTENDED_MODEL_ID:
            return __cpuid_extract_field(0x00000001, 0, 0, 19, 16);
        case EXTENDED_FAMILY_ID:
            return __cpuid_extract_field(0x00000001, 0, 0, 27, 20);

        case LOGICAL_PROCESSORS_PER_PACKAGE:
            return __cpuid_extract_field(0x00000001, 0, 1, 23, 16);
        case INITIAL_APIC_ID:
            return __cpuid_extract_field(0x00000001, 0, 1, 31, 24);

        case BASE_FREQUENCY_MHZ:
            if (!cpuid_leaf_supported(0x00000016)) return 0;
            return __cpuid_extract_field(0x00000016, 0, 0, 15, 0);
        case MAX_FREQUENCY_MHZ:
            if (!cpuid_leaf_supported(0x00000016)) return 0;
            return __cpuid_extract_field(0x00000016, 0, 1, 15, 0);
        case BUS_FREQUENCY_MHZ:
            if (!cpuid_leaf_supported(0x00000016)) return 0;
            return __cpuid_extract_field(0x00000016, 0, 2, 15, 0);

        case PHYSICAL_ADDRESS_BITS:
            if (!cpuid_leaf_supported(0x80000008)) return 36;
            return __cpuid_extract_field(0x80000008, 0, 0, 7, 0);
        case VIRTUAL_ADDRESS_BITS:
            if (!cpuid_leaf_supported(0x80000008)) return 32;
            return __cpuid_extract_field(0x80000008, 0, 0, 15, 8);

        case PMU_VERSION:
            if (!cpuid_leaf_supported(0x0000000A)) return 0;
            return __cpuid_extract_field(0x0000000A, 0, 0, 7, 0);
        case PMU_GP_COUNTERS:
            if (!cpuid_leaf_supported(0x0000000A)) return 0;
            return __cpuid_extract_field(0x0000000A, 0, 0, 15, 8);
        case PMU_COUNTER_BITS:
            if (!cpuid_leaf_supported(0x0000000A)) return 0;
            return __cpuid_extract_field(0x0000000A, 0, 0, 23, 16);

        case MAX_STANDARD_LEAF:
            return cpuid_query(0, 0).eax;
        case MAX_EXTENDED_LEAF:
            return cpuid_query(0x80000000, 0).eax;

        case IS_HYBRID:
            if (!cpuid_leaf_supported(0x0000001A)) return 0;
            return cpuid_query(0x0000001A, 0).eax != 0;
        case CORE_TYPE:
            if (!cpuid_leaf_supported(0x0000001A)) return 0;
            return __cpuid_extract_field(0x0000001A, 0, 0, 31, 24);
        case NATIVE_MODEL_ID:
            if (!cpuid_leaf_supported(0x0000001A)) return 0;
            return __cpuid_extract_field(0x0000001A, 0, 0, 23, 0);

        case VENDOR_STRING: {
            __cpuid_get_vendor(__str_buf);
            return (uintptr_t)__str_buf;
        }

        case BRAND_STRING: {
            __cpuid_get_brand(__str_buf);
            return (uintptr_t)__str_buf;
        }

        case MICROARCHITECTURE: {
            __cpuid_get_vendor(__str_buf);
            if (strcmp(__str_buf, "GenuineIntel") == 0) {
                int family = cpu_has(FAMILY_ID);
                int model = cpu_has(MODEL_ID);
                int ext_model = cpu_has(EXTENDED_MODEL_ID);
                int display_model = (family == 0x06 || family == 0x0F) ? 
                                    ((ext_model << 4) | model) : model;
                if (family == 0x06) {
                    switch (display_model) {
                        case 0x1C: case 0x26: case 0x27: case 0x35: case 0x36:
                            strcpy(__str_buf, "Bonnell (Atom)"); break;
                        case 0x37: case 0x4A: case 0x4D: case 0x5A: case 0x5D:
                            strcpy(__str_buf, "Silvermont (Atom)"); break;
                        case 0x4C: case 0x5C: case 0x75:
                            strcpy(__str_buf, "Airmont (Atom)"); break;
                        case 0x5F: case 0x7A:
                            strcpy(__str_buf, "Goldmont (Atom)"); break;
                        case 0x86: case 0x96: case 0x9C:
                            strcpy(__str_buf, "Tremont (Atom)"); break;
                        case 0xBE:
                            strcpy(__str_buf, "Gracemont (Atom)"); break;
                        case 0x1A: case 0x1E: case 0x1F: case 0x2E:
                            strcpy(__str_buf, "Nehalem"); break;
                        case 0x25: case 0x2C: case 0x2F:
                            strcpy(__str_buf, "Westmere"); break;
                        case 0x2A: case 0x2D:
                            strcpy(__str_buf, "Sandy Bridge"); break;
                        case 0x3A: case 0x3E:
                            strcpy(__str_buf, "Ivy Bridge"); break;
                        case 0x3C: case 0x3F: case 0x45: case 0x46:
                            strcpy(__str_buf, "Haswell"); break;
                        case 0x3D: case 0x47: case 0x4F: case 0x56:
                            strcpy(__str_buf, "Broadwell"); break;
                        case 0x4E: case 0x5E: case 0x55:
                            strcpy(__str_buf, "Skylake"); break;
                        case 0x8E: case 0x9E:
                            strcpy(__str_buf, "Kaby Lake / Coffee Lake"); break;
                        case 0xA5: case 0xA6:
                            strcpy(__str_buf, "Comet Lake"); break;
                        case 0x7D: case 0x7E:
                            strcpy(__str_buf, "Ice Lake"); break;
                        case 0x8C: case 0x8D:
                            strcpy(__str_buf, "Tiger Lake"); break;
                        case 0x97: case 0x9A: case 0xB7:
                            strcpy(__str_buf, "Alder Lake"); break;
                        case 0xBA: case 0xBF:
                            strcpy(__str_buf, "Raptor Lake"); break;
                        case 0xAD: case 0xAE:
                            strcpy(__str_buf, "Meteor Lake"); break;
                        case 0xB5:
                            strcpy(__str_buf, "Lunar Lake"); break;
                        case 0x57: case 0x85:
                            strcpy(__str_buf, "Knights Landing/Mill (Xeon Phi)"); break;
                        default:
                            strcpy(__str_buf, "Unknown Intel"); break;
                    }
                } else if (family == 0x0F) {
                    strcpy(__str_buf, "NetBurst (Pentium 4)");
                } else {
                    strcpy(__str_buf, "Unknown Intel");
                }
            } else if (strcmp(__str_buf, "AuthenticAMD") == 0) {
                int family = cpu_has(FAMILY_ID);
                int ext_family = cpu_has(EXTENDED_FAMILY_ID);
                int display_family = family + ext_family;
                if (display_family >= 0x19) {
                    strcpy(__str_buf, "Zen 4 / Zen 5 (AMD)");
                } else if (display_family == 0x18) {
                    strcpy(__str_buf, "Hygon Dhyana (AMD-compatible)");
                } else if (display_family == 0x17) {
                    strcpy(__str_buf, "Zen / Zen+ / Zen 2 (AMD)");
                } else if (display_family == 0x15) {
                    strcpy(__str_buf, "Bulldozer / Piledriver / Steamroller / Excavator (AMD)");
                } else if (display_family == 0x10) {
                    strcpy(__str_buf, "K10 (AMD)");
                } else if (display_family == 0x0F) {
                    strcpy(__str_buf, "K8 (AMD)");
                } else {
                    strcpy(__str_buf, "Unknown AMD");
                }
            } else {
                strcpy(__str_buf, "Unknown Vendor");
            }
            return (uintptr_t)__str_buf;
        }

        default:
            return 0;
    }
}

static inline bool cpuid_enum_subleaf(uint32_t leaf, uint32_t *subleaf, cpuid_res_t *out) {
    if (!cpuid_leaf_supported(leaf)) return false;

    *out = cpuid_query(leaf, *subleaf);

    switch (leaf) {
        case CPUID_DETERMINISTIC_CACHE:
        case CPUID_EXT_CACHE_PROPERTIES:
            /* EAX[4:0] == 0 means invalid cache */
            if ((*out).eax & 0x1F) { (*subleaf)++; return true; }
            return false;

        case CPUID_EXTENDED_TOPOLOGY:
        case CPUID_V2_EXTENDED_TOPOLOGY:
            /* ECX[15:8] == 0 means invalid level */
            if (((*out).ecx >> 8) & 0xFF) { (*subleaf)++; return true; }
            return false;

        case CPUID_XSAVE_FEATURES:
            /* Subleafs 0-63 based on XCR0 bits */
            if (*subleaf < 64) { (*subleaf)++; return true; }
            return false;

        case CPUID_STRUCTURED_FEATURES:
            /* Subleaf 0 returns max subleaf in EAX */
            if (*subleaf == 0) {
                uint32_t max = (*out).eax;
                (*subleaf)++;
                return true;
            }
            if (*subleaf <= (*out).eax) { (*subleaf)++; return true; }
            return false;

        case CPUID_SGX_CAPABILITIES:
            /* Subleafs 0-2 */
            if (*subleaf < 3) { (*subleaf)++; return true; }
            return false;

        case CPUID_INTEL_TRACE:
            /* Subleafs 0-1 */
            if (*subleaf < 2) { (*subleaf)++; return true; }
            return false;

        case CPUID_SOC_VENDOR:
            /* Subleaf 0 returns max subleaf */
            if (*subleaf == 0) {
                uint32_t max = (*out).eax;
                (*subleaf)++;
                return true;
            }
            if (*subleaf <= (*out).eax) { (*subleaf)++; return true; }
            return false;

        default:
            /* Generic: single subleaf only */
            if (*subleaf == 0) { (*subleaf)++; return true; }
            return false;
    }
}

static inline uint32_t cpuid_display_family(void) {
    int family = cpu_has(FAMILY_ID);
    int ext_family = cpu_has(EXTENDED_FAMILY_ID);
    return (family == 0x0F) ? family + ext_family : family;
}

static inline uint32_t cpuid_display_model(void) {
    int family = cpu_has(FAMILY_ID);
    int model = cpu_has(MODEL_ID);
    int ext_model = cpu_has(EXTENDED_MODEL_ID);
    return ((family == 0x06 || family == 0x0F) ? ((ext_model << 4) | model) : model);
}

static inline bool cpuid_hypervisor_vendor(char *buf) {
    if (!cpu_has(HYPERVISOR_PRESENT)) {
        buf[0] = '\0';
        return false;
    }
    cpuid_res_t res = cpuid_query(CPUID_HYPERVISOR_INTERFACE, 0);
    uint32_t vendor[4];
    vendor[0] = res.ebx;
    vendor[1] = res.ecx;
    vendor[2] = res.edx;
    vendor[3] = 0;
    memcpy(buf, vendor, 13);
    buf[12] = '\0';
    return true;
}

static inline bool cpuid_cache_info(uint32_t subleaf, uint32_t *level, 
                                     uint32_t *type, uint32_t *size_kb) {
    if (!cpuid_leaf_supported(CPUID_DETERMINISTIC_CACHE)) return false;
    cpuid_res_t res = cpuid_query(CPUID_DETERMINISTIC_CACHE, subleaf);

    uint32_t cache_type = res.eax & 0x1F;
    if (!cache_type) return false;

    *type = cache_type;
    *level = (res.eax >> 5) & 0x7;

    uint32_t line_size = (res.ebx & 0xFFF) + 1;
    uint32_t partitions = ((res.ebx >> 12) & 0x3FF) + 1;
    uint32_t ways = ((res.ebx >> 22) & 0x3FF) + 1;
    uint32_t sets = res.ecx + 1;

    *size_kb = (ways * partitions * line_size * sets) / 1024;
    return true;
}

static inline uint32_t cpuid_cache_shared_threads(uint32_t subleaf) {
    if (!cpuid_leaf_supported(CPUID_DETERMINISTIC_CACHE)) return 0;
    cpuid_res_t res = cpuid_query(CPUID_DETERMINISTIC_CACHE, subleaf);
    return ((res.eax >> 14) & 0xFFF) + 1;
}

static inline uint32_t cpuid_num_caches(void) {
    uint32_t count = 0;
    cpuid_res_t res;
    uint32_t sub = 0;
    while (cpuid_enum_subleaf(CPUID_DETERMINISTIC_CACHE, &sub, &res)) {
        count++;
    }
    return count;
}

static inline bool cpuid_xsave_component_supported(uint32_t component) {
    if (component > 63) return false;
    if (!cpuid_leaf_supported(CPUID_XSAVE_FEATURES)) return false;
    cpuid_res_t res = cpuid_query(CPUID_XSAVE_FEATURES, component);
    return (res.eax != 0) || (res.ebx != 0);
}

static inline uint32_t cpuid_xsave_component_size(uint32_t component) {
    if (component > 63) return 0;
    if (!cpuid_leaf_supported(CPUID_XSAVE_FEATURES)) return 0;
    cpuid_res_t res = cpuid_query(CPUID_XSAVE_FEATURES, component);
    return res.eax;
}

static inline uint32_t cpuid_xsave_component_offset(uint32_t component) {
    if (component > 63) return 0;
    if (!cpuid_leaf_supported(CPUID_XSAVE_FEATURES)) return 0;
    cpuid_res_t res = cpuid_query(CPUID_XSAVE_FEATURES, component);
    return res.ebx;
}

static inline bool cpuid_rdt_monitoring_supported(uint32_t resource) {
    if (!cpuid_leaf_supported(CPUID_RDT_MONITORING)) return false;
    cpuid_res_t res = cpuid_query(CPUID_RDT_MONITORING, resource);
    return (res.edx & 0x1F) != 0;
}

static inline bool cpuid_rdt_allocation_supported(uint32_t resource) {
    if (!cpuid_leaf_supported(CPUID_RDT_ALLOCATION)) return false;
    cpuid_res_t res = cpuid_query(CPUID_RDT_ALLOCATION, resource);
    return (res.eax & 0x1F) != 0;
}

static inline void cpuid_sgx_info(bool *sgx1, bool *sgx2, 
                                   uint32_t *max_enclave_64,
                                   uint32_t *max_enclave_32) {
    *sgx1 = false; *sgx2 = false;
    *max_enclave_64 = 0; *max_enclave_32 = 0;
    if (!cpuid_leaf_supported(CPUID_SGX_CAPABILITIES)) return;

    cpuid_res_t res = cpuid_query(CPUID_SGX_CAPABILITIES, 0);
    *sgx1 = (res.eax & 1) != 0;
    *sgx2 = (res.eax & 2) != 0;
    *max_enclave_64 = (res.edx >> 0) & 0xFF;
    *max_enclave_32 = (res.edx >> 8) & 0xFF;
}

static inline uint64_t cpuid_tsc_frequency_hz(void) {
    if (!cpuid_leaf_supported(CPUID_TSC_INFO)) return 0;
    cpuid_res_t res = cpuid_query(CPUID_TSC_INFO, 0);

    uint32_t denom = res.eax;
    uint32_t numer = res.ebx;
    uint32_t crystal_hz = res.ecx;

    if (!denom || !numer) return 0;
    if (crystal_hz) {
        return ((uint64_t)crystal_hz * numer) / denom;
    }
    /* Known crystal frequencies */
    if (cpu_has(VENDOR_STRING) && strcmp((char*)cpu_has(VENDOR_STRING), "GenuineIntel") == 0) {
        /* Intel typically uses 24 MHz crystal */
        return ((uint64_t)24000000 * numer) / denom;
    }
    return 0;
}

static inline bool cpuid_topology_level(uint32_t subleaf, uint32_t *level_type,
                                         uint32_t *num_logical) {
    if (!cpuid_leaf_supported(CPUID_EXTENDED_TOPOLOGY)) return false;
    cpuid_res_t res = cpuid_query(CPUID_EXTENDED_TOPOLOGY, subleaf);

    uint32_t type = (res.ecx >> 8) & 0xFF;
    if (!type) return false;

    *level_type = type;
    *num_logical = res.ebx & 0xFFFF;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* CPUID_H */
