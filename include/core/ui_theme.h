#pragma once

#include <Windows.h>
#include <gdiplus.h>

extern int g_uiThemePreset;

namespace uitheme {

enum preset {
    preset_default = 0,
    preset_default_plus = preset_default,
    preset_aqua = 1,
    preset_dark_red = 2,
    preset_autumn_gold = 3,
    preset_spring_green = 4,
    preset_purple_song = 5,
    preset_vape = 6,
    preset_liquidbounce = 7,
    preset_gemini = 8,
    preset_gpt = 9,
    preset_deepseek = 10,
    preset_count = 11
};

struct palette {
    Gdiplus::Color window_background;
    Gdiplus::Color sidebar_background;
    Gdiplus::Color sidebar_border;
    Gdiplus::Color sidebar_selected_background;
    Gdiplus::Color header_background;
    Gdiplus::Color title;
    Gdiplus::Color text;
    Gdiplus::Color dim;
    Gdiplus::Color accent;
    Gdiplus::Color accent_soft;
    Gdiplus::Color accent_strong;
    Gdiplus::Color toggle_off;
    Gdiplus::Color toggle_knob;
    Gdiplus::Color slider_background;
    Gdiplus::Color button_background;
    Gdiplus::Color button_hover;
    Gdiplus::Color button_border;
    Gdiplus::Color button_text;
    Gdiplus::Color card_background;
    Gdiplus::Color card_alt_background;
    Gdiplus::Color card_selected_background;
    Gdiplus::Color card_border;
    Gdiplus::Color input_background;
    Gdiplus::Color input_border;
    Gdiplus::Color preview_background;
    Gdiplus::Color tooltip_background;
    Gdiplus::Color tooltip_text;
    Gdiplus::Color tooltip_border;
    Gdiplus::Color success;
    Gdiplus::Color warning;
    Gdiplus::Color danger;
    Gdiplus::Color notice_background;
    Gdiplus::Color notice_text;
    Gdiplus::Color notice_border;
};

const palette& get_palette();
int get_preset();
void set_preset(int preset_id);
const wchar_t* get_preset_name(int preset_id);
bool is_advanced_preset(int preset_id);

} // namespace uitheme
