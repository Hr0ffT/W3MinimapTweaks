// W3MinimapTweaks.mix - minimap tweaks for Warcraft III 1.26a and 1.27b (Game.dll builds 6401 and 7085)
// Copyright (c) 2026 Hr0ffT - MIT License - https://github.com/Hr0ffT
//
// - Ally Color Mode (Alt+A) usable in campaign missions.
// - Smaller unit dots on the minimap, so groups of units do not melt into one blob on big screens.
// - Own minimap colours for Ally Color Mode (you, allies, enemies, creeps).
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
#include <stdlib.h>

typedef uint32_t u32;
static u32 g_base;
static FILE* g_log;
static int g_debug;

// per Game.dll build: the "push 0" (6A 00) argument of the disable call, and the function it is passed to
typedef struct {
    u32 build;
    u32 allyPush;              // push 0 right before "call <enable button>" for the ally color button
    u32 enableButton;          // thiscall(button, enable)
    u32 dotAt, dotDone;        // unit dots: the code that fills a dot into the minimap bitmap, and where it ends
    const char* dotCode;       // the 6 bytes expected at dotAt (load of the bitmap pointer)
} Addrs;
static const Addrs kAddrs[] = {
    { 6401, 0x39F0E4, 0x602FE0, 0x361A8C, 0x361C89, "\x8b\x97\xd8\x01\x00\x00" },   // 1.26a
    { 7085, 0x1E99F6, 0x10F240, 0x3DB57A, 0x3DB6CD, "\x8b\x83\xd8\x01\x00\x00" },   // 1.27b
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

// ---- Ally Color Mode colours on the minimap. The minimap takes them from the game data ([FogOfWar] FogColorPlayer,
//      FogColorAlly, FogColorEnemy, FogColorCreepAllied / FogColorCreepNormal: you white, allies teal, enemies red)
//      into its own fields when it is created, and builds its per-player colours from them at each update. They are
//      set from the ini when the minimap draws its unit dots (so from its second update on). The units themselves
//      keep their team colours. ----
static const struct { const char* name; u32 rgb; } kNamedColors[] = {
    { "white", 0xFFFFFF }, { "red", 0xFF0000 }, { "green", 0x00FF00 }, { "blue", 0x0040FF }, { "yellow", 0xFFFF00 },
    { "orange", 0xFF8000 }, { "teal", 0x00FFD2 }, { "cyan", 0x00FFFF }, { "purple", 0xA000FF }, { "pink", 0xFF60C0 },
    { "magenta", 0xFF00FF }, { "black", 0x000000 }, { "gray", 0x808080 }, { "grey", 0x808080 } };
static const char* const kColorKeys[4] = { "MinimapYouColor", "MinimapAllyColor", "MinimapEnemyColor", "MinimapCreepColor" };
static u32 g_mmColor[4]; static int g_mmSet[4], g_mmAny;
static int ParseRGB(const char* v, u32* out)
{
    while (*v == ' ') v++;
    if (!*v) return 0;
    int r, g, b;
    if (sscanf(v, "%d , %d , %d", &r, &g, &b) == 3 && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
        *out = 0xFF000000u | (u32)r << 16 | (u32)g << 8 | (u32)b; return 1;
    }
    for (size_t i = 0; i < sizeof kNamedColors / sizeof kNamedColors[0]; i++)
        if (!_stricmp(v, kNamedColors[i].name)) { *out = 0xFF000000u | kNamedColors[i].rgb; return 1; }
    return -1;
}
static void ReadColors(const char* ini)
{
    for (int i = 0; i < 4; i++) {
        char v[64]; GetPrivateProfileStringA("W3MinimapTweaks", kColorKeys[i], "", v, sizeof v, ini);
        int r = ParseRGB(v, &g_mmColor[i]);
        if (r < 0) logf_("%s=%s: not a colour (R,G,B or a name), the game's is kept", kColorKeys[i], v);
        if (r > 0) { g_mmSet[i] = 1; g_mmAny = 1; logf_("%s: %06X", kColorKeys[i], g_mmColor[i] & 0xFFFFFF); }
    }
}
static void ApplyColors(u32 minimap)
{
    static const u32 ofs[4][2] = { { 0x7BC, 0 }, { 0x7C0, 0 }, { 0x7C4, 0 }, { 0x7D8, 0x7DC } };
    for (int i = 0; i < 4; i++) if (g_mmSet[i])
        for (int k = 0; k < 2 && ofs[i][k]; k++) *(u32*)(minimap + ofs[i][k]) = g_mmColor[i];
}

// ---- unit dots. The minimap is a 256x256 bitmap (terrain, then the units drawn into it) stretched over the minimap
//      area; every unit is a square of 4x4 texels, a building 8x8. On a big screen one texel is several pixels, so
//      groups of units melt into one blob. The game's fill of that square is replaced: same centre, own sizes. ----
static int g_dotUnit = 4, g_dotBuilding = 8;
u32 g_dotDone;                       // (used from the asm below)
__attribute__((used, fastcall)) void DrawDot(u32 minimap, u32 rowcol, u32 color, u32 big)
{
    int row = rowcol >> 16, col = rowcol & 0xFFFF;
    int orig = big ? 8 : 4, n = big ? g_dotBuilding : g_dotUnit;
    int cy = row + orig / 2, cx = col + orig / 2;     // the game clamps the square to the bitmap: keep its centre
    int y0 = cy - n / 2, x0 = cx - n / 2;
    if (y0 < 0) y0 = 0;
    if (y0 > 256 - n) y0 = 256 - n;
    if (x0 < 0) x0 = 0;
    if (x0 > 256 - n) x0 = 256 - n;
    if (g_mmAny) ApplyColors(minimap);
    u32* px = *(u32**)(minimap + 0x1d8);
    if (!px) return;
    for (int y = 0; y < n; y++) {
        u32* r = px + (y0 + y) * 256 + x0;
        for (int x = 0; x < n; x++) r[x] = color;
    }
}
// 1.26: minimap in edi, row in eax, column in ebx, colour in esi, building flag at [esp+2Ch]
static void __attribute__((naked)) Dot126(void)
{
    asm volatile(
        "mov 0x2c(%%esp), %%ecx\n"
        "pushal\n"
        "shl $16, %%eax\n" "or %%ebx, %%eax\n"
        "push %%ecx\n" "push %%esi\n"
        "mov %%edi, %%ecx\n" "mov %%eax, %%edx\n"
        "call @DrawDot@16\n"
        "popal\n"
        "jmp *_g_dotDone\n" ::: "memory");
}
// 1.27: minimap in ebx, row in ecx, column in edx, colour in esi, building flag at [ebp-34h]
static void __attribute__((naked)) Dot127(void)
{
    asm volatile(
        "pushal\n"
        "shl $16, %%ecx\n" "or %%ecx, %%edx\n"
        "push -0x34(%%ebp)\n" "push %%esi\n"
        "mov %%ebx, %%ecx\n"
        "call @DrawDot@16\n"
        "popal\n"
        "jmp *_g_dotDone\n" ::: "memory");
}
static void InstallDots(void)
{
    if (g_dotUnit == 4 && g_dotBuilding == 8 && !g_mmAny) { logf_("unit dots: the game's own sizes"); return; }
    u32 at = g_base + A->dotAt;
    if (memcmp((void*)at, A->dotCode, 6)) { logf_("unit dots: code not recognised, left as is"); return; }
    g_dotDone = g_base + A->dotDone;
    uint8_t j[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
    u32 to = A->build == 6401 ? (u32)Dot126 : (u32)Dot127;
    *(int32_t*)(j + 1) = (int32_t)(to - (at + 5));
    WriteMem(at, j, 6);
    logf_("unit dots: %d (units), %d (buildings) texels of 256", g_dotUnit, g_dotBuilding);
}

// screen height: the game's video settings, else the primary monitor
static int ScreenHeight(void)
{
    HKEY k; DWORD h = 0, sz = 4;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Blizzard Entertainment\\Warcraft III\\Video", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueExA(k, "resheight", NULL, NULL, (LPBYTE)&h, &sz);
        RegCloseKey(k);
    }
    return h > 0 ? (int)h : GetSystemMetrics(SM_CYSCREEN);
}

static void Install(void)
{
    char dir[MAX_PATH], path[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* sl = strrchr(dir, '\\'); if (sl) *(sl + 1) = 0;
    snprintf(path, sizeof path, "%sW3MinimapTweaks.ini", dir);
    char path0[MAX_PATH]; strcpy(path0, path);
    int ally = GetPrivateProfileIntA("W3MinimapTweaks", "CampaignAllyColors", 1, path);
    g_dotUnit = GetPrivateProfileIntA("W3MinimapTweaks", "UnitDotSize", 0, path);
    g_dotBuilding = GetPrivateProfileIntA("W3MinimapTweaks", "BuildingDotSize", 0, path);
    int h = ScreenHeight();
    // 0 = auto: the dots keep the size in screen pixels they have at 1080p (where a texel is about a pixel)
    if (h <= 0) h = 1080;
    int autoUnit = (4 * 1080 + h / 2) / h;
    if (autoUnit < 1) autoUnit = 1;
    if (autoUnit > 4) autoUnit = 4;
    if (g_dotUnit <= 0) g_dotUnit = autoUnit;
    if (g_dotBuilding <= 0) g_dotBuilding = 2 * autoUnit;
    if (g_dotUnit > 16) g_dotUnit = 16;
    if (g_dotBuilding > 16) g_dotBuilding = 16;
    g_debug = GetPrivateProfileIntA("W3MinimapTweaks", "Debug", 0, path);
    if (g_debug) { snprintf(path, sizeof path, "%sW3MinimapTweaks.log", dir); g_log = fopen(path, "w"); }

    g_base = (u32)GetModuleHandleA("Game.dll");
    u32 build = GetGameBuild();
    logf_("W3MinimapTweaks 1.2  Game.dll build %u", build);
    for (size_t i = 0; i < sizeof kAddrs / sizeof kAddrs[0]; i++) if (kAddrs[i].build == build) A = &kAddrs[i];
    if (!g_base || !A) { logf_("unsupported game version, doing nothing (need 1.26a / 6401 or 1.27b / 7085)"); return; }
    if (ally) KeepEnabled(A->allyPush, "ally color button");
    ReadColors(path0);
    InstallDots();
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
