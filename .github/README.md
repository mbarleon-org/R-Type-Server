<h1 align="center">
  R-TYPE-SERVER - 2025<br>
  <img src="https://raw.githubusercontent.com/catppuccin/catppuccin/main/assets/palette/macchiato.png" width="600px" alt="color palette"/>
  <br>
</h1>

<p align="center">
  Server for R-Type project<br>
</p>

---

## Usage

Clone the repository

```bash
git clone --recurse-submodules git@github.com:mbarleon/R-Type-Server.git
```

Build the project

```bash
# On UNIX
./build.sh
```

```powershell
# On Windows
.\build.ps1
```

vcpkg (optional, recommended)
------------------------------------

This project supports vcpkg manifest mode to install and provide dependencies (OpenSSL) to CMake.

The build scripts have a few conveniences to make using vcpkg easier:

- If `VCPKG_ROOT` is not set, the scripts default it to `~/vcpkg` (tilde `~` is expanded).
- If `VCPKG_TARGET_TRIPLET` is not set, the scripts auto-select a sensible triplet based on OS and CPU architecture (for example `x64-linux`, `arm64-osx`, `x64-windows`).
- `--auto-vcpkg` (or `-AutoVcpkg` in PowerShell) will attempt to clone and bootstrap vcpkg non-interactively into the chosen `VCPKG_ROOT`.
- `--dry-run` (or `-DryRun`) prints the CMake and Ninja commands that would be executed and exits without running them.

Bootstrap vcpkg (once per machine)

```bash
cd ~
git clone https://github.com/microsoft/vcpkg.git vcpkg
cd vcpkg
```

```bash
# On UNIX
./bootstrap-vcpkg.sh
```

```powershell
# On Windows
.\bootstrap-vcpkg.bat
```

Usage (project root)

```bash
# (optional) let scripts default VCPKG_ROOT to ~/vcpkg or set it explicitly
export VCPKG_ROOT=~/vcpkg
# optionally override auto-detected triplet
export VCPKG_TARGET_TRIPLET=<triplet>
./build.sh --auto-vcpkg
```

Examples

- macOS (Intel x86_64)

```bash
export VCPKG_TARGET_TRIPLET=x64-osx
./build.sh
```

- macOS (Apple Silicon / arm64)

```bash
export VCPKG_TARGET_TRIPLET=arm64-osx
./build.sh
```

- Linux (x86_64)

```bash
export VCPKG_TARGET_TRIPLET=x64-linux
./build.sh
```

- Windows (PowerShell, MSVC x64)

```powershell
$env:VCPKG_ROOT = (Resolve-Path '~\vcpkg').Path
$env:VCPKG_TARGET_TRIPLET = 'x64-windows'
.\build.ps1 --auto-vcpkg
```

Direct CMake invocation (alternative)

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=$VCPKG_TARGET_TRIPLET
cmake --build build
```

Notes

- The repository contains a `vcpkg.json` manifest declaring `openssl` as a dependency; when you run CMake with the vcpkg toolchain file, vcpkg manifest mode will install OpenSSL automatically for the selected triplet.
- If CMake still cannot find OpenSSL, clear the `build/` directory (or `CMakeCache.txt`) and re-run the configure step with the vcpkg toolchain file.

Installing ninja-build
------------------------------------

This repository expects `ninja` as the build backend. You can install it with your platform package manager.

Platform package manager examples:

- macOS (Homebrew):

```bash
brew install ninja
```

- Linux (Debian/Ubuntu):

```bash
sudo apt update && sudo apt install -y ninja-build
```

- Windows (Chocolatey):

```powershell
# as Administrator
choco install ninja -y
```

Run the project

```bash
R_TYPE_SHARED_SECRET=$(openssl rand -hex 32) ./r-type_server
```

When exiting the server (by pressing CTRL+C), the SIGINT will be caught to exit
gracefully.

---

## WARNING

Dear Epitech students, using the contents of this repository directly in your projects may result in a grade penalty (e.g., -42). Use it with caution, and do not add it to your repository unless you have explicit approval from your local pedagogical staff.
