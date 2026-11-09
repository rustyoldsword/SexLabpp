#include "PostSceneGuard.h"

#include "Registry/Define/RaceKey.h"
#include "Registry/Util/Scale.h"

#include <intrin.h>

namespace Thread::PostSceneGuard
{
	namespace
	{
		constexpr float kFactionWait{ 15.0f };           // longest wait for an actor to leave the animating faction
		constexpr float kStanceWatch{ 10.0f };           // the stuck draw was seen settling ~3 s after the scene
		constexpr float kStanceStuck{ 1.5f };            // a normal draw has the graph ready well within 0.5 s
		constexpr float kResyncSheatheDelay{ 1.0f };     // graph drawn -> sheathe the normal way
		constexpr float kDriftConfirm{ 0.25f };          // in-scene drift must hold this long before it is reverted
		constexpr int kMaxRequestLogs{ 8 };              // per blocked period / watch window
		constexpr std::array kTiltChecks{ 0.5f, 2.0f, 5.0f };
		constexpr float kTiltThreshold{ 1.0f };          // degrees; the skeleton "NPC" node carries no rotation

		constexpr const char* kBaseNode{ "NPC" };
		constexpr const char* kHeadNode{ "NPC Head [Head]" };
		constexpr std::array kFeetNodes{ "NPC L Foot [Lft ]", "NPC R Foot [Rft ]" };

		struct TiltWatch
		{
			RE::FormID id{ 0 };
			float waited{ 0.0f };
			float elapsed{ 0.0f };
			size_t next{ 0 };
			bool left{ false };
		};

		struct StanceState
		{
			// in-scene
			bool blockedBefore{ false };
			bool sheathedSeen{ false };
			RE::WEAPON_STATE last{ RE::WEAPON_STATE::kSheathed };
			float drift{ 0.0f };
			int requestLogs{ 0 };
			// after the scene
			bool watching{ false };
			bool left{ false };
			float waited{ 0.0f };
			float elapsed{ 0.0f };
			float stuck{ 0.0f };
			float sheatheIn{ -1.0f };
		};

		std::mutex pendingLock;
		std::vector<RE::FormID> pendingEnds;

		std::vector<TiltWatch> tiltWatches;
		StanceState stance;

		std::string_view StateName(RE::WEAPON_STATE a_state)
		{
			switch (a_state) {
			case RE::WEAPON_STATE::kSheathed:
				return "sheathed";
			case RE::WEAPON_STATE::kWantToDraw:
				return "wantToDraw";
			case RE::WEAPON_STATE::kDrawing:
				return "drawing";
			case RE::WEAPON_STATE::kDrawn:
				return "drawn";
			case RE::WEAPON_STATE::kWantToSheathe:
				return "wantToSheathe";
			case RE::WEAPON_STATE::kSheathing:
				return "sheathing";
			default:
				return "unknown";
			}
		}

		bool LeftSheathed(RE::WEAPON_STATE a_state)
		{
			return a_state == RE::WEAPON_STATE::kWantToDraw || a_state == RE::WEAPON_STATE::kDrawing || a_state == RE::WEAPON_STATE::kDrawn;
		}

		// The behaviour graph shows a weapon or spell ready in either hand
		bool GraphReady(RE::Actor* a_actor)
		{
			for (const auto name : { "IsAttackReady", "bMRh_Ready", "bMLh_Ready" }) {
				bool value = false;
				if (a_actor->GetGraphVariableBool(name, value) && value) {
					return true;
				}
			}
			return false;
		}

		bool InScene(RE::Actor* a_actor)
		{
			return GameForms::AnimatingFaction && a_actor->IsInFaction(GameForms::AnimatingFaction);
		}

