quiet_cmd_run_tendril  = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_tendril  = /bin/bash $(srctree)/scripts/mktendril.sh $(srctree) "$(ARCH_CFLAGS)"

obj-y += tendril/tendril_lib.o

$(obj)/tendril/tendril_lib.o: $(srctree)/scripts/mktendril.sh
	$(call if_changed,run_tendril)
