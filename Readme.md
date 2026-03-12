#SmacAccess
==========
**Screen reader accessibility mod for Sid Meier's Alpha Centauri: Alien Crossfire.**
SmacAccess is a fork of [Thinker Mod](https://github.com/induktio/thinker) that adds full screen reader support to SMACX, making this classic 4X strategy game playable for blind and visually impaired players. It works with NVDA, JAWS, and other screen readers via the [Tolk](https://github.com/ndarilek/tolk) library.

##Quickstart:
Just unpack the .zip file to your game directory, then run thinker.exe against the will of Windows Defender.
And maybe have a quick look at the next couple of sections for keystrokes and known issues, then you're good to go.

##Important keys and concepts:
ctrl+f1: Should show context-sensitive help everywhere, might still be messy or missing in a few places.
Ctrl+f2: Opens the menu bar with loads of sections.
On the world map:
Explanation: The grid has only fully odd and fully even coordinates. You need to move diagonally in order to switch from one type to the other.
Also, one unit at a time is always selected and will take whatever order you give. This can defy from your cursor position, which can be rather confusing in the beginning.
Arrow keys, home, end, page up, page down: Move the cursor around the map without moving your units.
Shift + arrow keys or other movement keys: Move the selected unit in the respective direction.
Ctrl + Space: Moves your cursor to the currently selected unit and announces its position.
Shift + Space: Moves the currently selected unit to (or towards) your current cursor position.
U: Lists all your Units and you can activate them with Enter.
Shift + U: Similar for units which are not yours.
Enter: Opens the home base of the currently selected unit. (this is a game-native key).
There are loads more hotkeys on the world map, for example all the f-keys open some sort of status display, Shift E is for social engineering, b for building a new base using a colony pod etc.

##Known issues:
There are major issues with the planet council, bribes don't work at all and the rest may be unstable.
Only hot seat multiplayer works so far. The menus are extremely fiddly, I'm working on it.
When starting the game, it'll probably tell you that you don't have the original SMAC CD in your computer. Well what a surprise...
Main menu navigation may be unpredictable, occasionally the wrong item is selected when pressing Enter. This is a deeply rooted issue, but I hope I'll be able to fix it eventually.
Sometimes variable names are shown instead of processed, e.g. $tech0, $faction etc. Please tell me where and I'll do my best to fix them everywhere.
Same for german Screenreader announcements.
You can't rename your save files within the game yet.
Some status screens don't work properly yet, e.g. monolith and hall of fame. They aren't crucial for progressing in the game as far as I can tell.
The scenario editor is not at all accessible for now. Please let me know how big of a deal that is for passionate fans of the game, I might be able to implement it at some point.

##Sections which are not thoroughly tested yet:
Ranged Combat
All ways in which a game can end.
And probably quite some others which I haven't encountered so far.


##Features
--------

**Screen Reader Integration**
- All menus, dialogs, and popups announced via screen reader
- Localization support (English and German included)

**World Map Navigation**
- Virtual exploration cursor (arrow keys + diagonal navigation)
- Tile announcements: terrain, features, bases, units, resource yields, ownership
- Scanner mode: jump between points of interest with 10 filter categories
- Step-by-step unit movement with tile and movement point feedback

**Base Screen**
- 7 navigable information sections (Overview, Resources, Production, Economy, Facilities, Status, Units)
- Tab navigation with section content readout
- Production picker with item details (facilities, units, secret projects)

**Menu Bar**
- Full menu bar navigation (Ctrl + F2 from the world map)
- Shortcut summaries for each menu

**Context Help**
- Ctrl+F1: context-sensitive help for current window

**All Thinker Mod Features**
- Improved AI for single player
- Enhanced map generation
- Many gameplay configuration options
- See [Details.md](Details.md) for the full Thinker feature list


Requirements
------------
- [GOG version](https://www.gog.com/game/sid_meiers_alpha_centauri) of Alpha Centauri: Alien Crossfire (Planetary Pack)
- Alien Crossfire patch v2.0 (included with GOG version)
- A screen reader (NVDA recommended)
- Windows 10 or 11


Installation
------------

### What You Need

- The [GOG version](https://www.gog.com/game/sid_meiers_alpha_centauri) of Alpha Centauri: Alien Crossfire (Planetary Pack), installed
- A screen reader running (NVDA recommended, JAWS also supported)
- The SmacAccess release ZIP ([download from Releases](https://github.com/HappyStarfish/smac-access/releases))

### Step by Step

1. Download the latest `SmacAccess.zip` from [Releases](https://github.com/HappyStarfish/smac-access/releases).
2. Open your Alpha Centauri game folder (usually `C:\Program Files (x86)\Sid Meier's Alpha Centauri Planetary Pack\` or wherever you installed it).
3. Extract the entire ZIP into the game folder. Overwrite files if asked.
4. **Optional:** Open `thinker.ini` in a text editor and set `sr_language=de` for German screen reader output (default is English).
5. Start the game by running `thinker.exe` (not `terranx.exe`).

### What Gets Installed

The ZIP contains these files, all going into your game folder:

- `thinker.dll` — the mod (replaces the original if you had Thinker installed)
- `thinker.exe` — launcher, start the game with this
- `thinker.ini` — configuration (screen reader language, AI settings, gameplay options)
- `modmenu.txt` — menu definitions (required)
- `Tolk.dll` — screen reader library
- `nvdaControllerClient32.dll` — NVDA support
- `SAAPI32.dll` — SAPI support
- `dolapi32.dll` — JAWS support
- `sr_lang/en.txt` — English screen reader text
- `sr_lang/de.txt` — German screen reader text

### Updating

To update SmacAccess, download the new ZIP and extract it over the existing files. Your `thinker.ini` settings will be overwritten, so back it up first if you customized it.

### Uninstalling

Delete the files listed above from your game folder. The original game is not modified and will continue to work normally.


Quick Start Keys
----------------
- **Ctrl+R** - Read current screen text
- **Ctrl+Shift+R** - Stop speech
- **Arrow keys** - Explore map / navigate menus
- **Shift+Arrows** - Move selected unit
- **Ctrl+Left/Right** - Scanner: jump to next point of interest
- **Ctrl+PgUp/PgDn** - Scanner: change filter category
- **Alt** - Open menu bar
- **Ctrl+F1** - Context help
- **Ctrl+Up/Down** - Base screen: cycle info sections

For a complete list of original SMAC keys, see the in-game help or the game manual.


Configuration
-------------
Screen reader settings in `thinker.ini`:

- `sr_language=en` - Screen reader language (`en` or `de`)

All other settings are inherited from Thinker. See [Details.md](Details.md) for configuration options.


Building from Source
--------------------
SmacAccess requires a 32-bit (i686) MinGW GCC toolchain.

**Requirements:**
- GCC 14.2.0 i686 (msvcrt variant) - **GCC 15.x and ucrt builds will crash!**
- CMake 3.31+
- Ninja

**Build:**
```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Develop -B build_cmake -S .
ninja -C build_cmake
```

**Output:** `build_cmake/thinker.dll` and `build_cmake/thinker.exe`

**Package a release ZIP:**
```bash
./tools/package_release.sh
```
This creates `rel/SmacAccess.zip` with all files needed for installation (mod DLLs, Tolk, language files, config, docs).

**Manual deployment:** Copy `thinker.dll` and `thinker.exe` to the game directory. The Tolk DLLs, `sr_lang/` folder, `thinker.ini`, and `modmenu.txt` must also be present (see [Installation](#installation)).

See [Technical.md](Technical.md) for additional build information from upstream Thinker.


Troubleshooting
---------------
- **Game does not start:** Make sure `terranx.exe` (Alien Crossfire v2.0) is in the game folder. Always start from `thinker.exe`, not `terranx.exe`.
- **Permission errors:** Try running `thinker.exe` as administrator, especially if the game is installed in Program Files.
- **No screen reader output:** Check that `Tolk.dll` and the screen reader client DLLs (`nvdaControllerClient32.dll`, `SAAPI32.dll`, `dolapi32.dll`) are in the game folder alongside `thinker.dll`. Make sure your screen reader (NVDA, JAWS) is running before starting the game.
- **Missing modmenu.txt error:** The file `modmenu.txt` must be in the game folder. Re-extract the SmacAccess ZIP.
- **Language files not found:** The folder `sr_lang/` with `en.txt` and `de.txt` must be in the game folder (not a subfolder of something else).
- **Display issues:** Set Windows display scaling to 100% (the game does not support scaling).
- For additional troubleshooting, see the [Thinker documentation](Details.md).


Credits
-------
- **SmacAccess** accessibility layer by Sonja
- **[Thinker Mod](https://github.com/induktio/thinker)** by induktio - the foundation this mod builds on
- **[Tolk](https://github.com/ndarilek/tolk)** screen reader abstraction library
- **[OpenSMACX](https://github.com/b-casey/OpenSMACX)** - reverse engineering reference
- **[Scient's patch](https://github.com/DrazharLn/scient-unofficial-smacx-patch)** - game bug fixes (included via Thinker)

Sid Meier's Alpha Centauri and Sid Meier's Alien Crossfire is Copyright (c) 1997, 1998 by Firaxis Games Inc and Electronic Arts Inc.


License
-------
This software is licensed under the GNU General Public License version 2, or (at your option) version 3 of the GPL. See [License.txt](License.txt) for details.

The original game assets are not covered by this license and remain property of Firaxis Games Inc and Electronic Arts Inc.
