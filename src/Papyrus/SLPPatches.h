#pragma once

// RustyOldSword patches for SexLab P+: natives for the MCM pages "RustyOldSword Patches" and "Integrations".
// Script: SLPPatches.psc (native globals only). The feature switches themselves are MCM_SETTINGs (mcm.def) and
// are read/written through sslSystemConfig.GetSettingBool/SetSettingBool like every other SexLab setting.
namespace Papyrus::Patches
{
    // One line per known mod: "Name|State|Detail". State is "on", "off" or "info".
    std::vector<std::string> GetIntegrations(RE::StaticFunctionTag*);
    // Version of these patches, e.g. "2.19.0-rso.2"
    std::string GetPatchVersion(RE::StaticFunctionTag*);

    inline bool Register(VM* a_vm)
    {
        REGISTERFUNC(GetIntegrations, "SLPPatches", true);
        REGISTERFUNC(GetPatchVersion, "SLPPatches", true);
        return true;
    }
}  // namespace Papyrus::Patches
