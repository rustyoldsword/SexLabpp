#include "Thread.h"

#include "Registry/Define/Sex.h"
#include "Registry/Util/Scale.h"
#include "Registry/Util/RayCast.h"
#include "Registry/Util/RayCast/ObjectBound.h"
#include "Registry/Util/RayCast/Offsets.h"
#include "Thread/Interface/FurnSelectMenu.h"
#include "Util/World.h"
#include <future>

namespace
{
    constexpr std::string_view PETITE_ADULT_TAG{ "Petite" };
    constexpr std::string_view PETITE_MALE_SMF_TAG{ "smf" };
    constexpr std::string_view PETITE_MALE_MSMF_TAG{ "msmf" };
    constexpr std::string_view PETITE_MALE_SMSMF_TAG{ "smsmf" };
    constexpr std::string_view PETITE_MALE_SMFF_TAG{ "smff" };

    bool IsShortAdultActor(RE::Actor* a_actor)
    {
        return a_actor && Registry::RaceKey{ a_actor }.Is(Registry::RaceKey::Human) &&
               Registry::Scale::GetSingleton()->GetScale(a_actor) <= Settings::fPetiteAdultScale;
    }

    bool IsFemaleActor(RE::Actor* a_actor)
    {
        const auto sex = Registry::GetSex(a_actor);
        return sex == Registry::Sex::Female || sex == Registry::Sex::Futa;
    }

    bool IsMaleActor(RE::Actor* a_actor)
    {
        return Registry::GetSex(a_actor) == Registry::Sex::Male;
    }

    bool HasSceneTag(const Registry::Scene* a_scene, const std::string_view a_tag)
    {
        return a_scene && a_scene->IsCompatibleTags(Registry::TagDetails{ a_tag });
    }

    bool IsPetiteFemaleScene(const Registry::Scene* a_scene)
    {
        return HasSceneTag(a_scene, PETITE_ADULT_TAG);
    }

    // ---- Devious Devices rules (SLPP-002) ---------------------------------------------------------------------
    // Devious Devices filters a scene only after it started (zadBQ00.Logic on HookAnimationStart) and, when the
    // scene conflicts with the worn devices, replaces it through the legacy API - which under P+ ends the scene or
    // falls back to a vaginal/anal one through a closed suit. Applying DD's own rules here, while the thread still
    // holds its candidate scenes, means DD finds no conflict and leaves the scene alone.
    struct DeviceKeywords
    {
        RE::BGSKeyword* belt{ nullptr };
        RE::BGSKeyword* suit{ nullptr };
        RE::BGSKeyword* plugVaginal{ nullptr };
        RE::BGSKeyword* plugAnal{ nullptr };
        RE::BGSKeyword* bra{ nullptr };
        RE::BGSKeyword* gag{ nullptr };
        RE::BGSKeyword* gagPanel{ nullptr };
        RE::BGSKeyword* permitVaginal{ nullptr };
        RE::BGSKeyword* permitAnal{ nullptr };
        RE::BGSKeyword* permitOral{ nullptr };
        RE::BGSKeyword* armbinder{ nullptr };
        RE::BGSKeyword* elbowbinder{ nullptr };
        RE::BGSKeyword* elbowTie{ nullptr };
        RE::BGSKeyword* straitJacket{ nullptr };
        RE::BGSKeyword* yoke{ nullptr };
        RE::BGSKeyword* yokeBB{ nullptr };
        RE::BGSKeyword* cuffsFront{ nullptr };
        RE::BGSKeyword* petSuit{ nullptr };
        bool            loaded{ false };

        static const DeviceKeywords& Get()
        {
            static const DeviceKeywords keywords = [] {
                DeviceKeywords k;
                const auto kw = [](const char* a_id) { return RE::TESForm::LookupByEditorID<RE::BGSKeyword>(a_id); };
                k.belt = kw("zad_DeviousBelt");
                k.suit = kw("zad_DeviousSuit");
                k.plugVaginal = kw("zad_DeviousPlugVaginal");
                k.plugAnal = kw("zad_DeviousPlugAnal");
                k.bra = kw("zad_DeviousBra");
                k.gag = kw("zad_DeviousGag");
                k.gagPanel = kw("zad_DeviousGagPanel");
                k.permitVaginal = kw("zad_PermitVaginal");
                k.permitAnal = kw("zad_PermitAnal");
                k.permitOral = kw("zad_PermitOral");
                k.armbinder = kw("zad_DeviousArmbinder");
                k.elbowbinder = kw("zad_DeviousArmbinderElbow");
                k.elbowTie = kw("zad_DeviousElbowTie");
                k.straitJacket = kw("zad_DeviousStraitJacket");
                k.yoke = kw("zad_DeviousYoke");
                k.yokeBB = kw("zad_DeviousYokeBB");
                k.cuffsFront = kw("zad_DeviousCuffsFront");
                k.petSuit = kw("zad_DeviousPetSuit");
                k.loaded = k.belt && k.suit && k.gag && k.permitVaginal && k.permitAnal && k.armbinder && k.yoke;
                logger::info("Device scene filter: Devious Devices keywords {}.", k.loaded ? "found" : "not found, filter off");
                return k;
            }();
            return keywords;
        }
    };

