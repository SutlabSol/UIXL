// xui_devtest.h: scripted input + screenshot harness for automated testing.
//
//   "XUI Launcher.exe" --script "wait 4; key down; wait 1; shot a.png; quit"
//
// Commands (separated by ';'):
//   wait <seconds>      pause the script
//   key <name> [ms]     hold a key (up/down/left/right/return/escape/backspace/
//                       space/f11/...; any SDL key name) for ms (default 120)
//   shot <file.png>     save the next rendered frame as PNG
//   quit                exit the app
#pragma once
#include <SDL.h>

namespace bgfx { struct CallbackI; }

void              XuiDev_ParseArgs(int argc, char** argv);
bool              XuiDev_Active();
bgfx::CallbackI*  XuiDev_BgfxCallback();
void              XuiDev_Tick();            // once per frame, before bgfx::frame()
bool              XuiDev_QuitRequested();

// Keyboard state merged with scripted (virtual) key presses. Use in place of
// SDL_GetKeyboardState(NULL) wherever the dashboard polls keys.
const Uint8*      XuiKeyboardState();
