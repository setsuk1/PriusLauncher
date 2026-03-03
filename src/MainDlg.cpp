#include "MainDlg.h"
#include "OptionsDlg.h"
#include "resource.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "msimg32.lib")   // TransparentBlt

// ---- window dimensions -----------------------------------------------------
static const int DLG_W = 825;
static const int DLG_H = 510;

// Chroma-key transparency color used in both background and button bitmaps
static const COLORREF TRANS_COLOR = RGB(255, 0, 255);

// ---- button layouts ---------------------------------------------------------
// Close  (btn_close 82x24)  ??top-right black area
// Options (btn_options 126x35) ??left slot, y=401..436 (36px slot, 35px bitmap)
// Start  (btn_start 126x36) ??right slot, exact fit

// Stub loading line defaults (updated by theme layout)
static const int SEP_DEFAULT_X      = 281;
static const int SEP_DEFAULT_CUR_Y  = 451;
static const int SEP_DEFAULT_OVR_Y  = 462;
static const int SEP_DEFAULT_W      = 508;
static const int SEP_DEFAULT_H      = 6;

// 3s startup stub before Start is enabled
static const UINT_PTR IDT_LOAD_STUB = 1;
static const int      LOAD_STUB_TICK_MS = 33;
static const DWORD    LOAD_STUB_TOTAL_MS = 3000;

struct ThemeSepLayout
{
    int x;
    int curY;
    int overallY;
};

static const ThemeSepLayout kSepLayouts[] = {
    // 20100513_TW
    { 281, 451, 462 },
    // 20101110_TW
    { 281, 451, 462 },
    // 20100420_US
    { 281, 443, 454 },
};

struct ThemeButtonLayout
{
    int closeX;
    int closeY;
    int closeW;
    int closeH;
    int optionsX;
    int optionsY;
    int optionsW;
    int optionsH;
    int startX;
    int startY;
    int startW;
    int startH;
};

static const ThemeButtonLayout kButtonLayouts[] = {
    // 20100513_TW
    { 739, 64, 82, 24, 540, 405, 113, 36, 666, 405, 113, 36 },
    // 20101110_TW
    { 739, 64, 82, 24, 540, 405, 113, 36, 666, 405, 113, 36 },
    // 20100420_US
    { 733, 59, 82, 24, 535, 401, 126, 35, 668, 401, 126, 36 },
};

// ---- drag support ----------------------------------------------------------
static bool  s_dragging = false;
static POINT s_dragOrig = {};
static POINT s_winOrig  = {};

// ---- main dialog state -----------------------------------------------------
struct MainState
{
    HINSTANCE     hInst        = nullptr;
    GameSettings* settings     = nullptr;
    HBITMAP       hBgBitmap    = nullptr;
    // Close button (no disabled state)
    HBITMAP       hCloseNormal  = nullptr;
    HBITMAP       hCloseHover   = nullptr;
    HBITMAP       hClosePressed = nullptr;
    // Options button
    HBITMAP       hOptionsNormal  = nullptr;
    HBITMAP       hOptionsHover   = nullptr;
    HBITMAP       hOptionsPressed = nullptr;
    HBITMAP       hOptionsDisabled = nullptr;
    // Start button
    HBITMAP       hStartNormal  = nullptr;
    HBITMAP       hStartHover   = nullptr;
    HBITMAP       hStartPressed = nullptr;
    HBITMAP       hStartDisabled = nullptr;
    // Sep line bitmaps
    HBITMAP       hSepFillTotal    = nullptr;
    HBITMAP       hSepTrackDisabled  = nullptr;
    HBITMAP       hSepTrackHover     = nullptr;
    HBITMAP       hSepFillCurrent   = nullptr;
    int           sepX          = SEP_DEFAULT_X;
    int           sepCurY       = SEP_DEFAULT_CUR_Y;
    int           sepOverallY   = SEP_DEFAULT_OVR_Y;
    int           sepTrackW     = SEP_DEFAULT_W;
    int           sepTrackH     = SEP_DEFAULT_H;
    float         curFileProgress = 0.0f;
    float         overallProgress = 0.0f;
    DWORD         loadStubStartTick = 0;
    bool          loadStubDone = false;
    wchar_t       gameExe[MAX_PATH]  = {};
    wchar_t       themeDir[MAX_PATH] = {};  // e.g. "<exeDir>\assets\themes\20100513_TW"
};

static const wchar_t* kThemeNames[] = { L"20100513_TW", L"20101110_TW", L"20100420_US" };
static const int      kThemeCount   = (int)(sizeof(kThemeNames) / sizeof(kThemeNames[0]));
static MainState g_main;

enum ThemeBitmapSlot
{
    TB_MAIN_BG = 0,
    TB_CLOSE_NORMAL,
    TB_CLOSE_HOVER,
    TB_CLOSE_PRESSED,
    TB_OPTIONS_NORMAL,
    TB_OPTIONS_HOVER,
    TB_OPTIONS_PRESSED,
    TB_OPTIONS_DISABLED,
    TB_START_NORMAL,
    TB_START_HOVER,
    TB_START_PRESSED,
    TB_START_DISABLED,
    TB_SEP_FILL_TOTAL,
    TB_SEP_TRACK_DISABLED,
    TB_SEP_TRACK_HOVER,
    TB_SEP_FILL_CURRENT,
};

