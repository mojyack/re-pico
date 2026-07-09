CC      := clang --target=arm-none-eabi
CXX     := clang++ --target=arm-none-eabi
LD      := ld.lld
OBJCOPY := llvm-objcopy

BOARD ?= rp2040
OUT   := build/$(BOARD)

CFLAGS += -Os -nostdinc -Isrc
CXXFLAGS += $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti
LDFLAGS += -nostdlib

NOXX_OBJS := $(OUT)/noxx/malloc.o \
			 $(OUT)/noxx/string.o \
			 $(OUT)/noxx/string-view.o

.PHONY: all clean flash

ifeq ($(BOARD),rp2040)

CFLAGS += -mcpu=cortex-m0

all: $(OUT)/firmware.uf2

$(OUT)/firmware.elf: src/rp2040/link.ld \
					 $(OUT)/rp2040/boot2.o \
					 $(OUT)/rp2040/main.o \
					 $(OUT)/rp2040/hw/rom.o \
					 $(OUT)/abi.o \
					 $(NOXX_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

$(OUT)/firmware-crc.bin: $(OUT)/firmware.bin
	tools/crc/crc $< $@

$(OUT)/firmware.uf2: $(OUT)/firmware-crc.bin
	python tools/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $< -o $@

else ifeq ($(BOARD),ekh05)

CFLAGS += -mcpu=cortex-m33 -mfloat-abi=soft -I$(OUT)

# MM8108 firmware + board config, extracted from the Morse Micro ELF images
FW_ELF      := ref/morse-firmware/firmware/mm8108b2-rl.bin
BCF_ELF     := ref/morse-firmware/bcf/morsemicro/bcf_mf15457.bin
BCF_COUNTRY := JP

all: $(OUT)/firmware.bin

$(OUT)/firmware.elf: src/ekh05/link.ld \
					 $(OUT)/ekh05/main.o \
					 $(OUT)/ekh05/hal/gpio.o \
					 $(OUT)/ekh05/hal/sleep.o \
					 $(OUT)/ekh05/hal/spi.o \
					 $(OUT)/ekh05/halow.o \
					 $(OUT)/halow/halow.o \
					 $(OUT)/halow/firmware.o \
					 $(OUT)/halow-fw.o \
					 $(OUT)/abi.o \
					 $(OUT)/inflate.o \
					 $(NOXX_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

$(OUT)/halow-fw.cpp: tools/generate-halow-fw.py $(FW_ELF) $(BCF_ELF)
	mkdir -p $(OUT)
	python3 tools/generate-halow-fw.py $(FW_ELF) $(BCF_ELF) $(BCF_COUNTRY) \
		$(OUT)/halow-fw.bin $(OUT)/halow-fw.hpp $(OUT)/halow-fw.cpp

$(OUT)/halow-fw.hpp $(OUT)/halow-fw.bin: $(OUT)/halow-fw.cpp

$(OUT)/halow-fw.o: $(OUT)/halow-fw.cpp $(OUT)/halow-fw.bin
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/halow/firmware.o: $(OUT)/halow-fw.hpp

flash: $(OUT)/firmware.bin
	st-flash --reset write $< 0x08000000

else
$(error unknown BOARD "$(BOARD)")
endif

clean:
	rm -rf build

$(OUT)/firmware.bin: $(OUT)/firmware.elf
	$(OBJCOPY) -O binary $< $@

$(OUT)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
