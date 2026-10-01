#include "SurvivalReporter.h"

#include "Globals.h"
#include "HTTPManager.h"
#include "Misc.h"
#include "SurvivalReportPolicy.h"

#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace logger = SKSE::log;

namespace
{
    using namespace SurvivalReportPolicy;

    // ----- the mods' own records (local form id, plugin file), taken from the plugins themselves:
    // LastSeed.esp, Frostfall.esp 3.4.1, Campfire.esm 1.12.1, Dirt and Blood - Dynamic Visuals.esp and
    // ccQDRSSE001-SurvivalMode.esl. A record that is not found reads as "that mod is not here".
    constexpr auto kLastSeed = "LastSeed.esp";
    constexpr RE::FormID kSeedHungerLevel = 0xF7D6;
    constexpr RE::FormID kSeedThirstLevel = 0xF7D7;
    constexpr RE::FormID kSeedFatigueLevel = 0xF7D8;
    constexpr RE::FormID kSeedHungerEnabled = 0x7073;   // 2 = on
    constexpr RE::FormID kSeedThirstEnabled = 0x7074;
    constexpr RE::FormID kSeedFatigueEnabled = 0x7075;
    // Level spells: index 0 = level 0 (Well Fed / Quenched) ... 5 (Starving / Dehydrated). They sit on the
    // player and on every follower the mod tracks, which is how a follower's own level is read.
    constexpr std::array<RE::FormID, 6> kSeedHungerSpells{0x34B0, 0x34B4, 0x34B8, 0x34BC, 0x34C0, 0x34C4};
    constexpr std::array<RE::FormID, 6> kSeedThirstSpells{0x5A90, 0x5A92, 0x5A94, 0x5A96, 0x5A98, 0x5A9A};

    constexpr auto kFrostfall = "Frostfall.esp";
    constexpr RE::FormID kFrostExposureLevel = 0x68119;
    constexpr RE::FormID kFrostWetLevel = 0x6458D;
    constexpr RE::FormID kFrostNearFire = 0x64AFD;          // 2 = near a fire (Frostfall's own heat source scan)
    constexpr RE::FormID kFrostHeatSourceSize = 0x64AFC;
    constexpr RE::FormID kFrostHeatSourceDistance = 0x64AFB;

    constexpr auto kCampfire = "Campfire.esm";
    constexpr RE::FormID kCampPlacedCampfire = 0x31DAF;     // _Camp_Campfire: the furniture the player places

    constexpr auto kDirtAndBlood = "Dirt and Blood - Dynamic Visuals.esp";
    constexpr std::array<RE::FormID, 4> kDirtSpells{0x806, 0x807, 0x808, 0x838};    // Dirt1..4 (the player)
    constexpr std::array<RE::FormID, 4> kBloodSpells{0x809, 0x80A, 0x80B, 0x839};   // Blood1..4 (the player)
    constexpr RE::FormID kDirtySpellNpc = 0x822;            // Dirty_Spell_DirtForNPCs
    constexpr RE::FormID kBloodySpellNpc = 0xDE6;           // Dirty_BloodySpellforNPCs

    constexpr auto kSurvivalCc = "ccQDRSSE001-SurvivalMode.esl";
    constexpr RE::FormID kCcModeEnabled = 0x826;            // Survival_ModeEnabled, 1 = on
    constexpr std::array<RE::FormID, 5> kCcHungerStages{0x87E, 0x880, 0x881, 0x886, 0x885};
    constexpr std::array<RE::FormID, 5> kCcExhaustionStages{0x878, 0x879, 0x87A, 0x87B, 0x87D};
    // With the CC's disease setting off, stages 3-5 of exhaustion are these spells instead (0 = no variant)
    constexpr std::array<RE::FormID, 5> kCcExhaustionNoDisease{0, 0, 0x90F, 0x90C, 0x90D};
    constexpr std::array<RE::FormID, 5> kCcColdStages{0x86E, 0x891, 0x86D, 0x870, 0x871};

    // Frostfall reports the distance to its nearest heat source (it looks at most 600 units out); a
    // Campfire-placed fire within that distance plus some slack is taken to be the fire it measured.
    constexpr float kCampfireReachSlack = 150.0f;
    constexpr float kFrostfallRange = 600.0f;

    std::mutex g_mutex;
    std::string g_lastFingerprint;
    std::chrono::steady_clock::time_point g_lastSent{};

