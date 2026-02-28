#include "OptionsDlg.h"
#include "resource.h"
#include <commctrl.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

#define WM_COMMIT_OVERLAY (WM_APP + 1)
#define WM_REBUILD_TREE   (WM_APP + 2)

// ---------------------------------------------------------------------------
//  Resolution helpers
// ---------------------------------------------------------------------------

static std::vector<std::wstring> EnumResolutions()
{
    std::vector<std::wstring> res;
    DEVMODEW dm = {}; dm.dmSize = sizeof(dm);
    wchar_t seen[256][32] = {}; int nSeen = 0;
    for (int i = 0; EnumDisplaySettingsW(nullptr, i, &dm); ++i) {
        if (dm.dmBitsPerPel < 16) continue;
        wchar_t buf[32];
        swprintf_s(buf, L"%d x %d", dm.dmPelsWidth, dm.dmPelsHeight);
        bool dup = false;
        for (int j = 0; j < nSeen; ++j)
            if (wcscmp(seen[j], buf) == 0) { dup = true; break; }
        if (!dup && nSeen < 256) { wcscpy_s(seen[nSeen++], buf); res.push_back(buf); }
    }
    if (res.empty())
        res = { L"800 x 600", L"1024 x 768", L"1280 x 720",
                L"1280 x 1024", L"1366 x 768", L"1600 x 900", L"1920 x 1080" };
    return res;
}

static void ParseResolution(const wchar_t* s, int& w, int& h)
{ w = 1024; h = 768; swscanf_s(s, L"%d x %d", &w, &h); }

// ---------------------------------------------------------------------------
//  Property system — matches original binary structure from Ghidra
// ---------------------------------------------------------------------------

enum FieldId : int {
    FID_NONE = 0,
    // Video > Basic Settings
    FID_FULLSCREEN, FID_RESOLUTION, FID_GAMMA, FID_QUALITY,
    // Video > User-Defined Options
    FID_FARCLIP, FID_TEXTURE, FID_SHADER, FID_SHADOW, FID_SHADOWTEST,
    FID_SELFSHADOW, FID_ALPHABLEND, FID_DYNLIGHT, FID_OCCLUSION,
    FID_GRASS, FID_LENSFLARE, FID_WATERREFLECT, FID_WATERREFRACT,
    FID_POSTEFFECT, FID_FOOTSTEPMESH, FID_DECAL,
    // Audio > Basic Settings
    FID_MUTE, FID_BGMVOL, FID_SFXVOL, FID_LISTENER, FID_FOOTSTEPSND,
    // Launcher > Basic Settings
    FID_THEME, FID_LANGUAGE, FID_PROFILE,
};

enum PropType { PT_COMBO, PT_FLOAT, PT_TEXT };

struct PropDef {
    FieldId            fid;
    const wchar_t*     name;
    const wchar_t*     name_zh;
    PropType           type;
    const wchar_t* const* items;
    const wchar_t* const* items_zh;  // nullptr = same as items (e.g. themes, profile)
    int                itemCount;
    float              fMin, fMax;
};

// ---- Combo item arrays (order matches original binary combo indices) ----
// Each array has an English and Chinese variant; Chinese extracted from TW binary.

