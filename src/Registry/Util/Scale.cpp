#include "Scale.h"

#include "HeelMath.h"
#include "Thread/AdultHeightPolicy.h"

namespace Registry
{
    namespace
    {
        constexpr const char* heelKey{ "internal" };
        constexpr const char* pelvisNode{ "NPC Pelvis [Pelv]" };
        constexpr std::array footNodes{ "NPC L Foot [Lft ]", "NPC R Foot [Rft ]" };
        constexpr std::array kneeNodes{ "NPC L Calf [LClf]", "NPC R Calf [RClf]" };
        constexpr std::array thighNodes{ "NPC L Thigh [LThg]", "NPC R Thigh [RThg]" };
        // Stock 0.03 skipped a 1.03 actor (1.03f - 1.0f is just below 0.03f) and left them taller than the animation
        constexpr float scaleTolerance{ 0.005f };
        constexpr float legScaleHold{ 0.01f };
        constexpr float driftTranslation{ 0.05f };
        constexpr float driftScale{ 0.001f };
        constexpr float reapplyCooldown{ 0.5f };

        bool IsFemale(RE::Actor* a_actor)
        {
            const auto base = a_actor->GetActorBase();
            return base ? base->GetSex() == RE::SEXES::kFemale : false;
        }

        // Height of a node in the unscaled skeleton space of the "NPC" node
        std::optional<float> SkeletonHeight(RE::Actor* a_actor, const RE::NiTransform& a_inverse, const char* a_node)
        {
            const auto node = a_actor->GetNodeByName(a_node);
            if (!node) {
                return std::nullopt;
            }
            const auto height = (a_inverse * node->world.translate).z;
            return std::isfinite(height) ? std::optional{ height } : std::nullopt;
        }
    }

    float Scale::GetScale(RE::TESObjectREFR* a_reference)
    {
        assert(a_reference);
        const auto node = a_reference->GetNodeByName(basenode);
        const auto baseScale = a_reference->GetScale();
        const auto nodeScale = node ? node->local.scale : 1.0f;
        const auto retVal = baseScale * nodeScale;
        logger::info("GetScale: {:X} -> Base = {}, Skeleton = {} => {}", a_reference->GetFormID(), baseScale, nodeScale, retVal);
        return retVal;
    }

    float Scale::GetRaceRelativeHeight(RE::Actor* a_actor)
    {
        assert(a_actor);
        const auto scale = GetScale(a_actor);
        const auto race = a_actor->GetRace();
        const float raceHeight = race ? race->data.height[IsFemale(a_actor) ? RE::SEXES::kFemale : RE::SEXES::kMale] : 1.0f;
        const auto ret = Thread::AdultHeight::RaceRelativeHeight(scale, raceHeight);
        logger::info("Race relative height: {:X} -> {} / race {} => {}", a_actor->GetFormID(), scale, raceHeight, ret);
        return ret;
    }

    void Scale::SetScale(RE::Actor* a_actor, float a_absolutescale)
    {
        SetScale(a_actor, { a_actor }, a_absolutescale);
    }

