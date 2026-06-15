// StrikeSense.cpp — GDI+ 水色系 多页面版 + 快捷键 + 十字准星
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
bool  g_deathMute = false;

// 快捷键
#define HOTKEY_TOGGLE_VOL 1

// 准星参数
bool  g_crosshairEnabled = false;
int   g_crosshairR = 255, g_crosshairG = 0, g_crosshairB = 0;
int   g_crosshairStyle = 0;  // 0=空心圆, 1=实心圆, 2=经典
int   g_crosshairThickness = 2;
float g_crosshairScale = 1.0f;

// 准星覆盖窗口
static HWND g_crossHWnd = nullptr;

// 当前页面: 0=文件位置, 1=遗产核心, 2=进化分支
static int g_currentPage = 0;

struct SoundRow {
    int id;
    const wchar_t* label, *defName;
    std::wstring config::Settings::*cfgPtr;
    Gdiplus::RectF btnRect;
};

static SoundRow s_sounds[] = {
    { 1, L"一杀", L"1.wav", &config::Settings::snd_1 },
    { 2, L"二杀",L"2.wav",&config::Settings::snd_2},
    { 3, L"三杀",L"3.wav",&config::Settings::snd_3},
    { 4, L"四杀",L"4.wav",&config::Settings::snd_4},
    { 5, L"五杀",L"5.wav",&config::Settings::snd_5},
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
    { -99,L"闪光(可能)",L"flash.bmp",nullptr},
};
static constexpr int SND_COUNT = sizeof(s_sounds)/sizeof(s_sounds[0]);

enum Page { PAGE_SOUNDS = 0, PAGE_SETTINGS = 1, PAGE_EVOLUTION = 2 };

ATOM MyRegisterClass(HINSTANCE);
BOOL InitInstance(HINSTANCE,int);
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK About(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK ConfirmPathDlgProc(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK SettingsDlgProc(HWND,UINT,WPARAM,LPARAM);
static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND);
static void OnBrowse(HWND,int);

static void PaintSidebar(Gdiplus::Graphics& g, int W, int H);
static void PaintSoundsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
static void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
static void PaintEvolutionPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);

static LRESULT CALLBACK CrosshairWndProc(HWND,UINT,WPARAM,LPARAM);
static void CreateCrosshairWindow(HINSTANCE);
static void DestroyCrosshairWindow();

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

    // 注册 Ctrl+K 快捷键
    RegisterHotKey(nullptr, HOTKEY_TOGGLE_VOL, MOD_CONTROL, 'K');

    HACCEL hAcc=LoadAccelerators(hI,MAKEINTRESOURCE(IDC_STRIKESENSE));
    MSG m;
    while(GetMessage(&m,nullptr,0,0)){
        if(m.message == WM_HOTKEY && m.wParam == HOTKEY_TOGGLE_VOL){
            g_deathMute = !g_deathMute;
            std::cout << "即时音量降低: " << (g_deathMute ? "开启" : "关闭") << std::endl;
            InvalidateRect(FindWindowW(szWindowClass,nullptr),nullptr,FALSE);
            continue;
        }
        if(!TranslateAccelerator(m.hwnd,hAcc,&m)){TranslateMessage(&m);DispatchMessage(&m);}
    }
    UnregisterHotKey(nullptr, HOTKEY_TOGGLE_VOL);
    gsi::StopServer();gsi::Cleanup();sound::Quit();
    Gdiplus::GdiplusShutdown(g_gdiToken);
    return (int)m.wParam;
}

ATOM MyRegisterClass(HINSTANCE hI){
    WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc;wc.hInstance=hI;wc.hIcon=LoadIcon(hI,MAKEINTRESOURCE(IDI_STRIKESENSE));
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName=MAKEINTRESOURCEW(IDC_STRIKESENSE);wc.lpszClassName=szWindowClass;
    wc.hIconSm=LoadIcon(wc.hInstance,MAKEINTRESOURCE(IDI_SMALL));return RegisterClassExW(&wc);
}

