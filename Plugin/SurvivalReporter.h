#pragma once

#include "RE/Skyrim.h"

// Reports the party's physical conditions to the server as one "info_survival" event: Last Seed (hunger,
// thirst, fatigue), Frostfall (exposure, wetness, heat sources), Campfire (who built the fire the party
// is at), Dirt and Blood (the player and each NPC), and Survival Mode CC. Every one of those mods is
// optional and read softly: a mod that is not installed contributes nothing, and with none installed no
// event is sent at all. Sends only when something changed, plus a slow heartbeat. See
// SurvivalReportPolicy.h for the wire format.
void PostSurvivalState(RE::PlayerCharacter* player, float radius);

// Forget what was last sent, so the next call reports again (a save load: the server rolled its event log back).
void ResetSurvivalReport();
