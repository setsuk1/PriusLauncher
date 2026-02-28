#include "GameSettings.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static const wchar_t* REG_KEY = L"SOFTWARE\\Prius";

// ---- PriusLauncher.gui file helpers -----------------------------------------
// Simple key=value file alongside the executable for launcher-specific settings.

static void GetGuiFilePath(wchar_t* out, int cch)
{
    GetModuleFileNameW(nullptr, out, cch);
    PathRemoveFileSpecW(out);
    PathAppendW(out, L"PriusLauncher.gui");
}

static int ReadGuiInt(const wchar_t* key, int def)
{
    wchar_t path[MAX_PATH];
    GetGuiFilePath(path, MAX_PATH);
    wchar_t defStr[32], buf[64];
    swprintf_s(defStr, L"%d", def);
    GetPrivateProfileStringW(L"Launcher", key, defStr, buf, 64, path);
    return _wtoi(buf);
}

static void WriteGuiInt(const wchar_t* key, int value)
{
    wchar_t path[MAX_PATH];
    GetGuiFilePath(path, MAX_PATH);
    wchar_t buf[32];
    swprintf_s(buf, L"%d", value);
    WritePrivateProfileStringW(L"Launcher", key, buf, path);
}

static std::wstring ReadGuiString(const wchar_t* key, const wchar_t* def)
{
    wchar_t path[MAX_PATH];
    GetGuiFilePath(path, MAX_PATH);
    wchar_t buf[256];
    GetPrivateProfileStringW(L"Launcher", key, def, buf, 256, path);
    return buf;
}

static void WriteGuiString(const wchar_t* key, const wchar_t* value)
{
    wchar_t path[MAX_PATH];
    GetGuiFilePath(path, MAX_PATH);
    WritePrivateProfileStringW(L"Launcher", key, value, path);
}

// ---- registry helpers --------------------------------------------------------

static HKEY OpenKey(bool writable)
{
    HKEY hk = nullptr;
    if (writable)
        RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hk, nullptr);
    else
        RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &hk);
    return hk;
}

static int ReadDword(HKEY hk, const wchar_t* name, int def)
{
    if (!hk) return def;
    DWORD val = 0, sz = sizeof(val), type = 0;
    if (RegQueryValueExW(hk, name, nullptr, &type, reinterpret_cast<BYTE*>(&val), &sz) == ERROR_SUCCESS
        && type == REG_DWORD)
        return static_cast<int>(val);
    return def;
}

static float ReadFloat(HKEY hk, const wchar_t* name, float def)
{
    if (!hk) return def;
    // Original stores floats as REG_SZ strings (e.g. "0.390000")
    wchar_t buf[64] = {};
    DWORD sz = sizeof(buf), type = 0;
    if (RegQueryValueExW(hk, name, nullptr, &type, reinterpret_cast<BYTE*>(buf), &sz) == ERROR_SUCCESS
        && type == REG_SZ)
        return static_cast<float>(_wtof(buf));
    return def;
}

static bool ReadBool(HKEY hk, const wchar_t* name, bool def)
{
    return ReadDword(hk, name, def ? 1 : 0) != 0;
}

static void WriteDword(HKEY hk, const wchar_t* name, int value)
{
    if (!hk) return;
    DWORD v = static_cast<DWORD>(value);
    RegSetValueExW(hk, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof v);
}

static void WriteFloat(HKEY hk, const wchar_t* name, float value)
{
    if (!hk) return;
    // Original stores floats as REG_SZ strings (e.g. "1.000000")
    wchar_t buf[64];
    swprintf_s(buf, L"%f", value);
    RegSetValueExW(hk, name, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(buf),
                   static_cast<DWORD>((wcslen(buf) + 1) * sizeof(wchar_t)));
}

static void WriteBool(HKEY hk, const wchar_t* name, bool value)
{
    WriteDword(hk, name, value ? 1 : 0);
}

// ---- GameSettings::Load ------------------------------------------------------