BOOL InitInstance(HINSTANCE hI,int nSC){
    hInst=hI;
    HWND w=CreateWindowW(szWindowClass,szTitle,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,0,820,740,nullptr,nullptr,hI,nullptr);
    if(!w)return FALSE;
    ShowWindow(w,nSC);UpdateWindow(w);return TRUE;
}

static void OnBrowse(HWND hWnd,int id){
    wchar_t p[MAX_PATH]={};OPENFILENAMEW o={};o.lStructSize=sizeof(o);o.hwndOwner=hWnd;
    if(id==-99){o.lpstrFilter=L"图片\0*.bmp;*.png;*.jpg\0All\0*.*\0";}
    else{o.lpstrFilter=L"音频\0*.wav;*.ogg\0All\0*.*\0";}
    o.lpstrFile=p;o.nMaxFile=MAX_PATH;o.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
    if(!GetOpenFileNameW(&o))return;
    if(id==-99){InvalidateRect(hWnd,nullptr,FALSE);return;}
    config::Settings c=config::Load();
    switch(id){case 1:c.snd_1=p;break;case 2:c.snd_2=p;break;case 3:c.snd_3=p;break;
    case 4:c.snd_4=p;break;case 5:c.snd_5=p;break;case -1:c.snd_extra=p;break;
    case -2:c.snd_mvp=p;break;case -3:c.snd_win=p;break;case -4:c.snd_lose=p;break;
    case -12:c.snd_bomb=p;break;case -13:c.snd_round=p;break;case -14:c.snd_buy=p;break;
    case -18:c.snd_death=p;break;case -19:c.snd_gameover=p;break;case -21:c.snd_menu=p;break;}
    config::Save(c);InvalidateRect(hWnd,nullptr,FALSE);
}

static void PaintAll(HWND hw,HDC hdc){
    using namespace Gdiplus;
    RECT rc;GetClientRect(hw,&rc);
    int W=rc.right-rc.left,H=rc.bottom-rc.top;
    HDC md=CreateCompatibleDC(hdc);HBITMAP mb=CreateCompatibleBitmap(hdc,W,H);
    HBITMAP ob=(HBITMAP)SelectObject(md,mb);
    Graphics g(md);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    SolidBrush globalBg(Color(255,240,248,255));
    g.FillRectangle(&globalBg,0,0,W,H);

    PaintSidebar(g,W,H);
    int cx=SIDEBAR_W+12,cw=W-cx-12;
    switch(g_currentPage){
    case PAGE_SOUNDS:PaintSoundsPage(g,cx,cw,H,hw);break;
    case PAGE_SETTINGS:PaintSettingsPage(g,cx,cw,H,hw);break;
    case PAGE_EVOLUTION:PaintEvolutionPage(g,cx,cw,H,hw);break;
    }
    BitBlt(hdc,0,0,W,H,md,0,0,SRCCOPY);SelectObject(md,ob);DeleteObject(mb);DeleteDC(md);
}

static void PaintSidebar(Gdiplus::Graphics& g,int W,int H){
    using namespace Gdiplus;
    SolidBrush sbBg(Color(255,200,230,250));Pen sbLn(Color(255,160,210,240),2.0f);
    Font tF(L"Microsoft YaHei",16,FontStyleBold),nF(L"Microsoft YaHei",12),nAF(L"Microsoft YaHei",12,FontStyleBold);
    SolidBrush tb(Color(255,20,80,140)),td(Color(255,30,60,100)),naB(Color(255,160,210,245));
    g.FillRectangle(&sbBg,0,0,SIDEBAR_W,H);g.DrawLine(&sbLn,SIDEBAR_W,0,SIDEBAR_W,H);
    g.DrawString(L"StrikeSense",-1,&tF,PointF(10,12),&tb);
    struct NavItem{const wchar_t*t;int p;int y;};
    NavItem items[]={{L"文件位置",0,52},{L"遗产核心",1,82},{L"进化分支",2,112}};
    for(auto&it:items){
        if(g_currentPage==it.p)g.FillRectangle(&naB,8,it.y,SIDEBAR_W-16,24);
        Font&f=(g_currentPage==it.p)?nAF:nF;
        g.DrawString(it.t,-1,&f,PointF(14,it.y+3),g_currentPage==it.p?&tb:&td);
    }
}

