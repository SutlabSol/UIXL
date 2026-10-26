// xui_launch.h: launch a game from the list, keep the launcher open
// (minimised + muted) while it runs, and bring the launcher back to the front
// when the game exits.
#pragma once

// Starts the dashboard's launch fade, then spawns the game. Returns false if
// the index is invalid or the executable is missing.
bool XuiLaunch_Start(int gameIdx);

// True while a launched game (or any process it spawned) is still running.
bool XuiLaunch_IsRunning();

// Call once per frame on the main thread. Restores + raises the launcher
// window when the tracked game has exited.
void XuiLaunch_Tick();

// Last human readable launch status (for error toasts / logs).
const char* XuiLaunch_LastStatus();
