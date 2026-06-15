// StrikeSense.cpp — GDI+ 水色系 多页面版
//

#include "framework.h"
#include "StrikeSense.h"
#include "console.h"
#include "steam_helper.h"
#include "gsi_server.h"
#include "config.h"
#include "sound_player.h"
#include <iostream>
#include <filesystem>
#include <ShlObj.h>
#include <gdiplus.h>
#include <commdlg.h>

#pragma comment(lib, "gdiplus.lib")

namespace fs = std::filesystem;
#define MAX_LOADSTRING 100
#define SIDEBAR_W 140

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING], szWindowClass[MAX_LOADSTRING];
Console g_Console;
std::wstring g_gsiCfgPath;
static ULONG_PTR g_gdiToken = 0;

// 即时音量调整器变量
float g_death_vol = 0.8f;

// 当前页面: 0=文件位置, 1=遗产核心, 2=进化分支
static int g_currentPage = 0;

// ============================================================
// 音效行定义
// ============================================================
struct SoundRow {
    int id;
    const wchar_t* label, *defName;
    std::wstring config::Settings::*cfgPtr;
    Gdiplus::RectF btnRect;
};

static SoundRow s_sounds[] = {
    { 1, L"一杀 (1 kill)", L"1.wav", &config::Settings::snd_1 },
    { 2, L"二杀 (2 kills)",L"2.wav",&config::Settings::snd_2},
    { 3, L"三杀 (3 kills)",L"3.wav",&config::Settings::snd_3},
    { 4, L"四杀 (4 kills)",L"4.wav",&config::Settings::snd_4},
    { 5, L"五杀 (5 kills)",L"5.wav",&config::Settings::snd_5},
    { -1,L"多杀/死斗",L"deathmatch.wav",&config::Settings::snd_extra},
    { -2,L"MVP",L"mvp.wav",&config::Settings::snd_mvp},
    { -3,L"回合胜利",L"win.wav",&config::Settings::snd_win},
    { -4,L"回合失败",L"lose.wav",&config::Settings::snd_lose},
    { -13,L"回合开始",L"round.wav",&config::Settings::snd_round},
    { -14,L"购买阶段",L"buy.wav",&config::Settings::snd_buy},
    { -12,L"炸弹",L"bomb.wav",&config::Settings::snd_bomb},
    { -18,L"死亡",L"death.wav",&config::Settings::snd_death},
    { -19,L"游戏结束",L"gameover.wav",&config::Settings::snd_gameover},
    { -21,L"菜单",L"menu.wav",&config::Settings::snd_menu},
    { -99,L"闪光(可能)",L"flash.bmp",nullptr},  // 闪光占据控件
};
static constexpr int SND_COUNT = sizeof(s_sounds)/sizeof(s_sounds[0]);

// ============================================================
// 页面枚举
// ============================================================
enum Page { PAGE_SOUNDS = 0, PAGE_SETTINGS = 1, PAGE_EVOLUTION = 2 };

// 前向声明
ATOM MyRegisterClass(HINSTANCE);
BOOL InitInstance(HINSTANCE,int);
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK About(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK ConfirmPathDlgProc(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK SettingsDlgProc(HWND,UINT,WPARAM,LPARAM);
static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND);
static void OnBrowse(HWND,int);

// 绘制侧边栏
static void PaintSidebar(Gdiplus::Graphics& g, int W, int H);
// 绘制各页内容
static void PaintSoundsPage(Gdiplus::Graphics& g, int contentX, int contentW, int H, HWND hw);
static void PaintSettingsPage(Gdiplus::Graphics& g, int contentX, int contentW, int H, HWND hw);
static void PaintEvolutionPage(Gdiplus::Graphics& g, int contentX, int contentW, int H, HWND hw);
// 检查按钮点击
static void CheckVolumeSliderClick(HWND hw, int mx, int my);
static void CheckSettingsCheckboxClick(HWND hw, int mx, int my);

// ============================================================
// WinMain
// ============================================================
int APIENTRY wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nSC)
{
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&g_gdiToken,&in,nullptr);
    g_Console.InitRedirection();
    config::EnsureDirectoriesExist(); config::Load();
    sound::Init(); sound::PreloadSounds();
    if(gsi::Initialize()) gsi::StartServer();
    LoadStringW(hI,IDS_APP_TITLE,szTitle,MAX_LOADSTRING);
    LoadStringW(hI,IDC_STRIKESENSE,szWindowClass,MAX_LOADSTRING);
    MyRegisterClass(hI);
    if(!InitInstance(hI,nSC))return FALSE;
    HACCEL hAcc=LoadAccelerators(hI,MAKEINTRESOURCE(IDC_STRIKESENSE));
    MSG m;
    while(GetMessage(&m,nullptr,0,0)){
        if(!TranslateAccelerator(m.hwnd,hAcc,&m)){TranslateMessage(&m);DispatchMessage(&m);}
    }
    gsi::StopServer();gsi::Cleanup();sound::Quit();
    Gdiplus::GdiplusShutdown(g_gdiToken);
    return (int)m.wParam;
}