static void PaintSoundsPage(Gdiplus::Graphics& g,int cx,int cw,int H,HWND hw){
    using namespace Gdiplus;
    SolidBrush hdr(Color(255,180,220,245));
    Font pF(L"Microsoft YaHei",13,FontStyleBold),rF(L"Microsoft YaHei",11),bF(L"Microsoft YaHei",9),sF(L"Microsoft YaHei",9);
    SolidBrush tdCol(Color(255,30,60,100)),tmDim(Color(255,100,130,160)),tbCol(Color(255,20,80,140));
    SolidBrush r0(Color(255,220,240,255)),r1(Color(255,240,248,255));
    SolidBrush btnB(Color(255,140,200,240)),btnHB(Color(255,60,160,230)),btnT(Color(255,255,255,255));
    Pen btnP(Color(255,100,170,220));
    g.FillRectangle(&hdr,cx,8,cw,34);g.DrawString(L"文件位置",-1,&pF,PointF(cx+10,14),&tbCol);
    config::Settings c=config::Load();
    wchar_t st[256];swprintf_s(st,L"GSI:%s | 音效:%s | 音乐包:%s | 低内存:%s | 音量:%.0f%%",
        gsi::IsRunning()?L"运行中":L"未启动",c.enable_kill_sound?L"已启用":L"已禁用",
        c.custom_musickit?L"已启用":L"已禁用",c.low_memory?L"已启用":L"已禁用",c.volume*100.f);
    g.DrawString(st,-1,&sF,PointF(cx+10,48),&tmDim);
    int y=70;const int rh=30;
    for(int i=0;i<SND_COUNT;++i){
        auto&rw=s_sounds[i];SolidBrush*bg=(i%2==0)?&r0:&r1;
        g.FillRectangle(bg,cx,y,cw,rh);g.DrawString(rw.label,-1,&rF,PointF(cx+8,y+5),&tdCol);
        std::wstring nm=rw.defName;bool df=true;
        if(rw.id==-99)nm=L"img/flash.bmp";
        else if(rw.cfgPtr){std::wstring pp=c.*(rw.cfgPtr);if(!pp.empty()){fs::path p(pp);nm=p.filename().wstring();df=false;}}
        SolidBrush*nmB=df?&tmDim:&tdCol;g.DrawString(nm.c_str(),-1,&rF,PointF(cx+160,y+5),nmB);
        int bx=cx+cw-100,by=y+2,bw=80,bh=26;rw.btnRect=RectF((REAL)bx,(REAL)by,(REAL)bw,(REAL)bh);
        GraphicsPath bp;float rx=8.f,ry=8.f;
        bp.AddArc((REAL)bx,(REAL)by,2*rx,2*ry,180,90);
        bp.AddArc((REAL)(bx+bw-2*rx),(REAL)by,2*rx,2*ry,270,90);
        bp.AddArc((REAL)(bx+bw-2*rx),(REAL)(by+bh-2*ry),2*rx,2*ry,0,90);
        bp.AddArc((REAL)bx,(REAL)(by+bh-2*ry),2*rx,2*ry,90,90);bp.CloseFigure();
        POINT pt;GetCursorPos(&pt);ScreenToClient(hw,&pt);bool hv=(pt.x>=bx&&pt.x<=bx+bw&&pt.y>=by&&pt.y<=by+bh);
        g.FillPath(hv?&btnHB:&btnB,&bp);g.DrawPath(&btnP,&bp);
        g.DrawString(L"选择...",-1,&bF,PointF(bx+12,by+6),&btnT);
        y+=rh+2;
    }
}

