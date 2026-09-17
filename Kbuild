nifties-file := include/generated/nifties.h
need-gen-files := $(nifties-file)

define filechk_genifties
	echo 100 | bc -q $(srctree)/kernel/nifties.bc
endef

$(nifties-file): $(srctree)/kernel/nifties.bc FORCE
	$(call filechk,genifties)

kfont-file := include/generated/kfont.h
need-gen-files += $(kfont-file)

define filechk_genkfont
	python3 $(srctree)/scripts/extpsf1.py $(srctree)/lib/libc/tty/ter-v16b.psf $(srctree)
endef

$(kfont-file): $(srctree)/lib/libc/tty/ter-v16b.psf
	$(call filechk,genkfont)


