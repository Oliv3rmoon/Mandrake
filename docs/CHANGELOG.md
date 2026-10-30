# Changelog

What changed in each release of Mandrake, newest first. Mandrake was called Nightshade Station
until 1.6.0, and the entries for those releases keep the name they had. A release's notes on GitHub
are in `docs/releases/<version>.md`. What changed in SwanStationPS5 before Nightshade Station is in
[its releases](https://github.com/darkxex/SwanStationPS5/releases).

## Mandrake 1.8.1: nightly, the community on Discord

Mandrake now says where its community is: [nightly](https://discord.gg/pSqgRU3YP9), the Discord of
Mandrake and the other Nightshade apps. Its version, 1.8.1 (contentVersion 01.008.001), is the next
patch after 1.8.0. What changed:

- **Join the community.** **Settings > About** has a new row under **Community**, *Join the
  community*, with the server's name, nightly, and its short link, `discord.gg/pSqgRU3YP9`. Selected,
  its card shows a QR code that opens `https://discord.gg/pSqgRU3YP9` on a phone. The phone page lists
  the row too.
- **Once, after the update.** The question the first start asks after an update (the factory
  settings, or yours) has one more line under its answers: "Join nightly, Mandrake's community on
  Discord: discord.gg/pSqgRU3YP9", and where its QR code is. Either answer puts it away for good: the
  settings file keeps `community_seen=1`, a factory reset keeps it too, and later updates ask their
  question without the line. A new install has no question to ask, so it shows the invitation only on
  About.
- **In all five languages.** The new texts are in French, Portuguese, Spanish and Japanese as well;
  the name and the link are never translated. The Japanese font is unchanged: its subset already had
  every glyph they use.
- **What stays.** Everything else is 1.8.0: both emulators, the picture settings, your settings and
  your saves. The About page's GitHub QR code and its address are where they were.
- **Checked on a computer.** A new host check, `tests/test_community.py` (CI runs it on every build),
  makes the QR code with the app's own encoder (`src/ui/qr.h`, which the phone page's QR code now goes
  through too) and reads it back with a reader of its own: it must open exactly
  `https://discord.gg/pSqgRU3YP9`. The same reader reads the GitHub QR code, which segno made. The
  About row, the one-time line and its flag in the settings file are checked in the sources, and every
  new text in its five languages.

Built and checked by the host tests; not yet run on a console.

## Mandrake 1.8.0: Beetle PSX with every picture setting, and SwanStationPS5 1.6.1's fixes

Beetle PSX without the compromises of 1.7.0, and the fixes of SwanStationPS5 1.6.1. Its version,
1.8.0 (contentVersion 01.008.000), is the next minor after 1.7.0. What changed:

- **The picture settings work on Beetle PSX.** Beetle PSX draws on the CPU; each new picture is now
  uploaded to the GPU and takes the same way to the screen as SwanStation's image. The shaders (CRT
  Royale, the NTSC ones, LCD3x...), AMD FSR 1, AMD CAS, *Generate frames* and supersampling work for a
  game on Beetle PSX, and Settings no longer locks them for it. The colours (brightness, Vivid, Warm...)
  are the GPU's for both emulators. Should the GPU have no room for the picture, it is drawn by the CPU
  as in 1.7.0.
- **Generated frames and Show FPS count Beetle PSX's own frames.** Beetle PSX hands over every frame,
  also the ones that only repeat the last picture (a 30 fps game); those are now found by comparing
  the pictures, so frame generation starts from the game's real rate and *Show FPS* shows it in
  brackets, as with SwanStation.
- **Beetle PSX upscale filter.** Settings > Graphics > Beetle PSX: Off (the default), Sharp pixels,
  Smooth pixels (Scale2x) or xBR. It enlarges a Native picture twice on the CPU before the GPU, the
  shader and the screen take it; a 2x picture is left as it is. 1.7.0 bypassed the prescaler for
  Beetle PSX.
- **The ANALOG button on Beetle PSX.** The light bar's *Analog mode* (red in analog mode, off in
  digital) and *Left stick as D-pad: Auto* (the stick moves the D-pad while the DualShock is in digital
  mode) now follow Beetle PSX's DualShock: its own messages say when the mode changes, and a game the
  database starts in analog mode is known to be in it.
- **PAL games at 60 Hz on Beetle PSX.** SwanStation's *Force NTSC Timings* (Settings > Graphics) is
  given to Beetle PSX too, at once, and its row is no longer locked for a Beetle PSX game.
- **Japanese.** Every Beetle PSX text is in Japanese now, and the Japanese font has the ten kanji it
  lacked (two of them for texts that were already there).
- **SwanStationPS5 1.6.1's fixes.** The sandbox unlock waits for a resident Lapy service before it
  sends the Lapy helper, and gives the request files to LegacyJB and the PS5SX2 Helper before it asks
  etaHEN's command server; the helper is built with Payload SDK v0.42 again. SwanStationPS5 1.6.0 also
  took out the Game Boy LCD shader: Mandrake keeps it.
- **What stays.** SwanStation's GPU renderer, its settings and its picture are as they were. Its CPU
  renderer's pictures take the GPU's way too, with the prescaler they always had, so the shaders now
  work on them.
- **Checked on a computer.** The host check now runs the app's own code on Beetle PSX's frames: the
  picture through the GPU path's preparation at 1x, 2x and with xBR, and, with Vulkan (lavapipe in CI),
  uploaded through the presenter's own code and read back pixel for pixel; the repeated pictures; rewind
  stepped back and played forward again, frame for frame (rewind's ring is in `src/rewind.c` now); the
  ANALOG combination as Beetle PSX reports it; and a PAL disc at 50 Hz and at 60 Hz.

Built and checked by the host tests; not yet run on a console.

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
