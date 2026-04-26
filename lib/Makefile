#lib-$(CONFIG_ENABLE_LOGGING) += log_print.o
#lib-$(CONFIG_DEBUG_LIST)     += list_debug.o
KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone -Wno-unused-but-set-variable
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/x86/include

export KBUILD_CPPFLAGS KBUILD_CFLAGS

obj-y += libc/

KBUILD_CPPFLAGS += -I$(srctree)/lib/uacpi/include
obj-y += uacpi/uacpi_lib.o

$(obj)/uacpi/uacpi_lib.o: $(srctree)/scripts/uacpi.sh
	$(call if_changed,run_uacpi)

quiet_cmd_run_uacpi  = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_uacpi  = /bin/bash $(srctree)/scripts/uacpi.sh $(srctree)

