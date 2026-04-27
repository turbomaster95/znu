extra-y += init.elf
obj-y := main.o
obj-y += printf.o

KBUILD_CPPFLAGS := -Wno-builtin-declaration-mismatch

# Define the target for cross-compilation
# This matches your Termux-to-x86_64 setup
#USER_TARGET := --target=x86_64-unknown-none-elf

# We don't have .c files here, we are linking precompiled objects
# always-y ensures Kbuild tries to 'build' the elf even without a .obj-y list
always-y += init.elf

# --- Custom Quiet Commands ---

# Quiet Command for Linking
# Using $(CC) which is usually clang in your environment
quiet_cmd_znu_ld = LD      $@
      cmd_znu_ld = $(CC) $(USER_TARGET) -static -nostdlib -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -T $(src)/linker.ld $(obj)/built-in.o -o $@

# --- Rules ---

# The rule to 'build' the elf from the precompiled binaries
# We depend on the .uu and .uua files themselves
$(obj)/init.elf: $(addprefix $(obj)/,$(obj-y))  FORCE
	$(call if_changed,znu_ld)
	@cp $(srctree)/init/init.elf $(srctree)/configs/iso_root/boot/init.elf

$(obj)/%.o: $(src)/%.c FORCE
	$(CC) -c $(CFLAGS) $(KBUILD_CFLAGS) $< -o $@
