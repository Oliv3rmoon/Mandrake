#!/usr/bin/env bash
# Mandrake - builds Beetle PSX (libretro's port of Mednafen's PlayStation emulator, GPL-2.0-or-later) with its
# software renderer, as a static archive that lives in the same app as SwanStation.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   tools/build-beetle.sh        -> build/beetle-ps5/libbeetle.a, the libretro API renamed beetle_retro_*
#   tools/build-beetle.sh host   -> build/beetle-host/: the same core for this computer, linked into the headless
#                                   check of tests/core (tests/test_beetle.py runs it)
#
# Source: third_party/beetle-psx, libretro/beetle-psx-libretro pinned by its submodule commit. The build takes the
# commit's files (git archive), so the submodule stays pristine and the PS5 and host builds never share objects.
# Software renderer only: no Vulkan or OpenGL renderer (HAVE_HW=0), no lightrec recompiler (it needs memory it can
# write and run, which a PS5 title cannot map), no physical CD-ROM access. The interpreter is fast enough on the
# PS5's CPU, and the core's built-in OpenBIOS (PCSX-Redux, MIT) starts games when no BIOS of the player's is found.
#
# Like SwanStation's (tools/build-swanstation.sh), the archive is self-contained: every object is linked into one
# relocatable object, every symbol but the libretro API is made local, and the API is renamed beetle_retro_*. Both
# cores carry their own libretro-common, zlib-style decoders and CHD reader, and none of it may meet.

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
target=${1:-ps5}
case $target in ps5|host) ;; *)
    echo "usage: tools/build-beetle.sh [ps5|host]" >&2
    exit 2
esac
core="$root/third_party/beetle-psx"
out="$root/build/beetle-$target"
jobs=${BUILD_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}

[[ -f $core/Makefile && -f $core/libretro.c ]] || {
    echo "Beetle PSX source missing: run 'git submodule update --init third_party/beetle-psx'" >&2
    exit 2
}
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
patches=("$root"/tools/patches/beetle-psx-*.patch)
[[ -e ${patches[0]} ]] || patches=()
objects="$out/src/libbeetle-objects.a"
result="$out/libbeetle.a"
[[ $target == host ]] && result="$out/beetle-headless"

# The core's build switches, the same for the PS5 and the host check: the software renderer, the interpreter, CHD and
# PBP images, its threads (the CD's read-ahead and the memory card's writes run on them).
revision=$(git -C "$core" rev-parse HEAD)
switches=(HAVE_HW=0 HAVE_OPENGL=0 HAVE_VULKAN=0 HAVE_LIGHTREC=0 HAVE_CDROM=0 HAVE_CHD=1 NEED_THREADING=1
    LINK_STATIC_LIBCPLUSPLUS=0 DEBUG=0 GIT_VERSION=" ${revision:0:7}" MANDRAKE_ARCHIVE=libbeetle-objects.a)

if [[ $target == ps5 ]]; then
    sdk="${PS5_PAYLOAD_SDK:-$root/.deps/native/ps5-payload-sdk}"
    [[ -d $sdk/target/include ]] || bash "$root/tools/setup-native-dependencies.sh" >/dev/null
    export PS5_PAYLOAD_SDK=$sdk USE_CCACHE=0
    llvm=$(dirname "$(command -v llvm-ar-18 || command -v llvm-ar)")
    ar="$llvm/llvm-ar-18"; [[ -x $ar ]] || ar="$llvm/llvm-ar"
    objcopy="$llvm/llvm-objcopy-18"; [[ -x $objcopy ]] || objcopy="$llvm/llvm-objcopy"
    nm="$llvm/llvm-nm-18"; [[ -x $nm ]] || nm="$llvm/llvm-nm"
    lld=$(command -v ld.lld-18 || command -v ld.lld)
    # FLAGS through the environment: the Makefile appends its own with +=. EXTRA_FLAGS goes after the platform's own:
    # -UHAVE_MMAP keeps disc images read with read(), as SwanStation reads them (a game on an NFS share or an NTFS
    # disk is not a file the kernel can map).
    flags="-O2 -fPIC -w -ffunction-sections -fdata-sections -march=znver2"
    make_args=(platform=unix IS_X86=1 MANDRAKE_PS5=1 "${switches[@]}" EXTRA_FLAGS="-UHAVE_MMAP"
        CC="sh $root/tooling/prospero-clang18" CXX="sh $root/tooling/prospero-clang18 -x c++" AR="$ar")
    what="software renderer"
