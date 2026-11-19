#pragma once

#include "Util/SafeFile.h"

// YAML on top of Util::SafeFile: every user settings file is read and written through these two functions.
namespace Util::SafeYaml
{
    // The file's YAML, or its backup when the file is empty or damaged (the damaged file is kept aside).
    // An empty node when neither exists or neither can be read.
    inline YAML::Node Load(const std::filesystem::path& a_path)
    {
        auto loaded = SafeFile::Load<YAML::Node>(a_path, [](const std::filesystem::path& p) { return YAML::LoadFile(p.string()); });
        for (const auto& note : loaded.notes) {
            logger::warn("Settings file: {}", note);
        }
        return loaded.value ? *loaded.value : YAML::Node{};
    }

    // Whether the file or its backup is there, so a loader can tell "no settings yet" from "nothing to read".
    inline bool Exists(const std::filesystem::path& a_path)
    {
        std::error_code ec;
        return std::filesystem::exists(a_path, ec) || std::filesystem::exists(SafeFile::BackupPath(a_path), ec);
    }

    inline bool Save(const std::filesystem::path& a_path, const YAML::Node& a_node)
    {
        std::stringstream text;
        text << a_node;
        if (const auto error = SafeFile::Write(a_path, text.str())) {
            logger::error("Settings file: {}", *error);
            return false;
        }
        return true;
    }
}
