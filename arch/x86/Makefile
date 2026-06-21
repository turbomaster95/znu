KBUILD_CPPFLAGS += -I$(srctree)/arch/x86/include

obj-y += start.o
obj-y += gdt.o
obj-y += gdt_flush.o
obj-y += idt.o
obj-y += idt_stub.o
obj-y += pit.o
obj-y += pic.o
obj-y += tsc.o
obj-y += rtc.o
obj-y += lapic.o
obj-y += timekeeper.o
obj-y += usermode.o
obj-y += syscall.o
obj-y += syscall_entry.o
obj-y += ps2.o
obj-y += pat.o
obj-y += pci.o
obj-y += ahci.o
obj-y += cpu_speed.o
obj-y += spinlock.o
obj-y += mutex.o
obj-y += rwlock.o
obj-y += semaphore.o
obj-y += x86.o
obj-y += nx.o
obj-y += panic.o
obj-y += simd.o
obj-y += refreshe.o

# Define the target for the embedded ELF
$(obj)/refreshe.o: $(src)/refresh.asm
	$(Q)$(NASM) -f elf64 $(src)/refresh.asm -o $(obj)/refresh_raw.o
	$(Q)$(LD) -Ttext 0x0 $(obj)/refresh_raw.o -o $(obj)/refresh.elf	
	$(Q)$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--rename-section .data=.dummybin \
		$(obj)/refresh.elf $@
