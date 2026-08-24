#!/usr/bin/env python3
"""Verify STCC4 CRC-8 implementation against datasheet test vector."""

# CRC-8: poly 0x31, init 0xFF, no reflection, final XOR 0x00


def crc8(data: bytes) -> int:
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x31) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def main() -> None:
    vector = bytes([0xBE, 0xEF])
    result = crc8(vector)
    expected = 0x92
    if result != expected:
        raise SystemExit(f"CRC test failed: CRC(0xBEEF) = 0x{result:02X}, expected 0x{expected:02X}")
    print(f"CRC test passed: CRC(0xBEEF) = 0x{result:02X}")


if __name__ == "__main__":
    main()
