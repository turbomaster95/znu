KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone -fno-pic -fno-pie

KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/x86/include -I$(srctree)/lib/uacpi/include/
KBUILD_CPPFLAGS += -DACPI_MACHINE_WIDTH=64 -D_GNU_EFI -DACPI_USE_LOCAL_CACHE -DACPI_USE_STANDARD_HEADERS

obj-y := main.o
obj-y += gui.o
obj-y += acpi.o