void GameSettings::Load()
{
    HKEY hk = OpenKey(false);

    // Read version stamp to gate which fields exist
    unsigned int ver = static_cast<unsigned int>(ReadDword(hk, L"Version", 0));

    if (ver > 0x132671b) {
        // ShowCmd only exists in versions > 0x1328e3a
        if (ver > 0x1328e3a)
            ShowCmd = ReadDword(hk, L"ShowCmd", 10);

        // UIReset: original uses strict == 1 (not != 0)
        UIReset             = ReadDword(hk, L"UIReset", 0) == 1;
        UIStyle             = ReadDword(hk, L"UIStyle",             0);
        ReadDword(hk, L"SpeechBalloonLevel", 0); // read but always forced to 0
        SpeechBalloonLevel  = 0;
        FocusType           = ReadDword(hk, L"FocusType",           0);
        FocusOnTalk         = ReadBool (hk, L"FocusOnTalk",         true);
        ScreenShotFileFormat= ReadDword(hk, L"ScreenShotFileFormat",0);
        NpcInfoTooltipType  = ReadDword(hk, L"NpcInfoTooltipType",  0);

        if (ver > 0x1328e3a)
            InverceTargetting = ReadBool(hk, L"InverceTargetting", false);
        if (ver > 0x1328d08)
            AutoSwappable     = ReadBool(hk, L"AutoSwappable",     false);
        if (ver > 0x1328d59)
            SkillEffectLevel  = ReadDword(hk, L"SkillEffectLevel",  0);

        // ShadowFilter/ShadowFilterType forced to 0 for versions > 0x1328dc5
        if (ver > 0x1328dc5) {
            ShadowFilter     = false;
            ShadowFilterType = 0;
        }

        AttackEffectLevel   = ReadDword(hk, L"AttackEffectLevel",   0);
        NameTagPerspective  = ReadBool (hk, L"NameTagPerspective",  false);
        // +0x35 unnamed field always set to 0 in original (we don't expose it)
        QuestFilter         = ReadBool (hk, L"QuestFilter",         false);
        MemberAlarm         = ReadBool (hk, L"MemberAlarm",         true);
        GameHelpMessage     = ReadBool (hk, L"GameHelpMessage",     true);
        ChatInputMode       = ReadBool (hk, L"ChatInputMode",       true);
        UseMouseMove        = ReadBool (hk, L"UseMouseMove",        true);

        if (ver > 0x1328e33)
            PreLoad         = ReadBool(hk, L"PreLoad",             false);

        FullScreen   = ReadBool (hk, L"FullScreen",   false);
        ScreenWidth  = ReadDword(hk, L"ScreenWidth",  0);
        ScreenHeight = ReadDword(hk, L"ScreenHeight", 0);

        GammaRamp           = ReadFloat(hk, L"GammaRamp",           1.0f);
        GraphicQuality      = ReadDword(hk, L"GraphicQuality",      0);
        FarClipLevel        = ReadFloat(hk, L"FarClipLevel",        1.0f);

        if (ver > 0x1326a46)
            TextureLevel    = ReadDword(hk, L"TextureLevel",        0);

        ShaderLevel         = ReadDword(hk, L"ShaderLevel",         0);
        ShadowLevel         = ReadDword(hk, L"ShadowLevel",         0);
        ShadowTestLevel     = ReadDword(hk, L"ShadowTestLevel",     0);
        SelfShadowLevel     = ReadDword(hk, L"SelfShadowLevel",     0);
        AlphaBlendLevel     = ReadDword(hk, L"AlphaBlendLevel",     0);
        DynamicLightingLevel= ReadDword(hk, L"DynamicLightingLevel",0);
        OcclusionShadeLevel = ReadDword(hk, L"OcclusionShadeLevel", 0);
        GrassLevel          = ReadFloat(hk, L"GrassLevel",          1.0f);
        LensFlareLevel      = ReadDword(hk, L"LensFlareLevel",      0);
        WaterReflectLevel   = ReadDword(hk, L"WaterReflectLevel",   0);
        WaterRefractLevel   = ReadDword(hk, L"WaterRefractLevel",   0);
        PostEffectLevel     = ReadDword(hk, L"PostEffectLevel",     0);
        FootstepMeshLevel   = ReadDword(hk, L"FootstepMeshLevel",   0);

        if (ver > 0x1328d59)
            Decal           = ReadBool(hk, L"Decal",               false);

        ShotcutKeyVersion   = ReadDword(hk, L"ShotcutKeyVersion",   0);

        VolumeMute          = ReadBool (hk, L"VolumeMute",          false);
        HWMixing            = ReadBool (hk, L"HWMixing",            false);
        SoundEnable         = ReadBool (hk, L"Eax",                 false);
        BgmVolume           = ReadFloat(hk, L"BgmVolume",           1.0f);
        SfxVolume           = ReadFloat(hk, L"SfxVolume",           1.0f);
        ListenerLevel       = ReadDword(hk, L"ListenerLevel",       1);
        FootstepSoundLevel  = ReadDword(hk, L"FootstepSoundLevel",  0);
    }

    if (hk) RegCloseKey(hk);

    // Launcher-specific preferences (from PriusLauncher.gui file)
    ThemeIndex = ReadGuiInt(L"ThemeIndex", 0);
    Language   = ReadGuiInt(L"Language",   0);
    Profile    = ReadGuiString(L"Profile", L"release");
}