// Full Screen: 0=No, 1=Yes
static const wchar_t* kFullScreen[]    = { L"No", L"Yes" };
static const wchar_t* kFullScreen_zh[] = { L"\x5426", L"\x662F" }; // 否, 是
// Graphics Quality: 0=User-Defined, 1=Highest, 2=High, 3=Medium, 4=Low, 5=Lowest
static const wchar_t* kQuality[]    = { L"User-Defined", L"Highest", L"High", L"Medium", L"Low", L"Lowest" };
static const wchar_t* kQuality_zh[] = { L"\x4F7F\x7528\x8005\x5B9A\x7FA9", L"\x6700\x9AD8", L"\x9AD8", L"\x4E2D", L"\x4F4E", L"\x6700\x4F4E" }; // 使用者定義, 最高, 高, 中, 低, 最低
// Texture: 0=High, 1=Medium, 2=Low
static const wchar_t* kTexture[]    = { L"High", L"Medium", L"Low" };
static const wchar_t* kTexture_zh[] = { L"\x9AD8", L"\x4E2D", L"\x4F4E" }; // 高, 中, 低
// Shader: 0=Highest, 1=High, 2=Medium, 3=Low, 4=Lowest (NO Off!)
static const wchar_t* kShader[]    = { L"Highest", L"High", L"Medium", L"Low", L"Lowest" };
static const wchar_t* kShader_zh[] = { L"\x6700\x9AD8", L"\x9AD8", L"\x4E2D", L"\x4F4E", L"\x6700\x4F4E" }; // 最高, 高, 中, 低, 最低
// Shadow: 0=High, 1=Medium, 2=Low, 3=Turn Off
static const wchar_t* kShadow[]    = { L"High", L"Medium", L"Low", L"Turn Off" };
static const wchar_t* kShadow_zh[] = { L"\x9AD8", L"\x4E2D", L"\x4F4E", L"\x95DC\x9589" }; // 高, 中, 低, 關閉
// Quality of Shadows: 0=Quality preferred, 1=Speed preferred
static const wchar_t* kShadowTest[]    = { L"Quality preferred", L"Speed preferred" };
static const wchar_t* kShadowTest_zh[] = { L"\x54C1\x8CEA\x512A\x5148", L"\x901F\x5EA6\x512A\x5148" }; // 品質優先, 速度優先
// On/Off: 0=Turn On, 1=Turn Off (My Shadow, Cover Shaded, Post Effects, Decals)
static const wchar_t* kOnOff[]    = { L"Turn On", L"Turn Off" };
static const wchar_t* kOnOff_zh[] = { L"\x958B\x555F", L"\x95DC\x9589" }; // 開啟, 關閉
// AlphaBlend: 0=Advanced, 1=Normal
static const wchar_t* kAlphaBlend[]    = { L"Advanced", L"Normal" };
static const wchar_t* kAlphaBlend_zh[] = { L"\x9AD8\x7D1A", L"\x666E\x901A" }; // 高級, 普通
// Dynamic Lighting: 0=Highest, 1=High, 2=Medium, 3=Low, 4=No
static const wchar_t* kDynLight[]    = { L"Highest", L"High", L"Medium", L"Low", L"No" };
static const wchar_t* kDynLight_zh[] = { L"\x6700\x9AD8", L"\x9AD8", L"\x4E2D", L"\x4F4E", L"\x7121" }; // 最高, 高, 中, 低, 無
// Lens Flare: 0=Advanced, 1=Normal, 2=Turn Off
static const wchar_t* kLensFlare[]    = { L"Advanced", L"Normal", L"Turn Off" };
static const wchar_t* kLensFlare_zh[] = { L"\x9AD8\x7D1A", L"\x666E\x901A", L"\x95DC\x9589" }; // 高級, 普通, 關閉
// Water Reflect/Refract: 0=Background + All Characters, 1=Background + My Character + My Anima, 2=Background, 3=Terrain, 4=No
static const wchar_t* kWater[]    = { L"Background + All Characters", L"Background + My Character + My Anima", L"Background", L"Terrain", L"No" };
static const wchar_t* kWater_zh[] = { L"\x6574\x9AD4\x80CC\x666F+\x73A9\x5BB6+Anima", L"\x6574\x9AD4\x80CC\x666F+\x73A9\x5BB6", L"\x6574\x9AD4\x80CC\x666F", L"\x5730\x5F62", L"\x7121" }; // 整體背景+玩家+Anima, 整體背景+玩家, 整體背景, 地形, 無
// Footprint Graphics: 0=All Characters, 1=My Character, 2=Turn Off
static const wchar_t* kFootMesh[]    = { L"All Characters", L"My Character", L"Turn Off" };
static const wchar_t* kFootMesh_zh[] = { L"\x5168\x90E8\x986F\x793A", L"\x53EA\x986F\x793A\x81EA\x5DF1", L"\x95DC\x9589" }; // 全部顯示, 只顯示自己, 關閉
// Mute: 0=Yes, 1=No  (inverted — Yes means muted)
static const wchar_t* kMute[]    = { L"Yes", L"No" };
static const wchar_t* kMute_zh[] = { L"\x662F", L"\x5426" }; // 是, 否
// Listener: 0=Camera Default, 1=Character Default
static const wchar_t* kListener[]    = { L"Camera Default", L"Character Default" };
static const wchar_t* kListener_zh[] = { L"\x93E1\x982D\x4E2D\x5FC3", L"\x89D2\x8272\x4E2D\x5FC3" }; // 鏡頭中心, 角色中心
// Footprint Sound Effects: 0=All Characters, 1=My Character, 2=Turn Off
static const wchar_t* kFootSnd[]    = { L"All Characters", L"My Character", L"Turn Off" };
static const wchar_t* kFootSnd_zh[] = { L"\x5168\x90E8\x6A19\x793A", L"\x53EA\x6A19\x793A\x81EA\x5DF1", L"\x95DC\x9589" }; // 全部標示, 只標示自己, 關閉
// Theme
static const wchar_t* kThemes[] = { L"20100513_TW", L"20101110_TW", L"20100420_US" };
// Language
static const wchar_t* kLanguage[] = { L"English", L"\x4E2D\x6587" }; // English, 中文
// Profile presets (user can also type a custom name)
static const wchar_t* kProfilePresets[] = { L"release", L"debug" };

