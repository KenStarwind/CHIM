# RelDyn body bridge (optional, local `reldyn-blush` branch)

A game-side bridge for the RelDyn extension of HerikaServer (roadmap `oblush-physical-blush`, Ken 2026-10-01). It puts a blush on an NPC's face through [OBlush](https://www.nexusmods.com/skyrimspecialedition/mods/145083) when the server asks, and takes it off after a time. It is not part of the base deploy: it needs OBlush and RelDyn, and nothing else uses it.

## How it is reached (no DLL change)

The server queues a row in `responselog` for the NPC: `actor = <NPC name>`, `action = command|ExtCmdRelDynBody_Blush@<seconds>@<token>`. The plugin's `ExtCmd` branch (`Plugin/Commands.cpp`) takes the text between `ExtCmd` and the first `_` as a script name and calls `RelDynBody.DispatchExternalCommand(npcName, command, parameter)` when that script exists (custom-plugins guide, "Game-side integration"). Nothing to register, no ESP record.

| Command | Does |
|---|---|
| `ExtCmdRelDynBody_Blush@<seconds>@<token>` | `OBlushHandle.enableBlush(Actor)` on the NPC, `OBlushHandle.disableBlush(Actor, node)` after `<seconds>` real seconds. A newer token while it is on extends it (the older wait gives way); it never takes a second overlay slot. |
| `ExtCmdRelDynBody_BlushOff@<token>` | Takes it off now. The server's safeguard when the game never said the blush ended (a save loaded in the middle of it). Does nothing when the NPC is not blushing, or has a newer blush than `<token>`. |

`RelDynBody` finds the NPC by display name among `AIAgentFunctions.findAllNearbyAgents()` and calls `RelDynBodyOBlush` only when `OBlush.esp` is loaded; every OBlush type lives in `RelDynBodyOBlush`, so a load order without OBlush never loads a script that names them. The node and the newest token per NPC are kept in StorageUtil (saved with the game). OBlush's master switch (`OBEnable`, its MCM) is respected. Its male / female toggles are for its OStim scenes and are not read: RelDyn is for every character.

## Build

`D:\DwemerDistro\build\compile.ps1` compiles `AIAgent/Source/Scripts` only and does not see these. `OBlushHandle.psc` is type-checked with the rest of its imports, so compiling needs the sources of OBlush, OStim Standalone (`OsexIntegrationMain`), JContainers (OStim's `OUtils`) and a full `NiOverride.psc`. The toolchain's `build\headers_unused\NiOverride.psc` is a trimmed stub without the overlay functions, and the SKEE source is not in the mod list (RaceMenu ships it compiled). `compile.ps1` here is the same call as the stock one with those import paths added and one scratch `NiOverride.psc` (the stock stub plus the real SKEE signatures OBlush and OUtils call). Only the two scripts' own bytecode is written (`build\out-reldyn-blush`); the dependencies are type-checked, not built. Install: copy the two `.pex` files to `Data\Scripts` (an MO2 mod folder, or the overwrite).

## Manual in-game check of the blush itself

Needs OBlush enabled (`OBlush.esp`, its MCM "Enable" on), the two `.pex` in `Data\Scripts`, a follower or any CHIM agent standing near you, and the HerikaServer with RelDyn (`body` section on, the default).

1. The bridge alone. On the server (psql in the Dwemer distro), with the NPC loaded:
   `INSERT INTO responselog (localts, sent, actor, text, action, tag) VALUES (extract(epoch from now())::bigint, 0, 'Aela the Huntress', '', 'command|ExtCmdRelDynBody_Blush@20@1', '');`
   Within a second or two the NPC's cheeks should colour (a short fade in), and about 20 seconds later fade out. Repeat with `@60@2` and, 10 seconds in, `@60@3`: it stays on, one overlay, and goes off 60 s after the second. `...BlushOff@3` clears it at once.
2. The whole path. Flirt with an NPC who has some passion for you (a hug, a compliment in their love language, a kiss through Sharmat). The reply should carry the flush in its words, and the blush appears with it: a faint moment about half a minute, a strong one about two, longer for a primary love-language gesture, longer for a shy NPC. A second gesture while it is on does not blink it. `SELECT actor, action, sent FROM responselog WHERE action LIKE '%RelDynBody%' ORDER BY localts DESC;` shows the rows (`sent = 1` once the game polled them).
3. The refusals. OBlush off in its MCM: nothing happens. The NPC far away or unloaded: nothing, and the Papyrus log says `[RelDyn] blush for <name>: not nearby, dropped`. OBlush not loaded: `... OBlush.esp is not loaded, dropped`. A male NPC and a female one both blush. Save in the middle of a blush and reload: within a few exchanges the server sends `BlushOff` once.
4. Logs: `Documents\My Games\Skyrim Special Edition\Logs\Script\Papyrus.0.log` (grep `RelDyn`), the CHIM log for `[COMMAND_QUEUE] Processing command`.

Not verified without the game: that `enableBlush` finds a free face overlay slot on the NPC (it returns an empty node when RaceMenu's overlays are all used, and the bridge then logs `no free face overlay slot`), and the NiOverride fade timing.
