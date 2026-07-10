OUT := build/rp2040
.DEFAULT_GOAL := $(OUT)/firmware.uf2

include make/common.mk

CFLAGS += -mcpu=cortex-m0 -Wno-atomic-alignment

$(OUT)/firmware.elf: src/rp2040/link.ld \
					 $(OUT)/rp2040/boot2.o \
					 $(OUT)/rp2040/main.o \
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
	$(LD) $(LDFLAGS) -T $^ -o $@

$(OUT)/firmware-crc.bin: $(OUT)/firmware.bin
	tools/crc/crc $< $@

$(OUT)/firmware.uf2: $(OUT)/firmware-crc.bin
	python tools/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $< -o $@

# custom commands
.PHONY: flash

flash: $(OUT)/firmware.uf2
	picotool load -x $<