// ---- Property definitions in tree order ----
// Chinese names extracted from TW PriusLauncher.exe RT_STRING resources.
static const PropDef kProps[] = {
    // Video > Basic Settings                                         name_zh (TW)
    { FID_FULLSCREEN,   L"Full Screen",         L"\x5168\x87A2\x5E55",          PT_COMBO, kFullScreen,  kFullScreen_zh,  2, 0, 0 },     // 全螢幕
    { FID_RESOLUTION,   L"Resolution",          L"\x756B\x9762\x89E3\x6790\x5EA6", PT_COMBO, nullptr,  nullptr,         0, 0, 0 },     // 畫面解析度
    { FID_GAMMA,        L"Gamma Encoding",      L"\x4EAE\x5EA6",                PT_FLOAT, nullptr,      nullptr,         0, 0.1f, 2.0f }, // 亮度
    { FID_QUALITY,      L"Graphics Quality",    L"\x5716\x7247\x6C34\x6E96",    PT_COMBO, kQuality,     kQuality_zh,     6, 0, 0 },     // 圖片水準
    // Video > User-Defined Options
    { FID_FARCLIP,      L"Sight",               L"\x8996\x91CE\x8DDD\x96E2",    PT_FLOAT, nullptr,      nullptr,         0, 0.1f, 1.0f }, // 視野距離
    { FID_TEXTURE,      L"Texture",             L"\x7D0B\x7406",                PT_COMBO, kTexture,     kTexture_zh,     3, 0, 0 },     // 紋理
    { FID_SHADER,       L"Shader",              L"\x9670\x5F71",                PT_COMBO, kShader,      kShader_zh,      5, 0, 0 },     // 陰影
    { FID_SHADOW,       L"Shadows",             L"\x5F71\x5B50",                PT_COMBO, kShadow,      kShadow_zh,      4, 0, 0 },     // 影子
    { FID_SHADOWTEST,   L"Quality of Shadows",  L"\x5F71\x5B50\x5224\x5B9A",    PT_COMBO, kShadowTest,  kShadowTest_zh,  2, 0, 0 },     // 影子判定
    { FID_SELFSHADOW,   L"My Shadow",           L"\x81EA\x8EAB\x5F71\x5B50",    PT_COMBO, kOnOff,       kOnOff_zh,       2, 0, 0 },     // 自身影子
    { FID_ALPHABLEND,   L"Alpha Blend",         L"AlphaBlend",                   PT_COMBO, kAlphaBlend,  kAlphaBlend_zh,  2, 0, 0 },     // AlphaBlend (kept English in TW)
    { FID_DYNLIGHT,     L"Dynamic Lighting",    L"\x52D5\x614B\x5149\x5F71\x6548\x679C", PT_COMBO, kDynLight, kDynLight_zh, 5, 0, 0 },  // 動態光影效果
    { FID_OCCLUSION,    L"Cover Shaded",        L"\x906E\x853D\x9670\x5F71",    PT_COMBO, kOnOff,       kOnOff_zh,       2, 0, 0 },     // 遮蔽陰影
    { FID_GRASS,        L"Graphic Density",     L"\x8349\x5730",                PT_FLOAT, nullptr,      nullptr,         0, 0.0f, 2.0f }, // 草地
    { FID_LENSFLARE,    L"Lens Flare",          L"\x93E1\x982D\x5149\x6688\x6548\x679C", PT_COMBO, kLensFlare, kLensFlare_zh, 3, 0, 0 }, // 鏡頭光暈效果
    { FID_WATERREFLECT, L"Water Reflection",    L"\x6C34\x5F71\x53CD\x5C04",    PT_COMBO, kWater,       kWater_zh,       5, 0, 0 },     // 水影反射
    { FID_WATERREFRACT, L"Water Refraction",    L"\x6C34\x4E2D\x6298\x5C04",    PT_COMBO, kWater,       kWater_zh,       5, 0, 0 },     // 水中折射
    { FID_POSTEFFECT,   L"Post Effects",        L"\x5F8C\x88FD\x6548\x679C",    PT_COMBO, kOnOff,       kOnOff_zh,       2, 0, 0 },     // 後製效果
    { FID_FOOTSTEPMESH, L"Footprint Graphics",  L"\x8DB3\x8DE1",                PT_COMBO, kFootMesh,    kFootMesh_zh,    3, 0, 0 },     // 足跡
    { FID_DECAL,        L"Decals",              L"\x8CBC\x82B1",                PT_COMBO, kOnOff,       kOnOff_zh,       2, 0, 0 },     // 貼花
    // Audio > Basic Settings
    { FID_MUTE,         L"Mute",                L"\x975C\x97F3",                PT_COMBO, kMute,        kMute_zh,        2, 0, 0 },     // 靜音
    { FID_BGMVOL,       L"BG Sound",            L"\x80CC\x666F\x97F3",          PT_FLOAT, nullptr,      nullptr,         0, 0.0f, 1.0f }, // 背景音
    { FID_SFXVOL,       L"Sound Effects Volume",L"\x6548\x679C\x97F3",          PT_FLOAT, nullptr,      nullptr,         0, 0.0f, 1.0f }, // 效果音
    { FID_LISTENER,     L"Sound Effects Options",L"\x807D\x8005\x4F4D\x7F6E",   PT_COMBO, kListener,    kListener_zh,    2, 0, 0 },     // 聽者位置
    { FID_FOOTSTEPSND,  L"Footprint Sound Effects",L"\x8173\x6B65\x8072",       PT_COMBO, kFootSnd,     kFootSnd_zh,     3, 0, 0 },     // 腳步聲
    // Launcher > Basic Settings
    { FID_THEME,        L"Theme",               L"\x4E3B\x984C",                PT_COMBO, kThemes,      nullptr,         3, 0, 0 },     // 主題
    { FID_LANGUAGE,     L"Language",            L"\x8A9E\x8A00",                PT_COMBO, kLanguage,    nullptr,         2, 0, 0 },     // 語言
    { FID_PROFILE,      L"Profile",             L"\x8A2D\x5B9A\x6A94",          PT_TEXT,  kProfilePresets,nullptr,       2, 0, 0 },     // 設定檔
};

static const PropDef* FindProp(FieldId fid)
{
    for (const auto& p : kProps)
        if (p.fid == fid) return &p;
    return nullptr;
}

// ---------------------------------------------------------------------------
//  Localization helpers — language 0=English, 1=中文
// ---------------------------------------------------------------------------

// Forward-declare; defined below after OptionsState
static int GetLang();

static const wchar_t* PropName(const PropDef* p)
{
    return (GetLang() == 1 && p->name_zh) ? p->name_zh : p->name;
}

static const wchar_t* const* PropItems(const PropDef* p)
{
    return (GetLang() == 1 && p->items_zh) ? p->items_zh : p->items;
}

// Translate tree node names (categories extracted from TW binary)
struct TrEntry { const wchar_t* en; const wchar_t* zh; };
static const TrEntry kTrTable[] = {
    { L"Video Options",       L"\x8996\x89BA\x9078\x9805" },      // 視覺選項
    { L"Audio Options",       L"\x807D\x89BA\x9078\x9805" },      // 聽覺選項
    { L"Launcher Options",    L"\x555F\x52D5\x5668\x9078\x9805" }, // 啟動器選項
    { L"Basic Settings",      L"\x57FA\x672C\x8A2D\x5B9A" },      // 基本設定
    { L"User-Defined Option", L"\x4F7F\x7528\x8005\x5B9A\x7FA9\x9078\x9805" }, // 使用者定義選項
};

static const wchar_t* TR(const wchar_t* en)
{
    if (GetLang() != 1) return en;
    for (int i = 0; i < _countof(kTrTable); ++i)
        if (wcscmp(en, kTrTable[i].en) == 0) return kTrTable[i].zh;
    return en;
}

// ---------------------------------------------------------------------------
//  GameSettings accessors
//  Combo index = registry value for int fields.
//  Bools with On/Off or Yes/No combos: index 0 = active, 1 = inactive.
// ---------------------------------------------------------------------------

static float GetFloat(const GameSettings* s, FieldId fid)
{
    switch (fid) {
    case FID_GAMMA:   return s->GammaRamp;
    case FID_FARCLIP: return s->FarClipLevel;
    case FID_GRASS:   return s->GrassLevel;
    case FID_BGMVOL:  return s->BgmVolume;
    case FID_SFXVOL:  return s->SfxVolume;
    default: return 0.f;
    }
}