		// "Module.dll+RVA" of a code address; module file name only, never a path
		std::string DescribeAddress(const void* a_address)
		{
			REX::W32::MEMORY_BASIC_INFORMATION info{};
			if (a_address && REX::W32::VirtualQuery(a_address, &info, sizeof(info)) && info.allocationBase) {
				wchar_t path[260]{};
				const auto length = REX::W32::GetModuleFileNameW(reinterpret_cast<REX::W32::HMODULE>(info.allocationBase), path, 260);
				if (length > 0) {
					const auto file = fs::path{ std::wstring{ path, length } }.filename().string();
					return std::format("{}+{:X}", file, reinterpret_cast<std::uintptr_t>(a_address) - reinterpret_cast<std::uintptr_t>(info.allocationBase));
				}
			}
			return std::format("{:X}", reinterpret_cast<std::uintptr_t>(a_address));
		}

		float RotationDegrees(const RE::NiMatrix3& a_rotation)
		{
			const float trace = a_rotation.entry[0][0] + a_rotation.entry[1][1] + a_rotation.entry[2][2];
			const float cosine = std::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f);
			return std::acos(cosine) * 57.29578f;
		}

		// Angle between world up and the line from the middle of the feet to the head; nullopt without the nodes
		std::optional<float> BodyLeanDegrees(RE::Actor* a_actor)
		{
			const auto head = a_actor->GetNodeByName(kHeadNode);
			const auto left = a_actor->GetNodeByName(kFeetNodes[0]);
			const auto right = a_actor->GetNodeByName(kFeetNodes[1]);
			if (!head || !left || !right) {
				return std::nullopt;
			}
			const auto feet = (left->world.translate + right->world.translate) / 2.0f;
			const auto up = head->world.translate - feet;
			const float length = up.Length();
			if (!std::isfinite(length) || length < 1.0f) {
				return std::nullopt;
			}
			return std::acos(std::clamp(up.z / length, -1.0f, 1.0f)) * 57.29578f;
		}

		// ---- tilt ---------------------------------------------------------------------------------------------

		void CheckTilt(RE::Actor* a_actor, float a_elapsed, bool a_last)
		{
			const auto node = a_actor->GetNodeByName(kBaseNode);
			if (!node) {
				return;
			}
			const float before = RotationDegrees(node->local.rotate);
			const auto lean = BodyLeanDegrees(a_actor);
			const bool tilted = before > kTiltThreshold;
			if (tilted || a_last) {
				logger::info("Tilt check {:X} {:.1f}s after scene: NPC node rotation {:.2f} deg, body lean {}.",
					a_actor->GetFormID(), a_elapsed, before, lean ? std::format("{:.1f} deg", *lean) : std::string{ "n/a" });
			}
			if (!tilted) {
				return;
			}
			if (!::Settings::bPatchTiltGuard) {
				logger::warn("Tilt guard off: NPC node of {:X} left rotated by {:.2f} deg.", a_actor->GetFormID(), before);
				return;
			}
			if (!Registry::RaceKey{ a_actor }.Is(Registry::RaceKey::Human)) {
				logger::warn("Tilt guard: {:X} is not a humanoid, NPC node rotation {:.2f} deg left alone.", a_actor->GetFormID(), before);
				return;
			}
			// SKEE rebuilds the node from the skeleton plus its overrides (what a new scene start did by hand)
			Registry::Scale::GetSingleton()->RefreshBaseNode(a_actor);
			const float rebuilt = RotationDegrees(node->local.rotate);
			if (rebuilt <= kTiltThreshold) {
				logger::info("Tilt guard: NPC node of {:X} rebuilt by SKEE ({:.2f} -> {:.2f} deg).", a_actor->GetFormID(), before, rebuilt);
				return;
			}
			node->local.rotate = RE::NiMatrix3{};
			RE::NiUpdateData data{};
			node->Update(data);
			const auto leanAfter = BodyLeanDegrees(a_actor);
			logger::info("Tilt guard: NPC node rotation of {:X} cleared ({:.2f} deg, SKEE left {:.2f} deg); body lean now {}.",
				a_actor->GetFormID(), before, rebuilt, leanAfter ? std::format("{:.1f} deg", *leanAfter) : std::string{ "n/a" });
		}

