> [!NOTE]
> This whole file, alongside a decent amount of the platform-specific files
> for Switch and 3DS are vibecoded.

# Nintendo Switch & Nintendo 3DS

Betrock++ can be cross-compiled into homebrew for the Nintendo Switch (`.nro`)
and Nintendo 3DS (`.3dsx`) with [devkitPro](https://devkitpro.org/). Only the
**server** is supported. `CMakeLists.txt` detects the devkitPro toolchain file
(`Switch.cmake` / `3DS.cmake`) and switches to a console configuration:

- `BUILD_SERVER` is forced on.
- `ONLINE_MODE_AUTHENTICATION`, `BETACRAFT_HEARTBEAT` and `CRASH_LOGGING` are forced
  off (no libcurl/vcpkg triplet, and CrashCatch needs `fork()`/`execinfo.h`).
  CMake prints a `Building for a homebrew console target` line when this kicks in.
- The replxx line editor is replaced by a headless console stand-in
  (`server_console_headless.cpp`), so there is no console command input.
  Run commands in-game as an op instead.
- Addons are not loaded (there is no `dlopen()` on either console).

If your toolchain file isn't called `Switch.cmake`/`3DS.cmake`, pass
`-DBPP_TARGET=SWITCH` or `-DBPP_TARGET=3DS` to force the detection.

## How the console support is kept out of the way

Everything console specific lives in `src/bpp_shared/platform/`:

| File | What it does |
| --- | --- |
| `platform_caps.h` | `BPP_HAS_*` capability flags (flock, signals, socket options, ...) that the rest of the code tests instead of listing platforms |
| `3ds/`, `nx/` | `Platform::Init()/Shutdown()`, memory and hardware info |
| `homebrew/system_pump.h` | Runs the OS per-frame service on a background thread, enters the data folder, reports uncaught exceptions on screen |
| `fs_compat.h` | `ReplaceFile()` / `CopyFileOverwrite()` for filesystems that can't rename over an existing file |

The OS service loop (`aptMainLoop` + `gfxSwapBuffers` on 3DS, `appletMainLoop` +
`consoleUpdate` on Switch) runs on its own thread, so the server tick loop
and long blocking work like spawn chunk generation never starve it.
Closing the app from the HOME menu, or pressing **START** (3DS) / **+** (Switch),
requests a normal server shutdown so the world is saved.

Config, logs and the world live in `sdmc:/3ds/BetrockPlusPlus/` (3DS) or
`sdmc:/switch/BetrockPlusPlus/` (Switch), regardless of where the launcher started the app.

> [!NOTE]
> The 3DS has very little RAM for homebrew. Keep view/generation radii small and
> pre-generate the area you intend to play in.

## Nintendo Switch

### 1. Install devkitPro

```bash
podman run -it --rm -v $(pwd):/project:Z devkitpro/devkita64
```

### 2. Build libdeflate

```bash
apt-get update && apt-get install -y git cmake

git clone https://github.com/ebiggers/libdeflate.git /tmp/libdeflate
cd /tmp/libdeflate

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DLIBDEFLATE_BUILD_SHARED_LIB=OFF \
    -DLIBDEFLATE_BUILD_GZIP=OFF \
    -DCMAKE_INSTALL_PREFIX=$DEVKITPRO/portlibs/switch

cmake --build build
cmake --install build
```

### 3. Build the repo

```bash
cd /project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake --build build -j$(nproc)
```

`build/BetrockPlusPlus.nro` is produced as a post-build step (`nacptool` +
`elf2nro`). Copy it to `/switch/BetrockPlusPlus/` on the SD card and launch it from hbmenu.

## Nintendo 3DS

### 1. Install devkitPro

```bash
podman run -it --rm -v $(pwd):/project:Z docker.io/devkitpro/devkitarm
```

### 2. Build libdeflate

```bash
apt-get update && apt-get install -y git cmake

git clone https://github.com/ebiggers/libdeflate.git /tmp/libdeflate
cd /tmp/libdeflate

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DLIBDEFLATE_BUILD_SHARED_LIB=OFF \
    -DLIBDEFLATE_BUILD_GZIP=OFF \
    -DCMAKE_INSTALL_PREFIX=$DEVKITPRO/portlibs/3ds

cmake --build build
cmake --install build
```

### 3. Build the repo

```bash
cd /project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake
cmake --build build -j$(nproc)
```

`build/BetrockPlusPlus.3dsx` is produced as a post-build step (`3dsxtool`).
Copy it to `/3ds/BetrockPlusPlus/` on the SD card and launch it from the Homebrew Launcher.
