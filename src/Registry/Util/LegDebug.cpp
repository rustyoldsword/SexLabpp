#include "LegDebug.h"

#include <SimpleIni.h>
#include "SKSEMenuFramework.h"

namespace Registry::LegDebug
{
    namespace
    {
        constexpr auto debugPath{ "Data\\SKSE\\Plugins\\SexLabKneeDebug.ini" };

        std::mutex lock{};
        Params current{};
        bool initialised{ false };
        float pollTimer{ 0.0f };
        std::optional<std::filesystem::file_time_type> stamp{};
        std::unordered_map<RE::FormID, float> dumpTimers{};

        struct Entry
        {
            std::vector<Item> items{};
            uint64_t frame{ 0 };
        };
        std::unordered_map<RE::FormID, Entry> entries{};
        uint64_t frameCounter{ 0 };

        Params FromSettings()
        {
            Params p;
            p.kneeFix = ::Settings::bKneeFix;
            p.front = ::Settings::fKneeFixFront;
            p.along = ::Settings::fKneeFixAlong;
            p.max = ::Settings::fKneeFixMax;
            p.floorHeight = ::Settings::fKneeFixFloorHeight;
            p.smoothing = ::Settings::fKneeFixSmoothing;
            p.morph = ::Settings::bKneeMorph;
            p.morphStart = ::Settings::fKneeMorphStart;
            p.morphFull = ::Settings::fKneeMorphFull;
            p.morphHold = ::Settings::fKneeMorphHold;
            p.ubeCalfUpper = ::Settings::fKneeMorphUBECalfUpper;
            p.ubeThighLower = ::Settings::fKneeMorphUBEThighLower;
            p.ubeThighBack = ::Settings::fKneeMorphUBEThighBack;
            p.cbbeCalfSize = ::Settings::fKneeMorph3BACalfSize;
            p.cbbeCalfSmooth = ::Settings::fKneeMorph3BACalfSmooth;
            p.ubeThighOuter = ::Settings::fKneeMorphUBEThighOuter;
            p.cbbeCalfDepth = ::Settings::fKneeMorph3BACalfDepth;
            p.cbbeLegsFuller = ::Settings::fKneeMorph3BALegsFuller;
            p.deepStart = ::Settings::fKneeMorphDeepStart;
            p.deepFull = ::Settings::fKneeMorphDeepFull;
            p.ubeDeepCalfUpper = ::Settings::fKneeMorphUBEDeepCalfUpper;
            p.cbbeDeepCalfSize = ::Settings::fKneeMorph3BADeepCalfSize;
            return p;
        }

        void Reload()
        {
            Params p = FromSettings();
            std::error_code ec;
            const auto time = std::filesystem::last_write_time(debugPath, ec);
            if (ec) {
                if (stamp) {
                    logger::info("Knee debug: {} removed, back to SexLab.ini", debugPath);
                }
                stamp.reset();
                current = p;
                return;
            }
            if (stamp && *stamp == time) {
                return;
            }
            stamp = time;
            CSimpleIniA ini{};
            if (ini.LoadFile(debugPath) < 0) {
                logger::warn("Knee debug: cannot read {}", debugPath);
                return;
            }
            const auto f = [&](const char* a_key, float a_default) { return static_cast<float>(ini.GetDoubleValue("Knees", a_key, a_default)); };
            p.kneeFix = ini.GetBoolValue("Knees", "bKneeFix", p.kneeFix);
            p.front = f("fKneeFixFront", p.front);
            p.along = f("fKneeFixAlong", p.along);
            p.max = f("fKneeFixMax", p.max);
            p.floorHeight = f("fKneeFixFloorHeight", p.floorHeight);
            p.smoothing = f("fKneeFixSmoothing", p.smoothing);
            p.draw = ini.GetBoolValue("Debug", "bDraw", false);
            p.dumpInterval = static_cast<float>(ini.GetDoubleValue("Debug", "fDumpInterval", 0.0));
            p.poseDump = ini.GetBoolValue("Debug", "bPoseDump", false);
            const auto m = [&](const char* a_key, float a_default) { return static_cast<float>(ini.GetDoubleValue("KneeMorph", a_key, a_default)); };
            p.morph = ini.GetBoolValue("KneeMorph", "bEnabled", p.morph);
            p.morphStart = m("fStart", p.morphStart);
            p.morphFull = m("fFull", p.morphFull);
            p.morphHold = m("fHold", p.morphHold);
            p.ubeCalfUpper = m("fUBECalfUpper", p.ubeCalfUpper);
            p.ubeThighLower = m("fUBEThighLower", p.ubeThighLower);
            p.ubeThighBack = m("fUBEThighBack", p.ubeThighBack);
            p.cbbeCalfSize = m("f3BACalfSize", p.cbbeCalfSize);
            p.cbbeCalfSmooth = m("f3BACalfSmooth", p.cbbeCalfSmooth);
            p.ubeThighOuter = m("fUBEThighOuter", p.ubeThighOuter);
            p.cbbeCalfDepth = m("f3BACalfDepth", p.cbbeCalfDepth);
            p.cbbeLegsFuller = m("f3BALegsFuller", p.cbbeLegsFuller);
            p.deepStart = m("fDeepStart", p.deepStart);
            p.deepFull = m("fDeepFull", p.deepFull);
            p.ubeDeepCalfUpper = m("fUBEDeepCalfUpper", p.ubeDeepCalfUpper);
            p.cbbeDeepCalfSize = m("f3BADeepCalfSize", p.cbbeDeepCalfSize);
            p.fromDebugFile = true;
            current = p;
            logger::info("Knee debug: loaded {} - knee fix {}, front {:.2f}, along {:.2f}, max {:.2f}, floor {:.1f}, smoothing {:.3f}, draw {}, dump {:.1f}s",
                debugPath, p.kneeFix, p.front, p.along, p.max, p.floorHeight, p.smoothing, p.draw, p.dumpInterval);
            logger::info("Knee debug: morph {}, {:.0f}..{:.0f} deg, hold {:.1f}s, UBE calf upper {:+.2f} thigh lower {:+.2f} back {:+.2f}, 3BA calf size {:+.2f} smooth {:+.2f}; deep {:.0f}..{:.0f} deg UBE calf upper {:+.2f}, 3BA calf size {:+.2f}",
                p.morph, p.morphStart, p.morphFull, p.morphHold, p.ubeCalfUpper, p.ubeThighLower, p.ubeThighBack, p.cbbeCalfSize, p.cbbeCalfSmooth,
                p.deepStart, p.deepFull, p.ubeDeepCalfUpper, p.cbbeDeepCalfSize);
        }
    }

