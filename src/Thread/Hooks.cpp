#include "Hooks.h"

#include "Thread.h"
#include "GameForms.h"
#include "PostSceneGuard.h"

#include <intrin.h>

namespace Thread::Hooks
{
    namespace
    {
        struct FrameUpdateHook
        {
            static void Install()
            {
                auto& trampoline = SKSE::GetTrampoline();
                const auto address = REL::VariantID(35565, 36564, 0x5BAB10).address();
                const auto offset = REL::VariantOffset(0x748, AE_OFFSET(0xC26, 0xC38), 0x7EE).offset();

                original = trampoline.write_call<5>(address + offset, Thunk);
                logger::info("Installed frame hook");
            }

            static void Thunk()
            {
                static REL::Relocation<float*> deltaTime{ REL::VariantID(523660, 410199, 0x30C3A08) };
                const auto frameDelta = *deltaTime;
                Instance::UpdateAnimations(frameDelta);
                original();
                const bool paused = Util::IsGamePausedOrFrozen();
                if (!paused) {
                    Thread::Interaction::NiSurface::Manager::OnFrameUpdate(frameDelta);
                }
                PostSceneGuard::Update(paused ? 0.0f : frameDelta, IsWeaponDrawBlocked());
            }

            static inline REL::Relocation<decltype(Thunk)> original;
        };

        // TESNPC::Activate is the base-object activation path used for mounting a horse.
        // Check the target reference, since NPC base records can have multiple instances.
        struct SceneHorseActivationHook
        {
            static void Install()
            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_TESNPC[0] };
                original = vtable.write_vfunc(0x37, Thunk);
                logger::info("Installed scene horse activation guard");
            }

            static bool Thunk(RE::TESNPC* a_base, RE::TESObjectREFR* a_targetRef,
                RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3,
                RE::TESBoundObject* a_object, std::int32_t a_targetCount)
            {
                const auto actor = a_targetRef ? a_targetRef->As<RE::Actor>() : nullptr;
                if (actor && actor->IsHorse() && GameForms::AnimatingFaction &&
                    actor->IsInFaction(GameForms::AnimatingFaction)) {
                    return false;
                }
                return original(a_base, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
            }

            static inline REL::Relocation<decltype(Thunk)> original;
        };

        struct DrawWeaponHook
        {
            static void Install()
            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_PlayerCharacter[0] };
                const auto slot = REL::Module::IsVR() ? 0xA8 : 0xA6;

                original = vtable.write_vfunc(slot, Thunk);
                logger::info("Installed player draw weapon hook");
            }

			// This is called when either a mod, or the player tries to unsheathe their weapon
            static void Thunk(RE::PlayerCharacter* a_player, bool a_draw)
            {
                const bool blocked = a_draw && blockDraw.load(std::memory_order_relaxed);
                PostSceneGuard::OnDrawRequest(a_draw, blocked, _ReturnAddress());
                if (!blocked) {
                    original(a_player, a_draw);
                }
            }

            static inline std::atomic_bool blockDraw{ false };
            static inline REL::Relocation<decltype(Thunk)> original;
        };
    }

    void Install()
    {
        FrameUpdateHook::Install();
        DrawWeaponHook::Install();
        SceneHorseActivationHook::Install();
    }

    void SetWeaponDrawBlocked(bool a_blocked) noexcept
    {
        DrawWeaponHook::blockDraw.store(a_blocked, std::memory_order_relaxed);
    }

    bool IsWeaponDrawBlocked() noexcept
    {
        return DrawWeaponHook::blockDraw.load(std::memory_order_relaxed);
    }
}
