#include "notifications_layout.h"

#include <algorithm>
#include <windows.h>

namespace notifications_layout {
namespace {

constexpr float kBaseWidth = 2560.f;
constexpr float kBaseHeight = 1440.f;

struct style_base_metrics {
    float width = 310.f;
    float height = 72.f;
    float pad = 4.f;
};

style_base_metrics get_style_base_metrics(int style)
{
    switch (style) {
    case 1:
        return style_base_metrics{ 584.f, 136.f, 15.f };
    case 3:
        return style_base_metrics{ 622.f, 136.f, 8.f };
    case 4:
        return style_base_metrics{ 867.f, 128.f, 8.f };
    case 0:
    case 2:
    default:
        return style_base_metrics{ 584.f, 136.f, 8.f };
    }
}

float query_dpi_scale()
{
    const UINT dpi = GetDpiForSystem();
    if (dpi == 0) return 1.f;
    return static_cast<float>(dpi) / 96.f;
}

float resolve_scale(int screenWidth, int screenHeight)
{
    if (screenWidth <= 0 || screenHeight <= 0) return 1.f;
    const float dpiScale = query_dpi_scale();
    const float estimatedRealWidth = static_cast<float>(screenWidth) * dpiScale;
    const float estimatedRealHeight = static_cast<float>(screenHeight) * dpiScale;
    const float resolutionScaleX = estimatedRealWidth / kBaseWidth;
    const float resolutionScaleY = estimatedRealHeight / kBaseHeight;
    const float resolutionScale = (std::min)(resolutionScaleX, resolutionScaleY);
    return (std::max)(0.65f, resolutionScale * dpiScale);
}

int scaled_int(float scale, float baseValue)
{
    return static_cast<int>(baseValue * scale + 0.5f);
}

} // namespace

metrics CalculateMetrics(int style, int screenWidth, int screenHeight)
{
    metrics layout{};
    const style_base_metrics styleBase = get_style_base_metrics(style);
    layout.scale = resolve_scale(screenWidth, screenHeight);
    layout.width = scaled_int(layout.scale, styleBase.width);
    layout.height = scaled_int(layout.scale, styleBase.height);
    layout.pad = scaled_int(layout.scale, styleBase.pad);
    layout.renderWidth = layout.width + layout.pad * 2;
    layout.renderHeight = layout.height + layout.pad * 2;
    layout.baseX = screenWidth - layout.width - scaled_int(layout.scale, 49.f);
    layout.baseY = screenHeight - layout.height - scaled_int(layout.scale, 79.f);
    layout.slideDistanceX = Scale(layout, 678.f);
    layout.vapeSlideY = Scale(layout, 90.f);
    layout.deepseekSlideY = Scale(layout, 151.f);
    return layout;
}

float Scale(const metrics& layout, float baseValue)
{
    return baseValue * layout.scale;
}

} // namespace notifications_layout
