hostprogs-y := embsym
embsym-objs := embsym.o

always-y := $(hostprogs-y)
always   := $(hostprogs-y)

$(addprefix $(obj)/,$(filter-out embsym,$(always))): $(obj)/embsym
