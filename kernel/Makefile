KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone

KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/x86/include

obj-y := main.o
obj-y += gui.o