    // the worn armor carrying a_device; nullptr when none (DD: GetWornRenderedDeviceByKeyword)
    RE::TESObjectARMO* WornDevice(RE::Actor* a_actor, RE::BGSKeyword* a_device)
    {
        if (!a_device) {
            return nullptr;
        }
        const auto changes = a_actor->GetInventoryChanges();
        if (!changes || !changes->entryList) {
            return nullptr;
        }
        for (const auto& entry : *changes->entryList) {
            const auto armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
            if (armor && entry->IsWorn() && armor->HasKeyword(a_device)) {
                return armor;
            }
        }
        return nullptr;
    }

    // a worn device of this kind that does not carry the permit keyword (DD: IsBlockedVaginal / IsBlockedAnal)
    bool Blocks(RE::Actor* a_actor, RE::BGSKeyword* a_device, RE::BGSKeyword* a_permit)
    {
        const auto device = WornDevice(a_actor, a_device);
        return device && !(a_permit && device->HasKeyword(a_permit));
    }

    bool Wears(RE::Actor* a_actor, RE::BGSKeyword* a_device)
    {
        return WornDevice(a_actor, a_device) != nullptr;
    }

    struct DeviceRules
    {
        bool             permitVaginal{ true };
        bool             permitAnal{ true };
        bool             permitBoobs{ true };
        bool             permitOral{ true };
        bool             noBindings{ true };
        bool             noPetSuit{ true };
        std::string_view boundTag{};  // the scene tag made for the worn binding: Armbinder / Yoke / Cuffs

        bool Restricted() const { return !permitVaginal || !permitAnal || !permitBoobs || !permitOral || !noBindings || !noPetSuit; }

        std::string ToString() const
        {
            return std::format("vaginal={} anal={} boobs={} oral={} bound={} petsuit={} prefer={}", permitVaginal, permitAnal, permitBoobs, permitOral, !noBindings, !noPetSuit,
                boundTag.empty() ? std::string_view{ "-" } : boundTag);
        }
    };

    DeviceRules GetDeviceRules(const std::vector<RE::Actor*>& a_actors)
    {
        DeviceRules rules;
        const auto& k = DeviceKeywords::Get();
        if (!k.loaded) {
            return rules;
        }
        for (const auto actor : a_actors) {
            if (!actor) {
                continue;
            }
            rules.permitVaginal = rules.permitVaginal && !Blocks(actor, k.belt, k.permitVaginal) && !Blocks(actor, k.suit, k.permitVaginal) && !Blocks(actor, k.plugVaginal, k.permitVaginal);
            rules.permitAnal = rules.permitAnal && !Blocks(actor, k.belt, k.permitAnal) && !Blocks(actor, k.suit, k.permitAnal) && !Blocks(actor, k.plugAnal, k.permitAnal);
            rules.permitBoobs = rules.permitBoobs && !Wears(actor, k.bra) && !Wears(actor, k.suit);
            rules.permitOral = rules.permitOral && !(Wears(actor, k.gag) && !Wears(actor, k.gagPanel) && !Wears(actor, k.permitOral));
            // DD: zadlibs.NeedsBoundAnim
            if (Wears(actor, k.armbinder) || Wears(actor, k.elbowbinder) || Wears(actor, k.elbowTie) || Wears(actor, k.straitJacket)) {
                rules.noBindings = false;
                rules.boundTag = "Armbinder";
            } else if (Wears(actor, k.yoke) || Wears(actor, k.yokeBB)) {
                rules.noBindings = false;
                rules.boundTag = "Yoke";
            } else if (Wears(actor, k.cuffsFront)) {
                rules.noBindings = false;
                rules.boundTag = "Cuffs";
            }
            if (Wears(actor, k.petSuit)) {
                rules.noBindings = false;
                rules.noPetSuit = false;
            }
        }
        return rules;
    }

