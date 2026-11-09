#pragma once

#include "SKSEMenuFramework.h"

namespace Thread::Interface::UI::Theme
{
    struct ColorValues final
    {
        ImGuiMCP::ImU32 textPrimary = IM_COL32(221, 216, 208, 255);
        ImGuiMCP::ImU32 textSecondary = IM_COL32(176, 168, 152, 255);
        ImGuiMCP::ImU32 textMuted = IM_COL32(136, 128, 120, 255);
        ImGuiMCP::ImU32 accent = IM_COL32(112, 184, 112, 255);

        ImGuiMCP::ImU32 panelBackground = IM_COL32(0, 0, 0, 182);
        ImGuiMCP::ImU32 panelBorder = IM_COL32(255, 255, 255, 58);

        ImGuiMCP::ImU32 buttonIdle = IM_COL32(20, 20, 22, 245);
        ImGuiMCP::ImU32 buttonHovered = IM_COL32(36, 36, 40, 245);
        ImGuiMCP::ImU32 buttonPressed = IM_COL32(34, 34, 36, 245);
        ImGuiMCP::ImU32 buttonSelected = IM_COL32(22, 25, 23, 245);
        ImGuiMCP::ImU32 separator = IM_COL32(55, 55, 58, 128);
        ImGuiMCP::ImU32 shadow = IM_COL32(0, 0, 0, 210);
        ImGuiMCP::ImU32 shadowSoft = IM_COL32(0, 0, 0, 90);
        ImGuiMCP::ImU32 transparent = IM_COL32(0, 0, 0, 0);
        ImGuiMCP::ImU32 selectionText = IM_COL32(144, 176, 200, 255);
        ImGuiMCP::ImU32 selectionFill = IM_COL32(30, 40, 30, 128);
        ImGuiMCP::ImU32 nestedSurface = IM_COL32(10, 10, 10, 110);
        ImGuiMCP::ImU32 nestedHeader = IM_COL32(18, 18, 18, 180);
        ImGuiMCP::ImU32 nestedHeaderHovered = IM_COL32(30, 30, 30, 220);
        ImGuiMCP::ImU32 nestedControl = IM_COL32(10, 10, 10, 190);
        ImGuiMCP::ImU32 nestedControlHovered = IM_COL32(30, 30, 30, 225);
        ImGuiMCP::ImU32 nestedControlActive = IM_COL32(24, 24, 24, 235);
        ImGuiMCP::ImU32 nestedPopup = IM_COL32(14, 14, 14, 248);
        ImGuiMCP::ImU32 nestedSeparator = IM_COL32(92, 90, 86, 58);

        ImGuiMCP::ImU32 borderSubtle = IM_COL32(92, 90, 86, 105);
        ImGuiMCP::ImU32 borderHovered = IM_COL32(145, 142, 136, 150);
        ImGuiMCP::ImU32 borderActive = IM_COL32(112, 184, 112, 220);
    };

    struct FontSizeValues final
    {
        float detail = 7.5f;             // tiny enjoyment-bar interaction text.
        float smallText = 8.0f;          // enjoyment values and Scene hover-card keys
        float metadata = 8.5f;           // enjoyment actor names and Scene tags.
        float caption = 9.5f;            // panel titles, tabs, and many General field labels
        float compact = 10.0f;           // General actor-card content and Scene hover-card rows
        float subsectionHeader = 10.0f;  // collapsible subsection headers
        float sectionHeader = 11.0f;     // collapsible section headers
        float body = 10.5f;              // normal buttons, lists, checkboxes, sliders, and primary text
        float overlay = 9.0f;            // large animation-speed overlay text
    };

    struct Icon final
    {
        static constexpr const char* solidFont = "fa-solid-900";
        static constexpr const char* anglesLeft = "\xEF\x84\x80";    // U+F100
        static constexpr const char* anglesRight = "\xEF\x84\x81";   // U+F101
        static constexpr const char* chevronRight = "\xEF\x81\x94";  // U+F054
        static constexpr const char* chevronUp = "\xEF\x81\xB7";     // U+F077
        static constexpr const char* chevronDown = "\xEF\x81\xB8";   // U+F078
        static constexpr const char* plus = "\xEF\x81\xA7";          // U+F067
        static constexpr const char* minus = "\xEF\x81\xA8";         // U+F068
        static constexpr const char* rotateLeft = "\xEF\x8B\xAA";    // U+F2EA
        static constexpr const char* nextPerm = "\xEE\x95\x92";      // U+E552
    };

