# Optional Flags

Optional Features are settings that we expose at compile-time for people that want specific features, without unnecessarily inflating compile time, binary size or the number of necessary dependencies for those that don't want them.

For example, if you'd like to disable Online Mode Authentication, but enable Experimental features, you'd type:

```bash
cmake -S . -B build -DONLINE_MODE_AUTHENTICATION=OFF -DEXPERIMENTAL=ON
```

Here's a list of all optional flags and their default state:

| Flag                                                        | Default State |
| ----------------------------------------------------------- | ------------- |
| [`ONLINE_MODE_AUTHENTICATION`](#online-mode-authentication) | `ON`          |
| `CRASH_LOGGING`                                             | `ON`          |
| [`BETACRAFT_HEARTBEAT`](#betacraft-server-list-heartbeat)   | `ON`          |
| [`EXPERIMENTAL`](#experimental)                             | `OFF`         |
| [`GENERATION_PRECISION`](#terrain-precision)                | `DOUBLE`      |

### Online Mode Authentication

Online Mode Authentication is **on by default**. It allows users to be authenticated via the legacy Login protocol. By default this goes through the Betacraft.uk proxy, since the original authentication servers got shut down long ago. It requires `libcurl`.

Simply add `-DONLINE_MODE_AUTHENTICATION=OFF` to the first build command, if you'd like to disable it. This removes all auth-related code.

### Betacraft Server List Heartbeat

Betacraft heartbeat is **off by default**. Enabling it lets a dedicated server ping [the Betacraft list](https://betacraft.uk/serverlist). It requires `libcurl` and [nlohmann/json](https://github.com/nlohmann/json) (via vcpkg feature `betacraft-heartbeat`, or fetched by CMake).

Add `-DBETACRAFT_HEARTBEAT=ON` to the first build command.

To appear on the list, contact Moresteck on the [Betacraft Discord](https://betacraft.uk/) for a private key assigned to your address, then set these in `server.properties`:

| Key                      | Purpose                                                                                |
| ------------------------ | -------------------------------------------------------------------------------------- |
| `betacraft-heartbeat`    | `true` to ping `https://api.betacraft.uk/v2/server_update` every 60 seconds            |
| `betacraft-name`         | List name (max 64 characters)                                                          |
| `betacraft-description`  | List description (max 256 characters). State whitelist/anarchy clearly if that applies |
| `betacraft-socket`       | Public `host:port` players should join. Empty auto-detects public IP + `server-port`   |
| `betacraft-private-key`  | Verification key from Betacraft (required for category, domain names, and icons)       |
| `betacraft-category`     | `classic`, `indev`, `infdev`, `alpha`, `beta`, or `release`                            |
| `betacraft-game-version` | Version ID as shown in the Betacraft launcher (default `b1.7.3`)                       |
| `betacraft-protocol`     | Protocol ID from Betacraft version metadata (default `beta_14` for Beta 1.7.3)         |
| `betacraft-v1-version`   | Version string for Betacraft launcher v1                                               |
| `betacraft-send-players` | `true` shares online usernames; `false` only shares the player count                   |
| `betacraft-icon`         | Optional path to a PNG icon (max 128×128 and 64 KiB)                                   |

### Experimental

Experimental features are **off by default**. This flag enables experimental or work-in-progress features that are only partially implemented. This is mostly meant to keep not yet finished or unstable functionality out of Release builds.

Right now, this includes:

- Held map items (and their crafting recipe)

Simply add `-DEXPERIMENTAL=ON` to the first build command, if you'd like to enable it.

### Terrain Precision

Terrain Precision is set to **`DOUBLE` by default**. Setting it to another value changes the numerical precision of the Perlin and Simplex noise generators.

There exist four options, listed from most to least supported/usable.

| Value    | Description                                                                                                                                                                                                                                                                                                                                                                   | Consequences                                                                                                                                                                                                                              |
| -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `DOUBLE` | Uses a **64-Bit floating point number**. The default, supported mode                                                                                                                                                                                                                                                                                                          | None. This is identical to Vanilla                                                                                                                                                                                                        |
| `FLOAT`  | Uses a **32-Bit floating point number**. This shouldn't do much on systems with a dedicated floating-point co-processor, such as an Intel 8087, or integrated floating-point functionality, like most x86 CPUs made after ~1987, as they use the same 80-Bit registers for 32-bit and 64-bit floating-point math, the only difference being potential memory bandwidth usage. | _VERY_ slightly different terrain. The only major difference is that the farlands do not generate, and they just become an infinite ocean with a bedrock floor along the X-Axis, and the same but with a grid of blocks along the Z-Axis. |
| `LONG`   | Uses a **36.28 Fixed-point number**, removing some floating-point overhead                                                                                                                                                                                                                                                                                                    | Generation is identical to `DOUBLE`, except at the farlands, where they appear more compressed.                                                                                                                                           |
| `INT`    | Uses a **18.14 Fixed-point number**, removing some floating-point overhead                                                                                                                                                                                                                                                                                                    | Generation can differ noticeably under some circumstances, even near 0,0. The farlands do not generate at their usual location.                                                                                                           |

The main benefits for this are for some microcontrollers or cost-reduced x86 chips that don't have integrated floating-point support, and thus need to emulate it all in software. Examples for such include RISC-V cores that lack the F (float) and D (double) extensions (e.g. RV32I, RV64IM) or the i486SX.

Simply add `-DGENERATION_PRECISION=`, followed by your desired precision type, to the first build command, then resume as normal.

### Client

> [!IMPORTANT]  
> The client is HIGHLY wip and NON-FUNCTIONAL as of now!
> DO NOT MESSAGE US ABOUT IT IF YOU TRY TO COMPILE IT,
> IT IS NOT IN A USABLE STATE!!!

> [!NOTE]
> When the client _is_ in a more workable state again,
> we'll likely move these instructions back into the
> main building instructions.
>
> It's been moved here so people stop compiling the client
> and asking us if they did something wrong, and why it's
> not yet usable.
> The client is not our main focus right now,
> the server is, as it'll give us a solid foundation to
> build the Client on top of.
>
> This is also why the client isn't mentioned at the top
> of the usable flags, so it hopefully flies under the
> radar a little until it's in a more usable state.

You'll need some additional dependencies for the client to compile. This assumes you've already grabbed the necessary dependencies for the Server, as most of those are shared.

To build a client, just append `-DBUILD_SERVER=OFF` to the
initial build line.

##### Debian / Ubuntu / Linux Mint

```bash
sudo apt install git cmake build-essential libdeflate-dev libasan8 libcurl4-openssl-dev
```

> [!NOTE]
> `libsdl3-dev` is only packaged on Debian 13 (trixie) and newer, and Ubuntu 25.10 and newer. On Ubuntu 24.04/22.04 LTS it is not in `apt`, so you'll need a newer release or to build SDL3 from source.

##### RHEL / Fedora

```bash
sudo dnf install glm-devel SDL3-devel mesa-libGL-devel
```

##### Arch Linux / SteamOS / CachyOS

```bash
sudo pacman -S glm sdl3
```

##### openSUSE (Leap / Tumbleweed)

```bash
sudo zypper install glm-devel SDL3-devel Mesa-libGL-devel libasan8
```

##### Alpine Linux

```bash
sudo apk add glm-dev sdl3-dev mesa-dev compiler-rt
```

> [!NOTE]
> `sdl3-dev` is currently only in the **edge** branch's `community` repo. Also, Alpine ships no `libasan` and GCC's AddressSanitizer is broken on musl, so for Debug builds (which use `-fsanitize=address`) compile with **clang**, which uses the ASan runtime from `compiler-rt`.
> Additionally, you may need to install `libdeflate-static` too.
> For some reason, just `libdeflate` is not enough sometimes.

##### Void Linux

```bash
sudo xbps-install -S glm SDL3-devel MesaLib-devel libsanitizer-devel
```

##### Gentoo

```bash
sudo emerge media-libs/glm media-libs/libsdl3 media-libs/mesa
```

## Continuing

[Please continue after the first command of Step #3 in BUILDING](./BUILDING.md#3-building).
