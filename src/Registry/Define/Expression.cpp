#include "Expression.h"

#include "Util/SafeYaml.h"

namespace Registry
{
    namespace detail
    {
        struct LegacyExpressionStrings
        {
            std::optional<std::string> name;
        };

        struct LegacyExpression
        {
            std::optional<LegacyExpressionStrings> string;
            std::optional<std::unordered_map<std::string, std::vector<float>>> floatList;
            std::optional<std::unordered_map<std::string, int32_t>> integers;

            struct glaze
            {
                using T = LegacyExpression;
                static constexpr auto value = glz::object(&T::string, &T::floatList, "int", &T::integers);
            };
        };
    }

    Expression::Expression(const YAML::Node& a_src) :
      id(a_src["id"].as<std::string>("Missing Name")),
      version(static_cast<uint8_t>(a_src["version"].as<uint32_t>(0))),
      enabled(a_src["enabled"].as<bool>(true)),
      tags(a_src["tags"].as<std::vector<std::string>>(std::vector<std::string>{})),
      scaling(Scaling(a_src["scaling"].as<int32_t>(std::to_underlying(Scaling::Linear))))
    {
        if (!a_src["data"].IsDefined())
            throw std::runtime_error("Missing data field");
        for (size_t i = 0; i < RE::SEXES::kTotal; i++) {
            auto& fields = a_src["data"][i];
            for (auto&& arr : fields) {
                const auto values = arr.as<std::vector<float>>();
                if (values.size() != ValueType::Total) {
                    const auto err = std::format("{}: Invalid data field: {}/32 values found at {}.", id, values.size(), i);
                    throw std::runtime_error(err);
                }
                auto& it = data[i].emplace_back();
                std::copy_n(values.begin(), it.size(), it.begin());
            }
        }
    }

    Expression Expression::FromLegacyFile(const fs::path& a_path)
    {
        detail::LegacyExpression source;
        std::string buffer;
        const auto filename = a_path.string();
        constexpr glz::opts options{ .error_on_unknown_keys = false };
        if (const auto error = glz::read_file_json<options>(source, filename, buffer); error) {
            throw std::runtime_error(glz::format_error(error, buffer));
        }
        if (!source.string || !source.string->name || source.string->name->empty())
            throw std::runtime_error("Missing Name field");
        if (!source.floatList)
            throw std::runtime_error("Missing 'floatList' field");

        Expression expression{ RE::BSFixedString{ *source.string->name } };
        auto fill = [&](RE::SEXES::SEX sex, std::string prefix) {
            constexpr auto MAX_VALUE_FIELDS = 5;
            for (size_t i = 1; i <= MAX_VALUE_FIELDS; i++) {
                auto fieldname = prefix + std::to_string(i);
                auto field = source.floatList->find(fieldname);
                if (field == source.floatList->end())
                    break;
                const auto& values = field->second;
                if (values.size() != ValueType::Total) {
                    const auto err = std::format("Invalid value field: expected 32 values, found {} in field {}", values.size(), fieldname);
                    throw std::runtime_error(err);
                }
                auto& it = expression.data[sex].emplace_back();
                std::copy_n(values.begin(), it.size(), it.begin());
            }
        };
        fill(RE::SEXES::kMale, "male");
        fill(RE::SEXES::kFemale, "female");
        if (expression.data[RE::SEXES::kMale].empty() && expression.data[RE::SEXES::kFemale].empty()) {
            throw std::runtime_error("Data fields have no values");
        }
        if (source.integers) {
            const auto get = [&](const char* fieldname) -> bool {
                auto field = source.integers->find(fieldname);
                return field != source.integers->end() && field->second == 1;
            };
            expression.enabled = get("enabled");
            for (auto&& tag : { "aggressor", "normal", "victim " })
                if (get(tag))
                    expression.tags.AddTag(tag);
        }
        return expression;
    }

