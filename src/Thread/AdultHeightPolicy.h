#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

// Adult humanoid scene compatibility only. Admission/age validation remains in Registry::Validation.
namespace Thread::AdultHeight
{
    enum class Profile
    {
        Normal,
        PetiteFemale,
        PetiteMaleSMF,
        PetiteMaleMSMF,
        PetiteMaleSMSMF,
        PetiteMaleSMFF,
        MetadataRequired,
    };

    struct Participant
    {
        bool male;
        bool female;
        float scale;
    };

    struct Role
    {
        bool male;
        bool female;
        float scale;
        bool petiteAnnotation;
        bool normalAnnotation;
    };

    inline Profile Classify(std::span<const Participant> a_actors, float a_threshold)
    {
        if (!std::isfinite(a_threshold) || a_threshold <= 0.0f) return Profile::MetadataRequired;
        size_t pm = 0, pf = 0, nm = 0, nf = 0;
        for (const auto& actor : a_actors) {
            if (!std::isfinite(actor.scale) || actor.scale <= 0.0f || actor.male == actor.female) {
                return Profile::MetadataRequired;
            }
            const bool petite = actor.scale <= a_threshold;
            if (actor.male) {
                petite ? ++pm : ++nm;
            } else {
                petite ? ++pf : ++nf;
            }
        }
        if (!(pm + pf) || !(nm + nf)) {
            return Profile::Normal;
        }
        if (pm && pf) {
            return Profile::MetadataRequired;
        }
        if (pf) {
            return Profile::PetiteFemale;
        }
        if (pm == 1 && nm == 0 && nf == 1) return Profile::PetiteMaleSMF;
        if (pm == 1 && nm == 1 && nf == 1) return Profile::PetiteMaleMSMF;
        if (pm == 2 && nm == 0 && nf == 1) return Profile::PetiteMaleSMSMF;
        if (pm == 1 && nm == 0 && nf == 2) return Profile::PetiteMaleSMFF;
        return Profile::MetadataRequired;
    }

    // Bits correspond to the five existing profile tags in declaration order. Multiple tags are ambiguous.
    inline std::optional<Profile> DecodeProfile(unsigned a_mask)
    {
        if (a_mask > 31 || (a_mask && (a_mask & (a_mask - 1)))) return std::nullopt;
        if (!a_mask) return Profile::Normal;
        for (unsigned i = 0; i < 5; ++i) {
            if (a_mask == (1u << i)) return static_cast<Profile>(i + 1);
        }
        return std::nullopt;
    }

    inline std::optional<std::vector<bool>> ResolveRoles(std::span<const Role> a_roles, unsigned a_mask, float a_threshold)
    {
        const auto profile = DecodeProfile(a_mask);
        if (!profile || a_roles.empty() || !std::isfinite(a_threshold) || a_threshold <= 0.0f) return std::nullopt;
        const bool explicitPetite = std::ranges::any_of(a_roles, [a_threshold](const auto& role) {
            return role.petiteAnnotation || role.scale <= a_threshold;
        });
        std::vector<bool> result;
        size_t maleIndex = 0;
        for (const auto& role : a_roles) {
            if (!std::isfinite(role.scale) || role.scale <= 0.0f ||
                (role.petiteAnnotation && role.normalAnnotation) ||
                (role.normalAnnotation && role.scale <= a_threshold)) return std::nullopt;
            if (role.male && !role.female) ++maleIndex;
            bool petite = role.petiteAnnotation || role.scale <= a_threshold;
            if (!explicitPetite && !role.normalAnnotation && *profile != Profile::Normal) {
                // Legacy profile conventions apply only when the package supplies no short-role metadata.
                if (role.male == role.female) return std::nullopt;
                petite = *profile == Profile::PetiteFemale ? role.female :
                    role.male && (*profile != Profile::PetiteMaleMSMF || maleIndex == 1);
            }
            result.push_back(petite);
        }
        return result;
    }

    inline bool MatchesProfile(Profile a_actorProfile, unsigned a_mask, const std::vector<bool>& a_roles)
    {
        const auto profile = DecodeProfile(a_mask);
        if (!profile || a_roles.empty()) return false;
        if (*profile == a_actorProfile) return true;
        // Untagged scenes may explicitly describe combinations without a legacy profile tag.
        return *profile == Profile::Normal && a_actorProfile != Profile::Normal &&
            std::ranges::any_of(a_roles, [](bool p) { return p; }) &&
            std::ranges::any_of(a_roles, [](bool p) { return !p; });
    }

    inline bool MatchesAssignment(const std::vector<bool>& a_actors, const std::vector<bool>& a_roles)
    {
        if (a_actors.empty() || a_actors.size() != a_roles.size()) return false;
        // A uniformly short adult group uses an ordinary, uniformly normal-size animation at its own height.
        if (std::ranges::all_of(a_actors, [](bool p) { return p; }) &&
            std::ranges::none_of(a_roles, [](bool p) { return p; })) return true;
        return a_actors == a_roles;
    }
}
