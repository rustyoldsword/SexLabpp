#include "StripData.h"

#include "StripYaml.h"

namespace UserData
{
    void StripData::Load()
    {
        try {
            const auto handler = RE::TESDataHandler::GetSingleton();
            _root = YAML::LoadFile(STRIP_PATH);
            for (const auto&& mod : _root) {
                const auto esp = mod.first.as<std::string>();
                if (handler->LookupModByName(esp) == nullptr) {
                    logger::info("Cannot load strip settings for {}. Plugin is not found", esp);
                    continue;
                }
                for (const auto&& i : mod.second) {
                    const auto id = i.first.as<uint32_t>();
                    const auto formid = handler->LookupFormID(id, esp);
                    strips[formid] = Strip(i.second.as<int32_t>());
                }
            }
        } catch (const std::exception& e) {
            logger::error("Unable to load data. Error: {}", e.what());
        }
        logger::info("Loaded {} custom strip settings", strips.size());
    }

    namespace
    {
        // Plugin file and local form id of a loaded form, as the strip settings file stores it; empty file if unknown
        std::pair<std::string, uint32_t> FileEntry(RE::FormID a_formid)
        {
            const auto modidx = a_formid >> 24;
            if (modidx == 0xFE) {
                const auto file = RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByIndex(static_cast<uint16_t>((a_formid >> 12) & 0xFFF));
                return { file ? file->fileName : ""s, (0xFFF & a_formid) };
            }
            const auto file = RE::TESDataHandler::GetSingleton()->LookupLoadedModByIndex(static_cast<uint8_t>(modidx));
            return { file ? file->fileName : ""s, (0xFFFFFF & a_formid) };
        }
    }

    // An item reset in the Strip Editor must also leave the file tree, otherwise Save writes the old value back
    // and the next game start restores it.
    void StripData::RemoveArmor(RE::TESForm* a_form)
    {
        strips.erase(a_form->formID);
        const auto [file, id] = FileEntry(a_form->formID);
        if (!file.empty()) {
            StripYaml::Erase(_root, file, id);
        }
    }

    // Reset Strip Overrides: settings of plugins that are not loaded now stay in the file for when they come back
    void StripData::RemoveArmorAll()
    {
        strips.clear();
        const auto handler = RE::TESDataHandler::GetSingleton();
        StripYaml::EraseLoaded(_root, [handler](const std::string& a_file) { return handler->LookupModByName(a_file) != nullptr; });
    }

    void StripData::Save()
    {
        try {
            for (auto&& [formid, strip] : strips) {
                const auto [file, id] = FileEntry(formid);
                if (file.empty()) {
                    logger::error("Unable to find mod for formid {}", formid);
                    continue;
                }
                StripYaml::Write(_root, file, id, static_cast<int32_t>(strip));
            }
            std::ofstream{ STRIP_PATH } << _root;
        } catch (const std::exception& e) {
            logger::error("Unable to save StripConfig. Error: {}", e.what());
        }
        logger::info("Saved custom strip settings for {} mods", _root.size());
    }

    Strip StripData::CheckStrip(RE::TESForm* a_form)
    {
        const auto it = strips.find(a_form->formID);
        if (it == strips.end() || it->second == Strip::None) {
            return CheckKeywords(a_form);
        }
        return it->second;
    }

    Strip StripData::CheckKeywords(RE::TESForm* a_form)
    {
        const auto& kwd = a_form->As<RE::BGSKeywordForm>();
        if (kwd) {
            if (kwd->ContainsKeywordString("NoStrip"))
                return Strip::NoStrip;
            if (kwd->ContainsKeywordString("AlwaysStrip"))
                return Strip::Always;
        }
        return Strip::None;
    }
}  // namespace UserData