else
    cc=${CC:-cc}
    platform=unix
    [[ $(uname -s) == Darwin ]] && platform=osx
    flags="-O2 -fPIC -w"
    make_args=(platform=$platform "${switches[@]}" CC="$cc" CXX="${CXX:-c++}" AR="${AR:-ar}")
    what="for this computer"
fi

# The core is built again only when its revision, this script, the archive rules or a patch changed.
stamp="$out/.stamp"
want="$revision $(cat "$0" "$root/tools/beetle-archive.mk" ${patches[@]+"${patches[@]}"} | sha256 | cut -c1-16)"
if [[ ! -f $objects || ! -f $stamp || $(cat "$stamp") != "$want" ]]; then
    rm -rf "$out"
    mkdir -p "$out/src"
    # The pinned commit's files, with Mandrake's patches (tools/patches/beetle-psx-*.patch, in order): the PS5's libc.
    git -C "$core" archive --format=tar HEAD | tar -x -C "$out/src"
    for patch in ${patches[@]+"${patches[@]}"}; do
        echo "==> [beetle] applying $(basename "$patch")"
        (cd "$out/src" && patch -p1 --quiet <"$patch")
    done
    echo "==> [beetle] building Beetle PSX ${revision:0:12}, $what ($jobs jobs)"
    CFLAGS="$flags" CXXFLAGS="$flags" make -s -C "$out/src" -f Makefile -f "$root/tools/beetle-archive.mk" \
        -j"$jobs" "${make_args[@]}" Mandrake-archive
    echo "$want" >"$stamp"
fi

if [[ $target == ps5 ]]; then
    if [[ ! -f $result || $result -ot $objects ]]; then
        echo "==> [beetle] isolating its symbols"
        work="$out/isolate"
        rm -rf "$work"
        mkdir -p "$work"
        "$lld" -r --whole-archive "$objects" -o "$work/beetle.o"
        # keep only the libretro API global, then rename it
        "$nm" --defined-only --extern-only "$work/beetle.o" | awk '{print $NF}' | grep -E '^retro_' | sort -u \
            >"$work/api.txt"
        [[ -s $work/api.txt ]] || { echo "no retro_* symbols in Beetle PSX" >&2; exit 1; }
        sed 's/.*/& beetle_&/' "$work/api.txt" >"$work/rename.txt"
        "$objcopy" --keep-global-symbols="$work/api.txt" "$work/beetle.o" "$work/beetle-local.o"
        "$objcopy" --redefine-syms="$work/rename.txt" "$work/beetle-local.o" "$work/beetle-ps5.o"
        # What the object still needs from outside: the C library, its maths and threads, never another library.
        "$nm" --undefined-only "$work/beetle-ps5.o" | awk '{print $NF}' | sort -u >"$work/undefined.txt"
        rm -f "$result"
        "$ar" rcs "$result" "$work/beetle-ps5.o"
        echo "==> [beetle] $(wc -l <"$work/api.txt" | tr -d ' ') libretro functions as beetle_retro_*," \
            "$(wc -l <"$work/undefined.txt" | tr -d ' ') runtime symbols left for the app's link"
    fi
    echo "==> [beetle] $result"
    exit 0
fi

# ---- host: the headless frontend of tests/core, linked to the core as it is (its API keeps the retro_ names).
echo "==> [beetle] the headless check (tests/core/beetle_headless.c)"
"$cc" -std=c11 -O2 -g -Wall -Wextra -Werror -I"$out/src/libretro-common/include" -I"$root/src/core" \
    "$root/tests/core/beetle_headless.c" "$root/src/core/beetle_options.c" "$objects" -lpthread -lm -o "$result"
echo "==> [beetle] $result"