static void SetFloat(GameSettings* s, FieldId fid, float v)
{
    switch (fid) {
    case FID_GAMMA:   s->GammaRamp    = v; break;
    case FID_FARCLIP: s->FarClipLevel = v; break;
    case FID_GRASS:   s->GrassLevel   = v; break;
    case FID_BGMVOL:  s->BgmVolume    = v; break;
    case FID_SFXVOL:  s->SfxVolume    = v; break;
    }
}

// Get combo index from settings value
static int GetInt(const GameSettings* s, FieldId fid)
{
    switch (fid) {
    case FID_FULLSCREEN:   return s->FullScreen ? 1 : 0;         // 0=No, 1=Yes
    case FID_QUALITY:      return s->GraphicQuality;
    case FID_TEXTURE:      return s->TextureLevel;
    case FID_SHADER:       return s->ShaderLevel;
    case FID_SHADOW:       return s->ShadowLevel;
    case FID_SHADOWTEST:   return s->ShadowTestLevel;
    case FID_SELFSHADOW:   return s->SelfShadowLevel;             // 0=Turn On, 1=Turn Off
    case FID_ALPHABLEND:   return s->AlphaBlendLevel;
    case FID_DYNLIGHT:     return s->DynamicLightingLevel;
    case FID_OCCLUSION:    return s->OcclusionShadeLevel;        // 0=Turn On, 1=Turn Off
    case FID_LENSFLARE:    return s->LensFlareLevel;
    case FID_WATERREFLECT: return s->WaterReflectLevel;
    case FID_WATERREFRACT: return s->WaterRefractLevel;
    case FID_POSTEFFECT:   return s->PostEffectLevel;            // 0=Turn On, 1=Turn Off
    case FID_FOOTSTEPMESH: return s->FootstepMeshLevel;
    case FID_DECAL:        return s->Decal ? 0 : 1;              // 0=On, 1=Off
    case FID_MUTE:         return s->VolumeMute ? 0 : 1;         // 0=Yes(muted), 1=No
    case FID_LISTENER:     return s->ListenerLevel;
    case FID_FOOTSTEPSND:  return s->FootstepSoundLevel;
    case FID_THEME:        return s->ThemeIndex;
    case FID_LANGUAGE:     return s->Language;
    default: return 0;
    }
}

// Set settings value from combo index
static void SetInt(GameSettings* s, FieldId fid, int v)
{
    switch (fid) {
    case FID_FULLSCREEN:   s->FullScreen           = (v == 1);  break;
    case FID_QUALITY:      s->GraphicQuality        = v; break;
    case FID_TEXTURE:      s->TextureLevel          = v; break;
    case FID_SHADER:       s->ShaderLevel           = v; break;
    case FID_SHADOW:       s->ShadowLevel           = v; break;
    case FID_SHADOWTEST:   s->ShadowTestLevel       = v; break;
    case FID_SELFSHADOW:   s->SelfShadowLevel       = v; break;
    case FID_ALPHABLEND:   s->AlphaBlendLevel       = v; break;
    case FID_DYNLIGHT:     s->DynamicLightingLevel  = v; break;
    case FID_OCCLUSION:    s->OcclusionShadeLevel   = v; break;
    case FID_LENSFLARE:    s->LensFlareLevel        = v; break;
    case FID_WATERREFLECT: s->WaterReflectLevel     = v; break;
    case FID_WATERREFRACT: s->WaterRefractLevel     = v; break;
    case FID_POSTEFFECT:   s->PostEffectLevel       = v; break;
    case FID_FOOTSTEPMESH: s->FootstepMeshLevel     = v; break;
    case FID_DECAL:        s->Decal                 = (v == 0); break;
    case FID_MUTE:         s->VolumeMute            = (v == 0); break;
    case FID_LISTENER:     s->ListenerLevel         = v; break;
    case FID_FOOTSTEPSND:  s->FootstepSoundLevel    = v; break;
    case FID_THEME:        s->ThemeIndex            = v; break;
    case FID_LANGUAGE:     s->Language              = v; break;
    }
}

// ---------------------------------------------------------------------------
//  Value text for right column
// ---------------------------------------------------------------------------

static void FormatValue(wchar_t* buf, int n, FieldId fid,
                        const GameSettings* s,
                        const std::vector<std::wstring>& resList)
{
    const PropDef* p = FindProp(fid);
    if (!p) { buf[0] = L'\0'; return; }

    if (p->type == PT_TEXT) {
        if (fid == FID_PROFILE)
            wcscpy_s(buf, n, s->Profile.c_str());
        else
            buf[0] = L'\0';
    } else if (p->type == PT_FLOAT) {
        swprintf_s(buf, n, L"%.2f", GetFloat(s, fid));
    } else if (fid == FID_RESOLUTION) {
        swprintf_s(buf, n, L"%d x %d", s->ScreenWidth, s->ScreenHeight);
    } else {
        int idx = GetInt(s, fid);
        const wchar_t* const* items = PropItems(p);
        if (items && idx >= 0 && idx < p->itemCount)
            wcscpy_s(buf, n, items[idx]);
        else
            swprintf_s(buf, n, L"%d", idx);
    }
}

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------

struct OptionsState {
    GameSettings*             settings = nullptr;
    GameSettings              backup;
    HWND                      hTree    = nullptr;
    HWND                      hHost    = nullptr;
    HWND                      hOverlay = nullptr;
    HWND                      hPropLbl = nullptr;
    FieldId                   curFid   = FID_NONE;
    std::vector<std::wstring> resList;
    int                       colSplit = -1;
    bool                      draggingSep = false;
    WNDPROC                   origTreeProc = nullptr;
};
static OptionsState g_opts;

static int GetLang() { return g_opts.settings ? g_opts.settings->Language : 0; }

static const int SEP_HIT_W = 4;

// ---------------------------------------------------------------------------
//  Column separator helpers
// ---------------------------------------------------------------------------

static int GetSplitX()
{
    if (g_opts.colSplit >= 0) return g_opts.colSplit;
    RECT rc; GetClientRect(g_opts.hTree, &rc);
    return rc.right / 2;
}

// ---------------------------------------------------------------------------
//  Tree subclass for draggable column separator
// ---------------------------------------------------------------------------

