DEVICES := $(wildcard $(src)/*/main.c)

$(obj)/devfs.c: $(DEVICES) $(srctree)/scripts/basic/devcom
	@echo "  DEVGEN  $@"
	$(Q)$(srctree)/scripts/basic/devcom $(srctree)/kernel/vfs/devfs $@

clean-files += devfs.c
obj-y += devfs.o
obj-y += evdev/
