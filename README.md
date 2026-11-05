# Ossu Native

Play *Osu! Tatakae! Ouendan* (Nintendo DS) as a native program, without an emulator.

This is an experimental fan project. It is not finished, and you may run into bugs and crashes.

## You need your own copy of the game

**This repo does not include the game.** There is no ROM, no game data and no game code here. You need your own `.nds` dump of the game.

The build turns the code from your ROM into a program on your own computer. That program contains game code, so please do not upload or share it. The same goes for your ROM and the `gen/` and `work/` folders.

Only one exact version of the ROM works. The build checks this for you. Its SHA-256 is:

```
729b8676a150a064cd83a22cf34aeeafc71f0cb90e5ed7a39e1ab87d45bb26db
```

## How to play on Linux

**1. Install the tools.** On Arch or CachyOS:

```
sudo pacman -S --needed git python gcc make sdl3
```

On other systems, install the same tools with your package manager.

**2. Download this project:**

```
git clone https://github.com/Flezzonkeys/ossu-moero-recomp.git
cd ossu-moero-recomp
```

**3. Copy your `.nds` file into the `ossu-moero-recomp` folder.** The file name does not matter.

**4. Build the game.** This takes about a minute and you only need to do it once:

```
cd ossu-moero-recomp
```
```
tools/build.sh
```

**5. Start the game:**

```
./ossu-sdl
```

To play again later, open a terminal and run:

```
cd ossu-moero-recomp
./ossu-sdl
```

Your save file is stored as `ossu.sav` in the same folder. The commands above assume you run `cd ossu-moero-recomp` from the place where you ran `git clone`.

## Controls

| Input | DS button |
|---|---|
| Left mouse button | Touch screen (bottom screen) |
| Enter | Start |
| Right Shift | Select |
| Arrow keys | D-pad |
| X / Z | A / B |
| D / C | X / Y |
| A / S | L / R |
| F11 | Full screen |

## What works

| Part | Status |
|---|---|
| Menus, map and story scenes | Working |
| Playing a stage on Linux | Working, but not tested much |
| Sound | Plays, but may not sound exactly right |
| Windows version | Not tested yet |
| Other stages, results screen, loading saves | Not checked yet |
| Multiplayer | Not available |
| Some graphics effects (fog, outlines, toon shading, mosaic) | Missing |
| In-game clock | Missing |

If the game crashes, it may write the reason to `ossu_fehler.txt`. Please include that file when you report a bug.

## How it works

A small tool reads the DS code from your ROM and translates it into C. That C code is then compiled into a normal program. The DS hardware (graphics, sound, touch screen and so on) is rebuilt in C, so no emulator core is needed. Graphics, music and stages are loaded from your ROM while you play.

| Folder | What is inside |
|---|---|
| `tools/` | The translator and the build scripts |
| `src/` | The rebuilt DS hardware |

## Legal

This is an unofficial project. It is not connected to Nintendo or iNiS. All trademarks belong to their owners.
