# BetrockPlusPlus/Betrock++

![C++23](https://img.shields.io/badge/Language-C%2B%2B23-5E96CF)
[![Issues](https://img.shields.io/github/issues/OpenBetaGroup/BetrockPlusPlus)](https://github.com/OpenBetaGroup/BetrockPlusPlus/issues)
[![Pull requests](https://img.shields.io/github/issues-pr/OpenBetaGroup/BetrockPlusPlus)](https://github.com/OpenBetaGroup/BetrockPlusPlus/pulls)
[![OpenBeta](https://img.shields.io/badge/OpenBeta-Discord-purple)](https://discord.gg/JHTz2HSKrf)

A full, from-scratch reimplementation of Beta 1.7.x into C++.

## Goals

1. Betrock++ should be capable of acting as a drop-in replacement for the Java Server
2. Functionality may be extended where desired or necessary, but generally compatibility and faithfulness will be prioritized
3. Cross-platform (Windows, Linux, MacOS, etc.)
4. Fully open-source: Anyone can fork, commit and contribute however they please
5. Unless something requires decompiled code for the sake of accuracy, none will be used or referenced. If it **is** used for anything, it'll be very clearly marked as such in the code via comments. At most it'll serve as a reference for what not to do, and what architectural or implementation pitfalls we should avoid

## Community

This is another project that's part of/worked on by the OpenBeta Community. We have a [Discord Server](https://discord.gg/JHTz2HSKrf) and [Website](https://openbetagroup.github.io)!

## Running

Grab the [latest build from the Releases tab](https://github.com/OpenBetaGroup/BetrockPlusPlus/releases/latest), choose the build that you need for your OS, then just run it wherever you need!

## Bug Reporting

If you encounter a bug, inaccuracy or other issue, please file an issue here on Github. Check if a similar issue has already been reported, and roughly follow the same sort of style we use. Provide as much info as you can!

If you don't have a Github account, please report the issue on Discord, though this is generally discouraged.

## Hardware Requirements

To find out what's needed to run the project, please see the [recommended hardware requirements on the hardware page](./HARDWARE.md#recommended-specs-server).
The server can be run on lesser hardware, though the experience may not be ideal.

## Building

Please read the [BUILDING](./BUILDING.md) page.

## FAQ

Please read the [FAQ](./FAQ.md) page.

## Contributing

Please read the [CONTRIBUTING](./CONTRIBUTING.md) page.

## Addons

We provide a C ABI that allows for the creation of addons.
This feature is still in its infancy, so don't expect it to be reliable or stable just yet.
None the less, a few addons exist to demonstrate what's possible.

- [bpa_luau](https://github.com/OpenBetaGroup/bpa_luau): A luau plugin API for easier plugin development
- [bpa_discord](https://github.com/OpenBetaGroup/bpa_discord): Discord chat integration

For anyone curious in how to use the addon C ABI, check our [addon_api.h](./include/addon_api.h) file to see what interfaces are available.

## Related projects

- Beta++ by JcbbcEnjoyer: Beta 1.7.3 Client written in C++
- [Technical Beta Wiki](https://OpenBetaGroup.github.io/beta-wiki): Technical protocol and implementation reference (any findings are documented here)
- [LibreProg](https://github.com/OpenBetaGroup/LibreProg): Fully FOSS Beta 1.7.3 textures, sounds, etc. (will be used for default textures for Client)
- [LegacyMcServer](https://github.com/OpenBetaGroup/LegacyMcServer): Beta 1.7.3 Server written in C++ (some code was adopted from this)

## Credits

- Markus "Notch" Persson & Mojang, for creating Minecraft. https://www.minecraft.net/
- Barak Shoshany. Licensed under the MIT license. BS::thread_pool. https://github.com/bshoshany/thread-pool
- Keith Potz. Licensed under the MIT license. CrashCatch. https://github.com/keithpotz/CrashCatch
- Dugy. Licensed under the MIT license. quick_arg_parser. https://github.com/Dugy/quick_arg_parser
- Andre Weissflog. Licensed under the zlib license. sokol. https://github.com/floooh/sokol

## Star History

<a href="https://www.star-history.com/?repos=OpenBetaGroup%2FBetrockPlusPlus&type=timeline&legend=bottom-right">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=OpenBetaGroup/BetrockPlusPlus&type=timeline&theme=dark&legend=bottom-right&sealed_token=SiCPUTgIUA-J_nSNcOU255DhQcl9bI97c4J8pNGJ9H4SeGNcr2x9AIZCUyciKwnWu5iaPMBpR4jH4zbKPNe2eER55IKQuu1AjYEKt_HRJ0inpQtePnebJMKz9p6VF4Ffj2fBtcVkhnj2OCTm-tGDL_zD4Knq2lWGpJrYml1hH_bRvDJM4I2K_mlmapI9" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=OpenBetaGroup/BetrockPlusPlus&type=timeline&legend=bottom-right&sealed_token=SiCPUTgIUA-J_nSNcOU255DhQcl9bI97c4J8pNGJ9H4SeGNcr2x9AIZCUyciKwnWu5iaPMBpR4jH4zbKPNe2eER55IKQuu1AjYEKt_HRJ0inpQtePnebJMKz9p6VF4Ffj2fBtcVkhnj2OCTm-tGDL_zD4Knq2lWGpJrYml1hH_bRvDJM4I2K_mlmapI9" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=OpenBetaGroup/BetrockPlusPlus&type=timeline&legend=bottom-right&sealed_token=SiCPUTgIUA-J_nSNcOU255DhQcl9bI97c4J8pNGJ9H4SeGNcr2x9AIZCUyciKwnWu5iaPMBpR4jH4zbKPNe2eER55IKQuu1AjYEKt_HRJ0inpQtePnebJMKz9p6VF4Ffj2fBtcVkhnj2OCTm-tGDL_zD4Knq2lWGpJrYml1hH_bRvDJM4I2K_mlmapI9" />
 </picture>
</a>

## Legal Disclaimer

NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT
