// removed_features.cpp: XUI Launcher no-op stand-ins for Theseus subsystems
// that were cut from this fork (media player / libmpv, CD audio, Plex,
// Jellyfin, xCloud, projectM, ES-DE / Steam / RetroArch launchers, Xbox HDD
// image browser, playlists, and the ImGui developer tools).
//
// The dashboard scene graph and desktop_nodes.cpp still reference these entry
// points (the stock XAP scripts expect the node classes to exist), so each one
// is kept as a harmless stub that reports "nothing available".

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "cdaudio.h"
#include "media_player.h"
#include "milkdrop_window.h"
#include "xcloud_client.h"
#include "xcloud_vgames.h"
#include "esde_vgames.h"
#include "launchers/launcher.h"
#include "media_db.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ---- CD audio --------------------------------------------------------------
void CdAudio_Init() {}
void CdAudio_Shutdown() {}
void CdAudio_Poll() {}
void CdAudio_Update() {}
CdDiscType CdAudio_GetDiscType() { return CD_NONE; }
int  CdAudio_GetTrackCount() { return 0; }
int  CdAudio_GetTrackDurationSeconds(int) { return 0; }
int  CdAudio_GetTotalDurationSeconds() { return 0; }
bool CdAudio_Play(int) { return false; }
void CdAudio_Stop() {}
void CdAudio_Pause() {}
void CdAudio_Resume() {}
bool CdAudio_IsPlaying() { return false; }
bool CdAudio_IsPaused() { return false; }
double CdAudio_GetPosition() { return 0.0; }

// ---- Plex / Jellyfin / Playlist no-ops -------------------------------------
void Plex_StartSync() {}
void Plex_Shutdown() {}
void Jellyfin_StartSync() {}
void Jellyfin_Shutdown() {}
void Playlist_LoadAll() {}

// ---- Media player (libmpv) -------------------------------------------------
bool MediaPlayer_Init() { return false; }
void MediaPlayer_Shutdown() {}
bool MediaPlayer_Open(const char*) { return false; }
void MediaPlayer_Play() {}
void MediaPlayer_Pause() {}
void MediaPlayer_TogglePause() {}
void MediaPlayer_Stop() {}
void MediaPlayer_Seek(double) {}
void MediaPlayer_SeekRelative(double) {}
void MediaPlayer_NextChapter() {}
void MediaPlayer_PrevChapter() {}
int  MediaPlayer_GetChapter() { return 0; }
void MediaPlayer_NextAudioTrack() {}
void MediaPlayer_NextSubtitleTrack() {}
int  MediaPlayer_GetTracks(MediaTrack*, int) { return 0; }
void MediaPlayer_SetAudioTrack(int) {}
void MediaPlayer_SetSubtitleTrack(int) {}
void MediaPlayer_SetSpeed(double) {}
double MediaPlayer_GetSpeed() { return 1.0; }
MediaPlayerState MediaPlayer_GetState() { return (MediaPlayerState)0; }
double MediaPlayer_GetPosition() { return 0.0; }
double MediaPlayer_GetDuration() { return 0.0; }
bool MediaPlayer_HasVideo() { return false; }
unsigned int MediaPlayer_GetVideoTexture(int* w, int* h) { if (w) *w = 0; if (h) *h = 0; return 0; }
void MediaPlayer_Update() {}
void MediaPlayer_RenderToScreen(int, int) {}
void MediaPlayer_FrameStep() {}
void MediaPlayer_FrameBackStep() {}
void MediaPlayer_SetZoom(double) {}
void MediaPlayer_SetZoomPos(double, double) {}
void MediaPlayer_SetABLoopA() {}
void MediaPlayer_SetABLoopB() {}
void MediaPlayer_ClearABLoop() {}
int  MediaPlayer_GetABRepeatState() { return 0; }
int  MediaPlayer_GetAudioChannels() { return 0; }
int  MediaPlayer_GetAudioFormat() { return 3; }
const char* MediaPlayer_GetAudioLanguage() { return ""; }
const char* MediaPlayer_GetSubtitleLanguage() { return ""; }
int  MediaPlayer_GetSubtitleTrack() { return 0; }
void MediaPlayer_SetMasterVolume(float) {}

void MediaUI_NoteActivity() {}
void MediaUI_ToggleTrackMenu() {}
void MediaUI_PlayPlaylist(const char*, int) {}
void MediaUI_PlaylistNext() {}
void MediaUI_PlaylistPrev() {}
bool MediaUI_PlaylistActive() { return false; }
bool MediaUI_OsdVisible() { return false; }
void MediaUI_DrawFullscreenVideo() {}
void MediaUI_RenderOSD() {}
void MediaUI_StopFullscreen() {}

