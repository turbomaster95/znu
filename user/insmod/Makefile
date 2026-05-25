BINNAME := insmod

extra-y += $(BINNAME).elf
obj-y := main.o

KBUILD_CPPFLAGS := -I$(srctree)/lib/build-musl/sysr/usr/include
LINUXINCLUDE    :=
KBUILD_CPPFLAGS += -Wno-undef -Wno-builtin-declaration-mismatch -Wa,--noexecstack

always-y += $(BINNAME).elf

quiet_cmd_znu_ld = LD      $@
      cmd_znu_ld = $(CC) -static -nostdlib -Wl,-z,noexecstack -T $(srctree)/init/linker.ld \
	    $(srctree)/lib/build-musl/sysr/usr/lib/crt1.o \
	    $(srctree)/lib/build-musl/sysr/usr/lib/crti.o \
	    $(obj)/main.o \
	    -L$(dir $(shell $(CC) -print-libgcc-file-name)) -lgcc \
	    -Wl,--start-group \
	    $(srctree)/lib/build-musl/sysr/usr/lib/libc.a \
	    -Wl,--end-group \
	    $(srctree)/lib/build-musl/sysr/usr/lib/crtn.o \
	    -o $@

$(obj)/$(BINNAME).elf: $(obj)/main.o $(ULIBC_LIB) FORCE
	$(call if_changed,znu_ld)
	@mkdir -p $(srctree)/configs/sysroot/bin/
	@cp $(obj)/$(BINNAME).elf $(srctree)/configs/sysroot/bin/$(BINNAME)

$(obj)/%.o: $(src)/%.c FORCE
	$(CC) -c $(CFLAGS) $(KBUILD_CFLAGS) $< -o $@

$(ULIBC_LIB): ;