    void Scale::SetScale(RE::Actor* a_actor, RaceKey a_racekey, float a_absolutescale)
    {
        assert(a_actor && a_absolutescale > 0.0f);
        if (!::Settings::bPatchHeels) {
            DropHeelState(a_actor);
            SetScaleStock(a_actor, a_racekey, a_absolutescale);
            return;
        }
        if (!transformInterface) {
            if (!Settings::bDisableScale) {
                logger::error("Missing interface, scaling disabled");
                Settings::bDisableScale = true;
            }
            return;
        }
        switch (a_racekey) {
        case RaceKey::AshHopper:
            // a_absolutescale *= 0.5f;
            break;
        case RaceKey::Chaurus:
            a_absolutescale *= 0.5f;
            break;
        case RaceKey::ChaurusHunter:
            // a_absolutescale *= 0.69f;
            break;
        case RaceKey::Chicken:
            // a_absolutescale *= 1.3f;
            break;
        case RaceKey::Fox:
            // a_absolutescale *= 0.72f;
            break;
        case RaceKey::FrostAtronach:
            // a_absolutescale *= 1.3f;
            break;
        case RaceKey::Spider:
            a_absolutescale *= 0.75f;
            break;
        case RaceKey::LargeSpider:
            a_absolutescale *= 1.2f;
            break;
        case RaceKey::GiantSpider:
            a_absolutescale *= 1.9f;
            break;
        case RaceKey::Horker:
            // a_absolutescale *= 1.2f;
            break;
        case RaceKey::Mudcrab:
            // a_absolutescale *= 0.75f;
            break;
        default:
            // a_absolutescale *= 1.0f;
            break;
        }

        if (GetModernInterface()) {
            std::scoped_lock lock{ heelLock };
            auto& state = heelStates[a_actor->GetFormID()];
            state.absoluteScale = Settings::bDisableScale ? std::nullopt : std::optional{ a_absolutescale };
            state.sampling = true;
            state.stageTime = 0.0f;
            state.samples = 0;
            state.standingSamples = 0;
            state.anchors.clear();
            state.legDrops.clear();
            ApplyTransform(a_actor, state);
            return;
        }

        if (Settings::bDisableScale) {
            return;
        }
        float basescale = GetScale(a_actor);
        if (std::abs(basescale - a_absolutescale) < scaleTolerance) {
            logger::info("Attempted Node Transform to Actor = {:X}, Scale = {} -> {}", a_actor->GetFormID(), basescale, a_absolutescale);
            return;
        }

        const auto female = IsFemale(a_actor);
        const auto legacyInterface = reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(transformInterface);
        const SKEE::Legacy::FixedString node(basenode);
        const SKEE::Legacy::FixedString name(namekey);
        SKEE::Legacy::OverrideVariant scaleMode;
        scaleMode.SetInt(SKEE::Legacy::OverrideVariant::ScaleMode, ScaleModes::Multiplicative);
        legacyInterface->AddNodeTransform(a_actor, false, female, node, name, scaleMode);
        if (legacyInterface->RemoveNodeTransformComponent(a_actor, false, female, node, name, SKEE::Legacy::OverrideVariant::Scale, 0)) {
            legacyInterface->UpdateNodeTransforms(a_actor, false, female, node);
            basescale = GetScale(a_actor);
        }
        // base * x = absolute <=> x = absolute / base
        const float scale = a_absolutescale / basescale;
        logger::info("Applying Node Transform to Actor = {:X}, Scale = {} -> {}, x = {}", a_actor->GetFormID(), basescale, a_absolutescale, scale);
        SKEE::Legacy::OverrideVariant scaleOverride;
        scaleOverride.SetFloat(SKEE::Legacy::OverrideVariant::Scale, scale);
        legacyInterface->AddNodeTransform(a_actor, false, female, node, name, scaleOverride);
        legacyInterface->UpdateNodeTransforms(a_actor, false, female, node);
    }

    void Scale::RemoveScale(RE::Actor* a_actor)
    {
        assert(a_actor);
        if (!::Settings::bPatchHeels) {
            DropHeelState(a_actor);
            RemoveScaleStock(a_actor);
            return;
        }
        if (!transformInterface) {
            if (!Settings::bDisableScale) {
                logger::error("Missing interface, scaling disabled");
                Settings::bDisableScale = true;
            }
            return;
        }

        const auto female = IsFemale(a_actor);
        if (const auto modernInterface = GetModernInterface()) {
            {
                std::scoped_lock lock{ heelLock };
                heelStates.erase(a_actor->GetFormID());
            }
            // Heel compensation shares the key, so the whole key goes, not only its scale
            if (modernInterface->RemoveNodeTransform(a_actor, false, female, basenode, namekey)) {
                logger::info("Removed Transform Scale from {:X}", a_actor->GetFormID());
                modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
            }
            RemoveLegScale(a_actor, female);
            return;
        }
        if (Settings::bDisableScale) {
            return;
        }
        const auto legacyInterface = reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(transformInterface);
        const SKEE::Legacy::FixedString node(basenode);
        const SKEE::Legacy::FixedString name(namekey);
        if (!legacyInterface->RemoveNodeTransformComponent(a_actor, false, female, node, name, SKEE::Legacy::OverrideVariant::Scale, 0)) {
            return;
        }
        logger::info("Removed Transform Scale from {:X}", a_actor->GetFormID());
        legacyInterface->UpdateNodeTransforms(a_actor, false, female, node);
    }

