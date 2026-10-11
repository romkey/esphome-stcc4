"""STCC4 CRC-8 against the datasheet's test vector.

Mirrors the CRC in components/stcc4/stcc4.cpp: polynomial 0x31, init 0xFF,
no reflection, final XOR 0x00 (STCC4 datasheet section 3.3, "Checksum Calculation").
"""


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


def test_datasheet_vector():
    assert crc8(bytes([0xBE, 0xEF])) == 0x92
