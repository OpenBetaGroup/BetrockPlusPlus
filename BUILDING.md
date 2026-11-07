# Building Betrock++

This pages includes instructions on how to build and compile Betrock++ for various operating systems.

### 1. Clone the Repository

Simply clone the repository with `git`.

```bash
git clone --recurse-submodules https://github.com/OpenBetaGroup/BetrockPlusPlus.git
cd BetrockPlusPlus
```

Alternatively, download a **.zip**.

If you've cloned the repo in the past, ensure all the submodules are up-to-date,
especially if you intend to compile the client.
```bash
git submodule update --init --recursive
```

### 2. Install Dependencies

#### Docker

Docker will take care of all of this stuff automatically, so skip to the [building step](#option-4-docker).

#### Windows (10/11)

Prerequisites:

- CMake 3.25 (or later)
- MSVC 19.32 (or later)
- vcpkg

After those are all installed and set up, you can
install the dependencies with **vcpkg**.

```powershell
vcpkg install
```

Then move onto the [building step](#3-building).

#### Linux

Betrock++ also works on Linux! Theoretically, any Distro should be supported, so long as it has the required dependencies.

Prerequisites:

- CMake 3.25+
- GCC 13+ or Clang 17+
- Glibc 2.35+ or Musl 1.2.4+

**GCC 13+** is recommended, though Clang should work too, same goes for **glibc** and **musl**.
Ninja or GNU Make have also been confirmed to both work, so pick your poison.
The install instructions below assume that `glibc`, `gcc`/`g++` and `make` are used.

##### Debian / Ubuntu / Linux Mint

```bash
sudo apt install git cmake build-essential libdeflate-dev libasan8 libcurl4-openssl-dev
```

##### RHEL / Fedora

```bash
sudo dnf install git cmake gcc gcc-c++ make libasan libdeflate-devel libcurl-devel
```

##### Arch Linux / SteamOS / CachyOS

```bash
sudo pacman -S git cmake base-devel libdeflate libasan curl
```

##### openSUSE (Leap / Tumbleweed)

```bash
sudo zypper install git cmake gcc gcc-c++ make libdeflate-devel libcurl-devel
```

##### Alpine Linux

```bash
sudo apk add git cmake gcc g++ make libdeflate-dev curl-dev
```

##### Void Linux

```bash
sudo xbps-install -S base-devel git cmake libdeflate-devel libcurl-devel
```

##### Gentoo

```bash
sudo emerge dev-vcs/git dev-build/cmake sys-devel/gcc dev-build/make app-arch/libdeflate net-misc/curl
```

Then move onto the [building step](#3-building).

#### Optional Features

Optional Features are settings that we expose at compile-time for people that want specific features, without unnecessarily inflating compile time, binary size or the number of necessary dependencies for those that don't want them.

Check out the [BUILDING_OPTIONAL](./BUILDING_OPTIONAL.md) file for more info, then return here and continue with Step #3.

### 3. Building

#### Option #1: Command-line

First you prepare and enter the build directory. This is also where you pass in [optional compile-time features](#optional-features).

```bash
cmake -S . -B build
cd build
```

> Tip: If you have `ccache` and `mold` installed, you can speed up iterative builds with:
>
> ```bash
> cmake -S . -B build -G Ninja \
>   -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
>   -DCMAKE_C_COMPILER_LAUNCHER=ccache \
>   -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold" \
>   -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold"
> ```

This will make a Release Server build folder.
Then you build the project.

```bash
cmake --build . -j$(nproc)
```

After that, it's as easy as running the built application.

```bash
./BetrockPlusPlus
```

#### Option #2: Visual Studio Code

If your compiler, `cmake` and dependencies are properly set up,
Visual Studio Code (or anything based on it, like VSCodium) should just work.
Click the run or build buttons in the bar at the bottom.

#### Option #3: Visual Studio (Windows only)

**TODO**

#### Option #4: Docker

Docker makes compiling and running a server significantly easier,
since it'll take care of all the dependency hoo-haa.

Please see the [docker markdown file for more info](./DOCKER.md).

Do note that most of the information in that file was AI-generated,
and isn't updated often.

## Nintendo Switch / 3DS

See [BUILDING_CONSOLE.md](BUILDING_CONSOLE.md).
