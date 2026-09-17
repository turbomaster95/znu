NODES := $(wildcard $(src)/*/main.c)

$(obj)/procfs.c: $(NODES) $(srctree)/scripts/basic/procom
	@echo "  PROCGEN $@"
	$(Q)$(srctree)/scripts/basic/procom $(srctree)/kernel/vfs/procfs $@

clean-files += procfs.c
obj-y += procfs.o

$(obj)/procfs.o: $(obj)/configd

targets += configd

export KCONFIG_CONFIG
filechk_cat = cat $(KCONFIG_CONFIG)

$(obj)/configd: $(KCONFIG_CONFIG) FORCE
	$(call filechk,cat)
