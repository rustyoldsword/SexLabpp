#pragma once

// Live tuning and on-screen debugging of the knee fix and the heel leg fit.
// Data\SKSE\Plugins\SexLabKneeDebug.ini, when present, overrides the [Knees] settings of SexLab.ini and is re-read
// whenever it changes, so values can be tuned while a scene is running. With bDraw = 1 the bones, the knee offset and
// the numbers are drawn over every scene actor's legs, and every fDumpInterval seconds the raw numbers go to the log.
namespace Registry::LegDebug
{
    struct Params
    {
        bool kneeFix{ false };
        float front{ 0.0f };        // offset toward the kneecap at 90 degrees of flexion, skeleton units
        float along{ 0.0f };        // offset along the thigh (+ toward the knee, - toward the hip) at 90 degrees
        float max{ 6.0f };
        float floorHeight{ 12.0f };
        float smoothing{ 0.05f };
        bool draw{ false };
        float dumpInterval{ 0.0f };  // 0 = no dump
        bool poseDump{ false };      // with the dump: world positions of contact bones (role analysis)
        bool fromDebugFile{ false };
        // Corrective body morphs by knee flexion ([KneeMorph]); slider values at full gain, absent sliders ignored
        bool morph{ true };
        float morphStart{ 90.0f };
        float morphFull{ 140.0f };
        float morphHold{ 2.0f };
        float ubeCalfUpper{ -2.0f };
        float ubeThighLower{ 1.5f };
        float ubeThighBack{ 1.5f };
        float ubeThighOuter{ 1.0f };
        float cbbeCalfSize{ -2.5f };
        float cbbeCalfSmooth{ 1.5f };
        float cbbeCalfDepth{ -1.5f };   // CalfFBThicc_v2: - = calf thinner front to back
        float cbbeLegsFuller{ 0.5f };   // ChubbyLegs: + = fuller thighs
        // second tier, added on top from deepStart to deepFull degrees (kneeling upright)
        float deepStart{ 150.0f };
        float deepFull{ 167.0f };
        float ubeDeepCalfUpper{ 0.0f };
        float cbbeDeepCalfSize{ -1.0f };
    };

    Params Get();
    void Poll(float a_delta);  // once per frame
    bool DumpDue(RE::FormID a_actor, float a_delta);

    struct Item
    {
        RE::NiPoint3 a{};
        RE::NiPoint3 b{};
        uint32_t color{ 0 };
        std::string text{};  // non-empty: label at a
    };
    void Submit(RE::FormID a_actor, std::vector<Item>&& a_items);
    void Clear(RE::FormID a_actor);
    void Render();  // ImGui foreground, from the scene HUD
}
