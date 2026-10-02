# Traditional Dialogue Player TTS Interface Patches

These files are optional dialogue-menu overrides for `Player TTS for Traditional Dialogue`.

- `TraditionalDialoguePlayerTTS/Interface/dialoguemenu.swf` is for regular Skyrim/SSE.
- `TraditionalDialoguePlayerTTS-VR/Interface/dialoguemenu.swf` is for Skyrim VR.

`RelDynBody/` is not a dialogue patch: a Papyrus bridge for the RelDyn server extension that blushes an NPC through OBlush when the server asks (see its README for the command, the build and the in-game check). Install it only with OBlush and RelDyn.

Do not install either patch in the base CHIM deploy by default. These SWFs override the active dialogue menu, so users with VR or another dialogue-menu mod need the matching compatible variant.
