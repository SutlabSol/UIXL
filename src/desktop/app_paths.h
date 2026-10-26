// Per OS user data dir. Everything customizable lives there; only the
// compiled shaders stay next to the binary.
//   macOS    ~/Library/Application Support/Theseus
//   Linux    $XDG_DATA_HOME/theseus, else ~/.local/share/theseus
//   Windows  %APPDATA%\Theseus

#pragma once

#include <stddef.h>

const char* Plat_ExeDir();
const char* Plat_UserDataDir();
const char* Plat_ShippedDir();

// Creates the user tree, seeding it from a side by side install on first run.
void AppPaths_Init();

// Turns "Configs/foo.ini" into a real absolute path, matched case
// insensitively. Rotating buffer, so a couple of calls can be live at once.
const char* AppPath(const char* logical);

// Same, but builds the logical path first.
const char* AppPathf(const char* fmt, ...);

// Absolute root of one tree. Stable buffer, safe to keep as a prefix.
const char* AppPath_Tree(const char* tree);

void Plat_OpenInFileManager(const char* dir);
