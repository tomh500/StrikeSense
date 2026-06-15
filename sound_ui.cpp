#include "sound_ui.h"
#include "config.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>
#include <commdlg.h>

namespace sound_ui {

static SDL_Window*   s_sdlWindow = nullptr;
static SDL_Renderer* s_renderer = nullptr;
static TTF_Font*     s_font = nullptr;
static HWND          s_parentHwnd = nullptr;
static int           s_panelW = 450;
static int           s_panelH = 300;
static bool          s_needsRedraw = true;

struct SndRow {
    int   id;          // 音效 id (1-5, -1)
    const wchar_t* label;
    SDL_Rect btnRect;  // 浏览按钮区域
    SDL_Rect txtRect;  // 文件标签区域
};

static SndRow s_rows[] = {
    { 1,  L"一杀",      {0}, {0} },
    { 2,  L"二杀",      {0}, {0} },
    { 3,  L"三杀",      {0}, {0} },
    { 4,  L"四杀",      {0}, {0} },
    { 5,  L"五杀",      {0}, {0} },
    { -1, L"多杀/死斗", {0}, {0} },
};

// ----------------------------------------------------------
bool Initialize(HWND hParent, HINSTANCE)
{
    s_parentHwnd = hParent;

    // 获取父窗口客户区大小以确定 SDL 面板尺寸
    RECT rc;
    GetClientRect(hParent, &rc);
    s_panelW = rc.right - rc.left;
    s_panelH = rc.bottom - rc.top;

    // 将父窗口 HWND 转换为 SDL_Window
    s_sdlWindow = SDL_CreateWindowFrom(hParent);
    if (!s_sdlWindow)
        return false;

    s_renderer = SDL_CreateRenderer(s_sdlWindow, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s_renderer)
        return false;

    // 启用混合模式以支持透明绘制
    SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);

    // 初始化 SDL_ttf
    TTF_Init();

    // 尝试加载微软雅黑字体（系统默认中文字体）
    s_font = TTF_OpenFont("C:\\Windows\\Fonts\\msyh.ttc", 16);
    if (!s_font)
        s_font = TTF_OpenFont("C:\\Windows\\Fonts\\msyhbd.ttc", 16);
    if (!s_font)
        s_font = TTF_OpenFont("C:\\Windows\\Fonts\\simsun.ttc", 16);
    if (!s_font)
        s_font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 16);

    return true;
}