ATOM MyRegisterClass(HINSTANCE hI){
    WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc;wc.hInstance=hI;wc.hIcon=LoadIcon(hI,MAKEINTRESOURCE(IDI_STRIKESENSE));
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName=MAKEINTRESOURCEW(IDC_STRIKESENSE);wc.lpszClassName=szWindowClass;
    wc.hIconSm=LoadIcon(wc.hInstance,MAKEINTRESOURCE(IDI_SMALL));return RegisterClassExW(&wc);
}

BOOL InitInstance(HINSTANCE hI,int nSC){
    hInst=hI;
    HWND w=CreateWindowW(szWindowClass,szTitle,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,0,800,720,nullptr,nullptr,hI,nullptr);
    if(!w)return FALSE;
    ShowWindow(w,nSC);UpdateWindow(w);return TRUE;
}

// ============================================================
// 文件浏览
// ============================================================
static void OnBrowse(HWND hWnd,int id){
    if(id==-99){
        // 闪光 is a special case: browse for image file
        wchar_t p[MAX_PATH]={};
        OPENFILENAMEW o={};o.lStructSize=sizeof(o);o.hwndOwner=hWnd;
        o.lpstrFilter=L"图片\0*.bmp;*.png;*.jpg\0All\0*.*\0";o.lpstrFile=p;o.nMaxFile=MAX_PATH;
        o.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
        if(!GetOpenFileNameW(&o))return;
        // Save flash path to a global variable or config... for now just store
        InvalidateRect(hWnd,nullptr,FALSE);
        return;
    }
    wchar_t p[MAX_PATH]={};
    OPENFILENAMEW o={};o.lStructSize=sizeof(o);o.hwndOwner=hWnd;
    o.lpstrFilter=L"音频\0*.wav;*.ogg\0All\0*.*\0";o.lpstrFile=p;o.nMaxFile=MAX_PATH;
    o.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
    if(!GetOpenFileNameW(&o))return;
    config::Settings c=config::Load();
    switch(id){case 1:c.snd_1=p;break;case 2:c.snd_2=p;break;case 3:c.snd_3=p;break;
    case 4:c.snd_4=p;break;case 5:c.snd_5=p;break;case -1:c.snd_extra=p;break;
    case -2:c.snd_mvp=p;break;case -3:c.snd_win=p;break;case -4:c.snd_lose=p;break;
    case -12:c.snd_bomb=p;break;case -13:c.snd_round=p;break;case -14:c.snd_buy=p;break;
    case -18:c.snd_death=p;break;case -19:c.snd_gameover=p;break;case -21:c.snd_menu=p;break;}
    config::Save(c);InvalidateRect(hWnd,nullptr,FALSE);
}

