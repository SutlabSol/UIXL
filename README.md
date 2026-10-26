# XUI Launcher

**XUI Launcher** is a desktop executable/game launcher styled after the original 2001 Xbox dashboard (`MainMenu5.xip` / `Settings3.xip`), built as a stripped-down fork of [Theseus](https://github.com/MrMilenko/Theseus) (SDL2 + bgfx + Xbox XAP scene engine).

## Features

- **Authentic 2001 Xbox 3D Main Menu**:
  - Opens directly to the main menu in **Windowed** mode by default.
  - Menu order is always `[Game 1 ... Game N]`, **`ADD GAMES...`** (second-from-bottom), and **`SETTINGS`** (bottom).
  - On first launch (0 games), only **`ADD GAMES...`** and **`SETTINGS`** are displayed.
  - Up to **5 bars** are visible on the curved ring at once; when more than 5 total options exist, moving past the bottom or top bar smoothly scrolls the 5-slot window through the full list.
- **Hover Orb Game Image with Translucent Rim Framing**:
  - Hovering over a game replaces the inner Xbox `X` graphic inside the large left orb (`Main_pod_backing01`) with that game's UI image while preserving the translucent outer orb rim (`Main_pod_rim` + `Main_memory_podshell_09..11`) as a circular frame.
  - Hovering over **`ADD GAMES...`** or **`SETTINGS`** (or a game without a custom image) restores the glowing Xbox `X`.
- **Add Games Flow**:
  - Select **`ADD GAMES...`** to pick any `.exe` / executable via the native file dialog, enter its display name using the authentic 3D Xbox on-screen keyboard, optionally pick a UI image, preview it inside the main orb, and confirm.
- **Native Game Launching & Auto-Restore**:
  - Games launch with their **own native settings** and working directory (no resolution or window flags are injected).
  - While a game is running, XUI Launcher stays open (minimized and muted), monitors the game and any child processes via a Windows Job Object, and automatically restores its window to the front when the game exits.
- **Authentic 3D Xbox Settings Menu (`SETTINGS`)**:
  - **Display Mode**: Switch the launcher window between **Windowed** (default), **Fullscreen**, and **Borderless** (`F11` also toggles fullscreen).
  - **UI Colour**: Live hot-swap between **7 colour presets** (**Green**, **Blue**, **Red**, **Orange**, **Purple**, **White**, **Yellow**).
  - **Manage Games**: Edit existing games (**Change UI Image**, **Remove UI Image**, **Rename Game**, **Change Program**, **Move Up**, **Move Down**, **Remove Game**).
  - **Audio & Boot**: Adjust **Volume** (`0%`–`100%` in `10%` steps, `50%` default) and toggle **Menu Sounds** (`ON`/`OFF`), **Background Hum** (`ON`/`OFF`), and **Boot Animation** (`PLAY`/`SKIP`).

## Controls

| Action | Keyboard | Xbox / SDL Controller |
| :--- | :--- | :--- |
| Move Up / Down | `Up` / `Down` Arrow or `W` / `S` | D-Pad Up / Down or Left Stick |
| Select (`A`) | `Enter` or `Space` | `A` Button |
| Back (`B`) | `Backspace`, `B`, or `Esc` | `B` Button |
| Toggle Fullscreen | `F11` | via `SETTINGS` -> `DISPLAY MODE` |

## Running & Building

- **Executable**: `out\XUI Launcher.exe`
- **Build from source** (requires MSYS2 MinGW64):
  ```powershell
  .\build\build.ps1 -j12
  ```
- **User configuration & game list**:
  - Stored in `%APPDATA%\XUI Launcher\Configs\games.json` and `%APPDATA%\XUI Launcher\Configs\desktop.ini` (with cropped square 256x256 `.bmp` orb icons cached in `%APPDATA%\XUI Launcher\Configs\game_icons\`).

## License

GPL-3.0-or-later (see [LICENSE](LICENSE)). Based on [Theseus](https://github.com/MrMilenko/Theseus).