    // DD: the "does not conflict" test of zadBQ00.Logic (DD never replaces NoSwap / DeviousDevice / Estrus scenes,
    // but a closed belt or suit still rules out vaginal and anal ones)
    bool IsAllowedByDevices(const Registry::Scene* a_scene, const DeviceRules& a_rules)
    {
        const auto has = [&](std::string_view a_tag) { return HasSceneTag(a_scene, a_tag); };
        bool openings = (a_rules.permitVaginal || !(has("Vaginal") || has("Pussy"))) && (a_rules.permitAnal || !has("Anal"));
        if (!a_rules.permitVaginal && !a_rules.permitAnal) {
            // a closed crotch (hobble suit, belt): cunnilingus, fingering, facesitting, 69 or grinding scenes carry no
            // Vaginal/Pussy tag, so only a blowjob or a handjob on the partner is left
            constexpr std::array crotch{ "Cunnilingus"sv, "Fingering"sv, "Masturbation"sv, "Facesit"sv, "Facesitting"sv, "69"sv, "Sixtynine"sv, "Grinding"sv };
            openings = openings && (has("Blowjob") || has("Handjob")) && std::ranges::none_of(crotch, has);
        }
        if (has("NoSwap") || has("DeviousDevice") || has("Estrus")) {
            return openings;
        }
        return openings && (a_rules.permitBoobs || !has("Boobjob")) && (a_rules.permitOral || !has("Oral")) && (a_rules.noBindings || (a_rules.noPetSuit && !has("Handjob")));
    }

    std::string_view SceneProfileName(const Thread::Instance::SceneProfile a_profile)
    {
        switch (a_profile) {
        case Thread::Instance::SceneProfile::PetiteFemale:
            return "PetiteFemale";
        case Thread::Instance::SceneProfile::PetiteMaleSMF:
            return "PetiteMaleSMF";
        case Thread::Instance::SceneProfile::PetiteMaleMSMF:
            return "PetiteMaleMSMF";
        case Thread::Instance::SceneProfile::PetiteMaleSMSMF:
            return "PetiteMaleSMSMF";
        case Thread::Instance::SceneProfile::PetiteMaleSMFF:
            return "PetiteMaleSMFF";
        case Thread::Instance::SceneProfile::Normal:
        default:
            return "Normal";
        }
    }
}

namespace Thread
{
    Instance::Position::Position(RE::BGSRefAlias* alias, RE::Actor* actor, bool submissive, bool dominant) :
      alias(alias), data(actor, submissive)
    {
        const auto library = Registry::Library::GetSingleton();
        if (Settings::bPatchPetite) {
            voice = library->GetVoice(actor, Registry::TagDetails{ IsShortAdultActor(actor) ? PETITE_ADULT_TAG : "-Petite"sv });
            if (!voice) {
                logger::warn("No {} voice found for actor {:X}.", IsShortAdultActor(actor) ? "Petite" : "normal", actor->GetFormID());
            }
        } else {
            voice = library->GetVoice(actor, { "" });
        }
        if (data.IsHuman()) {
            Registry::TagDetails tags{ submissive ? "Victim" : (dominant ? "Aggressor" : "") };
            expression = library->GetExpression(tags);
        } else {
            expression = nullptr;
        }
    }

    Instance::Instance(RE::TESQuest* a_linkedQst, const std::vector<RE::Actor*>& a_submissives, const SceneMapping& a_scenes, FurniturePreference a_furniturepref) :
      linkedQst(a_linkedQst), center(nullptr), scenes({})
    {
        const auto centerAct = InitializeReferences(a_submissives);
        const auto fragments = InitializeScenes(a_scenes, a_furniturepref);
        if (Settings::bPatchPetite) {
            FilterScenesByProfile();
        }
        if (Settings::bPatchDevices) {
            FilterScenesByDevices(a_furniturepref);
        }
        InitializeCenter(centerAct, a_furniturepref);
        if (Instance::pendingQst != nullptr) {
            return;  // pending impl handled by FinalizeCenterRefSelection()
        }
        FinalizeInstanceMake();
    }

    void Instance::FinalizeInstanceMake()
    {
        auto& priorityScenes = scenes[SceneType::Custom].empty() ? scenes[SceneType::Primary] : scenes[SceneType::Custom];
        const auto& centerTy = center.offset.type;
        for (auto&& sceneArr : scenes) {
            const auto removed = std::erase_if(sceneArr, [&](const auto& scene) {
                return !scene->IsCompatibleFurniture(centerTy);
            });
            if (sceneArr.begin() == priorityScenes.begin()) {
                if (sceneArr.empty())
                    throw std::runtime_error("No compatible scenes found for thread.");
                const auto centerName = center.GetRef()->GetDisplayFullName();
                const auto centerId = center.GetRef()->GetFormID();
                const auto totalScenes = sceneArr.size() + removed;
                logger::info("Thread validated. Center: {}, {:X}, Scenes: {}/{} scenes are compatible.", centerName, centerId, sceneArr.size(), totalScenes);
            }
        }
        const auto firstScene = Random::draw(scenes[SceneType::LeadIn].empty() ? priorityScenes : scenes[SceneType::LeadIn]);
        [[maybe_unused]] const auto success = SetActiveScene(firstScene);
        assert(success && "Failed to set active scene.");
    }