// ---- GameSettings::Save ------------------------------------------------------

void GameSettings::Save() const
{
    HKEY hk = OpenKey(true);
    if (!hk) return;

    // Version stamp from original binary
    WriteDword(hk, L"Version", 0x1328e3b);

    WriteBool (hk, L"UIReset",             UIReset);
    WriteDword(hk, L"UIStyle",             UIStyle);
    WriteDword(hk, L"SpeechBalloonLevel",  SpeechBalloonLevel);
    WriteDword(hk, L"FocusType",           FocusType);
    WriteBool (hk, L"FocusOnTalk",         FocusOnTalk);
    WriteDword(hk, L"ScreenShotFileFormat",ScreenShotFileFormat);
    WriteDword(hk, L"NpcInfoTooltipType",  NpcInfoTooltipType);
    WriteBool (hk, L"InverceTargetting",   InverceTargetting);
    WriteBool (hk, L"AutoSwappable",       AutoSwappable);
    WriteDword(hk, L"SkillEffectLevel",    SkillEffectLevel);
    WriteDword(hk, L"AttackEffectLevel",   AttackEffectLevel);
    WriteBool (hk, L"ShadowFilter",        ShadowFilter);
    WriteDword(hk, L"ShadowFilterType",    ShadowFilterType);
    WriteBool (hk, L"NameTagPerspective",  NameTagPerspective);
    WriteBool (hk, L"QuestFilter",         QuestFilter);
    WriteBool (hk, L"MemberAlarm",         MemberAlarm);
    WriteBool (hk, L"GameHelpMessage",     GameHelpMessage);
    WriteBool (hk, L"PreLoad",             PreLoad);
    WriteBool (hk, L"ChatInputMode",       ChatInputMode);
    WriteBool (hk, L"UseMouseMove",        UseMouseMove);

    WriteBool (hk, L"FullScreen",          FullScreen);
    WriteDword(hk, L"ShowCmd",             ShowCmd);
    WriteDword(hk, L"ScreenWidth",         ScreenWidth);
    WriteDword(hk, L"ScreenHeight",        ScreenHeight);

    WriteFloat(hk, L"GammaRamp",           GammaRamp);
    WriteDword(hk, L"GraphicQuality",      GraphicQuality);
    WriteFloat(hk, L"FarClipLevel",        FarClipLevel);
    WriteDword(hk, L"TextureLevel",        TextureLevel);
    WriteDword(hk, L"ShaderLevel",         ShaderLevel);
    WriteDword(hk, L"ShadowLevel",         ShadowLevel);
    WriteDword(hk, L"ShadowTestLevel",     ShadowTestLevel);
    WriteDword(hk, L"SelfShadowLevel",     SelfShadowLevel);
    WriteDword(hk, L"AlphaBlendLevel",     AlphaBlendLevel);
    WriteDword(hk, L"DynamicLightingLevel",DynamicLightingLevel);
    WriteDword(hk, L"OcclusionShadeLevel", OcclusionShadeLevel);
    WriteFloat(hk, L"GrassLevel",          GrassLevel);
    WriteDword(hk, L"LensFlareLevel",      LensFlareLevel);
    WriteDword(hk, L"WaterReflectLevel",   WaterReflectLevel);
    WriteDword(hk, L"WaterRefractLevel",   WaterRefractLevel);
    WriteDword(hk, L"PostEffectLevel",     PostEffectLevel);
    WriteDword(hk, L"FootstepMeshLevel",   FootstepMeshLevel);
    WriteBool (hk, L"Decal",               Decal);

    WriteDword(hk, L"ShotcutKeyVersion",   ShotcutKeyVersion);

    WriteBool (hk, L"VolumeMute",          VolumeMute);
    WriteBool (hk, L"HWMixing",           HWMixing);
    WriteBool (hk, L"Eax",                 SoundEnable);
    WriteFloat(hk, L"BgmVolume",           BgmVolume);
    WriteFloat(hk, L"SfxVolume",           SfxVolume);
    WriteDword(hk, L"ListenerLevel",       ListenerLevel);
    WriteDword(hk, L"FootstepSoundLevel",  FootstepSoundLevel);

    RegCloseKey(hk);

    // Launcher-specific preferences (to PriusLauncher.gui file)
    SaveLauncherPrefs();
}

void GameSettings::SaveLauncherPrefs() const
{
    WriteGuiInt(L"ThemeIndex", ThemeIndex);
    WriteGuiInt(L"Language",   Language);
    WriteGuiString(L"Profile", Profile.c_str());
}
