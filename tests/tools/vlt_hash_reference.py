#!/usr/bin/env python3
"""Independent transcription of VaultLib's VLT32Hasher.cs / VLT64Hasher.cs
(NFSTools/VaultLib, MIT) used to generate the reference vectors in
tests/test_core.cpp. Run: python3 vlt_hash_reference.py [strings...]"""
import sys

M32 = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF


def bin_hash(s: bytes) -> int:
    h = 0xFFFFFFFF
    for ch in s:
        h = (h * 0x21 + ch) & M32
    return h


def _mix32(a, b, c):
    a = (a - b - c) & M32; a ^= c >> 13
    b = (b - c - a) & M32; b ^= (a << 8) & M32
    c = (c - a - b) & M32; c ^= b >> 13
    a = (a - b - c) & M32; a ^= c >> 12
    b = (b - c - a) & M32; b ^= (a << 16) & M32
    c = (c - a - b) & M32; c ^= b >> 5
    a = (a - b - c) & M32; a ^= c >> 3
    b = (b - c - a) & M32; b ^= (a << 10) & M32
    c = (c - a - b) & M32; c ^= b >> 15
    return a, b, c


def vlt32(k: bytes, init: int = 0xABCDEF00) -> int:
    a = b = 0x9E3779B9
    c = init
    pos, length = 0, len(k)
    while length >= 12:
        a = (a + int.from_bytes(k[pos:pos + 4], "little")) & M32
        b = (b + int.from_bytes(k[pos + 4:pos + 8], "little")) & M32
        c = (c + int.from_bytes(k[pos + 8:pos + 12], "little")) & M32
        a, b, c = _mix32(a, b, c)
        pos += 12
        length -= 12
    c = (c + len(k)) & M32
    tail = k[pos:]
    for i, ch in enumerate(tail):
        if i >= 8:
            c = (c + (ch << (8 * (i - 8) + 8))) & M32
        elif i >= 4:
            b = (b + (ch << (8 * (i - 4)))) & M32
        else:
            a = (a + (ch << (8 * i))) & M32
    a, b, c = _mix32(a, b, c)
    return c


def vlt64(k: bytes, init: int = 0xABCDEF0011223344) -> int:
    a = b = init
    c = 0x9E3779B97F4A7C13
    pos, length = 0, len(k)
    while length >= 24:
        t = (b + int.from_bytes(k[pos + 8:pos + 16], "little")) & M64
        c = (c + int.from_bytes(k[pos + 16:pos + 24], "little")) & M64
        b = ((a + int.from_bytes(k[pos:pos + 8], "little")) - t - c) & M64
        b ^= c >> 43
        a = (t - c - b) & M64; a ^= (b << 9) & M64
        m = (c - b - a) & M64; m ^= a >> 8
        b = (b - a - m) & M64; b ^= m >> 38
        a = (a - m - b) & M64; a ^= (b << 23) & M64
        m = (m - b - a) & M64; m ^= a >> 5
        b = (b - a - m) & M64; b ^= m >> 35
        c = (a - m - b) & M64; c ^= (b << 49) & M64
        m = (m - b - c) & M64; m ^= c >> 11
        a = (b - c - m) & M64; a ^= m >> 12
        b = (c - m - a) & M64; b ^= (a << 18) & M64
        c = (m - a - b) & M64; c ^= b >> 22
        pos += 24
        length -= 24
    c = (c + (len(k) & M32)) & M64
    tail = k[pos:]
    for i, ch in enumerate(tail):
        if i >= 16:
            c = (c + (ch << (8 * (i - 16) + 8))) & M64
        elif i >= 8:
            b = (b + (ch << (8 * (i - 8)))) & M64
        else:
            a = (a + (ch << (8 * i))) & M64
    m1 = (a - b - c) & M64; m1 ^= c >> 43
    a = (b - c - m1) & M64; a ^= (m1 << 9) & M64
    m2 = (c - m1 - a) & M64; m2 ^= a >> 8
    b = (m1 - a - m2) & M64; b ^= m2 >> 38
    m1 = (a - m2 - b) & M64; m1 ^= (b << 23) & M64
    m2 = (m2 - b - m1) & M64; m2 ^= m1 >> 5
    b = (b - m1 - m2) & M64; b ^= m2 >> 35
    m1 = (m1 - m2 - b) & M64; m1 ^= (b << 49) & M64
    m2 = (m2 - b - m1) & M64; m2 ^= m1 >> 11
    b = (b - m1 - m2) & M64; b ^= m2 >> 12
    m1 = (m1 - m2 - b) & M64; m1 ^= (b << 18) & M64
    return ((m2 - b - m1) & M64) ^ (m1 >> 22)


if __name__ == "__main__":
    words = sys.argv[1:] or ["a", "pvehicle", "aaaabbbbccccdd",
                             "abcdefghijklmnopqrstuvwxyz0123456789"]
    for w in words:
        raw = w.encode("ascii")
        print(f"{w!r}: bin=0x{bin_hash(raw):08X} vlt32=0x{vlt32(raw):08X} "
              f"vlt64=0x{vlt64(raw):016X}")