    Expression::Expression(DefaultExpression a_default) :
      id(magic_enum::enum_name(a_default)), version(1), scaling(Scaling::Linear)
    {
        switch (a_default) {
        case DefaultExpression::Afraid:
            data[RE::SEXES::kMale] = {
                { 0.2f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.3f, 0.1f, 0.1f, 0, 0, 0.5f, 0.5f, 0, 0, 0, 0, 0.3f, 0.3f, 3, 1 },
                { 0.3f, 0, 0.3f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0.9f, 0, 0, 9, 1 }
            };
            data[RE::SEXES::kFemale] = {
                { 0.2f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.3f, 0.1f, 0.1f, 0, 0, 0.5f, 0.5f, 0, 0, 0, 0, 0.3f, 0.3f, 3, 1 },
                { 0.3f, 0, 0.3f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0.9f, 0, 0, 9, 1 },
            };
            tags = { std::vector{ "Aggressor", "Afraid", "Scared", "Pain", "Negative" } };
            break;
        case DefaultExpression::Angry:
            data[RE::SEXES::kMale] = {
                { 0, 0, 0, 0, 0.4f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.3f, 0.3f, 0, 0.4f },
                { 0, 0, 0, 0.4f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.65f, 0.65f, 0, 1 }
            };
            data[RE::SEXES::kFemale] = {
                { 0, 0, 0, 0, 0.4f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.3f, 0.3f, 0, 0.4f },
                { 0, 0, 0, 0.4f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.65f, 0.65f, 0, 1 },
            };
            tags = { std::vector{ "Aggressor", "Victim", "Mad", "Angry", "Upset" } };
            break;
        case DefaultExpression::Happy:
            data[RE::SEXES::kMale] = {
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f, 0.5f, 2.0f, 0.5f },
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.8f, 0.8f, 2.0f, 0.8f }
            };
            data[RE::SEXES::kFemale] = {
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f, 0.5f, 2.0f, 0.5f },
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.8f, 0.8f, 2.0f, 0.8f }
            };
            tags = { std::vector{ "Normal", "Happy", "Consensual" } };
            break;
        case DefaultExpression::Joy:
            data[RE::SEXES::kMale] = {
                { 0, 0, 0, 0, 0, 0.2f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2f, 0.2f, 0, 0, 0, 0, 0, 0, 13.0f, 0.4f },
                { 0.3f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.8f, 0.8f, 0, 0, 0, 0, 0.3f, 0.3f, 13.0f, 0.8f }
            };
            data[RE::SEXES::kFemale] = {
                { 0.3f, 0, 0, 0, 0, 0, 0, 0.6f, 0, 0, 0, 0, 0.6f, 0, 0, 0, 0.3f, 0.3f, 0, 0, 1.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0.7f, 0.7f, 10, 0.45f },
                { 0.6f, 0, 0, 0, 0, 0, 0.5f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0.3f, 0.3f, 0.7f, 0.7f, 1.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0.7f, 0.7f, 10, 0.6f },
            };
            tags = { std::vector{ "Normal", "Happy", "Joy", "Pleasure", "Consensual" } };
            break;
        case DefaultExpression::Pained:
            data[RE::SEXES::kMale] = {
                { 0.2f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.3f, 0.1f, 0.1f, 0, 0, 0.5f, 0.5f, 0, 0, 0, 0, 0.3f, 0.3f, 3, 1 },
                { 0.3f, 0, 0.3f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0.9f, 0, 0, 9, 1 }
            };
            data[RE::SEXES::kFemale] = {
                { 0.2f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.3f, 0.1f, 0.1f, 0, 0, 0.5f, 0.5f, 0, 0, 0, 0, 0.3f, 0.3f, 3, 1 },
                { 0.3f, 0, 0.3f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0.9f, 0, 0, 9, 1 }
            };
            tags = { std::vector{ "Victim", "Afraid", "Pain", "Pained", "Negative" } };
            break;
        case DefaultExpression::Pleasure:
            data[RE::SEXES::kMale] = {
                { 0, 0, 0, 0, 0, 0.2f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2f, 0.2f, 0, 0, 0, 0, 0, 0, 13.0f, 0.4f },
                { 0.3f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.8f, 0.8f, 0, 0, 0, 0, 0.3f, 0.3f, 13.0f, 0.8f }
            };
            data[RE::SEXES::kFemale] = {
                { 0, 0, 0, 0, 0, 0.3f, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.3f },
                { 0.4f, 0, 0, 0, 0, 0.2f, 0, 0, 0, 0, 0, 0, 0.8f, 0, 0, 0.2f, 0.15f, 0.15f, 0.25f, 0.25f, 0.6f, 0.6f, 0, 0, 0, 0, 0, 1.0f, 0.7f, 0.6f, 12.0f, 0.6f }
            };
            tags = { std::vector{ "Normal", "Happy", "Consensual", "Pleasure" } };
            break;
        case DefaultExpression::Sad:
            data[RE::SEXES::kMale] = {
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f, 0.5f, 2, 0.5f },
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0.6f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.8f, 0.8f, 2, 0.8f }
            };
            data[RE::SEXES::kFemale] = {
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f, 0.5f, 2, 0.5f },
                { 0, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0.6f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.8f, 0.8f, 2, 0.8f },
            };
            tags = { std::vector{ "Normal", "Victim", "Sad" } };
            break;
        case DefaultExpression::Shy:
            data[RE::SEXES::kMale] = {
                { 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2f, 0, 0, 4.0f, 0.9f },
                { 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f, 0.4f, 0, 0, 0.3f, 0, 3.0f, 0.5f }
            };
            data[RE::SEXES::kFemale] = {
                { 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2f, 0, 0, 4.0f, 0.9f },
                { 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f, 0.4f, 0, 0, 0.3f, 0, 3.0f, 0.5f },
            };
            tags = { std::vector{ "Normal", "Consensual", "Nervous", "Sad", "Shy" } };
            break;
        default:
            assert(false);
            break;
        }
    }

    std::array<float, Expression::ValueType::Total> Expression::GetData(RE::SEXES::SEX a_sex, float a_strength) const
    {
        if (version < 1) {
            if (data[a_sex].empty()) {
                logger::error("Invalid Expression Profile, {}: No data found", id);
                auto ret = std::array<float, Total>{};
                ret[MoodType] = 7;
                return ret;
            }
            const auto max = static_cast<float>(data[a_sex].size()) - 1;
            const auto idx = static_cast<size_t>(std::floor((max * a_strength) / 100));
            assert(idx < data[a_sex].size() && idx >= 0);
            return data[a_sex][idx];
        } else if (data[a_sex].size() != 2) {
            logger::error("Invalid Expression Profile, {}: {}/2 Profiles present", id, data[a_sex].size());
            auto ret = std::array<float, Total>{};
            ret[MoodType] = 7;
            return ret;
        }
        float multiplier;
        switch (scaling) {
        case Scaling::Linear:
            multiplier = a_strength / 100.0f;
            break;
        case Scaling::Square:
            multiplier = 0.0f;
            for (float i = 0.0f; i <= a_strength / 100.0f; i += 0.07f) {
                const auto f = pow(i - 0.55f, 2.0f) - 0.04f;
                multiplier += std::max<float>(0.0f, f);
            }
            break;
        case Scaling::Cubic:
            multiplier = 0.0f;
            for (float i = 0.0f; i <= a_strength / 100.0f; i += 0.05f) {
                const auto f = 1.1f * pow(i - 0.6f, 3.0f) + 0.08f;
                multiplier += std::max<float>(0.0f, f);
            }
            break;
        case Scaling::Exponential:
            multiplier = 0.0f;
            for (float i = 0.0f; i <= a_strength / 100.0f; i += 0.05f) {
                const auto f = static_cast<float>(pow(2048, i - 1.1f));
                multiplier += std::max<float>(0.0f, f);
            }
            break;
        }
        multiplier = std::min(1.25f, multiplier);
        std::array<float, Total> ret{};
        for (size_t i = 0; i < ret.size(); i++) {
            ret[i] = data[a_sex][0][i] + (data[a_sex][1][i] - data[a_sex][0][i]) * multiplier;
            if (i == MoodType) {
                ret[i] = std::round(ret[i]);
            }
        }
        return ret;
    }

    void Expression::Save(std::string_view a_fileLocation, bool force) const
    {
        if (!has_edits && !force)
            return;
        has_edits = false;
        YAML::Node file;
        file["id"] = id.data();
        file["version"] = static_cast<int32_t>(version);
        file["scaling"] = static_cast<int32_t>(scaling);
        for (auto&& tag : tags.AsVector()) {
            file["tags"].push_back(tag.data());
        }
        for (size_t i = 0; i < RE::SEXES::kTotal; i++) {
            if (data[i].empty()) {
                file["data"][i].push_back(std::vector<int>(32, 0));
            } else {
                for (size_t n = 0; n < data[i].size(); n++) {
                    file["data"][i].push_back(data[i][n]);
                }
            }
        }
        file["enabled"] = enabled;
        Util::SafeYaml::Save(std::format("{}\\{}.yaml", a_fileLocation, id), file);
    }

    void Expression::UpdateValues(bool a_female, int a_level, std::vector<float> a_values)
    {
        has_edits = true;
        auto& dataEntry = data[a_female];
        while (dataEntry.size() <= a_level) {
            dataEntry.emplace_back();
        }
        std::copy_n(a_values.begin(), dataEntry[a_level].size(), dataEntry[a_level].begin());
    }

    void Expression::UpdateTags(const TagData& a_newtags)
    {
        has_edits = true;
        tags = a_newtags;
    }

    void Expression::SetScaling(Scaling a_scaling)
    {
        has_edits = true;
        scaling = a_scaling;
    }

    void Expression::SetEnabled(bool a_enabled)
    {
        has_edits = true;
        enabled = a_enabled;
    }

}  // namespace Registry
