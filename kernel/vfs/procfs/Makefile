NODES := $(wildcard $(src)/*/main.c)

$(obj)/procfs.c: $(NODES)
	@echo "  PROCGEN  $@"
	$(Q)$(srctree)/scripts/basic/procom $(srctree)/kernel/vfs/procfs $@

clean-files += procfs.c
obj-y += procfs.o

$(obj)/procfs.o: $(obj)/configd.gz

targets += configd configd.gz
$(obj)/configd.gz: $(obj)/configd FORCE
	$(call if_changed,gzip)

export KCONFIG_CONFIG
filechk_cat = cat $(KCONFIG_CONFIG)

$(obj)/configd: $(KCONFIG_CONFIG) FORCE
	$(call filechk,cat)
