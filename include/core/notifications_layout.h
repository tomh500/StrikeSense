#pragma once

namespace notifications_layout {

struct metrics {
    float scale = 1.f;
    int width = 0;
    int height = 0;
    int pad = 0;
    int renderWidth = 0;
    int renderHeight = 0;
    int baseX = 0;
    int baseY = 0;
    float slideDistanceX = 0.f;
    float vapeSlideY = 0.f;
    float deepseekSlideY = 0.f;
};

metrics CalculateMetrics(int style, int screenWidth, int screenHeight);
float Scale(const metrics& layout, float baseValue);

} // namespace notifications_layout