static bool s_toggleStates[6]={false,true,false,false,false,false};
static void PaintSettingsPage(Gdiplus::Graphics& g,int cx,int cw,int H,HWND){
    using namespace Gdiplus;
    SolidBrush hdr(Color(255,180,220,245));Font pF(L"Microsoft YaHei",13,FontStyleBold),tF(L"Microsoft YaHei",12);
    SolidBrush tdCol(Color(255,30,60,100)),tbCol(Color(255,20,80,140)),onBr(Color(255,100,200,140)),offBr(Color(255,180,180,190)),kBr(Color(255,255,255,255));
    SolidBrush sBg(Color(255,200,220,240)),sFill(Color(255,80,180,240));
    g.FillRectangle(&hdr,cx,8,cw,34);g.DrawString(L"遗产核心",-1,&pF,PointF(cx+10,14),&tbCol);
    config::Settings c=config::Load();
    s_toggleStates[0]=c.custom_musickit;s_toggleStates[1]=c.enable_kill_sound;
    s_toggleStates[2]=c.custom_flashbang;s_toggleStates[3]=c.low_memory;
    s_toggleStates[4]=c.show_mvp;s_toggleStates[5]=c.ogg;
    const wchar_t*lbs[]={L"自定义音乐包",L"击杀音效替换",L"闪光叠加",L"低内存模式",L"MVP信息板",L"OGG格式"};
    int y=50;
    for(int i=0;i<6;++i){
        g.DrawString(lbs[i],-1,&tF,PointF(cx+10,y),&tdCol);
        int tx=cx+cw-60,ty=y;RectF tr((REAL)tx,(REAL)ty,50.f,24.f);
        GraphicsPath tp;float rr=12.f;
        tp.AddArc((REAL)tx,(REAL)ty,2*rr,2*rr,90,180);
        tp.AddArc((REAL)(tx+50-2*rr),(REAL)ty,2*rr,2*rr,270,180);tp.CloseFigure();
        g.FillPath(s_toggleStates[i]?&onBr:&offBr,&tp);
        float kx=s_toggleStates[i]?tx+50-22.f:tx+2.f;g.FillEllipse(&kBr,kx,ty+2.f,20.f,20.f);
        y+=36;
    }
    y+=10;g.DrawString(L"音量",-1,&tF,PointF(cx+10,y),&tdCol);
    int sw=cw-80;g.FillRectangle(&sBg,cx+80,y,sw,8);int fw=(int)(sw*c.volume);g.FillRectangle(&sFill,cx+80,y,fw,8);
    wchar_t vt[32];swprintf_s(vt,L"%.0f%%",c.volume*100.f);g.DrawString(vt,-1,&tF,PointF(cx+80+sw+8,y-4),&tdCol);
}

