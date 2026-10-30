<p align="center">
  <img src="sce_sys/icon0.png" width="128" alt="Mandrake icon">
</p>

<h1 align="center">Mandrake</h1>

<p align="center">
  <strong>PS1 emulation for PS5 homebrew, on a cover-flow shelf, in Nightshade's black and red.</strong>
</p>

<p align="center">
  Formerly Nightshade Station
</p>

<p align="center">
  <a href="https://github.com/Oliv3rmoon/Mandrake/releases/latest"><img src="https://img.shields.io/github/v/release/Oliv3rmoon/Mandrake?style=for-the-badge&label=download&color=e5484d&labelColor=0d0d0f" alt="Download the latest release"></a>
  <a href="https://discord.gg/pSqgRU3YP9"><img src="https://img.shields.io/badge/discord-nightly-0d0d0f?style=for-the-badge&logo=discord&logoColor=e5484d" alt="Discord: nightly"></a>
  <a href="https://github.com/Oliv3rmoon/Mandrake/stargazers"><img src="https://img.shields.io/github/stars/Oliv3rmoon/Mandrake?style=for-the-badge&logo=github&logoColor=e5484d&color=e5484d&labelColor=0d0d0f" alt="GitHub stars"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0--or--later-0d0d0f?style=for-the-badge&labelColor=0d0d0f&color=e5484d" alt="GPL-3.0-or-later"></a>
  <a href="https://buymeacoffee.com/oliv3rmoon"><img src="https://img.shields.io/badge/buy%20me%20a%20coffee-0d0d0f?style=for-the-badge&logo=buymeacoffee&logoColor=e5484d" alt="Buy me a coffee"></a>
</p>

