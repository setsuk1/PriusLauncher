#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Registry key used by Prius Online to persist game options.
// Matches HKCU\SOFTWARE\Prius from the original launcher binary.
struct GameSettings
{
    // --- Interface ---
    bool UIReset            = false;
    int  UIStyle            = 0;
    int  SpeechBalloonLevel = 0;
    int  FocusType          = 0;
    bool FocusOnTalk        = true;
    int  ScreenShotFileFormat = 0;
    int  NpcInfoTooltipType = 0;
    bool InverceTargetting  = false;
    bool AutoSwappable      = false;
    int  SkillEffectLevel   = 0;
    int  AttackEffectLevel  = 0;
    bool ShadowFilter       = false;
    int  ShadowFilterType   = 0;
    bool NameTagPerspective = false;
    bool QuestFilter        = false;
    bool MemberAlarm        = true;
    bool GameHelpMessage    = true;
    bool PreLoad            = false;
    bool ChatInputMode      = true;
    bool UseMouseMove       = true;

    // --- Display ---
    bool FullScreen   = false;  // original default: 0
    int  ShowCmd      = 10;     // original default: SW_SHOWDEFAULT
    int  ScreenWidth  = 0;    // original default: 0
    int  ScreenHeight = 0;    // original default: 0

    // --- Graphics (original defaults all combo indices to 0 = highest quality) ---
    float GammaRamp           = 1.0f;
    int   GraphicQuality      = 0;  // 0=User-Defined,1=Highest,2=High,3=Medium,4=Low,5=Lowest
    float FarClipLevel        = 1.0f;
    int   TextureLevel        = 0;  // 0=High,1=Medium,2=Low
    int   ShaderLevel         = 0;  // 0=Highest..4=Lowest
    int   ShadowLevel         = 0;  // 0=High,1=Medium,2=Low,3=Turn Off
    int   ShadowTestLevel     = 0;  // 0=Quality preferred,1=Speed preferred
    int   SelfShadowLevel     = 0;
    int   AlphaBlendLevel     = 0;  // 0=Advanced,1=Normal
    int   DynamicLightingLevel = 0; // 0=Highest..4=No
    int   OcclusionShadeLevel = 0;
    float GrassLevel          = 1.0f;
    int   LensFlareLevel      = 0;  // 0=Advanced,1=Normal,2=Turn Off
    int   WaterReflectLevel   = 0;  // 0=BG+All..4=No
    int   WaterRefractLevel   = 0;  // 0=BG+All..4=No
    int   PostEffectLevel     = 0;  // 0=Turn On,1=Turn Off
    int   FootstepMeshLevel   = 0;  // 0=All Characters,1=My Character,2=Turn Off
    bool  Decal               = false; // original default: 0

    // --- Sound ---
    bool  VolumeMute        = false;
    bool  HWMixing          = false;
    bool  SoundEnable       = false; // registry key "Eax" (original default: 0)
    float BgmVolume         = 1.0f;
    float SfxVolume         = 1.0f;
    int   ListenerLevel     = 1;
    int   FootstepSoundLevel = 0;

    // --- Keybinding version ---
    int ShotcutKeyVersion = 0;

    // --- Launcher preferences (stored in PriusLauncher.gui alongside exe) ---
    int ThemeIndex = 0;    // 0=20100513_TW, 1=20101110_TW, 2=20100420_US
    int Language   = 0;    // 0=English
    std::wstring Profile = L"release";  // passed as -theme <profile> to Prius.exe

    void Load();
    void Save() const;
    void SaveLauncherPrefs() const;
};

