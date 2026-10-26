// xui_launch.cpp: see xui_launch.h.
//
// Windows: the game is started with CreateProcessW (or ShellExecuteExW for
// non-.exe targets such as .bat / .lnk) and placed in a Job Object, so child
// processes spawned by mod launchers (Chimera, SPV3 and friends) are tracked
// too. A watcher thread waits until the job has no live processes (and, if
// configured, until a named "watch process" has gone), then the main thread
// restores and foregrounds the launcher window.
//
// No display / resolution / window flags are ever added: the game runs with
// its own native settings.

#include "xui_launch.h"
#include "xui_games.h"

#include <SDL.h>
#include <SDL_syswm.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#else
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern SDL_Window* g_pSDLWindow;

// launch.cpp: dashboard-style fade-to-black overlay, fires `fn` at its hold point.
void LaunchOverlay_ArmCallback(void (*fn)());

namespace {

std::atomic<bool> s_running{false};
std::atomic<bool> s_exitedPending{false};
std::atomic<unsigned> s_launchSerial{0};
int               s_pendingGame = -1;
char              s_status[512] = "";

void SetStatus(const char* fmt, const std::string& a)
{
	snprintf(s_status, sizeof(s_status), fmt, a.c_str());
	fprintf(stderr, "[xui-launch] %s\n", s_status);
}

std::string Lower(std::string s)
{
	for (auto& c : s) c = (char)tolower((unsigned char)c);
	return s;
}

bool EndsWith(const std::string& s, const char* suf)
{
	size_t n = strlen(suf);
	return s.size() >= n && Lower(s.substr(s.size() - n)) == suf;
}

#ifdef _WIN32

std::wstring Widen(const std::string& s)
{
	if (s.empty()) return std::wstring();
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	std::wstring w(n ? n - 1 : 0, L'\0');
	if (n) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
	return w;
}

std::string Native(std::string p)
{
	for (auto& c : p) if (c == '/') c = '\\';
	return p;
}

bool ProcessNameRunning(const std::string& name)
{
	if (name.empty()) return false;
	std::wstring want = Widen(Lower(name));
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return false;
	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(pe);
	bool found = false;
	if (Process32FirstW(snap, &pe)) {
		do {
			std::wstring exe = pe.szExeFile;
			for (auto& c : exe) c = (wchar_t)towlower(c);
			if (exe == want) { found = true; break; }
		} while (Process32NextW(snap, &pe));
	}
	CloseHandle(snap);
	return found;
}

DWORD JobActiveProcesses(HANDLE job)
{
	JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {};
	if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr))
		return 0;
	return info.ActiveProcesses;
}

void WatchThread(HANDLE job, HANDLE proc, std::string watchName, unsigned serial)
{
	const DWORD start = GetTickCount();
	// Phase 1: the job (game + its children).
	for (;;) {
		bool alive = false;
		if (job)  alive = JobActiveProcesses(job) > 0;
		else if (proc) alive = WaitForSingleObject(proc, 0) == WAIT_TIMEOUT;
		if (!alive) break;
		Sleep(500);
	}
	// Phase 2: optional named process (launchers that hand off and exit).
	if (!watchName.empty()) {
		// Give a hand-off launcher time to start the real game.
		while (!ProcessNameRunning(watchName) && GetTickCount() - start < 20000) Sleep(500);
		while (ProcessNameRunning(watchName)) Sleep(1000);
	}
	if (job)  CloseHandle(job);
	if (proc) CloseHandle(proc);
	if (serial == s_launchSerial.load()) {
		s_running = false;
		s_exitedPending = true;
	}
}

bool Spawn(const XuiGame& g)
{
	std::string exe = Native(g.exe);
	std::string dir = Native(g.workdir.empty() ? XuiGames_DirOf(g.exe) : g.workdir);

	HANDLE job = CreateJobObjectW(nullptr, nullptr);   // no kill-on-close: games outlive us
	HANDLE proc = nullptr;

	if (EndsWith(exe, ".exe")) {
		std::wstring cmd = L"\"" + Widen(exe) + L"\"";
		if (!g.args.empty()) cmd += L" " + Widen(g.args);
		std::wstring wdir = Widen(dir);
		STARTUPINFOW si = {};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi = {};
		std::wstring wexe = Widen(exe);
		if (!CreateProcessW(wexe.c_str(), &cmd[0], nullptr, nullptr, FALSE,
		                    CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP, nullptr,
		                    wdir.empty() ? nullptr : wdir.c_str(), &si, &pi)) {
			char err[64];
			snprintf(err, sizeof(err), "%lu", GetLastError());
			SetStatus("Could not start game (Windows error %s)", err);
			if (job) CloseHandle(job);
			return false;
		}
		if (job && !AssignProcessToJobObject(job, pi.hProcess)) {
			CloseHandle(job);   // e.g. we're inside a job that forbids nesting
			job = nullptr;
		}
		ResumeThread(pi.hThread);
		CloseHandle(pi.hThread);
		proc = pi.hProcess;
	} else {
		// .bat / .cmd / .lnk / .url / anything with a shell association.
		std::wstring wexe = Widen(exe), wargs = Widen(g.args), wdir = Widen(dir);
		SHELLEXECUTEINFOW sei = {};
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
		sei.lpVerb = L"open";
		sei.lpFile = wexe.c_str();
		sei.lpParameters = wargs.empty() ? nullptr : wargs.c_str();
		sei.lpDirectory = wdir.empty() ? nullptr : wdir.c_str();
		sei.nShow = SW_SHOWNORMAL;
		if (!ShellExecuteExW(&sei)) {
			char err[64];
			snprintf(err, sizeof(err), "%lu", GetLastError());
			SetStatus("Could not open game (Windows error %s)", err);
			if (job) CloseHandle(job);
			return false;
		}
		proc = sei.hProcess;   // may be null (e.g. URL handlers)
		if (job && proc && !AssignProcessToJobObject(job, proc)) { CloseHandle(job); job = nullptr; }
		if (job && !proc) { CloseHandle(job); job = nullptr; }
	}

	if (!job && !proc && g.watchProcess.empty()) {
		// Nothing to wait on: treat as fire-and-forget.
		SetStatus("Launched (not tracked): %s", g.exe);
		return true;
	}

	unsigned serial = ++s_launchSerial;
	s_running = true;
	std::thread(WatchThread, job, proc, g.watchProcess, serial).detach();
	SetStatus("Launched: %s", g.exe);
	return true;
}

