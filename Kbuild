nifties-file := include/generated/nifties.h

define filechk_genifties
	echo 100 | bc -q $(srctree)/kernel/nifties.bc
endef

$(nifties-file): $(srctree)/kernel/nifties.bc FORCE
	$(call filechk,genifties)

need-gen-files := $(nifties-file)
