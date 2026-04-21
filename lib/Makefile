#lib-$(CONFIG_ENABLE_LOGGING) += log_print.o
#lib-$(CONFIG_DEBUG_LIST)     += list_debug.o
KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone

# 2. Add your local libc and kernel headers to the search path
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include

export KBUILD_CPPFLAGS KBUILD_CFLAGS

obj-y += libc/
