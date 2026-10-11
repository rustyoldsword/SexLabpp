# SexLab P++ Dwemer's Edition (for SexLab P+ 2.19)

A patch package for **SexLab P+ 2.19.0**. It adds Petite Profiles, Heels Compensation, a Clothing tab, a Devious
Devices scene filter and an enjoyment fix. Each of them can be switched on or off in the SexLab MCM. With every switch
off, SexLab P+ runs its own stock code.

It replaces the earlier separate releases for SexLab P+ 2.18.1 (Petite Profiles, Heels Compensation).

## MCM page "RustyOldSword Patches"
| Switch | What it does | Off = |
|---|---|---|
| Petite Profiles | Short adult actors take part in scenes. They are matched with scenes tagged `Petite`, get petite smf/msmf seating and Petite voices | stock scale check and scene choice |
| Heels Compensation | An actor standing on the feet keeps the heels and is scaled to the animation; everyone else has the heel lift removed. Heels are not stripped | stock scaling |
| Clothing tab | Clothing tab in the scene HUD and the strip/redress rules stored in `SKSE\SexLab\UserData\Wardrobe.yaml` | stock stripping, tab hidden |
| Devious Devices scene filter | Scenes are chosen by the devices each actor wears (belt, plug, gag, bra, hobble dress, bound arms), using DD's own rules, so DD no longer ends or replaces the scene | stock scene choice |
| Enjoyment fix | Keeps the fraction of every enjoyment tick, so actors with a small gain (most male roles) no longer get stuck below orgasm | stock enjoyment |

- All switches are on by default.
- Like every other SexLab setting, they are stored in `SKSE\SexLab\UserData\Settings.yaml` and apply to all saves.
- A change takes effect from the next scene. The Enjoyment fix applies at once.

## MCM page "Integrations"
A read-only list of the mods this build works with:
- WetFunction NG, Cumshot NG, Afterglow (and whether Afterglow or SexLab paints the cum overlays)
- Heels Fix NG, SexLab Survival and its Framework, Diary of Mine and its Framework, Creature Framework NG
- Devious Devices (and whether the scene filter is active), the arousal framework
- NaughtyVoices, SexLab Petite Voices, Player Voice Sync, Vitals HUD, RaceMenu

Highlight a row to see the full status.

## Clothing tab
For each clothing group (head, neck, body, underwear, hands, rings, feet, legs, back, shield, weapons, other) you
choose two things:
- what the scene may do with it: SexLab decides / always take off / never take off;
- what happens after the scene: SexLab's setting / always put back / leave off.

For feet there is also an option to keep high heels on. Rules can be stored for this scene, for this character or for
everyone. The tab also has Undress / Dress / Reset buttons.

## Installation
Install SexLab P+ 2.19.0 first, then install this package with a mod manager and let it overwrite SexLab P+.
The installer has one optional component, Player Voice Sync. It resets the player's SexLab voice when the voice
does not match the character's sex. Skip it if you already have Player Voice Sync installed.

Requirements: SKSE, SexLab P+ 2.19.0, SKSE Menu Framework (scene HUD), RaceMenu (Heels Compensation).
Optional: Devious Devices NG (scene filter).

## Files
| File | |
|---|---|
| `SKSE\Plugins\SexLabUtil.dll` | SexLab P+ 2.19.0 DLL with the patches, their switches and the natives for the MCM pages |
| `SKSE\Plugins\SexLab.ini` | stock 2.19.0 INI plus the `[Animation]` petite keys, `[Devices]` and `[Heels]` |
| `Scripts\sslConfigMenu.pex` | stock 2.19.0 SexLab MCM plus the two pages |
| `Scripts\sslActorAlias.pex` | stock 2.19.0 plus the Enjoyment fix behind its switch |
| `Scripts\SLPPatches.pex` | native declarations for the pages |
| `Interface\SexLab\Clothing\*.png` | Clothing tab icons |

## This repository
A fork of [SLP-Community/SexLabpp](https://github.com/SLP-Community/SexLabpp) at commit `d578f817` (SexLab P+ 2.19.0),
branch `dwemers-edition`. Licensed under GPL-3.0 like the upstream project.

| Path | |
|---|---|
| `src/Papyrus/SLPPatches.*` | natives for the MCM pages (integrations, patch version) |
| `src/Registry/Util/Scale.*` | Petite Profiles and Heels Compensation |
| `src/Registry/Util/Wardrobe.*`, `src/Thread/Interface/Elements/ClothingPanel.*` | Clothing tab and its rules |
| `src/Thread/ThreadCtor.cpp`, `src/Registry/Library.cpp` | Devious Devices scene filter |
| `src/UserData/mcm.def` | the five switches (`bPatchPetite`, `bPatchHeels`, `bPatchClothing`, `bPatchDevices`, `bPatchEnjoyment`) |
| `dist/Source/Scripts/` | `sslConfigMenu.psc`, `sslActorAlias.psc`, `SLPPatches.psc` |
| `dist/Interface/SexLab/Clothing/` | Clothing tab icons |

Build like upstream:
```
xmake f -m release --build_papyrus=n --build_spriggit=n --build_assets=n -y
xmake
```
