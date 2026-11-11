#pragma once

#include "API/IPluginInterface.h"
#include "Registry/Define/RaceKey.h"
#include "Registry/Util/HeelMath.h"

namespace Registry
{
    class Scale :
      public Singleton<Scale>
    {
        static constexpr const char* namekey{ "SexLabRegistry" };
        static constexpr const char* basenode{ "NPC" };
        static constexpr const char* extranode{ "NPC Root [Root]" };

        enum ScaleModes
        {
            Multiplicative = 0,
            Averaged = 1,
            Additive = 2,
            Maximum = 3,
        };

      public:
        float GetScale(RE::TESObjectREFR* a_reference);
        // GetScale divided by the race height for the actor's sex (petite checks)
        float GetRaceRelativeHeight(RE::Actor* a_actor);
        void SetScale(RE::Actor* a_reference, float a_absolutescale);
        void SetScale(RE::Actor* a_reference, RaceKey a_racekey, float a_absolutescale);
        void RemoveScale(RE::Actor* a_reference);

        // High heel compensation. The heel lift is the translation RaceMenu (key "internal") and any
        // other heel system put on the "NPC" node. Scenes either cancel it, or, when the actor stands
        // on their feet, keep it and shrink the actor so the pelvis stays where the animation wants it.
        void SetHeelsOnly(RE::Actor* a_actor);
        // What the coming stage needs to line up while standing: the pelvis (sex), the head (kissing) or both
        void SetStageFocus(RE::Actor* a_actor, float a_pelvisWeight, float a_headWeight);
        void UpdateHeels(RE::Actor* a_actor, float a_delta, bool a_animationRunning);
        bool WearsHighHeels(RE::Actor* a_actor);
        void CleanupStaleHeels();

        // Let SKEE rebuild the "NPC" node from the skeleton and the overrides it holds
        void RefreshBaseNode(RE::Actor* a_actor);

      private:
        // A translation we add to a node the animation never writes (XPMSSE "CME" nodes)
        struct LegJoint
        {
            RE::NiPointer<RE::NiAVObject> node{};
            RE::NiPoint3 base{};     // the node's own translation, without ours
            RE::NiPoint3 written{};  // what we wrote last, to see whether someone else wrote since
            float along{ 1.0f };     // thigh helpers: place along the thigh, 0 = hip, 1 = knee
            bool touched{ false };
        };

        struct LegSide
        {
            RE::NiPointer<RE::NiAVObject> thigh{};
            RE::NiPointer<RE::NiAVObject> calf{};
            RE::NiPointer<RE::NiAVObject> cmeThigh{};
            RE::NiPointer<RE::NiAVObject> cmeCalf{};
            LegJoint knee{};                  // "CME Thigh": carries the calf, so moving it moves the knee joint
            LegJoint ankle{};                 // "CME Knee": carries the foot
            std::vector<LegJoint> helpers{};  // the other children of "CME Thigh" (thigh flesh helpers)
            RE::NiPoint3 thighAxis{};         // hip -> knee in the thigh's local frame
            RE::NiPoint3 shinAxis{};          // knee -> ankle in the calf's local frame
            RE::NiPoint3 push{};              // smoothed knee push, skeleton units, world axes
            float kneeApplied{ 0.0f };        // shifts in effect, skeleton units
            float ankleApplied{ 0.0f };
        };

        struct LegRig
        {
            RE::NiPointer<RE::NiAVObject> root{};
            std::array<LegSide, 2> sides{};
            bool ready{ false };
            bool failed{ false };
        };

        struct HeelState
        {
            std::optional<float> absoluteScale{};  // scene scale requested by the animation, none = keep the actor's own
            float weight{ 0.0f };                   // 0 = heel lift cancelled, 1 = standing on heels with scale compensation
            float targetWeight{ 0.0f };
            float anchor{ 0.0f };                   // pelvis height in unscaled skeleton units while standing
            float expectedZ{ 0.0f };
            float expectedScale{ 1.0f };
            float heel{ 0.0f };
            float stageTime{ 0.0f };
            float cooldown{ 0.0f };
            uint32_t samples{ 0 };
            uint32_t standingSamples{ 0 };
            std::vector<float> anchors{};
            float legDrop{ 0.0f };                  // hip-to-foot drop of the supporting leg at full length, skeleton units
            float legScale{ 1.0f };                 // thigh scale currently applied (feet get the inverse)
            float expectedThighScale{ 0.0f };       // thigh local scale right after we set it, to see whether it holds
            std::vector<float> legDrops{};
            bool legsFailed{ false };               // the leg scale did not hold on this actor: body-only fit
            bool sampling{ false };
            bool applied{ false };
            bool standingKnown{ false };
            float head{ 0.0f };                     // head height while standing, skeleton units
            float thighVertical{ 1.0f };            // vertical share of thigh and shin of the supporting leg
            float shinVertical{ 1.0f };
            std::vector<float> heads{};
            std::vector<float> thighVerticals{};
            std::vector<float> shinVerticals{};
            float pelvisWeight{ 1.0f };             // what this stage lines up (SetStageFocus)
            float headWeight{ 0.2f };
            float kneeShift{ 0.0f };                // joint shifts asked for now, weight included, skeleton units
            float ankleShift{ 0.0f };
            LegRig legs{};
            HeelMath::KneeMorphTracker kneeMorph{};  // corrective body morphs by knee flexion
            float kneeMorphAmounts{ 0.0f };          // signature of the slider amounts last written
        };

        SKEE::INiTransformInterface* GetModernInterface() const;
        void DropHeelState(RE::Actor* a_actor);
        void SetScaleStock(RE::Actor* a_actor, RaceKey a_racekey, float a_absolutescale);
        void RemoveScaleStock(RE::Actor* a_actor);
        void ApplyTransform(RE::Actor* a_actor, HeelState& a_state);
        void SampleStance(RE::Actor* a_actor, RE::NiAVObject* a_npc, HeelState& a_state);
        void DecideStance(RE::Actor* a_actor, HeelState& a_state);
        void SetLegScale(RE::Actor* a_actor, bool a_female, HeelState& a_state, float a_legScale);
        bool RemoveLegScale(RE::Actor* a_actor, bool a_female);
        bool CaptureLegs(RE::Actor* a_actor, LegRig& a_rig);
        void UpdateLegs(RE::Actor* a_actor, HeelState& a_state, float a_delta);
        static void RestoreLegs(LegRig& a_rig);
        void ResetSampling(HeelState& a_state);

        struct ScaleNodeVisitor : public SKEE::INiTransformInterface::NodeVisitor
        {
            virtual bool VisitPosition(const char* node, const char* key, SKEE::INiTransformInterface::Position& position) override;
            virtual bool VisitRotation(const char* node, const char* key, SKEE::INiTransformInterface::Rotation& rotation) override;
            virtual bool VisitScale(const char* node, const char* key, float scale) override;
            virtual bool VisitScaleMode(const char* node, const char* key, uint32_t scaleMode) override;
        };

        SKEE::IPluginInterface* transformInterface = []() {
            const auto intfc = SKEE::GetInterfaceMap();
            return intfc ? intfc->QueryInterface("NiTransform") : nullptr;
        }();

        std::recursive_mutex heelLock{};
        std::unordered_map<RE::FormID, HeelState> heelStates{};
    };

}  // namespace Registry