static const int kThemeBitmapFallback[kThemeCount][16] = {
    {
        IDB_MAIN_BG,
        IDB_BTN_CLOSE_NORMAL, IDB_BTN_CLOSE_HOVER, IDB_BTN_CLOSE_PRESSED,
        IDB_BTN_OPTIONS_NORMAL, IDB_BTN_OPTIONS_HOVER, IDB_BTN_OPTIONS_PRESSED, IDB_BTN_OPTIONS_DISABLED,
        IDB_BTN_START_NORMAL, IDB_BTN_START_HOVER, IDB_BTN_START_PRESSED, IDB_BTN_START_DISABLED,
        IDB_SEP_FILL_TOTAL, IDB_SEP_TRACK_DISABLED, IDB_SEP_TRACK_HOVER, IDB_SEP_FILL_CURRENT
    },
    {
        IDB_MAIN_BG_20101110,
        IDB_BTN_CLOSE_NORMAL_20101110, IDB_BTN_CLOSE_HOVER_20101110, IDB_BTN_CLOSE_PRESSED_20101110,
        IDB_BTN_OPTIONS_NORMAL_20101110, IDB_BTN_OPTIONS_HOVER_20101110, IDB_BTN_OPTIONS_PRESSED_20101110, IDB_BTN_OPTIONS_DISABLED_20101110,
        IDB_BTN_START_NORMAL_20101110, IDB_BTN_START_HOVER_20101110, IDB_BTN_START_PRESSED_20101110, IDB_BTN_START_DISABLED_20101110,
        IDB_SEP_FILL_TOTAL_20101110, IDB_SEP_TRACK_DISABLED_20101110, IDB_SEP_TRACK_HOVER_20101110, IDB_SEP_FILL_CURRENT_20101110
    },
    {
        IDB_MAIN_BG_20100420,
        IDB_BTN_CLOSE_NORMAL_20100420, IDB_BTN_CLOSE_HOVER_20100420, IDB_BTN_CLOSE_PRESSED_20100420,
        IDB_BTN_OPTIONS_NORMAL_20100420, IDB_BTN_OPTIONS_HOVER_20100420, IDB_BTN_OPTIONS_PRESSED_20100420, IDB_BTN_OPTIONS_DISABLED_20100420,
        IDB_BTN_START_NORMAL_20100420, IDB_BTN_START_HOVER_20100420, IDB_BTN_START_PRESSED_20100420, IDB_BTN_START_DISABLED_20100420,
        IDB_SEP_FILL_TOTAL_20100420, IDB_SEP_TRACK_DISABLED_20100420, IDB_SEP_TRACK_HOVER_20100420, IDB_SEP_FILL_CURRENT_20100420
    },
};

static int ThemeBitmapSlotFromFile(const wchar_t* filename)
{
    if (!filename) return -1;
    if (wcscmp(filename, L"main_bg.bmp") == 0) return TB_MAIN_BG;
    if (wcscmp(filename, L"btn_close_normal.bmp") == 0) return TB_CLOSE_NORMAL;
    if (wcscmp(filename, L"btn_close_hover.bmp") == 0) return TB_CLOSE_HOVER;
    if (wcscmp(filename, L"btn_close_pressed.bmp") == 0) return TB_CLOSE_PRESSED;
    if (wcscmp(filename, L"btn_options_normal.bmp") == 0) return TB_OPTIONS_NORMAL;
    if (wcscmp(filename, L"btn_options_hover.bmp") == 0) return TB_OPTIONS_HOVER;
    if (wcscmp(filename, L"btn_options_pressed.bmp") == 0) return TB_OPTIONS_PRESSED;
    if (wcscmp(filename, L"btn_options_disabled.bmp") == 0) return TB_OPTIONS_DISABLED;
    if (wcscmp(filename, L"btn_start_normal.bmp") == 0) return TB_START_NORMAL;
    if (wcscmp(filename, L"btn_start_hover.bmp") == 0) return TB_START_HOVER;
    if (wcscmp(filename, L"btn_start_pressed.bmp") == 0) return TB_START_PRESSED;
    if (wcscmp(filename, L"btn_start_disabled.bmp") == 0) return TB_START_DISABLED;
    if (wcscmp(filename, L"sep_fill_total.bmp") == 0) return TB_SEP_FILL_TOTAL;
    if (wcscmp(filename, L"sep_track_disabled.bmp") == 0) return TB_SEP_TRACK_DISABLED;
    if (wcscmp(filename, L"sep_track_hover.bmp") == 0) return TB_SEP_TRACK_HOVER;
    if (wcscmp(filename, L"sep_fill_current.bmp") == 0) return TB_SEP_FILL_CURRENT;
    return -1;
}

