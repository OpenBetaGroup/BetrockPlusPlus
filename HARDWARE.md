# Hardware

This page lists all hardware that Betrock++ has been successfully compiled for and run on. This is also used to help determine the true hardware requirements.

For this list they're sorted by physical memory amount.

| Last tested Commit                                                           | Architecture               | OS                                                 | CPU                                             | Memory (Physical) | Swap/Paged Memory        | Model Name                     | Submitted by |
| ---------------------------------------------------------------------------- | -------------------------- | -------------------------------------------------- | ----------------------------------------------- | ----------------- | ------------------------ | ------------------------------ | ------------ |
| [`7b80726`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/7b80726) | x86 (i586)                 | Gentoo Linux i586 (Linux 7.1.5)                    | AMD-K5(tm) @ 100MHz                             | 32 MB             | 4GB (swap)               |                                | Pixel Brush  |
| [`7b76755`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/7b76755) | PowerPC (PowerPC ISA 1.10) | Arch POWER ppc (Linux 7.2.10-wii+)                 | 750CL @ 729MHz                                  | 128 MB            | 256MB (swap)             | Nintendo Wii                   | Killer       |
| [`7b76755`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/7b76755) | x86 (i686)                 | Gentoo Linux i686 (Linux 7.3.0)                    | Pentium II (Deschutes) @ 338 MHz                | 76 MB             | 1.11GB (swap)            | Lifetec (modified)             | Pixel Brush  |
| [`8cb7559`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/8cb7559) | x86 (i686)                 | Gentoo Linux i686 (Linux 6.18.38-gentoo)           | mobile AMD Athlon(tm) XP-M 2600+ (1) @ 1.99 GHz | 256 MB            | 1GB (zram) + 4GB (swap)  | Fujitsu AMILO K7600            | Pixel Brush  |
| [`0754a60`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/0754a60) | armv7l (aarch32)           | KOReader (Linux 4.9.77)                            | MediaTek mt8512 (1) @ 1.00 GHz                  | 437 MB            | 512 MB (swap)            | tolino shine 5                 | Pixel Brush  |
| [`c5e64f8`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/c5e64f8) | aarch64                    | Alpine Linux v3.24 aarch64 (Linux 6.18.35-0-rpi)   | BCM2711 (4) @ 1.50 GHz                          | 2 GB              |                          | Raspberry Pi 4 Model B Rev 1.1 | jwaxy        |
| [`9662935`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/9662935) | x86_64                     | Alpine Linux v3.24 x86_64 (Linux 6.18.49-0-lts)    | AMD E-450 (2) @ 1.65 GHz                        | 4 GB              |                          | ASUS K73BY                     | Pixel Brush  |
| [`7d4b516`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/7d4b516) | aarch64                    | Raspberry Pi OS aarch64 (Linux 6.12.47+rpt-rpi-v8) | BCM2711 (4) @ 1.80 GHz                          | 8 GB              | 512MB (swap)             | Raspberry Pi 4 Model B Rev 1.5 | Pixel Brush  |
| [`0754a60`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/e70311a) | x86_64                     | Fedora Linux 44 (Linux 7.2.10-200.fc44.x86_64)     | AMD Ryzen 9 7900X (24) @ 5.74 GHz               | 32 GB             | 8GB (zram) + 16GB (swap) | (Custom)                       | Pixel Brush  |

## Recommended Specs (Server)

The conditions to set or lower these requirements are relatively arbitrary, and mostly down to what feels right. These are only the lowest specs that have been tested and we can 100% confirm Betrock++ can run on without trouble\*, and reasonable playability. Lesser systems can definitely host a server but they aren't guaranteed to attain a playable level of performance.

As of [`0754a60`](https://github.com/OpenBetaGroup/BetrockPlusPlus/commit/0754a60) the known minimum requirements are:

- OS: A Linux Distro with Kernel 4.9 or later
- Processor: At least 32-Bit capable (e.g. i586, armv7 or later) @ >500MHz
- Memory: ~40MB of available RAM (as in, not used by the operating system or anything else, view distance: 10 chunks)

> [!NOTE]  
> CPU usage is rather minimal for the most part. The biggest bottlenecks, especially on older systems, are compression and world generation. If you plan to play on such a system, try to pregenerate the area you'd like to play in, or reduce the render distance so less chunks get generated at once as players explore.

> [!NOTE]  
> Memory usage is hugely affected by the server's view distance.
> At a low value, such as 3 chunks, we use as little as 10-20MB, and 70-80MB when set to 13 chunks.
> Some additional memory overhead can come from having a large number of CPU cores, though this is not as common anymore.

\*By "without trouble" we mean that you can

- Connect to the server
- Generate terrain
- Place, break blocks
- Mobs can pathfind in a reasonable amount of time
- Explore with other players
- A serviceable render distance
- Not have the server lag to oblivion

Overall, the metric is very subjective and left vague on purpose. What one person considers unplayable is a fine trade-off to another.
