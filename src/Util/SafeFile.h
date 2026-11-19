#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// Settings files that must never be lost: a write goes to "<file>.tmp" first and replaces the file only when it is
// complete, the previous version stays as "<file>.bak"; a file that cannot be read is copied aside as
// "<file>.broken-<time>" and the backup is read instead. Header-only, no game types, so it is tested on its own.
namespace Util::SafeFile
{
    namespace fs = std::filesystem;

    inline fs::path WithSuffix(const fs::path& a_path, std::string_view a_suffix)
    {
        auto p = a_path;
        p += std::string{ a_suffix };
        return p;
    }

    inline fs::path BackupPath(const fs::path& a_path) { return WithSuffix(a_path, ".bak"); }

    // Writes a_text to a_path. Returns an error text, or nothing on success. The old file is untouched on failure.
    inline std::optional<std::string> Write(const fs::path& a_path, std::string_view a_text)
    {
        std::error_code ec;
        if (a_path.has_parent_path()) {
            fs::create_directories(a_path.parent_path(), ec);
        }
        const auto tmp = WithSuffix(a_path, ".tmp");
        {
            std::ofstream out{ tmp, std::ios::binary | std::ios::trunc };
            out.write(a_text.data(), static_cast<std::streamsize>(a_text.size()));
            out.flush();
            if (!out) {
                fs::remove(tmp, ec);
                return std::format("cannot write {}", tmp.string());
            }
        }
        if (fs::file_size(tmp, ec) != a_text.size() || ec) {
            fs::remove(tmp, ec);
            return std::format("{} is incomplete after writing", tmp.string());
        }
        // keep the previous version, but never replace a good backup with an empty (truncated) file
        if (fs::exists(a_path, ec) && fs::file_size(a_path, ec) > 0 && !ec) {
            fs::copy_file(a_path, BackupPath(a_path), fs::copy_options::overwrite_existing, ec);
            if (ec) {
                fs::remove(tmp, ec);
                return std::format("cannot back up {}: {}", a_path.string(), ec.message());
            }
        }
        fs::rename(tmp, a_path, ec);  // MoveFileEx(REPLACE_EXISTING): the file is either the old or the new one
        if (ec) {
            const auto msg = std::format("cannot replace {}: {}", a_path.string(), ec.message());
            fs::remove(tmp, ec);
            return msg;
        }
        return std::nullopt;
    }

    template <class T>
    struct Loaded
    {
        std::optional<T> value;           // nothing when neither the file nor its backup could be read
        std::vector<std::string> notes;   // what happened, for the log
    };

    // Reads a_path with a_parse (which throws on bad content). A missing file is not an error. An empty or damaged
    // file is copied aside and its backup is read instead.
    template <class T, class Parse>
    Loaded<T> Load(const fs::path& a_path, Parse&& a_parse)
    {
        Loaded<T> result;
        std::error_code ec;
        const bool exists = fs::exists(a_path, ec);
        if (exists) {
            if (fs::file_size(a_path, ec) == 0 && !ec) {
                result.notes.push_back(std::format("{} is empty", a_path.string()));
            } else {
                try {
                    result.value = a_parse(a_path);
                    return result;
                } catch (const std::exception& e) {
                    const auto stamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1);
                    const auto aside = WithSuffix(a_path, std::format(".broken-{}", stamp));
                    fs::copy_file(a_path, aside, fs::copy_options::skip_existing, ec);
                    result.notes.push_back(std::format("{} cannot be read ({}), kept as {}", a_path.string(), e.what(), aside.filename().string()));
                }
            }
        }
        const auto backup = BackupPath(a_path);
        if (fs::exists(backup, ec) && fs::file_size(backup, ec) > 0) {
            try {
                result.value = a_parse(backup);
                result.notes.push_back(std::format("read the backup {}", backup.filename().string()));
            } catch (const std::exception& e) {
                result.notes.push_back(std::format("backup {} cannot be read either ({})", backup.filename().string(), e.what()));
            }
        }
        return result;
    }

    inline std::string ReadText(const fs::path& a_path)
    {
        std::ifstream in{ a_path, std::ios::binary };
        if (!in) throw std::runtime_error("cannot open");
        std::ostringstream s;
        s << in.rdbuf();
        return s.str();
    }
}