// ----------------------------------------------------------
void Render()
{
    if (!s_renderer || !s_font) return;

    config::Settings cfg = config::Load();
    int y = 10;
    const int rowH = 40;

    // 用深色半透明背景
    SDL_SetRenderDrawColor(s_renderer, 18, 18, 24, 220);
    SDL_RenderClear(s_renderer);

    SDL_Color white    = { 230, 230, 230, 255 };
    SDL_Color grey     = { 160, 160, 170, 255 };
    SDL_Color accent   = { 80,  160, 240, 255 };
    SDL_Color btnBg    = { 50,  50,  60,  200 };
    SDL_Color btnHover = { 70,  70,  85,  200 };

    // 标题
    {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(s_font, "音效配置", accent);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);
            SDL_Rect dst = { 12, 8, surf->w, surf->h };
            SDL_RenderCopy(s_renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }

    y = 42;

    for (auto& row : s_rows)
    {
        // ── 标签背景 ──
        SDL_Rect labelBg = { 10, y, 100, rowH };
        SDL_SetRenderDrawColor(s_renderer, 35, 35, 42, 200);
        SDL_RenderFillRect(s_renderer, &labelBg);

        // 标签文字
        {
            std::wstring ws(row.label);
            std::string utf8;
            for (wchar_t wc : ws) {
                if (wc < 0x80) utf8 += (char)wc;
                else {
                    utf8 += (char)(0xE0 | (wc >> 12));
                    utf8 += (char)(0x80 | ((wc >> 6) & 0x3F));
                    utf8 += (char)(0x80 | (wc & 0x3F));
                }
            }
            SDL_Surface* surf = TTF_RenderUTF8_Blended(s_font, utf8.c_str(), white);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);
                SDL_Rect dst = { 20, y + 8, surf->w, surf->h };
                SDL_RenderCopy(s_renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
                SDL_FreeSurface(surf);
            }
        }

        // ── 文件名区域 ──
        SDL_Rect fileBg = { 120, y, 250, rowH };
        SDL_SetRenderDrawColor(s_renderer, 40, 40, 48, 200);
        SDL_RenderFillRect(s_renderer, &fileBg);

        std::wstring filePath;
        switch (row.id) {
        case 1: filePath = cfg.snd_1; break;
        case 2: filePath = cfg.snd_2; break;
        case 3: filePath = cfg.snd_3; break;
        case 4: filePath = cfg.snd_4; break;
        case 5: filePath = cfg.snd_5; break;
        case -1: filePath = cfg.snd_extra; break;
        }

        std::string fileName;
        if (!filePath.empty()) {
            std::filesystem::path p(filePath);
            fileName = p.filename().string();
        } else {
            fileName = "默认";
        }

        {
            SDL_Surface* surf = TTF_RenderUTF8_Blended(s_font, fileName.c_str(),
                filePath.empty() ? grey : white);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);
                SDL_Rect dst = { 130, y + 8, surf->w, surf->h };
                SDL_RenderCopy(s_renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
                SDL_FreeSurface(surf);
            }
        }

        // ── 浏览按钮 ──
        SDL_Rect btn = { 380, y + 4, 40, 32 };
        SDL_SetRenderDrawColor(s_renderer, btnBg.r, btnBg.g, btnBg.b, btnBg.a);
        SDL_RenderFillRect(s_renderer, &btn);

        // 按钮边框
        SDL_SetRenderDrawColor(s_renderer, 100, 100, 120, 255);
        SDL_RenderDrawRect(s_renderer, &btn);

        // 按钮文字
        {
            SDL_Surface* surf = TTF_RenderUTF8_Blended(s_font, "...", white);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);
                SDL_Rect dst = { btn.x + 8, btn.y + 4, surf->w, surf->h };
                SDL_RenderCopy(s_renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
                SDL_FreeSurface(surf);
            }
        }

        row.btnRect = btn;
        y += rowH + 6;
    }

    SDL_RenderPresent(s_renderer);
    s_needsRedraw = false;
}

// ----------------------------------------------------------
void HandleClick(int mouseX, int mouseY)
{
    for (auto& row : s_rows)
    {
        if (mouseX >= row.btnRect.x && mouseX < row.btnRect.x + row.btnRect.w &&
            mouseY >= row.btnRect.y && mouseY < row.btnRect.y + row.btnRect.h)
        {
            // 打开文件选择对话框
            wchar_t path[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = s_parentHwnd;
            ofn.lpstrFilter = L"音频文件\0*.wav;*.ogg\0所有文件\0*.*\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = row.label;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

            if (GetOpenFileNameW(&ofn))
            {
                config::Settings cfg = config::Load();
                switch (row.id)
                {
                case 1: cfg.snd_1 = path; break;
                case 2: cfg.snd_2 = path; break;
                case 3: cfg.snd_3 = path; break;
                case 4: cfg.snd_4 = path; break;
                case 5: cfg.snd_5 = path; break;
                case -1: cfg.snd_extra = path; break;
                }
                config::Save(cfg);
                s_needsRedraw = true;
            }
            break;
        }
    }
}

// ----------------------------------------------------------
void Resize(int width, int height)
{
    s_panelW = width;
    s_panelH = height;
    if (s_sdlWindow)
        SDL_SetWindowSize(s_sdlWindow, width, height);
    s_needsRedraw = true;
}

// ----------------------------------------------------------
void Shutdown()
{
    if (s_font) { TTF_CloseFont(s_font); s_font = nullptr; }
    TTF_Quit();
    if (s_renderer) { SDL_DestroyRenderer(s_renderer); s_renderer = nullptr; }
    if (s_sdlWindow) { SDL_DestroyWindow(s_sdlWindow); s_sdlWindow = nullptr; }
}

// ----------------------------------------------------------
bool NeedsRedraw() { return s_needsRedraw; }

} // namespace sound_ui