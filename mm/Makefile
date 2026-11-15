KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -Wno-unused-but-set-variable
ifeq ($(ARCH),x86)
KBUILD_CFLAGS += -mno-red-zone
endif
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/$(ARCH)/include -I$(srctree)/lib/flanterm/src
KBUILD_CPPFLAGS += -I$(srctree)/lib/uacpi/include -I$(srctree)/drivers/include

export KBUILD_CPPFLAGS KBUILD_CFLAGS

ifeq ($(ARCH),x86)
obj-y += vmm.o
obj-y += mappage.o
obj-y += unmappage.o
endif
obj-y += pmm.o
obj-y += slab.o
