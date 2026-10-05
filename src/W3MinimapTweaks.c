// W3MinimapTweaks.mix - minimap tweaks for Warcraft III 1.26a and 1.27b (Game.dll builds 6401 and 7085)
// Copyright (c) 2026 Hr0ffT - MIT License - https://github.com/Hr0ffT
// Build: i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -s -o W3MinimapTweaks.mix W3MinimapTweaks.c -lversion
//
// In campaign missions the game greys out the Ally Color Mode button next to the minimap (Alt+A: own units blue,
// allies teal, enemies red); it works in every other game. The engine does it when it starts a campaign mission: it
// marks the game as a campaign and disables the button, a call of the button's enable function with 0. Here that
// argument becomes 1, so the button stays usable. (The creep camp button next to it is disabled the same way, but it
// is left alone: the game builds no creep camp marks in campaign missions, so it would show nothing.)
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
static u32 g_base;
static FILE* g_log;
static int g_debug;

// per Game.dll build: the "push 0" (6A 00) argument of the disable call, and the function it is passed to
typedef struct {
    u32 build;
    u32 allyPush;              // push 0 right before "call <enable button>" for the ally color button
    u32 enableButton;          // thiscall(button, enable)
} Addrs;
static const Addrs kAddrs[] = {
    { 6401, 0x39F0E4, 0x602FE0 },   // 1.26a
    { 7085, 0x1E99F6, 0x10F240 },   // 1.27b
};
static const Addrs* A;

static void logf_(const char* f, ...)
{
    if (!g_log) return;
    va_list a; va_start(a, f); vfprintf(g_log, f, a); va_end(a);
    fputc('\n', g_log); fflush(g_log);
}

static int WriteMem(u32 addr, const void* p, size_t n)
{
    DWORD old;
    if (!VirtualProtect((void*)addr, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy((void*)addr, p, n);
    VirtualProtect((void*)addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)addr, n);
    return 1;
}

static u32 GetGameBuild(void)
{
    DWORD h; DWORD sz = GetFileVersionInfoSizeA("Game.dll", &h);
    if (!sz) return 0;
    char* buf = (char*)malloc(sz); u32 build = 0;
    if (GetFileVersionInfoA("Game.dll", h, sz, buf)) {
        VS_FIXEDFILEINFO* vi; UINT l;
        if (VerQueryValueA(buf, "\\", (LPVOID*)&vi, &l)) build = vi->dwFileVersionLS & 0xFFFF;
    }
    free(buf);
    return build;
}

// keep a button enabled: "push 0 ; ... ; call enableButton" -> "push 1"
static void KeepEnabled(u32 push, const char* what)
{
    uint8_t* p = (uint8_t*)(g_base + push);
    // the call to the enable function follows the push
    int ok = 0;
    for (int k = 2; k <= 10 && !ok; k++)
        if (p[k] == 0xE8 && g_base + push + k + 5 + *(int32_t*)(p + k + 1) == g_base + A->enableButton) ok = 1;
    if (p[0] != 0x6A || p[1] != 0x00 || !ok) { logf_("%s: code not recognised, left as is", what); return; }
    static const uint8_t one = 0x01;
    WriteMem(g_base + push + 1, &one, 1);
    logf_("%s: stays enabled in campaign missions", what);
}

static void Install(void)
{
    char dir[MAX_PATH], path[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* sl = strrchr(dir, '\\'); if (sl) *(sl + 1) = 0;
    snprintf(path, sizeof path, "%sW3MinimapTweaks.ini", dir);
    int ally = GetPrivateProfileIntA("W3MinimapTweaks", "CampaignAllyColors", 1, path);
    g_debug = GetPrivateProfileIntA("W3MinimapTweaks", "Debug", 0, path);
    if (g_debug) { snprintf(path, sizeof path, "%sW3MinimapTweaks.log", dir); g_log = fopen(path, "w"); }

    g_base = (u32)GetModuleHandleA("Game.dll");
    u32 build = GetGameBuild();
    logf_("W3MinimapTweaks 1.0  Game.dll build %u", build);
    for (size_t i = 0; i < sizeof kAddrs / sizeof kAddrs[0]; i++) if (kAddrs[i].build == build) A = &kAddrs[i];
    if (!g_base || !A) { logf_("unsupported game version, doing nothing (need 1.26a / 6401 or 1.27b / 7085)"); return; }
    if (ally) KeepEnabled(A->allyPush, "ally color button");
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        // the Miles sound library loads the .mix files and may unload / reload them (sound provider change): stay loaded
        { HMODULE self; GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)DllMain, &self); }
        Install();
    }
    return TRUE;
}
