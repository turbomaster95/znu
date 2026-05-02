KBUILD_CPPFLAGS += -I$(src)/include
KBUILD_CFLAGS += -Wno-builtin-declaration-mismatch

always-y += stdio/built-in.o

obj-y := crt0.o malloc.o posix.o
obj-y += qsort.o
obj-y += stdio/
obj-y += string/
obj-y += setjmp/
obj-y += signal/