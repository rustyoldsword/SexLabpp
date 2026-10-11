#include "ClothingPanel.h"

namespace Thread::Interface
{
    using Registry::GroupRule;
    using Registry::RedressRule;
    using Registry::Scope;
    using Registry::StripRule;
    using Registry::Wardrobe;
    using Registry::WardrobeGroup;
    using UI::SetWindowFontSize;
    using ImGuiMCP::ImVec2;
    namespace dl = ImGuiMCP::ImDrawListManager;

    namespace
    {
        constexpr size_t kGroupCount = static_cast<size_t>(WardrobeGroup::Total);

        // Icon of each group, same order as WardrobeGroup (Data\Interface\SexLab\Clothing\<id>.png)
        constexpr std::array<std::string_view, kGroupCount> kGroupIcons{
            "group_head", "group_neck", "group_body", "group_underwear", "group_hands", "group_rings",
            "group_feet", "group_legs", "group_back", "group_shield", "group_weapons", "group_other"
        };

        const char* ScopeName(Scope a_scope)
        {
            switch (a_scope) {
            case Scope::Scene:
                return "this scene";
            case Scope::Actor:
                return "this character";
            default:
                return "everyone";
            }
        }

        std::string WornText(RE::Actor* a_actor, WardrobeGroup a_group)
        {
            std::string text;
            for (const auto& worn : Wardrobe::GetSingleton()->GetWorn(a_actor)) {
                if (worn.group != a_group)
                    continue;
                if (!text.empty())
                    text += ", ";
                text += worn.item->GetName();
            }
            return text;
        }

        // Textures are loaded once; a missing file stays null and the control falls back to text
        ImGuiMCP::ImTextureID Texture(std::string_view a_id)
        {
            static std::unordered_map<std::string, ImGuiMCP::ImTextureID> cache;
            std::string key{ a_id };
            auto it = cache.find(key);
            if (it == cache.end()) {
                const auto path = std::format("Data/Interface/SexLab/Clothing/{}.png", key);
                ImGuiMCP::ImTextureID texture = nullptr;
                if (std::filesystem::exists(std::filesystem::current_path() / path)) {
                    texture = SKSEMenuFramework::LoadTexture(path);
                }
                it = cache.emplace(std::move(key), texture).first;
            }
            return it->second;
        }

        // Text cut to a_width, with "..." when it does not fit
        std::string Fit(const std::string& a_text, float a_width)
        {
            if (a_text.empty() || ImGuiMCP::CalcTextSize(a_text.c_str()).x <= a_width)
                return a_text;
            std::string cut = a_text;
            while (!cut.empty() && ImGuiMCP::CalcTextSize((cut + "...").c_str()).x > a_width)
                cut.pop_back();
            return cut + "...";
        }

