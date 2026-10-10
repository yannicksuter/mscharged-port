# Mario Strikers Charged — Native Port

[![Build stable](https://github.com/yannicksuter/mscharged-port/actions/workflows/build.yml/badge.svg?branch=stable)](https://github.com/yannicksuter/mscharged-port/actions/workflows/build.yml?query=branch%3Astable)
[![Development version: 0.0.1-dev](https://img.shields.io/badge/version-0.0.1--dev-blue)](CMakeLists.txt)

Play **Mario Strikers Charged** natively on your PC. The port compiles the
fully reconstructed game source of
[mscharged-decomp](https://github.com/yannicksuter/mscharged-decomp) into native
code, with [Aurora](https://github.com/encounter/aurora) in place of the Wii
hardware. It is not an emulator and not a recompilation of the PowerPC binary:
the original game runs unchanged, modern systems provide the rest.

> [!IMPORTANT]
> Only the **USA version** of the game (**R4QE01**, revision 1) is supported.
> European and Japanese discs need their own complete decompilation before they
> can run glitch free.

## Status

**Every offline feature of the original game works**: all modes, up to four
players, saves, movies, music and sound. What remains is polishing and testing.
Online play is not available, since Nintendo Wi-Fi Connection has shut down.

Highlights:

- Keyboard & mouse with freely assignable keys
- Xbox, PlayStation and other gamepads
- Real Wii Remotes with Nunchuk through a Mayflash DolphinBar, with pointer
  calibration
- Sharp picture, 4:3 or 16:9, optional antialiasing, any window size or
  fullscreen
- A launcher for disc, players, controls, display and audio

See the full **[feature list](docs/FEATURES.md)**.

| Platform | State |
| --- | --- |
| Linux x86_64 | Tested |
| macOS Apple Silicon | Builds; testing in progress |
| Windows | In progress |

## Download

Builds of the reviewed `stable` branch are attached to each successful
[**Build binaries** run](https://github.com/yannicksuter/mscharged-port/actions/workflows/build.yml?query=branch%3Astable+is%3Asuccess)
under *Artifacts* (Linux x86_64 and macOS Apple Silicon). Extract the archive,
keep its files together and start `mscharged`; the launcher guides you through
the rest. See [GitHub builds](docs/BUILDING_GITHUB.md) for details.

## Game data

Supply an **ISO or RVZ from your own copy** of the USA disc. Game data is not
included. Choose the file in the launcher or see
[disc setup](docs/BUILDING.md#configure-your-disc).

## Controls

Keyboard & mouse act as a Wii Remote with a Nunchuk; gamepads and real Wii
Remotes can be assigned to players 1–4 in the launcher. Default keys:

| Wii input | Keyboard / mouse |
| --- | --- |
| Pointer | Mouse |
| A / B | Enter or left click / Esc or right click |
| D-pad | Arrow keys |
| Nunchuk stick | W A S D |
| Nunchuk C / Z | C / V |
| 1 / 2 | Z / X |
| + / − / HOME | Tab / - / Home |
| Shake Remote / Nunchuk | E / Q |

Press **P** for a screenshot. See [Wii Remote setup](docs/RUNTIME.md#wii-remote)
for the DolphinBar.

## Build from source

Install the [build prerequisites](docs/BUILDING.md#requirements) first; Mac users
can follow [macOS tool setup](docs/BUILDING_MACOS.md#install-tools). Then:

```sh
git clone --branch main --no-recurse-submodules https://github.com/yannicksuter/mscharged-port.git && cd mscharged-port
python3 tools/setup_dependencies.py && CMAKE_BUILD_PARALLEL_LEVEL=3 cmake --workflow --preset release --fresh
./build/release/mscharged
```

`main` is the development branch; `stable` holds reviewed snapshots. See the
[build guide](docs/BUILDING.md) for updates and options.

## Porting approach

Original source plus reviewed patches becomes the compiled game:

1. **Pin.** The decomp and every dependency stay clean submodules at reviewed
   commits; upstream changes enter only through explicit updates.
2. **Patch.** The build exports the decomp's complete `include/`, `libs/`, and
   `src/` trees and applies the ordered [patch series](patches/README.md):
   compiler compatibility, native replacements for Wii hardware and platform
   services, and data/ABI adaptation.
3. **Format.** clang-format 16+ gives the patched tree one readable layout
   ([definition](tools/formatting/prepared-sources.clang-format)). It changes
   whitespace only and is verified token by token; files whose program depends
   on their layout (`__LINE__`, stringified macro arguments) stay as patched.
4. **Compile.** Original game code controls the game flow from `main(...)`;
   native adapters replace Wii hardware services while preserving retail behavior.

Steps 1–3 run during CMake configuration. To produce the source without
configuring or building:

```sh
git submodule update --init extern/mscharged-decomp
python3 tools/prepare_sources.py --build-dir build/release
```

This writes `build/release/prepared/mscharged-decomp/patched/` (pinned decomp
plus patches, byte-exact; patches are developed here) and `source/` (the
formatted tree that is compiled). A later `cmake --preset release` reuses it.
Without clang-format, `source/` is an unformatted copy.

See the short [port strategy](docs/PORTING.md), [patch workflow](patches/README.md),
and [contributing guide](CONTRIBUTING.md).

## License

Original port code, tools, and documentation use [CC0 1.0 Universal](LICENSE).
Third-party material retains its own terms; see
[license scope and notices](LICENSES/README.md).

Mario Strikers Charged was developed by Next Level Games and published by
Nintendo. This is an unofficial project, unaffiliated with either company.
