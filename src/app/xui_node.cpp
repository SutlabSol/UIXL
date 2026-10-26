// xui_node.cpp: "XuiLauncher" XAP node. The bridge between the dashboard
// scene scripts (main menu, Add Games flow, Settings) and the launcher's C++
// side (game list, launching, native file pickers, preferences).
//
// Declared once in default.xap as:   DEF theXui XuiLauncher { ... }
//
// Main menu model: [game 0 .. game N-1] + "ADD GAMES..." + "SETTINGS".
// ADD GAMES... is always second-last and SETTINGS always last.

#include "app_paths.h"
#include "std.h"
#include "dashapp.h"
#include "node.h"
#include "runner.h"

#include "xui_games.h"
#include "xui_launch.h"

#include <SDL.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <SDL_syswm.h>
#endif

// Preferences (persisted in Configs/desktop.ini by SaveDesktopSettings).
bool g_xuiMenuSounds   = true;
bool g_xuiAmbientSound = true;

extern SDL_Window* g_pSDLWindow;
extern bool  g_bShowBootAnimation;
extern int   g_windowMode;            // 0 windowed, 1 borderless fullscreen, 2 fullscreen
extern bool  g_displayChangeRequested;
extern char  g_currentSkin[64];
extern float g_masterVolume;
extern "C" void DashAudio_SetMasterVolume(float vol);
extern void  SaveDesktopSettings();
extern void  ApplyDisplayMode();
extern void  ReloadSkin();

namespace {

// The 7 UI colour presets: display name -> Data/Skins folder.
struct ColourPreset { const char* name; const char* skin; };
const ColourPreset kColours[] = {
	{ "Green",  "Stock"  },
	{ "Blue",   "Blue"   },
	{ "Red",    "Red"    },
	{ "Orange", "Orange" },
	{ "Purple", "Purple" },
	{ "White",  "White"  },
	{ "Yellow", "Yellow" },
};
const int kColourCount = (int)(sizeof(kColours) / sizeof(kColours[0]));

std::string Upper(std::string s)
{
	for (auto& c : s) c = (char)toupper((unsigned char)c);
	return s;
}

CStrObject* Str(const std::string& s) { return new CStrObject(s.c_str()); }

// ---------------------------------------------------------------------------
// Native file picker. Blocks the main loop while open (like any modal
// dialog). Exclusive fullscreen is dropped to windowed for the duration so
// the dialog is actually visible.
// ---------------------------------------------------------------------------
std::string PickFile(bool image, const std::string& startDir)
{
	bool wasExclusive = g_pSDLWindow &&
		(SDL_GetWindowFlags(g_pSDLWindow) & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
	if (wasExclusive) SDL_SetWindowFullscreen(g_pSDLWindow, 0);

	std::string result;
#ifdef _WIN32
	wchar_t file[4096] = L"";
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (g_pSDLWindow && SDL_GetWindowWMInfo(g_pSDLWindow, &wmi)) ofn.hwndOwner = wmi.info.win.window;
	ofn.lpstrFilter = image
		? L"Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All files\0*.*\0"
		: L"Programs (*.exe;*.bat;*.cmd;*.lnk)\0*.exe;*.bat;*.cmd;*.lnk\0All files\0*.*\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = (DWORD)(sizeof(file) / sizeof(file[0]));
	ofn.lpstrTitle = image ? L"Choose a UI image" : L"Choose the game's program";
	std::wstring wdir;
	if (!startDir.empty()) {
		int n = MultiByteToWideChar(CP_UTF8, 0, startDir.c_str(), -1, nullptr, 0);
		wdir.assign(n ? n - 1 : 0, L'\0');
		if (n) MultiByteToWideChar(CP_UTF8, 0, startDir.c_str(), -1, &wdir[0], n);
		ofn.lpstrInitialDir = wdir.c_str();
	}
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
	if (GetOpenFileNameW(&ofn)) {
		int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
		result.assign(n ? n - 1 : 0, '\0');
		if (n) WideCharToMultiByte(CP_UTF8, 0, file, -1, &result[0], n, nullptr, nullptr);
		for (auto& c : result) if (c == '\\') c = '/';
	}
#else
	// zenity (GNOME / Steam Deck), kdialog (KDE), osascript (macOS).
	const char* cmds[] = {
#ifdef __APPLE__
		image ? "osascript -e 'POSIX path of (choose file of type {\"public.image\"})' 2>/dev/null"
		      : "osascript -e 'POSIX path of (choose file)' 2>/dev/null",
#endif
		image ? "zenity --file-selection --title='Choose a UI image' --file-filter='*.png *.jpg *.jpeg *.bmp' 2>/dev/null"
		      : "zenity --file-selection --title='Choose the game program' 2>/dev/null",
		image ? "kdialog --getopenfilename ~ '*.png *.jpg *.jpeg *.bmp' 2>/dev/null"
		      : "kdialog --getopenfilename ~ 2>/dev/null",
	};
	for (const char* cmd : cmds) {
		FILE* p = popen(cmd, "r");
		if (!p) continue;
		char buf[4096] = "";
		bool got = fgets(buf, sizeof(buf), p) != nullptr;
		int rc = pclose(p);
		if (got && rc == 0) {
			result = buf;
			while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
			break;
		}
		if (rc == 0) break;   // tool ran, user cancelled
	}
#endif

	if (wasExclusive) SDL_SetWindowFullscreen(g_pSDLWindow, SDL_WINDOW_FULLSCREEN);
	if (g_pSDLWindow) SDL_RaiseWindow(g_pSDLWindow);
	return result;
}

std::string s_lastImageDir;
std::string s_lastExeDir;

} // namespace

class CXuiLauncher : public CNode
{
public:
	CXuiLauncher() {}
	DECLARE_NODE(CXuiLauncher, CNode)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()

