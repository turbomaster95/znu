# This is a skeleton from which you can build user applications for znu
# Copy this folder and rename to the name that you want your user app to be named and then,
# simply change the BINNAME variable to the name of the folder which this Makefile is currently in.

BINNAME := skeleton

extra-y += $(BINNAME).elf
obj-y := main.o

KBUILD_CPPFLAGS := -I$(srctree)/lib/ulibc/include
LINUXINCLUDE    :=
KBUILD_CPPFLAGS += -Wno-builtin-declaration-mismatch

ULIBC_LIB := $(srctree)/lib/ulibc/built-in.o

always-y += $(BINNAME).elf

quiet_cmd_znu_ld = LD      $@
      cmd_znu_ld = $(CC) -static -nostdlib -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -T $(srctree)/init/linker.ld $(obj)/main.o $(ULIBC_LIB) -o $@

$(obj)/$(BINNAME).elf: $(obj)/main.o $(ULIBC_LIB) FORCE
	$(call if_changed,znu_ld)
	@mkdir -p $(srctree)/configs/sysroot/bin/
	@cp $(obj)/$(BINNAME).elf $(srctree)/configs/sysroot/bin/$(BINNAME)

$(obj)/%.o: $(src)/%.c FORCE
	$(CC) -c $(CFLAGS) $(KBUILD_CFLAGS) $< -o $@

$(ULIBC_LIB): ;
