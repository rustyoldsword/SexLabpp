#pragma once

// Weapon stance and skeleton tilt guards around SexLab scenes.
//
// Stance (WEAP-001/002): during a player scene SexLab blocks every draw request (Hooks::DrawWeaponHook), yet the
// engine weapon state was seen moving to "drawing" with no input, and to "drawn" a few seconds after the scene,
// while the behaviour graph had been reset to the unarmed idle. Attack, jump and draw/sheathe then do nothing.
//  - every draw request that reaches the player while blocked is logged with the calling module, so the writer
//    can be identified;
//  - while the scene runs, an engine state that left "sheathed" without the graph following is put back to
//    "sheathed" (the scene animation never drew anything);
//  - for a while after the scene, an engine "drawn" state the graph does not show is resynchronised: the graph
//    is sent to the drawn state, then the weapon is sheathed the normal way.
//
// Tilt: the "NPC" node (parent of "NPC Root [Root]") is written during scenes by SexLab's scale/heels through
// SKEE and by other plugins that tilt actors to the ground. A rotation left on that node keeps the skeleton
// leaning after the scene although the reference itself is upright. Shortly after each actor leaves the scene
// the node is checked, logged, and handed back to SKEE (which rebuilds it from the skeleton); a rotation that
// survives that is cleared.
namespace Thread::PostSceneGuard
{
	// From the player's draw hook (main thread). a_caller is the return address of the request.
	void OnDrawRequest(bool a_draw, bool a_blocked, const void* a_caller);

	// The scene that held these actors was destroyed (any thread).
	void OnSceneEnd(const std::vector<RE::Actor*>& a_actors);

	// Every frame from the frame hook (main thread). a_delta is 0 while the game is paused.
	void Update(float a_delta, bool a_drawBlocked);
}
