KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += 
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/$(ARCH)/include -I$(srctree)/lib/uacpi/include/ -I$(srctree)/drivers/include  -I$(srctree)/drivers/external/include

obj-y += fb/
obj-y += net/
obj-y += hw/
obj-y += block/
obj-y += misc/
obj-y += external/
