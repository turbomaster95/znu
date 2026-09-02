KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone -Wno-unused-but-set-variable
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/$(ARCH)/include -I$(srctree)/lib/flanterm/src
KBUILD_CPPFLAGS += -I$(srctree)/lib/uacpi/include -I$(srctree)/drivers/include

export KBUILD_CPPFLAGS KBUILD_CFLAGS

obj-y += mappage.o
obj-y += pmm.o
obj-y += slab.o
obj-y += vmm.o
obj-y += unmappage.o
