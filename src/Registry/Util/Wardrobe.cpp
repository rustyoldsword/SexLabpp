#include "Wardrobe.h"

#include "Registry/Util/Scale.h"
#include "UserData/StripData.h"

namespace Registry
{
    namespace
    {
        constexpr uint32_t Slot(uint32_t a_slot) { return 1u << (a_slot - 30); }

        constexpr uint32_t kHeadMask = Slot(30) | Slot(31) | Slot(41) | Slot(42) | Slot(43) | Slot(44) | Slot(55);
        constexpr uint32_t kNeckMask = Slot(35) | Slot(45);
        constexpr uint32_t kBodyMask = Slot(32);
        constexpr uint32_t kUnderwearMask = Slot(49) | Slot(52) | Slot(56);
        constexpr uint32_t kHandsMask = Slot(33) | Slot(34);
        constexpr uint32_t kRingMask = Slot(36);
        constexpr uint32_t kFeetMask = Slot(37);
        constexpr uint32_t kLegsMask = Slot(38) | Slot(53) | Slot(54);
        constexpr uint32_t kBackMask = Slot(46) | Slot(47);
        constexpr uint32_t kShieldMask = Slot(39);

        constexpr std::array kGroupNames{
            "Head", "Neck", "Body", "Underwear", "Hands", "Rings", "Feet", "Legs", "Back", "Shield", "Weapons", "Other"
        };
        static_assert(kGroupNames.size() == static_cast<size_t>(WardrobeGroup::Total));

        constexpr auto kClothingPath = USER_CONFIGS("Wardrobe.yaml");

        constexpr size_t Idx(WardrobeGroup a_group) { return static_cast<size_t>(a_group); }

        std::string_view StripName(StripRule a_rule)
        {
            switch (a_rule) {
            case StripRule::Strip:
                return "strip";
            case StripRule::Keep:
                return "keep";
            default:
                return "auto";
            }
        }
        std::string_view RedressName(RedressRule a_rule)
        {
            switch (a_rule) {
            case RedressRule::Redress:
                return "redress";
            case RedressRule::Never:
                return "never";
            default:
                return "auto";
            }
        }
        StripRule ParseStrip(std::string_view a_text)
        {
            return a_text == "strip" ? StripRule::Strip : a_text == "keep" ? StripRule::Keep :
                                                                              StripRule::Auto;
        }
        RedressRule ParseRedress(std::string_view a_text)
        {
            return a_text == "redress" ? RedressRule::Redress : a_text == "never" ? RedressRule::Never :
                                                                                    RedressRule::Auto;
        }

        // "plugin" + local id, the same way Stripping.yaml addresses forms
        std::optional<std::pair<std::string, uint32_t>> ToPluginKey(RE::FormID a_formid)
        {
            const auto handler = RE::TESDataHandler::GetSingleton();
            const auto modidx = a_formid >> 24;
            if (modidx == 0xFE) {
                const auto file = handler->LookupLoadedLightModByIndex(static_cast<uint16_t>((a_formid >> 12) & 0xFFF));
                if (file)
                    return std::make_pair(std::string{ file->fileName }, a_formid & 0xFFFu);
            } else {
                const auto file = handler->LookupLoadedModByIndex(static_cast<uint8_t>(modidx));
                if (file)
                    return std::make_pair(std::string{ file->fileName }, a_formid & 0xFFFFFFu);
            }
            return std::nullopt;
        }

        YAML::Node ProfileToYaml(const Profile& a_profile)
        {
            YAML::Node node;
            for (size_t i = 0; i < a_profile.size(); i++) {
                const auto& rule = a_profile[i];
                if (rule.IsDefault())
                    continue;
                YAML::Node entry;
                entry["strip"] = std::string{ StripName(rule.strip) };
                entry["redress"] = std::string{ RedressName(rule.redress) };
                if (rule.heelsOnly)
                    entry["heelsOnly"] = true;
                node[kGroupNames[i]] = entry;
            }
            return node;
        }

