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
					 $(OUT)/boot2.o \
					 $(OUT)/main.o \
					 $(OUT)/abi.o \
					 $(OUT)/hw/rom.o \
					 $(NOXX_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

$(OUT)/firmware-crc.bin: $(OUT)/firmware.bin
	tools/crc/crc $< $@

$(OUT)/firmware.uf2: $(OUT)/firmware-crc.bin
	python tools/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $< -o $@

else ifeq ($(BOARD),ekh05)

CFLAGS += -mcpu=cortex-m33 -mfloat-abi=soft

all: $(OUT)/firmware.bin

$(OUT)/firmware.elf: src/ekh05/link.ld \
					 $(OUT)/main.o \
					 $(OUT)/abi.o \
					 $(NOXX_OBJS)
	$(LD) $(LDFLAGS) -T $^ -o $@

flash: $(OUT)/firmware.bin
	st-flash --reset write $< 0x08000000

else
$(error unknown BOARD "$(BOARD)")
endif

clean:
	rm -rf build

$(OUT)/firmware.bin: $(OUT)/firmware.elf
	$(OBJCOPY) -O binary $< $@

$(OUT)/%.o: src/$(BOARD)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
