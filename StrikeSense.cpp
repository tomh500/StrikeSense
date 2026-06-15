// StrikeSense.cpp — GDI+ 水色系亚克力版
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
#include <dwmapi.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

namespace fs = std::filesystem;
#define MAX_LOADSTRING 100
#define SIDEBAR_W 140

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING], szWindowClass[MAX_LOADSTRING];
Console g_Console;
std::wstring g_gsiCfgPath;
static ULONG_PTR g_gdiToken = 0;

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
};
static constexpr int SND_COUNT = sizeof(s_sounds)/sizeof(s_sounds[0]);

ATOM MyRegisterClass(HINSTANCE);
BOOL InitInstance(HINSTANCE,int);
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK About(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK ConfirmPathDlgProc(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK SettingsDlgProc(HWND,UINT,WPARAM,LPARAM);
static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND);
static void OnBrowse(HWND,int);

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
        CW_USEDEFAULT,0,780,700,nullptr,nullptr,hI,nullptr);
    if(!w)return FALSE;
    ShowWindow(w,nSC);UpdateWindow(w);return TRUE;
}

static void OnBrowse(HWND hWnd,int id){
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

static void PaintAll(HWND hw,HDC hdc){
    using namespace Gdiplus;
    RECT rc;GetClientRect(hw,&rc);
    int W=rc.right-rc.left,H=rc.bottom-rc.top;
    HDC md=CreateCompatibleDC(hdc);HBITMAP mb=CreateCompatibleBitmap(hdc,W,H);
    HBITMAP ob=(HBITMAP)SelectObject(md,mb);
    Graphics g(md);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // ---- 水色系配色 ----
    SolidBrush bgMain(Color(255,235,248,255));     // 主背景：浅水蓝
    SolidBrush sidebarBg(Color(255,200,230,250));  // 侧边栏：水蓝
    Pen sidebarLine(Color(255,160,210,240),2.0f);  // 分隔线
    SolidBrush headerBg(Color(255,180,220,245));   // 顶栏：中水蓝
    SolidBrush rowBg0(Color(255,220,240,255));     // 奇数行：浅白水蓝
    SolidBrush rowBg1(Color(255,240,248,255));     // 偶数行：微白
    SolidBrush textDark(Color(255,30,60,100));     // 深色文字：深蓝
    SolidBrush textDim(Color(255,100,130,160));    // 暗色文字
    SolidBrush textBright(Color(255,20,80,140));   // 亮色文字：蓝
    SolidBrush btnBg(Color(255,140,200,240));      // 按钮：水蓝
    SolidBrush btnHoverBg(Color(255,60,160,230));  // hover：亮水蓝
    SolidBrush btnText(Color(255,255,255,255));    // 按钮文字：白
    Pen btnPen(Color(255,100,170,220));            // 按钮边框

    Font titleFont(L"Microsoft YaHei", 16, FontStyleBold);
    Font pageFont(L"Microsoft YaHei", 13, FontStyleBold);
    Font rowFont(L"Microsoft YaHei", 11);
    Font btnFont(L"Microsoft YaHei", 9);

    // ---- 绘制 ----

    // 1. 主背景
    g.FillRectangle(&bgMain, 0, 0, W, H);

    // 2. 侧边栏
    g.FillRectangle(&sidebarBg, 0, 0, SIDEBAR_W, H);
    g.DrawLine(&sidebarLine, SIDEBAR_W, 0, SIDEBAR_W, H);

    // 3. 侧边栏标题
    g.DrawString(L"StrikeSense", -1, &titleFont, PointF(10,12), &textBright);
    Font navFont(L"Microsoft YaHei", 12);
    g.DrawString(L"文件位置", -1, &navFont, PointF(14,52), &textDark);

    // 4. 主内容区顶栏
    int contentX = SIDEBAR_W + 12;
    int contentW = W - contentX - 12;
    g.FillRectangle(&headerBg, contentX, 8, contentW, 34);
    g.DrawString(L"文件位置", -1, &pageFont, PointF(contentX+10,14), &textBright);

    // GSI 状态行
    config::Settings c=config::Load();
    wchar_t st[256];swprintf_s(st,L"GSI: %s  |  音效: %s  |  音乐包: %s  |  低内存: %s  |  音量: %.0f%%",
        gsi::IsRunning()?L"运行中":L"未启动",
        c.enable_kill_sound?L"已启用":L"已禁用",
        c.custom_musickit?L"已启用":L"已禁用",
        c.low_memory?L"已启用":L"已禁用",c.volume*100.f);
    Font sF(L"Microsoft YaHei", 9);
    g.DrawString(st,-1,&sF,PointF(contentX+10,48),&textDim);

    int y=70;const int rh=30;
    for(int i=0;i<SND_COUNT;++i){
        auto&r=s_sounds[i];
        SolidBrush* bg = (i%2==0) ? &rowBg0 : &rowBg1;
        g.FillRectangle(bg,contentX,y,contentW,rh);
        g.DrawString(r.label,-1,&rowFont,PointF(contentX+8,y+5),&textDark);
        std::wstring nm=r.defName;bool isDef=true;
        std::wstring pp=c.*(r.cfgPtr);
        if(!pp.empty()){fs::path p(pp);nm=p.filename().wstring();isDef=false;}
        SolidBrush* nmB = isDef ? &textDim : &textDark;
        g.DrawString(nm.c_str(),-1,&rowFont,PointF(contentX+200,y+5),nmB);
        int bx=W-100,by=y+2,bw=80,bh=26;
        r.btnRect=RectF((REAL)bx,(REAL)by,(REAL)bw,(REAL)bh);
        GraphicsPath bp;
        float rx=8.f,ry=8.f;
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
    BitBlt(hdc,0,0,W,H,md,0,0,SRCCOPY);SelectObject(md,ob);DeleteObject(mb);DeleteDC(md);
}

LRESULT CALLBACK WndProc(HWND hw,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_CREATE:std::cout<<"StrikeSense "<<std::endl;break;
    case WM_PAINT:{PAINTSTRUCT ps;HDC hdc=BeginPaint(hw,&ps);PaintAll(hw,hdc);EndPaint(hw,&ps);break;}
    case WM_LBUTTONDOWN:{
        int mx=LOWORD(lp),my=HIWORD(lp);
        for(int i=0;i<SND_COUNT;++i){
            auto&r=s_sounds[i];
            if(mx>=r.btnRect.X&&mx<=r.btnRect.X+r.btnRect.Width&&
               my>=r.btnRect.Y&&my<=r.btnRect.Y+r.btnRect.Height){OnBrowse(hw,r.id);break;}
        }break;}
    case WM_COMMAND:{
        int id=LOWORD(wp);
        switch(id){
        case IDM_CREATE_GSI_CFG:OnCreateGSIConfig(hw);break;
        case IDM_DEBUGGER:g_Console.ShowDebugger(hInst,hw);break;
        case IDM_ABOUT:DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUTBOX),hw,About);break;
        case IDM_SETTINGS:DialogBoxW(hInst,MAKEINTRESOURCEW(IDD_SETTINGS),hw,SettingsDlgProc);InvalidateRect(hw,nullptr,FALSE);break;
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
    static config::Settings s;switch(m){case WM_INITDIALOG:s=config::Load();
    CheckDlgButton(hD,IDC_CK_CUSTOM_KIT,s.custom_musickit?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(hD,IDC_CK_ENABLE_KILL_SOUND,s.enable_kill_sound?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(hD,IDC_CK_FLASHBANG,s.custom_flashbang?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(hD,IDC_CK_LOW_MEMORY,s.low_memory?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(hD,IDC_CK_SHOW_MVP,s.show_mvp?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(hD,IDC_CK_USE_OGG,s.ogg?BST_CHECKED:BST_UNCHECKED);
    {wchar_t t[32];swprintf_s(t,L"%.2f",s.volume);SetDlgItemTextW(hD,IDC_EDIT_VOL,t);}
    {RECT r;GetWindowRect(GetParent(hD),&r);SetWindowPos(hD,nullptr,r.left+(r.right-r.left)/2-175,r.top+(r.bottom-r.top)/2-115,0,0,SWP_NOSIZE|SWP_NOZORDER);}return TRUE;
    case WM_COMMAND:if(LOWORD(wp)==IDOK){s.custom_musickit=(IsDlgButtonChecked(hD,IDC_CK_CUSTOM_KIT)==BST_CHECKED);
    s.enable_kill_sound=(IsDlgButtonChecked(hD,IDC_CK_ENABLE_KILL_SOUND)==BST_CHECKED);
    s.custom_flashbang=(IsDlgButtonChecked(hD,IDC_CK_FLASHBANG)==BST_CHECKED);
    s.low_memory=(IsDlgButtonChecked(hD,IDC_CK_LOW_MEMORY)==BST_CHECKED);
    s.show_mvp=(IsDlgButtonChecked(hD,IDC_CK_SHOW_MVP)==BST_CHECKED);
    s.ogg=(IsDlgButtonChecked(hD,IDC_CK_USE_OGG)==BST_CHECKED);
    wchar_t t[32];GetDlgItemTextW(hD,IDC_EDIT_VOL,t,32);try{float v=std::stof(t);if(v>=0&&v<=1)s.volume=v;}catch(...){}
    config::Save(s);EndDialog(hD,IDOK);return TRUE;}if(LOWORD(wp)==IDCANCEL){EndDialog(hD,IDCANCEL);return TRUE;}break;}return FALSE;}
INT_PTR CALLBACK About(HWND hD,UINT m,WPARAM wp,LPARAM lp){
    UNREFERENCED_PARAMETER(lp);switch(m){case WM_INITDIALOG:return(INT_PTR)TRUE;
    case WM_COMMAND:if(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)EndDialog(hD,LOWORD(wp));return(INT_PTR)TRUE;}return(INT_PTR)FALSE;}