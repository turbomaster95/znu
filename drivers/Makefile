KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/x86/include -I$(srctree)/lib/uacpi/include/ -I$(srctree)/drivers/include

obj-y += fb/
obj-y += net/

