
# --- Nifties convertion file --
nifties-file := include/generated/nifties.h
need-gen-files := $(nifties-file)

define filechk_genifties
	echo 1000 | bc -q $(srctree)/kernel/nifties.bc
endef

$(nifties-file): $(srctree)/kernel/nifties.bc FORCE
	$(call filechk,genifties)

# --- Font file used by the tty ---
kfont-file := include/generated/kfont.h
need-gen-files += $(kfont-file)

define filechk_genkfont
	python3 $(srctree)/scripts/extpsf1.py $(srctree)/lib/libc/tty/ter-v16b.psf $(srctree)
endef

$(kfont-file): $(srctree)/lib/libc/tty/ter-v16b.psf
	$(call filechk,genkfont)

# --- Offsets file ---
offsets-file := include/generated/asm-offsets.h
offsets-asm  := arch/$(ARCH)/asm-offsets.s
need-gen-files += $(offsets-file)

define filechk_offsets
	echo '#ifndef __ASM_OFFSETS_H'; \
	echo '#define __ASM_OFFSETS_H'; \
	printf '\n'; \
	sed -ne 's:^[[:space:]]*\.ascii[[:space:]]*"\(.*\)".*:\1:; /^->/{s:->#\(.*\):/* \1 */:; s:^->\([^ ]*\) [\$$#]*\([^ ]*\) \(.*\):#define \1 \2 /* \3 */:; s:->::; p;}' < $<; \
	echo '#endif'
endef

$(offsets-asm): $(srctree)/arch/$(ARCH)/asm-offsets.c $(srctree)/include/znu/offsets.h
	$(CC) -S $(LINUXINCLUDE) $(KBUILD_CFLAGS) $(KBUILD_CPPFLAGS) -o $@ $<

$(offsets-file): $(offsets-asm) FORCE
	$(call filechk,offsets)