static LRESULT CALLBACK TreeSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SETCURSOR:
    {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
        int sx = GetSplitX();
        if (pt.x >= sx - SEP_HIT_W && pt.x <= sx + SEP_HIT_W) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        int mx = (short)LOWORD(lParam);
        int sx = GetSplitX();
        if (mx >= sx - SEP_HIT_W && mx <= sx + SEP_HIT_W) {
            g_opts.draggingSep = true;
            SetCapture(hwnd);
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (g_opts.draggingSep) {
            int mx = (short)LOWORD(lParam);
            RECT rc; GetClientRect(hwnd, &rc);
            int minX = 60, maxX = rc.right - 60;
            g_opts.colSplit = (mx < minX) ? minX : (mx > maxX) ? maxX : mx;
            InvalidateRect(hwnd, nullptr, FALSE);
            if (g_opts.hOverlay && g_opts.curFid != FID_NONE) {
                DestroyWindow(g_opts.hOverlay);
                g_opts.hOverlay = nullptr;
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_opts.draggingSep) {
            g_opts.draggingSep = false;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
            HTREEITEM hSel = TreeView_GetSelection(g_opts.hTree);
            if (hSel && g_opts.curFid != FID_NONE && !g_opts.hOverlay) {
                HWND hDlg = GetParent(hwnd);
                TVITEMW tvi = {}; tvi.mask = TVIF_PARAM; tvi.hItem = hSel;
                TreeView_GetItem(g_opts.hTree, &tvi);
                FieldId fid = (FieldId)tvi.lParam;
                extern void CreateOverlayEx(HWND hDlg, HTREEITEM hItem, FieldId fid);
                CreateOverlayEx(hDlg, hSel, fid);
            }
            return 0;
        }
        break;
    }
    return CallWindowProcW(g_opts.origTreeProc, hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
//  Overlay management
// ---------------------------------------------------------------------------

static void DestroyOverlay()
{
    // Null out state BEFORE DestroyWindow to prevent re-entrancy crash:
    // DestroyWindow can trigger focus-change → TVN_SELCHANGING → CommitOverlay
    HWND hWnd = g_opts.hOverlay;
    g_opts.hOverlay = nullptr;
    g_opts.curFid = FID_NONE;
    if (hWnd)
        DestroyWindow(hWnd);
}

static void CommitOverlay()
{
    if (!g_opts.hOverlay || g_opts.curFid == FID_NONE) return;

    FieldId fid = g_opts.curFid;
    const PropDef* p = FindProp(fid);
    if (!p) { DestroyOverlay(); return; }

    int prevLang = g_opts.settings->Language;

    if (p->type == PT_FLOAT) {
        wchar_t buf[64] = {};
        GetWindowTextW(g_opts.hOverlay, buf, 64);
        float v = (float)_wtof(buf);
        if (v < p->fMin) v = p->fMin;
        if (v > p->fMax) v = p->fMax;
        SetFloat(g_opts.settings, fid, v);
    } else if (p->type == PT_TEXT) {
        wchar_t buf[256] = {};
        GetWindowTextW(g_opts.hOverlay, buf, 256);
        if (fid == FID_PROFILE)
            g_opts.settings->Profile = buf;
    } else {
        int sel = (int)SendMessageW(g_opts.hOverlay, CB_GETCURSEL, 0, 0);
        if (sel < 0) sel = 0;
        if (fid == FID_RESOLUTION) {
            if (sel < (int)g_opts.resList.size()) {
                int w, h;
                ParseResolution(g_opts.resList[sel].c_str(), w, h);
                g_opts.settings->ScreenWidth  = w;
                g_opts.settings->ScreenHeight = h;
            }
        } else {
            SetInt(g_opts.settings, fid, sel);
        }
    }

    DestroyOverlay();
    InvalidateRect(g_opts.hTree, nullptr, FALSE);

    // If language changed, rebuild the tree with new translations
    if (fid == FID_LANGUAGE && g_opts.settings->Language != prevLang) {
        HWND hDlg = GetParent(g_opts.hTree);
        if (hDlg) PostMessageW(hDlg, WM_REBUILD_TREE, 0, 0);
    }
}

void CreateOverlayEx(HWND hDlg, HTREEITEM hItem, FieldId fid)
{
    const PropDef* p = FindProp(fid);
    if (!p) return;

    RECT itemRc;
    TreeView_GetItemRect(g_opts.hTree, hItem, &itemRc, FALSE);
    MapWindowPoints(g_opts.hTree, hDlg, reinterpret_cast<POINT*>(&itemRc), 2);

    RECT treeClRc;
    GetClientRect(g_opts.hTree, &treeClRc);
    MapWindowPoints(g_opts.hTree, hDlg, reinterpret_cast<POINT*>(&treeClRc), 2);

    int splitInDlg = treeClRc.left + GetSplitX();
    RECT rc = {
        splitInDlg + 2,
        itemRc.top + 1,
        treeClRc.right - 2,
        itemRc.bottom - 1
    };
    if (rc.right <= rc.left + 20) return;

    HINSTANCE hi = GetModuleHandleW(nullptr);

    if (p->type == PT_FLOAT) {
        float val = GetFloat(g_opts.settings, fid);
        wchar_t buf[32];
        swprintf_s(buf, L"%.2f", val);

        g_opts.hOverlay = CreateWindowExW(0, L"EDIT", buf,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_BORDER |
            ES_AUTOHSCROLL | ES_LEFT,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            hDlg, nullptr, hi, nullptr);
        HFONT hFont = (HFONT)SendMessageW(g_opts.hTree, WM_GETFONT, 0, 0);
        if (hFont) SendMessageW(g_opts.hOverlay, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_opts.hOverlay, EM_SETSEL, 0, -1);
        SetFocus(g_opts.hOverlay);
    } else if (p->type == PT_TEXT) {
        // Editable combo: user can pick a preset or type custom text
        g_opts.hOverlay = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | CBS_DROPDOWN | WS_VSCROLL,
            rc.left, rc.top, rc.right - rc.left, 140,
            hDlg, nullptr, hi, nullptr);

        HFONT hFont = (HFONT)SendMessageW(g_opts.hTree, WM_GETFONT, 0, 0);
        if (hFont) SendMessageW(g_opts.hOverlay, WM_SETFONT, (WPARAM)hFont, TRUE);

        // PT_TEXT presets are not localized (e.g. "release", "debug")
        for (int i = 0; i < p->itemCount; ++i)
            SendMessageW(g_opts.hOverlay, CB_ADDSTRING, 0, (LPARAM)p->items[i]);

        // Set current text (may or may not match a preset)
        if (fid == FID_PROFILE)
            SetWindowTextW(g_opts.hOverlay, g_opts.settings->Profile.c_str());
    } else {
        g_opts.hOverlay = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | CBS_DROPDOWNLIST | WS_VSCROLL,
            rc.left, rc.top, rc.right - rc.left, 140,
            hDlg, nullptr, hi, nullptr);

        HFONT hFont = (HFONT)SendMessageW(g_opts.hTree, WM_GETFONT, 0, 0);
        if (hFont) SendMessageW(g_opts.hOverlay, WM_SETFONT, (WPARAM)hFont, TRUE);

        if (fid == FID_RESOLUTION) {
            for (const auto& r : g_opts.resList)
                SendMessageW(g_opts.hOverlay, CB_ADDSTRING, 0, (LPARAM)r.c_str());
            wchar_t cur[32];
            swprintf_s(cur, L"%d x %d",
                       g_opts.settings->ScreenWidth, g_opts.settings->ScreenHeight);
            int idx = (int)SendMessageW(g_opts.hOverlay, CB_FINDSTRINGEXACT,
                                        (WPARAM)-1, (LPARAM)cur);
            SendMessageW(g_opts.hOverlay, CB_SETCURSEL, idx >= 0 ? idx : 0, 0);
        } else {
            const wchar_t* const* items = PropItems(p);
            for (int i = 0; i < p->itemCount; ++i)
                SendMessageW(g_opts.hOverlay, CB_ADDSTRING, 0, (LPARAM)items[i]);
            int idx = GetInt(g_opts.settings, fid);
            idx = (idx < 0 ? 0 : idx >= p->itemCount ? p->itemCount - 1 : idx);
            SendMessageW(g_opts.hOverlay, CB_SETCURSEL, idx, 0);
        }
    }
    g_opts.curFid = fid;

    if (g_opts.hOverlay) {
        BringWindowToTop(g_opts.hOverlay);
        InvalidateRect(g_opts.hOverlay, nullptr, TRUE);
    }
}

static void CreateOverlay(HWND hDlg, HTREEITEM hItem, FieldId fid)
{
    CreateOverlayEx(hDlg, hItem, fid);
}

// ---------------------------------------------------------------------------
//  Populate tree with localized strings (called on init and language change)
// ---------------------------------------------------------------------------

static void PopulateTree()
{
    HWND hT = g_opts.hTree;
    if (!hT) return;

    TreeView_DeleteAllItems(hT);

    auto addItem = [](HWND hT2, HTREEITEM hPar,
                      const wchar_t* txt, LPARAM lp) -> HTREEITEM {
        TVINSERTSTRUCTW ins = {};
        ins.hParent      = hPar;
        ins.hInsertAfter = TVI_LAST;
        ins.item.mask    = TVIF_TEXT | TVIF_PARAM;
        ins.item.pszText = const_cast<wchar_t*>(txt);
        ins.item.lParam  = lp;
        return TreeView_InsertItem(hT2, &ins);
    };

    auto pn = [](FieldId f) -> const wchar_t* {
        const PropDef* p = FindProp(f);
        return p ? PropName(p) : L"";
    };

    // Video Options (matches original tree structure)
    HTREEITEM hVid  = addItem(hT, TVI_ROOT, TR(L"Video Options"),          FID_NONE);
    HTREEITEM hVBas = addItem(hT, hVid,     TR(L"Basic Settings"),         FID_NONE);
    addItem(hT, hVBas, pn(FID_FULLSCREEN),  FID_FULLSCREEN);
    addItem(hT, hVBas, pn(FID_RESOLUTION),  FID_RESOLUTION);
    addItem(hT, hVBas, pn(FID_GAMMA),       FID_GAMMA);
    addItem(hT, hVBas, pn(FID_QUALITY),     FID_QUALITY);
    HTREEITEM hVUsr = addItem(hT, hVid,     TR(L"User-Defined Option"),    FID_NONE);
    addItem(hT, hVUsr, pn(FID_FARCLIP),     FID_FARCLIP);
    addItem(hT, hVUsr, pn(FID_TEXTURE),     FID_TEXTURE);
    addItem(hT, hVUsr, pn(FID_SHADER),      FID_SHADER);
    addItem(hT, hVUsr, pn(FID_SHADOW),      FID_SHADOW);
    addItem(hT, hVUsr, pn(FID_SHADOWTEST),  FID_SHADOWTEST);
    addItem(hT, hVUsr, pn(FID_SELFSHADOW),  FID_SELFSHADOW);
    addItem(hT, hVUsr, pn(FID_ALPHABLEND),  FID_ALPHABLEND);
    addItem(hT, hVUsr, pn(FID_DYNLIGHT),    FID_DYNLIGHT);
    addItem(hT, hVUsr, pn(FID_OCCLUSION),   FID_OCCLUSION);
    addItem(hT, hVUsr, pn(FID_GRASS),       FID_GRASS);
    addItem(hT, hVUsr, pn(FID_LENSFLARE),   FID_LENSFLARE);
    addItem(hT, hVUsr, pn(FID_WATERREFLECT),FID_WATERREFLECT);
    addItem(hT, hVUsr, pn(FID_WATERREFRACT),FID_WATERREFRACT);
    addItem(hT, hVUsr, pn(FID_POSTEFFECT),  FID_POSTEFFECT);
    addItem(hT, hVUsr, pn(FID_FOOTSTEPMESH),FID_FOOTSTEPMESH);
    addItem(hT, hVUsr, pn(FID_DECAL),       FID_DECAL);

    // Audio Options
    HTREEITEM hAud  = addItem(hT, TVI_ROOT, TR(L"Audio Options"),          FID_NONE);
    HTREEITEM hABas = addItem(hT, hAud,     TR(L"Basic Settings"),         FID_NONE);
    addItem(hT, hABas, pn(FID_MUTE),        FID_MUTE);
    addItem(hT, hABas, pn(FID_BGMVOL),      FID_BGMVOL);
    addItem(hT, hABas, pn(FID_SFXVOL),      FID_SFXVOL);
    addItem(hT, hABas, pn(FID_LISTENER),    FID_LISTENER);
    addItem(hT, hABas, pn(FID_FOOTSTEPSND), FID_FOOTSTEPSND);

    // Launcher Options
    HTREEITEM hLnc  = addItem(hT, TVI_ROOT, TR(L"Launcher Options"),       FID_NONE);
    HTREEITEM hLBas = addItem(hT, hLnc,     TR(L"Basic Settings"),         FID_NONE);
    addItem(hT, hLBas, pn(FID_THEME),       FID_THEME);
    addItem(hT, hLBas, pn(FID_LANGUAGE),    FID_LANGUAGE);
    addItem(hT, hLBas, pn(FID_PROFILE),     FID_PROFILE);

    // Expand all sections
    TreeView_Expand(hT, hVid,  TVE_EXPAND);
    TreeView_Expand(hT, hVBas, TVE_EXPAND);
    TreeView_Expand(hT, hAud,  TVE_EXPAND);
    TreeView_Expand(hT, hABas, TVE_EXPAND);
    TreeView_Expand(hT, hLnc,  TVE_EXPAND);
    TreeView_Expand(hT, hLBas, TVE_EXPAND);

    TreeView_SelectItem(hT, hVBas);
}

// ---------------------------------------------------------------------------
//  Dialog proc
// ---------------------------------------------------------------------------

static INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        g_opts.hHost    = GetDlgItem(hDlg, IDC_OPTIONS_HOST);
        g_opts.hTree    = GetDlgItem(hDlg, IDC_TREE_OPTIONS);
        g_opts.hPropLbl = GetDlgItem(hDlg, IDC_LBL_PROPNAME);
        g_opts.resList  = EnumResolutions();
        g_opts.colSplit = -1;

        if (!g_opts.hTree && g_opts.hHost) {
            RECT rcHost{};
            GetClientRect(g_opts.hHost, &rcHost);
            MapWindowPoints(g_opts.hHost, hDlg, reinterpret_cast<POINT*>(&rcHost), 2);

            g_opts.hTree = CreateWindowExW(
                0, WC_TREEVIEWW, L"",
                TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT |
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
                rcHost.left, rcHost.top, rcHost.right - rcHost.left, rcHost.bottom - rcHost.top,
                hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TREE_OPTIONS)),
                GetModuleHandleW(nullptr), nullptr);

            HFONT hFont = reinterpret_cast<HFONT>(SendMessageW(hDlg, WM_GETFONT, 0, 0));
            if (hFont && g_opts.hTree)
                SendMessageW(g_opts.hTree, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        }

        if (!g_opts.hTree) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }

        {
            LONG style = GetWindowLongW(g_opts.hTree, GWL_STYLE);
            style |= WS_CLIPSIBLINGS | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_LINESATROOT;
            style &= ~TVS_HASLINES;
            SetWindowLongW(g_opts.hTree, GWL_STYLE, style);
        }

        g_opts.origTreeProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(g_opts.hTree, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(TreeSubclassProc)));

        TreeView_SetItemHeight(g_opts.hTree, 20);

        PopulateTree();
        return TRUE;
    }

    case WM_NOTIFY:
    {
        NMHDR* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm->hwndFrom != g_opts.hTree) break;

        if (nm->code == NM_CUSTOMDRAW) {
            NMTVCUSTOMDRAW* pcd = reinterpret_cast<NMTVCUSTOMDRAW*>(lParam);
            DWORD  stage = pcd->nmcd.dwDrawStage;
            LRESULT res  = CDRF_DODEFAULT;

            if (stage == CDDS_PREPAINT) {
                res = CDRF_NOTIFYITEMDRAW;
            } else if (stage == CDDS_ITEMPREPAINT) {
                // TVS_FULLROWSELECT handles full-row highlight automatically
                res = CDRF_NOTIFYPOSTPAINT;

            } else if (stage == CDDS_ITEMPOSTPAINT) {
                FieldId fid = (FieldId)pcd->nmcd.lItemlParam;
                bool sel = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;

                RECT treeClRc;
                GetClientRect(g_opts.hTree, &treeClRc);
                int splitX = GetSplitX();

                int saved = SaveDC(pcd->nmcd.hdc);
                SelectClipRgn(pcd->nmcd.hdc, NULL);

                if (fid != FID_NONE) {
                    bool skipDraw = (fid == g_opts.curFid && g_opts.hOverlay);
                    if (!skipDraw) {
                        wchar_t val[64];
                        FormatValue(val, 64, fid, g_opts.settings, g_opts.resList);

                        RECT valRc = { splitX + 4, pcd->nmcd.rc.top, treeClRc.right - 2, pcd->nmcd.rc.bottom };

                        if (!sel) {
                            RECT bgRc = { splitX, valRc.top, treeClRc.right, valRc.bottom };
                            HBRUSH hBr = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
                            FillRect(pcd->nmcd.hdc, &bgRc, hBr);
                            DeleteObject(hBr);
                        }

                        SetBkMode(pcd->nmcd.hdc, TRANSPARENT);
                        SetTextColor(pcd->nmcd.hdc,
                            sel ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                                : GetSysColor(COLOR_WINDOWTEXT));
                        HFONT hFont = (HFONT)SendMessageW(g_opts.hTree, WM_GETFONT, 0, 0);
                        HFONT hOld  = hFont ? (HFONT)SelectObject(pcd->nmcd.hdc, hFont) : nullptr;
                        DrawTextW(pcd->nmcd.hdc, val, -1, &valRc,
                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                        if (hOld) SelectObject(pcd->nmcd.hdc, hOld);
                    }
                }

                // Separator line
                HPEN hPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DFACE));
                HPEN hOldPen = (HPEN)SelectObject(pcd->nmcd.hdc, hPen);
                MoveToEx(pcd->nmcd.hdc, splitX, pcd->nmcd.rc.top, nullptr);
                LineTo(pcd->nmcd.hdc, splitX, pcd->nmcd.rc.bottom);
                SelectObject(pcd->nmcd.hdc, hOldPen);
                DeleteObject(hPen);

                RestoreDC(pcd->nmcd.hdc, saved);
                res = CDRF_DODEFAULT;
            }

            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, res);
            return TRUE;
        }

        if (nm->code == NM_CLICK) {
            TVHITTESTINFO ht = {};
            GetCursorPos(&ht.pt);
            ScreenToClient(g_opts.hTree, &ht.pt);

            int sx = GetSplitX();
            if (ht.pt.x >= sx - SEP_HIT_W && ht.pt.x <= sx + SEP_HIT_W)
                break;

            HTREEITEM hHit = TreeView_HitTest(g_opts.hTree, &ht);
            if (!hHit) {
                HTREEITEM hChild = TreeView_GetFirstVisible(g_opts.hTree);
                while (hChild) {
                    RECT rc;
                    TreeView_GetItemRect(g_opts.hTree, hChild, &rc, FALSE);
                    if (ht.pt.y >= rc.top && ht.pt.y < rc.bottom) {
                        hHit = hChild;
                        break;
                    }
                    hChild = TreeView_GetNextVisible(g_opts.hTree, hChild);
                }
            }
            if (hHit) {
                HTREEITEM hCur = TreeView_GetSelection(g_opts.hTree);
                if (hHit != hCur) {
                    TreeView_SelectItem(g_opts.hTree, hHit);
                } else {
                    TVITEMW tvi = {}; tvi.mask = TVIF_PARAM; tvi.hItem = hHit;
                    TreeView_GetItem(g_opts.hTree, &tvi);
                    FieldId fid = (FieldId)tvi.lParam;
                    if (fid != FID_NONE && !g_opts.hOverlay)
                        CreateOverlay(hDlg, hHit, fid);
                }
            }
            break;
        }

        if (nm->code == TVN_SELCHANGING) {
            CommitOverlay();
            return FALSE;
        }

        if (nm->code == TVN_SELCHANGED) {
            NMTREEVIEWW* pnm = reinterpret_cast<NMTREEVIEWW*>(lParam);
            FieldId fid = (FieldId)pnm->itemNew.lParam;

            if (g_opts.hPropLbl) {
                const PropDef* p = (fid != FID_NONE) ? FindProp(fid) : nullptr;
                if (p) {
                    SetWindowTextW(g_opts.hPropLbl, PropName(p));
                } else {
                    wchar_t txt[128] = {};
                    TVITEMW tvi = {};
                    tvi.mask = TVIF_TEXT;
                    tvi.hItem = pnm->itemNew.hItem;
                    tvi.pszText = txt;
                    tvi.cchTextMax = _countof(txt);
                    if (TreeView_GetItem(g_opts.hTree, &tvi))
                        SetWindowTextW(g_opts.hPropLbl, txt);
                    else
                        SetWindowTextW(g_opts.hPropLbl, L"");
                }
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        HWND hCtrl = (HWND)lParam;
        if (hCtrl == g_opts.hOverlay && HIWORD(wParam) == EN_CHANGE) {
            FieldId fid = g_opts.curFid;
            const PropDef* p = FindProp(fid);
            if (p && p->type == PT_FLOAT) {
                wchar_t buf[64] = {};
                GetWindowTextW(g_opts.hOverlay, buf, 64);
                float v = (float)_wtof(buf);
                if (v < p->fMin) v = p->fMin;
                if (v > p->fMax) v = p->fMax;
                SetFloat(g_opts.settings, fid, v);
                InvalidateRect(g_opts.hTree, nullptr, FALSE);
            }
            break;
        }
        if (hCtrl == g_opts.hOverlay && HIWORD(wParam) == CBN_SELCHANGE) {
            // Defer commit so the combo isn't destroyed during its own notification
            PostMessageW(hDlg, WM_COMMIT_OVERLAY, 0, 0);
            break;
        }

        switch (LOWORD(wParam))
        {
        case IDOK:
            CommitOverlay();
            g_opts.settings->Save();
            EndDialog(hDlg, IDOK);
            break;
        case IDCANCEL:
            DestroyOverlay();
            *g_opts.settings = g_opts.backup;
            EndDialog(hDlg, IDCANCEL);
            break;
        case IDC_BTN_DEFAULTS:
        {
            DestroyOverlay();
            *g_opts.settings = GameSettings{};
            HTREEITEM hSel = TreeView_GetSelection(g_opts.hTree);
            if (hSel) {
                TVITEMW tvi = {}; tvi.mask = TVIF_PARAM; tvi.hItem = hSel;
                TreeView_GetItem(g_opts.hTree, &tvi);
                FieldId fid = (FieldId)tvi.lParam;
                if (fid != FID_NONE) CreateOverlay(hDlg, hSel, fid);
            }
            InvalidateRect(g_opts.hTree, nullptr, FALSE);
            break;
        }
        }
        break;
    }

    case WM_COMMIT_OVERLAY:
        CommitOverlay();
        break;

    case WM_REBUILD_TREE:
        PopulateTree();
        break;

    case WM_CLOSE:
        DestroyOverlay();
        *g_opts.settings = g_opts.backup;
        EndDialog(hDlg, IDCANCEL);
        break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
//  Public entry point
// ---------------------------------------------------------------------------

bool ShowOptionsDialog(HWND hParent, GameSettings& settings)
{
    g_opts          = {};
    g_opts.settings = &settings;
    g_opts.backup   = settings;
    INT_PTR ret = DialogBoxW(GetModuleHandleW(nullptr),
                             MAKEINTRESOURCEW(IDD_OPTIONS),
                             hParent, OptionsDlgProc);
    return ret == IDOK;
}
