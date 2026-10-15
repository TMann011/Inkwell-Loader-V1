#include "theme.h"

namespace Inkwell {

void ApplyThemePreset(ThemePreset preset) {
    g_Style.currentPreset = preset;

    switch (preset) {
    case ThemePreset::PurpleBlack:
        g_Style.AccentColor   = ImVec4(0.58f, 0.28f, 0.95f, 1.00f); // Neon Violet (#9447F2)
        g_Style.AccentHovered = ImVec4(0.68f, 0.38f, 1.00f, 0.90f);
        g_Style.AccentActive  = ImVec4(0.75f, 0.45f, 1.00f, 1.00f);
        g_Style.Background    = ImVec4(0.04f, 0.03f, 0.06f, 0.96f); // Pitch Black Glass
        g_Style.PanelBg       = ImVec4(0.08f, 0.06f, 0.12f, 0.85f); // Dark Violet Card
        g_Style.PanelAlt      = ImVec4(0.12f, 0.09f, 0.18f, 0.60f);
        g_Style.Border        = ImVec4(0.24f, 0.15f, 0.38f, 0.55f);
        g_Style.TextPrimary   = ImVec4(0.96f, 0.95f, 0.98f, 1.00f);
        g_Style.TextSecondary = ImVec4(0.65f, 0.58f, 0.75f, 1.00f);
        g_Style.TextMuted     = ImVec4(0.42f, 0.35f, 0.50f, 1.00f);
        break;

    case ThemePreset::ElectricIndigo:
        g_Style.AccentColor   = ImVec4(0.38f, 0.45f, 0.98f, 1.00f); // Electric Indigo (#6173FA)
        g_Style.AccentHovered = ImVec4(0.42f, 0.50f, 0.99f, 0.90f);
        g_Style.AccentActive  = ImVec4(0.48f, 0.55f, 1.00f, 1.00f);
        g_Style.Background    = ImVec4(0.06f, 0.06f, 0.08f, 0.96f);
        g_Style.PanelBg       = ImVec4(0.10f, 0.11f, 0.14f, 0.85f);
        g_Style.PanelAlt      = ImVec4(0.12f, 0.13f, 0.17f, 0.60f);
        g_Style.Border        = ImVec4(0.18f, 0.20f, 0.26f, 0.55f);
        g_Style.TextPrimary   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        g_Style.TextSecondary = ImVec4(0.55f, 0.58f, 0.65f, 1.00f);
        g_Style.TextMuted     = ImVec4(0.35f, 0.38f, 0.45f, 1.00f);
        break;

    case ThemePreset::CyberEmerald:
        g_Style.AccentColor   = ImVec4(0.06f, 0.75f, 0.50f, 1.00f); // Neon Emerald (#10B981)
        g_Style.AccentHovered = ImVec4(0.10f, 0.85f, 0.58f, 0.90f);
        g_Style.AccentActive  = ImVec4(0.15f, 0.95f, 0.65f, 1.00f);
        g_Style.Background    = ImVec4(0.02f, 0.05f, 0.04f, 0.96f);
        g_Style.PanelBg       = ImVec4(0.05f, 0.10f, 0.08f, 0.85f);
        g_Style.PanelAlt      = ImVec4(0.08f, 0.14f, 0.11f, 0.60f);
        g_Style.Border        = ImVec4(0.12f, 0.28f, 0.22f, 0.55f);
        g_Style.TextPrimary   = ImVec4(0.92f, 0.98f, 0.95f, 1.00f);
        g_Style.TextSecondary = ImVec4(0.52f, 0.72f, 0.62f, 1.00f);
        g_Style.TextMuted     = ImVec4(0.32f, 0.48f, 0.40f, 1.00f);
        break;

    case ThemePreset::CrimsonBlood:
        g_Style.AccentColor   = ImVec4(0.85f, 0.20f, 0.25f, 1.00f); // Crimson Red (#EF4444)
        g_Style.AccentHovered = ImVec4(0.95f, 0.25f, 0.30f, 0.90f);
        g_Style.AccentActive  = ImVec4(1.00f, 0.30f, 0.35f, 1.00f);
        g_Style.Background    = ImVec4(0.06f, 0.03f, 0.04f, 0.96f);
        g_Style.PanelBg       = ImVec4(0.12f, 0.06f, 0.08f, 0.85f);
        g_Style.PanelAlt      = ImVec4(0.18f, 0.08f, 0.10f, 0.60f);
        g_Style.Border        = ImVec4(0.35f, 0.14f, 0.18f, 0.55f);
        g_Style.TextPrimary   = ImVec4(0.98f, 0.94f, 0.95f, 1.00f);
        g_Style.TextSecondary = ImVec4(0.72f, 0.52f, 0.55f, 1.00f);
        g_Style.TextMuted     = ImVec4(0.48f, 0.32f, 0.35f, 1.00f);
        break;

    case ThemePreset::Custom:
        // Retain current user custom colors
        break;
    }

    if (g_Style.highContrast) {
        g_Style.Background = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        g_Style.PanelBg    = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        g_Style.Border     = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        g_Style.TextPrimary = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    }

    ApplyTheme();
}

void SetHighContrast(bool enable) {
    g_Style.highContrast = enable;
    ApplyThemePreset(g_Style.currentPreset);
}

void ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding     = ImVec2(16.0f, 16.0f);
    style.FramePadding      = ImVec2(12.0f, 7.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 6.0f);
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowRounding    = g_Style.WindowRounding;
    style.FrameRounding     = g_Style.FrameRounding;
    style.PopupRounding     = g_Style.PopupRounding;
    style.ChildRounding     = g_Style.ChildRounding;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = g_Style.Background;
    colors[ImGuiCol_ChildBg]              = g_Style.PanelBg;
    colors[ImGuiCol_PopupBg]              = ImVec4(g_Style.Background.x * 1.2f, g_Style.Background.y * 1.2f, g_Style.Background.z * 1.2f, 0.98f);
    colors[ImGuiCol_Border]               = g_Style.Border;
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = g_Style.PanelAlt;
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(g_Style.PanelAlt.x * 1.4f, g_Style.PanelAlt.y * 1.4f, g_Style.PanelAlt.z * 1.4f, 0.85f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(g_Style.AccentColor.x * 0.5f, g_Style.AccentColor.y * 0.5f, g_Style.AccentColor.z * 0.5f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.03f, 0.02f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.05f, 0.04f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.03f, 0.02f, 0.04f, 0.50f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.06f, 0.04f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.03f, 0.06f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(g_Style.AccentColor.x * 0.4f, g_Style.AccentColor.y * 0.4f, g_Style.AccentColor.z * 0.4f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = g_Style.AccentHovered;
    colors[ImGuiCol_ScrollbarGrabActive]  = g_Style.AccentActive;
    colors[ImGuiCol_CheckMark]            = g_Style.AccentColor;
    colors[ImGuiCol_SliderGrab]           = g_Style.AccentColor;
    colors[ImGuiCol_SliderGrabActive]     = g_Style.AccentActive;
    colors[ImGuiCol_Button]               = ImVec4(g_Style.PanelAlt.x * 1.2f, g_Style.PanelAlt.y * 1.2f, g_Style.PanelAlt.z * 1.2f, 0.85f);
    colors[ImGuiCol_ButtonHovered]        = g_Style.AccentHovered;
    colors[ImGuiCol_ButtonActive]         = g_Style.AccentActive;
    colors[ImGuiCol_Header]               = ImVec4(g_Style.PanelAlt.x * 1.3f, g_Style.PanelAlt.y * 1.3f, g_Style.PanelAlt.z * 1.3f, 0.85f);
    colors[ImGuiCol_HeaderHovered]        = g_Style.AccentHovered;
    colors[ImGuiCol_HeaderActive]         = g_Style.AccentActive;
    colors[ImGuiCol_Separator]            = g_Style.Border;
    colors[ImGuiCol_SeparatorHovered]     = g_Style.AccentHovered;
    colors[ImGuiCol_SeparatorActive]      = g_Style.AccentActive;
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.20f, 0.15f, 0.28f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]    = g_Style.AccentHovered;
    colors[ImGuiCol_ResizeGripActive]     = g_Style.AccentActive;
    colors[ImGuiCol_Tab]                  = ImVec4(g_Style.PanelBg.x, g_Style.PanelBg.y, g_Style.PanelBg.z, 0.80f);
    colors[ImGuiCol_TabHovered]           = g_Style.AccentHovered;
    colors[ImGuiCol_TabActive]            = ImVec4(g_Style.AccentColor.x * 0.4f, g_Style.AccentColor.y * 0.4f, g_Style.AccentColor.z * 0.4f, 1.00f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.05f, 0.04f, 0.07f, 0.80f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.10f, 0.08f, 0.14f, 1.00f);
    colors[ImGuiCol_Text]                 = g_Style.TextPrimary;
    colors[ImGuiCol_TextDisabled]         = g_Style.TextSecondary;
}

} // namespace Inkwell