// ============================================================
// GDI+ 绘制整个窗口
// ============================================================
static void PaintAll(HWND hw,HDC hdc){
    using namespace Gdiplus;
    RECT rc;GetClientRect(hw,&rc);
    int W=rc.right-rc.left,H=rc.bottom-rc.top;
    HDC md=CreateCompatibleDC(hdc);HBITMAP mb=CreateCompatibleBitmap(hdc,W,H);
    HBITMAP ob=(HBITMAP)SelectObject(md,mb);
    Graphics g(md);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    PaintSidebar(g,W,H);

    int contentX = SIDEBAR_W + 12;
    int contentW = W - contentX - 12;

    switch(g_currentPage){
    case PAGE_SOUNDS: PaintSoundsPage(g,contentX,contentW,H,hw); break;
    case PAGE_SETTINGS: PaintSettingsPage(g,contentX,contentW,H,hw); break;
    case PAGE_EVOLUTION: PaintEvolutionPage(g,contentX,contentW,H,hw); break;
    }

    BitBlt(hdc,0,0,W,H,md,0,0,SRCCOPY);SelectObject(md,ob);DeleteObject(mb);DeleteDC(md);
}

// ============================================================
// 侧边栏
// ============================================================
static void PaintSidebar(Gdiplus::Graphics& g, int W, int H){
    using namespace Gdiplus;
    SolidBrush sidebarBg(Color(255,200,230,250));
    Pen sidebarLine(Color(255,160,210,240),2.0f);
    Font titleFont(L"Microsoft YaHei",16,FontStyleBold);
    Font navFont(L"Microsoft YaHei",12);
    Font navActiveFont(L"Microsoft YaHei",12,FontStyleBold);
    SolidBrush textBright(Color(255,20,80,140));
    SolidBrush textDark(Color(255,30,60,100));
    SolidBrush navActiveBg(Color(255,160,210,245));

    g.FillRectangle(&sidebarBg,0,0,SIDEBAR_W,H);
    g.DrawLine(&sidebarLine,SIDEBAR_W,0,SIDEBAR_W,H);
    g.DrawString(L"StrikeSense",-1,&titleFont,PointF(10,12),&textBright);

    // 导航项
    struct NavItem { const wchar_t* text; int page; int y; };
    NavItem items[] = {
        {L"文件位置", PAGE_SOUNDS, 52},
        {L"遗产核心", PAGE_SETTINGS, 82},
        {L"进化分支", PAGE_EVOLUTION, 112},
    };
    for(auto& item : items){
        if(g_currentPage==item.page){
            g.FillRectangle(&navActiveBg,8,item.y,SIDEBAR_W-16,24);
        }
        Font& f = (g_currentPage==item.page) ? navActiveFont : navFont;
        g.DrawString(item.text,-1,&f,PointF(14,item.y+3),
            g_currentPage==item.page ? &textBright : &textDark);
    }
}

