# NoSteamWebHelper (Fork)

This is a fork of [Aetopia/NoSteamWebHelper](https://github.com/Aetopia/NoSteamWebHelper). Refer to the original repository for the base documentation and project concept.

## Key Differences

This fork adds two states:

* **Steam On:** Steam Web Helper is allowed to run. It will **not** be killed when a game is launched. If it was previously off, selecting this will immediately restore it.
* **Steam Off:** Steam Web Helper is automatically killed when a game is launched. If it was previously on while a game is running, selecting this will immediately kill it.

The last chosen state persists across restarts, acting as the default for next time.

## Usage

1.  Download the latest `umpdc.dll` from the [Releases](https://github.com/Deniz1433/NoSteamWebHelper/releases) page.
2.  Place `umpdc.dll` in your Steam installation directory (the same folder as `steam.exe`).
3.  Ensure Steam is fully closed, then launch it.
4.  **Right-click the Tray Icon** (which looks like a standard application icon) to toggle the persistent state.

## Build

To compile this fork using **MSYS2**:

1.  Install the 64-bit UCRT toolchain:
    ```bash
    pacman -Syu mingw-w64-ucrt-x86_64-gcc --noconfirm
    ```
2.  Open the **UCRT64** terminal environment.
3.  Run `build.cmd`.
