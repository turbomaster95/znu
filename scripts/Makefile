###
# scripts contains sources for various helper programs used throughout
# the kernel for the build process.
# ---------------------------------------------------------------------------
# kallsyms:      Find all symbols in vmlinux
# pnmttologo:    Convert pnm files to logo files
# conmakehash:   Create chartable
# conmakehash:	 Create arrays for initializing the kernel console tables
# docproc:       Used in Documentation/DocBook

#hostprogs-$(CONFIG_KALLSYMS)     += kallsyms
#hostprogs-$(CONFIG_LOGO)         += pnmtologo
#hostprogs-$(CONFIG_VT)           += conmakehash
#hostprogs-$(CONFIG_IKCONFIG)     += bin2c
#hostprogs-$(BUILD_C_RECORDMCOUNT) += recordmcount

always		:= $(hostprogs-y) $(hostprogs-m)

subdir-y += embsym

subdir-	+= basic kconfig embsym
