// cdaudio.h: Audio CD detection and playback for desktop builds.
// TOC reading uses platform ioctls (no extra deps). Playback uses a
// dedicated audio-only libmpv instance (guarded by UIX_CDAUDIO).

#pragma once

enum CdDiscType { CD_NONE = 0, CD_AUDIO, CD_DATA };

// Init/shutdown — call from DashAudio_Init / DashAudio_Shutdown
void CdAudio_Init();
void CdAudio_Shutdown();

// Poll for disc changes. Call every few seconds (e.g. from CDiscDrive::Advance).
// Re-reads the TOC and updates internal state.
void CdAudio_Poll();

// Disc info — valid after Poll()
CdDiscType CdAudio_GetDiscType();
int        CdAudio_GetTrackCount();
int        CdAudio_GetTrackDurationSeconds(int track);  // 1-based
int        CdAudio_GetTotalDurationSeconds();

// Playback — requires UIX_CDAUDIO (libmpv). Stubs return false/0 otherwise.
// track: 1-based CD track number
bool   CdAudio_Play(int track);
void   CdAudio_Stop();
void   CdAudio_Pause();
void   CdAudio_Resume();
bool   CdAudio_IsPlaying();
bool   CdAudio_IsPaused();
double CdAudio_GetPosition();  // seconds into the current track

// Process pending mpv events. Call from CAudioClip::Advance while a cd: URL is active.
void CdAudio_Update();
