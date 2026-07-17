OUT := build/ekh05
.DEFAULT_GOAL := $(OUT)/firmware.bin

include make/common.mk

CFLAGS += -mcpu=cortex-m33 -mfloat-abi=soft -I$(OUT) -Wno-c99-designator

# UART bootloader download settings (see src/ekh05/boot.cpp, tools/send-firmware.py)
SERIAL ?= /dev/ttyACM0

# firmware objects, shared by flash and ram link variants
EKH05_OBJS := $(OUT)/ekh05/main.o \
			  $(OUT)/ekh05/halow.o \
			  $(OUT)/ekh05/system.o \
			  $(OUT)/ekh05/hal/gpio.o \
			  $(OUT)/ekh05/hal/time.o \
			  $(OUT)/ekh05/hal/uart.o \
			  $(OUT)/ekh05/hal/spi.o \
			  $(OUT)/ekh05/hal/rng.o \
			  $(OUT)/halow/halow.o \
			  $(OUT)/halow/firmware.o \
			  $(OUT)/halow/host-table.o \
			  $(OUT)/halow/command.o \
			  $(OUT)/halow/yaps.o \
			  $(OUT)/halow/scan.o \
			  $(OUT)/halow/connect.o \
			  $(OUT)/halow/netif.o \
			  $(OUT)/net/packet.o \
			  $(OUT)/net/ip.o \
			  $(OUT)/net/arp.o \
			  $(OUT)/net/icmp.o \
			  $(OUT)/net/udp.o \
			  $(OUT)/net/dhcp.o \
			  $(OUT)/net/tcp.o \
			  $(OUT)/net/stack.o \
			  $(OUT)/crypto/sha256.o \
			  $(OUT)/crypto/hmac-sha256.o \
			  $(OUT)/crypto/kdf.o \
			  $(OUT)/crypto/aes.o \
			  $(OUT)/crypto/aes-cmac.o \
			  $(OUT)/crypto/aes-keywrap.o \
			  $(OUT)/crypto/p256.o \
			  $(OUT)/connect/sae.o \
			  $(OUT)/connect/eapol.o \
			  $(OUT)/halow-regdb.o \
			  $(OUT)/coop/runner.o \
			  $(OUT)/noxx/malloc.o \
			  $(OUT)/noxx/string.o \
			  $(OUT)/noxx/string-view.o \
			  $(OUT)/abi.o \
			  $(OUT)/split.o \
			  $(OUT)/print.o \
			  $(OUT)/uart.o \
			  $(OUT)/console.o \
			  $(OUT)/inflate.o

all: $(OUT)/firmware.bin

# firmware images

# flash variant
$(OUT)/firmware.elf: src/ekh05/link.ld $(EKH05_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

# ram variant
$(OUT)/firmware-ram.elf: src/ekh05/link-ram.ld $(EKH05_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

# bootloader
$(OUT)/boot.elf: src/ekh05/link-boot.ld \
				 $(OUT)/ekh05/boot.o \
				 $(OUT)/ekh05/system.o \
				 $(OUT)/ekh05/hal/time.o \
				 $(OUT)/ekh05/hal/uart.o \
				 $(OUT)/noxx/string-view.o \
				 $(OUT)/noxx/string.o \
				 $(OUT)/inflate.o \
				 $(OUT)/uart.o \
				 $(OUT)/print.o \
				 $(OUT)/abi.o
	$(LD) $(LDFLAGS) -T $^ -o $@

# custom commands
.PHONY: flash flash-boot send

# halow firmware store location, keep in sync with src/ekh05/main.cpp and link*.ld
FW_STORE_ADDR := 0x08040000

# flash firmware
flash: $(OUT)/firmware.bin $(OUT)/halow-fw-store.bin
	st-flash write $(OUT)/halow-fw-store.bin $(FW_STORE_ADDR)
	st-flash --reset write $(OUT)/firmware.bin 0x08000000

# flash bootloader
flash-boot: $(OUT)/boot.bin $(OUT)/halow-fw-store.bin
	st-flash write $(OUT)/halow-fw-store.bin $(FW_STORE_ADDR)
	st-flash --reset write $(OUT)/boot.bin 0x08000000

send: $(OUT)/firmware-ram.bin
	python3 tools/send-firmware.py -p $(SERIAL) -b 921600 $<
