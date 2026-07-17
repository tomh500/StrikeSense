#include "notifications_layout.h"

#include <algorithm>

namespace notifications_layout {
namespace {

constexpr float kBaseWidth = 2560.f;
constexpr float kBaseHeight = 1440.f;

float resolve_scale(int screenWidth, int screenHeight)
{
    if (screenWidth <= 0 || screenHeight <= 0) return 1.f;
    const float scaleX = static_cast<float>(screenWidth) / kBaseWidth;
    const float scaleY = static_cast<float>(screenHeight) / kBaseHeight;
    return (std::max)(0.65f, (std::min)(scaleX, scaleY));
}

int scaled_int(float scale, float baseValue)
{
    return static_cast<int>(baseValue * scale + 0.5f);
}

} // namespace

metrics CalculateMetrics(int style, int screenWidth, int screenHeight)
{
    metrics layout{};
    layout.scale = resolve_scale(screenWidth, screenHeight);
    layout.width = scaled_int(layout.scale, style == 4 ? 460.f : (style == 3 ? 330.f : 310.f));
    layout.height = scaled_int(layout.scale, style == 4 ? 68.f : 72.f);
    layout.pad = scaled_int(layout.scale, style == 1 ? 8.f : 4.f);
    layout.renderWidth = layout.width + layout.pad * 2;
    layout.renderHeight = layout.height + layout.pad * 2;
    layout.baseX = screenWidth - layout.width - scaled_int(layout.scale, 26.f);
    layout.baseY = screenHeight - layout.height - scaled_int(layout.scale, 42.f);
    layout.slideDistanceX = Scale(layout, 360.f);
    layout.vapeSlideY = Scale(layout, 48.f);
    layout.deepseekSlideY = Scale(layout, 80.f);
    return layout;
}

float Scale(const metrics& layout, float baseValue)
{
    return baseValue * layout.scale;
}

} // namespace notifications_layout
