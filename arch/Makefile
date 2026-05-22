KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone -Wno-unused-but-set-variable
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/lib/uacpi/include/

export KBUILD_CPPFLAGS KBUILD_CFLAGS

obj-y += x86/
