extra-y += init.elf
obj-y := main.o

KBUILD_CPPFLAGS := -I$(srctree)/lib/ulibc/include -I$(srctree)/lib/ulibc/third-party/zline
LINUXINCLUDE    :=
KBUILD_CPPFLAGS += -Wno-builtin-declaration-mismatch

ULIBC_LIB := $(srctree)/lib/ulibc/built-in.a
always-y += init.elf

quiet_cmd_znu_ld = LD      $@
      cmd_znu_ld = $(CC) $(USER_CFLAGS) -static -nostdlib \
                     -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments \
                     -T $(srctree)/init/linker.ld \
                     -Wl,--start-group $< $(ULIBC_LIB) -Wl,--end-group \
                     -o $@

$(obj)/init.elf: $(addprefix $(obj)/,$(obj-y)) $(ULIBC_LIB) FORCE
	$(call if_changed,znu_ld)
	@mkdir -p $(srctree)/configs/sysroot/bin/
	@cp $(obj)/init.elf $(srctree)/configs/sysroot/bin/init

$(obj)/%.o: $(src)/%.c FORCE
	$(CC) -c $(CFLAGS) $(KBUILD_CFLAGS) $< -o $@

$(ULIBC_LIB): ;