// ============================================================
// 文件位置页面
// ============================================================
static void PaintSoundsPage(Gdiplus::Graphics& g, int contentX, int contentW, int H, HWND hw){
    using namespace Gdiplus;
    SolidBrush bg(Color(255,240,248,255));
    SolidBrush headerBg(Color(255,180,220,245));
    Font pageFont(L"Microsoft YaHei",13,FontStyleBold);
    Font rowFont(L"Microsoft YaHei",11);
    Font btnFont(L"Microsoft YaHei",9);
    Font sF(L"Microsoft YaHei",9);
    SolidBrush textDark(Color(255,30,60,100));
    SolidBrush textDim(Color(255,100,130,160));
    SolidBrush textBright(Color(255,20,80,140));
    SolidBrush brHeader(Color(255,180,220,245));
    SolidBrush rowBg0(Color(255,220,240,255));
    SolidBrush rowBg1(Color(255,240,248,255));
    SolidBrush btnBg(Color(255,140,200,240));
    SolidBrush btnHoverBg(Color(255,60,160,230));
    SolidBrush btnText(Color(255,255,255,255));
    Pen btnPen(Color(255,100,170,220));

    g.FillRectangle(&brHeader,contentX,8,contentW,34);
    g.DrawString(L"文件位置",-1,&pageFont,PointF(contentX+10,14),&textBright);

    // 状态
    config::Settings c=config::Load();
    wchar_t st[256];swprintf_s(st,L"GSI: %s  |  音效: %s  |  音乐包: %s  |  低内存: %s  |  音量: %.0f%%",
        gsi::IsRunning()?L"运行中":L"未启动",
        c.enable_kill_sound?L"已启用":L"已禁用",c.custom_musickit?L"已启用":L"已禁用",
        c.low_memory?L"已启用":L"已禁用",c.volume*100.f);
    g.DrawString(st,-1,&sF,PointF(contentX+10,48),&textDim);

    int y=70;const int rh=30;
    for(int i=0;i<SND_COUNT;++i){
        auto&r=s_sounds[i];
        SolidBrush* bg = (i%2==0) ? &rowBg0 : &rowBg1;
        g.FillRectangle(bg,contentX,y,contentW,rh);
        g.DrawString(r.label,-1,&rowFont,PointF(contentX+8,y+5),&textDark);
        std::wstring nm=r.defName;bool isDef=true;
        if(r.id == -99) nm = L"img/flash.bmp"; // 闪光默认
        else if(r.cfgPtr){
            std::wstring pp=c.*(r.cfgPtr);
            if(!pp.empty()){fs::path p(pp);nm=p.filename().wstring();isDef=false;}
        }
        SolidBrush* nmB = isDef ? &textDim : &textDark;
        g.DrawString(nm.c_str(),-1,&rowFont,PointF(contentX+200,y+5),nmB);
        int bx=contentX+contentW-100,by=y+2,bw=80,bh=26;
        r.btnRect=RectF((REAL)bx,(REAL)by,(REAL)bw,(REAL)bh);
        GraphicsPath bp;float rx=8.f,ry=8.f;
        bp.AddArc((REAL)bx,(REAL)by,2*rx,2*ry,180,90);
        bp.AddArc((REAL)(bx+bw-2*rx),(REAL)by,2*rx,2*ry,270,90);
        bp.AddArc((REAL)(bx+bw-2*rx),(REAL)(by+bh-2*ry),2*rx,2*ry,0,90);
        bp.AddArc((REAL)bx,(REAL)(by+bh-2*ry),2*rx,2*ry,90,90);
        bp.CloseFigure();
        POINT pt;GetCursorPos(&pt);ScreenToClient(hw,&pt);
        bool hv=(pt.x>=bx&&pt.x<=bx+bw&&pt.y>=by&&pt.y<=by+bh);
        g.FillPath(hv?&btnHoverBg:&btnBg,&bp);g.DrawPath(&btnPen,&bp);
        g.DrawString(L"选择...",-1,&btnFont,PointF(bx+12,by+6),&btnText);
        y+=rh+2;
    }
}

// ============================================================
// 遗产核心页面（设置）— GDI+ 版
// ============================================================
static bool s_toggleStates[6] = {false,true,false,false,false,false};

static void PaintSettingsPage(Gdiplus::Graphics& g, int contentX, int contentW, int H, HWND hw){
    using namespace Gdiplus;
    SolidBrush bg(Color(255,240,248,255));
    SolidBrush headerBg(Color(255,180,220,245));
    Font pageFont(L"Microsoft YaHei",13,FontStyleBold);
    Font toggleFont(L"Microsoft YaHei",12);
    SolidBrush textDark(Color(255,30,60,100));
    SolidBrush textBright(Color(255,20,80,140));
    SolidBrush toggleOn(Color(255,100,200,140));
    SolidBrush toggleOff(Color(255,180,180,190));
    SolidBrush toggleKnob(Color(255,255,255,255));
    SolidBrush sliderBg(Color(255,200,220,240));
    SolidBrush sliderFill(Color(255,80,180,240));
    Font sF(L"Microsoft YaHei",9);
    SolidBrush textDim(Color(255,100,130,160));

    g.FillRectangle(&headerBg,contentX,8,contentW,34);
    g.DrawString(L"遗产核心 - 设置",-1,&pageFont,PointF(contentX+10,14),&textBright);

    config::Settings c=config::Load();
    // 同步变量
    s_toggleStates[0] = c.custom_musickit;
    s_toggleStates[1] = c.enable_kill_sound;
    s_toggleStates[2] = c.custom_flashbang;
    s_toggleStates[3] = c.low_memory;
    s_toggleStates[4] = c.show_mvp;
    s_toggleStates[5] = c.ogg;

    const wchar_t* labels[] = {
        L"自定义音乐包 (custom_musickit)",
        L"启用击杀音效替换 (enable_kill_sound)",
        L"闪光叠加页面 (custom_flashbang)",
        L"低内存模式 (low_memory)",
        L"显示MVP信息板 (show_mvp)",
        L"使用OGG音频格式 (ogg)"
    };

    int y = 50;
    for(int i=0;i<6;++i){
        g.DrawString(labels[i],-1,&toggleFont,PointF(contentX+10,y),&textDark);
        // 开关
        int tx=contentX+contentW-60,ty=y;
        RectF toggleRect((REAL)tx,(REAL)ty,50.f,24.f);
        GraphicsPath tp;
        float rr=12.f;
        tp.AddArc((REAL)tx,(REAL)ty,2*rr,2*rr,90,180);
        tp.AddArc((REAL)(tx+50-2*rr),(REAL)ty,2*rr,2*rr,270,180);
        tp.CloseFigure();
        g.FillPath(s_toggleStates[i]?&toggleOn:&toggleOff,&tp);
        // 滑块
        float knobX = s_toggleStates[i] ? tx+50-22.f : tx+2.f;
        g.FillEllipse(&toggleKnob,knobX,ty+2.f,20.f,20.f);
        y += 36;
    }

    // 音量滑块
    y += 10;
    g.DrawString(L"音量",-1,&toggleFont,PointF(contentX+10,y),&textDark);
    int sliderW = contentW-80;
    g.FillRectangle(&sliderBg,contentX+80,y,sliderW,8);
    int fillW = (int)(sliderW * c.volume);
    g.FillRectangle(&sliderFill,contentX+80,y,fillW,8);
    wchar_t vt[32];swprintf_s(vt,L"%.0f%%",c.volume*100.f);
    g.DrawString(vt,-1,&toggleFont,PointF(contentX+80+sliderW+8,y-4),&textDark);
}

