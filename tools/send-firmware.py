#!/usr/bin/env python3
# Send a RAM firmware image to the EKH05 UART bootloader (src/ekh05/boot.cpp).
#
# frame format:
#   magic    "RPBL"       (4 bytes)
#   comp_len u32 little-endian
#   orig_len u32 little-endian
#   crc32    u32 little-endian  (CRC-32/zlib of the compressed payload)
#   payload  comp_len bytes     (raw DEFLATE stream, wbits = -15)

import argparse
import sys
import time
import zlib

import serial  # pyserial


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "image", help="raw firmware .bin (e.g. build/ekh05/firmware-ram.bin)"
    )
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    ap.add_argument("-b", "--baud", type=int, default=921600, help="must match BL_BAUD")
    args = ap.parse_args()

    with open(args.image, "rb") as f:
        orig = f.read()
    deflate = zlib.compressobj(9, zlib.DEFLATED, -15)
    comp = deflate.compress(orig) + deflate.flush()
    crc = zlib.crc32(comp) & 0xFFFFFFFF
    frame = (
        b"RPBL"
        + len(comp).to_bytes(4, "little")
        + len(orig).to_bytes(4, "little")
        + crc.to_bytes(4, "little")
        + comp
    )
    print(
        f"image {len(orig)} bytes -> {len(comp)} compressed ({100 * len(comp) // len(orig)}%), crc {crc:#010x}"
    )

    ser = serial.Serial(args.port, args.baud, timeout=0.2, xonxoff=False, rtscts=False)

    # drain, then send into a silent, listening bootloader
    ser.reset_input_buffer()
    ser.write(frame)
    ser.flush()
    print("done")


if __name__ == "__main__":
    sys.exit(main())