        Profile ProfileFromYaml(const YAML::Node& a_node)
        {
            Profile profile{};
            // A missing key on a const node is a zombie node: anything but IsDefined() throws InvalidNode
            if (!a_node.IsDefined() || !a_node.IsMap())
                return profile;
            for (size_t i = 0; i < kGroupNames.size(); i++) {
                const auto entry = a_node[kGroupNames[i]];
                if (!entry.IsDefined() || !entry.IsMap())
                    continue;
                profile[i].strip = ParseStrip(entry["strip"].as<std::string>("auto"));
                profile[i].redress = ParseRedress(entry["redress"].as<std::string>("auto"));
                profile[i].heelsOnly = entry["heelsOnly"].as<bool>(false);
            }
            return profile;
        }

        bool IsProfileEmpty(const Profile& a_profile)
        {
            return std::ranges::all_of(a_profile, [](const GroupRule& a_rule) { return a_rule.IsDefault(); });
        }

        // Items nobody should ever take off through a UI button (SOS schlongs, quest gear, ...)
        bool IsProtected(RE::TESBoundObject* a_item)
        {
            return UserData::StripData::GetSingleton()->CheckStrip(a_item) == UserData::Strip::NoStrip;
        }
    }

    const char* Wardrobe::GroupLabel(WardrobeGroup a_group)
    {
        return a_group < WardrobeGroup::Total ? kGroupNames[Idx(a_group)] : "";
    }

    WardrobeGroup Wardrobe::Classify(const RE::TESForm* a_form)
    {
        if (!a_form)
            return WardrobeGroup::Other;
        if (a_form->IsWeapon())
            return WardrobeGroup::Weapons;
        const auto biped = a_form->As<RE::BGSBipedObjectForm>();
        if (!biped || !a_form->IsArmor())
            return WardrobeGroup::Other;
        const auto mask = biped->GetSlotMask().underlying();
        if (mask & kBodyMask)
            return WardrobeGroup::Body;
        if (mask & kShieldMask)
            return WardrobeGroup::Shield;
        if (mask & kUnderwearMask)
            return WardrobeGroup::Underwear;
        if (mask & kFeetMask)
            return WardrobeGroup::Feet;
        if (mask & kLegsMask)
            return WardrobeGroup::Legs;
        if (mask & kHandsMask)
            return WardrobeGroup::Hands;
        if (mask & kRingMask)
            return WardrobeGroup::Rings;
        if (mask & kNeckMask)
            return WardrobeGroup::Neck;
        if (mask & kHeadMask)
            return WardrobeGroup::Head;
        if (mask & kBackMask)
            return WardrobeGroup::Back;
        return WardrobeGroup::Other;
    }

    StripRule Wardrobe::Resolved::Strip(WardrobeGroup a_group) const
    {
        const auto& rule = profile[Idx(a_group)];
        if (a_group == WardrobeGroup::Feet && rule.heelsOnly) {
            // "Keep the heels": the shoes stay while the actor stands on high heels, everything else goes
            return wearsHeels ? StripRule::Keep : StripRule::Strip;
        }
        return rule.strip;
    }

    RE::FormID Wardrobe::ActorKey(RE::Actor* a_actor)
    {
        const auto base = a_actor ? a_actor->GetActorBase() : nullptr;
        return base ? base->GetFormID() : 0;
    }

    Wardrobe::Resolved Wardrobe::Resolve(RE::Actor* a_actor)
    {
        Profile result{};
        {
            std::scoped_lock lock{ _m };
            const auto sceneIt = _scene.find(a_actor->GetFormID());
            const auto actorIt = _actors.find(ActorKey(a_actor));
            for (size_t i = 0; i < result.size(); i++) {
                const std::array<const GroupRule*, 3> layers{
                    sceneIt != _scene.end() ? &sceneIt->second[i] : nullptr,
                    actorIt != _actors.end() ? &actorIt->second[i] : nullptr,
                    &_global[i],
                };
                for (const auto layer : layers) {
                    if (layer && layer->strip != StripRule::Auto) {
                        result[i].strip = layer->strip;
                        result[i].heelsOnly = layer->heelsOnly;
                        break;
                    }
                    // a layer that only sets the heel mode still wins for the Feet group
                    if (layer && layer->heelsOnly && i == Idx(WardrobeGroup::Feet)) {
                        result[i].heelsOnly = true;
                        break;
                    }
                }
                for (const auto layer : layers) {
                    if (layer && layer->redress != RedressRule::Auto) {
                        result[i].redress = layer->redress;
                        break;
                    }
                }
            }
        }
        return Resolved{ result, Scale::GetSingleton()->WearsHighHeels(a_actor) };
    }

