#include "Theme.h"

#include "Util/AsyncIO.h"
#include "Util/SafeFile.h"

#include <fstream>
#include <optional>

namespace Thread::Interface::UI::Theme
{
    namespace
    {
        constexpr auto THEME_PATH = USER_CONFIGS("ui_theme.json");

        struct WriteOptions : glz::opts
        {
            bool prettify = true;
            std::uint8_t indentation_width = 2;
        };

        bool loadComplete{ false };
    }

    void Load()
    {
        loadComplete = false;
        Util::AsyncIO::Submit([]() {
            std::optional<Data> loadedTheme;
            std::error_code fileError;
            if (fs::exists(THEME_PATH, fileError)) {
                Data candidate{};
                std::string buffer;
                if (const auto error = glz::read_file_json(candidate, THEME_PATH, buffer); error) {
                    logger::error("Unable to load UI theme: {}", glz::format_error(error, buffer));
                } else {
                    loadedTheme = candidate;
                    logger::info("Loaded UI theme from {}", THEME_PATH);
                }
            } else if (fileError) {
                logger::error("Unable to inspect UI theme path {}: {}", THEME_PATH, fileError.message());
            } else {
                loadedTheme.emplace();
                logger::info("UI theme file not found; using defaults");
            }

            SKSE::GetTaskInterface()->AddTask([loadedTheme = std::move(loadedTheme)]() mutable {
                if (loadedTheme)
                    data = std::move(*loadedTheme);
                loadComplete = true;
            });
        });
    }

    void Save()
    {
        if (!IsLoaded()) {
            logger::warn("UI theme save ignored while the initial load is pending");
            return;
        }

        auto snapshot = data;
        Util::AsyncIO::Submit([snapshot = std::move(snapshot)]() {
            const fs::path path{ THEME_PATH };
            std::error_code directoryError;
            fs::create_directories(path.parent_path(), directoryError);
            if (directoryError) {
                logger::error("Unable to create UI theme directory {}: {}", path.parent_path().string(), directoryError.message());
                return;
            }

            std::string buffer;
            if (const auto error = glz::write<WriteOptions{}>(snapshot, buffer); error) {
                logger::error("Unable to serialize UI theme: {}", glz::format_error(error, buffer));
                return;
            }

            if (const auto error = Util::SafeFile::Write(path, buffer)) {
                logger::error("Unable to write UI theme: {}", *error);
                return;
            }
            logger::info("Saved UI theme to {}", THEME_PATH);
        });
    }

    bool IsLoaded()
    {
        return loadComplete;
    }
}
