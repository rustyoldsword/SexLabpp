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
