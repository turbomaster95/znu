KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone -Wno-return-type

KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/x86/include -I$(srctree)/lib/uacpi/include/
KBUILD_CPPFLAGS += -DUACPI_OVERRIDE_LIBC -DUACPI_NATIVE_ALLOC_ZEROED -DUACPI_FORMATTED_LOGGING

KBUILD_LDFLAGS := -T $(srctree)/scripts/linker.ld
export KBUILD_LDFLAGS

obj-y := main.o
obj-y += gui.o
obj-y += acpi.o
obj-y += keyboard.o
obj-y += sched.o
obj-y += metadata.o
obj-y += lz4.o
obj-y += cmdline.o
