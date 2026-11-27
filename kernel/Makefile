KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -Wno-return-type -Wno-format-security -fsanitize=undefined -fno-sanitize-recover=all
ifeq ($(ARCH),x86)
KBUILD_CFLAGS += -mno-red-zone
endif

KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/$(ARCH)/include -I$(srctree)/lib/uacpi/include/ -I$(srctree)/drivers/include -I$(srctree)/drivers/external/include -I$(srctree)/drivers/external/tendril/include
KBUILD_CPPFLAGS += -DUACPI_OVERRIDE_LIBC -DUACPI_NATIVE_ALLOC_ZEROED -DUACPI_FORMATTED_LOGGING

KBUILD_LDFLAGS := -T $(LINKER_SCRIPT) -z max-page-size=0x1000
export KBUILD_LDFLAGS

obj-y := early.o
obj-y += main.o
obj-y += ubsan.o
ifeq ($(ARCH),x86)
   obj-y += acpi.o
endif
obj-y += keyboard.o
obj-y += sched.o
obj-y += dispatcher.o
obj-y += nifties.o
obj-y += kthread.o
obj-y += metadata.o
obj-y += lz4.o
obj-y += cmdline.o
obj-y += serial.o
obj-y += image.o
obj-y += stack_guard.o
obj-y += tty.o
obj-y += disk.o
obj-y += symbols.o
obj-y += ksyms.o
obj-$(CONFIG_SMP) += smp.o
obj-$(CONFIG_SMP) += smplog.o
obj-$(CONFIG_SMP) += mailbox.o
obj-y += signal.o
CFLAGS_diskio.o += -I$(srctree)/lib/fatfs/source
obj-y += diskio.o
obj-$(CONFIG_KTEST) += tests.o
obj-y += initcall.o

CFLAGS_netdemo.o += -I$(srctree)/drivers/external/znet/include
obj-y += netdemo.o

# Folders
obj-y += vfs/
obj-$(CONFIG_ZFILTER) += filters/

# DevKit
obj-y += cxxstd.o
obj-y += devkit/