    // Heels Compensation switched off: remove the heel translation we left on this actor while it was on
    // (it shares the "SexLabRegistry" key with the scene scale, which the stock code then handles itself)
    void Scale::DropHeelState(RE::Actor* a_actor)
    {
        {
            std::scoped_lock lock{ heelLock };
            if (heelStates.erase(a_actor->GetFormID()) == 0) {
                return;
            }
        }
        if (const auto modernInterface = GetModernInterface()) {
            const auto female = IsFemale(a_actor);
            if (modernInterface->RemoveNodeTransformPosition(a_actor, false, female, basenode, namekey)) {
                logger::info("Heels {:X}: compensation switched off, heel translation removed", a_actor->GetFormID());
                modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
            }
            RemoveLegScale(a_actor, female);
        }
    }

    // ---- stock SexLab P+ 2.19.0 (upstream d578f817), used while Heels Compensation is switched off

    void Scale::SetScaleStock(RE::Actor* a_actor, RaceKey a_racekey, float a_absolutescale)
    {
        assert(a_actor && a_absolutescale > 0.0f);
        if (Settings::bDisableScale) {
            return;
        } else if (!transformInterface) {
            logger::error("Missing interface, scaling disabled");
            Settings::bDisableScale = true;
            return;
        }
        switch (a_racekey) {
        case RaceKey::AshHopper:
            // a_absolutescale *= 0.5f;
            break;
        case RaceKey::Chaurus:
            a_absolutescale *= 0.5f;
            break;
        case RaceKey::ChaurusHunter:
            // a_absolutescale *= 0.69f;
            break;
        case RaceKey::Chicken:
            // a_absolutescale *= 1.3f;
            break;
        case RaceKey::Fox:
            // a_absolutescale *= 0.72f;
            break;
        case RaceKey::FrostAtronach:
            // a_absolutescale *= 1.3f;
            break;
        case RaceKey::Spider:
            a_absolutescale *= 0.75f;
            break;
        case RaceKey::LargeSpider:
            a_absolutescale *= 1.2f;
            break;
        case RaceKey::GiantSpider:
            a_absolutescale *= 1.9f;
            break;
        case RaceKey::Horker:
            // a_absolutescale *= 1.2f;
            break;
        case RaceKey::Mudcrab:
            // a_absolutescale *= 0.75f;
            break;
        default:
            // a_absolutescale *= 1.0f;
            break;
        }
        float basescale = GetScale(a_actor);
        if (std::abs(basescale - a_absolutescale) < 0.03) {
            logger::info("Attempted Node Transform to Actor = {:X}, Scale = {} -> {}", a_actor->GetFormID(), basescale, a_absolutescale);
            return;
        }

        const auto base = a_actor->GetActorBase();
        const auto female = base ? base->GetSex() == RE::SEXES::kFemale : false;
        if (transformInterface->GetVersion() < SKEE::INiTransformInterface::Version) {
            const auto legacyInterface = reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(transformInterface);
            const SKEE::Legacy::FixedString node(basenode);
            const SKEE::Legacy::FixedString name(namekey);
            SKEE::Legacy::OverrideVariant scaleMode;
            scaleMode.SetInt(SKEE::Legacy::OverrideVariant::ScaleMode, ScaleModes::Multiplicative);
            legacyInterface->AddNodeTransform(a_actor, false, female, node, name, scaleMode);
            if (legacyInterface->RemoveNodeTransformComponent(a_actor, false, female, node, name, SKEE::Legacy::OverrideVariant::Scale, 0)) {
                legacyInterface->UpdateNodeTransforms(a_actor, false, female, node);
                basescale = GetScale(a_actor);
            }
            // base * x = absolute <=> x = absolute / base
            const float scale = a_absolutescale / basescale;
            logger::info("Applying Node Transform to Actor = {:X}, Scale = {} -> {}, x = {}", a_actor->GetFormID(), basescale, a_absolutescale, scale);
            SKEE::Legacy::OverrideVariant scaleOverride;
            scaleOverride.SetFloat(SKEE::Legacy::OverrideVariant::Scale, scale);
            legacyInterface->AddNodeTransform(a_actor, false, female, node, name, scaleOverride);
            legacyInterface->UpdateNodeTransforms(a_actor, false, female, node);
        } else {
            const auto modernInterface = static_cast<SKEE::INiTransformInterface*>(transformInterface);
            modernInterface->AddNodeTransformScaleMode(a_actor, false, female, basenode, namekey, ScaleModes::Multiplicative);
            if (modernInterface->RemoveNodeTransformScale(a_actor, false, female, basenode, namekey)) {
                modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
                basescale = GetScale(a_actor);
            }

            const float scale = a_absolutescale / basescale;
            logger::info("Applying Node Transform to Actor = {:X}, Scale = {} -> {}, x = {}", a_actor->GetFormID(), basescale, a_absolutescale, scale);
            modernInterface->AddNodeTransformScale(a_actor, false, female, basenode, namekey, scale);
            modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
        }
    }