    Scope Wardrobe::Origin(RE::Actor* a_actor, WardrobeGroup a_group)
    {
        std::scoped_lock lock{ _m };
        const auto i = Idx(a_group);
        if (const auto it = _scene.find(a_actor->GetFormID()); it != _scene.end() && !it->second[i].IsDefault())
            return Scope::Scene;
        if (const auto it = _actors.find(ActorKey(a_actor)); it != _actors.end() && !it->second[i].IsDefault())
            return Scope::Actor;
        return Scope::Global;
    }

    Profile& Wardrobe::SceneProfile(RE::Actor* a_actor)
    {
        return _scene[a_actor->GetFormID()];
    }

    GroupRule Wardrobe::GetRule(RE::Actor* a_actor, Scope a_scope, WardrobeGroup a_group)
    {
        std::scoped_lock lock{ _m };
        const auto i = Idx(a_group);
        switch (a_scope) {
        case Scope::Scene:
            if (const auto it = _scene.find(a_actor->GetFormID()); it != _scene.end())
                return it->second[i];
            return {};
        case Scope::Actor:
            if (const auto it = _actors.find(ActorKey(a_actor)); it != _actors.end())
                return it->second[i];
            return {};
        default:
            return _global[i];
        }
    }

    void Wardrobe::SetRule(RE::Actor* a_actor, Scope a_scope, WardrobeGroup a_group, const GroupRule& a_rule)
    {
        {
            std::scoped_lock lock{ _m };
            const auto i = Idx(a_group);
            switch (a_scope) {
            case Scope::Scene:
                SceneProfile(a_actor)[i] = a_rule;
                break;
            case Scope::Actor:
                _actors[ActorKey(a_actor)][i] = a_rule;
                _dirty = true;
                break;
            default:
                _global[i] = a_rule;
                _dirty = true;
                break;
            }
        }
        if (a_scope != Scope::Scene)
            Save();
    }

    void Wardrobe::ResetRules(RE::Actor* a_actor, Scope a_scope)
    {
        {
            std::scoped_lock lock{ _m };
            switch (a_scope) {
            case Scope::Scene:
                _scene.erase(a_actor->GetFormID());
                break;
            case Scope::Actor:
                _actors.erase(ActorKey(a_actor));
                _dirty = true;
                break;
            default:
                _global = {};
                _dirty = true;
                break;
            }
        }
        if (a_scope != Scope::Scene)
            Save();
    }

    // ---------------------------------------------------------------------------------------------
    // Equipment
    // ---------------------------------------------------------------------------------------------

    void Wardrobe::Equip(RE::Actor* a_actor, RE::TESBoundObject* a_item)
    {
        if (const auto manager = RE::ActorEquipManager::GetSingleton())
            manager->EquipObject(a_actor, a_item, nullptr, 1, nullptr, false, false, false, true);
    }

    void Wardrobe::Unequip(RE::Actor* a_actor, RE::TESBoundObject* a_item)
    {
        if (const auto manager = RE::ActorEquipManager::GetSingleton())
            manager->UnequipObject(a_actor, a_item, nullptr, 1, nullptr, false, false, false, true);
    }

    std::vector<Wardrobe::WornItem> Wardrobe::GetWorn(RE::Actor* a_actor) const
    {
        std::vector<WornItem> result;
        if (!a_actor)
            return result;
        for (const auto& [form, data] : a_actor->GetInventory()) {
            if (!data.second || !data.second->IsWorn())
                continue;
            const auto group = Classify(form);
            // Plain items without any slot are not clothing
            if (group == WardrobeGroup::Other && (!form->IsArmor() || !form->As<RE::BGSBipedObjectForm>()))
                continue;
            result.push_back({ form, group });
        }
        return result;
    }

