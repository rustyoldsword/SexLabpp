#include "SLPPatches.h"

namespace Papyrus::Patches
{
    namespace
    {
        // Packed SKSE plugin version (major << 24 | minor << 16 | patch << 4 | build), as CommonLib writes it.
        // Plugins that are not built with CommonLib (skee) report a plain number instead: shown as is.
        std::string FormatVersion(std::uint32_t a_version)
        {
            if (a_version < (1u << 16)) {
                return std::to_string(a_version);
            }
            return std::format("{}.{}.{}", (a_version >> 24) & 0xFF, (a_version >> 16) & 0xFF, (a_version >> 4) & 0xFFF);
        }

        // First of the candidate SKSE plugin names that is loaded; empty when none is
        std::string SksePlugin(std::initializer_list<std::string_view> a_names)
        {
            for (const auto name : a_names) {
                if (const auto info = SKSE::GetPluginInfo(name)) {
                    return FormatVersion(info->version);
                }
            }
            return {};
        }

        // ESP/ESM/ESL in the active load order. Full and light (ESL-flagged) plugins are kept in two separate
        // lists, so both are searched (Papyrus GetModByName does not see light plugins at all).
        bool HasFile(std::string_view a_file)
        {
            const auto handler = RE::TESDataHandler::GetSingleton();
            return handler && (handler->LookupLoadedModByName(a_file) || handler->LookupLoadedLightModByName(a_file));
        }

        std::string Line(std::string_view a_name, bool a_on, std::string_view a_detail)
        {
            return std::format("{}|{}|{}", a_name, a_on ? "on" : "off", a_detail);
        }

        std::string Installed(std::string_view a_version)
        {
            return a_version.empty() ? "Not installed" : std::format("Installed, v{}", a_version);
        }
    }

    std::string GetPatchVersion(RE::StaticFunctionTag*)
    {
        return "2.19.0-rso.2";
    }

    std::vector<std::string> GetIntegrations(RE::StaticFunctionTag*)
    {
        std::vector<std::string> ret{};

        const auto wfng = SksePlugin({ "WetFunctionNG", "WetFunction NG" });
        ret.push_back(Line("WetFunction NG", !wfng.empty(), Installed(wfng)));

        const bool cumshot = HasFile("Cumshot.esp");
        const bool hybrid = HasFile("Cumshot - Particles Hybrid.esp");
        ret.push_back(Line("Cumshot NG", cumshot, !cumshot ? "Not installed" : (hybrid ? "Installed, particles hybrid on" : "Installed")));

        // Whether Afterglow or SexLab paints the overlays is a Papyrus/StorageUtil flag; the MCM page adds it
        ret.push_back(Line("Afterglow", HasFile("SLACS_2_Plus_SCO.esp"), HasFile("SLACS_2_Plus_SCO.esp") ? "Installed" : "Not installed"));

        const auto heelsFix = SksePlugin({ "HeelsFixNative" });
        ret.push_back(Line("Heels Fix NG", !heelsFix.empty(),
            heelsFix.empty() ? "Not installed" : std::format("Installed, v{}. In SexLab scenes the heels are handled by Heels Compensation", heelsFix)));

        const bool sls = HasFile("SL Survival.esp");
        const auto slsfw = SksePlugin({ "SLSurvivalFramework" });
        ret.push_back(Line("SexLab Survival", sls, sls ? "Installed" : "Not installed"));
        ret.push_back(Line("SexLab Survival Framework", !slsfw.empty(), Installed(slsfw)));

        const bool dom = HasFile("DiaryOfMine.esm");
        const auto domfw = SksePlugin({ "DiaryOfMineFramework" });
        ret.push_back(Line("Diary of Mine", dom, dom ? "Installed" : "Not installed"));
        ret.push_back(Line("Diary of Mine Framework", !domfw.empty(), Installed(domfw)));

        const auto cfng = SksePlugin({ "CreatureFrameworkNG", "CreatureFramework NG" });
        ret.push_back(Line("Creature Framework NG", !cfng.empty(), Installed(cfng)));

        const bool dd = HasFile("Devious Devices - Integration.esm");
        ret.push_back(Line("Devious Devices", dd,
            !dd ? "Not installed - the Devious Devices filter has nothing to do" :
                  (::Settings::bPatchDevices && ::Settings::bDeviceSceneFilter ? "Installed, scene filter active" : "Installed, scene filter off")));

        const auto osl = SksePlugin({ "OSLAroused", "OSL Aroused" });
        ret.push_back(Line("Arousal", !osl.empty() || HasFile("SexLabAroused.esm"),
            !osl.empty() ? std::format("OSL Aroused v{}", osl) : (HasFile("SexLabAroused.esm") ? "SexLab Aroused" : "Not installed")));

        const bool nv = HasFile("NaughtyVoicesSLPP.esp");
        const bool nvDialogue = HasFile("NaughtyVoicesSLPP_Dialogue.esp");
        ret.push_back(Line("NaughtyVoices", nv, !nv ? "Not installed" : (nvDialogue ? "Voices + dialogue" : "Voices")));

        ret.push_back(Line("SexLab Petite Voices", HasFile("SLPetiteVoices.esp"), HasFile("SLPetiteVoices.esp") ? "Installed" : "Not installed"));
        ret.push_back(Line("Player Voice Sync", HasFile("SLPP_PlayerVoiceSync.esp"), HasFile("SLPP_PlayerVoiceSync.esp") ? "Installed" : "Not installed"));

        const auto vitals = SksePlugin({ "VitalsHUD", "Vitals HUD" });
        ret.push_back(Line("Vitals HUD", !vitals.empty(), Installed(vitals)));

        const auto skee = SksePlugin({ "skee" });
        // skee reports a plain build number (1) instead of the RaceMenu version, so no version is shown
        ret.push_back(Line("RaceMenu (skee)", !skee.empty(), skee.empty() ? "Not installed - Heels Compensation needs it" : "Installed"));

        return ret;
    }
}  // namespace Papyrus::Patches
