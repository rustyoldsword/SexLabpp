#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace Registry::HeelMath
{
    // ---------------------------------------------------------------------------------------------------------
    // Standing on heels in an animation made for bare feet
    //
    // The animation puts the pelvis where it wants it and the legs hang from it. A heel lifts the actor by h, so
    // something has to give way. Every way of giving way distorts the actor in its own visible way:
    //
    //   body scale b        the whole actor is smaller            visible as height, tolerance tolBody (fraction)
    //   leg scale k         thighs and shins thinner and shorter  visible as girth,  tolerance tolLegScale (fraction)
    //   knee shift sK       the knee joint slides up the thigh    the skin blend at the knee (width kneeBlend) is
    //                                                             compressed by sK / kneeBlend, tolerance tolKnee
    //   ankle shift sA      the foot slides up the shin           same at the ankle, width ankleBlend, tolAnkle
    //   anchor error        pelvis / head away from where the animation wants them, tolerance tolAnchor (units)
    //
    // Each distortion is expressed in units of its own tolerance and the sum of squares is minimised under one hard
    // constraint: the soles stay exactly on the floor. With L the drop the legs take, the "NPC" node is lifted by
    // b * (h - L) and a point at bare-foot height Y ends at b * (Y + h - L).
    //
    // For a given L the legs share it by least squares (each mechanism takes L * c_i / sum c^2 of the work, c_i
    // being the drop one tolerance of it buys, clamped mechanisms hand their excess to the others), and the body
    // scale has a closed form. The remaining one-dimensional problem in L is solved by sampling plus golden section.
    // All lengths are unscaled skeleton units of the "NPC" node.
    // ---------------------------------------------------------------------------------------------------------

    struct Anchor
    {
        float height{ 0.0f };  // bare-foot height of the point, 0 = unknown
        float weight{ 0.0f };  // how much this stage cares about it
    };

    struct StandingInput
    {
        float heel{ 0.0f };
        Anchor pelvis{};
        Anchor head{};
        float legDrop{ 0.0f };        // vertical hip-to-foot drop of the supporting leg at full length, 0 = unknown
        float thighVertical{ 1.0f };  // vertical share of the thigh and shin directions while standing
        float shinVertical{ 1.0f };

        float tolBody{ 0.03f };
        float tolLegScale{ 0.04f };
        float tolKnee{ 0.25f };
        float tolAnkle{ 0.35f };
        float tolAnchor{ 0.25f };
        float kneeBlend{ 10.0f };
        float ankleBlend{ 8.0f };

        float bodyMin{ 0.8f };
        float legScaleMin{ 0.85f };
        float kneeMax{ 4.0f };
        float ankleMax{ 4.0f };
        bool legScale{ true };
        bool joints{ true };
    };

    struct StandingFit
    {
        float body{ 1.0f };
        float legScale{ 1.0f };
        float kneeShift{ 0.0f };   // along the thigh, toward the hip
        float ankleShift{ 0.0f };  // along the shin, toward the knee
        float lift{ 0.0f };        // "NPC" node lift per unit of final scale: node z = lift * finalScale
        float legDrop{ 0.0f };     // L, the part of the heel the legs absorb
        float pelvisError{ 0.0f }; // final minus bare-foot height
        float headError{ 0.0f };
        float cost{ 0.0f };
    };

    namespace detail
    {
        constexpr size_t mechanisms = 3;  // leg scale, knee shift, ankle shift

        struct LegModel
        {
            std::array<float, mechanisms> c{};    // drop bought by one tolerance unit
            std::array<float, mechanisms> cap{};  // most tolerance units allowed
            float capacity{ 0.0f };
        };

        inline LegModel MakeLegModel(const StandingInput& a_in)
        {
            LegModel m;
            if (!(a_in.legDrop > 0.0f)) {
                return m;
            }
            if (a_in.legScale && a_in.tolLegScale > 0.0f) {
                m.c[0] = a_in.tolLegScale * a_in.legDrop;
                m.cap[0] = std::max(0.0f, 1.0f - a_in.legScaleMin) / a_in.tolLegScale;
            }
            if (a_in.joints && a_in.tolKnee > 0.0f && a_in.kneeBlend > 0.0f) {
                m.c[1] = a_in.kneeBlend * a_in.tolKnee * std::clamp(a_in.thighVertical, 0.0f, 1.0f);
                m.cap[1] = std::max(0.0f, a_in.kneeMax) / (a_in.kneeBlend * a_in.tolKnee);
            }
            if (a_in.joints && a_in.tolAnkle > 0.0f && a_in.ankleBlend > 0.0f) {
                m.c[2] = a_in.ankleBlend * a_in.tolAnkle * std::clamp(a_in.shinVertical, 0.0f, 1.0f);
                m.cap[2] = std::max(0.0f, a_in.ankleMax) / (a_in.ankleBlend * a_in.tolAnkle);
            }
            for (size_t i = 0; i < mechanisms; i++) {
                m.capacity += m.c[i] * m.cap[i];
            }
            return m;
        }

        // Least-squares share of a leg drop L, with clamped mechanisms handing their excess to the others
        inline std::array<float, mechanisms> SplitLegDrop(const LegModel& a_model, float a_drop)
        {
            std::array<float, mechanisms> e{};
            std::array<bool, mechanisms> fixed{};
            float rest = std::max(a_drop, 0.0f);
            for (size_t pass = 0; pass <= mechanisms; pass++) {
                float sum = 0.0f;
                for (size_t i = 0; i < mechanisms; i++) {
                    if (!fixed[i]) {
                        sum += a_model.c[i] * a_model.c[i];
                    }
                }
                if (sum <= 0.0f) {
                    break;
                }
                bool clamped = false;
                for (size_t i = 0; i < mechanisms; i++) {
                    if (fixed[i]) {
                        continue;
                    }
                    e[i] = rest * a_model.c[i] / sum;
                    if (e[i] > a_model.cap[i]) {
                        e[i] = a_model.cap[i];
                        fixed[i] = true;
                        rest -= a_model.c[i] * a_model.cap[i];
                        clamped = true;
                    }
                }
                if (!clamped) {
                    break;
                }
                for (size_t i = 0; i < mechanisms; i++) {
                    if (!fixed[i]) {
                        e[i] = 0.0f;
                    }
                }
                rest = std::max(rest, 0.0f);
            }
            return e;
        }

        struct BodyFit
        {
            float body{ 1.0f };
            float cost{ 0.0f };
        };

        // Best body scale once the legs took L: closed form of a one-variable quadratic, then clamped
        inline BodyFit FitBody(const StandingInput& a_in, float a_residual)
        {
            const float wb = 1.0f / (a_in.tolBody * a_in.tolBody);
            const float wa = 1.0f / (a_in.tolAnchor * a_in.tolAnchor);
            float a = wb;
            float b = wb;
            for (const auto& anchor : { a_in.pelvis, a_in.head }) {
                if (anchor.height > 0.0f && anchor.weight > 0.0f) {
                    const float y = anchor.height + a_residual;
                    a += wa * anchor.weight * y * y;
                    b += wa * anchor.weight * anchor.height * y;
                }
            }
            BodyFit fit;
            fit.body = std::clamp(b / a, std::clamp(a_in.bodyMin, 0.1f, 1.0f), 1.0f);
            const float db = 1.0f - fit.body;
            fit.cost = wb * db * db;
            for (const auto& anchor : { a_in.pelvis, a_in.head }) {
                if (anchor.height > 0.0f && anchor.weight > 0.0f) {
                    const float err = fit.body * (anchor.height + a_residual) - anchor.height;
                    fit.cost += wa * anchor.weight * err * err;
                }
            }
            return fit;
        }

        inline float LegCost(const std::array<float, mechanisms>& a_e)
        {
            float cost = 0.0f;
            for (const auto e : a_e) {
                cost += e * e;
            }
            return cost;
        }

        inline float TotalCost(const StandingInput& a_in, const LegModel& a_model, float a_drop)
        {
            return LegCost(SplitLegDrop(a_model, a_drop)) + FitBody(a_in, a_in.heel - a_drop).cost;
        }
    }

    inline StandingFit SolveStanding(const StandingInput& a_in)
    {
        StandingFit fit;
        if (!(a_in.heel > 0.0f) || !std::isfinite(a_in.heel)) {
            fit.lift = std::isfinite(a_in.heel) ? std::max(a_in.heel, 0.0f) : 0.0f;
            return fit;
        }
        const auto model = detail::MakeLegModel(a_in);
        const float most = std::min(a_in.heel, model.capacity);

        float best = 0.0f;
        if (most > 0.0f) {
            constexpr int samples = 64;
            float bestCost = detail::TotalCost(a_in, model, 0.0f);
            for (int i = 1; i <= samples; i++) {
                const float drop = most * static_cast<float>(i) / samples;
                const float cost = detail::TotalCost(a_in, model, drop);
                if (cost < bestCost) {
                    bestCost = cost;
                    best = drop;
                }
            }
            // Golden section inside the bracket around the best sample
            const float step = most / samples;
            float lo = std::max(0.0f, best - step);
            float hi = std::min(most, best + step);
            constexpr float ratio = 0.6180340f;
            float x1 = hi - ratio * (hi - lo);
            float x2 = lo + ratio * (hi - lo);
            float f1 = detail::TotalCost(a_in, model, x1);
            float f2 = detail::TotalCost(a_in, model, x2);
            for (int i = 0; i < 40; i++) {
                if (f1 < f2) {
                    hi = x2;
                    x2 = x1;
                    f2 = f1;
                    x1 = hi - ratio * (hi - lo);
                    f1 = detail::TotalCost(a_in, model, x1);
                } else {
                    lo = x1;
                    x1 = x2;
                    f1 = f2;
                    x2 = lo + ratio * (hi - lo);
                    f2 = detail::TotalCost(a_in, model, x2);
                }
            }
            const float refined = 0.5f * (lo + hi);
            if (detail::TotalCost(a_in, model, refined) < bestCost) {
                best = refined;
            }
        }

        const auto e = detail::SplitLegDrop(model, best);
        const auto body = detail::FitBody(a_in, a_in.heel - best);
        fit.body = body.body;
        fit.legScale = 1.0f - e[0] * a_in.tolLegScale;
        fit.kneeShift = e[1] * a_in.kneeBlend * a_in.tolKnee;
        fit.ankleShift = e[2] * a_in.ankleBlend * a_in.tolAnkle;
        // What the legs really take (the clamps can leave a sliver of the requested drop unused)
        fit.legDrop = (1.0f - fit.legScale) * a_in.legDrop + fit.kneeShift * std::clamp(a_in.thighVertical, 0.0f, 1.0f) +
                      fit.ankleShift * std::clamp(a_in.shinVertical, 0.0f, 1.0f);
        const auto final = detail::FitBody(a_in, a_in.heel - fit.legDrop);
        fit.body = final.body;
        fit.lift = a_in.heel - fit.legDrop;
        fit.cost = detail::LegCost(e) + final.cost;
        if (a_in.pelvis.height > 0.0f) {
            fit.pelvisError = fit.body * (a_in.pelvis.height + fit.lift) - a_in.pelvis.height;
        }
        if (a_in.head.height > 0.0f) {
            fit.headError = fit.body * (a_in.head.height + fit.lift) - a_in.head.height;
        }
        return fit;
    }

    // ---------------------------------------------------------------------------------------------------------
    // Knee joint offset
    //
    // Skinning shared vertices between the thigh and the calf misplaces them more the deeper the knee bends; the sag
    // of a point at radius r from the knee axis grows as r * (1 - cos(theta / 2)). The joint is offset by that
    // curve, normalised to 1 at 90 degrees, along two directions whose amounts are tuned in game:
    //   front  toward the kneecap, the bisector of the outer angle (thigh direction minus shin direction)
    //   along  along the thigh (+ toward the knee)
    // Both can be negative. The 11.11 first build pushed +front 9 * curve and made the knees far worse in game.
    // ---------------------------------------------------------------------------------------------------------

    struct Vec3
    {
        float x{ 0.0f };
        float y{ 0.0f };
        float z{ 0.0f };
    };

    inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline Vec3 Sub(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    inline Vec3 Mul(const Vec3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
    inline float Length(const Vec3& a) { return std::sqrt(Dot(a, a)); }

    inline Vec3 Add(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }

    // 0 on a straight leg, 1 at 90 degrees, 3.41 fully folded
    inline float KneeCurve(float a_flexion)
    {
        constexpr float at90 = 0.29289322f;  // 1 - cos(45 degrees)
        return (1.0f - std::cos(0.5f * std::clamp(a_flexion, 0.0f, 3.14159265f))) / at90;
    }

    inline float KneeFlexion(const Vec3& a_thigh, const Vec3& a_shin)
    {
        return std::acos(std::clamp(Dot(a_thigh, a_shin), -1.0f, 1.0f));
    }

    // a_thigh: hip -> knee direction, a_shin: knee -> ankle direction, a_up: the floor normal. Unit vectors.
    // Result in skeleton units, world axes, no longer than a_max. On the floor the downward part is dropped,
    // so a knee resting on the ground is never pressed into it.
    inline Vec3 KneeOffset(const Vec3& a_thigh, const Vec3& a_shin, const Vec3& a_up, float a_front, float a_along, float a_max, bool a_onFloor)
    {
        const float curve = KneeCurve(KneeFlexion(a_thigh, a_shin));
        Vec3 front = Sub(a_thigh, a_shin);
        const float length = Length(front);
        front = length > 1e-4f ? Mul(front, 1.0f / length) : Vec3{};
        Vec3 offset = Add(Mul(front, a_front * curve), Mul(a_thigh, a_along * curve));
        if (a_onFloor) {
            const float down = Dot(offset, a_up);
            if (down < 0.0f) {
                offset = Sub(offset, Mul(a_up, down));
            }
        }
        const float size = Length(offset);
        const float cap = std::max(a_max, 0.0f);
        return size > cap && size > 0.0f ? Mul(offset, cap / size) : offset;
    }

    // Knee corrective morphs. A deeply bent knee pushes the calf through the thigh; the body's own sliders
    // (thinner upper calf) take that volume out. The slider amounts are scaled by a gain that follows the deepest
    // flexion of the actor's legs: smoothstep from start to full degrees. The peak is held for a while so a looping
    // stage gets one value, and the morphs are re-applied only on a real change, never faster than the interval
    // (a body morph update rebuilds the actor's body).
    struct KneeMorphParams
    {
        float start{ 90.0f };    // degrees of flexion where the correction begins
        float full{ 140.0f };    // degrees where it is complete
        float deepStart{ 150.0f };  // second tier for kneeling upright and other extreme folds
        float deepFull{ 167.0f };
        float hold{ 2.0f };      // seconds a peak is kept
        float step{ 0.08f };     // smallest gain change worth a body update
        float interval{ 0.5f };  // seconds between body updates
    };

    struct KneeMorphTracker
    {
        float peak{ 0.0f };      // degrees
        float holdLeft{ 0.0f };
        float applied{ 0.0f };   // gain currently on the body
        float appliedDeep{ 0.0f };  // second-tier gain currently on the body
        float sinceApply{ 1e9f };
    };

    inline float KneeMorphGain(float a_flexionDeg, const KneeMorphParams& a_params)
    {
        const float span = std::max(a_params.full - a_params.start, 1e-3f);
        const float x = std::clamp((a_flexionDeg - a_params.start) / span, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    // One frame. Returns true when the morphs should be written with a_tracker.applied as the new gain.
    inline bool KneeMorphStep(KneeMorphTracker& a_tracker, float a_flexionDeg, float a_delta, const KneeMorphParams& a_params)
    {
        a_tracker.sinceApply += a_delta;
        if (a_flexionDeg >= a_tracker.peak) {
            a_tracker.peak = a_flexionDeg;
            a_tracker.holdLeft = a_params.hold;
        } else {
            a_tracker.holdLeft -= a_delta;
            if (a_tracker.holdLeft <= 0.0f) {
                a_tracker.peak = a_flexionDeg;
                a_tracker.holdLeft = a_params.hold;
            }
        }
        const float gain = KneeMorphGain(a_tracker.peak, a_params);
        KneeMorphParams deep = a_params;
        deep.start = a_params.deepStart;
        deep.full = a_params.deepFull;
        const float deepGain = KneeMorphGain(a_tracker.peak, deep);
        const bool release = (gain == 0.0f && a_tracker.applied > 0.0f) || (deepGain == 0.0f && a_tracker.appliedDeep > 0.0f);
        if (!release && std::fabs(gain - a_tracker.applied) < a_params.step && std::fabs(deepGain - a_tracker.appliedDeep) < a_params.step) {
            return false;
        }
        if (a_tracker.sinceApply < a_params.interval) {
            return false;
        }
        a_tracker.applied = gain;
        a_tracker.appliedDeep = deepGain;
        a_tracker.sinceApply = 0.0f;
        return true;
    }
}
