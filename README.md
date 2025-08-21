# CEmGBA - Game Boy Advance Emulator with Cheat Engine

This is a fork of the popular [mGBA emulator](https://mgba.io/) that includes an additional **Cheat Engine**. Used as a project for the Praktikum Binary Hacking at Heidelberg University.

## About mGBA

mGBA is an emulator for running Game Boy/ Game Boy Advance games. The original mGBA project supports:

- High accuracy Game Boy Advance emulation
- Game Boy/Game Boy Color backwards compatibility  
- Fast forwarding and rewinding
- Screenshot and video recording
- Cheat code support
- Cross-platform support (Windows, macOS, Linux)

## Cheat Engine

This fork adds an experimental **Cheat Engine** that provides real-time memory for GB/GBA games.

### Features:
- Modify game values while playing
- Freeze values to prevent the game from changing them (e.g., infinite health, lives)
- Share cheat collections with `.mgbatable` files
- Edit values directly in the cheat table

### Cheat Map Example:
This repository includes a sample cheat map for **Super Mario Land** (Game Boy) that demonstrates the functionality (SML.mgbatable)

### Usage:
1. Load your legally obtained ROM file
2. Open the Cheat Engine from the Tools menu
3. Either manually add cheats or import a `.mgbatable` file
4. Toggle cheats active/inactive and freeze values as needed

## Legal Notice

**IMPORTANT**: This emulator is intended for use with legally obtained ROM files only. You must own the original game cartridge to legally use ROM files. The developers do not condone or support piracy in any form.

The included cheat maps are provided for educational and legitimate use purposes only, for games that you legally own.

## Building the Project

### Building with Docker

1. **Clone the repository:**
   ```bash
   git clone https://github.com/Taldux/cemgba
   cd cemgba

2. **Build using Docker:**
   ```bash
   docker run --rm -it -v ${PWD}:/home/mgba/src mgba/windows:w32
   ```

3. **Run the executable:**
   The executable will be located in `build-win32/mgba.exe`
