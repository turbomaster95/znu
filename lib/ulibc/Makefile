KBUILD_CPPFLAGS := -I$(src)/include
LINUXINCLUDE    :=

KBUILD_CFLAGS += -Wno-builtin-declaration-mismatch

export KBUILD_CPPFLAGS
export LINUXINCLUDE

always-y += stdio/built-in.o

obj-y := stdio/
obj-y += string/

