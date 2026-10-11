#pragma once

#include <mutex>

namespace Registry
{
    // Smart clothing: every worn item belongs to one logical group. Each group has a strip rule
    // (what SexLab does with it while a scene runs) and a redress rule (what happens afterwards).
    // Rules come from layers, most specific first:
    //   scene   - set from the clothing panel, lives until the actor leaves the scene
    //   actor   - remembered for one character (saved to disk)
    //   global  - remembered for everybody (saved to disk)
    //   auto    - whatever SexLab would have done
    enum class WardrobeGroup : uint8_t
    {
        Head,       // helmets, hoods, hair slots, circlets, ears
        Neck,       // amulets, necklaces, scarves
        Body,       // armor, robes, dresses, shirts
        Underwear,  // panties, bras and other lingerie slots
        Hands,      // gloves and bracers
        Rings,
        Feet,       // shoes, boots, heels
        Legs,       // calves and leg pieces
        Back,       // cloaks and backpacks
        Shield,
        Weapons,
        Other,
        Total
    };

    enum class StripRule : uint8_t
    {
        Auto,   // let the scene decide
        Strip,  // take it off whenever a scene runs
        Keep,   // never take it off
    };

    enum class RedressRule : uint8_t
    {
        Auto,    // follow SexLab's redress settings
        Redress, // always put it back after the scene
        Never,   // leave it off after the scene
    };

    struct GroupRule
    {
        StripRule strip{ StripRule::Auto };
        RedressRule redress{ RedressRule::Auto };
        // Feet only: keep the footwear when the actor stands on high heels, strip every other shoe.
        bool heelsOnly{ false };

        bool operator==(const GroupRule&) const = default;
        [[nodiscard]] bool IsDefault() const { return *this == GroupRule{}; }
    };

    using Profile = std::array<GroupRule, static_cast<size_t>(WardrobeGroup::Total)>;

    enum class Scope : uint8_t
    {
        Scene,   // this scene only
        Actor,   // remembered for this character
        Global,  // remembered for everybody
    };

    class Wardrobe :
      public Singleton<Wardrobe>
    {
      public:
        static const char* GroupLabel(WardrobeGroup a_group);
        static WardrobeGroup Classify(const RE::TESForm* a_form);

        // The rules that apply to one actor right now, with all layers flattened.
        class Resolved
        {
          public:
            Resolved(const Profile& a_profile, bool a_wearsHeels) :
              profile(a_profile), wearsHeels(a_wearsHeels) {}

            // Returns Auto when the scene should decide.
            [[nodiscard]] StripRule Strip(WardrobeGroup a_group) const;
            [[nodiscard]] RedressRule Redress(WardrobeGroup a_group) const { return profile[static_cast<size_t>(a_group)].redress; }

          private:
            Profile profile;
            bool wearsHeels;
        };

        Resolved Resolve(RE::Actor* a_actor);

        // --- Hooks used by SexLab's own stripping ---------------------------------------------
        // Called for every item SexLab took off. Returns false if the item must not be handed back
        // to SexLab's redress list (the redress rule says it stays off).
        bool OnStripped(RE::Actor* a_actor, RE::TESBoundObject* a_item, WardrobeGroup a_group, RedressRule a_redress);
        // Called when a thread is destroyed: puts back what the panel took off.
        void EndScene(const std::vector<RE::Actor*>& a_actors, const std::vector<bool>& a_redressAllowed);

        // --- Live control from the panel ------------------------------------------------------
        struct WornItem
        {
            RE::TESBoundObject* item;
            WardrobeGroup group;
        };
        std::vector<WornItem> GetWorn(RE::Actor* a_actor) const;
        [[nodiscard]] std::vector<RE::TESBoundObject*> GetRemoved(RE::Actor* a_actor, WardrobeGroup a_group) const;
        [[nodiscard]] bool IsGroupWorn(RE::Actor* a_actor, WardrobeGroup a_group) const;
        [[nodiscard]] bool HasRemoved(RE::Actor* a_actor, WardrobeGroup a_group) const;

        void StripNow(RE::Actor* a_actor, WardrobeGroup a_group);
        void DressNow(RE::Actor* a_actor, WardrobeGroup a_group);
        void StripAllNow(RE::Actor* a_actor, bool a_weapons);
        void DressAllNow(RE::Actor* a_actor);
        // Takes off or puts back whatever the resolved rule of the group asks for
        void ApplyLive(RE::Actor* a_actor, WardrobeGroup a_group);

        // --- Rules ----------------------------------------------------------------------------
        GroupRule GetRule(RE::Actor* a_actor, Scope a_scope, WardrobeGroup a_group);
        void SetRule(RE::Actor* a_actor, Scope a_scope, WardrobeGroup a_group, const GroupRule& a_rule);
        void ResetRules(RE::Actor* a_actor, Scope a_scope);
        // The scope that currently decides the rule of a group, for display.
        Scope Origin(RE::Actor* a_actor, WardrobeGroup a_group);

        void Load();
        void Save();

      private:
        static RE::FormID ActorKey(RE::Actor* a_actor);
        Profile& SceneProfile(RE::Actor* a_actor);
        void Equip(RE::Actor* a_actor, RE::TESBoundObject* a_item);
        void Unequip(RE::Actor* a_actor, RE::TESBoundObject* a_item);

        mutable std::mutex _m;
        Profile _global{};
        std::unordered_map<RE::FormID, Profile> _actors;   // keyed by base NPC
        std::unordered_map<RE::FormID, Profile> _scene;    // keyed by reference, cleared by EndScene
        // Everything taken off through the panel or by SexLab's stripping, per actor reference
        struct Removed
        {
            RE::FormID item;
            WardrobeGroup group;
            RedressRule redress;
            bool viaPanel;
        };
        std::unordered_map<RE::FormID, std::vector<Removed>> _removed;
        bool _dirty{ false };
    };
}
