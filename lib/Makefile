#lib-$(CONFIG_ENABLE_LOGGING) += log_print.o
#lib-$(CONFIG_DEBUG_LIST)     += list_debug.o
KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone -Wno-unused-but-set-variable
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include -I$(srctree)/arch/x86/include -I$(srctree)/lib/flanterm/src
KBUILD_CPPFLAGS += -I$(srctree)/lib/uacpi/include

export KBUILD_CPPFLAGS KBUILD_CFLAGS

quiet_cmd_run_uacpi  = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_uacpi  = /bin/bash $(srctree)/scripts/uacpi.sh $(srctree)

quiet_cmd_run_flanterm  = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_flanterm  = /bin/bash $(srctree)/scripts/mkflanterm.sh $(srctree)

obj-y += libc/
CFLAGS_ff.o := -I$(srctree)/include
obj-y += fatfs/source/ff.o
obj-y += fatfs/source/ffunicode.o
obj-y += uacpi/uacpi_lib.o
obj-y += flanterm/flanterm_lib.o

$(obj)/uacpi/uacpi_lib.o: $(srctree)/scripts/uacpi.sh
	$(call if_changed,run_uacpi)

$(obj)/flanterm/flanterm_lib.o: $(srctree)/scripts/mkflanterm.sh
	$(call if_changed,run_flanterm)