    void Scale::RemoveScaleStock(RE::Actor* a_actor)
    {
        assert(a_actor);
        if (Settings::bDisableScale) {
            return;
        } else if (!transformInterface) {
            logger::error("Missing interface, scaling disabled");
            Settings::bDisableScale = true;
            return;
        }

        const auto base = a_actor->GetActorBase();
        const auto female = base ? base->GetSex() == RE::SEXES::kFemale : false;
        if (transformInterface->GetVersion() < SKEE::INiTransformInterface::Version) {
            const auto legacyInterface = reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(transformInterface);
            const SKEE::Legacy::FixedString node(basenode);
            const SKEE::Legacy::FixedString name(namekey);
            if (!legacyInterface->RemoveNodeTransformComponent(a_actor, false, female, node, name, SKEE::Legacy::OverrideVariant::Scale, 0)) {
                return;
            }
            logger::info("Removed Transform Scale from {:X}", a_actor->GetFormID());
            legacyInterface->UpdateNodeTransforms(a_actor, false, female, node);
        } else {
            const auto modernInterface = static_cast<SKEE::INiTransformInterface*>(transformInterface);
            if (modernInterface->RemoveNodeTransformScale(a_actor, false, female, basenode, namekey)) {
                logger::info("Removed Transform Scale from {:X}", a_actor->GetFormID());
                modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
            }
        }
    }

    void Scale::RefreshBaseNode(RE::Actor* a_actor)
    {
        if (!a_actor || !transformInterface) {
            return;
        }
        const auto female = IsFemale(a_actor);
        if (const auto modernInterface = GetModernInterface()) {
            modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
            return;
        }
        const auto legacyInterface = reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(transformInterface);
        const SKEE::Legacy::FixedString node(basenode);
        legacyInterface->UpdateNodeTransforms(a_actor, false, female, node);
    }

    SKEE::INiTransformInterface* Scale::GetModernInterface() const
    {
        if (!transformInterface || transformInterface->GetVersion() < SKEE::INiTransformInterface::Version) {
            return nullptr;
        }
        return static_cast<SKEE::INiTransformInterface*>(transformInterface);
    }

    void Scale::SetHeelsOnly(RE::Actor* a_actor)
    {
        assert(a_actor);
        if (!::Settings::bPatchHeels) {
            return;
        }
        if (!GetModernInterface()) {
            return;
        }
        std::scoped_lock lock{ heelLock };
        auto& state = heelStates[a_actor->GetFormID()];
        state.absoluteScale = std::nullopt;
        state.sampling = true;
        state.stageTime = 0.0f;
        state.samples = 0;
        state.standingSamples = 0;
        state.anchors.clear();
        state.legDrops.clear();
        ApplyTransform(a_actor, state);
    }