    void Instance::FilterScenesByProfile()
    {
        const auto profile = GetSceneProfile();
        size_t totalFiltered = 0;
        for (auto&& sceneArr : scenes) {
            totalFiltered += std::erase_if(sceneArr, [&](const auto& scene) {
                return !IsCompatibleSceneProfile(scene, profile);
            });
        }
        logger::info("Petite adult scene filter: profile={}, removed {} scene(s).", SceneProfileName(profile), totalFiltered);
    }

    Instance::SceneProfile Instance::GetSceneProfile() const
    {
        size_t petiteMaleCount = 0;
        size_t petiteFemaleCount = 0;
        size_t normalMaleCount = 0;
        size_t normalFemaleCount = 0;

        for (const auto& position : positions) {
            const auto actor = position.data.GetActor();
            const auto petite = IsShortAdultActor(actor);
            if (petite && IsMaleActor(actor)) {
                petiteMaleCount++;
            } else if (petite && IsFemaleActor(actor)) {
                petiteFemaleCount++;
            } else if (!petite && IsMaleActor(actor)) {
                normalMaleCount++;
            } else if (!petite && IsFemaleActor(actor)) {
                normalFemaleCount++;
            }
        }

        const auto hasPetite = petiteMaleCount != 0 || petiteFemaleCount != 0;
        const auto hasNormal = normalMaleCount != 0 || normalFemaleCount != 0;
        if (!hasPetite || !hasNormal || (petiteMaleCount != 0 && petiteFemaleCount != 0)) {
            return SceneProfile::Normal;
        }

        if (petiteFemaleCount != 0) {
            return SceneProfile::PetiteFemale;
        }

        if (petiteMaleCount == 1 && normalMaleCount == 0 && normalFemaleCount == 1) {
            return SceneProfile::PetiteMaleSMF;
        }
        if (petiteMaleCount == 1 && normalMaleCount == 1 && normalFemaleCount == 1) {
            return SceneProfile::PetiteMaleMSMF;
        }
        if (petiteMaleCount == 2 && normalMaleCount == 0 && normalFemaleCount == 1) {
            return SceneProfile::PetiteMaleSMSMF;
        }
        if (petiteMaleCount == 1 && normalMaleCount == 0 && normalFemaleCount == 2) {
            return SceneProfile::PetiteMaleSMFF;
        }

        return SceneProfile::Normal;
    }

    bool Instance::IsCompatibleSceneProfile(const Registry::Scene* a_scene, const SceneProfile a_profile) const
    {
        const auto isPetiteFemale = IsPetiteFemaleScene(a_scene);
        switch (a_profile) {
        case SceneProfile::PetiteFemale:
            return isPetiteFemale && !HasSceneTag(a_scene, PETITE_MALE_SMF_TAG) &&
                   !HasSceneTag(a_scene, PETITE_MALE_MSMF_TAG) &&
                   !HasSceneTag(a_scene, PETITE_MALE_SMSMF_TAG) &&
                   !HasSceneTag(a_scene, PETITE_MALE_SMFF_TAG);
        case SceneProfile::PetiteMaleSMF:
            return HasSceneTag(a_scene, PETITE_MALE_SMF_TAG) && !isPetiteFemale;
        case SceneProfile::PetiteMaleMSMF:
            return HasSceneTag(a_scene, PETITE_MALE_MSMF_TAG) && !isPetiteFemale;
        case SceneProfile::PetiteMaleSMSMF:
            return HasSceneTag(a_scene, PETITE_MALE_SMSMF_TAG) && !isPetiteFemale;
        case SceneProfile::PetiteMaleSMFF:
            return HasSceneTag(a_scene, PETITE_MALE_SMFF_TAG) && !isPetiteFemale;
        case SceneProfile::Normal:
        default:
            return !isPetiteFemale && !HasSceneTag(a_scene, PETITE_MALE_SMF_TAG) &&
                   !HasSceneTag(a_scene, PETITE_MALE_MSMF_TAG) &&
                   !HasSceneTag(a_scene, PETITE_MALE_SMSMF_TAG) &&
                   !HasSceneTag(a_scene, PETITE_MALE_SMFF_TAG);
        }
    }

    bool Instance::ShouldPreserveActorScale(RE::Actor* a_actor) const
    {
        const auto isSoloScene = positions.size() == 1;
        return IsShortAdultActor(a_actor) && (isSoloScene || GetSceneProfile() != SceneProfile::Normal);
    }

