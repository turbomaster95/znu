#include <znu/offsets.h>
#include <proc.h>

int main(void) {
    OFFSET_T(KSTACK_TOP, process_t, kstack_top);

    return 0;
}   