    Params Get()
    {
        std::scoped_lock guard{ lock };
        if (!initialised) {
            initialised = true;
            Reload();
        }
        return current;
    }

    void Poll(float a_delta)
    {
        std::scoped_lock guard{ lock };
        frameCounter++;
        pollTimer -= a_delta;
        if (initialised && pollTimer > 0.0f) {
            return;
        }
        initialised = true;
        pollTimer = 1.0f;
        Reload();
    }

    bool DumpDue(RE::FormID a_actor, float a_delta)
    {
        std::scoped_lock guard{ lock };
        if (current.dumpInterval <= 0.0f) {
            return false;
        }
        auto& timer = dumpTimers[a_actor];
        timer -= a_delta;
        if (timer > 0.0f) {
            return false;
        }
        timer = current.dumpInterval;
        return true;
    }

    void Submit(RE::FormID a_actor, std::vector<Item>&& a_items)
    {
        std::scoped_lock guard{ lock };
        auto& entry = entries[a_actor];
        entry.items = std::move(a_items);
        entry.frame = frameCounter;
    }

    void Clear(RE::FormID a_actor)
    {
        std::scoped_lock guard{ lock };
        entries.erase(a_actor);
        dumpTimers.erase(a_actor);
    }

    void Render()
    {
        auto* camera = RE::Main::WorldRootCamera();
        auto* drawList = ImGuiMCP::GetForegroundDrawList();
        const auto display = ImGuiMCP::GetIO()->DisplaySize;
        if (!camera || !drawList || display.x <= 0.0f || display.y <= 0.0f) {
            return;
        }
        const auto project = [&](const RE::NiPoint3& a_point, ImGuiMCP::ImVec2& a_screen) {
            float x{}, y{}, z{};
            if (!camera->WorldPtToScreenPt3(a_point, x, y, z, 1.0e-5f) || z < 0.0f) {
                return false;
            }
            a_screen = { x * display.x, (1.0f - y) * display.y };
            return true;
        };
        std::scoped_lock guard{ lock };
        if (!current.draw) {
            return;
        }
        for (const auto& [actor, entry] : entries) {
            if (frameCounter > entry.frame + 30) {
                continue;  // actor no longer updated (left the scene, unloaded)
            }
            for (const auto& item : entry.items) {
                ImGuiMCP::ImVec2 a{}, b{};
                if (!project(item.a, a)) {
                    continue;
                }
                if (!item.text.empty()) {
                    ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::ImVec2{ a.x + 6.0f, a.y - 6.0f }, item.color, item.text.c_str());
                } else if (project(item.b, b)) {
                    ImGuiMCP::ImDrawListManager::AddLine(drawList, a, b, item.color, 2.0f);
                }
            }
        }
    }
}
