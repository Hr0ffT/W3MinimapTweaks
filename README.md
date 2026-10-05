# W3MinimapTweaks

Minimap tweaks for **Warcraft III 1.26a and 1.27b**.

[Русская версия](README.ru.md)

- **Ally Color Mode in campaign missions.** The button next to the minimap (or **Alt+A**) switches the colours to "you, allies, enemies", first on the minimap (you white, allies teal, enemies red), then on the units too (you blue, allies teal, enemies red). It works in every game except campaign missions, where the game greys it out. With this mod it works there as well.
- **Own Ally Color Mode colours on the minimap.** In that mode the minimap shows you white, allies teal, enemies red. Any of them can be set to another colour (R,G,B or a name), for example to see enemies better on the terrain. Only the minimap: the units keep their team colours.
- **Smaller unit dots.** The minimap is a 256x256 picture stretched over the minimap area; every unit is a 4x4 square in it, a building 8x8. On a big screen one point of that picture is several pixels, so groups of units melt into one blob. The mod draws the dots smaller, at the same place: by default as big on screen as the game's dots are at 1080p (4K: 2 and 4, 1440p: 3 and 6; on 1080p nothing changes). Heroes keep their own icon.

Everything happens in memory while the game runs. No game files are modified.

## Requirements

- Warcraft III 1.26a (Game.dll 1.26.0.6401) or 1.27b (Game.dll 1.27.1.7085). The mod checks the version and does nothing on any other patch.

## Installation

1. Copy `W3MinimapTweaks.mix` and `W3MinimapTweaks.ini` into the Warcraft III folder, next to `war3.exe`.
2. Start the game. The `.mix` is loaded automatically, no launcher needed.

Works alongside other `.mix` mods, for example [W3TrueWidescreen](https://github.com/Hr0ffT/W3TrueWidescreen) and [W3MultislotQuickSave](https://github.com/Hr0ffT/W3MultislotQuickSave).

To uninstall, delete `W3MinimapTweaks.mix` and `W3MinimapTweaks.ini`.

## Settings

All in `W3MinimapTweaks.ini`:

| Setting | Default | What it does |
|---|---|---|
| `CampaignAllyColors` | 1 | 1 = the Ally Color Mode button works in campaign missions too |
| `MinimapYouColor` | (empty) | Your colour on the minimap in Ally Color Mode: `R,G,B` (0..255) or a name (white, red, green, blue, yellow, orange, teal, cyan, purple, pink, magenta, black, gray). Empty = the game's (white) |
| `MinimapAllyColor` | (empty) | The same for allies (the game: teal) |
| `MinimapEnemyColor` | (empty) | The same for enemies (the game: red) |
| `MinimapCreepColor` | (empty) | The same for neutral hostile creeps (the game: dark blue) |
| `UnitDotSize` | 0 | Size of a unit's dot, in points of the 256x256 minimap picture (the game: 4). 0 = auto (by the screen height), 1..16 = fixed |
| `BuildingDotSize` | 0 | The same for buildings (the game: 8) |
| `Debug` | 0 | 1 = write `W3MinimapTweaks.log` next to the game (for bug reports) |

## Notes

- Blizzard never used this mode in the campaign, so a mission that recolours the player by script can show odd colours in it (for example, a mission where you play purple).
- The creep camp button next to it is greyed out in campaign missions too, but it is left as is: the game builds no creep camp marks there, so it would show nothing.

## Changelog

- **1.2** — Own minimap colours for Ally Color Mode (`MinimapYouColor`, `MinimapAllyColor`, `MinimapEnemyColor`, `MinimapCreepColor`).
- **1.1** — Smaller unit dots on the minimap (`UnitDotSize`, `BuildingDotSize`), automatic for the screen height.
- **1.0** — First release.

## Building from source

```
i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -s -o W3MinimapTweaks.mix src/W3MinimapTweaks.c -lversion
```

## License

MIT, see [LICENSE](LICENSE).