    struct SpacingValues final
    {
        float xxs = 2.0f;
        float xs = 4.0f;
        float sm = 6.0f;
        float md = 8.0f;
        float lg = 12.0f;
        float xl = 16.0f;
    };

    struct GeometryValues final
    {
        float roundingSmall = 2.0f;
        float roundingPanelTab = 5.0f;
        float roundingPanel = 0.0f;
        float roundingEnjBar = 3.0f;
        float borderThin = 1.0f;
        float checkboxPaddingY = 0.5f;
        float checkboxRowHeight = 24.0f;
        float panelTabWidth = 78.0f;
        float panelTabGap = 8.0f;
        float nestedMenuScale = 0.90f;
        float nestedHeaderPaddingX = 10.0f;
        float nestedHeaderPaddingY = 5.0f;
        float nestedHeaderAccentWidth = 2.0f;
    };

    struct EnjoymentValues final
    {
        ImGuiMCP::ImU32 interactionText = IM_COL32(136, 128, 120, 255);
        ImGuiMCP::ImU32 normalLow = IM_COL32(122, 40, 40, 255);
        ImGuiMCP::ImU32 normalHigh = IM_COL32(208, 104, 88, 255);
        ImGuiMCP::ImU32 negativeLow = IM_COL32(30, 58, 88, 255);
        ImGuiMCP::ImU32 negativeHigh = IM_COL32(72, 128, 192, 255);
        ImGuiMCP::ImU32 zoneIdle = IM_COL32(50, 155, 60, 41);
        ImGuiMCP::ImU32 zoneActive = IM_COL32(60, 185, 65, 71);
        ImGuiMCP::ImU32 zoneBorder = IM_COL32(90, 200, 70, 128);
        ImGuiMCP::ImU32 zoneFocused = IM_COL32(110, 250, 90, 242);
        ImGuiMCP::ImU32 needle = IM_COL32(200, 216, 184, 255);
        ImGuiMCP::ImU32 needleActive = IM_COL32(144, 248, 120, 255);
        ImGuiMCP::ImU32 hit = IM_COL32(96, 204, 80, 255);
        ImGuiMCP::ImU32 miss = IM_COL32(224, 96, 80, 255);
        ImGuiMCP::ImU32 frameSurface = IM_COL32(16, 16, 18, 255);
        ImGuiMCP::ImU32 frameBorder = IM_COL32(40, 40, 48, 255);
        ImGuiMCP::ImU32 fillTrail = IM_COL32(208, 188, 168, 64);
        ImGuiMCP::ImU32 zoneCenter = IM_COL32(80, 180, 60, 51);
        ImGuiMCP::ImU32 zoneCenterActive = IM_COL32(100, 230, 80, 77);
        ImGuiMCP::ImU32 feedbackHit = IM_COL32(60, 200, 80, 89);
        ImGuiMCP::ImU32 feedbackMiss = IM_COL32(200, 60, 40, 97);
        ImGuiMCP::ImU32 targetBorder = IM_COL32(106, 96, 85, 255);
        float fillEaseRate = 15.0f;
        float trailEaseRate = 5.0f;
        float waveIntensity = 1.0f;
        float waveSpeed = 1.0f;
        float waveSpatialFrequency = 0.6f;
        float waveSecondaryStrength = 2.3f;
        float anchorX = 0.0f;  // where the bars sit across the screen: 0 left, 0.5 center, 1 right
        float anchorY = 1.0f;  // 0 top, 0.5 middle, 1 bottom
    };

    struct OffsetValues final
    {
        ImGuiMCP::ImU32 fill = IM_COL32(160, 160, 160, 56);
        ImGuiMCP::ImU32 track = IM_COL32(255, 255, 255, 10);
        ImGuiMCP::ImU32 centerTick = IM_COL32(255, 255, 255, 26);
        float panelWidth = 300.0f;
        float maxBodyHeight = 420.0f;
        float trackHitExtension = 8.0f;
        float trackValueWidth = 40.0f;
        float trackLabelWidth = 20.0f;
        float trackHeight = 4.0f;
        float trackNeedleWidth = 3.0f;
        float trackNeedleExtension = 5.0f;
        float trackNeedleRounding = 1.5f;
        float trackCenterTickExtension = 2.0f;
        float dragDistance = 300.0f;
    };