static void PaintEvolutionPage(Gdiplus::Graphics& g,int cx,int cw,int H,HWND){
    using namespace Gdiplus;
    SolidBrush hdr(Color(255,180,220,245));
    Font pF(L"Microsoft YaHei",13,FontStyleBold),rF(L"Microsoft YaHei",11),sF(L"Microsoft YaHei",9);
    SolidBrush tdCol(Color(255,30,60,100)),tbCol(Color(255,20,80,140)),tmDim(Color(255,100,130,160));
    SolidBrush sBg(Color(255,200,220,240)),sFill(Color(255,80,180,240)),knB(Color(255,60,160,230));
    g.FillRectangle(&hdr,cx,8,cw,34);g.DrawString(L"进化分支",-1,&pF,PointF(cx+10,14),&tbCol);
    int y=50;
    g.DrawString(L"即时音量调整器",-1,&rF,PointF(cx+10,y),&tdCol);
    int slW=cw-100;g.FillRectangle(&sBg,cx+10,y+30,slW,10);int fw=(int)(slW*g_death_vol);g.FillRectangle(&sFill,cx+10,y+30,fw,10);
    float kx=cx+10+fw-8.f;g.FillEllipse(&knB,kx,y+24.f,16.f,16.f);
    wchar_t vt[32];swprintf_s(vt,L"%.0f%%",g_death_vol*100.f);g.DrawString(vt,-1,&rF,PointF(cx+20+slW,y+26),&tdCol);
    y+=70;

    g.DrawString(L"快捷键 Ctrl+K",-1,&rF,PointF(cx+10,y),&tdCol);
    wchar_t hs[64];swprintf_s(hs,L"状态: %s",g_deathMute?L"降低开启":L"正常");g.DrawString(hs,-1,&sF,PointF(cx+10,y+20),&tmDim);
    y+=50;

    g.DrawString(L"狙击准星设置",-1,&rF,PointF(cx+10,y),&tdCol);
    y+=25;
    const wchar_t*rgbL[]={L"R",L"G",L"B"};int*rgbV[]={&g_crosshairR,&g_crosshairG,&g_crosshairB};
    for(int i=0;i<3;++i){
        g.DrawString(rgbL[i],-1,&sF,PointF(cx+10+i*160,y),&tdCol);
        int rgbW=100;g.FillRectangle(&sBg,cx+30+i*160,y,100,10);int fw2=(int)(100.f*(*rgbV[i])/255.f);g.FillRectangle(&sFill,cx+30+i*160,y,fw2,10);
        wchar_t bf[8];swprintf_s(bf,L"%d",*rgbV[i]);g.DrawString(bf,-1,&sF,PointF(cx+140+i*160,y-2),&tdCol);
    }
    y+=25;
    g.DrawString(L"样式:",-1,&sF,PointF(cx+10,y),&tdCol);
    const wchar_t*sty[]={L"空心圆",L"实心圆",L"经典"};g.DrawString(sty[g_crosshairStyle],-1,&rF,PointF(cx+60,y),&tbCol);
    g.DrawString(L"粗细:",-1,&sF,PointF(cx+180,y),&tdCol);int thW=80;g.FillRectangle(&sBg,cx+230,y,thW,10);int fw3=(int)(thW*g_crosshairThickness/10.f);g.FillRectangle(&sFill,cx+230,y,fw3,10);
    wchar_t thT[8];swprintf_s(thT,L"%d",g_crosshairThickness);g.DrawString(thT,-1,&sF,PointF(cx+320,y-2),&tdCol);
    y+=25;
    g.DrawString(L"缩放:",-1,&sF,PointF(cx+10,y),&tdCol);int scW=100;g.FillRectangle(&sBg,cx+60,y,scW,10);int fw4=(int)(scW*g_crosshairScale/3.f);g.FillRectangle(&sFill,cx+60,y,fw4,10);
    wchar_t scT[8];swprintf_s(scT,L"%.1f",g_crosshairScale);g.DrawString(scT,-1,&sF,PointF(cx+170,y-2),&tdCol);
    y+=30;
    g.DrawString(L"启用准星",-1,&rF,PointF(cx+10,y),&tdCol);
    {int tx=cx+120,ty=y-4;RectF tr((REAL)tx,(REAL)ty,50.f,24.f);GraphicsPath tp;float rr=12.f;
        tp.AddArc((REAL)tx,(REAL)ty,2*rr,2*rr,90,180);
        tp.AddArc((REAL)(tx+50-2*rr),(REAL)ty,2*rr,2*rr,270,180);tp.CloseFigure();
        SolidBrush onBr(Color(255,100,200,140)),offBr(Color(255,180,180,190)),kBr(Color(255,255,255,255));
        g.FillPath(g_crosshairEnabled?&onBr:&offBr,&tp);
        float kkx=g_crosshairEnabled?tx+50-22.f:tx+2.f;g.FillEllipse(&kBr,kkx,ty+2.f,20.f,20.f);}
}