static int ThemeBitmapResourceFallback(int themeIndex, const wchar_t* filename)
{
    int idx = ThemeBitmapSlotFromFile(filename);
    if (idx < 0 || idx >= 16) return 0;
    if (themeIndex < 0 || themeIndex >= kThemeCount)
        themeIndex = 0;
    return kThemeBitmapFallback[themeIndex][idx];
}

// ---- shaped-window helper --------------------------------------------------
// Builds an HRGN covering all non-TRANS_COLOR pixels in hBmp.
// Ownership is transferred to SetWindowRgn ??caller must NOT DeleteObject.
static HRGN CreateRegionFromBitmap(HBITMAP hBmp, COLORREF crTransparent)
{
    if (!hBmp) return nullptr;

    BITMAP bm = {};
    GetObject(hBmp, sizeof(bm), &bm);
    const int W = bm.bmWidth, H = bm.bmHeight;

    const int stride = (W * 3 + 3) & ~3;
    std::vector<BYTE> px((size_t)stride * H);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;   // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    GetDIBits(hdc, hBmp, 0, H, px.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    // DIB stores pixels as [Blue, Green, Red]
    const BYTE tB = GetBValue(crTransparent);
    const BYTE tG = GetGValue(crTransparent);
    const BYTE tR = GetRValue(crTransparent);

    HRGN hRgn = CreateRectRgn(0, 0, 0, 0);
    for (int y = 0; y < H; ++y)
    {
        const BYTE* row = px.data() + (size_t)y * stride;
        int xStart = -1;
        for (int x = 0; x <= W; ++x)
        {
            bool isTrans = true;
            if (x < W)
                isTrans = (row[x*3] == tB && row[x*3+1] == tG && row[x*3+2] == tR);
            if (!isTrans && xStart < 0)          { xStart = x; }
            else if (isTrans && xStart >= 0)
            {
                HRGN hRow = CreateRectRgn(xStart, y, x, y + 1);
                CombineRgn(hRgn, hRgn, hRow, RGN_OR);
                DeleteObject(hRow);
                xStart = -1;
            }
        }
    }
    return hRgn;
}

// ---- button subclass -------------------------------------------------------
static WNDPROC s_origBtnProc = nullptr;

// Return the correct bitmap for a button given its control ID and state
static HBITMAP PickButtonBitmap(HWND hwnd, int id, bool hovered, bool pressed)
{
    bool enabled = IsWindowEnabled(hwnd);
    if (id == IDC_BTN_EXIT) {
        // Close button has no disabled state
        if (pressed)  return g_main.hClosePressed;
        if (hovered)  return g_main.hCloseHover;
        return g_main.hCloseNormal;
    }
    if (id == IDC_BTN_OPTIONS) {
        if (!enabled) return g_main.hOptionsDisabled;
        if (pressed)  return g_main.hOptionsPressed;
        if (hovered)  return g_main.hOptionsHover;
        return g_main.hOptionsNormal;
    }
    // IDC_BTN_START
    if (!enabled) return g_main.hStartDisabled;
    if (pressed)  return g_main.hStartPressed;
    if (hovered)  return g_main.hStartHover;
    return g_main.hStartNormal;
}

static LRESULT CALLBACK BtnSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        // Suppress default erase to prevent gray flash before WM_PAINT runs
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        const int w = rc.right, h = rc.bottom;

        // Step 1: paint the correct slice of the background bitmap behind this button
        POINT ptBg = { 0, 0 };
        MapWindowPoints(hwnd, GetParent(hwnd), &ptBg, 1);
        if (g_main.hBgBitmap)
        {
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hdcMem, g_main.hBgBitmap));
            BitBlt(hdc, 0, 0, w, h, hdcMem, ptBg.x, ptBg.y, SRCCOPY);
            SelectObject(hdcMem, hOld);
            DeleteDC(hdcMem);
        }

        // Step 2: determine state
        POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
        const bool hovered = (PtInRect(&rc, pt) != 0);
        const bool pressed = hovered && (GetKeyState(VK_LBUTTON) & 0x8000) != 0;

        // Step 3: TransparentBlt the button bitmap (magenta = transparent ??background shows through)
        HBITMAP hBmp = PickButtonBitmap(hwnd, GetDlgCtrlID(hwnd), hovered, pressed);
        if (hBmp)
        {
            BITMAP bm = {};
            GetObject(hBmp, sizeof(bm), &bm);
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hdcMem, hBmp));
            TransparentBlt(hdc, 0, 0, w, h,
                           hdcMem, 0, 0, bm.bmWidth, bm.bmHeight,
                           TRANS_COLOR);
            SelectObject(hdcMem, hOld);
            DeleteDC(hdcMem);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    }
    case WM_MOUSELEAVE:
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    }

    return CallWindowProcW(s_origBtnProc, hwnd, msg, wParam, lParam);
}

static void SubclassButton(HWND hBtn)
{
    WNDPROC prev = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hBtn, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(BtnSubclassProc)));
    if (!s_origBtnProc)
        s_origBtnProc = prev;
}

