# SexLab P++ Dwemer's Edition 2.19.6 (for SexLab P+ 2.19.0)

A patch package for **SexLab P+ 2.19.0**. Each patch can be switched off in the SexLab MCM; with every switch off,
SexLab P+ runs its own stock code.

## MCM page "Dwemer Patches"
| Switch | What it does |
|---|---|
| Petite Profiles | Short adult actors take part in scenes and get scenes and roles made for their height. Normal-height actors never take a short role |
| Heels Compensation | An actor standing on the feet keeps the heels; the heel lift is shared between a slightly smaller body, slightly slimmer legs and shorter knee and ankle joints, so the pelvis (sex) or the head (kissing) lands where the animation expects. Everyone else has the heel lift removed. Bent knees get corrective body morphs |
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

Requirements: SKSE, SexLab P+ 2.19.0, SKSE Menu Framework (scene HUD), RaceMenu (Heels Compensation and knee morphs),
XPMSSE (knee and ankle joints).
Optional: Devious Devices NG (scene filter). Recommended: Better Deformation with `NPC L Calf [LClf]` and
`NPC R Calf [RClf]` set to 1.0 in `BetterDeformation.json`, which keeps the front of a bent knee round.

## Changelog
### 2.19.6
A big step for knees and alignment: scenes on heels, kneeling and squatting now look much better than before.

Heels and alignment:
- The heel lift is now shared between four small changes, each kept below the point where it shows: the whole body
  slightly smaller, the legs slightly slimmer, the knee and ankle joints slightly shorter (XPMSSE `CME` joints, spread
  over the skin blend). The soles stay exactly on the floor.
- What has to match the animation depends on the stage: the pelvis for sex, the head for kissing, both when a stage
  does both, half and half for hugging and other face-to-face stages. Kissing partners on different heels now meet at
  the mouth.
- `[Heels]` keys in `SexLab.ini`: `bHeelLegJoints`, `bHeelStageFocus`, `fHeelTol*`, `fHeelKneeBlend`,
  `fHeelAnkleBlend`, `fHeelMaxKneeShift`, `fHeelMaxAnkleShift`. `fHeelBodyMinFactor` is no longer used.

Knees:
- On a deeply bent knee (kneeling, squatting, sitting on the heels) the calf used to come through the thigh. Every
  scene actor now gets corrective body morphs by how far the knee is bent, the way real flesh gives way: a thinner calf
  back and a fuller lower thigh, with a second step for kneeling upright. UBE and CBBE 3BA sliders are both written; a
  body uses only the ones it has. The morphs are removed when the scene ends; animations are not changed.
- The amounts were tuned on the real UBE and 3BA bodies (their own meshes, skin weights and sliders) and checked in game.
  `[Knees]` keys in `SexLab.ini` (`bKneeMorph`, `fKneeMorph*`) change them.

Petite:
- One petite woman + one normal-height woman in Petite-tagged scenes without role marks (Billyy Petite Lesbian): the
  petite woman now takes her own role (the first position) instead of either role.

### 2.19.5
Petite:
- A short adult is now recognised by her height relative to her race, not by the rendered scale. A 0.8 NPC of a race
  that is 1.03 tall renders at 0.824 and used to count as normal height, so she got normal scenes and was stretched.
  `fPetiteAdultScale` in `SexLab.ini` is now 0.85 (was 0.8).
- One petite woman + one normal-height woman: scenes tagged `Petite` with two female roles that do not say which role is
  the petite one were all refused, so this pair had no scenes at all. Either woman may now take either role, and the
  petite one keeps her height. All other combinations keep the existing rules.

Heels Compensation:
- Standing on heels: the body is scaled down only to 0.95 at most and the thighs are shortened for the rest (feet keep
  their size), instead of shrinking the whole actor. If the thigh scale does not hold in an animation, that actor falls
  back to the old body-only fit. New `SexLab.ini` keys in `[Heels]`: `bHeelLegShortening`, `fHeelBodyMinFactor`,
  `fHeelMinLegScale`.
- An actor only slightly taller than the animation (for example 1.03) is now scaled to it; before, anything within 0.03
  was left as is.

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