		void UpdateTilt(float a_delta)
		{
			std::erase_if(tiltWatches, [&](TiltWatch& a_watch) {
				const auto actor = RE::TESForm::LookupByID<RE::Actor>(a_watch.id);
				if (!actor || !actor->Get3D()) {
					return true;
				}
				if (InScene(actor)) {
					if (a_watch.left) {
						return true;  // a new scene took the actor; its own end watches again
					}
					a_watch.waited += a_delta;
					if (a_watch.waited >= kFactionWait) {
						logger::info("Tilt check {:X}: still in a scene {:.0f}s after the end, not checked.", a_watch.id, a_watch.waited);
						return true;
					}
					return false;
				}
				if (!a_watch.left) {
					a_watch.left = true;
					// the moment the scene let go: report what it left behind before anything else touches the node
					if (const auto node = actor->GetNodeByName(kBaseNode)) {
						const auto lean = BodyLeanDegrees(actor);
						logger::info("Tilt check {:X} at scene exit: NPC node rotation {:.2f} deg, body lean {}.", a_watch.id,
							RotationDegrees(node->local.rotate), lean ? std::format("{:.1f} deg", *lean) : std::string{ "n/a" });
					}
				}
				a_watch.elapsed += a_delta;
				while (a_watch.next < kTiltChecks.size() && a_watch.elapsed >= kTiltChecks[a_watch.next]) {
					const bool last = a_watch.next + 1 == kTiltChecks.size();
					CheckTilt(actor, a_watch.elapsed, last);
					a_watch.next++;
				}
				return a_watch.next >= kTiltChecks.size();
			});
		}

		// ---- stance -------------------------------------------------------------------------------------------

		void UpdateStanceInScene(RE::PlayerCharacter* a_player, float a_delta, bool a_drawBlocked)
		{
			const auto state = a_player->AsActorState()->GetWeaponState();
			if (!a_drawBlocked) {
				if (stance.blockedBefore) {
					const bool ready = GraphReady(a_player);
					logger::info("Stance: player draw block lifted, weapon state {}, graph {}.", StateName(state), ready ? "ready" : "not ready");
				}
				stance.blockedBefore = false;
				return;
			}
			if (!stance.blockedBefore) {
				stance = StanceState{};
				stance.blockedBefore = true;
				stance.last = state;
				logger::info("Stance: player draw block set, weapon state {}.", StateName(state));
			}
			if (state != stance.last) {
				logger::info("Stance: player weapon state {} -> {} during the scene (graph {}).", StateName(stance.last), StateName(state),
					GraphReady(a_player) ? "ready" : "not ready");
				stance.last = state;
			}
			if (state == RE::WEAPON_STATE::kSheathed) {
				stance.sheathedSeen = true;
				stance.drift = 0.0f;
				return;
			}
			// Before the sheathe wait finished the state is SexLab's own sheathe in progress
			if (!stance.sheathedSeen || !LeftSheathed(state) || GraphReady(a_player)) {
				stance.drift = 0.0f;
				return;
			}
			stance.drift += a_delta;
			if (stance.drift < kDriftConfirm || !::Settings::bPatchStanceGuard) {
				return;
			}
			a_player->AsActorState()->actorState2.weaponState = RE::WEAPON_STATE::kSheathed;
			logger::warn("Stance guard: player weapon state drifted to {} during the scene with the graph not drawn; set back to sheathed.", StateName(state));
			stance.last = RE::WEAPON_STATE::kSheathed;
			stance.drift = 0.0f;
		}

