# Semiclip Half-Life
Author: rtxa - Version: 2.2

## Description
This module allows to pass through players. Modified version from [s1lent's semiclip](https://forums.alliedmods.net/showthread.php?t=250891), made to work on Half-Life.

## Features
- Allows jump to crouching players when semiclip works;
- Fixed jamming on mobile platform (a global problem on DeathrunMod).
- Intersecting players can take damage.

## Installation:
- Move all files from archive `semiclip.dll` | `semiclip_mm_i386.so` to folder `/valve/addons/semiclip/`
- Open `/addons/metamod/plugins.ini` and insert path to the meta-plugin:

#### Linux
`linux addons/semiclip/semiclip_mm_i386.so`
#### Windows

`win32 addons\semiclip\semiclip.dll`

## Settings
```ini
# Description
# semiclip 0|1 disable/enable semiclip.
# team 0|1|2|3
# - 0 Semiclip works for all.
# - 1 Semiclip works only for BLUE team.
# - 2 Semiclip works only for RED team.
# - 3 Semiclip works only for teammates.

# patch 0|1 Fix jamming on a mobile platform. (A global problem on DeathrunMod)
# crouch 0|1 Allows jump to crouching players when semiclip works.
# effects 0|1 Effect of transparency of the player. Depends from distance between players.
# distance 0|200 At what distance player can have transparency and semiclip.
# transparency 0|255 transparency of the player.
```



