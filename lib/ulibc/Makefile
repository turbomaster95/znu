KBUILD_CPPFLAGS := -I$(src)/include -I$(src)/third-party/zline/
KBUILD_CFLAGS := -Wno-builtin-declaration-mismatch -msse -msse2 
CFLAGS_KERNEL :=

export KBUILD_CPPFLAGS KBUILD_CPPFLAGS

always-y += stdio/built-in.a

obj-y := crt0.o malloc.o posix.o stack_check.o
obj-y += qsort.o
obj-y += math.o
obj-y += stdio/
obj-y += string/
obj-y += setjmp/
obj-y += signal/
#obj-y += third-party/