static void CheckSidebarClick(int mx,int my){
    struct{int y;int p;}items[]={{52,0},{82,1},{112,2}};
    for(auto&it:items){if(mx>=8&&mx<=SIDEBAR_W&&my>=it.y&&my<=it.y+24){g_currentPage=it.p;InvalidateRect(FindWindowW(szWindowClass,nullptr),nullptr,FALSE);return;}}
}
static void CheckSettingsCheckboxClick(HWND hw,int mx,int my){
    int cx=SIDEBAR_W+12,cw=0;{RECT rc;GetClientRect(hw,&rc);cw=rc.right-rc.left-cx-12;}
    for(int i=0;i<6;++i){int tx=cx+cw-60,ty=50+i*36;if(mx>=tx&&mx<=tx+50&&my>=ty&&my<=ty+24){
        s_toggleStates[i]=!s_toggleStates[i];config::Settings c=config::Load();
        c.custom_musickit=s_toggleStates[0];c.enable_kill_sound=s_toggleStates[1];
        c.custom_flashbang=s_toggleStates[2];c.low_memory=s_toggleStates[3];
        c.show_mvp=s_toggleStates[4];c.ogg=s_toggleStates[5];config::Save(c);InvalidateRect(hw,nullptr,FALSE);return;}}
}
static void CheckEvolutionClick(HWND hw,int mx,int my){
    int cx=SIDEBAR_W+12,cw=0;{RECT rc;GetClientRect(hw,&rc);cw=rc.right-rc.left-cx-12;}
    int y=50,slW=cw-100;
    if(mx>=cx+10&&mx<=cx+10+slW&&my>=y+30-10&&my<=y+30+20){float t=(float)(mx-cx-10)/(float)slW;if(t<0)t=0;if(t>1)t=1;g_death_vol=t;InvalidateRect(hw,nullptr,FALSE);return;}
    for(int i=0;i<3;++i){int ry=50+70+25+i*25;int*vals[]={&g_crosshairR,&g_crosshairG,&g_crosshairB};
        if(mx>=cx+30+i*160&&mx<=cx+130+i*160&&my>=ry-8&&my<=ry+12){float t=(float)(mx-cx-30-i*160)/100.f;if(t<0)t=0;if(t>1)t=1;*vals[i]=(int)(t*255.f);InvalidateRect(hw,nullptr,FALSE);return;}}
    int sy=50+70+25*3;if(mx>=cx+60&&mx<=cx+140&&my>=sy-10&&my<=sy+12){g_crosshairStyle=(g_crosshairStyle+1)%3;InvalidateRect(hw,nullptr,FALSE);return;}
    int ty2=sy+25,thW=80;if(mx>=cx+230&&mx<=cx+230+thW&&my>=ty2-8&&my<=ty2+12){float t=(float)(mx-cx-230)/(float)thW;if(t<0)t=0;if(t>1)t=1;g_crosshairThickness=1+(int)(t*9.f);InvalidateRect(hw,nullptr,FALSE);return;}
    int sy3=ty2+25,scW=100;if(mx>=cx+60&&mx<=cx+60+scW&&my>=sy3-8&&my<=sy3+12){float t=(float)(mx-cx-60)/(float)scW;if(t<0)t=0;if(t>1)t=1;g_crosshairScale=0.5f+t*2.5f;InvalidateRect(hw,nullptr,FALSE);return;}
    int ey=sy3+30,tx=cx+120,tye=ey-4;if(mx>=tx&&mx<=tx+50&&my>=tye&&my<=tye+24){g_crosshairEnabled=!g_crosshairEnabled;if(g_crosshairEnabled)CreateCrosshairWindow(hInst);else DestroyCrosshairWindow();InvalidateRect(hw,nullptr,FALSE);}
}