// ============================================================
// 进化分支页面
// ============================================================
static void PaintEvolutionPage(Gdiplus::Graphics& g, int contentX, int contentW, int H, HWND hw){
    using namespace Gdiplus;
    SolidBrush bg(Color(255,240,248,255));
    SolidBrush headerBg(Color(255,180,220,245));
    Font pageFont(L"Microsoft YaHei",13,FontStyleBold);
    Font rowFont(L"Microsoft YaHei",11);
    SolidBrush textDark(Color(255,30,60,100));
    SolidBrush textBright(Color(255,20,80,140));
    SolidBrush textDim(Color(255,100,130,160));
    SolidBrush sliderBg(Color(255,200,220,240));
    SolidBrush sliderFill(Color(255,80,180,240));

    g.FillRectangle(&headerBg,contentX,8,contentW,34);
    g.DrawString(L"进化分支",-1,&pageFont,PointF(contentX+10,14),&textBright);

    int y = 50;
    // 瞬时音量调整器（滑块）
    g.DrawString(L"即时音量调整器 (death_vol)",-1,&rowFont,PointF(contentX+10,y),&textDark);
    int sliderW = contentW-100;
    g.FillRectangle(&sliderBg,contentX+10,y+30,sliderW,10);
    int fillW = (int)(sliderW * g_death_vol);
    g.FillRectangle(&sliderFill,contentX+10,y+30,fillW,10);

    // 滑块把手
    SolidBrush knobBr(Color(255,60,160,230));
    float knobX = contentX+10 + fillW - 8.f;
    g.FillEllipse(&knobBr,knobX,y+24.f,16.f,16.f);

    wchar_t vt[32];swprintf_s(vt,L"%.0f%%",g_death_vol*100.f);
    g.DrawString(vt,-1,&rowFont,PointF(contentX+20+sliderW,y+26),&textDark);

    y += 70;
    // 快捷键组合选择
    g.DrawString(L"即时音量快捷键组合",-1,&rowFont,PointF(contentX+10,y),&textDark);
    Font smallFont(L"Microsoft YaHei",9);
    g.DrawString(L"(点击字母键选择，Ctrl/Alt/Shift 作为修饰键)",-1,&smallFont,PointF(contentX+10,y+22),&textDim);
    g.DrawString(L"当前: Ctrl+K",-1,&rowFont,PointF(contentX+10,y+44),&textDim);
}