    bool Scale::WearsHighHeels(RE::Actor* a_actor)
    {
        const auto modernInterface = GetModernInterface();
        if (!a_actor || !modernInterface) {
            return false;
        }
        const auto female = IsFemale(a_actor);
        if (modernInterface->HasNodeTransformPosition(a_actor, false, female, basenode, heelKey) &&
            modernInterface->GetNodeTransformPosition(a_actor, false, female, basenode, heelKey).z > ::Settings::fHeelMinHeight) {
            return true;
        }
        const auto npc = a_actor->GetNodeByName(basenode);
        return npc && !modernInterface->HasNodeTransformPosition(a_actor, false, female, basenode, namekey) &&
               npc->local.translate.z > ::Settings::fHeelMinHeight;
    }

    // SKEE composes every key of a node as a matrix product in unordered_map order, and its two update
    // paths even walk the keys in opposite directions. Whether the heel translation ends up scaled by our
    // scale therefore cannot be known in advance, so the key is solved against the node it produces.
    void Scale::ApplyTransform(RE::Actor* a_actor, HeelState& a_state)
    {
        const auto modernInterface = GetModernInterface();
        const auto female = IsFemale(a_actor);
        const auto npc = a_actor->GetNodeByName(basenode);
        a_state.cooldown = reapplyCooldown;

        SKEE::INiTransformInterface::Position position{ 0.0f, 0.0f, 0.0f };
        modernInterface->AddNodeTransformScaleMode(a_actor, false, female, basenode, namekey, ScaleModes::Multiplicative);
        modernInterface->AddNodeTransformPosition(a_actor, false, female, basenode, namekey, position);
        modernInterface->AddNodeTransformScale(a_actor, false, female, basenode, namekey, 1.0f);
        modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);

        if (!npc) {
            // No skeleton yet: plain scene scale, heels are solved once the 3D is there
            const float baseScale = a_actor->GetScale();
            float sceneMult = 1.0f;
            if (a_state.absoluteScale && std::abs(baseScale - *a_state.absoluteScale) >= scaleTolerance) {
                sceneMult = *a_state.absoluteScale / baseScale;
            }
            modernInterface->RemoveNodeTransformPosition(a_actor, false, female, basenode, namekey);
            modernInterface->AddNodeTransformScale(a_actor, false, female, basenode, namekey, sceneMult);
            SetLegScale(a_actor, female, a_state, 1.0f);
            a_state.applied = false;
            return;
        }

        const float preZ = npc->local.translate.z;
        const float preScale = npc->local.scale > 0.0f ? npc->local.scale : 1.0f;
        const float baseScale = a_actor->GetScale() * preScale;
        float sceneMult = 1.0f;
        if (a_state.absoluteScale && std::abs(baseScale - *a_state.absoluteScale) >= scaleTolerance) {
            sceneMult = *a_state.absoluteScale / baseScale;
        }
        const float sceneScale = preScale * sceneMult;  // "NPC" scale the animation expects on bare feet
        const float heel = preZ / preScale;             // heel lift in skeleton units
        a_state.heel = heel;

        if (!::Settings::bHeelCompensation || !std::isfinite(heel) || heel <= ::Settings::fHeelMinHeight) {
            modernInterface->RemoveNodeTransformPosition(a_actor, false, female, basenode, namekey);
            modernInterface->AddNodeTransformScale(a_actor, false, female, basenode, namekey, sceneMult);
            modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
            SetLegScale(a_actor, female, a_state, 1.0f);
            a_state.expectedZ = npc->local.translate.z;
            a_state.expectedScale = npc->local.scale;
            a_state.applied = true;
            logger::info("Applying Node Transform to Actor = {:X}, Scale = {} -> {}, x = {}", a_actor->GetFormID(), baseScale, baseScale * sceneMult, sceneMult);
            return;
        }