    struct AnimationValues final
    {
        ImGuiMCP::ImU32 timerTrack = IM_COL32(10, 10, 12, 178);
        ImGuiMCP::ImU32 timerEdge = IM_COL32(255, 255, 255, 38);
        ImGuiMCP::ImU32 timerCenter = IM_COL32(255, 255, 255, 217);
    };

    struct Data final
    {
        ColorValues color{};
        EnjoymentValues enjoyment{};
        OffsetValues offset{};
        AnimationValues animation{};
        SpacingValues spacing{};
        GeometryValues geometry{};
        FontSizeValues fontSize{};
    };

    inline Data data{};
    inline auto& Color = data.color;
    inline auto& Enjoyment = data.enjoyment;
    inline auto& Offset = data.offset;
    inline auto& Animation = data.animation;
    inline auto& Spacing = data.spacing;
    inline auto& Geometry = data.geometry;
    inline auto& FontSize = data.fontSize;

    void Load();
    void Save();
    [[nodiscard]] bool IsLoaded();

    inline ImGuiMCP::ImVec4 ToVec4(ImGuiMCP::ImU32 a_color)
    {
        return {
            static_cast<float>((a_color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
            static_cast<float>((a_color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
            static_cast<float>((a_color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
            static_cast<float>((a_color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f,
        };
    }
}

namespace Thread::Interface::UI
{
    inline void SetWindowFontSize(float a_size)
    {
        ImGuiMCP::SetWindowFontScale(1.0f);
        const float baseSize = ImGuiMCP::GetFontSize();
        ImGuiMCP::SetWindowFontScale(baseSize > 0.0f ? a_size / baseSize : 1.0f);
    }

    inline bool ActionButton(const char* a_label, float a_width)
    {
        return ImGuiMCP::Button(a_label, ImGuiMCP::ImVec2{ a_width, 0.0f });
    }

    inline void DrawRoundedGradientRect(ImGuiMCP::ImDrawList* a_drawList, ImGuiMCP::ImVec2 a_min, ImGuiMCP::ImVec2 a_max, ImGuiMCP::ImU32 a_left, ImGuiMCP::ImU32 a_right, float a_rounding, ImGuiMCP::ImDrawFlags a_flags)
    {
        if (a_max.x <= a_min.x || a_max.y <= a_min.y)
            return;

        const int vertexStart = a_drawList->VtxBuffer.Size;
        ImGuiMCP::ImDrawListManager::AddRectFilled(a_drawList, a_min, a_max, IM_COL32(255, 255, 255, 255),
            a_rounding, a_flags);
        const auto left = Theme::ToVec4(a_left);
        const auto right = Theme::ToVec4(a_right);
        for (int index = vertexStart; index < a_drawList->VtxBuffer.Size; ++index) {
            auto& vertex = a_drawList->VtxBuffer.Data[index];
            const float factor = std::clamp((vertex.pos.x - a_min.x) / (a_max.x - a_min.x), 0.0f, 1.0f);
            vertex.col = ImGuiMCP::ColorConvertFloat4ToU32({ std::lerp(left.x, right.x, factor), std::lerp(left.y, right.y, factor),
                std::lerp(left.z, right.z, factor), std::lerp(left.w, right.w, factor) });
        }
    }

    inline bool SelectableButton(const char* a_label, bool a_selected, ImGuiMCP::ImGuiSelectableFlags a_flags, ImGuiMCP::ImVec2 a_size)
    {
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_SelectableTextAlign, ImGuiMCP::ImVec2{ 0.0f, 0.5f });
        const bool clicked = ImGuiMCP::Selectable(a_label, a_selected, a_flags, a_size);
        ImGuiMCP::PopStyleVar();
        return clicked;
    }

    inline void PushCheckboxStyle(float a_scale)
    {
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding,
            ImGuiMCP::ImVec2{ ImGuiMCP::GetStyle()->FramePadding.x, Theme::Geometry.checkboxPaddingY * a_scale });
    }

    inline void PopCheckboxStyle()
    {
        ImGuiMCP::PopStyleVar();
    }

    inline bool CheckboxRow(const char* a_label, const char* a_id, bool& a_value, float a_scale)
    {
        ImGuiMCP::PushID(a_id);
        const float horizontalPadding = Theme::Spacing.lg * a_scale;
        const float rowHeight = Theme::Geometry.checkboxRowHeight * a_scale;
        const float availableWidth = ImGuiMCP::GetContentRegionAvail().x - horizontalPadding * 2.0f;
        const ImGuiMCP::ImVec2 rowOrigin = ImGuiMCP::GetCursorScreenPos();
        ImGuiMCP::SetCursorScreenPos({ rowOrigin.x + horizontalPadding, rowOrigin.y });

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderHovered, Theme::Color.transparent);
        const bool rowClicked = SelectableButton("##row", false, ImGuiMCP::ImGuiSelectableFlags_AllowOverlap, { availableWidth, rowHeight });
        ImGuiMCP::PopStyleColor();

        PushCheckboxStyle(a_scale);
        const float checkboxSize = ImGuiMCP::GetFrameHeight();
        ImGuiMCP::SetCursorScreenPos({ rowOrigin.x + horizontalPadding + availableWidth - checkboxSize,
            rowOrigin.y + (rowHeight - checkboxSize) * 0.5f });
        const bool checkboxChanged = ImGuiMCP::Checkbox("##checkbox", &a_value);
        PopCheckboxStyle();

        const ImGuiMCP::ImVec2 labelSize = ImGuiMCP::CalcTextSize(a_label);
        ImGuiMCP::SetCursorScreenPos({ rowOrigin.x + horizontalPadding, rowOrigin.y + (rowHeight - labelSize.y) * 0.5f });
        ImGuiMCP::TextColored(Theme::ToVec4(Theme::Color.textSecondary), "%s", a_label);

        const bool rowChanged = rowClicked && !checkboxChanged;
        if (rowChanged)
            a_value = !a_value;
        ImGuiMCP::SetCursorScreenPos({ rowOrigin.x, rowOrigin.y + rowHeight });
        ImGuiMCP::PopID();
        return rowChanged || checkboxChanged;
    }

    inline bool CollapsibleSectionHeader(const char* a_label, const char* a_id, bool a_open, ImGuiMCP::ImVec2 a_size)
    {
        const ImGuiMCP::ImVec2 headerMin = ImGuiMCP::GetCursorScreenPos();
        const bool clicked = ImGuiMCP::Selectable(a_id, false, 0, a_size);
        const bool hovered = ImGuiMCP::IsItemHovered();
        const ImGuiMCP::ImVec2 cursorAfter = ImGuiMCP::GetCursorPos();
        const ImGuiMCP::ImVec2 labelSize = ImGuiMCP::CalcTextSize(a_label);
        const ImGuiMCP::ImVec4 color = Theme::ToVec4(hovered ? Theme::Color.textPrimary : Theme::Color.textSecondary);
        const float horizontalPadding = a_size.y * 0.5f;

        ImGuiMCP::SetCursorScreenPos({ headerMin.x + horizontalPadding, headerMin.y + (a_size.y - labelSize.y) * 0.5f });
        ImGuiMCP::TextColored(color, "%s", a_label);

        const char* icon = a_open ? Theme::Icon::chevronUp : Theme::Icon::chevronDown;
        SKSEMenuFramework::PushFont(Theme::Icon::solidFont);
        const ImGuiMCP::ImVec2 iconSize = ImGuiMCP::CalcTextSize(icon);
        const float availW = ImGuiMCP::GetContentRegionAvail().x;
        ImGuiMCP::SetCursorScreenPos({ headerMin.x + availW - iconSize.x - horizontalPadding,
            headerMin.y + (a_size.y - iconSize.y) * 0.5f });
        ImGuiMCP::TextColored(color, "%s", icon);
        FontAwesome::Pop();

        ImGuiMCP::SetCursorPos(cursorAfter);
        return clicked;
    }

    inline bool CollapsibleCardHeader(const char* a_label, const char* a_badge, const char* a_id, bool a_open, float a_width, float a_scale)
    {
        const ImGuiMCP::ImVec2 headerMin = ImGuiMCP::GetCursorScreenPos();
        const float paddingX = Theme::Geometry.nestedHeaderPaddingX * a_scale;
        const float paddingY = Theme::Geometry.nestedHeaderPaddingY * a_scale;
        const float headerHeight = ImGuiMCP::CalcTextSize(a_label).y + paddingY * 2.0f;

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, Theme::Color.nestedHeader);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderHovered, Theme::Color.nestedHeaderHovered);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderActive, Theme::Color.nestedControlActive);
        const bool clicked = ImGuiMCP::Selectable(a_id, false, 0, { a_width, headerHeight });
        ImGuiMCP::PopStyleColor(3);

        const bool hovered = ImGuiMCP::IsItemHovered();
        auto* drawList = ImGuiMCP::GetWindowDrawList();
        if (!hovered)
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, headerMin, { headerMin.x + a_width, headerMin.y + headerHeight }, Theme::Color.nestedHeader, 0.0f, 0);

        ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, headerMin,
            { headerMin.x + Theme::Geometry.nestedHeaderAccentWidth * a_scale, headerMin.y + headerHeight },
            hovered ? Theme::Color.accent : Theme::Color.borderSubtle, 0.0f, 0);

        const ImGuiMCP::ImVec4 textColor = Theme::ToVec4(hovered ? Theme::Color.textPrimary : Theme::Color.textSecondary);
        const ImGuiMCP::ImVec2 labelSize = ImGuiMCP::CalcTextSize(a_label);
        ImGuiMCP::SetCursorScreenPos({ headerMin.x + paddingX, headerMin.y + (headerHeight - labelSize.y) * 0.5f });
        ImGuiMCP::TextColored(textColor, "%s", a_label);

        SKSEMenuFramework::PushFont(Theme::Icon::solidFont);
        const char* toggleIcon = a_open ? Theme::Icon::minus : Theme::Icon::plus;
        const ImGuiMCP::ImVec2 toggleSize = ImGuiMCP::CalcTextSize(toggleIcon);
        FontAwesome::Pop();
        const float toggleX = headerMin.x + a_width - paddingX - toggleSize.x;

        if (a_badge && *a_badge) {
            const ImGuiMCP::ImVec2 badgeSize = ImGuiMCP::CalcTextSize(a_badge);
            ImGuiMCP::SetCursorScreenPos({ toggleX - Theme::Spacing.sm * a_scale - badgeSize.x,
                headerMin.y + (headerHeight - badgeSize.y) * 0.5f });
            ImGuiMCP::TextColored(Theme::ToVec4(Theme::Color.accent), "%s", a_badge);
        }

        SKSEMenuFramework::PushFont(Theme::Icon::solidFont);
        ImGuiMCP::SetCursorScreenPos({ toggleX, headerMin.y + (headerHeight - toggleSize.y) * 0.5f });
        ImGuiMCP::TextColored(Theme::ToVec4(Theme::Color.textSecondary), "%s", toggleIcon);
        FontAwesome::Pop();

        ImGuiMCP::SetCursorScreenPos({ headerMin.x, headerMin.y + headerHeight });
        return clicked;
    }

    inline void DrawTextShadowed(ImGuiMCP::ImDrawList* a_drawList, ImGuiMCP::ImVec2 a_position, ImGuiMCP::ImU32 a_color, const char* a_text)
    {
        const auto textAlpha = (a_color & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
        const auto shadowAlpha = ((Theme::Color.shadow & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT) * textAlpha / 255;
        const auto shadow = (Theme::Color.shadow & ~IM_COL32_A_MASK) | (shadowAlpha << IM_COL32_A_SHIFT);
        ImGuiMCP::ImDrawListManager::AddText(a_drawList, { a_position.x - 1, a_position.y - 1 }, shadow, a_text, nullptr);
        ImGuiMCP::ImDrawListManager::AddText(a_drawList, { a_position.x + 1, a_position.y - 1 }, shadow, a_text, nullptr);
        ImGuiMCP::ImDrawListManager::AddText(a_drawList, { a_position.x - 1, a_position.y + 1 }, shadow, a_text, nullptr);
        ImGuiMCP::ImDrawListManager::AddText(a_drawList, { a_position.x + 1, a_position.y + 1 }, shadow, a_text, nullptr);
        ImGuiMCP::ImDrawListManager::AddText(a_drawList, a_position, a_color, a_text, nullptr);
    }
}
