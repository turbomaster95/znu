DEVICES := $(wildcard $(src)/*/main.c)

$(obj)/devfs.c: $(DEVICES)
	@echo "  DEVGEN  $@"
	$(Q)$(srctree)/scripts/basic/devcom $(srctree)/kernel/vfs/devfs $@

obj-y += devfs.o
