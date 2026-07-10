OUT := build/rp2040
.DEFAULT_GOAL := $(OUT)/firmware.uf2

include make/common.mk

CFLAGS += -mcpu=cortex-m0 -Wno-atomic-alignment -Wno-c99-designator

# UART bootloader download settings (see src/rp2040/boot.cpp, tools/send-firmware.py)
SERIAL ?= /dev/ttyUSB0

# firmware objects, shared by flash and ram link variants (boot2 is flash-only)
RP2040_OBJS := $(OUT)/rp2040/main.o \
			   $(OUT)/rp2040/atomic.o \
			   $(OUT)/rp2040/system.o \
			   $(OUT)/rp2040/hal/time.o \
			   $(OUT)/rp2040/hal/uart.o \
			   $(OUT)/rp2040/hw/rom.o \
			   $(OUT)/coop/runner.o \
			   $(OUT)/noxx/malloc.o \
			   $(OUT)/noxx/string.o \
			   $(OUT)/noxx/string-view.o \
			   $(OUT)/abi.o \
			   $(OUT)/split.o \
			   $(OUT)/print.o \
			   $(OUT)/uart.o

# firmware images

# flash variant
$(OUT)/firmware.elf: src/rp2040/link.ld $(OUT)/rp2040/boot2.o $(RP2040_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

# ram variant (downloaded to SRAM by the bootloader)
$(OUT)/firmware-ram.elf: src/rp2040/link-ram.ld $(RP2040_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

# bootloader
$(OUT)/boot.elf: src/rp2040/link-boot.ld \
				 $(OUT)/rp2040/boot2.o \
				 $(OUT)/rp2040/boot.o \
				 $(OUT)/rp2040/system.o \
				 $(OUT)/rp2040/atomic.o \
				 $(OUT)/rp2040/hal/time.o \
				 $(OUT)/rp2040/hal/uart.o \
				 $(OUT)/rp2040/hw/rom.o \
				 $(OUT)/noxx/string.o \
				 $(OUT)/noxx/string-view.o \
				 $(OUT)/inflate.o \
				 $(OUT)/uart.o \
				 $(OUT)/print.o \
				 $(OUT)/abi.o
	$(LD) $(LDFLAGS) -T $^ -o $@

# flash images carry the boot2 crc and are packed into uf2
$(OUT)/%-crc.bin: $(OUT)/%.bin
	tools/crc/crc $< $@

$(OUT)/%.uf2: $(OUT)/%-crc.bin
	python tools/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $< -o $@

# custom commands
.PHONY: flash flash-boot send

# flash firmware
flash: $(OUT)/firmware.uf2
	picotool load -x $<

# flash bootloader
flash-boot: $(OUT)/boot.uf2
	picotool load -x $<

# send ram firmware to the running bootloader over uart
send: $(OUT)/firmware-ram.bin
	python3 tools/send-firmware.py -p $(SERIAL) -b 921600 $<