    template <class T>
    T* Find(RE::FormID localId, const char* plugin)
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return nullptr;
        }
        return data->LookupForm<T>(localId, plugin);
    }

    std::optional<float> GlobalValue(RE::FormID localId, const char* plugin)
    {
        if (auto* global = Find<RE::TESGlobal>(localId, plugin)) {
            return global->value;
        }
        return std::nullopt;
    }

    // A Last Seed need switch: 2 = on. A missing record counts as on (the level global is still the truth).
    bool NeedSystemOn(RE::FormID enabledId)
    {
        const auto value = GlobalValue(enabledId, kLastSeed);
        return !value.has_value() || static_cast<int>(std::lround(*value)) == 2;
    }

    template <std::size_t N>
    std::array<bool, N> SpellsOn(RE::Actor* actor, const std::array<RE::FormID, N>& ids, const char* plugin)
    {
        std::array<bool, N> present{};
        for (std::size_t i = 0; i < N; ++i) {
            if (auto* spell = Find<RE::SpellItem>(ids[i], plugin)) {
                present[i] = actor && actor->HasSpell(spell);
            }
        }
        return present;
    }

    // The CC's stage spells, with an optional alternate spell per stage (0 = none): a stage is present when either is
    std::array<bool, 5> StagesOn(RE::Actor* actor, const std::array<RE::FormID, 5>& ids,
                                 const std::array<RE::FormID, 5>& alternates, const char* plugin)
    {
        auto present = SpellsOn(actor, ids, plugin);
        for (std::size_t i = 0; i < alternates.size(); ++i) {
            if (alternates[i] == 0 || present[i]) {
                continue;
            }
            if (auto* spell = Find<RE::SpellItem>(alternates[i], plugin)) {
                present[i] = actor && actor->HasSpell(spell);
            }
        }
        return present;
    }

    // Every plugin here has a record with a known local id; finding it is the presence test
    bool PluginPresent(const char* plugin, RE::FormID anchorId)
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        return data && data->LookupForm(anchorId, plugin) != nullptr;
    }

    // Is a Campfire-placed campfire within reach of the player's measured heat source?
    bool CampfirePlacedNearby(RE::PlayerCharacter* player, float heatDistance)
    {
        auto* base = Find<RE::TESFurniture>(kCampPlacedCampfire, kCampfire);
        if (!base || !player) {
            return false;
        }
        const float reach = (heatDistance >= 0.0f ? heatDistance : kFrostfallRange) + kCampfireReachSlack;
        bool found = false;
        if (auto* tes = RE::TES::GetSingleton()) {
            tes->ForEachReferenceInRange(player, reach, [&](RE::TESObjectREFR* ref) {
                if (ref && !ref->IsDisabled() && !ref->IsDeleted() && ref->GetBaseObject() == base) {
                    found = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
        return found;
    }

    void ReadPlayer(RE::PlayerCharacter* player, Snapshot& snap)
    {
        // Last Seed: the level globals are the party's needs (its API calls them the party's)
        if (PluginPresent(kLastSeed, kSeedHungerLevel)) {
            snap.mods.push_back("lastseed");
            if (NeedSystemOn(kSeedHungerEnabled)) {
                if (auto v = GlobalValue(kSeedHungerLevel, kLastSeed)) snap.player.hunger = ClampLevel(*v, kMaxNeedLevel);
            }
            if (NeedSystemOn(kSeedThirstEnabled)) {
                if (auto v = GlobalValue(kSeedThirstLevel, kLastSeed)) snap.player.thirst = ClampLevel(*v, kMaxNeedLevel);
            }
            if (NeedSystemOn(kSeedFatigueEnabled)) {
                if (auto v = GlobalValue(kSeedFatigueLevel, kLastSeed)) snap.player.fatigue = ClampLevel(*v, kMaxNeedLevel);
            }
        }

        // Frostfall: exposure and wetness are the player's; the party is out in the same weather
        const bool campfireMod = PluginPresent(kCampfire, kCampPlacedCampfire);
        if (PluginPresent(kFrostfall, kFrostExposureLevel)) {
            snap.mods.push_back("frostfall");
            if (auto v = GlobalValue(kFrostExposureLevel, kFrostfall)) snap.player.exposure = ClampLevel(*v, kMaxExposureLevel);
            if (auto v = GlobalValue(kFrostWetLevel, kFrostfall)) snap.player.wet = ClampLevel(*v, kMaxWetLevel);

            FireState fire;
            const auto nearFire = GlobalValue(kFrostNearFire, kFrostfall);
            fire.nearFire = nearFire.has_value() && static_cast<int>(std::lround(*nearFire)) == 2;
            if (auto v = GlobalValue(kFrostHeatSourceSize, kFrostfall)) fire.heat = ClampLevel(*v, kMaxHeatLevel);
            if (auto v = GlobalValue(kFrostHeatSourceDistance, kFrostfall); v.has_value() && *v >= 0.0f) {
                fire.distance = *v;
            }
            if (fire.nearFire) {
                fire.builder = FireBuilder(campfireMod && CampfirePlacedNearby(player, fire.distance.value_or(-1.0f)));
            }
            snap.fire = fire;
        }
        if (campfireMod) {
            snap.mods.push_back("campfire");   // with no Frostfall nothing can say whether a fire is lit
        }

        // Dirt and Blood: the player's level spells (Dirt1 / Blood1 is the lightest mark; none = clean)
        if (PluginPresent(kDirtAndBlood, kDirtySpellNpc)) {
            snap.mods.push_back("dirtandblood");
            const auto dirt = HighestPresent(SpellsOn(player, kDirtSpells, kDirtAndBlood));
            const auto blood = HighestPresent(SpellsOn(player, kBloodSpells, kDirtAndBlood));
            snap.player.dirt = dirt.has_value() ? *dirt + 1 : 0;
            snap.player.blood = blood.has_value() ? *blood + 1 : 0;
        }

        // Survival Mode CC: its stage spells, only while the mode is on
        if (PluginPresent(kSurvivalCc, kCcModeEnabled)) {
            snap.mods.push_back("survivalcc");
            CcSurvival cc;
            const auto on = GlobalValue(kCcModeEnabled, kSurvivalCc);
            cc.on = on.has_value() && static_cast<int>(std::lround(*on)) == 1;
            cc.hunger = SurvivalStage(cc.on, SpellsOn(player, kCcHungerStages, kSurvivalCc));
            cc.exhaustion = SurvivalStage(cc.on, StagesOn(player, kCcExhaustionStages, kCcExhaustionNoDisease, kSurvivalCc));
            cc.cold = SurvivalStage(cc.on, SpellsOn(player, kCcColdStages, kSurvivalCc));
            snap.player.cc = cc;
        }
    }

    void ReadActor(RE::Actor* actor, ActorState& state, bool lastSeed, bool dirtAndBlood)
    {
        if (lastSeed) {
            state.hunger = HighestPresent(SpellsOn(actor, kSeedHungerSpells, kLastSeed));
            state.thirst = HighestPresent(SpellsOn(actor, kSeedThirstSpells, kLastSeed));
        }
        if (dirtAndBlood) {
            auto* dirty = Find<RE::SpellItem>(kDirtySpellNpc, kDirtAndBlood);
            auto* bloody = Find<RE::SpellItem>(kBloodySpellNpc, kDirtAndBlood);
            if (dirty && bloody) {
                state.dirty = actor->HasSpell(dirty);
                state.bloody = actor->HasSpell(bloody);
            }
        }
    }
}

void ResetSurvivalReport()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_lastFingerprint.clear();
    g_lastSent = std::chrono::steady_clock::time_point{};
}

void PostSurvivalState(RE::PlayerCharacter* player, float radius)
{
    if (!player) {
        return;
    }
    try {
        Snapshot snap;
        ReadPlayer(player, snap);
        if (!HasAnything(snap)) {
            return;   // none of the optional mods is installed: nothing to report, nothing to send
        }
        const bool lastSeed = PluginPresent(kLastSeed, kSeedHungerLevel);
        const bool dirtAndBlood = PluginPresent(kDirtAndBlood, kDirtySpellNpc);

        AIAgentManager& manager = AIAgentManager::getInstance();
        for (const auto& agent : manager.getAgents()) {
            if (!agent) {
                continue;
            }
            auto* actor = agent->getActor();
            if (!actor) {
                actor = agent->getActorByFormId();
            }
            if (!actor || actor->GetFormID() == player->GetFormID() || actor->IsDeleted() || actor->IsDisabled()) {
                continue;
            }
            const float distance = player->GetPosition().GetDistance(actor->GetPosition());
            if (distance > radius) {
                continue;
            }
            std::string name = agent->getActorName();
            if (name.empty()) {
                name = actor->GetDisplayFullName();
            }
            ActorState state;
            state.name = name;
            state.follower = actor->IsPlayerTeammate();
            state.distance = distance;
            ReadActor(actor, state, lastSeed, dirtAndBlood);
            snap.actors.push_back(std::move(state));
        }

        const std::string fingerprint = ChangeFingerprint(snap);
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!ShouldSend(fingerprint, g_lastFingerprint, g_lastSent, now)) {
                return;
            }
            g_lastFingerprint = fingerprint;
            g_lastSent = now;
        }
        HTTPManager::log(std::format("{}|{}|{}|{}", kEventType, getCurrentTimeMillis(), GetGameTimeStamp(),
                                     BuildReport(snap).dump()));
        logger::debug("[SURVIVAL] reported {} mod(s), {} actor(s)", snap.mods.size(), snap.actors.size());
    } catch (const std::exception& e) {
        logger::warn("[SURVIVAL] report skipped: {}", e.what());
    }
}
