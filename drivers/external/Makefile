quiet_cmd_run_tendril  = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_tendril  = /bin/bash $(srctree)/drivers/external/scripts/mktendril.sh $(srctree) "$(KBUILD_CFLAGS)"

quiet_cmd_run_lwip     = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_lwip     = /bin/bash $(srctree)/drivers/external/scripts/mklwip.sh $(srctree) "$(KBUILD_CFLAGS)"

obj-y += tendril/tendril_lib.o
obj-y += lwip/lwip_lib.o

$(obj)/tendril/tendril_lib.o: $(srctree)/drivers/external/scripts/mktendril.sh
	$(call if_changed,run_tendril)

$(obj)/lwip/lwip_lib.o: $(srctree)/drivers/external/scripts/mklwip.sh
	$(call if_changed,run_lwip)
