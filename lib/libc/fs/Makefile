obj-y += cpio.o
CFLAGS_vfs.o += -I$(srctree)/lib/fatfs/source
obj-y += vfs.o
obj-y += vfse.o
CFLAGS_fat32.o += -I$(srctree)/lib/fatfs/source
obj-y += fat32.o
CFLAGS_ext2.o += -I$(srctree)/lib/libc/fs -I$(srctree)/lib/libc/fs/ext2fs -include $(srctree)/lib/libc/fs/ext2fs/ext2_types.h -Wno-implicit-function-declaration
CFLAGS_ext2.o += -Wno-int-conversion
obj-y += ext2.o