// ---- bitmap loading --------------------------------------------------------
static HBITMAP LoadBitmapRes(HINSTANCE hInst, int id)
{
    return static_cast<HBITMAP>(
        LoadImageW(hInst, MAKEINTRESOURCEW(id), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR));
}

// Locate the assets directory: try alongside the exe first, then walk up
// parent directories (handles the dev layout where exe is in bin/Release/).
static void FindAssetsDir(wchar_t* out, int cch)
{
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
    PathRemoveFileSpecW(exeDir);

    // Try up to 4 levels: exeDir, .., ../.., ../../..
    wchar_t probe[MAX_PATH];
    wcscpy_s(probe, exeDir);
    for (int i = 0; i < 4; ++i) {
        wchar_t test[MAX_PATH];
        wcscpy_s(test, probe);
        PathAppendW(test, L"assets");
        if (PathIsDirectoryW(test)) {
            wcscpy_s(out, cch, test);
            return;
        }
        PathRemoveFileSpecW(probe);  // go up one level
    }
    // Fallback: assume alongside exe
    wcscpy_s(out, cch, exeDir);
    PathAppendW(out, L"assets");
}

static void SetThemeDirectoryForIndex(int themeIndex)
{
    g_main.themeDir[0] = L'\0';
    if (themeIndex <= 0 || themeIndex >= kThemeCount)
        return;

    wchar_t assetsDir[MAX_PATH];
    FindAssetsDir(assetsDir, MAX_PATH);

    // Prefer the structured folder layout: assets/themes/<theme>.
    wchar_t themeRoot[MAX_PATH];
    wcscpy_s(themeRoot, assetsDir);
    PathAppendW(themeRoot, L"themes");
    if (!PathIsDirectoryW(themeRoot))
        wcscpy_s(themeRoot, assetsDir); // backward compatibility with assets/<theme>

    wcscpy_s(g_main.themeDir, themeRoot);
    PathAppendW(g_main.themeDir, kThemeNames[themeIndex]);
}