Mandrake is a PlayStation 1 emulator that installs as an app on the PS5 home screen, and a Nightshade app by **[Oliv3rmoon](https://github.com/Oliv3rmoon)**. Your games sit on a cover-flow shelf, run at up to 16x resolution through Vulkan, and come from the console, a drive, an NFS share, an NTFS disk or your own discs. The latest release is **1.6.1**; what changed is in the [changelog](docs/CHANGELOG.md).

> [!NOTE]
> **1.7.0 and 1.8.0 are test releases.** They add a second emulator and put the picture settings on both. They are built and checked by the host tests and have not been run on a console yet. They are marked pre-release on the [Releases](https://github.com/Oliv3rmoon/Mandrake/releases) page: try them, and tell me what breaks.

## Why Mandrake

- **A shelf for your games.** Cover flow with 3D jewel cases and a reflection under every cover. Covers are matched by each disc's serial and download on the console, or use your own. It plays `.cue`/`.bin`, `.chd`, `.pbp` (multi-disc too), `.iso`, `.img`, `.mdf`, `.ccd` and `.m3u` playlists.
- **Two emulators, chosen per game** *(test releases 1.7.0 and 1.8.0)*. Every game runs on the default emulator; set **Settings > System > Emulator** to **Beetle PSX** (slower, closer to the real console) for one game or for all of them when a game runs badly. In 1.8.0 the shaders, AMD FSR and CAS, generated frames and supersampling work on both, and each keeps its own saves.
- **Sharp on a 4K TV.** Up to 16x internal resolution with PGXP to keep polygons still, CRT, NTSC and LCD shaders, AMD FSR 3 frame generation, widescreen, and scanlines.
- **Play from wherever your games are.** Drives (a USB or extended drive), NFS shares, NTFS disks, and your own PS1 discs from a USB CD drive, copied to `.bin` and `.cue` and added to the shelf. A disc daemon does it with the app closed.
- **Everything around the game.** RetroAchievements with hardcore mode, cheats found and downloaded for each game, ten save-state slots with pictures, quick resume, rewind and fast forward, profiles, a memory card manager, and settings from your phone.
- **Nightshade's look.** The default theme is black and red, with near-black panels, one red accent and a low red glow in the sky. Classic, Neon Arcade, Memory Card, Memory Card Dark, Record Shelf and Grayrat are still in **Settings > Library > Theme**.
- **Updates from its own releases.** It asks this repository for a newer release and installs it, after checking `PPSA98510.zip` against `SHA256SUMS`. Your memory cards, saves, save states, covers and settings carry over.

## Mandrake and SwanStationPS5

Mandrake installs over SwanStationPS5, so here is where they differ. A dash means the other app's README doesn't list it.

| | Mandrake | SwanStationPS5 |
| --- | --- | --- |
| Latest release | 1.6.1 (1.7.0 and 1.8.0 are test releases) | 1.6.1 |
| Emulators | SwanStation, and Beetle PSX per game (test releases, 1.7.0 and later) | SwanStation only |
| Shaders, frame generation, supersampling | Yes, on both emulators in the 1.8.0 test release | Yes, on SwanStation |
| Game Boy LCD shader | Kept | Removed in 1.6.0 |
| Default theme | Nightshade, black and red | Neon Arcade |
| Installs from Nightshade's Settings > Apps | Yes | — |
| Where updates come from | This repository's GitHub releases | darkxex's GitHub releases |
| Title ID and data folder | `PPSA98510`, `/data/SwanStationPS5` | The same |
| Games, drives, NFS, NTFS, USB CD drive, RetroAchievements, cheats, PC tool | The same | The same |

## Install

1. Close the app if it is running.
2. Download `PPSA98510.zip` from the [latest release](https://github.com/Oliv3rmoon/Mandrake/releases/latest) and unzip it. For a test release, take it from the [Releases](https://github.com/Oliv3rmoon/Mandrake/releases) page instead.
3. Copy its `PPSA98510` folder over your current one (usually `/data/homebrew/PPSA98510`), replacing the files, with permissions `777` as [Getting started](docs/GUIDE.md#getting-started) describes. Nightshade's **Settings > Apps** can install it for you instead.

It keeps the original's title ID, so it installs over the original and your memory cards, saves, save states, covers and settings stay as they are (they live in `/data/SwanStationPS5`). The home screen calls it **Mandrake**. At its first start it asks, as after every update, whether to start from the factory settings: **✕** does, and opens in the Nightshade theme; **○** keeps your settings, theme included (pick **Nightshade** in Settings > Library > Theme). The original's updater reads another repository's releases, so this first Mandrake is copied by hand; from it on, **Settings > About > Check for updates** installs the newest full release. To go back, copy an [official release](https://github.com/darkxex/SwanStationPS5/releases) over it the same way.

Mandrake ships no games and no BIOS: use dumps of discs and consoles you own. New to it? [Getting started](docs/GUIDE.md#getting-started) covers the games folder, the BIOS and unlocking `/data`.

## Docs

- **[Guide](docs/GUIDE.md):** [Getting started](docs/GUIDE.md#getting-started) · [Folders for games and BIOS](docs/GUIDE.md#supported-folders-for-games-and-bios) · [Second emulator](docs/GUIDE.md#beetle-psx-the-second-emulator) · [USB CD drive](docs/GUIDE.md#playing-from-a-usb-cd-drive) · [NFS](docs/GUIDE.md#games-on-an-nfs-server) · [NTFS](docs/GUIDE.md#games-on-an-ntfs-disk) · [Covers](docs/GUIDE.md#covers-for-games-without-one) · [Controls](docs/GUIDE.md#controls) · [Settings](docs/GUIDE.md#settings) · [RetroAchievements](docs/GUIDE.md#retroachievements) · [Cheats](docs/GUIDE.md#cheats) · [PC tool](docs/GUIDE.md#pc-tool) · [Known limitations](docs/GUIDE.md#known-limitations) · [Building from source](docs/GUIDE.md#building-from-source)
- **[Changelog](docs/CHANGELOG.md)**, [Privacy policy](PRIVACY.md), [Third-party notices](THIRD_PARTY_NOTICES.md) and [Contributing](CONTRIBUTING.md).

## Requests, bugs and support

- **Ask for a fix or a feature:** [open an issue](https://github.com/Oliv3rmoon/Mandrake/issues/new/choose), or ask in **[nightly on Discord](https://discord.gg/pSqgRU3YP9)**.
- **Bugs in my releases:** open an issue here with `/data/SwanStationPS5/logs/SwanStationPS5.log`, copied right after the problem. Never attach games, BIOS files or keys.
- **If my work helps you,** you can [buy me a coffee](https://buymeacoffee.com/oliv3rmoon).

## The Nightshade family

Mandrake is one of seven repositories in the Nightshade family, all by [Oliv3rmoon](https://github.com/Oliv3rmoon): the same black-and-red look, one Discord.

| App | What it is |
| --- | --- |
| [**Nightshade**](https://github.com/Oliv3rmoon/Nightshade) | The Switch emulator for PS5 homebrew |
| [**Physalis**](https://github.com/Oliv3rmoon/Physalis) | Live TV, movies and Plex on the PS5 |
| [**Datura**](https://github.com/Oliv3rmoon/Datura) | Game streaming from your PC to the PS5 |
| **Mandrake** (this repo) | PS1 emulation, with two emulators |
| [**Dulcamara**](https://github.com/Oliv3rmoon/Dulcamara) | RPG Maker 2000 and 2003 games, and Ren'Py visual novels |
| [**Solanum**](https://github.com/Oliv3rmoon/Solanum) | A store for open-source PS5 homebrew, with its catalog [on the web](https://oliv3rmoon.github.io/Solanum-Catalog/) |
| [**Belladonna**](https://github.com/Oliv3rmoon/Belladonna) | Nightshade's PS5 Vulkan driver |

## Built on

[SwanStationPS5](https://github.com/darkxex/SwanStationPS5) 1.5.9 by darkxex, with the fixes of its 1.6.1, running [SwanStation](https://github.com/libretro/swanstation) and, in the 1.7.0 and 1.8.0 test releases, [Beetle PSX](https://github.com/libretro/beetle-psx-libretro).

## Credits

SwanStationPS5 started as a fork of [PSXS5](https://github.com/SynoPiia/PSXS5) by SynoPiia and is built on the work of a lot of people. Thank you all.

* **The SwanStation and DuckStation contributors**, for [SwanStation](https://github.com/libretro/swanstation), the emulator SwanStationPS5 runs.
* **Mednafen's authors and the Beetle PSX contributors**, for [Beetle PSX](https://github.com/libretro/beetle-psx-libretro), Mandrake's second emulator, and **the PCSX-Redux authors** for the OpenBIOS built into it.
* **Mihawk-99**, for the PS5 port of Mesa's RADV Vulkan driver.
* **BlackBearReloaded**, for [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate), the build pipeline, runtime and deploy tooling that turn SwanStationPS5 into a home-screen app.
* **John Törnblom**, for the [ps5-payload-dev](https://github.com/ps5-payload-dev) SDK, and PacBrew's SDL2 and libcurl ports.
* **SvenGDK**, for [SharpProspero](https://github.com/SvenGDK/SharpProspero), whose ELF converter and FSELF writer sign `eboot.bin`.
* **The etaHEN team**, for [etaHEN](https://github.com/etaHEN/etaHEN): the HEN, its FTP server and ELF loader, and the on-demand jailbreak SwanStationPS5 asks for to reach `/data`. Thanks also to the kstuff authors and maintainers.
* **drakmor**, for [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus), which puts SwanStationPS5 on the home screen, and **VoidWhisper**, for ShadowMount, which it's based on.
* **ArkSama / Team PHU and mpereiraesaa**, for [PS5-Lapy-JB-Daemon](https://github.com/ArkSama/PS5-Lapy-JB-Daemon), the fallback sandbox elevation.
* **[RetroAchievements](https://retroachievements.org)**, for the achievements, the community that makes the sets, and its [rcheevos](https://github.com/RetroAchievements/rcheevos) library.
* **[xlenore/psx-covers](https://github.com/xlenore/psx-covers)**, for the covers the shelf downloads.
* **sodaraptor** ([@sodaraptor_dev](https://sodaraptor.itch.io/psx-jewel-case)), for the PSX jewel case model and textures the shelf's 3D cases are built from: [Hypnagogia 無限の夢 Boundless Dreams Jewel Case](https://sketchfab.com/3d-models/hypnagogia-boundless-dreams-jewel-case-20e4780167b6441fb364060c79870378) (CC-BY 4.0), with each game's own cover in place of its art.
* **The [libretro-database](https://github.com/libretro/libretro-database) contributors**, for the PlayStation cheat library.
* **Swordpdf**, for [PS5SX2](https://github.com/Swordpdf/PS5SX2). Its shelf and project page inspired SwanStationPS5's, and studying it and the other PS5 emulator ports showed how a native emulator title runs on the console.
* **Ronnie Sahlberg**, for [libnfs](https://github.com/sahlberg/libnfs) (LGPL-2.1-or-later), the NFS client behind *Games on an NFS server*, and **tutifrutas**, for the [SwanStationPS5 fork](https://github.com/tutifrutas/SwanStationPS5) whose console configuration of it is used here.
* **Swordpdf and AMD**, for the frame interpolation behind *Generate frames*: [PS5SX2](https://github.com/Swordpdf/PS5SX2)'s port of AMD FSR 3 (FidelityFX SDK), as first integrated in PSXS5 by SynoPiia.
* **Al-Azif**, for [ps-disc-detect](https://github.com/Al-Azif/ps-disc-detect) (GPL-3.0): the disc installation follows its way of reaching the drive through the CAM pass-through and of dumping a CD to `.bin` and `.cue` with `READ CD`. And **Swordpdf**, for [PS5SX2](https://github.com/Swordpdf/PS5SX2)'s disc code and its rule that a command only counts as done when the drive handed over every byte.
* **crashGG** ([github.com/crashGG](https://github.com/crashGG)), for MMPX 3.0 and MMPX 3.2, the texture filters of the same names, and **Morgan McGuire and Mara Gagiu**, for [MMPX](https://casual-effects.com/research/McGuire2021PixelArt/), the pixel art filter they are built on (MMPX Basic).
* **Hans-Kristian Arntzen (Themaister)**, for the NTSC shaders, **TroggleMonkey and Hyllian** for CRT Royale and CRT Royale Fast, **Hyllian** for CRT Hyllian NTSC, **Mattias Gustavsson** and **hunterk** for CRT Newpixie, **Mattias** for CRT Mattias, **Timothy Lottes**, **davej**, **Lior Halphon** and **Greg Hogan** for the CRT Lottes, CRT Pi, Game Boy LCD and zfast LCD shaders, and **Gigaherz** for LCD3x.
* **Sam Lantinga and the SDL contributors** for [SDL2](https://www.libsdl.org/), **Daniel Stenberg** for [curl](https://curl.se/), **Sean Barrett** for [stb](https://github.com/nothings/stb), **Rasmus Andersson** for the [Inter](https://github.com/rsms/inter) font, **Google and Adobe** for [Noto Sans JP](https://github.com/notofonts/noto-cjk) (the Japanese text), and **Doug Lea** for [dlmalloc](https://gee.cs.oswego.edu/dl/html/malloc.html).
* **Microsoft's [DirectXTex](https://github.com/microsoft/DirectXTex)**, **[Pillow](https://python-pillow.org/)** and **[7-Zip](https://www.7-zip.org/)**, for the art pipeline and the PC tool.
* Developed with the help of [Claude Code](https://claude.com/claude-code).

Licences for everything above are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Thanks

- **[SwanStationPS5](https://github.com/darkxex/SwanStationPS5)**, by **darkxex** (AngelXex), who
  made it and keeps it going, with its contributors and testers ([its Discord](https://discord.gg/MseFnnfYzt)).
  Mandrake is his app with a new name, a new look and a new update source; the credit for
  it is his.
- **[PSXS5](https://github.com/SynoPiia/PSXS5)**, by **SynoPiia**, where SwanStationPS5 started.
- **The [SwanStation](https://github.com/libretro/swanstation) and [DuckStation](https://github.com/stenzek/duckstation)
  contributors**, for the emulator it runs, and everyone in [Credits](#credits) above.
- **[Nightshade](https://github.com/Oliv3rmoon/Nightshade)**, whose palette and art code this release reuses.

## License

Mandrake is an unofficial release. SwanStationPS5 is made by darkxex and its contributors; neither they nor Sony endorse Mandrake. It is free software under the GNU General Public License, version 3 or later, as SwanStationPS5 is ([LICENSE](LICENSE)).

[Privacy policy](PRIVACY.md) · [Third-party notices and shipped cores](THIRD_PARTY_NOTICES.md)

SwanStationPS5 is free software under the GPL-3.0-or-later. SwanStation is GPL-3.0.

## Disclaimer

SwanStationPS5 is not affiliated with or endorsed by Sony Interactive Entertainment. "PlayStation" is a registered trademark of Sony Interactive Entertainment Inc. It is used here only to describe what the emulator does.

SwanStationPS5 doesn't include and won't provide BIOS files, games or keys. Only use dumps of discs and consoles you own.

## Help it grow

Like Mandrake? **[Star it](https://github.com/Oliv3rmoon/Mandrake)** and **[join the Discord](https://discord.gg/pSqgRU3YP9)**.
