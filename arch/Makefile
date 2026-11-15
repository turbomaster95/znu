KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -Wno-unused-but-set-variable
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/lib/uacpi/include -I$(srctree)/drivers/include

export KBUILD_CPPFLAGS KBUILD_CFLAGS

obj-y += $(ARCH)/
