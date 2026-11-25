quiet_cmd_run_tendril  = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_tendril  = /bin/bash $(srctree)/drivers/external/scripts/mktendril.sh $(srctree) "$(KBUILD_CFLAGS)"

quiet_cmd_run_znet     = SH      $(patsubst $(srctree)/%,%,$<)
      cmd_run_znet     = /bin/bash $(srctree)/drivers/external/scripts/mkznet.sh $(srctree) "$(KBUILD_CFLAGS)"

obj-y += tendril/tendril_lib.o
obj-y += znet/znet_lib.o

$(obj)/tendril/tendril_lib.o: $(srctree)/drivers/external/scripts/mktendril.sh
	$(call if_changed,run_tendril)

$(obj)/znet/znet_lib.o: $(srctree)/drivers/external/scripts/mkznet.sh
	$(call if_changed,run_znet)