	// ---- Main menu --------------------------------------------------------
	int GetGameCount() { return XuiGames_Count(); }
	int GetMenuCount() { return XuiGames_Count() + 2; }

	// 0 = game, 1 = ADD GAMES..., 2 = SETTINGS, -1 = out of range
	int GetMenuKind(int i)
	{
		int n = XuiGames_Count();
		if (i < 0 || i >= n + 2) return -1;
		if (i < n) return 0;
		return (i == n) ? 1 : 2;
	}

	CStrObject* GetMenuText(int i)
	{
		switch (GetMenuKind(i)) {
		case 0:  return Str(Upper(XuiGames_Get(i)->name));
		case 1:  return Str("ADD GAMES...");
		case 2:  return Str("SETTINGS");
		default: return new CStrObject;
		}
	}

	CStrObject* GetGameName(int i)
	{
		const XuiGame* g = XuiGames_Get(i);
		return g ? Str(g->name) : new CStrObject;
	}

	CStrObject* GetGameImage(int i) { return Str(XuiGames_ImagePath(i)); }

	int LaunchGame(int i) { return XuiLaunch_Start(i) ? 1 : 0; }
	int IsGameRunning()   { return XuiLaunch_IsRunning() ? 1 : 0; }
	CStrObject* GetLastStatus() { return Str(XuiLaunch_LastStatus()); }

	// ---- Add Games flow ---------------------------------------------------
	// BrowseProgram(): native picker; returns "" when cancelled. Starts a new
	// pending game and suggests its name (GetPendingName).
	CStrObject* BrowseProgram()
	{
		std::string p = PickFile(false, s_lastExeDir);
		if (!p.empty()) {
			s_lastExeDir = XuiGames_DirOf(p);
			m_pending = XuiGame();
			m_pendingImage.clear();
			m_pending.exe = p;
			m_pending.name = XuiGames_NameFromExe(p);
		}
		return Str(p);
	}

	CStrObject* BrowseImage()
	{
		std::string p = PickFile(true, s_lastImageDir);
		if (!p.empty()) s_lastImageDir = XuiGames_DirOf(p);
		return Str(p);
	}