    bool Wardrobe::IsGroupWorn(RE::Actor* a_actor, WardrobeGroup a_group) const
    {
        const auto worn = GetWorn(a_actor);
        return std::ranges::any_of(worn, [&](const WornItem& a_item) { return a_item.group == a_group; });
    }

    std::vector<RE::TESBoundObject*> Wardrobe::GetRemoved(RE::Actor* a_actor, WardrobeGroup a_group) const
    {
        std::vector<RE::TESBoundObject*> result;
        std::scoped_lock lock{ _m };
        const auto it = _removed.find(a_actor->GetFormID());
        if (it == _removed.end())
            return result;
        for (const auto& entry : it->second) {
            if (entry.group != a_group)
                continue;
            if (const auto form = RE::TESForm::LookupByID<RE::TESBoundObject>(entry.item))
                result.push_back(form);
        }
        return result;
    }

    bool Wardrobe::HasRemoved(RE::Actor* a_actor, WardrobeGroup a_group) const
    {
        std::scoped_lock lock{ _m };
        const auto it = _removed.find(a_actor->GetFormID());
        return it != _removed.end() && std::ranges::any_of(it->second, [&](const Removed& a_entry) { return a_entry.group == a_group; });
    }

    bool Wardrobe::OnStripped(RE::Actor* a_actor, RE::TESBoundObject* a_item, WardrobeGroup a_group, RedressRule a_redress)
    {
        {
            std::scoped_lock lock{ _m };
            auto& list = _removed[a_actor->GetFormID()];
            const auto id = a_item->GetFormID();
            if (std::ranges::none_of(list, [&](const Removed& a_entry) { return a_entry.item == id; }))
                list.push_back({ id, a_group, a_redress, false });
        }
        // Never: SexLab must not put it back
        return a_redress != RedressRule::Never;
    }

    void Wardrobe::StripNow(RE::Actor* a_actor, WardrobeGroup a_group)
    {
        if (!a_actor)
            return;
        const auto rule = Resolve(a_actor).Redress(a_group);
        for (const auto& worn : GetWorn(a_actor)) {
            if (worn.group != a_group || IsProtected(worn.item))
                continue;
            Unequip(a_actor, worn.item);
            std::scoped_lock lock{ _m };
            auto& list = _removed[a_actor->GetFormID()];
            const auto id = worn.item->GetFormID();
            // An item SexLab already stripped keeps its owner: SexLab redresses it, unless the user said never
            if (std::ranges::none_of(list, [&](const Removed& a_entry) { return a_entry.item == id; }))
                list.push_back({ id, a_group, rule, true });
        }
        a_actor->Update3DModel();
    }

    void Wardrobe::DressNow(RE::Actor* a_actor, WardrobeGroup a_group)
    {
        if (!a_actor)
            return;
        std::vector<RE::TESBoundObject*> items;
        {
            std::scoped_lock lock{ _m };
            const auto it = _removed.find(a_actor->GetFormID());
            if (it == _removed.end())
                return;
            std::erase_if(it->second, [&](const Removed& a_entry) {
                if (a_entry.group != a_group)
                    return false;
                if (const auto form = RE::TESForm::LookupByID<RE::TESBoundObject>(a_entry.item))
                    items.push_back(form);
                return true;
            });
        }
        for (const auto item : items)
            Equip(a_actor, item);
        a_actor->Update3DModel();
    }

    void Wardrobe::StripAllNow(RE::Actor* a_actor, bool a_weapons)
    {
        for (size_t i = 0; i < Idx(WardrobeGroup::Total); i++) {
            const auto group = static_cast<WardrobeGroup>(i);
            if (group == WardrobeGroup::Weapons && !a_weapons)
                continue;
            // Groups the user pinned with "Keep" stay on
            if (Resolve(a_actor).Strip(group) == StripRule::Keep)
                continue;
            StripNow(a_actor, group);
        }
    }

    void Wardrobe::DressAllNow(RE::Actor* a_actor)
    {
        for (size_t i = 0; i < Idx(WardrobeGroup::Total); i++)
            DressNow(a_actor, static_cast<WardrobeGroup>(i));
    }

