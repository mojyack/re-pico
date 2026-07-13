CC      := clang --target=arm-none-eabi
CXX     := clang++ --target=arm-none-eabi
LD      := ld.lld
OBJCOPY := llvm-objcopy

CFLAGS += -Os -nostdinc -Isrc -ffunction-sections
CXXFLAGS += $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti -fno-use-cxa-atexit
LDFLAGS += -nostdlib --gc-sections

# common rules

.PHONY: clean

clean:
	rm -rf build

$(OUT)/%.bin: $(OUT)/%.elf
	$(OBJCOPY) -O binary $< $@

$(OUT)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# halow firmware blob

FW_ELF      := ref/morse-firmware/firmware/mm8108b2-rl.bin
BCF_ELF     := ref/morse-firmware/bcf/morsemicro/bcf_mf15457.bin
BCF_COUNTRY := JP

$(addprefix $(OUT)/halow-fw-blob, .bin .hpp .cpp): tools/generate-halow-fw.py $(FW_ELF) $(BCF_ELF)
	mkdir -p $(OUT)
	python3 tools/generate-halow-fw.py $(FW_ELF) $(BCF_ELF) $(BCF_COUNTRY) $(addprefix $(OUT)/halow-fw-blob, .bin .hpp .cpp)

$(OUT)/halow/firmware.o: src/halow/firmware.cpp $(OUT)/halow-fw-blob.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