	CStrObject* GetPendingName()      { return Str(m_pending.name); }
	CStrObject* GetPendingMenuText()  { return Str(Upper(m_pending.name)); }
	CStrObject* GetPendingImage()     { return Str(m_pendingImage); }
	void SetPendingName(const TCHAR* s)  { if (s && *s) m_pending.name = s; }
	void SetPendingImage(const TCHAR* s) { m_pendingImage = s ? s : ""; }

	// Returns the new game's menu index, or -1.
	int CommitPendingGame()
	{
		if (m_pending.exe.empty()) return -1;
		int idx = XuiGames_Add(m_pending, m_pendingImage);
		m_pending = XuiGame();
		m_pendingImage.clear();
		return idx;
	}
	void CancelPendingGame() { m_pending = XuiGame(); m_pendingImage.clear(); }

	// ---- Manage games -----------------------------------------------------
	int RemoveGame(int i)              { return XuiGames_Remove(i) ? 1 : 0; }
	int MoveGameUp(int i)              { return XuiGames_Move(i, -1) ? 1 : 0; }
	int MoveGameDown(int i)            { return XuiGames_Move(i, +1) ? 1 : 0; }
	void RenameGame(int i, const TCHAR* s)
	{
		const XuiGame* g = XuiGames_Get(i);
		if (!g || !s || !*s) return;
		XuiGame copy = *g;
		copy.name = s;
		XuiGames_Update(i, copy);
	}
	// Browse for a new image / program for game i. 1 = changed, 0 = cancelled.
	int ChangeGameImage(int i)
	{
		if (!XuiGames_Get(i)) return 0;
		std::string p = PickFile(true, s_lastImageDir);
		if (p.empty()) return 0;
		s_lastImageDir = XuiGames_DirOf(p);
		return XuiGames_SetImage(i, p) ? 1 : 0;
	}
	int RemoveGameImage(int i) { return XuiGames_SetImage(i, "") ? 1 : 0; }
	int ChangeGameProgram(int i)
	{
		const XuiGame* g = XuiGames_Get(i);
		if (!g) return 0;
		std::string p = PickFile(false, XuiGames_DirOf(g->exe));
		if (p.empty()) return 0;
		XuiGame copy = *g;
		copy.exe = p;
		copy.workdir.clear();   // follow the new program's folder
		return XuiGames_Update(i, copy) ? 1 : 0;
	}
	CStrObject* GetGameProgram(int i)
	{
		const XuiGame* g = XuiGames_Get(i);
		return g ? Str(g->exe) : new CStrObject;
	}

	// ---- Preferences ------------------------------------------------------
	// Display: 0 = Windowed, 1 = Fullscreen, 2 = Borderless (menu order).
	int GetDisplayMode()
	{
		return g_windowMode == 2 ? 1 : (g_windowMode == 1 ? 2 : 0);
	}
	void SetDisplayMode(int n)
	{
		int mode = (n == 1) ? 2 : (n == 2 ? 1 : 0);
		if (mode == g_windowMode) return;
		g_windowMode = mode;
		g_displayChangeRequested = true;
		SaveDesktopSettings();
	}
	CStrObject* GetDisplayModeName(int n)
	{
		static const char* k[] = { "Windowed", "Fullscreen", "Borderless" };
		return Str((n >= 0 && n < 3) ? k[n] : "");
	}

	int GetColourCount() { return kColourCount; }
	CStrObject* GetColourName(int i)
	{
		return Str((i >= 0 && i < kColourCount) ? kColours[i].name : "");
	}
	int GetColour()
	{
		for (int i = 0; i < kColourCount; i++)
			if (strcasecmp(g_currentSkin, kColours[i].skin) == 0) return i;
		return 0;
	}
	void SetColour(int i)
	{
		if (i < 0 || i >= kColourCount) return;
		snprintf(g_currentSkin, sizeof(g_currentSkin), "%s", kColours[i].skin);
		SaveDesktopSettings();
		ReloadSkin();   // live: textures, meshes and materials re-tinted
	}

