V := @
PROJECT_DIR := .
BUILD_DIR := $(PROJECT_DIR)
KERNEL_IMG := $(BUILD_DIR)/rtthread.elf
QEMU := qemu-system-aarch64
QEMU_GDB_PORT := 1234
QEMU_OPTS := -machine raspi4b -nographic -serial mon:stdio -m size=2G -kernel $(KERNEL_IMG)
GDB := gdb-multiarch

.PHONY: qemu qemu-gdb gdb

qemu:
	$(V)$(QEMU) $(QEMU_OPTS)

qemu-gdb:
	$(V)$(QEMU) -S -gdb tcp::$(QEMU_GDB_PORT) $(QEMU_OPTS)

gdb:
	$(V)gdb-multiarch -x $(PROJECT_DIR)/.gdbinit

