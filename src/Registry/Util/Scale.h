#pragma once

#include "API/IPluginInterface.h"
#include "Registry/Define/RaceKey.h"

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
        void UpdateHeels(RE::Actor* a_actor, float a_delta, bool a_animationRunning);
        bool WearsHighHeels(RE::Actor* a_actor);
        void CleanupStaleHeels();

        // Let SKEE rebuild the "NPC" node from the skeleton and the overrides it holds
        void RefreshBaseNode(RE::Actor* a_actor);

      private:
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
