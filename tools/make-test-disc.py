#!/usr/bin/env python3
# Mandrake - makes the PS1 test disc of the host checks: a homebrew program written here, on a disc image written
# here, so no game, BIOS or anybody else's file is needed. Free to share like the rest of the repository.
# SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes <out>/mandrake-test.bin and .cue: one Mode 2 data track (2352-byte sectors with their sync, header,
subheader, EDC and ECC) holding an ISO 9660 file system with SYSTEM.CNF and the program it boots (SLUS_999.99), as a
PS1 game disc has them.

The program, assembled below, sets the GPU up for a 320x240 picture, then on every vertical blank fills the screen with
one of four colours (each shown for 32 frames: red, green, blue, yellow) and a white 32x32 square at y 96 that
steps 16 pixels to the right every 4 frames (x = 16 * (frame / 4) mod 256). It waits for the blank by polling the
interrupt status, with every interrupt masked, so it needs nothing of the BIOS once it runs: any BIOS that boots a
disc by its SYSTEM.CNF starts it, Beetle PSX's built-in OpenBIOS among them.

The image is the same, byte for byte, every time (tests/test_beetle.py checks its SHA-256).

    tools/make-test-disc.py <folder>
"""

import struct
import sys
from pathlib import Path

# ---------------------------------------------------------------- the program (MIPS R3000, little-endian)

REG = {name: i for i, name in enumerate(
    "zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra".split())}
LOAD_ADDRESS = 0x80010000
COLOURS = [0x0000C0, 0x00C000, 0xC00000, 0x00C0C0]  # GP0 colours are 0xBBGGRR: red, green, blue, yellow


class Assembler:
    def __init__(self, origin):
        self.origin = origin
        self.words = []      # int, or (kind, label, encoder) to resolve
        self.labels = {}

    @property
    def pc(self):
        return self.origin + 4 * len(self.words)

    def label(self, name):
        self.labels[name] = self.pc

    def emit(self, word):
        self.words.append(word & 0xFFFFFFFF)

    def i_type(self, op, rs, rt, imm):
        self.emit(op << 26 | REG[rs] << 21 | REG[rt] << 16 | (imm & 0xFFFF))

    def r_type(self, funct, rs="zero", rt="zero", rd="zero", sa=0):
        self.emit(REG[rs] << 21 | REG[rt] << 16 | REG[rd] << 11 | sa << 6 | funct)

    def nop(self):
        self.emit(0)

    def lui(self, rt, imm): self.i_type(0x0F, "zero", rt, imm)
    def ori(self, rt, rs, imm): self.i_type(0x0D, rs, rt, imm)
    def andi(self, rt, rs, imm): self.i_type(0x0C, rs, rt, imm)
    def addiu(self, rt, rs, imm): self.i_type(0x09, rs, rt, imm)
    def lw(self, rt, offset, base): self.i_type(0x23, base, rt, offset)
    def sw(self, rt, offset, base): self.i_type(0x2B, base, rt, offset)
    def sll(self, rd, rt, sa): self.r_type(0x00, rt=rt, rd=rd, sa=sa)
    def srl(self, rd, rt, sa): self.r_type(0x02, rt=rt, rd=rd, sa=sa)
    def addu(self, rd, rs, rt): self.r_type(0x21, rs=rs, rt=rt, rd=rd)
    def and_(self, rd, rs, rt): self.r_type(0x24, rs=rs, rt=rt, rd=rd)
    def or_(self, rd, rs, rt): self.r_type(0x25, rs=rs, rt=rt, rd=rd)

    def li(self, rt, value):
        self.lui(rt, value >> 16)
        self.ori(rt, rt, value & 0xFFFF)

    def branch(self, op, rs, rt, target):
        self.words.append(("branch", target, (op, rs, rt, self.pc)))
        self.nop()  # the delay slot

    def beq(self, rs, rt, target): self.branch(0x04, rs, rt, target)

    def j(self, target):
        self.words.append(("jump", target, None))
        self.nop()

    def assemble(self):
        out = bytearray()
        for word in self.words:
            if isinstance(word, tuple):
                kind, target, extra = word
                address = self.labels[target]
                if kind == "branch":
                    op, rs, rt, at = extra
                    word = op << 26 | REG[rs] << 21 | REG[rt] << 16 | (((address - (at + 4)) >> 2) & 0xFFFF)
                else:
                    word = 0x02 << 26 | (address >> 2) & 0x3FFFFFF
            out += struct.pack("<I", word)
        return bytes(out)


def program():
    a = Assembler(LOAD_ADDRESS)

    def gpu_ready():
        # GPUSTAT bit 26: ready for a command word
        here = f"ready{len(a.words)}"
        a.label(here)
        a.lw("t8", 0x1814, "t0")
        a.nop()
        a.lui("t9", 0x0400)
        a.and_("t8", "t8", "t9")
        a.beq("t8", "zero", here)

    def gp0(register):
        a.sw(register, 0x1810, "t0")

    a.lui("t0", 0x1F80)                # the I/O ports, 0x1F801000 and up
    a.sw("zero", 0x1074, "t0")         # I_MASK: no interrupt reaches the CPU
    a.sw("zero", 0x1814, "t0")         # GP1(00h): reset the GPU
    for command in (0x03000000,        # display on
                    0x05000000,        # display area at (0, 0) of VRAM
                    0x06C60260,        # horizontal range: 320 pixels
                    0x07040010,        # vertical range: lines 16 to 256, 240 lines
                    0x08000001):       # 320 wide, 240 lines, NTSC, 15-bit colour
        a.li("t1", command)
        a.sw("t1", 0x1814, "t0")
    for command in (0xE1000400,        # draw mode: drawing to the displayed area allowed
                    0xE3000000,        # drawing area from (0, 0)...
                    0xE403BD3F,        # ...to (319, 239)
                    0xE5000000):       # drawing offset (0, 0)
        gpu_ready()
        a.li("t1", command)
        gp0("t1")
    a.addu("s0", "zero", "zero")       # the frame counter

    a.label("frame")
    a.label("vblank")
    a.lw("t1", 0x1070, "t0")           # I_STAT
    a.nop()
    a.andi("t1", "t1", 1)              # bit 0: vertical blank
    a.beq("t1", "zero", "vblank")
    a.li("t1", 0xFFFFFFFE)
    a.sw("t1", 0x1070, "t0")           # acknowledged

    # the background: COLOURS[(frame >> 5) & 3], over the whole picture
    a.srl("t2", "s0", 5)
    a.andi("t2", "t2", 3)
    a.sll("t2", "t2", 2)
    a.li("t4", 0)                      # the table's address, filled in below
    table_li = len(a.words) - 2
    a.addu("t4", "t4", "t2")
    a.lw("t3", 0, "t4")
    a.nop()
    gpu_ready()
    gp0("t3")                          # GP0(02h): fill rectangle, colour
    gp0("zero")                        # at (0, 0)
    a.li("t5", 240 << 16 | 320)
    gp0("t5")                          # 320x240

    # the square: white, 32x32, at y 96 and x 16 * (frame >> 2) mod 256
    a.srl("t6", "s0", 2)
    a.sll("t6", "t6", 4)
    a.andi("t6", "t6", 0xF0)
    a.lui("t7", 96)
    a.or_("t7", "t7", "t6")
    gpu_ready()
    a.li("t5", 0x02FFFFFF)
    gp0("t5")
    gp0("t7")
    a.li("t5", 32 << 16 | 32)
    gp0("t5")

    a.addiu("s0", "s0", 1)
    a.j("frame")

    a.label("table")
    table = a.pc
    for colour in COLOURS:
        a.emit(0x02000000 | colour)
    a.words[table_li] = 0x0F << 26 | REG["t4"] << 16 | table >> 16                  # lui t4, %hi(table)
    a.words[table_li + 1] = 0x0D << 26 | REG["t4"] << 21 | REG["t4"] << 16 | table & 0xFFFF  # ori t4, t4, %lo
    return a.assemble()


def ps_exe(code):
    text = code + bytes(-len(code) % 2048)
    header = bytearray(2048)
    header[0:8] = b"PS-X EXE"
    struct.pack_into("<IIII", header, 0x10, LOAD_ADDRESS, 0, LOAD_ADDRESS, len(text))  # pc0, gp0, t_addr, t_size
    struct.pack_into("<II", header, 0x30, 0x801FFFF0, 0)                                # stack
    header[0x4C:0x4C + 27] = b"Mandrake host check program"
    return bytes(header) + text


# ---------------------------------------------------------------- the disc (ISO 9660 on Mode 2 Form 1 sectors)

def both16(v): return struct.pack("<H", v) + struct.pack(">H", v)
def both32(v): return struct.pack("<I", v) + struct.pack(">I", v)


DATE7 = bytes([126, 10, 29, 12, 0, 0, 0])  # 2026-10-29 12:00:00 UTC
DATE17 = b"2026102912000000\x00"


def dir_record(name, lba, size, directory):
    record = bytearray(33 + len(name) + (1 - len(name) % 2))
    record[0] = len(record)
    record[2:10] = both32(lba)
    record[10:18] = both32(size)
    record[18:25] = DATE7
    record[25] = 2 if directory else 0
    record[28:32] = both16(1)
    record[32] = len(name)
    record[33:33 + len(name)] = name
    return bytes(record)


def pad(text, length):
    return text.encode("ascii").ljust(length, b" ")


EDC_TABLE = []
for i in range(256):
    e = i
    for _ in range(8):
        e = (e >> 1) ^ (0xD8018001 if e & 1 else 0)
    EDC_TABLE.append(e)
ECC_F = [0] * 256
ECC_B = [0] * 256
for i in range(256):
    j = (i << 1) ^ (0x11D if i & 0x80 else 0)
    ECC_F[i] = j & 0xFF
    ECC_B[i ^ (j & 0xFF)] = i


def edc(data):
    e = 0
    for b in data:
        e = (e >> 8) ^ EDC_TABLE[(e ^ b) & 0xFF]
    return e


def ecc_block(sector, major_count, minor_count, major_mult, minor_inc, dest):
    size = major_count * minor_count
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        ecc_a = ecc_b = 0
        for _ in range(minor_count):
            temp = sector[0xC + index]
            index += minor_inc
            if index >= size:
                index -= size
            ecc_a ^= temp
            ecc_b ^= temp
            ecc_a = ECC_F[ecc_a]
        ecc_a = ECC_B[ECC_F[ecc_a] ^ ecc_b]
        sector[dest + major] = ecc_a
        sector[dest + major + major_count] = ecc_a ^ ecc_b


def raw_sector(lba, data, submode):
    sector = bytearray(2352)
    sector[0:12] = b"\x00" + b"\xff" * 10 + b"\x00"
    sector[16:24] = bytes([0, 0, submode, 0]) * 2       # file, channel, submode, coding: twice
    sector[24:24 + len(data)] = data
    struct.pack_into("<I", sector, 0x818, edc(sector[16:0x818]))
    ecc_block(sector, 86, 24, 2, 86, 0x81C)              # P parity, the header counted as zeros (Mode 2)
    ecc_block(sector, 52, 43, 86, 88, 0x8C8)             # Q parity
    minutes, rest = divmod(lba + 150, 75 * 60)
    seconds, frames = divmod(rest, 75)
    bcd = lambda v: (v // 10) << 4 | v % 10
    sector[12:16] = bytes([bcd(minutes), bcd(seconds), bcd(frames), 2])
    return bytes(sector)


def disc():
    exe = ps_exe(program())
    # The boot file is named like a game's, with a serial no game has: a PS1 drive (and Beetle PSX, like it) reads
    # nothing from a disc it takes for unlicensed, and Beetle tells a licensed one by that name's region (SLUS: NTSC-U)
    cnf = b"BOOT = cdrom:\\SLUS_999.99;1\r\nTCB = 4\r\nEVENT = 10\r\nSTACK = 801FFFF0\r\n"
    root_lba, cnf_lba, exe_lba = 22, 23, 24
    total = 600  # sectors: a small disc, room after the files
    files = [(b"SLUS_999.99;1", exe_lba, exe), (b"SYSTEM.CNF;1", cnf_lba, cnf)]

    root = dir_record(b"\x00", root_lba, 2048, True) + dir_record(b"\x01", root_lba, 2048, True)
    for name, lba, data in files:
        root += dir_record(name, lba, len(data), False)

    path_l = bytes([1, 0]) + struct.pack("<I", root_lba) + struct.pack("<H", 1) + b"\x00\x00"
    path_m = bytes([1, 0]) + struct.pack(">I", root_lba) + struct.pack(">H", 1) + b"\x00\x00"

    pvd = bytearray(2048)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1
    pvd[8:40] = pad("PLAYSTATION", 32)
    pvd[40:72] = pad("MANDRAKE_TEST", 32)
    pvd[80:88] = both32(total)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(2048)
    pvd[132:140] = both32(len(path_l))
    struct.pack_into("<I", pvd, 140, 18)
    struct.pack_into(">I", pvd, 148, 19)
    pvd[156:190] = dir_record(b"\x00", root_lba, 2048, True)
    pvd[190:318] = pad("", 128)          # volume set
    pvd[318:446] = pad("MANDRAKE", 128)  # publisher
    pvd[446:574] = pad("", 128)          # data preparer
    pvd[574:702] = pad("MANDRAKE", 128)  # application
    pvd[702:813] = pad("", 111)          # copyright, abstract and bibliographic files
    for at in (813, 830, 847, 864):      # creation, modification, expiration, effective dates
        pvd[at:at + 17] = DATE17
    pvd[881] = 1

    terminator = bytearray(2048)
    terminator[0] = 255
    terminator[1:6] = b"CD001"
    terminator[6] = 1

    sectors = {16: (bytes(pvd), 0x09), 17: (bytes(terminator), 0x89), 18: (path_l, 0x89), 19: (path_m, 0x89),
               root_lba: (root, 0x89)}
    for _, lba, data in files:
        count = (len(data) + 2047) // 2048
        for k in range(count):
            sectors[lba + k] = (data[k * 2048:(k + 1) * 2048], 0x89 if k == count - 1 else 0x08)
    image = bytearray()
    for lba in range(total):
        data, submode = sectors.get(lba, (b"", 0x08 if lba >= 16 else 0x00))
        image += raw_sector(lba, data, submode)
    return bytes(image)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: tools/make-test-disc.py <folder>")
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    (out / "mandrake-test.bin").write_bytes(disc())
    (out / "mandrake-test.cue").write_text(
        'FILE "mandrake-test.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
    print(out / "mandrake-test.cue")


if __name__ == "__main__":
    main()
