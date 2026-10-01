#pragma once

// Pure policy for the survival report ("info_survival" events): what the party's physical conditions
// look like on the wire, and when a report is worth sending again. No game types in here, so it is
// unit-tested without Skyrim (tests/SurvivalReportPolicyTests.cpp). The game reads live in
// SurvivalReporter.cpp.
//
// Optional mods are read softly: a mod that is not installed contributes NOTHING (the key is left
// out), which the server reads as "unknown", never as "fine". Every need is a level on the same
// 0..5 scale (0 = no need / best, higher = worse) so a consumer does not care which mod said it.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "json.hpp"

namespace SurvivalReportPolicy
{
    inline constexpr int kReportVersion = 1;
    inline constexpr int kMaxNeedLevel = 5;
    inline constexpr int kMaxExposureLevel = 6;   // Frostfall: 0 Warm ... 6 Maximum Exposure
    inline constexpr int kMaxWetLevel = 3;        // Frostfall: 0 Not wet ... 3 Drenched
    inline constexpr int kMaxDirtLevel = 4;       // Dirt and Blood: Dirt1..4 / Blood1..4, 0 = clean
    inline constexpr int kMaxHeatLevel = 3;       // Frostfall heat source size: 1 small ... 3 large

    // Re-send an unchanged report this often, so the server's copy never goes stale while nothing moves.
    inline constexpr std::chrono::seconds kHeartbeat{300};
    // A fire distance moves every step the player takes: only a change this large is news.
    inline constexpr float kDistanceStep = 100.0f;

    inline int ClampLevel(float raw, int maxLevel)
    {
        if (!(raw == raw)) {   // NaN
            return 0;
        }
        const int level = static_cast<int>(std::lround(raw));
        return std::clamp(level, 0, maxLevel);
    }

    // Highest N such that present[N] is true, or nullopt when none is: "which of the mod's six level
    // spells does this actor carry". An actor with none of them is unknown, not at level 0.
    template <std::size_t N>
    inline std::optional<int> HighestPresent(const std::array<bool, N>& present)
    {
        for (std::size_t i = N; i > 0; --i) {
            if (present[i - 1]) {
                return static_cast<int>(i - 1);
            }
        }
        return std::nullopt;
    }

    // Survival Mode CC stage spells: stage 1..5 present flags (index 0 = stage 1). On with no stage
    // spell = stage 0 (no need); off = unknown.
    inline std::optional<int> SurvivalStage(bool modeOn, const std::array<bool, 5>& stagePresent)
    {
        if (!modeOn) {
            return std::nullopt;
        }
        for (int stage = 5; stage >= 1; --stage) {
            if (stagePresent[static_cast<std::size_t>(stage - 1)]) {
                return stage;
            }
        }
        return 0;
    }

    // Who built the fire the party is warming at: the Campfire mod only lets the player place fires, so a
    // Campfire-placed fire within reach is the player's; any other lit fire (a hearth, a bandit camp, a
    // world brazier) is somebody else's.
    inline std::string FireBuilder(bool campfirePlacedFireNearby)
    {
        return campfirePlacedFireNearby ? "player" : "other";
    }

    struct CcSurvival
    {
        bool on = false;
        std::optional<int> hunger;
        std::optional<int> exhaustion;
        std::optional<int> cold;
    };

    struct PlayerState
    {
        std::optional<int> hunger;      // Last Seed 0 Well Fed .. 5 Starving
        std::optional<int> thirst;      // Last Seed 0 Quenched .. 5 Dehydrated
        std::optional<int> fatigue;     // Last Seed 0 Rested .. 5 Exhausted
        std::optional<int> exposure;    // Frostfall 0 Warm .. 6
        std::optional<int> wet;         // Frostfall 0 .. 3
        std::optional<int> dirt;        // Dirt and Blood 0 .. 4
        std::optional<int> blood;       // Dirt and Blood 0 .. 4
        std::optional<CcSurvival> cc;   // Survival Mode CC (present only when the CC is installed)
    };

    struct FireState
    {
        bool nearFire = false;
        int heat = 0;                   // 0 none, 1 small .. 3 large
        std::optional<float> distance;  // game units from the player
        std::string builder = "other";
    };

    struct ActorState
    {
        std::string name;
        bool follower = false;
        std::optional<float> distance;  // game units from the player
        std::optional<int> hunger;
        std::optional<int> thirst;
        std::optional<bool> dirty;
        std::optional<bool> bloody;
    };

    struct Snapshot
    {
        std::vector<std::string> mods;                  // lastseed, frostfall, campfire, dirtandblood, survivalcc
        PlayerState player;
        std::optional<FireState> fire;                  // only when Frostfall can say
        std::vector<ActorState> actors;
    };

