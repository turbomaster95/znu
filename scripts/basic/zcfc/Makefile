hostprogs-y := zcfc
always      := $(hostprogs-y)

zcfc-objs := main.o ../../../lib/csexp.o vendor/tomlc17/src/tomlc17.o

HOSTCFLAGS_main.o           := -std=c17 -I$(srctree)/include -I$(src)/vendor/tomlc17/src
HOSTCFLAGS_csexp.o          := -std=c17 -I$(srctree)/include
HOSTCFLAGS_vendor/tomlc17/src/tomlc17.o := -std=c17 -I$(src)/vendor/tomlc17/src
HOSTLOADLIBES_zcfc          := -lm