// ============================================================
// 侧边栏点击检测
// ============================================================
static void CheckSidebarClick(int mx, int my){
    struct { int y; int page; } items[] = {
        {52, PAGE_SOUNDS}, {82, PAGE_SETTINGS}, {112, PAGE_EVOLUTION}
    };
    for(auto& item : items){
        if(mx>=8 && mx<=SIDEBAR_W && my>=item.y && my<=item.y+24){
            g_currentPage = item.page;
            InvalidateRect(GetForegroundWindow(),nullptr,FALSE);
            return;
        }
    }
}

// ============================================================
// 设置页面开关点击
// ============================================================
static void CheckSettingsCheckboxClick(HWND hw, int mx, int my){
    int contentX = SIDEBAR_W + 12;
    int contentW = 0; {RECT rc;GetClientRect(hw,&rc);contentW=rc.right-rc.left-contentX-12;}
    for(int i=0;i<6;++i){
        int tx=contentX+contentW-60, ty=50+i*36;
        if(mx>=tx && mx<=tx+50 && my>=ty && my<=ty+24){
            s_toggleStates[i] = !s_toggleStates[i];
            config::Settings c=config::Load();
            c.custom_musickit = s_toggleStates[0];
            c.enable_kill_sound = s_toggleStates[1];
            c.custom_flashbang = s_toggleStates[2];
            c.low_memory = s_toggleStates[3];
            c.show_mvp = s_toggleStates[4];
            c.ogg = s_toggleStates[5];
            config::Save(c);
            InvalidateRect(hw,nullptr,FALSE);
            return;
        }
    }
}

// ============================================================
// 进化分支滑块点击
// ============================================================
static void CheckVolumeSliderClick(HWND hw, int mx, int my){
    if(g_currentPage != PAGE_EVOLUTION) return;
    int contentX = SIDEBAR_W+12;
    int sliderY = 50+30;
    int sliderW = 0; {RECT rc;GetClientRect(hw,&rc);sliderW=rc.right-rc.left-contentX-12-100;}
    int sliderX = contentX+10;
    if(mx>=sliderX && mx<=sliderX+sliderW && my>=sliderY-10 && my<=sliderY+20){
        float t = (float)(mx-sliderX)/(float)sliderW;
        if(t<0.f)t=0.f; if(t>1.f)t=1.f;
        g_death_vol = t;
        InvalidateRect(hw,nullptr,FALSE);
    }
}

// ============================================================
// WndProc
// ============================================================
LRESULT CALLBACK WndProc(HWND hw,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_CREATE:std::cout<<"StrikeSense "<<std::endl;break;
    case WM_PAINT:{PAINTSTRUCT ps;HDC hdc=BeginPaint(hw,&ps);PaintAll(hw,hdc);EndPaint(hw,&ps);break;}
    case WM_LBUTTONDOWN:{
        int mx=LOWORD(lp),my=HIWORD(lp);
        if(mx<SIDEBAR_W){CheckSidebarClick(mx,my);break;}
        // 检测各页面的按钮
        switch(g_currentPage){
        case PAGE_SOUNDS:
            for(int i=0;i<SND_COUNT;++i){
                auto&r=s_sounds[i];
                if(mx>=r.btnRect.X&&mx<=r.btnRect.X+r.btnRect.Width&&
                   my>=r.btnRect.Y&&my<=r.btnRect.Y+r.btnRect.Height){OnBrowse(hw,r.id);break;}
            }
            break;
        case PAGE_SETTINGS:
            CheckSettingsCheckboxClick(hw,mx,my);
            break;
        case PAGE_EVOLUTION:
            CheckVolumeSliderClick(hw,mx,my);
            break;
        }
        break;}
    case WM_COMMAND:{
        int id=LOWORD(wp);
        switch(id){
        case IDM_CREATE_GSI_CFG:OnCreateGSIConfig(hw);break;
        case IDM_DEBUGGER:g_Console.ShowDebugger(hInst,hw);break;
        case IDM_ABOUT:DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUTBOX),hw,About);break;
        case IDM_SETTINGS:g_currentPage=PAGE_SETTINGS;InvalidateRect(hw,nullptr,FALSE);break;
        case IDM_EXIT:DestroyWindow(hw);break;
        default:return DefWindowProc(hw,m,wp,lp);}break;}
    case WM_DESTROY:PostQuitMessage(0);break;
    default:return DefWindowProc(hw,m,wp,lp);}
    return 0;
}

