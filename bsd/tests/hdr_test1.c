#include <sys/cdefs.h>
#include <sys/cdefs_elf.h>
#include <sys/common_ansi.h>
#include <sys/common_int_const.h>
#include <sys/common_int_fmtio.h>
#include <sys/common_int_limits.h>
#include <sys/common_int_mwgwtypes.h>
#include <sys/common_int_types.h>
#include <sys/featuretest.h>

#include <sys/ansi.h>
#include <sys/null.h>
#include <sys/stdarg.h>
#include <sys/stddef.h>
#include <sys/stdbool.h>
#include <sys/stdalign.h>
#include <sys/stdint.h>
#include <sys/inttypes.h>
#include <sys/types.h>
#include <sys/fd_set.h>

#include <sys/bswap.h>
#include <sys/endian.h>

#include <sys/queue.h>
#include <sys/tree.h>

#include <sys/systm.h>

#include <err.h>

#if defined(__x86_64__)

#include <machine/ansi.h>
#include <machine/cdefs.h>
#include <machine/types.h>
#include <machine/int_const.h>
#include <machine/int_fmtio.h>
#include <machine/int_limits.h>
#include <machine/int_mwgwtypes.h>
#include <machine/int_types.h>
#include <machine/wchar_limits.h>
#include <machine/bswap.h>
#include <machine/byte_swap.h>
#include <machine/endian_machdep.h>

#elif defined(__aarch64__)

#include <machine/ansi.h>
#include <machine/cdefs.h>
#include <machine/int_const.h>
#include <machine/int_fmtio.h>
#include <machine/int_limits.h>
#include <machine/int_mwgwtypes.h>
#include <machine/int_types.h>
#include <machine/wchar_limits.h>
#include <machine/bswap.h>
#include <machine/byte_swap.h>

#include <arm/cdefs.h>
#include <arm/types.h>
#include <arm/int_const.h>
#include <arm/int_fmtio.h>
#include <arm/int_limits.h>
#include <arm/int_mwgwtypes.h>
#include <arm/int_types.h>
#include <arm/wchar_limits.h>
#include <arm/bswap.h>
#include <arm/byte_swap.h>

#else
#error "Unsupported architecture for BSD header test"
#endif

int hdr_all_test(void) {
    return 0;
}