	int  GetMenuSounds()        { return g_xuiMenuSounds ? 1 : 0; }
	void SetMenuSounds(int on)  { g_xuiMenuSounds = on != 0; SaveDesktopSettings(); }
	int  GetAmbientSound()      { return g_xuiAmbientSound ? 1 : 0; }
	void SetAmbientSound(int on){ g_xuiAmbientSound = on != 0; SaveDesktopSettings(); }
	int  GetBootAnimation()     { return g_bShowBootAnimation ? 1 : 0; }
	void SetBootAnimation(int on){ g_bShowBootAnimation = on != 0; SaveDesktopSettings(); }

	int GetVolume()
	{
		int pct = (int)(g_masterVolume * 100.0f + 0.5f);
		if (pct < 0) pct = 0;
		if (pct > 100) pct = 100;
		return pct;
	}
	void SetVolume(int pct)
	{
		if (pct < 0) pct = 0;
		if (pct > 100) pct = 100;
		DashAudio_SetMasterVolume((float)pct / 100.0f);
		SaveDesktopSettings();
	}

	void Quit()
	{
		SDL_Event e;
		memset(&e, 0, sizeof(e));
		e.type = SDL_QUIT;
		SDL_PushEvent(&e);
	}

private:
	XuiGame     m_pending;
	std::string m_pendingImage;
};

IMPLEMENT_NODE("XuiLauncher", CXuiLauncher, CNode)

START_NODE_PROPS(CXuiLauncher, CNode)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CXuiLauncher
START_NODE_FUN(CXuiLauncher, CNode)
	NODE_FUN_IV(GetGameCount)
	NODE_FUN_IV(GetMenuCount)
	NODE_FUN_II(GetMenuKind)
	NODE_FUN_SI(GetMenuText)
	NODE_FUN_SI(GetGameName)
	NODE_FUN_SI(GetGameImage)
	NODE_FUN_II(LaunchGame)
	NODE_FUN_IV(IsGameRunning)
	NODE_FUN_SV(GetLastStatus)
	NODE_FUN_SV(BrowseProgram)
	NODE_FUN_SV(BrowseImage)
	NODE_FUN_SV(GetPendingName)
	NODE_FUN_SV(GetPendingMenuText)
	NODE_FUN_SV(GetPendingImage)
	NODE_FUN_VS(SetPendingName)
	NODE_FUN_VS(SetPendingImage)
	NODE_FUN_IV(CommitPendingGame)
	NODE_FUN_VV(CancelPendingGame)
	NODE_FUN_II(RemoveGame)
	NODE_FUN_II(MoveGameUp)
	NODE_FUN_II(MoveGameDown)
	NODE_FUN_VIS(RenameGame)
	NODE_FUN_II(ChangeGameImage)
	NODE_FUN_II(RemoveGameImage)
	NODE_FUN_II(ChangeGameProgram)
	NODE_FUN_SI(GetGameProgram)
	NODE_FUN_IV(GetDisplayMode)
	NODE_FUN_VI(SetDisplayMode)
	NODE_FUN_SI(GetDisplayModeName)
	NODE_FUN_IV(GetColourCount)
	NODE_FUN_SI(GetColourName)
	NODE_FUN_IV(GetColour)
	NODE_FUN_VI(SetColour)
	NODE_FUN_IV(GetMenuSounds)
	NODE_FUN_VI(SetMenuSounds)
	NODE_FUN_IV(GetAmbientSound)
	NODE_FUN_VI(SetAmbientSound)
	NODE_FUN_IV(GetBootAnimation)
	NODE_FUN_VI(SetBootAnimation)
	NODE_FUN_IV(GetVolume)
	NODE_FUN_VI(SetVolume)
	NODE_FUN_VV(Quit)
END_NODE_FUN()
