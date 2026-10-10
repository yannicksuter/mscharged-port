# GitHub builds

`main` contains development work. `stable` holds reviewed snapshots, promoted
through a `main` → `stable` pull request.

The **Build binaries** workflow builds Linux x86_64, Windows x86_64 and macOS
Apple Silicon packages for PRs targeting `stable` and for pushes to `stable`.
Packages appear under **Actions → Build binaries → run → Artifacts**: a PR's
for 7 days (to test before merging), `stable`'s for 14 days. Failed checks may
attach configuration logs. These are unsigned builds, not tagged GitHub
Releases. A separate job compiles the Windows host-service tests natively.

Windows packages are cross-compiled on Ubuntu with the pinned LLVM-MinGW
toolchain and include its C++ runtime DLLs (`libc++`, `libunwind`,
`winpthread`) with their notices.

Extract the downloaded `.tar.gz` (Linux, macOS) or `.zip` (Windows), keep the
runtime modules, DLLs and `assets/` beside the executable, and start the
launcher (`mscharged`, or `mscharged.exe`), or run the game directly with your
own disc:

```sh
./mscharged --disc /path/to/R4QE01.rvz --window
```

Game data, personal settings and saves are excluded. Linux builds use Ubuntu
26.04/GCC 15 and need compatible system libraries plus a Vulkan driver. Windows
builds need Windows 10/11 and a Vulkan driver. macOS builds use macOS 15/Apple
Clang and need Metal. These archives are not self-contained installers.

## Initial repository setup

Rename the old `master` branch to `stable` using GitHub's branch settings.
The workflow and this policy enter that branch with the first reviewed PR;
merely renaming the old branch does not promote current development code.
Protect `stable` against direct pushes and require PR review and the Linux,
Windows and macOS build checks.