        // Standing: keep the heel and put the pelvis where it would be on bare feet. The body is shrunk a little
        // (OStim scaleHeight) and the thighs are scaled down for the rest, so the actor keeps most of their height.
        // The animation places the pelvis, the legs hang from it: shorter legs lift the feet, so the lift drops to match.
        // See HeelMath.h for the derivation; with the legs off this is s = pelvis / (pelvis + lift) as before.
        auto fit = HeelMath::StandingFit{ 1.0f, 1.0f, heel };
        if (a_state.anchor > 0.0f) {
            fit = HeelMath::SolveStanding(a_state.anchor, heel, a_state.legDrop, ::Settings::fHeelMinFactor, ::Settings::fHeelBodyMinFactor,
                ::Settings::fHeelMinLegScale, ::Settings::bHeelLegShortening && !a_state.legsFailed);
        }
        const float weight = std::clamp(a_state.weight, 0.0f, 1.0f);
        const float finalScale = sceneScale * (1.0f - weight + weight * fit.body);
        const float targetZ = weight * fit.lift * finalScale;
        SetLegScale(a_actor, female, a_state, 1.0f - weight * (1.0f - fit.legScale));
        modernInterface->AddNodeTransformScale(a_actor, false, female, basenode, namekey, finalScale / preScale);
        modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
        const float zeroZ = npc->local.translate.z;
        position.z = 1.0f;
        modernInterface->AddNodeTransformPosition(a_actor, false, female, basenode, namekey, position);
        modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
        float slope = npc->local.translate.z - zeroZ;
        if (!(slope > 0.2f && slope < 5.0f)) {
            logger::warn("Heels {:X}: unexpected transform response {}, assuming additive keys", a_actor->GetFormID(), slope);
            slope = 1.0f;
        }
        position.z = (targetZ - zeroZ) / slope;
        modernInterface->AddNodeTransformPosition(a_actor, false, female, basenode, namekey, position);
        modernInterface->UpdateNodeTransforms(a_actor, false, female, basenode);
        a_state.expectedZ = npc->local.translate.z;
        a_state.expectedScale = npc->local.scale;
        a_state.applied = true;
        if (weight == 0.0f || weight == 1.0f) {
            logger::info("Heels {:X}: lift {:.2f}, {}, anchor {:.1f}, scene scale {:.3f} -> {:.3f}, NPC z {:.2f} -> {:.2f} (key z {:.2f}), body {:.3f}, legs -{:.2f} (thigh x{:.3f}, leg drop {:.1f})",
                a_actor->GetFormID(), heel, weight == 1.0f ? "standing on heels" : "lift cancelled", a_state.anchor,
                baseScale * sceneMult, a_actor->GetScale() * npc->local.scale, preZ, npc->local.translate.z, position.z,
                weight * fit.body + (1.0f - weight), weight * (heel - fit.lift), a_state.legScale, a_state.legDrop);
        }
    }

    void Scale::SampleStance(RE::Actor* a_actor, RE::NiAVObject* a_npc, HeelState& a_state)
    {
        const auto inverse = a_npc->world.Invert();
        const auto pelvis = SkeletonHeight(a_actor, inverse, pelvisNode);
        const auto leftFoot = SkeletonHeight(a_actor, inverse, footNodes[0]);
        const auto rightFoot = SkeletonHeight(a_actor, inverse, footNodes[1]);
        const auto leftKnee = SkeletonHeight(a_actor, inverse, kneeNodes[0]);
        const auto rightKnee = SkeletonHeight(a_actor, inverse, kneeNodes[1]);
        if (!pelvis || !leftFoot || !rightFoot || !leftKnee || !rightKnee) {
            return;
        }
        // Weight is on the soles when the lowest ankle is near the floor and both the knees and the pelvis are clear of it.
        // Kneeling, lying, sitting on the floor, squatting and legs in the air all fail one of these.
        const float foot = std::min(*leftFoot, *rightFoot);
        const float knee = std::min(*leftKnee, *rightKnee);
        const bool standing = foot <= ::Settings::fHeelFootMaxHeight &&
                              knee - foot >= ::Settings::fHeelKneeClearance &&
                              *pelvis - foot >= ::Settings::fHeelPelvisClearance;
        a_state.samples++;
        if (standing) {
            a_state.standingSamples++;
            a_state.anchors.push_back(*pelvis);
            // Supporting leg = the lower foot. The thigh scale shrinks everything below the hip joint by the same
            // factor, so the drop measured now is divided by the scale applied to get the full-length drop.
            const size_t side = *leftFoot <= *rightFoot ? 0 : 1;
            const auto hip = SkeletonHeight(a_actor, inverse, thighNodes[side]);
            const float drop = hip ? (*hip - (side == 0 ? *leftFoot : *rightFoot)) / std::max(a_state.legScale, 0.1f) : 0.0f;
            if (std::isfinite(drop) && drop > 0.0f) {
                a_state.legDrops.push_back(drop);
            }
        }
    }

    void Scale::SetLegScale(RE::Actor* a_actor, bool a_female, HeelState& a_state, float a_legScale)
    {
        const auto modernInterface = GetModernInterface();
        if (!modernInterface) {
            return;
        }
        if (a_legScale >= 0.999f || !std::isfinite(a_legScale)) {
            if (a_state.legScale != 1.0f) {
                RemoveLegScale(a_actor, a_female);
            }
            a_state.legScale = 1.0f;
            a_state.expectedThighScale = 0.0f;
            return;
        }
        if (std::abs(a_legScale - a_state.legScale) < 0.0005f && a_state.expectedThighScale > 0.0f) {
            return;
        }
        // Thighs shorter, feet back to full size so the shoes and the heel keep their shape
        for (size_t i = 0; i < thighNodes.size(); i++) {
            modernInterface->AddNodeTransformScaleMode(a_actor, false, a_female, thighNodes[i], namekey, ScaleModes::Multiplicative);
            modernInterface->AddNodeTransformScale(a_actor, false, a_female, thighNodes[i], namekey, a_legScale);
            modernInterface->UpdateNodeTransforms(a_actor, false, a_female, thighNodes[i]);
            modernInterface->AddNodeTransformScaleMode(a_actor, false, a_female, footNodes[i], namekey, ScaleModes::Multiplicative);
            modernInterface->AddNodeTransformScale(a_actor, false, a_female, footNodes[i], namekey, 1.0f / a_legScale);
            modernInterface->UpdateNodeTransforms(a_actor, false, a_female, footNodes[i]);
        }
        a_state.legScale = a_legScale;
        const auto thigh = a_actor->GetNodeByName(thighNodes[0]);
        a_state.expectedThighScale = thigh ? thigh->local.scale : 0.0f;
    }

    bool Scale::RemoveLegScale(RE::Actor* a_actor, bool a_female)
    {
        const auto modernInterface = GetModernInterface();
        if (!modernInterface) {
            return false;
        }
        bool removed = false;
        for (const auto* nodes : { &thighNodes, &footNodes }) {
            for (const auto* node : *nodes) {
                if (modernInterface->RemoveNodeTransform(a_actor, false, a_female, node, namekey)) {
                    modernInterface->UpdateNodeTransforms(a_actor, false, a_female, node);
                    removed = true;
                }
            }
        }
        return removed;
    }

    void Scale::DecideStance(RE::Actor* a_actor, HeelState& a_state)
    {
        a_state.sampling = false;
        float anchor = 0.0f;
        bool standing = false;
        if (::Settings::bHeelStandingScale && a_state.samples >= 3 && a_state.standingSamples * 10 >= a_state.samples * 7) {
            auto& anchors = a_state.anchors;
            std::ranges::nth_element(anchors, anchors.begin() + anchors.size() / 2);
            anchor = anchors[anchors.size() / 2];
            standing = anchor >= ::Settings::fHeelMinAnchor;
        }
        if (a_state.heel > ::Settings::fHeelMinHeight) {
            logger::info("Heels {:X}: {} of {} pose samples stand on the feet, pelvis {:.1f} -> {}",
                a_actor->GetFormID(), a_state.standingSamples, a_state.samples, anchor, standing ? "keep heels, scale" : "cancel lift");
        }
        if (standing && std::abs(anchor - a_state.anchor) > 0.5f) {
            a_state.anchor = anchor;
            a_state.applied = false;
        }
        if (standing && !a_state.legDrops.empty()) {
            auto& drops = a_state.legDrops;
            std::ranges::nth_element(drops, drops.begin() + drops.size() / 2);
            const float drop = drops[drops.size() / 2];
            if (std::abs(drop - a_state.legDrop) > 0.5f) {
                a_state.legDrop = drop;
                a_state.applied = false;
            }
        }
        a_state.targetWeight = standing ? 1.0f : 0.0f;
    }

    void Scale::UpdateHeels(RE::Actor* a_actor, float a_delta, bool a_animationRunning)
    {
        if (!::Settings::bPatchHeels) {
            return;
        }
        if (!a_actor || !GetModernInterface()) {
            return;
        }
        std::scoped_lock lock{ heelLock };
        const auto where = heelStates.find(a_actor->GetFormID());
        if (where == heelStates.end()) {
            return;
        }
        auto& state = where->second;
        const auto npc = a_actor->GetNodeByName(basenode);
        if (!npc) {
            state.applied = false;
            return;
        }
        state.cooldown = std::max(0.0f, state.cooldown - a_delta);
        if (a_animationRunning && state.sampling && a_delta > 0.0f) {
            state.stageTime += a_delta;
            if (state.stageTime >= ::Settings::fHeelSampleDelay) {
                SampleStance(a_actor, npc, state);
            }
            if (state.stageTime >= ::Settings::fHeelSampleDelay + ::Settings::fHeelSampleWindow) {
                DecideStance(a_actor, state);
            }
        }

        bool dirty = !state.applied;
        if (state.weight != state.targetWeight) {
            if (a_delta > 0.0f) {
                const float step = ::Settings::fHeelBlendTime > 0.0f ? a_delta / ::Settings::fHeelBlendTime : 1.0f;
                state.weight = state.weight < state.targetWeight ? std::min(state.weight + step, state.targetWeight) : std::max(state.weight - step, state.targetWeight);
                dirty = true;
            }
        } else if (!dirty && state.cooldown <= 0.0f &&
                   (std::abs(npc->local.translate.z - state.expectedZ) > driftTranslation || std::abs(npc->local.scale - state.expectedScale) > driftScale)) {
            logger::info("Heels {:X}: NPC node changed outside the scene (z {:.2f} / {:.2f}, scale {:.3f} / {:.3f}), solving again",
                a_actor->GetFormID(), npc->local.translate.z, state.expectedZ, npc->local.scale, state.expectedScale);
            dirty = true;
        }
        // The animation must not undo the thigh scale; if it does, this actor falls back to the body-only fit
        if (!dirty && state.cooldown <= 0.0f && state.legScale < 1.0f && state.expectedThighScale > 0.0f) {
            const auto thigh = a_actor->GetNodeByName(thighNodes[0]);
            if (thigh && std::abs(thigh->local.scale - state.expectedThighScale) > legScaleHold) {
                logger::warn("Heels {:X}: leg shortening did not hold (thigh scale {:.3f} / {:.3f}), body-only fit from now on",
                    a_actor->GetFormID(), thigh->local.scale, state.expectedThighScale);
                state.legsFailed = true;
                dirty = true;
            }
        }
        if (dirty) {
            ApplyTransform(a_actor, state);
        }
    }

    void Scale::CleanupStaleHeels()
    {
        const auto modernInterface = GetModernInterface();
        {
            std::scoped_lock lock{ heelLock };
            heelStates.clear();
        }
        if (!modernInterface || !GameForms::AnimatingFaction) {
            return;
        }
        std::vector<RE::Actor*> actors{ RE::PlayerCharacter::GetSingleton() };
        if (const auto lists = RE::ProcessLists::GetSingleton()) {
            for (auto& handle : lists->highActorHandles) {
                if (const auto actor = handle.get().get()) {
                    actors.push_back(actor);
                }
            }
        }
        for (const auto actor : actors) {
            if (!actor || actor->IsInFaction(GameForms::AnimatingFaction)) {
                continue;
            }
            const auto female = IsFemale(actor);
            if (modernInterface->RemoveNodeTransformPosition(actor, false, female, basenode, namekey)) {
                logger::info("Heels {:X}: removed heel compensation left over from an unfinished scene", actor->GetFormID());
                modernInterface->UpdateNodeTransforms(actor, false, female, basenode);
            }
            if (RemoveLegScale(actor, female)) {
                logger::info("Heels {:X}: removed leg shortening left over from an unfinished scene", actor->GetFormID());
            }
        }
    }

}  // namespace Registry