static std::wstring GetCS2CfgPath(){
    std::wstring sv=strikesense::LoadSavedCfgPath();if(!sv.empty()&&fs::exists(sv))return sv;
    std::wstring sp=strikesense::GetSteamPathFromRegistry();if(sp.empty())return L"";
    std::wstring cd=strikesense::FindCS2InstallDir(sp);if(cd.empty())return L"";
    return strikesense::GetCS2CfgPath(cd);
}
static void OnCreateGSIConfig(HWND hw){
    g_gsiCfgPath=GetCS2CfgPath();
    if(g_gsiCfgPath.empty()){wchar_t p[MAX_PATH]={};BROWSEINFOW b={};b.hwndOwner=hw;
    b.lpszTitle=L"选择CS2cfg目录";b.ulFlags=BIF_RETURNONLYFSDIRS;
    LPITEMIDLIST pid=SHBrowseForFolderW(&b);if(!pid)return;
    SHGetPathFromIDListW(pid,p);g_gsiCfgPath=p;IMalloc*m=nullptr;if(SUCCEEDED(SHGetMalloc(&m))){m->Free(pid);m->Release();}}
    DialogBoxW(hInst,MAKEINTRESOURCEW(IDD_CONFIRM_PATH),hw,ConfirmPathDlgProc);
}
INT_PTR CALLBACK ConfirmPathDlgProc(HWND hD,UINT m,WPARAM wp,LPARAM lp){
    switch(m){case WM_INITDIALOG:SetDlgItemTextW(hD,IDC_PATH_LABEL,g_gsiCfgPath.c_str());
    {RECT r;GetWindowRect(GetParent(hD),&r);SetWindowPos(hD,nullptr,r.left+(r.right-r.left)/2-225,r.top+(r.bottom-r.top)/2-108,0,0,SWP_NOSIZE|SWP_NOZORDER);}return TRUE;
    case WM_COMMAND:switch(LOWORD(wp)){
    case IDYES:if(strikesense::WriteGSIConfig(g_gsiCfgPath)){strikesense::SaveCfgPath(g_gsiCfgPath);MessageBoxW(hD,L"成功！",L"提示",MB_OK);}else MessageBoxW(hD,L"失败！",L"错误",MB_OK);EndDialog(hD,IDYES);return TRUE;
    case IDC_DELETE_CFG:{fs::path f=fs::path(g_gsiCfgPath)/L"gamestate_integration_square.cfg";std::error_code ec;fs::remove(f,ec);MessageBoxW(hD,ec?L"失败":L"已删除",L"提示",MB_OK);return TRUE;}
    case IDC_BROWSE_BTN:{wchar_t p[MAX_PATH]={};BROWSEINFOW b={};b.hwndOwner=hD;b.lpszTitle=L"选择cfg目录";b.ulFlags=BIF_RETURNONLYFSDIRS;LPITEMIDLIST pid=SHBrowseForFolderW(&b);if(pid){SHGetPathFromIDListW(pid,p);g_gsiCfgPath=p;SetDlgItemTextW(hD,IDC_PATH_LABEL,p);IMalloc*mm=nullptr;if(SUCCEEDED(SHGetMalloc(&mm))){mm->Free(pid);mm->Release();}}return TRUE;}
    case IDCANCEL:EndDialog(hD,IDCANCEL);return TRUE;}}return FALSE;}
INT_PTR CALLBACK SettingsDlgProc(HWND hD,UINT m,WPARAM wp,LPARAM lp){
    // Legacy dialog retained for backward compatibility — now GDI+ handles it
    (void)hD;(void)m;(void)wp;(void)lp;return TRUE;}
INT_PTR CALLBACK About(HWND hD,UINT m,WPARAM wp,LPARAM lp){
    UNREFERENCED_PARAMETER(lp);switch(m){case WM_INITDIALOG:return(INT_PTR)TRUE;
    case WM_COMMAND:if(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)EndDialog(hD,LOWORD(wp));return(INT_PTR)TRUE;}return(INT_PTR)FALSE;}