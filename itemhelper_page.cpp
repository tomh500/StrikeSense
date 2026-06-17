#include "pages.h"
#include "StrikeSense.h"
#include "i18n.h"
#include "itemhelper_page.h"
#include "Hotkey.h"

Gdiplus::RectF g_itemHelperToggleRect;
Gdiplus::RectF g_itemHelperHotkeyRect;

extern bool g_itemHelperEnabled;

extern bool g_isBindingItemHelperHotkey;

extern int g_itemHelperHotkeyMod;
extern int g_itemHelperHotkeyVk;

void PaintItemHelperPage(Gdiplus::Graphics& g,int cx,int cw,int H,HWND)
{
    using namespace Gdiplus;

    ui::DrawHeader(g,cx,cw,L"道具助手");

    Font rF(L"Microsoft YaHei",11);
    Font sF(L"Microsoft YaHei",9);
    SolidBrush tdCol(Color(255,30,60,100));

    g.DrawString(L"启用道具助手",-1,&rF,PointF((float)(cx+10),60.f),&tdCol);

    g_itemHelperToggleRect=RectF((REAL)(cx+130),56.f,50.f,24.f);
    ui::DrawToggle(g,cx+130,56,g_itemHelperEnabled);

    if(!g_itemHelperEnabled) return;

    std::wstring keyName=Hotkey::HotkeyToString(g_itemHelperHotkeyMod,g_itemHelperHotkeyVk);

    wchar_t hs[128];
    swprintf_s(hs,L"快捷键: %s",keyName.c_str());

    g.DrawString(hs,-1,&rF,PointF((float)(cx+10),92.f),&tdCol);

    Pen kp(Color(255,30,60,100));

    g_itemHelperHotkeyRect=RectF((REAL)(cx+10),118.f,200.f,20.f);

    g.DrawRectangle(&kp,
        g_itemHelperHotkeyRect.X,
        g_itemHelperHotkeyRect.Y,
        g_itemHelperHotkeyRect.Width,
        g_itemHelperHotkeyRect.Height);

    const wchar_t* hintStr=g_isBindingItemHelperHotkey?L"按下任意键...":L"点击修改快捷键";

    g.DrawString(hintStr,-1,&sF,PointF((float)(cx+14),118.f),&tdCol);
}

void CheckItemHelperClick(HWND hw,int mx,int my)
{
    if(ui::CheckToggleClick(mx,my,(int)g_itemHelperToggleRect.X,(int)g_itemHelperToggleRect.Y))
    {
        g_itemHelperEnabled=!g_itemHelperEnabled;
        SaveEvolutionParams();
        Hotkey::UpdateItemHelperHotkey(hw);
        InvalidateRect(hw,nullptr,FALSE);
        return;
    }

    if(!g_itemHelperEnabled) return;

    if(mx>=g_itemHelperHotkeyRect.X &&
       mx<=g_itemHelperHotkeyRect.X+g_itemHelperHotkeyRect.Width &&
       my>=g_itemHelperHotkeyRect.Y &&
       my<=g_itemHelperHotkeyRect.Y+g_itemHelperHotkeyRect.Height)
    {
        g_isBindingItemHelperHotkey=true;
        InvalidateRect(hw,nullptr,FALSE);
    }
}