LRESULT CALLBACK WndProc(HWND hw,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_ERASEBKGND:return DefWindowProc(hw,m,wp,lp);
    case WM_CREATE:std::cout<<"StrikeSense "<<std::endl;break;
    case WM_PAINT:{PAINTSTRUCT ps;HDC hdc=BeginPaint(hw,&ps);PaintAll(hw,hdc);EndPaint(hw,&ps);break;}
    case WM_LBUTTONDOWN:{int mx=LOWORD(lp),my=HIWORD(lp);if(mx<SIDEBAR_W){CheckSidebarClick(mx,my);break;}
        switch(g_currentPage){case PAGE_SOUNDS:for(int i=0;i<SND_COUNT;++i){auto&r=s_sounds[i];if(mx>=r.btnRect.X&&mx<=r.btnRect.X+r.btnRect.Width&&my>=r.btnRect.Y&&my<=r.btnRect.Y+r.btnRect.Height){OnBrowse(hw,r.id);break;}}break;
        case PAGE_SETTINGS:CheckSettingsCheckboxClick(hw,mx,my);break;case PAGE_EVOLUTION:CheckEvolutionClick(hw,mx,my);break;}break;}
    case WM_COMMAND:{int id=LOWORD(wp);switch(id){case IDM_CREATE_GSI_CFG:OnCreateGSIConfig(hw);break;
        case IDM_DEBUGGER:g_Console.ShowDebugger(hInst,hw);break;case IDM_ABOUT:DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUTBOX),hw,About);break;
        case IDM_SETTINGS:g_currentPage=PAGE_SETTINGS;InvalidateRect(hw,nullptr,FALSE);break;
        case IDM_EXIT:DestroyCrosshairWindow();DestroyWindow(hw);break;default:return DefWindowProc(hw,m,wp,lp);}break;}
    case WM_DESTROY:DestroyCrosshairWindow();PostQuitMessage(0);break;
    default:return DefWindowProc(hw,m,wp,lp);}return 0;}

static LRESULT CALLBACK CrosshairWndProc(HWND hw,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_PAINT:{PAINTSTRUCT ps;HDC hdc=BeginPaint(hw,&ps);RECT rc;GetClientRect(hw,&rc);int W=rc.right-rc.left,H=rc.bottom-rc.top;
        HDC md=CreateCompatibleDC(hdc);HBITMAP mb=CreateCompatibleBitmap(hdc,W,H);HBITMAP ob=(HBITMAP)SelectObject(md,mb);
        using namespace Gdiplus;Graphics gx(md);gx.SetSmoothingMode(SmoothingModeAntiAlias);
        int cx=W/2,cy=H/2;Color crCol(255,(BYTE)g_crosshairR,(BYTE)g_crosshairG,(BYTE)g_crosshairB);Pen crPen(crCol,(REAL)g_crosshairThickness);int sz=(int)(20.f*g_crosshairScale);
        if(g_crosshairStyle==0){gx.DrawEllipse(&crPen,cx-sz,cy-sz,sz*2,sz*2);gx.DrawLine(&crPen,cx-sz-10,cy,cx-sz+2,cy);gx.DrawLine(&crPen,cx+sz-2,cy,cx+sz+10,cy);gx.DrawLine(&crPen,cx,cy-sz-10,cx,cy-sz+2);gx.DrawLine(&crPen,cx,cy+sz-2,cx,cy+sz+10);}
        else if(g_crosshairStyle==1){SolidBrush crBr(crCol);gx.FillEllipse(&crBr,cx-sz,cy-sz,sz*2,sz*2);}
        else{gx.DrawLine(&crPen,cx-15,cy,cx+15,cy);gx.DrawLine(&crPen,cx,cy-15,cx,cy+15);gx.DrawEllipse(&crPen,cx-3,cy-3,6,6);}
        BitBlt(hdc,0,0,W,H,md,0,0,SRCCOPY);SelectObject(md,ob);DeleteObject(mb);DeleteDC(md);EndPaint(hw,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProc(hw,m,wp,lp);}

static void CreateCrosshairWindow(HINSTANCE hI){
    if(g_crossHWnd)return;
    WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.lpfnWndProc=CrosshairWndProc;wc.hInstance=hI;wc.hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH);wc.lpszClassName=L"StrikeSense_Crosshair";RegisterClassExW(&wc);
    int sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);
    g_crossHWnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_TOOLWINDOW,L"StrikeSense_Crosshair",L"",WS_POPUP,0,0,sw,sh,nullptr,nullptr,hI,nullptr);
    SetLayeredWindowAttributes(g_crossHWnd,RGB(0,0,0),0,LWA_COLORKEY);ShowWindow(g_crossHWnd,SW_SHOW);UpdateWindow(g_crossHWnd);}
