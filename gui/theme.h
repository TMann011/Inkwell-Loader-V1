#pragma once
#include "../imgui/imgui.h"

namespace Inkwell {

enum class ThemePreset {
    PurpleBlack = 0,    // Default Purple & Black Theme
    ElectricIndigo = 1, // Classic Indigo Theme
    CyberEmerald = 2,   // High-Visibility Emerald Green
    CrimsonBlood = 3,   // High-Contrast Red
    Custom = 4          // User-configured custom palette
};

struct Style {
    // Default: Inkwell Purple & Black
    ImVec4 AccentColor     = ImVec4(0.58f, 0.28f, 0.95f, 1.00f);     // Neon Violet (#9447F2)
    ImVec4 AccentActive    = ImVec4(0.75f, 0.45f, 1.00f, 1.00f);     // Bright Purple
    ImVec4 AccentHovered   = ImVec4(0.68f, 0.38f, 1.00f, 0.90f);     // Purple Glow
    ImVec4 Background      = ImVec4(0.04f, 0.03f, 0.06f, 0.96f);     // Pitch Black Glass
    ImVec4 PanelBg         = ImVec4(0.08f, 0.06f, 0.12f, 0.85f);     // Dark Violet Card
    ImVec4 PanelAlt        = ImVec4(0.12f, 0.09f, 0.18f, 0.60f);     // Inner Card
    ImVec4 Border          = ImVec4(0.24f, 0.15f, 0.38f, 0.55f);     // Muted Violet Border
    ImVec4 TextPrimary     = ImVec4(0.96f, 0.95f, 0.98f, 1.00f);     // Pure White
    ImVec4 TextSecondary   = ImVec4(0.65f, 0.58f, 0.75f, 1.00f);     // Lavender Gray Muted
    ImVec4 TextMuted       = ImVec4(0.42f, 0.35f, 0.50f, 1.00f);     // Dim Label

    // Status colors
    ImVec4 StatusSuccess   = ImVec4(0.20f, 0.85f, 0.55f, 1.00f);     // Emerald Success
    ImVec4 StatusWarning   = ImVec4(0.95f, 0.75f, 0.20f, 1.00f);     // Amber Warning
    ImVec4 StatusError     = ImVec4(0.95f, 0.30f, 0.30f, 1.00f);     // Crimson Error
    ImVec4 StatusStealth   = ImVec4(0.75f, 0.45f, 1.00f, 1.00f);     // Violet Cloak

    float WindowRounding   = 8.0f;
    float FrameRounding    = 5.0f;
    float PopupRounding    = 6.0f;
    float ChildRounding    = 6.0f;

    ThemePreset currentPreset = ThemePreset::PurpleBlack;
    bool highContrast = false;
};

inline Style g_Style;

void ApplyTheme();
void ApplyThemePreset(ThemePreset preset);
void SetHighContrast(bool enable);

} // namespace Inkwell