// Try to load a bitmap from the active theme folder on disk; fall back to the
// embedded resource if the file is missing or the theme dir is not set.
// The default theme (index 0 = 20100513_TW) is embedded in the exe ??no disk needed.
static HBITMAP TryLoadThemeBitmap(const wchar_t* filename, int fallbackId, int themeIndex)
{
    // Non-default themes: try loading from disk first
    if (themeIndex > 0 && g_main.themeDir[0]) {
        wchar_t path[MAX_PATH];
        wcscpy_s(path, g_main.themeDir);
        PathAppendW(path, filename);
        HBITMAP h = static_cast<HBITMAP>(
            LoadImageW(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
        if (h) return h;
    }
    // Fallback to embedded theme resources.
    int rid = ThemeBitmapResourceFallback(themeIndex, filename);
    if (rid == 0)
        rid = fallbackId;
    return LoadBitmapRes(g_main.hInst, rid);
}

static void ApplySepLayoutForTheme(int themeIndex)
{
    if (themeIndex < 0 || themeIndex >= kThemeCount)
        themeIndex = 0;

    g_main.sepX = kSepLayouts[themeIndex].x;
    g_main.sepCurY = kSepLayouts[themeIndex].curY;
    g_main.sepOverallY = kSepLayouts[themeIndex].overallY;
}

static void ApplyButtonLayoutForTheme(HWND hDlg, int themeIndex)
{
    if (!hDlg) return;
    if (themeIndex < 0 || themeIndex >= kThemeCount)
        themeIndex = 0;

    const ThemeButtonLayout& layout = kButtonLayouts[themeIndex];
    MoveWindow(GetDlgItem(hDlg, IDC_BTN_EXIT),
               layout.closeX, layout.closeY, layout.closeW, layout.closeH, FALSE);
    MoveWindow(GetDlgItem(hDlg, IDC_BTN_OPTIONS),
               layout.optionsX, layout.optionsY, layout.optionsW, layout.optionsH, FALSE);
    MoveWindow(GetDlgItem(hDlg, IDC_BTN_START),
               layout.startX, layout.startY, layout.startW, layout.startH, FALSE);
}

static void RefreshSepBitmapMetrics()
{
    g_main.sepTrackW = SEP_DEFAULT_W;
    g_main.sepTrackH = SEP_DEFAULT_H;
    HBITMAP hBase = g_main.hSepTrackDisabled ? g_main.hSepTrackDisabled : g_main.hSepTrackHover;
    BITMAP bm = {};
    if (hBase && GetObject(hBase, sizeof(bm), &bm) == sizeof(BITMAP)) {
        g_main.sepTrackW = bm.bmWidth;
        g_main.sepTrackH = bm.bmHeight;
    }
}

static void DrawFallbackProgressLine(HDC hdc, int x, int y, int w, int h, float progress, COLORREF topColor, COLORREF bodyColor)
{
    if (!hdc || w <= 0 || h <= 0) return;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    RECT rcTrack = { x, y, x + w, y + h };
    HBRUSH hTrack = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rcTrack, hTrack);
    DeleteObject(hTrack);

    int fillW = static_cast<int>(progress * static_cast<float>(w) + 0.5f);
    if (fillW <= 0) return;

    RECT rcTop = { x, y, x + fillW, y + 1 };
    HBRUSH hTop = CreateSolidBrush(topColor);
    FillRect(hdc, &rcTop, hTop);
    DeleteObject(hTop);

    if (h > 1) {
        RECT rcBody = { x, y + 1, x + fillW, y + h };
        HBRUSH hBody = CreateSolidBrush(bodyColor);
        FillRect(hdc, &rcBody, hBody);
        DeleteObject(hBody);
    }
}

static void DrawSepProgressLine(HDC hdc, int y, float progress, HBITMAP hFill)
{
    if (!hdc) return;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    HBITMAP hTrack = g_main.hSepTrackDisabled ? g_main.hSepTrackDisabled : g_main.hSepTrackHover;
    if (!hTrack || !hFill) {
        COLORREF top = (y == g_main.sepCurY) ? RGB(100, 245, 240) : RGB(235, 230, 120);
        COLORREF body = (y == g_main.sepCurY) ? RGB(0, 145, 135) : RGB(120, 115, 20);
        DrawFallbackProgressLine(hdc, g_main.sepX, y, g_main.sepTrackW, g_main.sepTrackH, progress, top, body);
        return;
    }

    HDC memTrack = CreateCompatibleDC(hdc);
    HBITMAP oldTrack = static_cast<HBITMAP>(SelectObject(memTrack, hTrack));
    BitBlt(hdc, g_main.sepX, y, g_main.sepTrackW, g_main.sepTrackH, memTrack, 0, 0, SRCCOPY);
    SelectObject(memTrack, oldTrack);
    DeleteDC(memTrack);

    if (progress <= 0.0f) return;

    BITMAP fillBm = {};
    int fillW = g_main.sepTrackW;
    int fillH = g_main.sepTrackH;
    if (GetObject(hFill, sizeof(fillBm), &fillBm) == sizeof(BITMAP)) {
        fillW = fillBm.bmWidth;
        fillH = fillBm.bmHeight;
    }
    if (fillW <= 0 || fillH <= 0) return;

    int fillOffX = (g_main.sepTrackW - fillW) / 2;
    int fillOffY = (g_main.sepTrackH - fillH) / 2;
    if (fillOffX < 0) fillOffX = 0;
    if (fillOffY < 0) fillOffY = 0;

    int drawW = static_cast<int>(progress * static_cast<float>(fillW) + 0.5f);
    if (drawW <= 0) return;
    if (drawW > fillW) drawW = fillW;
    if (drawW > g_main.sepTrackW) drawW = g_main.sepTrackW;

    HDC memFill = CreateCompatibleDC(hdc);
    HBITMAP oldFill = static_cast<HBITMAP>(SelectObject(memFill, hFill));
    BitBlt(hdc,
           g_main.sepX + fillOffX,
           y + fillOffY,
           drawW,
           fillH,
           memFill,
           0, 0,
           SRCCOPY);
    SelectObject(memFill, oldFill);
    DeleteDC(memFill);
}

static void DrawStubProgressLines(HDC hdc)
{
    // First line = total progress, second line = current-file progress.
    DrawSepProgressLine(hdc, g_main.sepCurY, g_main.overallProgress, g_main.hSepFillTotal);
    DrawSepProgressLine(hdc, g_main.sepOverallY, g_main.curFileProgress, g_main.hSepFillCurrent);
}

static void UpdateLoadStub(HWND hDlg)
{
    if (g_main.loadStubDone) return;

    DWORD elapsed = GetTickCount() - g_main.loadStubStartTick;
    if (elapsed >= LOAD_STUB_TOTAL_MS) {
        g_main.curFileProgress = 1.0f;
        g_main.overallProgress = 1.0f;
        g_main.loadStubDone = true;
        KillTimer(hDlg, IDT_LOAD_STUB);
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_START), TRUE);
        InvalidateRect(GetDlgItem(hDlg, IDC_BTN_START), nullptr, TRUE);
    } else {
        // Overall moves smoothly 0->1, current-file loops twice during the stub.
        float overall = static_cast<float>(elapsed) / static_cast<float>(LOAD_STUB_TOTAL_MS);
        float current = overall * 2.0f;
        if (current > 1.0f)
            current -= 1.0f;
        g_main.overallProgress = overall;
        g_main.curFileProgress = current;
    }

    int top = (g_main.sepCurY < g_main.sepOverallY) ? g_main.sepCurY : g_main.sepOverallY;
    int bottom = (g_main.sepCurY > g_main.sepOverallY) ? g_main.sepCurY : g_main.sepOverallY;
    bottom += g_main.sepTrackH;
    RECT rcDirty = { g_main.sepX, top, g_main.sepX + g_main.sepTrackW, bottom };
    InvalidateRect(hDlg, &rcDirty, TRUE);
}

