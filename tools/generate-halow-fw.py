#!/usr/bin/env python3
# Extract the chip-loadable segments from the Morse Micro firmware and BCF ELF
# images (ref/morse-firmware/*) into a self-describing store image (header +
# segment table + blob; layout in src/halow/fw-store.hpp) that is flashed at a
# fixed rom address alongside the bootloader and read in place by the EKH05
# firmware downloader (halow::load_firmware).
#
# The blob is stored raw-DEFLATE compressed to keep the rom image small;
# load_firmware inflates it into RAM (halow::inflate) before writing segments.
# The segment offsets index into the *uncompressed* blob.
#
# The firmware ELF's PT_LOAD program headers map 1:1 onto the mbin FW_SEGMENT
# records the morselib loader would produce (p_paddr -> chip address). The BCF
# ELF keeps the board config and every regulatory domain in named sections; all
# regdoms share one load address, so we pick exactly one country.

import struct
import sys
import zlib

STORE_MAGIC = 0x53574648  # "HFWS"


def read_elf_load_segments(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"\x7fELF", f"{path}: not an ELF file"
    assert data[4] == 1, "expected 32-bit ELF"
    assert data[5] == 1, "expected little-endian ELF"
    (e_phoff,) = struct.unpack_from("<I", data, 0x1C)
    e_phentsize, e_phnum = struct.unpack_from("<HH", data, 0x2A)
    segments = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from(
            "<IIIII", data, off
        )
        if p_type != 1 or p_filesz == 0:  # PT_LOAD with data only
            continue
        segments.append((p_paddr, data[p_offset : p_offset + p_filesz]))
    return segments


def read_elf_section(path, name):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"\x7fELF", f"{path}: not an ELF file"
    (e_shoff,) = struct.unpack_from("<I", data, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 0x2E)
    strtab_off = struct.unpack_from(
        "<I", data, e_shoff + e_shstrndx * e_shentsize + 0x10
    )[0]

    def section_name(name_idx):
        end = data.index(b"\x00", strtab_off + name_idx)
        return data[strtab_off + name_idx : end].decode()

    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack_from(
            "<IIIIII", data, off
        )
        if section_name(sh_name) == name:
            return sh_addr, data[sh_offset : sh_offset + sh_size]
    raise KeyError(f"{path}: section {name} not found")


def pad4(b):
    if len(b) % 4:
        b = b + b"\x00" * (4 - len(b) % 4)
    return b


def main():
    fw_elf, bcf_elf, country, out_bin = sys.argv[1:5]

    blob = bytearray()
    fw_table = []
    bcf_table = []
    orig_total = 0

    def append(addr, payload):
        nonlocal orig_total
        payload = pad4(payload)
        deflate = zlib.compressobj(9, zlib.DEFLATED, -15)
        compressed = deflate.compress(payload) + deflate.flush()
        offset = len(blob)
        blob.extend(compressed)
        orig_total += len(payload)
        return (addr, offset, len(compressed), len(payload))

    for addr, payload in read_elf_load_segments(fw_elf):
        fw_table.append(append(addr, payload))

    for name in (".board_config", f".regdom_{country}"):
        addr, payload = read_elf_section(bcf_elf, name)
        bcf_table.append(append(addr, payload))

    # struct FwStore (src/halow/fw-store.hpp) + segment table + blob
    table = fw_table + bcf_table
    blob_offset = 20 + 16 * len(table)
    with open(out_bin, "wb") as f:
        f.write(
            struct.pack(
                "<IIII4s",
                STORE_MAGIC,
                len(fw_table),
                len(bcf_table),
                blob_offset,
                country.encode(),
            )
        )
        for row in table:
            f.write(struct.pack("<IIII", *row))
        f.write(blob)

    largest = max(n for _, _, _, n in table)
    print(
        f"fw: {len(fw_table)} segments; bcf: {len(bcf_table)} segments; "
        f"{orig_total} bytes -> {len(blob)} compressed "
        f"({100 * len(blob) // orig_total}%); "
        f"largest segment {largest} bytes (peak decompress RAM)"
    )


if __name__ == "__main__":
    main()
