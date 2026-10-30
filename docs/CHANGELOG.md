# Changelog

What changed in each release of Mandrake, newest first. Mandrake was called Nightshade Station
until 1.6.0, and the entries for those releases keep the name they had. A release's notes on GitHub
are in `docs/releases/<version>.md`. What changed in SwanStationPS5 before Nightshade Station is in
[its releases](https://github.com/darkxex/SwanStationPS5/releases).

## Mandrake 1.7.0: Beetle PSX, a second emulator

A second PlayStation emulator, [Beetle PSX](https://github.com/libretro/beetle-psx-libretro)
(Mednafen's PlayStation), for the games SwanStation runs badly. SwanStation stays the default, and
every game runs on it until you choose otherwise. Its version, 1.7.0 (contentVersion 01.007.000),
is the next minor after 1.6.1. What changed:

- **Choose the emulator, for one game or all.** **Settings > System > Emulator**: SwanStation or
  Beetle PSX. In *All games* it sets every game without settings of its own; in *This game*
  (**L2** / **R2** in Settings, from the game or from the shelf) only that game. It applies the next
  time the game starts. The shelf shows a **Beetle PSX** tag on the games set to it, and the game
  menu shows which emulator is running.
- **Beetle PSX's settings.** **Settings > Graphics > Beetle PSX**: resolution (Native, the default,
  or 2x), dithering (on, as the PS1 draws) and its widescreen hack (off). They apply at once. Region,
  fast CD loading, the PS1 startup intro, players (multitap), the controller and its analog-mode
  combination, rumble, light guns, the NeGcon and the mouse, cheats, RetroAchievements, rewind,
  run-ahead, fast forward, quick resume, auto-save and the screen's framing, scanlines and colours
  work with both emulators.
- **Saves kept apart.** Each emulator keeps its own save states (Beetle PSX's in
  `states/beetle/`) and memory cards (`saves/beetle/`), so a save state of one is never loaded into
  the other, and switching a game's emulator never hands it the other's card. The shelf offers
  Continue only from the emulator the game will start on. The memory card manager and the phone page
  show each game's cards and states of its emulator; the manager lists Beetle PSX's cards with its
  name.
- **What only SwanStation does.** Beetle PSX draws its picture on the CPU, so the shaders, AMD FSR,
  AMD CAS, generated frames, supersampling and HD texture packs, which work on SwanStation's GPU
  picture, don't apply to it, nor do SwanStation's own settings (renderer, up to 16x, PGXP, true
  colour, its aspect ratio and widescreen hack, its console and controller options). For a game on
  Beetle PSX, Settings shows those rows locked, says they are SwanStation's only and why. Start BIOS
  always runs SwanStation, as Beetle PSX needs a disc.
- **Its BIOS.** Beetle PSX reads the same BIOS folder, `/data/SwanStationPS5/bios/`, and uses
  `scph5501.bin` (North America), `scph5500.bin` (Japan) or `scph5502.bin` (Europe), by those names
  or by their content. With none for the game's region it starts the game on its built-in OpenBIOS,
  and a notice says so.
- **Scanlines on CPU pictures.** Settings > Display > CRT scanlines now darkens the lines of a
  picture the frontend gets drawn on the CPU too, as Beetle PSX's is.
- **Built and checked.** `tools/build-beetle.sh` builds Beetle PSX from a pinned commit, software
  renderer only, as one self-contained archive beside SwanStation's. A host check builds the same core
  for a computer and boots a test disc on it, at 1x and 2x: a small program and disc image made by
  `tools/make-test-disc.py`, no game and no BIOS. CI runs it on every build.

Built and checked by the host tests; not yet run on a console.

## Mandrake 1.6.1: renamed from Nightshade Station

Renamed to Mandrake; same app, same title ID, your settings stay. Mandrake, a Nightshade app, is the
mandrake, a relative of the nightshade. Its version, 1.6.1 (contentVersion 01.006.001), is the next
patch after 1.6.0. What changed:

- **The name.** The PS5 home screen, the shelf's header, the About page, the update texts, the
  notifications and every message, tip and help say Mandrake, in all five languages, and so does
  the phone page. The About page's QR code opens Oliv3rmoon/Mandrake.
- **Updates.** The app now asks [Oliv3rmoon/Mandrake](https://github.com/Oliv3rmoon/Mandrake/releases)
  for a newer release. The release's files are named as before: `PPSA98510.zip`,
  `SwanStationPS5-DiscDaemon.elf` and `SHA256SUMS`. Nightshade Station 1.6.0 takes release files only
  from its own old address, which GitHub now answers with Mandrake's, so it does not offer this
  release itself: copy it by hand or install it from Nightshade's Settings > Apps; from it on, the
  updates come by themselves.
- **What stays.** The title ID (PPSA98510) and the data folder (`/data/SwanStationPS5`), so it
  installs over Nightshade Station and keeps your memory cards, saves, save states, covers and
  settings. As after every update, the first start asks whether to start from the factory
  settings: **○** keeps yours, theme included. The disc daemon keeps its file name. The Nightshade
  theme, the art and the credit to SwanStationPS5 and its authors are as they were.

## Nightshade Station 1.6.0: the first Nightshade Station, based on SwanStationPS5 1.5.9

Nightshade Station is Oliv3rmoon's release of SwanStationPS5 by darkxex, built on SwanStation:
SwanStationPS5 1.5.9 with a new name, Nightshade's look and updates from
Oliv3rmoon/Nightshade-Station. Its version, 1.6.0 (contentVersion 01.006.000), is the next minor
after 1.5.9 (01.005.009), so it installs over it as an update. What changed:

- **The name.** The PS5 home screen, the shelf's header, the About page, the update texts, the
  notifications and every message, tip and help say Nightshade Station, in all five languages, and
  so does the phone page. Under the About page's QR code, which opens this repository, are its
  address, the nightly Discord and "Nightshade Station is Oliv3rmoon's release of SwanStationPS5 by
  darkxex, built on SwanStation." "Created by" still says AngelXex.
- **The look.** A seventh theme, Nightshade, in Nightshade's palette (its "nightly" theme):
  near-black pages and panels, one red accent (#e5484d) for the focus, the switches and the selected
  cover, cool white text, an amber for warnings, and a near-black sky with a low red glow behind the
  shelf. It is the default for a new install and after Factory settings; Classic, Neon Arcade,
  Memory Card, Memory Card Dark, Record Shelf and Grayrat are still there. The phone page has the
  same colours.
- **The art.** The icon (a black disc lit red, the nightshade flower at its hub), the home
  background (the disc risen like an eclipsed moon, the nightshade climbing beside it) and the launch
  picture (a disc-shaped ring of red light round the flower) are drawn by
  `tools/render-home-art.py`, in Nightshade's style.
- **Updates from this repository.** The app asks
  [Oliv3rmoon/Nightshade-Station](https://github.com/Oliv3rmoon/Nightshade-Station/releases) for its
  newest release that is not a pre-release, and offers it when its tag is a newer version and it
  carries `PPSA98510.zip` and `SHA256SUMS`, both this repository's own files; the download is
  checked against `SHA256SUMS` before anything is replaced. darkxex's releases are never offered.
- **Release builds.** A version tag pushed to Oliv3rmoon/Nightshade-Station builds
  `PPSA98510.zip` and `SwanStationPS5-DiscDaemon.elf` and publishes them with `SHA256SUMS` as the
  latest release, with a build attestation.
- **What stays.** The title ID (PPSA98510) and the data folder (`/data/SwanStationPS5`), so it
  installs over SwanStationPS5 and keeps your memory cards, saves, save states, covers and settings.
  The disc daemon keeps its file name. Everything else is SwanStationPS5 1.5.9.

Built and checked by the host tests; not yet run on a console.

### Install

Close SwanStationPS5, copy this release's `PPSA98510` folder (from `PPSA98510.zip`) over your
current one (usually `/data/homebrew/PPSA98510`), and start it. SwanStationPS5 doesn't offer this
release itself, so this one is copied by hand (or installed by Nightshade's Settings > Apps); from it
on, the update check applies the next one. To go back, copy an official
[SwanStationPS5 release](https://github.com/darkxex/SwanStationPS5/releases) over it the same way.