    void Instance::FilterScenesByDevices(FurniturePreference a_furniturePreference)
    {
        if (!Settings::bDeviceSceneFilter) {
            return;
        }
        std::vector<RE::Actor*> actors;
        std::vector<RE::Actor*> submissives;
        for (const auto& position : positions) {
            const auto actor = position.data.GetActor();
            actors.push_back(actor);
            if (position.data.IsSubmissive()) {
                submissives.push_back(actor);
            }
        }
        const auto rules = GetDeviceRules(actors);
        if (!rules.Restricted()) {
            return;
        }
        const bool wantMade = Settings::bDevicePreferBound && !rules.boundTag.empty();
        const auto allowed = [&](const Registry::Scene* a_scene) { return IsAllowedByDevices(a_scene, rules); };
        const auto made = [&](const Registry::Scene* a_scene) { return HasSceneTag(a_scene, rules.boundTag); };
        // the allowed scenes; with bound arms only the ones made for the binding, when there are any
        const auto narrow = [&](std::vector<const Registry::Scene*> a_list) {
            std::erase_if(a_list, [&](const auto* s) { return !allowed(s); });
            if (wantMade && std::ranges::any_of(a_list, made)) {
                std::erase_if(a_list, [&](const auto* s) { return !made(s); });
            }
            return a_list;
        };

        auto& custom = scenes[SceneType::Custom];
        auto& primary = scenes[SceneType::Primary];
        auto& leadIn = scenes[SceneType::LeadIn];
        const std::array<size_t, 3> before{ primary.size(), leadIn.size(), custom.size() };
        if (!custom.empty()) {
            // a mod asked for these scenes: filter them, but never empty them (DD's own fallback still runs)
            auto kept = narrow(custom);
            if (!kept.empty()) {
                custom = std::move(kept);
            } else {
                logger::warn("Device scene filter: none of the {} forced scenes fits the worn devices, left unchanged.", custom.size());
            }
        } else {
            auto kept = narrow(primary);
            if (kept.empty() || (wantMade && !std::ranges::any_of(kept, made))) {
                // nothing fitting among the caller's scenes: look through every scene registered for these actors
                const auto lib = Registry::Library::GetSingleton();
                auto subm = submissives;
                std::vector<const Registry::Scene*> found;
                do {
                    found = lib->LookupScenes(actors, {}, subm);
                    std::erase_if(found, [&](const auto* s) { return s->RequiresFurniture() && a_furniturePreference == FurniturePreference::Disallow; });
                    found = narrow(found);
                    if (!found.empty() || subm.empty())
                        break;
                    subm.pop_back();
                } while (true);
                if (!found.empty() && (kept.empty() || std::ranges::any_of(found, made))) {
                    kept = std::move(found);
                }
            }
            if (!kept.empty()) {
                primary = std::move(kept);
            } else {
                logger::warn("Device scene filter: no scene fits the worn devices ({}), scenes left unchanged.", rules.ToString());
            }
            // lead-ins play first: the same rules, and with bound arms only the ones made for the binding
            std::erase_if(leadIn, [&](const auto* s) { return !allowed(s) || (wantMade && !made(s)); });
        }
        logger::info("Device scene filter: {} -> scenes [{},{},{}] (were [{},{},{}]).", rules.ToString(), primary.size(), leadIn.size(), custom.size(), before[0], before[1],
            before[2]);
    }

    RE::Actor* Instance::InitializeReferences(const std::vector<RE::Actor*>& a_submissives)
    {
        RE::Actor* centerAct{ nullptr };
        linkedQst->aliasAccessLock.LockForRead();
        for (auto&& alias : linkedQst->aliases) {
            const auto aliasref = alias ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
            if (!aliasref)
                continue;
            if (alias->aliasName == CENTER_REF_NAME) {
                center = { aliasref };
            } else if (const auto ref = aliasref->GetActorReference()) {
                positions.emplace_back(aliasref, ref, std::ranges::contains(a_submissives, ref), !a_submissives.empty());
                if (ref->IsPlayerRef() || !centerAct) {
                    centerAct = ref;
                }
            }
        }
        linkedQst->aliasAccessLock.UnlockForRead();
        const auto centerId = center.GetRef() ? center.GetRef()->GetFormID() : 0;
        const auto centerName = center.GetRef() ? center.GetRef()->GetDisplayFullName() : "None";
        logger::info("Thread initialized. Center: {}, {:X}, Actors: {}.", centerName, centerId, positions.size());
        return centerAct;
    }

