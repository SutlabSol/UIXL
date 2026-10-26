// xui_games.h: the launcher's game list (Configs/games.json in the user dir).
//
// List order is menu order. Each entry is a plain program launch: the game is
// started exactly as if double-clicked (its own folder as working directory,
// no display/resolution flags added), so it always uses its native settings.
#pragma once

#include <string>
#include <vector>

struct XuiGame
{
	std::string name;          // menu label (shown uppercased by the dashboard font)
	std::string exe;           // absolute path to the program
	std::string args;          // optional extra arguments (blank = none)
	std::string workdir;       // working folder; blank = the exe's folder
	std::string image;         // UI image, relative to the user dir (Library/Images/...)
	std::string watchProcess;  // optional process name to wait for (launcher-style games)
};

void   XuiGames_Load();
bool   XuiGames_Save();

int             XuiGames_Count();
const XuiGame*  XuiGames_Get(int idx);

// Adds a game; `imageSrc` (any path, may be blank) is copied into the user's
// Library/Images folder. Returns the new index, or -1 on failure.
int    XuiGames_Add(const XuiGame& g, const std::string& imageSrc);
bool   XuiGames_Update(int idx, const XuiGame& g);
bool   XuiGames_SetImage(int idx, const std::string& imageSrc);
bool   XuiGames_Remove(int idx);
bool   XuiGames_Move(int idx, int delta);   // -1 = up, +1 = down

// Absolute path of a game's UI image ("" when it has none / file missing).
std::string XuiGames_ImagePath(int idx);

// Default display name for an executable path ("C:/Games/Halo/haloce.exe" -> "haloce").
std::string XuiGames_NameFromExe(const std::string& exe);

// Folder part of a path ("C:/a/b.exe" -> "C:/a").
std::string XuiGames_DirOf(const std::string& path);

// Bumps every time the list changes (UI caches can compare against it).
unsigned XuiGames_Generation();
