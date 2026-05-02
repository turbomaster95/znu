# user/test_shell/Makefile
extra-y += shell.elf
obj-y := main.o

KBUILD_CPPFLAGS := -I$(srctree)/lib/ulibc/include
LINUXINCLUDE    :=
KBUILD_CPPFLAGS += -Wno-builtin-declaration-mismatch

ULIBC_LIB := $(srctree)/lib/ulibc/built-in.o

always-y += shell.elf

quiet_cmd_znu_ld = LD      $@
      cmd_znu_ld = $(CC) -static -nostdlib -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -T $(srctree)/init/linker.ld $(obj)/main.o $(ULIBC_LIB) -o $@

$(obj)/shell.elf: $(obj)/main.o $(ULIBC_LIB) FORCE
	$(call if_changed,znu_ld)
	@mkdir -p $(srctree)/configs/sysroot/bin/
	@cp $(obj)/shell.elf $(srctree)/configs/sysroot/bin/test_shell

$(obj)/%.o: $(src)/%.c FORCE
	$(CC) -c $(CFLAGS) $(KBUILD_CFLAGS) $< -o $@

$(ULIBC_LIB): ;