    std::vector<Registry::ActorFragment> Instance::InitializeScenes(const SceneMapping& a_scenes, FurniturePreference a_furniturepref)
    {
        logger::info("Initializing scenes: [{},{},{}].", a_scenes[SceneType::Primary].size(), a_scenes[SceneType::LeadIn].size(), a_scenes[SceneType::Custom].size());
        const auto fragments = std::ranges::fold_left(positions, std::vector<Registry::ActorFragment>{}, [&](auto&& acc, const auto& it) {
            return (acc.push_back(it.data), acc);
        });
        for (size_t i = 0; i < SceneType::Total; i++) {
            scenes[i] = std::ranges::fold_left(a_scenes[i], std::vector<const Registry::Scene*>{}, [&](auto&& acc, const Registry::Scene* it) {
                if (it->FindAssignments(fragments).empty()) {
                    logger::warn("Scene {}, {} has no assignments.", it->id, it->name);
                    return acc;
                } else if (it->RequiresFurniture() && a_furniturepref == FurniturePreference::Disallow) {
                    logger::warn("Scene {}, {} requires furniture, but furniture is disallowed.", it->id, it->name);
                    return acc;
                }
                acc.push_back(it);
                return acc;
            });
            if (i == SceneType::Primary && scenes[i].empty()) {
                logger::warn("No primary scenes found for thread.");
                const auto lib = Registry::Library::GetSingleton();
                auto [pos, subm] = std::ranges::fold_left(positions, std::pair{ std::vector<RE::Actor*>(), std::vector<RE::Actor*>() }, [&](auto&& acc, const auto& it) {
                    const auto actor = it.data.GetActor();
                    if (it.data.IsSubmissive()) {
                        acc.second.push_back(actor);
                    }
                    acc.first.push_back(actor);
                    return acc;
                });
                do {
                    scenes[i] = lib->LookupScenes(pos, {}, subm);
                    if (!scenes[i].empty())
                        break;
                    if (subm.empty())
                        break;
                    subm.pop_back();
                } while (true);
            }
        }
        logger::info("Scenes initialized: [{},{},{}].", scenes[SceneType::Primary].size(), scenes[SceneType::LeadIn].size(), scenes[SceneType::Custom].size());
        return fragments;
    }

    std::vector<const Registry::Scene*>& Instance::InitializeCenter(RE::Actor* centerAct, FurniturePreference furniturePreference)
    {
        auto& prioScenes = scenes[SceneType::Custom].empty() ? scenes[SceneType::Primary] : scenes[SceneType::Custom];
        if (prioScenes.empty()) {
            throw std::runtime_error("No primary scenes found for thread.");
        }
        const auto sceneTypes = std::ranges::fold_left(prioScenes, REX::EnumSet{ Registry::FurnitureType::None }, [](auto&& acc, const auto& it) {
            return acc | it->GetFurnitureTypes();
        });
        FurnitureMapping furnitureMap;
        std::promise<bool> promise;
        auto future = promise.get_future();
        const auto selectionMethod = GetSelectionMethod(furniturePreference);
        SKSE::GetTaskInterface()->AddTask([&]() mutable {
            if (((bool (*)(void))Offsets::NotOnGameThread.address())()) {
                logger::error("Task is not on valid thread, this should never happen and can cause random CTD/freezes");
            }
            try {
                if (center.GetRef() && InitializeFixedCenter(centerAct, prioScenes, sceneTypes)) {
                    logger::info("Using fixed center {:X} with offset {}.", center.GetRef()->GetFormID(), center.offset.type.ToString());
                } else if (sceneTypes == Registry::FurnitureType::None) {
                    logger::info("No Furniture scenes found in thread. Using actor {:X} as center.", centerAct->GetFormID());
                    center.SetReference(centerAct, {});
                } else if (selectionMethod == CenterSelection::Actor) {
                    logger::info("Using actor {:X} as center.", centerAct->GetFormID());
                    center.SetReference(centerAct, {});
                } else {
                    furnitureMap = GetUniqueFurnituesOfTypeInBound(centerAct, sceneTypes);
                    promise.set_value(false);
                    return;
                }
            } catch (const std::exception& e) {
                logger::error("Thread initialization failed: {}", e.what());
            }
            promise.set_value(true);
        });
        future.wait();
        if (future.get()) {
            return prioScenes;
        } else if (furnitureMap.empty()) {
            logger::info("No furniture found in range. Using actor {:X} as center.", centerAct->GetFormID());
            center.SetReference(centerAct, {});
        } else if (selectionMethod == CenterSelection::SelectionMenu) {
            InitializeCenterRefMenu(furnitureMap, centerAct);
        } else {
            const auto [ref, type] = furnitureMap.front();
            center.SetReference(ref, type);
            if (type.type.Is(Registry::FurnitureType::None)) {
                logger::info("Using actor {:X} as center.", centerAct->GetFormID());
            } else {
                logger::info("Using furniture {:X} with offset {} as center.", ref->GetFormID(), type.type.ToString());
            }
        }
        return prioScenes;
    }

