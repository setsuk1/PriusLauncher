#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dbghelp.h>
#include <shlwapi.h>
#include <cstdio>
#include "GameSettings.h"
#include "MainDlg.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shlwapi.lib")

// Mutex name to prevent multiple launcher instances.
// Change this to match your server project (e.g. L"PriusRE Launcher").
#ifndef LAUNCHER_MUTEX_NAME
#define LAUNCHER_MUTEX_NAME  L"PriusRE Online Launcher"
#endif

// ---- Crash dump handler -----------------------------------------------------

static LONG WINAPI CrashDumpHandler(EXCEPTION_POINTERS* ep)
{
    // Build dump path: <exe_dir>\PriusLauncher_<timestamp>.dmp
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t name[64];
    swprintf_s(name, L"\\PriusLauncher_%04d%02d%02d_%02d%02d%02d.dmp",
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    wcscat_s(path, name);

    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers    = FALSE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hFile, MiniDumpWithDataSegs, &mei, nullptr, nullptr);
        CloseHandle(hFile);

        wchar_t msg[MAX_PATH + 64];
        swprintf_s(msg, L"Crash dump saved to:\n%s", path);
        MessageBoxW(nullptr, msg, L"PriusLauncher Crash", MB_ICONERROR | MB_OK);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

// ---- Entry point ------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    SetUnhandledExceptionFilter(CrashDumpHandler);

    // Prevent multiple launcher instances (mirrors original CreateMutexA logic)
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, LAUNCHER_MUTEX_NAME);
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hMutex);
        return 0;
    }

    // Initialize common controls (for TrackBar, TabControl, etc.)
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_TAB_CLASSES };
    InitCommonControlsEx(&icc);

    // Load settings from registry
    GameSettings settings;
    settings.Load();

    // Show main launcher dialog
    ShowMainDialog(hInst, settings);

    if (hMutex) CloseHandle(hMutex);
    return 0;
}