        // Square icon toggle; the selected one gets a lit plate and an accent border
        bool IconToggle(const char* a_id, std::string_view a_icon, const char* a_fallback, bool a_selected, float a_size, const char* a_tooltip)
        {
            using namespace UI::Theme;
            ImGuiMCP::PushID(a_id);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, ToVec4(a_selected ? Color.buttonSelected : Color.transparent));
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, ToVec4(Color.buttonHovered));
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, ToVec4(Color.buttonPressed));
            bool clicked;
            if (const auto texture = Texture(a_icon)) {
                const auto tint = ToVec4(a_selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 95));
                clicked = ImGuiMCP::ImageButton("##btn", texture, ImVec2{ a_size, a_size }, ImVec2{ 0, 0 }, ImVec2{ 1, 1 }, ImGuiMCP::ImVec4{ 0, 0, 0, 0 }, tint);
            } else {
                const float side = a_size + ImGuiMCP::GetStyle()->FramePadding.y * 2.0f;
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ToVec4(a_selected ? Color.textPrimary : Color.textMuted));
                clicked = ImGuiMCP::Button(a_fallback, ImVec2{ 0.0f, side });
                ImGuiMCP::PopStyleColor();
            }
            ImGuiMCP::PopStyleColor(3);
            if (a_selected) {
                dl::AddRect(ImGuiMCP::GetWindowDrawList(), ImGuiMCP::GetItemRectMin(), ImGuiMCP::GetItemRectMax(), Color.borderActive,
                    Geometry.roundingSmall, 0, 1.5f);
            }
            if (ImGuiMCP::IsItemHovered())
                ImGuiMCP::SetTooltip("%s", a_tooltip);
            ImGuiMCP::PopID();
            return clicked;
        }

        // Button with an icon in front of its label (selectable = stays lit while selected)
        bool IconLabelButton(const char* a_id, std::string_view a_icon, const char* a_label, bool a_selectable, bool a_selected, ImVec2 a_size)
        {
            using namespace UI::Theme;
            ImGuiMCP::PushID(a_id);
            const bool clicked = a_selectable ? UI::SelectableButton("##btn", a_selected, 0, a_size) : ImGuiMCP::Button("##btn", a_size);
            const auto min = ImGuiMCP::GetItemRectMin();
            const auto max = ImGuiMCP::GetItemRectMax();
            auto* list = ImGuiMCP::GetWindowDrawList();
            const float h = max.y - min.y;
            const float icon = std::floor(h * 0.70f);
            const float gap = std::floor(h * 0.25f);
            const auto texture = Texture(a_icon);
            const auto textSize = ImGuiMCP::CalcTextSize(a_label);
            const float content = (texture ? icon + gap : 0.0f) + textSize.x;
            float x = std::floor(min.x + ((max.x - min.x) - content) * 0.5f);
            const auto col = (a_selected || !a_selectable) ? Color.textPrimary : Color.textSecondary;
            if (texture) {
                const float y = std::floor(min.y + (h - icon) * 0.5f);
                dl::AddImage(list, texture, ImVec2{ x, y }, ImVec2{ x + icon, y + icon }, ImVec2{ 0, 0 }, ImVec2{ 1, 1 },
                    (a_selected || !a_selectable) ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 150));
                x += icon + gap;
            }
            dl::AddText(list, ImVec2{ x, std::floor(min.y + (h - textSize.y) * 0.5f) }, col, a_label);
            if (a_selected)
                dl::AddRect(list, min, max, Color.borderActive, Geometry.roundingSmall, 0, 1.5f);
            ImGuiMCP::PopID();
            return clicked;
        }

        void MutedText(const char* a_text)
        {
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color.textMuted), "%s", a_text);
        }
    }

    // Column layout, in unscaled pixels from the left edge of the panel
    namespace Layout
    {
        constexpr float panelW = 540.0f;
        constexpr float rowH = 28.0f;
        constexpr float groupIcon = 18.0f;
        constexpr float button = 18.0f;    // icon size inside a toggle
        constexpr float buttonPad = 3.0f;  // frame padding around it
        constexpr float buttonGap = 2.0f;
        constexpr float colStrip = 128.0f;
        constexpr float colRedress = 222.0f;
        constexpr float colHeels = 316.0f;
        constexpr float colWorn = 350.0f;
    }

    void ClothingPanel::Open(SceneHUD&)
    {
        _actorIndex = 0;
    }

    void ClothingPanel::Close() {}

    void ClothingPanel::OnSetRule(SceneHUD& a_hud, RE::Actor* a_actor, WardrobeGroup a_group, const GroupRule& a_rule)
    {
        auto* wardrobe = Wardrobe::GetSingleton();
        wardrobe->SetRule(a_actor, _scope, a_group, a_rule);
        // A rule for everyone applies to every actor of the scene, so act on all of them right away
        if (_scope == Scope::Global) {
            if (auto* inst = a_hud.GetThreadInstance()) {
                for (auto* actor : inst->GetActors())
                    wardrobe->ApplyLive(actor, a_group);
                return;
            }
        }
        wardrobe->ApplyLive(a_actor, a_group);
    }

    void ClothingPanel::OnUndressAll(RE::Actor* a_actor)
    {
        auto* wardrobe = Wardrobe::GetSingleton();
        for (size_t i = 0; i < kGroupCount; i++) {
            const auto group = static_cast<WardrobeGroup>(i);
            if (group == WardrobeGroup::Weapons)
                continue;
            auto rule = wardrobe->GetRule(a_actor, Scope::Scene, group);
            rule.strip = StripRule::Strip;
            wardrobe->SetRule(a_actor, Scope::Scene, group, rule);
            wardrobe->ApplyLive(a_actor, group);
        }
    }

    void ClothingPanel::OnDressAll(RE::Actor* a_actor)
    {
        auto* wardrobe = Wardrobe::GetSingleton();
        for (size_t i = 0; i < kGroupCount; i++) {
            const auto group = static_cast<WardrobeGroup>(i);
            auto rule = wardrobe->GetRule(a_actor, Scope::Scene, group);
            rule.strip = StripRule::Keep;
            wardrobe->SetRule(a_actor, Scope::Scene, group, rule);
            wardrobe->ApplyLive(a_actor, group);
        }
    }

    void ClothingPanel::RenderGroupRow(SceneHUD& a_hud, RE::Actor* a_actor, WardrobeGroup a_group)
    {
        using namespace UI::Theme;
        auto& scale = a_hud.GetScale();
        auto* wardrobe = Wardrobe::GetSingleton();
        const auto rule = wardrobe->GetRule(a_actor, _scope, a_group);
        const auto effective = wardrobe->Resolve(a_actor);
        const auto origin = wardrobe->Origin(a_actor, a_group);
        const auto worn = WornText(a_actor, a_group);
        const bool hasRemoved = wardrobe->HasRemoved(a_actor, a_group);
        const bool onBody = !worn.empty();

        ImGuiMCP::PushID(static_cast<int>(a_group));
        const float rowH = scale.Px(Layout::rowH);
        const float padH = scale.Px(Spacing.lg);
        const float btn = scale.Px(Layout::button);
        const float btnGap = scale.Px(Layout::buttonGap);

        // Zebra plate behind every second row, full panel width
        const auto rowTop = ImGuiMCP::GetCursorScreenPos();
        const auto winPos = ImGuiMCP::GetWindowPos();
        if (_row++ % 2 == 0) {
            dl::AddRectFilled(ImGuiMCP::GetWindowDrawList(), ImVec2{ winPos.x, rowTop.y }, ImVec2{ winPos.x + ImGuiMCP::GetWindowWidth(), rowTop.y + rowH },
                Color.nestedSurface, 0.0f, 0);
        }
        const float baseY = ImGuiMCP::GetCursorPosY();
        const float framePad = scale.Px(Layout::buttonPad);
        const float buttonY = baseY + (rowH - (btn + framePad * 2.0f)) * 0.5f;

        // Group icon (lit when something of this group is on the body) + name
        const float iconSize = scale.Px(Layout::groupIcon);
        ImGuiMCP::SetCursorPos(ImVec2{ padH, baseY + (rowH - iconSize) * 0.5f });
        const auto groupTexture = Texture(kGroupIcons[static_cast<size_t>(a_group)]);
        if (groupTexture) {
            ImGuiMCP::Image(groupTexture, ImVec2{ iconSize, iconSize }, ImVec2{ 0, 0 }, ImVec2{ 1, 1 },
                ToVec4(onBody ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 90)));
            ImGuiMCP::SameLine(0.0f, scale.Px(Spacing.sm));
        }
        ImGuiMCP::SetCursorPosY(baseY + (rowH - ImGuiMCP::GetTextLineHeight()) * 0.5f);
        ImGuiMCP::TextColored(ToVec4(onBody ? Color.textPrimary : Color.textSecondary), "%s", Wardrobe::GroupLabel(a_group));

        // Where the active rule comes from, if it is not the layer being edited, and what the panel took off
        std::string note;
        const auto effectiveRule = effective.Strip(a_group);
        if (effectiveRule != StripRule::Auto && _scope != origin)
            note = std::format("Set for {}: {}", ScopeName(origin), effectiveRule == StripRule::Strip ? "take off" : "keep on");
        if (hasRemoved)
            note += note.empty() ? "Taken off by this panel" : "\nTaken off by this panel";
        if (!note.empty() && ImGuiMCP::IsItemHovered())
            ImGuiMCP::SetTooltip("%s", note.c_str());

        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImVec2{ framePad, framePad });

        // In the scene: [Auto | Off | On]
        struct StripOption
        {
            const char* id;
            std::string_view icon;
            const char* fallback;
            StripRule rule;
            const char* tooltip;
        };
        constexpr std::array<StripOption, 3> strip{ {
            { "sa", "strip_auto", "Auto", StripRule::Auto, "In the scene: SexLab decides (the stage and your SexLab strip settings)" },
            { "so", "strip_off", "Off", StripRule::Strip, "In the scene: always take it off" },
            { "sk", "strip_on", "On", StripRule::Keep, "In the scene: never take it off" },
        } };
        ImGuiMCP::SetCursorPos(ImVec2{ scale.Px(Layout::colStrip), buttonY });
        for (size_t i = 0; i < strip.size(); i++) {
            if (i > 0)
                ImGuiMCP::SameLine(0.0f, btnGap);
            if (IconToggle(strip[i].id, strip[i].icon, strip[i].fallback, rule.strip == strip[i].rule, btn, strip[i].tooltip)) {
                GroupRule updated = rule;
                updated.strip = strip[i].rule;
                OnSetRule(a_hud, a_actor, a_group, updated);
            }
        }

        // After the scene: [Auto | Put back | Stay off]
        struct RedressOption
        {
            const char* id;
            std::string_view icon;
            const char* fallback;
            RedressRule rule;
            const char* tooltip;
        };
        constexpr std::array<RedressOption, 3> redress{ {
            { "ra", "redress_auto", "Auto", RedressRule::Auto, "After the scene: follow the SexLab redress setting" },
            { "rp", "redress_put_back", "Put back", RedressRule::Redress, "After the scene: always put it back on" },
            { "rn", "redress_stay_off", "Stay off", RedressRule::Never, "After the scene: leave it off" },
        } };
        ImGuiMCP::SetCursorPos(ImVec2{ scale.Px(Layout::colRedress), buttonY });
        for (size_t i = 0; i < redress.size(); i++) {
            if (i > 0)
                ImGuiMCP::SameLine(0.0f, btnGap);
            if (IconToggle(redress[i].id, redress[i].icon, redress[i].fallback, rule.redress == redress[i].rule, btn, redress[i].tooltip)) {
                GroupRule updated = rule;
                updated.redress = redress[i].rule;
                OnSetRule(a_hud, a_actor, a_group, updated);
            }
        }

        // Feet only: keep high heels, take off other shoes
        if (a_group == WardrobeGroup::Feet) {
            ImGuiMCP::SetCursorPos(ImVec2{ scale.Px(Layout::colHeels), buttonY });
            if (IconToggle("hh", "heels_only", "Heels", rule.heelsOnly, btn, "Keep high heels on, take off any other shoes")) {
                GroupRule updated = rule;
                updated.heelsOnly = !rule.heelsOnly;
                OnSetRule(a_hud, a_actor, a_group, updated);
            }
        }
        ImGuiMCP::PopStyleVar();

        // What is worn right now (full list on hover)
        if (onBody) {
            const float wornX = scale.Px(Layout::colWorn);
            ImGuiMCP::SetCursorPos(ImVec2{ wornX, baseY + (rowH - ImGuiMCP::GetTextLineHeight()) * 0.5f });
            const auto shown = Fit(worn, scale.Px(Layout::panelW) - wornX - padH);
            MutedText(shown.c_str());
            if (shown != worn && ImGuiMCP::IsItemHovered())
                ImGuiMCP::SetTooltip("%s", worn.c_str());
        }

        ImGuiMCP::SetCursorPos(ImVec2{ 0.0f, baseY + rowH });
        ImGuiMCP::Dummy(ImVec2{ 0.0f, 0.0f });
        ImGuiMCP::PopID();
    }

    void ClothingPanel::Render(SceneHUD& a_hud)
    {
        using namespace UI::Theme;
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        const auto& actors = inst->GetActors();
        if (actors.empty())
            return;
        if (_actorIndex >= actors.size())
            _actorIndex = 0;
        auto* actor = actors[_actorIndex];

        auto& scale = a_hud.GetScale();
        auto* io = ImGuiMCP::GetIO();
        const float panelW = scale.Px(Layout::panelW);
        const float offset = scale.Px(Geometry.panelTabWidth + Geometry.panelTabGap);
        const float padH = scale.Px(Spacing.lg);
        const float gap = scale.Px(Spacing.sm);

        ImGuiMCP::SetNextWindowPos(
            ImVec2{ io->DisplaySize.x - offset, io->DisplaySize.y * 0.5f },
            ImGuiMCP::ImGuiCond_Always, ImVec2{ 1.0f, 0.5f });
        ImGuiMCP::SetNextWindowSizeConstraints(
            ImVec2{ panelW, scale.Px(28.0f) },
            ImVec2{ panelW, io->DisplaySize.y * 0.85f });
        ImGuiMCP::SetNextWindowSize(ImVec2{ panelW, 0.0f }, ImGuiMCP::ImGuiCond_Always);

        constexpr auto kFlags =
            ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
            ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
            ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;

        if (!ImGuiMCP::Begin("##slpp_CLO", nullptr, kFlags)) {
            ImGuiMCP::End();
            return;
        }
        SetWindowFontSize(scale.TextPx(FontSize.body));
        const float innerW = panelW - padH * 2.0f;

        // --- Who ---
        ImGuiMCP::SetCursorPosX(padH);
        const float chipW = (innerW - gap * static_cast<float>(actors.size() - 1)) / static_cast<float>(actors.size());
        for (size_t i = 0; i < actors.size(); i++) {
            if (i > 0)
                ImGuiMCP::SameLine(0.0f, gap);
            ImGuiMCP::PushID(static_cast<int>(i));
            if (UI::SelectableButton(actors[i]->GetDisplayFullName(), i == _actorIndex, 0, ImVec2{ chipW, scale.Px(26.0f) })) {
                _actorIndex = i;
                actor = actors[i];
            }
            ImGuiMCP::PopID();
        }

        // --- Where changes are stored ---
        ImGuiMCP::Dummy(ImVec2{ 0.0f, gap });
        ImGuiMCP::SetCursorPosX(padH);
        ImGuiMCP::TextColored(ToVec4(Color.textSecondary), "Remember for");
        struct ScopeOption
        {
            const char* id;
            std::string_view icon;
            const char* label;
            Scope scope;
        };
        constexpr std::array<ScopeOption, 3> scopes{ {
            { "s_scene", "scope_scene", "This scene", Scope::Scene },
            { "s_actor", "scope_actor", "This character", Scope::Actor },
            { "s_all", "scope_everyone", "Everyone", Scope::Global },
        } };
        ImGuiMCP::SetCursorPosX(padH);
        const float thirdW = (innerW - gap * 2.0f) / 3.0f;
        for (size_t i = 0; i < scopes.size(); i++) {
            if (i > 0)
                ImGuiMCP::SameLine(0.0f, gap);
            if (IconLabelButton(scopes[i].id, scopes[i].icon, scopes[i].label, true, _scope == scopes[i].scope, ImVec2{ thirdW, scale.Px(28.0f) }))
                _scope = scopes[i].scope;
        }

        // --- Quick actions ---
        ImGuiMCP::Dummy(ImVec2{ 0.0f, gap });
        ImGuiMCP::SetCursorPosX(padH);
        if (IconLabelButton("a_undress", "action_undress", "Undress", false, false, ImVec2{ thirdW, scale.Px(28.0f) }))
            OnUndressAll(actor);
        ImGuiMCP::SameLine(0.0f, gap);
        if (IconLabelButton("a_dress", "action_dress", "Dress", false, false, ImVec2{ thirdW, scale.Px(28.0f) }))
            OnDressAll(actor);
        ImGuiMCP::SameLine(0.0f, gap);
        if (IconLabelButton("a_reset", "action_reset", "Reset", false, false, ImVec2{ thirdW, scale.Px(28.0f) }))
            Wardrobe::GetSingleton()->ResetRules(actor, _scope);
        if (ImGuiMCP::IsItemHovered())
            ImGuiMCP::SetTooltip("Forget the rules saved for %s", ScopeName(_scope));

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Separator, Color.nestedSeparator);
        ImGuiMCP::Dummy(ImVec2{ 0.0f, gap });
        ImGuiMCP::Separator();
        ImGuiMCP::PopStyleColor();

        // --- Column titles ---
        SetWindowFontSize(scale.TextPx(FontSize.caption));
        const float headY = ImGuiMCP::GetCursorPosY() + scale.Px(Spacing.xs);
        ImGuiMCP::SetCursorPos(ImVec2{ padH, headY });
        MutedText("Clothing");
        ImGuiMCP::SetCursorPos(ImVec2{ scale.Px(Layout::colStrip), headY });
        MutedText("In the scene");
        ImGuiMCP::SetCursorPos(ImVec2{ scale.Px(Layout::colRedress), headY });
        MutedText("After the scene");
        ImGuiMCP::SetCursorPos(ImVec2{ scale.Px(Layout::colWorn), headY });
        MutedText("Worn now");
        SetWindowFontSize(scale.TextPx(FontSize.body));
        ImGuiMCP::Dummy(ImVec2{ 0.0f, scale.Px(Spacing.xxs) });

        // --- Groups ---
        _row = 0;
        for (size_t i = 0; i < kGroupCount; i++) {
            const auto group = static_cast<WardrobeGroup>(i);
            // "Other" only matters if the actor actually wears something unclassified
            if (group == WardrobeGroup::Other && !Wardrobe::GetSingleton()->IsGroupWorn(actor, group))
                continue;
            RenderGroupRow(a_hud, actor, group);
        }
        ImGuiMCP::Dummy(ImVec2{ 0.0f, scale.Px(Spacing.xs) });

        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::End();
    }
}