    // The event name the server logs (core: any "info*" request is written to the eventlog and not
    // spoken). The data column carries the JSON below.
    inline constexpr const char* kEventType = "info_survival";

    // '|' separates the wire fields and a newline would split a log row: neither may appear in a name.
    inline std::string SafeName(std::string name)
    {
        for (char& c : name) {
            if (c == '|' || c == '\n' || c == '\r') {
                c = ' ';
            }
        }
        const auto first = name.find_first_not_of(' ');
        if (first == std::string::npos) {
            return "";
        }
        const auto last = name.find_last_not_of(' ');
        return name.substr(first, last - first + 1);
    }

    template <class T>
    inline void PutIfKnown(nlohmann::json& object, const char* key, const std::optional<T>& value)
    {
        if (value.has_value()) {
            object[key] = *value;
        }
    }

    inline nlohmann::json BuildReport(const Snapshot& snapshot)
    {
        using nlohmann::json;
        json report = json::object();
        report["v"] = kReportVersion;
        report["mods"] = snapshot.mods;

        json player = json::object();
        PutIfKnown(player, "hunger", snapshot.player.hunger);
        PutIfKnown(player, "thirst", snapshot.player.thirst);
        PutIfKnown(player, "fatigue", snapshot.player.fatigue);
        PutIfKnown(player, "exposure", snapshot.player.exposure);
        PutIfKnown(player, "wet", snapshot.player.wet);
        PutIfKnown(player, "dirt", snapshot.player.dirt);
        PutIfKnown(player, "blood", snapshot.player.blood);
        if (snapshot.player.cc.has_value()) {
            json cc = json::object();
            cc["on"] = snapshot.player.cc->on;
            PutIfKnown(cc, "hunger", snapshot.player.cc->hunger);
            PutIfKnown(cc, "exhaustion", snapshot.player.cc->exhaustion);
            PutIfKnown(cc, "cold", snapshot.player.cc->cold);
            player["cc"] = cc;
        }
        report["player"] = player;

        if (snapshot.fire.has_value()) {
            json fire = json::object();
            fire["near"] = snapshot.fire->nearFire;
            fire["heat"] = snapshot.fire->heat;
            if (snapshot.fire->distance.has_value()) {
                fire["dist"] = std::round(static_cast<double>(*snapshot.fire->distance) * 10.0) / 10.0;
            }
            fire["builder"] = snapshot.fire->builder;
            report["fire"] = fire;
        }

        json actors = json::object();
        for (const auto& actor : snapshot.actors) {
            const std::string name = SafeName(actor.name);
            if (name.empty()) {
                continue;
            }
            json entry = json::object();
            entry["follower"] = actor.follower;
            if (actor.distance.has_value()) {
                entry["d"] = static_cast<int>(std::lround(*actor.distance));
            }
            PutIfKnown(entry, "hunger", actor.hunger);
            PutIfKnown(entry, "thirst", actor.thirst);
            PutIfKnown(entry, "dirty", actor.dirty);
            PutIfKnown(entry, "bloody", actor.bloody);
            actors[name] = entry;
        }
        report["actors"] = actors;
        return report;
    }

    // The part of a report that decides "has anything changed": the distances are bucketed so walking
    // around a fire does not resend every eight seconds.
    inline std::string ChangeFingerprint(const Snapshot& snapshot)
    {
        Snapshot bucketed = snapshot;
        if (bucketed.fire.has_value() && bucketed.fire->distance.has_value()) {
            bucketed.fire->distance = std::floor(*bucketed.fire->distance / kDistanceStep) * kDistanceStep;
        }
        for (auto& actor : bucketed.actors) {
            if (actor.distance.has_value()) {
                actor.distance = std::floor(*actor.distance / kDistanceStep) * kDistanceStep;
            }
        }
        std::sort(bucketed.actors.begin(), bucketed.actors.end(),
                  [](const ActorState& a, const ActorState& b) { return a.name < b.name; });
        return BuildReport(bucketed).dump();
    }

    // An empty report (no mod answered and nobody is around) is never worth sending.
    inline bool HasAnything(const Snapshot& snapshot)
    {
        return !snapshot.mods.empty();
    }

    // Send when the fingerprint moved, or on the heartbeat.
    inline bool ShouldSend(const std::string& fingerprint, const std::string& lastFingerprint,
                           std::chrono::steady_clock::time_point lastSent, std::chrono::steady_clock::time_point now)
    {
        if (fingerprint != lastFingerprint) {
            return true;
        }
        return now - lastSent >= kHeartbeat;
    }
}
