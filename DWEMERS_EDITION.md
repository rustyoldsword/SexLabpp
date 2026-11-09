# SexLab P++ Dwemer's Edition 2.19.4 (for SexLab P+ 2.19.0)

A patch package for **SexLab P+ 2.19.0**. Each patch can be switched off in the SexLab MCM; with every switch off,
SexLab P+ runs its own stock code.

## MCM page "Dwemer Patches"
| Switch | What it does |
|---|---|
| Petite Profiles | Short adult actors take part in scenes and get scenes and roles made for their height. Normal-height actors never take a short role |
| Heels Compensation | An actor standing on the feet keeps the heels and is scaled to the animation; everyone else has the heel lift removed |
| Clothing tab | Clothing tab in the scene HUD, with strip/redress rules per clothing group |
| Devious Devices scene filter | Scenes are chosen by the devices each actor wears, using DD's own rules |
| Enjoyment fix | Keeps the fraction of every enjoyment tick, so actors with a small gain no longer get stuck below orgasm |
| TNG gender | Female-bodied actors with a TNG schlong are treated as male (or as futa, if you choose) |
| Voice sex sync | Voices follow the body sex, also for the player after RaceMenu |

Two more switches have no MCM control and are on by default: `bPatchStanceGuard` and `bPatchTiltGuard` in
`SKSE\SexLab\Settings.yaml`. With a switch off, that guard only writes to the log (`SexLabUtil.log`, lines that
start with `Stance` and `Tilt`).

## MCM page "Integrations"
A read-only list of the mods this build works with and whether each is active. Highlight a row to see details.

## Installation
Install SexLab P+ 2.19.0 first, then install this package with a mod manager and let it overwrite SexLab P+.
Start a new game.

Requirements: SKSE, SexLab P+ 2.19.0, SKSE Menu Framework (scene HUD), RaceMenu (Heels Compensation).
Optional: Devious Devices NG (scene filter).

## Changelog
### 2.19.4
New:
- TNG gender: female-bodied actors with a TNG schlong get male roles by default; an MCM switch lets them play futa
  roles instead. Manual gender overrides still win.
- Voice sex sync: voices follow the body sex, also for the player after RaceMenu. This replaces the separate Player
  Voice Sync plugin of earlier versions; remove `SLPP_PlayerVoiceSync.esp` if you still have it.
- Stance guard: after a scene the player's weapon or spell could stay "drawn" while the hands were lowered, which
  blocked attack, jump and draw. The weapon state and the animation are now kept in step during and after a scene.
- Tilt guard: an actor's skeleton could keep leaning after a scene. It is now straightened when the actor leaves the
  scene.
- Theme editor: the enjoyment bars can be placed on screen (`anchorX` / `anchorY`).

Fixed:
- Strip Editor: an item set back to default, and "Reset Strip Overrides", were lost after the game restarted (SexLab
  P+ wrote the old value back). Note that SexLab writes these settings to disk only when you save the game.
- Height roles: a normal-height actor no longer takes a petite role, so a normal actor is not resized in a short
  actor's scene. Unsupported height mixes no longer fall back to an unchecked scene.
- Scene horses can no longer be mounted while they are in a scene.

From SexLab P+ 2.19.0-dev.1 (crajjjj), ported:
- Creatures: interactions are detected in creature scenes again (action text over the enjoyment bar, enjoyment from
  the actual action).
- Handjob and footjob interactions were swapped between the scripts and the DLL; enjoyment factors read the old
  interaction order.
- Actors left half restored after loading a save made during a scene are freed.
- Actor collision and movement are locked in a different way during scenes.
- `SetPathing()` ignored its pathing flag.

Not ported from dev.1: the CumFx changes (Afterglow paints the cum overlays in this setup).

### 2.19.1
- The erection is reset again when a scene ends (`SOSFlaccid`, as in SexLab 1.66).

### 2.19.0
- First release.

## This repository
A fork of [SLP-Community/SexLabpp](https://github.com/SLP-Community/SexLabpp) at commit `d578f817` (SexLab P+ 2.19.0),
branch `dwemers-edition`. Licensed under GPL-3.0 like the upstream project. The dev.1 fixes are cherry-picked from
crajjjj's SexLab P+ 2.19.0-dev.1 pull requests (#77, #78, #79, #84, #85, #86); the CumFx ones (#80-#82) are left out.

| Path | |
|---|---|
| `src/Papyrus/SLPPatches.*` | natives for the MCM pages (integrations, patch version) |
| `src/Registry/Util/Scale.*` | Petite Profiles and Heels Compensation |
| `src/Thread/AdultHeightPolicy.h`, `src/Thread/ThreadCtor.cpp` | height roles, Devious Devices scene filter |
| `src/Registry/Util/Wardrobe.*`, `src/Thread/Interface/Elements/ClothingPanel.*` | Clothing tab and its rules |
| `src/Registry/Define/Sex.cpp`, `src/Registry/Library.*` | TNG gender, voice sex sync |
| `src/Thread/PostSceneGuard.*` | stance and tilt guards |
| `src/UserData/StripData.*`, `src/UserData/StripYaml.h` | Strip Editor persistence fix |
| `src/UserData/mcm.def` | the switches (`bPatch*`) |
| `dist/Source/Scripts/` | the shipped `sslConfigMenu.psc`, `sslActorAlias.psc`, `SexLabThread.psc`, `SLPPatches.psc` |
| `dist/Interface/SexLab/Clothing/` | Clothing tab icons |

Build like upstream:
```
xmake f -m release --build_papyrus=n --build_spriggit=n --build_assets=n -y
xmake
```
