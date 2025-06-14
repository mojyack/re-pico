CC      := clang --target=arm-none-eabi
CXX     := clang++ --target=arm-none-eabi
LD      := ld.lld
OBJCOPY := llvm-objcopy

CFLAGS += -mcpu=cortex-m0 -nostdinc -Isrc
CXXFLAGS += $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti
LDFLAGS += -nostdlib

.PHONY: all clean

all: build/firmware.uf2

clean:
	rm -rf build

build/firmware.elf: src/link.ld \
					build/boot2.o \
					build/main.o \
					build/abi.o \
					build/hw/rom.o \
					build/noxx/malloc.o \
					build/noxx/string.o \
					build/noxx/string-view.o
	$(LD) $(LDFLAGS) -T $^ -o $@

build/firmware.bin: build/firmware.elf
	$(OBJCOPY) -O binary $< $@

build/firmware-crc.bin: build/firmware.bin
	tools/crc/crc $< $@

build/firmware.uf2: build/firmware-crc.bin
	python tools/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $< -o $@

build/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