static void DestroyCrosshairWindow(){if(g_crossHWnd){DestroyWindow(g_crossHWnd);g_crossHWnd=nullptr;}}

static std::wstring GetCS2CfgPath(){
    std::wstring sv=strikesense::LoadSavedCfgPath();if(!sv.empty()&&fs::exists(sv))return sv;
    std::wstring sp=strikesense::GetSteamPathFromRegistry();if(sp.empty())return L"";std::wstring cd=strikesense::FindCS2InstallDir(sp);if(cd.empty())return L"";return strikesense::GetCS2CfgPath(cd);}
static void OnCreateGSIConfig(HWND hw){
    g_gsiCfgPath=GetCS2CfgPath();if(g_gsiCfgPath.empty()){wchar_t p[MAX_PATH]={};BROWSEINFOW b={};b.hwndOwner=hw;b.lpszTitle=L"选择CS2cfg目录";b.ulFlags=BIF_RETURNONLYFSDIRS;LPITEMIDLIST pid=SHBrowseForFolderW(&b);if(!pid)return;SHGetPathFromIDListW(pid,p);g_gsiCfgPath=p;IMalloc*m=nullptr;if(SUCCEEDED(SHGetMalloc(&m))){m->Free(pid);m->Release();}}
    DialogBoxW(hInst,MAKEINTRESOURCEW(IDD_CONFIRM_PATH),hw,ConfirmPathDlgProc);}
INT_PTR CALLBACK ConfirmPathDlgProc(HWND hD,UINT m,WPARAM wp,LPARAM lp){
    switch(m){case WM_INITDIALOG:SetDlgItemTextW(hD,IDC_PATH_LABEL,g_gsiCfgPath.c_str());
    {RECT r;GetWindowRect(GetParent(hD),&r);SetWindowPos(hD,nullptr,r.left+(r.right-r.left)/2-225,r.top+(r.bottom-r.top)/2-108,0,0,SWP_NOSIZE|SWP_NOZORDER);}return TRUE;
    case WM_COMMAND:switch(LOWORD(wp)){case IDYES:if(strikesense::WriteGSIConfig(g_gsiCfgPath)){strikesense::SaveCfgPath(g_gsiCfgPath);MessageBoxW(hD,L"成功！",L"提示",MB_OK);}else MessageBoxW(hD,L"失败！",L"错误",MB_OK);EndDialog(hD,IDYES);return TRUE;
    case IDC_DELETE_CFG:{fs::path f=fs::path(g_gsiCfgPath)/L"gamestate_integration_square.cfg";std::error_code ec;fs::remove(f,ec);MessageBoxW(hD,ec?L"失败":L"已删除",L"提示",MB_OK);return TRUE;}
    case IDC_BROWSE_BTN:{wchar_t p[MAX_PATH]={};BROWSEINFOW b={};b.hwndOwner=hD;b.lpszTitle=L"选择cfg目录";b.ulFlags=BIF_RETURNONLYFSDIRS;LPITEMIDLIST pid=SHBrowseForFolderW(&b);if(pid){SHGetPathFromIDListW(pid,p);g_gsiCfgPath=p;SetDlgItemTextW(hD,IDC_PATH_LABEL,p);IMalloc*mm=nullptr;if(SUCCEEDED(SHGetMalloc(&mm))){mm->Free(pid);mm->Release();}}return TRUE;}
    case IDCANCEL:EndDialog(hD,IDCANCEL);return TRUE;}}return FALSE;}
INT_PTR CALLBACK SettingsDlgProc(HWND hD,UINT m,WPARAM wp,LPARAM lp){(void)hD;(void)m;(void)wp;(void)lp;return TRUE;}
INT_PTR CALLBACK About(HWND hD,UINT m,WPARAM wp,LPARAM lp){
    UNREFERENCED_PARAMETER(lp);switch(m){case WM_INITDIALOG:return(INT_PTR)TRUE;
    case WM_COMMAND:if(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)EndDialog(hD,LOWORD(wp));return(INT_PTR)TRUE;}return(INT_PTR)FALSE;}