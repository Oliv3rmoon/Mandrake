#!/usr/bin/env python3
# Mandrake - Beetle PSX's core, built for this computer and run headless on a test disc made for it.
# SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds the core the PS5 app links (tools/build-beetle.sh host: the same pinned beetle-psx-libretro commit, the same
patches and build switches, the software renderer) with the headless frontend of tests/core/beetle_headless.c, makes
the test disc (tools/make-test-disc.py: a homebrew program written for this check, no game and no BIOS of anybody's)
and boots it: the disc must start, draw its picture frame after frame at 1x and 2x, make sound, take and replay a
save state, and start again in the same process, with the options the app sets (src/core/beetle_options.c), which
the core must all offer.

Beetle PSX needs no BIOS: without one it starts discs with the OpenBIOS built into it (PCSX-Redux's, MIT). To boot the
disc with a real BIOS as well, set MANDRAKE_PS1_BIOS to a folder holding your own dump: scph5501.bin (or any file
that is that BIOS) for this NTSC-U disc. That test is skipped, and says so, when it is not set. No BIOS is ever
downloaded or kept by this repository."""

import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADLESS = ROOT / "build/beetle-host/beetle-headless"
# tools/make-test-disc.py makes the same image every time: a change to it is a change to this check
DISC_SHA256 = "102ae5502eae53351326644f15129da4f5bafab9232ad103ec934fc52218e6c8"


def run(command, **kwargs):
    return subprocess.run(command, capture_output=True, text=True, errors="replace", check=False, **kwargs)


class BeetleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (ROOT / "third_party/beetle-psx/libretro.c").is_file():
            raise unittest.SkipTest("needs the Beetle PSX submodule: git submodule update --init third_party/beetle-psx")
        build = run(["bash", str(ROOT / "tools/build-beetle.sh"), "host"])
        if build.returncode != 0:
            raise AssertionError("tools/build-beetle.sh host failed:\n" + build.stdout[-4000:] + build.stderr[-4000:])
        print(build.stdout.strip().splitlines()[-1])
        cls.work = tempfile.TemporaryDirectory()
        disc = run(["python3", str(ROOT / "tools/make-test-disc.py"), cls.work.name])
        if disc.returncode != 0:
            raise AssertionError("tools/make-test-disc.py failed:\n" + disc.stderr[-3000:])
        cls.cue = Path(cls.work.name) / "mandrake-test.cue"

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def boot(self, system, *extra):
        with tempfile.TemporaryDirectory() as saves:
            result = run([str(HEADLESS), str(self.cue), str(system), saves, *extra], timeout=600)
        print()
        print(result.stdout.strip())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr[-3000:])
        self.assertIn("PASS", result.stdout)
        return result.stdout

    def test_the_disc_is_the_pinned_one(self):
        image = (Path(self.work.name) / "mandrake-test.bin").read_bytes()
        self.assertEqual(hashlib.sha256(image).hexdigest(), DISC_SHA256)

    def test_boots_on_the_built_in_openbios(self):
        with tempfile.TemporaryDirectory() as empty:
            output = self.boot(empty)
        self.assertIn("Firmware is missing", output)  # no BIOS in the folder: the core's own OpenBIOS ran the disc
        self.assertIn("the picture is 320x240", output)

    def test_twice_the_internal_resolution(self):
        with tempfile.TemporaryDirectory() as empty:
            output = self.boot(empty, "900", "2x")
        self.assertIn("the picture is 640x480", output)
        self.assertIn("its white square is 64x64", output)

    def test_with_your_own_bios(self):
        given = os.environ.get("MANDRAKE_PS1_BIOS", "")
        if not given:
            self.skipTest("no BIOS given: set MANDRAKE_PS1_BIOS to the folder holding your own PS1 BIOS (scph5501.bin "
                          "for this NTSC-U disc) to boot the test disc with it too; the tests above booted it on "
                          "Beetle PSX's built-in OpenBIOS")
        folder = Path(given)
        if not folder.is_dir():
            self.fail(f"MANDRAKE_PS1_BIOS is {given}, which is not a folder")
        # the BIOS's start-up (the PlayStation logo) runs first: room for it
        output = self.boot(folder, "1800")
        self.assertIn("Firmware found", output)


if __name__ == "__main__":
    unittest.main()
