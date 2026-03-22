SmacAccess
--
**Screen reader accessibility mod for Sid Meier's Alpha Centauri plus Alien Crossfire.**

SmacAccess is a fork of [Thinker Mod](https://github.com/induktio/thinker) that adds full screen reader support to SMACX, making this classic 4X strategy game playable for blind and visually impaired players. It works with NVDA, JAWS, and potentially other screen readers via the [Tolk](https://github.com/ndarilek/tolk) library.

Quickstart:--
-
1. Install Alpha Centauri Planetary Pack. I've tested both the Steam and Gog version, they are identical. Other sources might work too, but I can't promise anything.
2. Unpack the mod .zip file to the directory where the game is installed.
Direkt download link:
https://github.com/HappyStarfish/smac-access/releases/download/v0.1.4-alpha/SmacAccess_v0.1.4.zip
3. Run thinker.exe (not terranx.dxe). The first time you try, Windows defender might display a warning. To circumvent it, click "more info" and then "run anyway".
4. Have a look at the next couple of sections in this document in order to familiarize yourself with the game: it's rather complex both strategically and mod-handling-wise. I tried to make it as intuitive as possible, and I'm open for suggestions to improve it further.

Important keys and concepts:
--
ctrl+f1: Should show context-sensitive help everywhere, might still be messy or missing in a few places.
Ctrl+f2: Opens the menu bar with loads of sections.
On the world map:
Arrow keys, home, end, page up, page down: Move the cursor around the map without moving your units.
Shift + arrow keys or other movement keys: Move the selected unit in the respective direction.
Ctrl + Space: Move your cursor to the currently selected unit and announces its position.
Shift + Space: Move the currently selected unit to (or towards) your current cursor position.
Tab: Cycle through all your units on the same field.
U: Lists all your Units, you can activate them with Enter.
Shift + U: Similar for units which are not yours.
- **Ctrl+Left/Right** - Scanner: jump to next point of interest
- **Ctrl+PgUp/PgDn** - Scanner: change filter category
Enter: Opens the home base of the currently selected unit. (this is a game-native key). Then Ctrl+F1 to hear about all the things you can do within the base, there's a huge lot of actions and settings available.
There are loads more hotkeys on the world map, for example all the f-keys open some sort of status display, Shift E is for social engineering, b for building a new base using a colony pod etc.
For a complete list of original SMAC keys, see the in-game help or the game manual.
You might sometimes get error messages from the game when trying to execute actions like terraforming. This is due to a build-in feature: The game always marks one specific unit at a time as active, usually indicated by a flashing cursor. Just press Ctrl+Space to check if the currently active unit is the right one for the type of action you want to take.

A word on navigating the grid coordinates:
--
The grid has only fully odd and fully even coordinates. Think like a chess board. You can always move to any of the eight directly neighbouring fields. The cardinal neighbours are two coordinates apart from each other, the non-cardinal ones translate to one coordinate-step in botht the horizontal and vertical direction. Don't worry if you don't get my explanation though. Firstly the explanation is crap, secondly you'll probably manage to find your way around just fine once you've started getting into the game.

Known issues:
--
There are major issues with the planet council, bribes might not work as expected and the whole council may be unstable. Sometimes the game will prompt you to propose something to the council even though it isn't open.
Only hot seat multiplayer works reliably. When playing over the internet, you can't choose which faction to play, and some other settings might be broken as well. I'm working on this.
When starting the game, it'll probably tell you that you don't have the original SMAC CD in your computer. Well what a surprise...
Sometimes variable names are shown instead of processed, e.g. $tech0, $faction etc. Or the mod will display your base name instead of a recently researched technology. Please tell me where and I'll do my best to fix them everywhere.
Same goes for Screen reader announcements in the wrong language.
You can't rename your save files within the game yet.
Some status screens don't work properly yet, e.g. monolith and hall of fame. But they aren't crucial for progressing in the game as far as I can tell.
The scenario editor is not at all accessible for now. Please let me know how big of a deal that is for passionate fans of the game, I might be able to implement it at some point.

Sections which are not thoroughly tested yet:
--
Spanish and French language pack. English and German are somewhat tested. If you want to play in a different language, please provide a download link to the according files and I'll see if <i can loop it in witht he mod.
Ranged Combat
What happens when you win the game (I desperately need playtesters who don't constantly get wiped out after like 100 turns!)
and probably there are quite some more issues which I haven't encountered so far.

Features:
---
**Screen Reader Integration**
- World map, all menus, dialogs, and popups announced via screen reader
- Localization support (English and German tested, Spanish and French also included)

**All Thinker Mod Features**
- Improved AI for single player
- Enhanced map generation
- Many gameplay configuration options
- See [Details.md](Details.md) for the full Thinker feature list

Installation
------------

### What You Need

- The [GOG version](https://www.gog.com/game/sid_meiers_alpha_centauri) of Alpha Centauri: Alien Crossfire (Planetary Pack), installed (Steam works as well, other sources not confirmed. Decisive is if the game version is the exact same as the one distributed via Gog and Steam.)
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


### Uninstalling

Delete the files listed above from your game folder. The original game is not modified and will continue to work normally.


Configuration
-------------
Screen reader settings in `thinker.ini`:

- `sr_language=en` - Screen reader language
Also, here you can adjust whether you want to play the original version of the game or the Alien Crossfire expansion.

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
- **Display issues:** Set Windows display scaling to 100% (the game does not support scaling).
- For additional troubleshooting, see the [Thinker documentation](Details.md).

Credits
- **SmacAccess** accessibility layer by Plueschyoda
- **[Thinker Mod](https://github.com/induktio/thinker)** by induktio - the foundation this mod builds on
- **[Tolk](https://github.com/ndarilek/tolk)** screen reader abstraction library
- **[OpenSMACX](https://github.com/b-casey/OpenSMACX)** - reverse engineering reference
- **[Scient's patch](https://github.com/DrazharLn/scient-unofficial-smacx-patch)** - game bug fixes (included via Thinker)

Sid Meier's Alpha Centauri and Sid Meier's Alien Crossfire is Copyright (c) 1997, 1998 by Firaxis Games Inc and Electronic Arts Inc.


License
-------
This software is licensed under the GNU General Public License version 2, or (at your option) version 3 of the GPL. See [License.txt](License.txt) for details.

The original game assets are not covered by this license and remain property of Firaxis Games Inc and Electronic Arts Inc.