// Recompute theme dir, free + reload all bitmaps, reshape and repaint the window.
static void ReloadThemeBitmaps(HWND hDlg)
{
    // Free old bitmaps
    DeleteObject(g_main.hBgBitmap);        g_main.hBgBitmap       = nullptr;
    DeleteObject(g_main.hCloseNormal);     g_main.hCloseNormal    = nullptr;
    DeleteObject(g_main.hCloseHover);      g_main.hCloseHover     = nullptr;
    DeleteObject(g_main.hClosePressed);    g_main.hClosePressed   = nullptr;
    DeleteObject(g_main.hOptionsNormal);   g_main.hOptionsNormal  = nullptr;
    DeleteObject(g_main.hOptionsHover);    g_main.hOptionsHover   = nullptr;
    DeleteObject(g_main.hOptionsPressed);  g_main.hOptionsPressed = nullptr;
    DeleteObject(g_main.hOptionsDisabled); g_main.hOptionsDisabled= nullptr;
    DeleteObject(g_main.hStartNormal);     g_main.hStartNormal    = nullptr;
    DeleteObject(g_main.hStartHover);      g_main.hStartHover     = nullptr;
    DeleteObject(g_main.hStartPressed);    g_main.hStartPressed   = nullptr;
    DeleteObject(g_main.hStartDisabled);   g_main.hStartDisabled  = nullptr;
    DeleteObject(g_main.hSepFillTotal);       g_main.hSepFillTotal      = nullptr;
    DeleteObject(g_main.hSepTrackDisabled);     g_main.hSepTrackDisabled    = nullptr;
    DeleteObject(g_main.hSepTrackHover);        g_main.hSepTrackHover       = nullptr;
    DeleteObject(g_main.hSepFillCurrent);      g_main.hSepFillCurrent     = nullptr;

    int tidx = g_main.settings->ThemeIndex;
    SetThemeDirectoryForIndex(tidx);

    // Reload bitmaps
    g_main.hBgBitmap       = TryLoadThemeBitmap(L"main_bg.bmp",         IDB_MAIN_BG,            tidx);
    g_main.hCloseNormal    = TryLoadThemeBitmap(L"btn_close_normal.bmp",    IDB_BTN_CLOSE_NORMAL,   tidx);
    g_main.hCloseHover     = TryLoadThemeBitmap(L"btn_close_hover.bmp",     IDB_BTN_CLOSE_HOVER,    tidx);
    g_main.hClosePressed   = TryLoadThemeBitmap(L"btn_close_pressed.bmp",   IDB_BTN_CLOSE_PRESSED,  tidx);
    g_main.hOptionsNormal  = TryLoadThemeBitmap(L"btn_options_normal.bmp",  IDB_BTN_OPTIONS_NORMAL, tidx);
    g_main.hOptionsHover   = TryLoadThemeBitmap(L"btn_options_hover.bmp",   IDB_BTN_OPTIONS_HOVER,  tidx);
    g_main.hOptionsPressed = TryLoadThemeBitmap(L"btn_options_pressed.bmp", IDB_BTN_OPTIONS_PRESSED,tidx);
    g_main.hOptionsDisabled= TryLoadThemeBitmap(L"btn_options_disabled.bmp",IDB_BTN_OPTIONS_DISABLED,tidx);
    g_main.hStartNormal    = TryLoadThemeBitmap(L"btn_start_normal.bmp",    IDB_BTN_START_NORMAL,   tidx);
    g_main.hStartHover     = TryLoadThemeBitmap(L"btn_start_hover.bmp",     IDB_BTN_START_HOVER,    tidx);
    g_main.hStartPressed   = TryLoadThemeBitmap(L"btn_start_pressed.bmp",   IDB_BTN_START_PRESSED,  tidx);
    g_main.hStartDisabled  = TryLoadThemeBitmap(L"btn_start_disabled.bmp",  IDB_BTN_START_DISABLED, tidx);
    g_main.hSepFillTotal      = TryLoadThemeBitmap(L"sep_fill_total.bmp",          IDB_SEP_FILL_TOTAL,        tidx);
    g_main.hSepTrackDisabled    = TryLoadThemeBitmap(L"sep_track_disabled.bmp",        IDB_SEP_TRACK_DISABLED,  tidx);
    g_main.hSepTrackHover       = TryLoadThemeBitmap(L"sep_track_hover.bmp",           IDB_SEP_TRACK_HOVER,     tidx);
    g_main.hSepFillCurrent     = TryLoadThemeBitmap(L"sep_fill_current.bmp",         IDB_SEP_FILL_CURRENT,    tidx);
    ApplyButtonLayoutForTheme(hDlg, tidx);
    ApplySepLayoutForTheme(tidx);
    RefreshSepBitmapMetrics();

    // Rebuild window shape from the new background
    HRGN hRgn = CreateRegionFromBitmap(g_main.hBgBitmap, TRANS_COLOR);
    SetWindowRgn(hDlg, hRgn, FALSE);   // Windows takes ownership of hRgn

    // Repaint everything
    EnableWindow(GetDlgItem(hDlg, IDC_BTN_START), g_main.loadStubDone ? TRUE : FALSE);
    InvalidateRect(hDlg, nullptr, TRUE);
    InvalidateRect(GetDlgItem(hDlg, IDC_BTN_EXIT),    nullptr, TRUE);
    InvalidateRect(GetDlgItem(hDlg, IDC_BTN_OPTIONS), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hDlg, IDC_BTN_START),   nullptr, TRUE);
    UpdateWindow(hDlg);
}

