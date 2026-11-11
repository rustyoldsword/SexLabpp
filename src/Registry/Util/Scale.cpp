#include "Scale.h"

#include "HeelMath.h"
#include "LegDebug.h"
#include "Thread/AdultHeightPolicy.h"
#include "Util/Script.h"

namespace Registry
{
    namespace
    {
        constexpr const char* heelKey{ "internal" };
        constexpr const char* pelvisNode{ "NPC Pelvis [Pelv]" };
        constexpr std::array footNodes{ "NPC L Foot [Lft ]", "NPC R Foot [Rft ]" };
        constexpr std::array kneeNodes{ "NPC L Calf [LClf]", "NPC R Calf [RClf]" };
        constexpr std::array thighNodes{ "NPC L Thigh [LThg]", "NPC R Thigh [RThg]" };
        constexpr const char* headNode{ "NPC Head [Head]" };
        // XPMSSE nodes the animation never writes: "CME Thigh" carries the calf, "CME Calf" -> "CME Knee" carries the foot
        constexpr std::array cmeThighNodes{ "CME L Thigh [LThg]", "CME R Thigh [RThg]" };
        constexpr std::array cmeCalfNodes{ "CME L Calf [LClf]", "CME R Calf [RClf]" };
        constexpr std::array cmeKneeNodes{ "CME L Knee [LKne]", "CME R Knee [RKne]" };

        constexpr uint32_t Rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) { return (a << 24) | (b << 16) | (g << 8) | r; }

        // Corrective body morphs by knee flexion, through NiOverride's Papyrus API (independent of the SKEE build).
        // UBE and 3BA sliders are written together: a body loads only the sliders its TRI has.
        constexpr auto kneeMorphKey{ "SexLabKneeFix" };

        template <class... Args>
        void CallNiOverride(const char* a_function, Args... a_args)  // by value: the VM argument pack wants plain types
        {
            Script::CallbackPtr none{};
            auto args = RE::MakeFunctionArguments(std::move(a_args)...);
            if (!Script::DispatchStaticCall("NiOverride", a_function, none, args)) {
                logger::warn("Knee morph: NiOverride.{} could not be called", a_function);
            }
        }

        void SetKneeMorphs(RE::Actor* a_actor, float a_gain, float a_deep, const LegDebug::Params& a_params)
        {
            const std::array<std::pair<const char*, float>, 8> sliders{ {
                { "CalfUpperSize p|n", a_params.ubeCalfUpper * a_gain + a_params.ubeDeepCalfUpper * a_deep },
                { "LowerThighsSizeBig", a_params.ubeThighLower * a_gain },
                { "ThighBackDepth", a_params.ubeThighBack * a_gain },
                { "LowerOuterThighSize Bigger", a_params.ubeThighOuter * a_gain },
                { "CalfSize", a_params.cbbeCalfSize * a_gain + a_params.cbbeDeepCalfSize * a_deep },
                { "CalfSmooth", a_params.cbbeCalfSmooth * a_gain },
                { "CalfFBThicc_v2", a_params.cbbeCalfDepth * a_gain },
                { "ChubbyLegs", a_params.cbbeLegsFuller * a_gain },
            } };
            const auto refr = static_cast<RE::TESObjectREFR*>(a_actor);
            for (const auto& [name, amount] : sliders) {
                const float value = amount;
                if (value != 0.0f) {
                    CallNiOverride("SetBodyMorph", refr, RE::BSFixedString(name), RE::BSFixedString(kneeMorphKey), value);
                } else {
                    CallNiOverride("ClearBodyMorph", refr, RE::BSFixedString(name), RE::BSFixedString(kneeMorphKey));
                }
            }
            CallNiOverride("UpdateModelWeight", refr);
        }

        void ClearKneeMorphs(RE::Actor* a_actor)
        {
            const auto refr = static_cast<RE::TESObjectREFR*>(a_actor);
            CallNiOverride("ClearBodyMorphKeys", refr, RE::BSFixedString(kneeMorphKey));
            CallNiOverride("UpdateModelWeight", refr);
        }