    void Wardrobe::ApplyLive(RE::Actor* a_actor, WardrobeGroup a_group)
    {
        if (!a_actor)
            return;
        switch (Resolve(a_actor).Strip(a_group)) {
        case StripRule::Strip:
            StripNow(a_actor, a_group);
            break;
        case StripRule::Keep:
            DressNow(a_actor, a_group);
            break;
        default:
            break;
        }
    }

    void Wardrobe::EndScene(const std::vector<RE::Actor*>& a_actors, const std::vector<bool>& a_redressAllowed)
    {
        for (size_t n = 0; n < a_actors.size(); n++) {
            const auto actor = a_actors[n];
            if (!actor)
                continue;
            const bool allowed = n < a_redressAllowed.size() ? static_cast<bool>(a_redressAllowed[n]) : true;
            std::vector<RE::TESBoundObject*> restore;
            {
                std::scoped_lock lock{ _m };
                if (const auto it = _removed.find(actor->GetFormID()); it != _removed.end()) {
                    for (const auto& entry : it->second) {
                        if (entry.redress == RedressRule::Never)
                            continue;
                        // SexLab restores its own strips; we only restore ours, or force it when asked to
                        const bool ours = entry.viaPanel && allowed;
                        if (!ours && entry.redress != RedressRule::Redress)
                            continue;
                        if (const auto form = RE::TESForm::LookupByID<RE::TESBoundObject>(entry.item))
                            restore.push_back(form);
                    }
                    _removed.erase(it);
                }
                _scene.erase(actor->GetFormID());
            }
            for (const auto item : restore)
                Equip(actor, item);
            if (!restore.empty())
                actor->Update3DModel();
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Persistence
    // ---------------------------------------------------------------------------------------------

    void Wardrobe::Load()
    {
        std::scoped_lock lock{ _m };
        _global = {};
        _actors.clear();
        try {
            if (!fs::exists(kClothingPath)) {
                logger::info("Wardrobe: no saved rules yet");
                return;
            }
            const auto root = YAML::LoadFile(kClothingPath);
            _global = ProfileFromYaml(root["global"]);
            const auto handler = RE::TESDataHandler::GetSingleton();
            const auto actors = root["actors"];
            if (!actors.IsDefined() || !actors.IsMap()) {
                logger::info("Wardrobe: loaded rules for 0 characters");
                return;
            }
            for (const auto&& plugin : actors) {
                const auto esp = plugin.first.as<std::string>();
                if (!handler->LookupModByName(esp)) {
                    logger::info("Wardrobe: skipping rules for {}, plugin is not loaded", esp);
                    continue;
                }
                for (const auto&& entry : plugin.second) {
                    const auto formid = handler->LookupFormID(entry.first.as<uint32_t>(), esp);
                    if (formid)
                        _actors[formid] = ProfileFromYaml(entry.second);
                }
            }
        } catch (const std::exception& e) {
            logger::error("Wardrobe: unable to load {}: {}", kClothingPath, e.what());
        }
        logger::info("Wardrobe: loaded rules for {} characters", _actors.size());
    }

    void Wardrobe::Save()
    {
        std::scoped_lock lock{ _m };
        if (!_dirty)
            return;
        try {
            YAML::Node root;
            if (!IsProfileEmpty(_global))
                root["global"] = ProfileToYaml(_global);
            for (const auto& [formid, profile] : _actors) {
                if (IsProfileEmpty(profile))
                    continue;
                const auto key = ToPluginKey(formid);
                if (!key) {
                    logger::warn("Wardrobe: cannot address form {:X}, rules not saved", formid);
                    continue;
                }
                root["actors"][key->first][key->second] = ProfileToYaml(profile);
            }
            fs::create_directories(fs::path{ kClothingPath }.parent_path());
            std::ofstream{ kClothingPath } << root;
            _dirty = false;
        } catch (const std::exception& e) {
            logger::error("Wardrobe: unable to save {}: {}", kClothingPath, e.what());
        }
    }
}
