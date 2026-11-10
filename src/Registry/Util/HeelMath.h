#pragma once

#include <algorithm>

namespace Registry::HeelMath
{
    // How a standing actor on heels is fitted to an animation made for bare feet.
    // body:     uniform scale factor of the whole actor (1 = unchanged)
    // legScale: scale of both thighs (the feet get 1/legScale back, so the shoes keep their size)
    // lift:     "NPC" node lift per unit of final scale; the node z is lift * finalScale
    struct StandingFit
    {
        float body{ 1.0f };
        float legScale{ 1.0f };
        float lift{ 0.0f };
    };

    // All lengths are in unscaled skeleton units of the "NPC" node.
    // a_anchor   pelvis height of the pose on bare feet
    // a_heel     heel lift
    // a_legDrop  vertical drop from the hip joint to the foot of the supporting leg at full leg length (0 = unknown)
    //
    // Feet on the floor:   z = body * (heel - d), d = (1 - legScale) * legDrop
    // Pelvis where it was: z + body * anchor = anchor  =>  body = anchor / (anchor + heel - d)
    // The body is shrunk no further than a_bodyMin; the legs take the rest, down to a_minLegScale.
    // With no leg shortening this is the pure scale fit anchor / (anchor + heel).
    inline StandingFit SolveStanding(float a_anchor, float a_heel, float a_legDrop, float a_minFactor, float a_bodyMin, float a_minLegScale, bool a_legs)
    {
        StandingFit fit;
        if (!(a_anchor > 0.0f) || !(a_heel > 0.0f)) {
            fit.lift = std::max(a_heel, 0.0f);
            return fit;
        }
        float drop = 0.0f;
        if (a_legs && a_legDrop > 0.0f) {
            const float bodyTarget = std::clamp(a_bodyMin, a_minFactor, 1.0f);
            const float wanted = a_heel - a_anchor * (1.0f - bodyTarget) / bodyTarget;
            const float most = (1.0f - std::clamp(a_minLegScale, 0.0f, 1.0f)) * a_legDrop;
            drop = std::clamp(wanted, 0.0f, std::min(most, a_heel));
        }
        fit.body = std::clamp(a_anchor / (a_anchor + a_heel - drop), a_minFactor, 1.0f);
        fit.legScale = drop > 0.0f ? 1.0f - drop / a_legDrop : 1.0f;
        fit.lift = a_heel - drop;
        return fit;
    }
}
