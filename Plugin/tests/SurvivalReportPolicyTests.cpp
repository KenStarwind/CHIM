#include "SurvivalReportPolicy.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) {
            std::cerr << message << std::endl;
            std::exit(1);
        }
    }
}

int main()
{
    using namespace SurvivalReportPolicy;
    using nlohmann::json;

    // Levels are clamped to their scale and never negative
    Check(ClampLevel(2.0f, kMaxNeedLevel) == 2, "A plain level must pass through");
    Check(ClampLevel(7.0f, kMaxNeedLevel) == kMaxNeedLevel, "A level above the scale must clamp to the top");
    Check(ClampLevel(-1.0f, kMaxNeedLevel) == 0, "A negative level (a mod's unset marker) must clamp to 0");
    Check(ClampLevel(2.4f, kMaxNeedLevel) == 2, "Globals are floats: 2.4 reads as level 2");
    Check(ClampLevel(std::nanf(""), kMaxNeedLevel) == 0, "NaN must not escape as a level");

    // Which of a mod's six level spells an actor carries: the highest one; none at all is unknown, not level 0
    Check(!HighestPresent(std::array<bool, 6>{false, false, false, false, false, false}).has_value(),
          "An actor with none of the level spells must be unknown");
    Check(HighestPresent(std::array<bool, 6>{true, false, false, false, false, false}).value() == 0,
          "Only the first spell present is level 0 (a known, well-fed actor)");
    Check(HighestPresent(std::array<bool, 6>{false, false, true, false, false, false}).value() == 2,
          "The third spell is level 2");
    Check(HighestPresent(std::array<bool, 6>{true, true, true, false, false, true}).value() == 5,
          "The highest spell present wins when a mod leaves an old one on");

    // Survival Mode CC: off = unknown; on with no stage spell = stage 0; otherwise the highest stage
    Check(!SurvivalStage(false, {false, false, false, false, false}).has_value(), "CC off must be unknown");
    Check(!SurvivalStage(false, {true, true, true, true, true}).has_value(), "CC off ignores stale stage spells");
    Check(SurvivalStage(true, {false, false, false, false, false}).value() == 0, "CC on with no stage is stage 0");
    Check(SurvivalStage(true, {true, false, false, false, false}).value() == 1, "Stage 1");
    Check(SurvivalStage(true, {true, true, true, false, false}).value() == 3, "The highest stage wins");

    // Who built the fire
    Check(FireBuilder(true) == "player", "A Campfire-placed fire is the player's");
    Check(FireBuilder(false) == "other", "Any other lit fire is somebody else's");

    // The wire format: unknown keys are left OUT, never zero
    Snapshot nothing;
    Check(!HasAnything(nothing), "No mod at all: nothing to say");
    Snapshot s;
    s.mods = {"lastseed", "dirtandblood"};
    s.player.hunger = 3;
    s.player.thirst = 1;
    s.player.fatigue = 0;
    s.player.dirt = 2;
    s.player.blood = 0;
    {
        FireState fire;
        fire.nearFire = true;
        fire.heat = 2;
        fire.distance = 212.34f;
        fire.builder = "player";
        s.fire = fire;
    }
    {
        ActorState aela;
        aela.name = "Aela the Huntress";
        aela.follower = true;
        aela.distance = 180.4f;
        aela.hunger = 2;
        aela.dirty = true;
        aela.bloody = false;
        s.actors.push_back(aela);
        ActorState stranger;
        stranger.name = "Hadvar";   // nothing readable about this one but where they stand
        stranger.distance = 900.0f;
        s.actors.push_back(stranger);
    }
    const json report = BuildReport(s);
    Check(report["v"] == 1, "The report carries its version");
    Check(report["mods"].size() == 2, "The installed mods are listed");
    Check(report["player"]["hunger"] == 3 && report["player"]["dirt"] == 2, "Known player values are sent");
    Check(report["player"]["blood"] == 0, "A known zero is sent as 0 (it is known)");
    Check(!report["player"].contains("exposure") && !report["player"].contains("cc"),
          "A mod that is absent leaves its keys out (unknown, never zero)");
    Check(report["fire"]["near"] == true && report["fire"]["heat"] == 2 && report["fire"]["builder"] == "player",
          "The fire block is sent");
    Check(report["fire"]["dist"] == 212.3, "The fire distance is rounded to a tenth");
    Check(report["actors"]["Aela the Huntress"]["follower"] == true &&
              report["actors"]["Aela the Huntress"]["hunger"] == 2 &&
              report["actors"]["Aela the Huntress"]["dirty"] == true &&
              report["actors"]["Aela the Huntress"]["bloody"] == false,
          "A follower's own readings are sent, a known false included");
    Check(!report["actors"]["Aela the Huntress"].contains("thirst"), "An unread thirst is left out");
    Check(report["actors"]["Hadvar"]["follower"] == false && report["actors"]["Hadvar"]["d"] == 900,
          "A bystander is listed with where they stand");

    // No fire block when Frostfall cannot say
    Snapshot noFire = s;
    noFire.fire.reset();
    Check(!BuildReport(noFire).contains("fire"), "No Frostfall: no fire block (the fire is unknown, not absent)");

    // The CC block
    Snapshot cc;
    cc.mods = {"survivalcc"};
    CcSurvival ccState;
    ccState.on = true;
    ccState.hunger = 1;
    ccState.exhaustion = 4;
    cc.player.cc = ccState;
    const json ccReport = BuildReport(cc);
    Check(ccReport["player"]["cc"]["on"] == true && ccReport["player"]["cc"]["exhaustion"] == 4 &&
              !ccReport["player"]["cc"].contains("cold"),
          "The CC block carries what it could read");

    // Names cannot break the wire (fields are split on '|', rows on newlines)
    Check(SafeName("A|B\nC") == "A B C", "Separators in a name become spaces");
    Check(SafeName("  Aela  ") == "Aela", "Names are trimmed");
    Snapshot odd;
    odd.mods = {"lastseed"};
    ActorState bad;
    bad.name = "|";
    odd.actors.push_back(bad);
    Check(BuildReport(odd)["actors"].empty(), "A nameless actor is not listed");

    // Change detection: steps around a fire are not news, a real change is, and a quiet report repeats on the heartbeat
    const auto t0 = std::chrono::steady_clock::now();
    const std::string fp = ChangeFingerprint(s);
    Snapshot stepped = s;
    stepped.fire->distance = 230.0f;                       // same 100-unit bucket
    stepped.actors[0].distance = 150.0f;                   // same bucket (100..199)
    Check(ChangeFingerprint(stepped) == fp, "Small movement is not a change");
    Snapshot moved = s;
    moved.fire->distance = 420.0f;
    Check(ChangeFingerprint(moved) != fp, "Walking away from the fire is a change");
    Snapshot hungrier = s;
    hungrier.player.hunger = 4;
    Check(ChangeFingerprint(hungrier) != fp, "A level change is a change");
    Snapshot reordered = s;
    std::reverse(reordered.actors.begin(), reordered.actors.end());
    Check(ChangeFingerprint(reordered) == fp, "The order actors are listed in is not a change");

    Check(ShouldSend(fp, "", t0, t0), "The first report is sent");
    Check(!ShouldSend(fp, fp, t0, t0 + std::chrono::seconds(30)), "An unchanged report is held back");
    Check(ShouldSend(fp, fp, t0, t0 + kHeartbeat), "An unchanged report is repeated on the heartbeat");
    Check(ShouldSend(ChangeFingerprint(hungrier), fp, t0, t0 + std::chrono::seconds(1)), "A change is sent at once");

    Check(std::string(kEventType) == "info_survival", "The event name is the one the server logs without speaking it");
    return 0;
}