    bool Instance::InitializeFixedCenter(RE::Actor* centerAct, std::vector<const Registry::Scene*>& prioScenes, REX::EnumSet<Registry::FurnitureType::Value> sceneTypes)
    {
        const auto& details = center.details = Registry::Library::GetSingleton()->GetFurnitureDetails(center.GetRef());
        if (((bool (*)(void))Offsets::NotOnGameThread.address())()) {
            logger::error("Initialize Fixed Center called on wrong thread, not executing!");
            return false;
        } else {
            auto inBounds = details ? details->GetClosestCoordinatesInBound(center.GetRef(), sceneTypes, centerAct) : std::vector<Registry::FurnitureOffset>{};

            for (auto i = inBounds.begin(); i < inBounds.end(); i++) {
                if (std::ranges::any_of(prioScenes, [type = i->type](const auto& scene) { return scene->IsCompatibleFurniture(type); })) {
                    center.offset = *i;
                    return true;
                }
            }
            if (std::ranges::any_of(prioScenes, [](const auto& scene) { return scene->IsCompatibleFurniture(Registry::FurnitureType::None); })) {
                center.offset = { Registry::FurnitureType::None, {} };
                return true;
            }
            logger::warn("Center reference {:X} is not compatible with any scene.", center.GetRef()->GetFormID());
            return false;
        }
    }

    Instance::CenterSelection Instance::GetSelectionMethod(FurniturePreference furniturePreference)
    {
        if (furniturePreference == FurniturePreference::Disallow) {
            return CenterSelection::Actor;
        } else if (furniturePreference == FurniturePreference::Prefer) {
            return CenterSelection::Furniture;
        }
        const auto pickRandom = []() {
            return Random::draw<float>(0.0f, 1.0f) < Settings::fFurniturePreference ? CenterSelection::Furniture : CenterSelection::Actor;
        };
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto position = GetPosition(player);
        if (!position) {
            switch (Settings::FurnitureSlection(Settings::iNPCBed)) {
            case Settings::FurnitureSlection::Never:
                return CenterSelection::Actor;
            case Settings::FurnitureSlection::Always:
                return CenterSelection::Furniture;
            case Settings::FurnitureSlection::Sometimes:
                return pickRandom();
            default:
                logger::error("Invalid furniture selection setting (npc): {}", Settings::iNPCBed);
                return CenterSelection::Actor;
            }
        } else {
            switch (Settings::FurnitureSlection(Settings::iAskBed)) {
            case Settings::FurnitureSlection::Never:
                return CenterSelection::Actor;
            case Settings::FurnitureSlection::Always:
                return CenterSelection::Furniture;
            case Settings::FurnitureSlection::AskAlways:
                return CenterSelection::SelectionMenu;
            case Settings::FurnitureSlection::IfNotSubmissive:
                if (!position->data.IsSubmissive()) {
                    return CenterSelection::SelectionMenu;
                }
                __fallthrough;
            case Settings::FurnitureSlection::Sometimes:
                return pickRandom();
            default:
                logger::error("Invalid furniture selection setting (player): {}", Settings::iAskBed);
                return CenterSelection::Actor;
            }
        }
    }