        float Median(std::vector<float>& a_values)
        {
            std::ranges::nth_element(a_values, a_values.begin() + a_values.size() / 2);
            return a_values[a_values.size() / 2];
        }
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
            ResetSampling(state);
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
        {
            // Knee morphs go with the scene. Without a state (a save loaded mid-scene) they are cleared blindly.
            bool morphed = LegDebug::Get().morph;
            {
                std::scoped_lock lock{ heelLock };
                if (const auto where = heelStates.find(a_actor->GetFormID()); where != heelStates.end()) {
                    morphed = where->second.kneeMorph.applied > 0.0f;
                    where->second.kneeMorph = {};
                }
            }
            if (morphed) {
                ClearKneeMorphs(a_actor);
                logger::info("Knee morph {:X}: cleared at scene end", a_actor->GetFormID());
            }
        }
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
                if (const auto where = heelStates.find(a_actor->GetFormID()); where != heelStates.end()) {
                    RestoreLegs(where->second.legs);
                    heelStates.erase(where);
                }
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
            const auto where = heelStates.find(a_actor->GetFormID());
            if (where == heelStates.end()) {
                return;
            }
            RestoreLegs(where->second.legs);
            heelStates.erase(where);
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
        ResetSampling(state);
        ApplyTransform(a_actor, state);
    }

    void Scale::ResetSampling(HeelState& a_state)
    {
        a_state.sampling = true;
        a_state.stageTime = 0.0f;
        a_state.samples = 0;
        a_state.standingSamples = 0;
        a_state.anchors.clear();
        a_state.legDrops.clear();
        a_state.heads.clear();
        a_state.thighVerticals.clear();
        a_state.shinVerticals.clear();
    }

    void Scale::SetStageFocus(RE::Actor* a_actor, float a_pelvisWeight, float a_headWeight)
    {
        if (!a_actor || !::Settings::bPatchHeels || !GetModernInterface()) {
            return;
        }
        std::scoped_lock lock{ heelLock };
        auto& state = heelStates[a_actor->GetFormID()];
        if (state.pelvisWeight != a_pelvisWeight || state.headWeight != a_headWeight) {
            state.pelvisWeight = a_pelvisWeight;
            state.headWeight = a_headWeight;
            state.applied = false;
        }
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
            a_state.kneeShift = 0.0f;
            a_state.ankleShift = 0.0f;
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
            a_state.kneeShift = 0.0f;
            a_state.ankleShift = 0.0f;
            a_state.expectedZ = npc->local.translate.z;
            a_state.expectedScale = npc->local.scale;
            a_state.applied = true;
            logger::info("Applying Node Transform to Actor = {:X}, Scale = {} -> {}, x = {}", a_actor->GetFormID(), baseScale, baseScale * sceneMult, sceneMult);
            return;
        }

        // Standing: keep the heel, soles on the floor, and share the lift between a slightly smaller body, slightly
        // slimmer legs and shorter thigh and shin joints, each in proportion to how little it shows (HeelMath.h).
        // The stage focus decides whether the pelvis (sex) or the head (kissing) has to match the bare-foot animation.
        HeelMath::StandingFit fit{};
        fit.lift = heel;
        if (a_state.anchor > 0.0f) {
            HeelMath::StandingInput in;
            in.heel = heel;
            in.pelvis = { a_state.anchor, a_state.pelvisWeight };
            in.head = { a_state.head, a_state.headWeight };
            in.legDrop = a_state.legDrop;
            in.thighVertical = a_state.thighVertical;
            in.shinVertical = a_state.shinVertical;
            in.tolBody = ::Settings::fHeelTolBody;
            in.tolLegScale = ::Settings::fHeelTolLegScale;
            in.tolKnee = ::Settings::fHeelTolKnee;
            in.tolAnkle = ::Settings::fHeelTolAnkle;
            in.tolAnchor = ::Settings::fHeelTolAnchor;
            in.kneeBlend = ::Settings::fHeelKneeBlend;
            in.ankleBlend = ::Settings::fHeelAnkleBlend;
            in.bodyMin = ::Settings::fHeelMinFactor;
            in.legScaleMin = ::Settings::fHeelMinLegScale;
            in.kneeMax = ::Settings::fHeelMaxKneeShift;
            in.ankleMax = ::Settings::fHeelMaxAnkleShift;
            in.legScale = ::Settings::bHeelLegShortening && !a_state.legsFailed;
            in.joints = ::Settings::bHeelLegJoints && !a_state.legs.failed;
            fit = HeelMath::SolveStanding(in);
        }
        const float weight = std::clamp(a_state.weight, 0.0f, 1.0f);
        const float finalScale = sceneScale * (1.0f - weight + weight * fit.body);
        const float targetZ = weight * fit.lift * finalScale;
        SetLegScale(a_actor, female, a_state, 1.0f - weight * (1.0f - fit.legScale));
        a_state.kneeShift = weight * fit.kneeShift;
        a_state.ankleShift = weight * fit.ankleShift;
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
            if (weight == 1.0f && a_state.anchor > 0.0f) {
                logger::info("Heels {:X}: fit focus pelvis {:.2f} / head {:.2f}, knee shift {:.2f}, ankle shift {:.2f}, pelvis {:+.2f}, head {:+.2f} (head {:.1f}, legs vertical {:.2f}/{:.2f}{})",
                    a_actor->GetFormID(), a_state.pelvisWeight, a_state.headWeight, fit.kneeShift, fit.ankleShift, fit.pelvisError, fit.headError,
                    a_state.head, a_state.thighVertical, a_state.shinVertical, a_state.legs.failed ? ", no CME joints" : "");
            }
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
        if (!standing) {
            return;
        }
        a_state.standingSamples++;
        a_state.anchors.push_back(*pelvis);
        if (const auto head = SkeletonHeight(a_actor, inverse, headNode); head && *head > *pelvis) {
            a_state.heads.push_back(*head);
        }
        // Supporting leg = the lower foot. Its full-length drop is what is measured now with our own changes
        // taken back out: the thigh scale k shrinks everything below the hip, the joint shifts shorten the bones
        // along their axes, so measured = k * drop - kneeShift * thighVertical - ankleShift * shinVertical.
        const size_t side = *leftFoot <= *rightFoot ? 0 : 1;
        const auto thighNode = a_actor->GetNodeByName(thighNodes[side]);
        const auto calfNode = a_actor->GetNodeByName(kneeNodes[side]);
        const auto footNode = a_actor->GetNodeByName(footNodes[side]);
        if (!thighNode || !calfNode || !footNode) {
            return;
        }
        const auto hip = inverse * thighNode->world.translate;
        const auto kneeJoint = inverse * calfNode->world.translate;
        const auto ankle = inverse * footNode->world.translate;
        const auto vertical = [](const RE::NiPoint3& a_from, const RE::NiPoint3& a_to) {
            const auto bone = a_to - a_from;
            const float length = bone.Length();
            return length > 1e-3f ? std::clamp((a_from.z - a_to.z) / length, 0.0f, 1.0f) : 1.0f;
        };
        const float thighVertical = vertical(hip, kneeJoint);
        const float shinVertical = vertical(kneeJoint, ankle);
        const auto& legSide = a_state.legs.sides[side];
        const float shifts = a_state.legs.ready ? legSide.kneeApplied * thighVertical + legSide.ankleApplied * shinVertical : 0.0f;
        const float drop = (hip.z - ankle.z + shifts) / std::max(a_state.legScale, 0.1f);
        if (std::isfinite(drop) && drop > 0.0f) {
            a_state.legDrops.push_back(drop);
            a_state.thighVerticals.push_back(thighVertical);
            a_state.shinVerticals.push_back(shinVertical);
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
            anchor = Median(a_state.anchors);
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
            const float drop = Median(a_state.legDrops);
            const float thighVertical = Median(a_state.thighVerticals);
            const float shinVertical = Median(a_state.shinVerticals);
            if (std::abs(drop - a_state.legDrop) > 0.5f || std::abs(thighVertical - a_state.thighVertical) > 0.02f ||
                std::abs(shinVertical - a_state.shinVertical) > 0.02f) {
                a_state.legDrop = drop;
                a_state.thighVertical = thighVertical;
                a_state.shinVertical = shinVertical;
                a_state.applied = false;
            }
        }
        if (standing && !a_state.heads.empty()) {
            const float head = Median(a_state.heads);
            if (std::abs(head - a_state.head) > 0.5f) {
                a_state.head = head;
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
        UpdateLegs(a_actor, state, a_delta);
    }

    bool Scale::CaptureLegs(RE::Actor* a_actor, LegRig& a_rig)
    {
        RestoreLegs(a_rig);
        a_rig.root.reset(a_actor->Get3D(false));
        for (size_t i = 0; i < a_rig.sides.size(); i++) {
            auto& side = a_rig.sides[i];
            const auto thigh = a_actor->GetNodeByName(thighNodes[i]);
            const auto calf = a_actor->GetNodeByName(kneeNodes[i]);
            const auto foot = a_actor->GetNodeByName(footNodes[i]);
            const auto cmeThigh = a_actor->GetNodeByName(cmeThighNodes[i]);
            const auto cmeCalf = a_actor->GetNodeByName(cmeCalfNodes[i]);
            const auto cmeKnee = a_actor->GetNodeByName(cmeKneeNodes[i]);
            if (!thigh || !calf || !foot || !cmeThigh || !cmeCalf || !cmeKnee || !cmeThigh->AsNode() ||
                cmeThigh->parent != thigh->AsNode() || calf->parent != cmeThigh->AsNode() ||
                cmeKnee->parent != cmeCalf->AsNode() || foot->parent != cmeKnee->AsNode()) {
                a_rig.failed = true;
                return false;
            }
            auto thighBone = calf->world.translate - thigh->world.translate;
            auto shinBone = foot->world.translate - calf->world.translate;
            const float thighLength = thighBone.Length();
            const float shinLength = shinBone.Length();
            if (!(thighLength > 1.0f) || !(shinLength > 1.0f)) {
                return false;  // 3D not placed yet, try again next frame
            }
            side.thigh.reset(thigh);
            side.calf.reset(calf);
            side.cmeThigh.reset(cmeThigh);
            side.cmeCalf.reset(cmeCalf);
            // Bone directions are fixed in the parent's frame; kept there so our own shifts never feed back into them
            side.thighAxis = thigh->world.rotate.Transpose() * (thighBone * (1.0f / thighLength));
            side.shinAxis = calf->world.rotate.Transpose() * (shinBone * (1.0f / shinLength));
            side.knee = LegJoint{ RE::NiPointer<RE::NiAVObject>(cmeThigh), cmeThigh->local.translate, cmeThigh->local.translate, 1.0f, false };
            side.ankle = LegJoint{ RE::NiPointer<RE::NiAVObject>(cmeKnee), cmeKnee->local.translate, cmeKnee->local.translate, 1.0f, false };
            side.helpers.clear();
            for (const auto& child : cmeThigh->AsNode()->GetChildren()) {
                if (!child || child.get() == calf) {
                    continue;
                }
                // A flesh helper halfway down the thigh follows half of the knee shift: the thigh is compressed evenly
                const float along = std::clamp((child->world.translate - thigh->world.translate).Dot(thighBone) / (thighLength * thighLength), 0.0f, 1.0f);
                side.helpers.push_back(LegJoint{ child, child->local.translate, child->local.translate, along, false });
            }
            side.push = {};
            side.kneeApplied = 0.0f;
            side.ankleApplied = 0.0f;
        }
        a_rig.ready = true;
        return true;
    }

    void Scale::RestoreLegs(LegRig& a_rig)
    {
        const auto restore = [](LegJoint& a_joint) {
            // Only if the translation is still ours; a node someone else rewrote is left to them
            if (a_joint.touched && a_joint.node && (a_joint.node->local.translate - a_joint.written).Length() < 1e-3f) {
                a_joint.node->local.translate = a_joint.base;
            }
            a_joint.touched = false;
        };
        for (auto& side : a_rig.sides) {
            restore(side.knee);
            restore(side.ankle);
            for (auto& helper : side.helpers) {
                restore(helper);
            }
        }
        const bool failed = a_rig.failed;
        a_rig = {};
        a_rig.failed = failed;
    }

    // Every frame of the scene: knee volume (all actors) and the joint shortening of a standing actor on heels.
    // Offsets are built in world space and written into the parent's frame, so they are exact whatever the
    // scene scale, the thigh scale or the pose.
    void Scale::UpdateLegs(RE::Actor* a_actor, HeelState& a_state, float a_delta)
    {
        auto& rig = a_state.legs;
        const auto knee = LegDebug::Get();
        const bool wanted = knee.kneeFix || knee.draw || knee.morph || a_state.kneeShift > 0.0f || a_state.ankleShift > 0.0f;
        if (!knee.morph && a_state.kneeMorph.applied > 0.0f) {
            ClearKneeMorphs(a_actor);  // switched off live
            a_state.kneeMorph = {};
            logger::info("Knee morph {:X}: switched off, cleared", a_actor->GetFormID());
        }
        if (!wanted) {
            if (rig.ready) {
                RestoreLegs(rig);
                LegDebug::Clear(a_actor->GetFormID());
            }
            return;
        }
        if (rig.failed) {
            return;
        }
        const auto root = a_actor->Get3D(false);
        if (!root) {
            return;
        }
        if (!rig.ready || rig.root.get() != root) {
            if (!CaptureLegs(a_actor, rig)) {
                if (rig.failed) {
                    logger::info("Legs {:X}: no XPMSSE CME leg joints, knee fix and joint shortening off for this actor", a_actor->GetFormID());
                    a_state.applied = false;  // solve the heels again without the joints
                }
                return;
            }
            logger::info("Legs {:X}: CME leg joints found, thigh helpers {} / {}", a_actor->GetFormID(), rig.sides[0].helpers.size(), rig.sides[1].helpers.size());
        }
        const auto npc = a_actor->GetNodeByName(basenode);
        if (!npc) {
            return;
        }
        const float unit = npc->world.scale;
        const auto up = npc->world.rotate * RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
        const auto inverse = npc->world.Invert();
        const float alpha = a_delta > 0.0f ? 1.0f - std::exp(-a_delta / std::max(knee.smoothing, 1e-3f)) : 0.0f;
        const bool dump = LegDebug::DumpDue(a_actor->GetFormID(), a_delta);
        std::vector<LegDebug::Item> items{};

        const auto write = [](LegJoint& a_joint, RE::NiAVObject* a_parent, const RE::NiPoint3& a_worldOffset) {
            const auto node = a_joint.node.get();
            if (!node || !a_parent) {
                return;
            }
            if (a_joint.touched && (node->local.translate - a_joint.written).Length() > 1e-3f) {
                a_joint.base = node->local.translate;  // rewritten by someone else (SKEE rebuild): that is the new base
            }
            if (!a_joint.touched && a_worldOffset.Length() < 1e-5f) {
                return;
            }
            const float parentScale = a_parent->world.scale > 1e-6f ? a_parent->world.scale : 1.0f;
            const auto value = a_joint.base + a_parent->world.rotate.Transpose() * (a_worldOffset * (1.0f / parentScale));
            node->local.translate = value;
            a_joint.written = value;
            a_joint.touched = true;
        };

        float deepest = 0.0f;
        for (size_t i = 0; i < rig.sides.size(); i++) {
            auto& side = rig.sides[i];
            const auto thighDir = side.thigh->world.rotate * side.thighAxis;
            const auto shinDir = side.calf->world.rotate * side.shinAxis;
            const auto hip = side.thigh->world.translate;
            const auto kneeJoint = side.calf->world.translate;
            const float kneeHeight = (inverse * kneeJoint).z;
            const bool onFloor = kneeHeight < knee.floorHeight;
            const float flexion = HeelMath::KneeFlexion({ thighDir.x, thighDir.y, thighDir.z }, { shinDir.x, shinDir.y, shinDir.z });
            deepest = std::max(deepest, flexion * 57.29578f);
            RE::NiPoint3 target{};
            if (knee.kneeFix) {
                const auto offset = HeelMath::KneeOffset({ thighDir.x, thighDir.y, thighDir.z }, { shinDir.x, shinDir.y, shinDir.z }, { up.x, up.y, up.z },
                    knee.front, knee.along, knee.max, onFloor);
                target = { offset.x, offset.y, offset.z };
            }
            side.push += (target - side.push) * alpha;

            if (knee.draw || dump) {
                // How the engine composes a child: compare the node it computed last frame with both conventions
                const auto cme = side.cmeThigh.get();
                const auto& parent = side.thigh->world;
                const auto local = cme->local.translate * parent.scale;
                const float errDirect = (cme->world.translate - (parent.translate + parent.rotate * local)).Length();
                const float errTransposed = (cme->world.translate - (parent.translate + parent.rotate.Transpose() * local)).Length();
                const auto applied = parent.rotate * ((side.knee.written - side.knee.base) * parent.scale);  // what we put in, world
                const auto footNode = a_actor->GetNodeByName(footNodes[i]);
                const auto ankle = footNode ? footNode->world.translate : kneeJoint + shinDir * (25.0f * unit);
                auto front = thighDir - shinDir;
                front.Unitize();
                const char* name = i == 0 ? "L" : "R";
                const auto text = std::format("{} {:.0f} deg h{:.1f}{} off {:+.2f}/{:+.2f}/{:+.2f} |{:.2f}| conv {:.2f}/{:.2f}", name,
                    flexion * 57.29578f, kneeHeight, onFloor ? " floor" : "", side.push.x, side.push.y, side.push.z, side.push.Length(), errDirect, errTransposed);
                if (knee.draw) {
                    items.push_back({ hip, kneeJoint, Rgba(60, 230, 60, 255) });
                    items.push_back({ kneeJoint, ankle, Rgba(80, 160, 255, 255) });
                    items.push_back({ kneeJoint, kneeJoint + front * (4.0f * unit), Rgba(255, 255, 255, 200) });
                    items.push_back({ kneeJoint, kneeJoint + side.push * (3.0f * unit), Rgba(255, 220, 0, 255) });
                    items.push_back({ kneeJoint - applied, kneeJoint, Rgba(255, 60, 60, 255) });
                    items.push_back({ kneeJoint, {}, Rgba(255, 255, 255, 255), text });
                }
                if (dump) {
                    logger::info("Knee debug {:X} {}: flexion {:.1f}, knee height {:.1f}{}, target {:+.2f} {:+.2f} {:+.2f}, smoothed |{:.2f}|, CME base {:.2f} {:.2f} {:.2f} local {:.2f} {:.2f} {:.2f}, applied world {:+.2f} {:+.2f} {:+.2f}, convention error direct {:.3f} transposed {:.3f}, knee shift {:.2f} ankle shift {:.2f}",
                        a_actor->GetFormID(), name, flexion * 57.29578f, kneeHeight, onFloor ? " (floor)" : "", target.x, target.y, target.z, side.push.Length(),
                        side.knee.base.x, side.knee.base.y, side.knee.base.z, cme->local.translate.x, cme->local.translate.y, cme->local.translate.z,
                        applied.x, applied.y, applied.z, errDirect, errTransposed, a_state.kneeShift, a_state.ankleShift);
                }
            }

            const auto kneeOffset = (side.push - thighDir * a_state.kneeShift) * unit;
            const auto ankleOffset = shinDir * (-a_state.ankleShift * unit);
            write(side.knee, side.thigh.get(), kneeOffset);
            for (auto& helper : side.helpers) {
                write(helper, side.cmeThigh.get(), kneeOffset * (helper.along - 1.0f));
            }
            write(side.ankle, side.cmeCalf.get(), ankleOffset);
            side.kneeApplied = a_state.kneeShift;
            side.ankleApplied = a_state.ankleShift;
        }
        if (knee.draw) {
            LegDebug::Submit(a_actor->GetFormID(), std::move(items));
        }
        if (dump && knee.poseDump) {
            // Contact points for offline role analysis (which position fits a petite actor)
            constexpr std::array poseNodes{ "NPC Head [Head]", "NPC Pelvis [Pelv]", "NPC Spine2 [Spn2]", "NPC L Hand [LHnd]",
                "NPC R Hand [RHnd]", "NPC L Calf [LClf]", "NPC R Calf [RClf]", "NPC L Foot [Lft ]", "NPC R Foot [Rft ]", "VaginaB1" };
            std::string line = std::format("Pose dump {:X} scale {:.3f}:", a_actor->GetFormID(), npc->world.scale);
            for (const auto* name : poseNodes) {
                if (const auto node = a_actor->GetNodeByName(name)) {
                    const auto& p = node->world.translate;
                    line += std::format(" {}={:.1f},{:.1f},{:.1f}", name, p.x, p.y, p.z);
                }
            }
            logger::info("{}", line);
        }
        if (knee.morph) {
            HeelMath::KneeMorphParams params{};
            params.start = knee.morphStart;
            params.full = knee.morphFull;
            params.hold = knee.morphHold;
            params.deepStart = knee.deepStart;
            params.deepFull = knee.deepFull;
            auto& tracker = a_state.kneeMorph;
            const float before = tracker.applied;
            const float amounts = knee.ubeCalfUpper * 1.0f + knee.ubeThighLower * 3.0f + knee.ubeThighBack * 7.0f +
                                  knee.cbbeCalfSize * 13.0f + knee.cbbeCalfSmooth * 29.0f + knee.ubeDeepCalfUpper * 31.0f + knee.cbbeDeepCalfSize * 37.0f +
                                  knee.ubeThighOuter * 41.0f + knee.cbbeCalfDepth * 43.0f + knee.cbbeLegsFuller * 47.0f;
            const bool retuned = tracker.applied > 0.0f && amounts != a_state.kneeMorphAmounts;  // INI edited live
            a_state.kneeMorphAmounts = amounts;
            if (HeelMath::KneeMorphStep(tracker, deepest, a_delta, params) || retuned) {
                if (tracker.applied > 0.0f) {
                    SetKneeMorphs(a_actor, tracker.applied, tracker.appliedDeep, knee);
                } else {
                    ClearKneeMorphs(a_actor);
                }
                logger::info("Knee morph {:X}: deepest flexion {:.0f} deg, gain {:.2f} -> {:.2f}, deep {:.2f} (UBE calf upper {:+.2f}, 3BA calf size {:+.2f} smooth {:+.2f})",
                    a_actor->GetFormID(), tracker.peak, before, tracker.applied, tracker.appliedDeep,
                    knee.ubeCalfUpper * tracker.applied + knee.ubeDeepCalfUpper * tracker.appliedDeep,
                    knee.cbbeCalfSize * tracker.applied + knee.cbbeDeepCalfSize * tracker.appliedDeep, knee.cbbeCalfSmooth * tracker.applied);
            }
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