		void UpdateStanceAfterScene(RE::PlayerCharacter* a_player, float a_delta)
		{
			if (!stance.watching) {
				return;
			}
			if (InScene(a_player)) {
				if (stance.left) {
					stance.watching = false;  // a new scene took the player
					return;
				}
				stance.waited += a_delta;
				if (stance.waited >= kFactionWait) {
					stance.watching = false;
				}
				return;
			}
			if (!stance.left) {
				stance.left = true;
				stance.last = a_player->AsActorState()->GetWeaponState();
				stance.requestLogs = 0;
				logger::info("Stance: player left the scene, weapon state {}, graph {}.", StateName(stance.last), GraphReady(a_player) ? "ready" : "not ready");
			}
			const auto state = a_player->AsActorState()->GetWeaponState();
			const bool ready = GraphReady(a_player);
			if (state != stance.last) {
				logger::info("Stance: player weapon state {} -> {} {:.1f}s after the scene (graph {}).", StateName(stance.last), StateName(state), stance.elapsed,
					ready ? "ready" : "not ready");
				stance.last = state;
			}
			if (stance.sheatheIn >= 0.0f) {
				stance.sheatheIn -= a_delta;
				if (stance.sheatheIn < 0.0f) {
					a_player->DrawWeaponMagicHands(false);
					logger::info("Stance guard: graph drawn (graph {}), sheathing the player weapon normally.", ready ? "ready" : "not ready");
					stance.watching = false;
				}
				return;
			}
			stance.elapsed += a_delta;
			if (state == RE::WEAPON_STATE::kDrawn && !ready) {
				stance.stuck += a_delta;
			} else {
				stance.stuck = 0.0f;
			}
			if (stance.stuck >= kStanceStuck) {
				if (!::Settings::bPatchStanceGuard) {
					logger::warn("Stance guard off: player drawn without a ready graph {:.1f}s after the scene.", stance.elapsed);
					stance.watching = false;
					return;
				}
				logger::warn("Stance guard: player drawn without a ready graph for {:.1f}s ({:.1f}s after the scene); resyncing the graph.", stance.stuck, stance.elapsed);
				a_player->NotifyAnimationGraph("WeapEquip");
				stance.sheatheIn = kResyncSheatheDelay;
				return;
			}
			if (stance.elapsed >= kStanceWatch) {
				logger::info("Stance: watch over, player weapon state {}, graph {}.", StateName(state), ready ? "ready" : "not ready");
				stance.watching = false;
			}
		}
	}

	void OnDrawRequest(bool a_draw, bool a_blocked, const void* a_caller)
	{
		const bool inWindow = stance.watching && stance.left;
		if (!(a_blocked && a_draw) && !inWindow) {
			return;
		}
		if (stance.requestLogs >= kMaxRequestLogs) {
			return;
		}
		stance.requestLogs++;
		logger::info("Stance: player {} request from {}{}.", a_draw ? "draw" : "sheathe", DescribeAddress(a_caller),
			a_blocked ? " (blocked, scene running)" : " (after the scene)");
	}

	void OnSceneEnd(const std::vector<RE::Actor*>& a_actors)
	{
		std::scoped_lock lock{ pendingLock };
		for (const auto actor : a_actors) {
			if (actor) {
				pendingEnds.push_back(actor->GetFormID());
			}
		}
	}

	void Update(float a_delta, bool a_drawBlocked)
	{
		std::vector<RE::FormID> ended;
		{
			std::scoped_lock lock{ pendingLock };
			ended.swap(pendingEnds);
		}
		for (const auto id : ended) {
			std::erase_if(tiltWatches, [id](const TiltWatch& a_watch) { return a_watch.id == id; });
			tiltWatches.push_back({ id });
			if (id == 0x14) {
				const bool blockedBefore = stance.blockedBefore;
				stance = StanceState{};
				stance.blockedBefore = blockedBefore;
				stance.watching = true;
			}
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Get3D()) {
			return;
		}
		UpdateStanceInScene(player, a_delta, a_drawBlocked);
		UpdateStanceAfterScene(player, a_delta);
		UpdateTilt(a_delta);
	}
}
