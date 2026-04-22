#lib-$(CONFIG_ENABLE_LOGGING) += log_print.o
#lib-$(CONFIG_DEBUG_LIST)     += list_debug.o
KBUILD_CFLAGS += -ffreestanding -nostdinc -isystem $(shell $(CC) -print-file-name=include)
KBUILD_CFLAGS += -mcmodel=kernel -mno-red-zone

# 2. Add your local libc and kernel headers to the search path
KBUILD_CPPFLAGS += -I$(srctree)/include -I$(srctree)/lib/libc/include

export KBUILD_CPPFLAGS KBUILD_CFLAGS

obj-y += libc/

KBUILD_CPPFLAGS += -I$(srctree)/lib/acpica/source/include
# Define the object so Kbuild includes it in the link stage
obj-y += acpica_lib.o

# Tell Kbuild how to 'create' acpica_lib.o
$(obj)/acpica_lib.o: $(srctree)/scripts/acpica.sh
	$(call if_changed,run_acpica)

# The actual command definition
quiet_cmd_run_acpica = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_acpica = /bin/bash $(srctree)/scripts/acpica.sh $@