extern "C" {
void MediaDB_Lock() {}
void MediaDB_Unlock() {}
void MediaDB_LoadCache() {}
int  MediaDB_GetMovieCount() { return 0; }
const char* MediaDB_GetMovieTitleC(int) { return ""; }
const char* MediaDB_GetMoviePathC(int) { return ""; }
int  MediaDB_GetMovieYearC(int) { return 0; }
int  MediaDB_GetShowCount() { return 0; }
const char* MediaDB_GetShowTitleC(int) { return ""; }
int  MediaDB_GetSeasonCountC(int) { return 0; }
const char* MediaDB_GetSeasonNameC(int, int) { return ""; }
int  MediaDB_GetEpisodeCountC(int, int) { return 0; }
const char* MediaDB_GetEpisodeTitleC(int, int, int) { return ""; }
const char* MediaDB_GetEpisodePathC(int, int, int) { return ""; }
int  MediaDB_GetEpisodeSeasonNumC(int, int, int) { return 0; }
int  MediaDB_GetEpisodeNumberC(int, int, int) { return 0; }
}

// ---- projectM / MilkDrop ---------------------------------------------------
bool g_projectMConfigOpen = false;
bool MilkdropWindow_IsOpen() { return false; }
void MilkdropWindow_Toggle() {}
void MilkdropWindow_Tick() {}
void MilkdropWindow_Shutdown() {}
unsigned int MilkdropWindow_GetWindowID() { return 0; }
void MilkdropWindow_ToggleFullscreen() {}
unsigned short MilkdropWindow_GetBgfxTexId() { return 0xFFFF; }
const unsigned char* MilkdropWindow_GetReadbackRGBA(int* w, int* h) { if (w) *w = 0; if (h) *h = 0; return nullptr; }
void MilkdropWindow_SetPreviewVisible(bool) {}

// ---- xCloud ----------------------------------------------------------------
std::string Xcloud_ArtCachePath(const std::string&) { return {}; }
void Xcloud_QueueArtDownload(const std::string&, const std::string&) {}
void Xcloud_StartStreamSession(const std::string&, const std::string&) {}
void Xcloud_StopStreamSession() {}
void Xcloud_SendGamepad(const XcloudGamepad&) {}
bool Xcloud_StreamRunning() { return false; }
bool Xcloud_StreamReady() { return false; }
uint64_t Xcloud_StreamGeneration() { return 0; }
std::string Xcloud_StreamPhase() { return {}; }
void XcloudVGames_Sync() {}
void XcloudVGames_Tick() {}
void XcloudVGames_Clear() {}

// ---- ES-DE / Steam / RetroArch launchers -----------------------------------
bool g_retroarchFullscreen = false;
void EsdeVGames_Sync(const char*) {}
void EsdeVGames_Clear() {}
void Launcher_Register(const Launcher*) {}
const Launcher* Launcher_FindForSpec(const char*) { return nullptr; }
void Launcher_Build(const char* spec, char* outCmd, size_t outSize)
{
	// No launcher plugins: the spec is the command line.
	if (!outCmd || outSize == 0) return;
	if (!spec) spec = "";
	strncpy(outCmd, spec, outSize - 1);
	outCmd[outSize - 1] = '\0';
}
void Launchers_RegisterAll() {}

// ---- ImGui developer tools (menu bar, editors, inspector, preloader) -------
struct SDL_Window;
struct IDirect3DDevice8;
bool g_titleMakerOpen    = false;
bool g_hddBrowserOpen    = false;
bool g_xapEditorOpen     = false;
bool g_skinEditorOpen    = false;
bool g_playlistMakerOpen = false;
bool g_inspectorOpen     = false;
bool g_settingsOpen      = false;
bool g_extractedMode     = false;
void RenderMainMenuBar() {}
void RenderSettingsWindow() {}
void RenderAboutWindow() {}
void RenderShortcutsWindow() {}
void ToggleSettingsWindow() {}
void RenderControllerMenu() {}
void RenderControllerHints() {}
void RenderProjectMConfig() {}
void RenderPlaylistMaker() {}
void TogglePlaylistMaker() {}
void RenderHDDBrowser() {}
void RenderSkinEditor() {}
void RenderXAPEditor() {}
void ReloadSceneFromEditor() {}
void XapEditor_LoadFile(const char*) {}
bool XapEditor_HasBuffer() { return false; }
bool XapEditor_ConsumeReloadRequest() { return false; }
void XapEditor_Cleanup() {}
void RenderInspectorPanel(IDirect3DDevice8*) {}
void DrawSelectionHighlight(IDirect3DDevice8*) {}
bool RunPreloader(SDL_Window*) { return false; }
void RenderTitleMaker() {}