// ---- main dialog proc ------------------------------------------------------
static INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        // Fully borderless
        SetWindowLongW(hDlg, GWL_STYLE,
            GetWindowLongW(hDlg, GWL_STYLE)
            & ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER));
        SetWindowLongW(hDlg, GWL_EXSTYLE,
            GetWindowLongW(hDlg, GWL_EXSTYLE)
            & ~(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE));

        // Set window icon (shows in taskbar and Alt-Tab)
        HICON hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_LAUNCHER));
        if (hIcon) {
            SendMessageW(hDlg, WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(hIcon));
            SendMessageW(hDlg, WM_SETICON, ICON_SMALL,  reinterpret_cast<LPARAM>(hIcon));
        }

        // Resize to match background bitmap
        const int cx = GetSystemMetrics(SM_CXSCREEN);
        const int cy = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hDlg, nullptr,
            (cx - DLG_W) / 2, (cy - DLG_H) / 2, DLG_W, DLG_H,
            SWP_NOZORDER | SWP_FRAMECHANGED);

        int tidx = g_main.settings->ThemeIndex;
        SetThemeDirectoryForIndex(tidx);

        // Place each button at its exact pixel slot in the background image
        ApplyButtonLayoutForTheme(hDlg, tidx);

        // Load bitmaps (theme-aware; falls back to embedded resources)
        g_main.hBgBitmap       = TryLoadThemeBitmap(L"main_bg.bmp",         IDB_MAIN_BG,            tidx);
        g_main.hCloseNormal    = TryLoadThemeBitmap(L"btn_close_normal.bmp",    IDB_BTN_CLOSE_NORMAL,   tidx);
        g_main.hCloseHover     = TryLoadThemeBitmap(L"btn_close_hover.bmp",     IDB_BTN_CLOSE_HOVER,    tidx);
        g_main.hClosePressed   = TryLoadThemeBitmap(L"btn_close_pressed.bmp",   IDB_BTN_CLOSE_PRESSED,  tidx);
        g_main.hOptionsNormal  = TryLoadThemeBitmap(L"btn_options_normal.bmp",  IDB_BTN_OPTIONS_NORMAL, tidx);
        g_main.hOptionsHover   = TryLoadThemeBitmap(L"btn_options_hover.bmp",   IDB_BTN_OPTIONS_HOVER,  tidx);
        g_main.hOptionsPressed = TryLoadThemeBitmap(L"btn_options_pressed.bmp", IDB_BTN_OPTIONS_PRESSED,tidx);
        g_main.hOptionsDisabled= TryLoadThemeBitmap(L"btn_options_disabled.bmp",IDB_BTN_OPTIONS_DISABLED,tidx);
        g_main.hStartNormal    = TryLoadThemeBitmap(L"btn_start_normal.bmp",    IDB_BTN_START_NORMAL,   tidx);
        g_main.hStartHover     = TryLoadThemeBitmap(L"btn_start_hover.bmp",     IDB_BTN_START_HOVER,    tidx);
        g_main.hStartPressed   = TryLoadThemeBitmap(L"btn_start_pressed.bmp",   IDB_BTN_START_PRESSED,  tidx);
        g_main.hStartDisabled  = TryLoadThemeBitmap(L"btn_start_disabled.bmp",  IDB_BTN_START_DISABLED, tidx);
        g_main.hSepFillTotal      = TryLoadThemeBitmap(L"sep_fill_total.bmp",          IDB_SEP_FILL_TOTAL,        tidx);
        g_main.hSepTrackDisabled    = TryLoadThemeBitmap(L"sep_track_disabled.bmp",        IDB_SEP_TRACK_DISABLED,  tidx);
        g_main.hSepTrackHover       = TryLoadThemeBitmap(L"sep_track_hover.bmp",           IDB_SEP_TRACK_HOVER,     tidx);
        g_main.hSepFillCurrent     = TryLoadThemeBitmap(L"sep_fill_current.bmp",         IDB_SEP_FILL_CURRENT,    tidx);
        ApplySepLayoutForTheme(tidx);
        RefreshSepBitmapMetrics();

        // Shape the window: cut out all magenta pixels in the background
        HRGN hRgn = CreateRegionFromBitmap(g_main.hBgBitmap, TRANS_COLOR);
        if (hRgn)
            SetWindowRgn(hDlg, hRgn, FALSE);   // Windows owns hRgn after this call

        // Subclass all three buttons for transparent bitmap rendering + hover
        SubclassButton(GetDlgItem(hDlg, IDC_BTN_EXIT));
        SubclassButton(GetDlgItem(hDlg, IDC_BTN_OPTIONS));
        SubclassButton(GetDlgItem(hDlg, IDC_BTN_START));

        // Startup stub: show 3s fake loading before enabling Start.
        g_main.curFileProgress = 0.0f;
        g_main.overallProgress = 0.0f;
        g_main.loadStubDone = false;
        g_main.loadStubStartTick = GetTickCount();
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_START), FALSE);
        SetTimer(hDlg, IDT_LOAD_STUB, LOAD_STUB_TICK_MS, nullptr);

        return TRUE;
    }

    // BS_OWNERDRAW buttons send WM_DRAWITEM ??we paint in the subclass WM_PAINT instead
    case WM_DRAWITEM:
        return TRUE;

    case WM_TIMER:
        if (wParam == IDT_LOAD_STUB) {
            UpdateLoadStub(hDlg);
            return TRUE;
        }
        break;

    case WM_ERASEBKGND:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        if (g_main.hBgBitmap)
        {
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hdcMem, g_main.hBgBitmap));
            BitBlt(hdc, 0, 0, DLG_W, DLG_H, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hOld);
            DeleteDC(hdcMem);
        }
        else
        {
            RECT rc; GetClientRect(hDlg, &rc);
            HBRUSH hBr = CreateSolidBrush(RGB(30, 28, 45));
            FillRect(hdc, &rc, hBr);
            DeleteObject(hBr);
        }

        DrawStubProgressLines(hdc);
        return TRUE;
    }

    // Drag the borderless window by clicking the background
    case WM_LBUTTONDOWN:
    {
        s_dragging = true;
        GetCursorPos(&s_dragOrig);
        RECT wr; GetWindowRect(hDlg, &wr);
        s_winOrig = { wr.left, wr.top };
        SetCapture(hDlg);
        break;
    }
    case WM_MOUSEMOVE:
    {
        if (s_dragging)
        {
            POINT cur; GetCursorPos(&cur);
            SetWindowPos(hDlg, nullptr,
                s_winOrig.x + cur.x - s_dragOrig.x,
                s_winOrig.y + cur.y - s_dragOrig.y,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        break;
    }
    case WM_LBUTTONUP:
        s_dragging = false;
        ReleaseCapture();
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BTN_START:
        {
            if (!PathFileExistsW(g_main.gameExe))
            {
                MessageBoxW(hDlg,
                    L"Cannot find Prius.exe.\n\nMake sure Prius.exe is in the same folder as the launcher.",
                    L"Prius Launcher", MB_ICONERROR | MB_OK);
                break;
            }
            wchar_t dir[MAX_PATH];
            wcscpy_s(dir, g_main.gameExe);
            PathRemoveFileSpecW(dir);

            // Build command-line arguments: -theme <profile>
            std::wstring args = L"-theme " + g_main.settings->Profile;

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask       = SEE_MASK_NOASYNC;
            sei.lpVerb      = L"open";
            sei.lpFile      = g_main.gameExe;
            sei.lpParameters = args.c_str();
            sei.lpDirectory = dir;
            sei.nShow       = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei))
                PostMessageW(hDlg, WM_CLOSE, 0, 0);
            else
                MessageBoxW(hDlg, L"Failed to start Prius.exe.", L"Prius Launcher", MB_ICONERROR | MB_OK);
            break;
        }
        case IDC_BTN_OPTIONS:
        {
            int prevTheme = g_main.settings->ThemeIndex;
            ShowOptionsDialog(hDlg, *g_main.settings);
            if (g_main.settings->ThemeIndex != prevTheme)
                ReloadThemeBitmaps(hDlg);
            break;
        }
        case IDC_BTN_EXIT:
        case IDCANCEL:
            PostMessageW(hDlg, WM_CLOSE, 0, 0);
            break;
        }
        break;

    case WM_CLOSE:
        KillTimer(hDlg, IDT_LOAD_STUB);
        DeleteObject(g_main.hBgBitmap);
        DeleteObject(g_main.hCloseNormal);     DeleteObject(g_main.hCloseHover);
        DeleteObject(g_main.hClosePressed);
        DeleteObject(g_main.hOptionsNormal);   DeleteObject(g_main.hOptionsHover);
        DeleteObject(g_main.hOptionsPressed);  DeleteObject(g_main.hOptionsDisabled);
        DeleteObject(g_main.hStartNormal);     DeleteObject(g_main.hStartHover);
        DeleteObject(g_main.hStartPressed);    DeleteObject(g_main.hStartDisabled);
        DeleteObject(g_main.hSepFillTotal);       DeleteObject(g_main.hSepTrackDisabled);
        DeleteObject(g_main.hSepTrackHover);        DeleteObject(g_main.hSepFillCurrent);
        EndDialog(hDlg, 0);
        break;
    }
    return FALSE;
}

// ---- public entry -----------------------------------------------------------
void ShowMainDialog(HINSTANCE hInst, GameSettings& settings)
{
    g_main          = {};
    g_main.hInst    = hInst;
    g_main.settings = &settings;

    GetModuleFileNameW(hInst, g_main.gameExe, MAX_PATH);
    PathRemoveFileSpecW(g_main.gameExe);
    PathAppendW(g_main.gameExe, L"Prius.exe");

    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_MAIN), nullptr, MainDlgProc);
}


