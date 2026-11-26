#include <sys/cdefs.h>
#include <sys/stdint.h>
#include <sys/inttypes.h>
#include <machine/cdefs.h>

int main(void) {
    int64_t value = INT64_C(123456789);
    uint64_t unsigned_value = UINT64_C(123456789);
    uintptr_t address = 0;

    (void)value;
    (void)unsigned_value;
    (void)address;

    return 0;
}