    Instance::FurnitureMapping Instance::GetUniqueFurnituesOfTypeInBound(RE::Actor* a_centerAct, REX::EnumSet<Registry::FurnitureType::Value> a_furnitureTypes)
    {
        std::vector<RE::TESObjectREFR*> inUseFurniture{};
        const auto processlist = RE::ProcessLists::GetSingleton();
        for (auto&& handle : processlist->highActorHandles) {
            const auto it = handle.get().get();
            if (!it || GetPosition(it))
                continue;
            if (const auto furni = it->GetOccupiedFurniture().get()) {
                inUseFurniture.push_back(furni.get());
            }
        }
        std::vector<std::pair<RE::TESObjectREFR*, glm::vec4>> raycastStart{};
        for (auto&& p : positions) {
            auto act = p.data.GetActor();
            assert(act);
            auto head = act->GetNodeByName("NPC Head [Head]");
            if (!head)
                continue;
            auto& t = head->world.translate;
            raycastStart.emplace_back(act, glm::vec4{ t.x, t.y, t.z, 0.0f });
        }
        FurnitureMapping retVal{};
        const auto library = Registry::Library::GetSingleton();
        Util::ForEachObjectInRange(a_centerAct, Settings::fFurnitureScanRadius, [&](RE::TESObjectREFR* a_ref) {
            if (std::ranges::contains(inUseFurniture, a_ref)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto details = library->GetFurnitureDetails(a_ref);
            if (!details || details->GetTypes().none(a_furnitureTypes.get())) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto coordinates = details->GetClosestCoordinatesInBound(a_ref, a_furnitureTypes, a_centerAct);
            if (coordinates.empty()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            auto obj = a_ref->Get3D();
            auto node = obj ? obj->AsNode() : nullptr;
            auto box = node ? ObjectBound::MakeBoundingBox(node) : std::nullopt;
            if (!box) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto endPoint = glm::vec4(box->GetCenterWorld(), 0.0f);
            const auto isReachable = std::ranges::any_of(raycastStart, [&](auto&& it) {
                auto [startRef, startPoint] = it;
                std::vector<RE::NiAVObject*> filterList{ a_ref->Get3D(), startRef->Get3D() };
                do {
                    auto res = Raycast::hkpCastRay(startPoint, endPoint, filterList);
                    if (!res.hit) {
                        return true;
                    }
                    auto hitRef = res.hitObject ? res.hitObject->GetUserData() : nullptr;
                    auto base = hitRef ? hitRef->GetBaseObject() : nullptr;
                    if (!base || base->Is(RE::FormType::Static, RE::FormType::MovableStatic, RE::FormType::Furniture)) {
                        break;
                    }
                    if (base->Is(RE::FormType::Door) && hitRef->IsLocked()) {
                        break;
                    }
                    filterList.push_back(res.hitObject);
                    startPoint = res.hitPos;
                } while (true);
                return false;
            });
            if (isReachable) {
                for (auto&& coordinate : coordinates) {
                    retVal.emplace_back(a_ref, coordinate);
                }
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        return retVal;
    }

    void Instance::InitializeCenterRefMenu(const FurnitureMapping& a_furnitures, RE::Actor* a_tmpCenter)
    {
        std::vector<Interface::FurnSelectMenu::Item> items;
        const auto actName = std::format("{}", a_tmpCenter->GetDisplayFullName());
        const auto actID = std::format("0x{:X}", a_tmpCenter->GetFormID());
        items.emplace_back(actName, "", actID);
        for (const auto& [ref, offset] : a_furnitures) {
            const auto itemName = std::format("{}", ref->GetDisplayFullName());
            const auto itemType = std::format("{}", offset.type.ToString());
            const auto itemID = std::format("0x{:X}", ref->GetFormID());
            items.emplace_back(itemName, itemType, itemID);
        }
        Instance::pendingFurnitureMap = a_furnitures;
        Instance::pendingCenterAct = a_tmpCenter;
        Instance::pendingQst = linkedQst;
        Interface::FurnSelectMenu::GetSingleton().Open(linkedQst, items);
    }

    void Instance::SetCenterRefSelected(size_t a_index)
    {
        // Called by Interface::FurnSelectMenu
        if (a_index == 0 || a_index > pendingFurnitureMap.size()) {
            logger::info("SetCenterRefSelected: using actor {:X} as center.", Instance::pendingCenterAct->GetFormID());
            center.SetReference(Instance::pendingCenterAct, {});
        } else {
            const auto [selectedRef, selectedType] = pendingFurnitureMap.at(a_index - 1);
            center.SetReference(selectedRef, selectedType);
            if (selectedType.type.Is(Registry::FurnitureType::None)) {
                logger::info("SetCenterRefSelected: using actor {:X} as center.", pendingCenterAct->GetFormID());
            } else {
                logger::info("SetCenterRefSelected: using furniture {:X} with offset {} as center.", selectedRef->GetFormID(), selectedType.type.ToString());
            }
        }
        Instance::pendingFurnitureMap.clear();
        Instance::pendingCenterAct = nullptr;
        const auto qst = Instance::pendingQst;
        Instance::pendingQst = nullptr;
        FinalizeCenterRefSelection(qst);
    }

    void Instance::FinalizeCenterRefSelection(RE::TESQuest* a_linkedQst)
    {
        std::thread([a_linkedQst]() {
            std::unique_ptr<Instance> instance{};
            {
                std::unique_lock lock{ _mInstances };
                const auto it = std::ranges::find_if(pendingInstances, [a_linkedQst](const auto& i) { return i->linkedQst == a_linkedQst; });
                if (it == pendingInstances.end()) {
                    logger::error("FinalizeCenterRefSelection: no pending instance found for TESQuest {:X}.", a_linkedQst->formID);
                    DispatchContinueSetup(a_linkedQst, false);
                    return;
                }
                instance = std::move(*it);
                pendingInstances.erase(it);
            }
            try {
                instance->FinalizeInstanceMake();
                std::unique_lock lock{ _mInstances };
                instances.emplace_back(std::move(instance));
                DispatchContinueSetup(a_linkedQst, true);
            } catch (const std::exception& e) {
                logger::error("FinalizeCenterRefSelection: Failed to complete thread instance: {}", e.what());
                DispatchContinueSetup(a_linkedQst, false);
            }
        }).detach();
    }

}  // namespace Thread