void ForceForeground()
{
	if (!g_pSDLWindow) return;
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (!SDL_GetWindowWMInfo(g_pSDLWindow, &wmi)) return;
	HWND hwnd = wmi.info.win.window;

	if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
	else                ShowWindow(hwnd, SW_SHOW);

	// Windows blocks focus stealing; borrow the foreground thread's input
	// state for a moment so SetForegroundWindow is honoured.
	HWND fg = GetForegroundWindow();
	DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
	DWORD me = GetCurrentThreadId();
	bool attached = fgThread && fgThread != me && AttachThreadInput(fgThread, me, TRUE);

	// A synthetic Alt tap also unlocks the foreground lock on most builds.
	keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY, 0);
	keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

	SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	BringWindowToTop(hwnd);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);
	if (attached) AttachThreadInput(fgThread, me, FALSE);
}

#else // POSIX

bool ProcessNameRunning(const std::string& name)
{
	if (name.empty()) return false;
	std::string cmd = "pgrep -x '" + name + "' >/dev/null 2>&1";
	return system(cmd.c_str()) == 0;
}

void WatchThread(pid_t pid, std::string watchName, unsigned serial)
{
	int status = 0;
	waitpid(pid, &status, 0);
	// Children in the game's process group (setpgid below).
	while (kill(-pid, 0) == 0) sleep(1);
	if (!watchName.empty()) {
		for (int i = 0; i < 20 && !ProcessNameRunning(watchName); i++) sleep(1);
		while (ProcessNameRunning(watchName)) sleep(1);
	}
	if (serial == s_launchSerial.load()) {
		s_running = false;
		s_exitedPending = true;
	}
}

bool Spawn(const XuiGame& g)
{
	std::string dir = g.workdir.empty() ? XuiGames_DirOf(g.exe) : g.workdir;
	std::string cmd = "exec \"" + g.exe + "\"";
	if (!g.args.empty()) cmd += " " + g.args;
	pid_t pid = fork();
	if (pid == 0) {
		setpgid(0, 0);
		if (!dir.empty()) (void)chdir(dir.c_str());
		execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
		_exit(127);
	}
	if (pid < 0) { SetStatus("fork failed: %s", g.exe); return false; }
	unsigned serial = ++s_launchSerial;
	s_running = true;
	std::thread(WatchThread, pid, g.watchProcess, serial).detach();
	SetStatus("Launched: %s", g.exe);
	return true;
}

void ForceForeground()
{
	if (!g_pSDLWindow) return;
	SDL_RestoreWindow(g_pSDLWindow);
	SDL_ShowWindow(g_pSDLWindow);
	SDL_RaiseWindow(g_pSDLWindow);
}

#endif

bool FileExists(const std::string& p)
{
#ifdef _WIN32
	DWORD a = GetFileAttributesW(Widen(p).c_str());
	return a != INVALID_FILE_ATTRIBUTES;
#else
	struct stat st;
	return stat(p.c_str(), &st) == 0;
#endif
}

void OverlayFired()
{
	const XuiGame* g = XuiGames_Get(s_pendingGame);
	s_pendingGame = -1;
	if (!g) return;
	if (Spawn(*g) && g_pSDLWindow)
		SDL_MinimizeWindow(g_pSDLWindow);   // focus loss mutes the dashboard audio
}

} // namespace

bool XuiLaunch_Start(int gameIdx)
{
	const XuiGame* g = XuiGames_Get(gameIdx);
	if (!g) return false;
	if (!FileExists(g->exe)) {
		SetStatus("Program not found: %s", g->exe);
		return false;
	}
	s_pendingGame = gameIdx;
	LaunchOverlay_ArmCallback(OverlayFired);
	return true;
}

bool XuiLaunch_IsRunning() { return s_running; }

void XuiLaunch_Tick()
{
	if (!s_exitedPending.exchange(false)) return;
	fprintf(stderr, "[xui-launch] game exited; restoring launcher\n");
	ForceForeground();
}

const char* XuiLaunch_LastStatus() { return s_status; }
