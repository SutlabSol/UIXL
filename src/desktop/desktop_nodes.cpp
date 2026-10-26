// desktop_nodes.cpp: node class registrations and stubs for the
// Xbox-only XAP nodes that don't have a desktop implementation yet
// (memory monitor, disc drive, copy game, etc.). Audio nodes have
// real SDL_mixer-backed implementations; the rest are stubs that
// keep XAP scripts resolving.

#include "app_paths.h"
#include "std.h"
#ifdef _WIN32
#include <io.h>
#ifndef F_OK
#define F_OK 0
#endif
// Windows wingdi.h defines GetFreeSpace as a macro; conflicts with our method
#undef GetFreeSpace
#endif
#include "dashapp.h"
#include "node.h"
#include "runner.h"
#include "audio_sdl.h"
#include "cdaudio.h"
extern void DiscDrive_SetDiscType(const char* type);

// Helper: resolve XAP audio URL to local filesystem path
// XAP urls look like "Audio/MainAudio/A Button Select.wav" (relative to Q:\)
static const char* ResolveAudioURL(const TCHAR* url)
{
	if (!url || !*url) return NULL;

	static char s_path[512];

	// Check if it already has a drive letter (Q:\...)
	if (url[0] && url[1] == ':') {
		// Use XboxFS translation
		const char* translated = XboxFS_TranslatePath(url);
		return translated;
	}

	// Relative path; prepend Data/
	snprintf(s_path, sizeof(s_path), "%s", AppPathf("Data/%s", url));
	// Convert backslashes
	for (char* p = s_path; *p; p++)
		if (*p == '\\') *p = '/';
	return s_path;
}

// ============================================================================
// CAudioClip - real SDL_mixer implementation
// ============================================================================
class CAudioClip : public CTimeDepNode
{
public:
	CAudioClip() : m_volume(1.0f), m_pan(0.0f), m_frequency(1.0f), m_fade(0.0f),
	               m_removeVoice(false), m_sendProgress(false), m_pause_on_moving(false), m_progress(0.0f),
	               m_url(NULL), m_transportMode(0),
	               m_soundHandle(-1), m_channel(-1), m_isStreaming(false), m_loadedUrl(NULL) {}
	~CAudioClip() {
		Unload();
	}

	void Unload() {
		if (m_loadedUrl && strncmp(m_loadedUrl, "cd:", 3) == 0)
			CdAudio_Stop();
		if (m_channel >= 0) { DashAudio_StopChannel(m_channel); m_channel = -1; }
		if (m_soundHandle >= 0) { DashAudio_FreeSound(m_soundHandle); m_soundHandle = -1; }
		if (m_isStreaming) { DashAudio_FreeMusic(); m_isStreaming = false; }
		free(m_loadedUrl);
		m_loadedUrl = NULL;
	}

	static bool IsCdUrl(const char* url) { return url && strncmp(url, "cd:", 3) == 0; }

	DECLARE_NODE(CAudioClip, CTimeDepNode)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()

	// XAP-visible properties
	float m_volume, m_pan, m_frequency, m_fade, m_progress;
	TCHAR* m_url;
	int m_transportMode; // 0=stopped, 1=playing, 2=paused

	bool m_removeVoice, m_sendProgress, m_pause_on_moving;

	// Internal state
	int   m_soundHandle;  // DashAudio sound handle (-1 if not loaded or streaming)
	int   m_channel;      // SDL_mixer channel (-1 if not playing)
	bool  m_isStreaming;   // true if using Mix_Music (long tracks), false for Mix_Chunk (SFX)
	char* m_loadedUrl;    // tracks what URL is currently loaded (detects changes)

	bool UrlChanged() {
		if (!m_url && !m_loadedUrl) return false;
		if (!m_url || !m_loadedUrl) return true;
		return strcmp(m_url, m_loadedUrl) != 0;
	}

	void LoadIfNeeded()
	{
		if (!m_url || !*m_url) return;

		// If URL changed, unload previous audio
		if (UrlChanged()) {
			Unload();
		}

		// Already loaded
		if (m_soundHandle >= 0 || m_isStreaming) return;

		// Handle "st:SONGID" URLs; soundtrack songs from the music collection
		if (strncmp(m_url, "st:", 3) == 0) {
			int songID = atoi(m_url + 3);
			const char* songPath = DashMusic_GetSongPathByID(songID);
			if (songPath) {
				if (DashAudio_LoadMusic(songPath) == 0)
					m_isStreaming = true;
			}
			m_loadedUrl = strdup(m_url);
			return;
		}

		// Handle "cd:N" URLs, CDDA track; CdAudio_Play handles the actual load
		if (strncmp(m_url, "cd:", 3) == 0) {
			m_loadedUrl = strdup(m_url);
			return;
		}

		const char* path = ResolveAudioURL(m_url);
		if (!path) return;

		// Use streaming for music files (long tracks)
		if (strstr(path, "/Music/") != NULL || strstr(path, ".mp3") != NULL) {
			if (DashAudio_LoadMusic(path) == 0)
				m_isStreaming = true;
		} else {
			// Short sound effect
			m_soundHandle = DashAudio_LoadSound(path);
		}
		m_loadedUrl = strdup(m_url);
	}

	void Play()
	{
		// If paused, resume from current position rather than restart.
		if (m_transportMode == 2) {
			if (IsCdUrl(m_loadedUrl)) {
				CdAudio_Resume();
				DashAudio_MuteAll();
			} else if (m_isStreaming) {
				DashAudio_ResumeMusic();
			} else if (m_channel >= 0) {
				DashAudio_ResumeChannel(m_channel);
			}
			m_transportMode = 1;
			CallFunction(this, _T("onPlay"));
			CallFunction(this, _T("OnTransportModeChanged"));
			return;
		}

		LoadIfNeeded();

		if (IsCdUrl(m_loadedUrl)) {
			int track = atoi(m_loadedUrl + 3);
			if (track < 1) track = 1;
			if (CdAudio_Play(track)) {
				DashAudio_MuteAll();
				m_transportMode = 1;
				m_isActive = true;
				CallFunction(this, _T("onPlay"));
				CallFunction(this, _T("OnTransportModeChanged"));
			}
			return;
		}

		// XUI Launcher: Settings > Audio toggles. Looping clips are the
		// ambient hum; one-shot clips are menu sound effects.
		if (!m_isStreaming && m_soundHandle >= 0) {
			extern bool g_xuiMenuSounds, g_xuiAmbientSound;
			if (m_loop ? !g_xuiAmbientSound : !g_xuiMenuSounds) return;
		}

		if (m_isStreaming) {
			int loops = m_loop ? -1 : 0;
			int fadeMs = (m_fade > 0.0f) ? (int)(m_fade * 1000.0f) : 0;
			DashAudio_SetMusicVolume(m_volume);
			DashAudio_PlayMusic(loops, fadeMs);
			m_transportMode = 1;
			m_isActive = true;
			m_progress = 0.0f;
			CallFunction(this, _T("onPlay"));
			CallFunction(this, _T("OnTransportModeChanged"));
		} else if (m_soundHandle >= 0) {
			int loops = m_loop ? -1 : 0;
			int fadeMs = (m_fade > 0.0f) ? (int)(m_fade * 1000.0f) : 0;
			m_channel = DashAudio_PlaySound(m_soundHandle, loops, fadeMs);
			if (m_channel >= 0) {
				DashAudio_SetChannelVolume(m_channel, m_volume);
				DashAudio_SetChannelPan(m_channel, m_pan);
				m_transportMode = 1;
				m_isActive = true;
				CallFunction(this, _T("onPlay"));
				CallFunction(this, _T("OnTransportModeChanged"));
			}
		}
	}

	void Stop()
	{
		if (IsCdUrl(m_loadedUrl)) {
			CdAudio_Stop();
			DashAudio_UnmuteAll();
			m_transportMode = 0;
			m_isActive = false;
			m_progress = 0.0f;
			CallFunction(this, _T("onStop"));
			CallFunction(this, _T("OnTransportModeChanged"));
			return;
		}

		if (m_isStreaming) {
			int fadeMs = (m_fade > 0.0f) ? (int)(m_fade * 1000.0f) : 0;
			DashAudio_StopMusic(fadeMs);
		} else if (m_channel >= 0) {
			// Hard halt always. Mix_FadeOutChannel was observed leaving
			// channels audible past their fade window on at least macOS,
			// which made ambient sounds (fade=2 in default.xap) keep
			// playing under music. Match the dashboard's expectation that
			// isActive=false means "shut up now".
			DashAudio_StopChannel(m_channel);
		}
		m_channel = -1;
		m_transportMode = 0;
		m_isActive = false;
		m_progress = 0.0f;
		CallFunction(this, _T("onStop"));
		CallFunction(this, _T("OnTransportModeChanged"));
	}

	void Pause()
	{
		if (m_transportMode == 2) {
			// Toggle: resume if already paused
			if (IsCdUrl(m_loadedUrl)) {
				CdAudio_Resume();
				DashAudio_MuteAll();
			} else if (m_isStreaming) {
				DashAudio_ResumeMusic();
			} else if (m_channel >= 0) {
				DashAudio_ResumeChannel(m_channel);
			}
			m_transportMode = 1;
			CallFunction(this, _T("onPlay"));
			CallFunction(this, _T("OnTransportModeChanged"));
			return;
		}

		if (m_transportMode != 1) return;

		if (IsCdUrl(m_loadedUrl)) {
			CdAudio_Pause();
			DashAudio_UnmuteAll();
			m_transportMode = 2;
			CallFunction(this, _T("onPause"));
			CallFunction(this, _T("OnTransportModeChanged"));
			return;
		}

		if (m_isStreaming)
			DashAudio_PauseMusic();
		else if (m_channel >= 0)
			DashAudio_PauseChannel(m_channel);
		m_transportMode = 2;
		CallFunction(this, _T("onPause"));
		CallFunction(this, _T("OnTransportModeChanged"));
	}

	void PlayOrPause()
	{
		if (IsCdUrl(m_loadedUrl)) {
			if (m_transportMode == 1) {
				Pause();
			} else if (m_transportMode == 2) {
				CdAudio_Resume();
				m_transportMode = 1;
				CallFunction(this, _T("onPlay"));
				CallFunction(this, _T("OnTransportModeChanged"));
			} else {
				Play();
			}
			return;
		}

		if (m_transportMode == 1)
			Pause();
		else if (m_transportMode == 2) {
			if (m_isStreaming)
				DashAudio_ResumeMusic();
			else if (m_channel >= 0)
				DashAudio_ResumeChannel(m_channel);
			m_transportMode = 1;
			CallFunction(this, _T("onPlay"));
			CallFunction(this, _T("OnTransportModeChanged"));
		} else {
			Play();
		}
	}

	int getMinutes()
	{
		double pos = IsCdUrl(m_loadedUrl) ? CdAudio_GetPosition()
		                                  : (m_isStreaming ? DashAudio_GetMusicPosition() : 0.0);
		return (int)(pos / 60.0);
	}

	int getSeconds()
	{
		double pos = IsCdUrl(m_loadedUrl) ? CdAudio_GetPosition()
		                                  : (m_isStreaming ? DashAudio_GetMusicPosition() : 0.0);
		return ((int)pos) % 60;
	}

	void Advance(float nSeconds)
	{
		CTimeDepNode::Advance(nSeconds);

		if (m_transportMode == 1 && IsCdUrl(m_loadedUrl)) {
			CdAudio_Update();

			if (m_sendProgress) {
				int track = atoi(m_loadedUrl + 3);
				int dur = CdAudio_GetTrackDurationSeconds(track);
				double pos = CdAudio_GetPosition();
				m_progress = (dur > 0) ? (float)(pos / dur) : 0.0f;
				CallFunction(this, _T("OnProgressChanged"));
			}

			if (!CdAudio_IsPlaying() && !CdAudio_IsPaused()) {
				DashAudio_UnmuteAll();
				m_transportMode = 0;
				m_isActive = false;
				m_progress = 1.0f;
				CallFunction(this, _T("OnEndOfAudio"));
				CallFunction(this, _T("OnTransportModeChanged"));
			}
			return;
		}

		if (m_transportMode == 1) {
			if (m_isStreaming) {
				DashAudio_SetMusicVolume(m_volume);

				// Update progress; always fire OnProgressChanged so script can poll time
				if (m_sendProgress) {
					double dur = DashAudio_GetMusicDuration();
					double pos = DashAudio_GetMusicPosition();
					float newProgress = (dur > 0.0) ? (float)(pos / dur) : 0.0f;
					m_progress = newProgress;
					// Always fire so the script can call getMinutes()/getSeconds()
					CallFunction(this, _T("OnProgressChanged"));
				}

				// Check if music finished
				if (!DashAudio_IsMusicPlaying()) {
					m_transportMode = 0;
					m_isActive = false;
					m_progress = 1.0f;
					CallFunction(this, _T("OnEndOfAudio"));
					CallFunction(this, _T("OnTransportModeChanged"));
				}
			} else if (m_channel >= 0) {
				DashAudio_SetChannelVolume(m_channel, m_volume);
				DashAudio_SetChannelPan(m_channel, m_pan);

				// Check if channel finished
				if (DashAudio_DidChannelFinish(m_channel)) {
					m_channel = -1;
					m_transportMode = 0;
					m_isActive = false;
					m_progress = 1.0f;
					CallFunction(this, _T("OnEndOfAudio"));
					CallFunction(this, _T("OnTransportModeChanged"));
				}
			}
		}
	}

	// Match Xbox CAudioClip: writing isActive triggers Play/Stop. The dashboard
	// uses this to start/stop ambient + periodic audio (DisableAmbientAudio
	// sets every ambient.isActive = false). Without this hook, those writes
	// stored the bool but didn't stop playback, so ambient leaked under music.
	bool OnSetProperty(const PRD* pprd, const void* pvValue)
	{
		if ((intptr_t)pprd->pbOffset == MEMBER_OFFSET(m_isActive))
		{
			bool wantActive = *(bool*)pvValue;
			if (wantActive == m_isActive) return true;
			m_isActive = wantActive;
			if (m_isActive) Play();
			else            Stop();
		}
		return true;
	}
};

IMPLEMENT_NODE("AudioClip", CAudioClip, CTimeDepNode)

START_NODE_PROPS(CAudioClip, CTimeDepNode)
	NODE_PROP(pt_number, CAudioClip, volume)
	NODE_PROP(pt_number, CAudioClip, pan)
	NODE_PROP(pt_number, CAudioClip, frequency)
	NODE_PROP(pt_number, CAudioClip, fade)
	NODE_PROP(pt_string, CAudioClip, url)
	NODE_PROP(pt_integer, CAudioClip, transportMode)
	NODE_PROP(pt_boolean, CAudioClip, removeVoice)
	NODE_PROP(pt_boolean, CAudioClip, sendProgress)
	NODE_PROP(pt_boolean, CAudioClip, pause_on_moving)
	NODE_PROP(pt_number, CAudioClip, progress)
	NODE_PROP(pt_boolean, CAudioClip, isActive)
	NODE_PROP(pt_boolean, CAudioClip, loop)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CAudioClip
START_NODE_FUN(CAudioClip, CTimeDepNode)
	NODE_FUN_VV(Play)
	NODE_FUN_VV(Stop)
	NODE_FUN_VV(Pause)
	NODE_FUN_IV(getMinutes)
	NODE_FUN_IV(getSeconds)
	NODE_FUN_VV(PlayOrPause)
END_NODE_FUN()

// ============================================================================
// CPeriodicAudioGroup - from Audio.cpp
// ============================================================================
class CPeriodicAudioGroup : public CGroup
{
public:
	CPeriodicAudioGroup() : m_isActive(false), m_period(10.0f), m_periodNoise(0.0f) {}

	DECLARE_NODE(CPeriodicAudioGroup, CGroup)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()
	bool m_isActive;
	float m_period, m_periodNoise;
};

IMPLEMENT_NODE("PeriodicAudioGroup", CPeriodicAudioGroup, CGroup)

START_NODE_PROPS(CPeriodicAudioGroup, CGroup)
	NODE_PROP(pt_boolean, CPeriodicAudioGroup, isActive)
	NODE_PROP(pt_number, CPeriodicAudioGroup, period)
	NODE_PROP(pt_number, CPeriodicAudioGroup, periodNoise)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CPeriodicAudioGroup
START_NODE_FUN(CPeriodicAudioGroup, CGroup)
END_NODE_FUN()

// ============================================================================
// CMusicCollection - real SDL_mixer implementation
// Delegates to DashMusic_* functions in audio_sdl.cpp
// ============================================================================
class CMusicCollection : public CNode
{
public:
	CMusicCollection() : m_copyProgress(0.0f), m_error(0) {}

	DECLARE_NODE(CMusicCollection, CNode)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()
	float m_copyProgress;
	int m_error;

	int GetSoundtrackCount() { return DashMusic_GetSoundtrackCount(); }
	int GetSoundtrackID(int index) { return DashMusic_GetSoundtrackID(index); }
	int GetSoundtrackIndexFromID(int id) { return DashMusic_GetSoundtrackIndexFromID(id); }

	CStrObject* GetSoundtrackName(int index) {
		return new CStrObject(_T(DashMusic_GetSoundtrackName(index)));
	}

	CStrObject* FormatSoundtrackTime(int stIndex) {
		// Sum all song durations in this soundtrack
		int total = 0;
		int count = DashMusic_GetSongCount(stIndex);
		for (int i = 0; i < count; i++)
			total += DashMusic_GetSongDuration(stIndex, i);
		return new CStrObject(_T(DashMusic_FormatTime(total)));
	}

	int GetSoundtrackSongCount(int stIndex) { return DashMusic_GetSongCount(stIndex); }
	int GetSoundtrackSongID(int stIndex, int songIndex) { return DashMusic_GetSongID(stIndex, songIndex); }

	CStrObject* GetSoundtrackSongName(int stIndex, int songIndex) {
		return new CStrObject(_T(DashMusic_GetSongName(stIndex, songIndex)));
	}

	CStrObject* FormatSoundtrackSongTime(int stIndex, int songIndex) {
		int dur = DashMusic_GetSongDuration(stIndex, songIndex);
		return new CStrObject(_T(DashMusic_FormatTime(dur)));
	}

	// Editing operations; no-ops for now (read-only collection)
	void AddSoundtrack(int) {}
	void DeleteSoundtrack(int) {}
	void ClearCopyList() {}
	void AddSongToCopyList(int, int) {}
	void StartCopy() {}
	void SetSongName(int, int, CStrObject*) {}
	void SetSoundtrackName(int, CStrObject*) {}
	void MoveSongUp(int, int) {}
	void MoveSongDown(int, int) {}
	void DeleteSong(int, int) {}
	CStrObject* CreateSoundtrackName() { return new CStrObject(_T("New Soundtrack")); }
	CStrObject* GetUpdateString() { return new CStrObject; }
};

IMPLEMENT_NODE("MusicCollection", CMusicCollection, CNode)

START_NODE_PROPS(CMusicCollection, CNode)
	NODE_PROP(pt_number, CMusicCollection, copyProgress)
	NODE_PROP(pt_integer, CMusicCollection, error)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CMusicCollection
START_NODE_FUN(CMusicCollection, CNode)
	NODE_FUN_IV(GetSoundtrackCount)
	NODE_FUN_II(GetSoundtrackID)
	NODE_FUN_II(GetSoundtrackIndexFromID)
	NODE_FUN_SI(GetSoundtrackName)
	NODE_FUN_SI(FormatSoundtrackTime)
	NODE_FUN_II(GetSoundtrackSongCount)
	NODE_FUN_III(GetSoundtrackSongID)
	NODE_FUN_SII(GetSoundtrackSongName)
	NODE_FUN_SII(FormatSoundtrackSongTime)
	NODE_FUN_VI(AddSoundtrack)
	NODE_FUN_VI(DeleteSoundtrack)
	NODE_FUN_VV(ClearCopyList)
	NODE_FUN_VII(AddSongToCopyList)
	NODE_FUN_VV(StartCopy)
	NODE_FUN_VIIS(SetSongName)
	NODE_FUN_VIS(SetSoundtrackName)
	NODE_FUN_VII(MoveSongUp)
	NODE_FUN_VII(MoveSongDown)
	NODE_FUN_VII(DeleteSong)
	NODE_FUN_SV(CreateSoundtrackName)
	NODE_FUN_SV(GetUpdateString)
END_NODE_FUN()

// ============================================================================
// CMemoryMonitor - from Memory.cpp
// ============================================================================
class CMemoryMonitor : public CNode
{
public:
	CMemoryMonitor() : m_curDevUnit(8), m_invalidDevUnit(0), m_blockInsertion(false), m_enumerationOn(false) {}

	DECLARE_NODE(CMemoryMonitor, CNode)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()
	int m_curDevUnit, m_invalidDevUnit;
	bool m_blockInsertion, m_enumerationOn;

	// Navigation: on desktop, always stay on HDD (unit 8)
	void selectUp() { m_curDevUnit = 8; }
	void selectDown() { m_curDevUnit = 8; }
	void selectLeft() { m_curDevUnit = 8; }
	void selectRight() { m_curDevUnit = 8; }
	int selectDevUnit(int unit) { m_curDevUnit = 8; return 8; }
	CStrObject* FormatDeviceName(int) { return new CStrObject(_T("Xbox Hard Disk")); }
	CStrObject* FormatTotalBlocks() { return new CStrObject(_T("50000+")); }
	CStrObject* FormatFreeBlocks(int) { return new CStrObject(_T("50000+")); }
	CStrObject* FormatFreeSlots(int) { return new CStrObject(_T("50000+")); }
	int GetTotalFreeBlocks(int) { return 50000; }
	int GetTotalFreeSlots(int) { return 50000; }
	int HaveDevice(int n) { return (n == 8) ? 1 : 0; }  // Hard drive always present, no MUs
	int HaveDeviceTop(int) { return 0; }    // No MUs on desktop
	int HaveDeviceBottom(int) { return 0; }  // No MUs on desktop
	float GetFreeTotalRatio(int) { return 1.0f; }
	void SetMUName(int, CStrObject*) {}
	void FormatMemoryUnit(int) {}
};

IMPLEMENT_NODE("MemoryMonitor", CMemoryMonitor, CNode)

START_NODE_PROPS(CMemoryMonitor, CNode)
	NODE_PROP(pt_integer, CMemoryMonitor, curDevUnit)
	NODE_PROP(pt_integer, CMemoryMonitor, invalidDevUnit)
	NODE_PROP(pt_boolean, CMemoryMonitor, blockInsertion)
	NODE_PROP(pt_boolean, CMemoryMonitor, enumerationOn)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CMemoryMonitor
START_NODE_FUN(CMemoryMonitor, CNode)
	NODE_FUN_VV(selectUp)
	NODE_FUN_VV(selectDown)
	NODE_FUN_VV(selectLeft)
	NODE_FUN_VV(selectRight)
	NODE_FUN_II(selectDevUnit)
	NODE_FUN_SI(FormatDeviceName)
	NODE_FUN_SV(FormatTotalBlocks)
	NODE_FUN_SI(FormatFreeBlocks)
	NODE_FUN_SI(FormatFreeSlots)
	NODE_FUN_II(GetTotalFreeBlocks)
	NODE_FUN_II(GetTotalFreeSlots)
	NODE_FUN_II(HaveDevice)
	NODE_FUN_II(HaveDeviceTop)
	NODE_FUN_II(HaveDeviceBottom)
	NODE_FUN_NI(GetFreeTotalRatio)
	NODE_FUN_VIS(SetMUName)
	NODE_FUN_VI(FormatMemoryUnit)
END_NODE_FUN()

// ============================================================================
// CDiscDrive - from disc.cpp
// ============================================================================
class CDiscDrive : public CNode
{
public:
	static CDiscDrive* s_instance;

	CDiscDrive() : m_discType(NULL), m_locked(false), m_pollTimer(2.0f) {
		m_discType = strdup("none");
		s_instance = this;
	}

	DECLARE_NODE(CDiscDrive, CNode)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()
	TCHAR* m_discType;
	bool   m_locked;
	float  m_pollTimer;

	void Advance(float nSeconds) {
		CNode::Advance(nSeconds);
		m_pollTimer += nSeconds;
		if (m_pollTimer < 2.0f) return;
		m_pollTimer = 0.0f;

		CdAudio_Poll();

		const char* newType;
		switch (CdAudio_GetDiscType()) {
			case CD_AUDIO: newType = "Audio"; break;
			default:       newType = "none";  break;
		}

		if (m_discType && strcmp(m_discType, newType) == 0) return;

		bool wasNone = !m_discType || strcmp(m_discType, "none") == 0;
		bool isNone  = strcmp(newType, "none") == 0;

		free(m_discType);
		m_discType = strdup(newType);

		if (!m_locked) {
			if (!isNone && wasNone)
				CallFunction(this, "OnDiscInserted");
			else if (isNone && !wasNone)
				CallFunction(this, "OnDiscRemoved");
		}
	}

	int getTrackCount() { return CdAudio_GetTrackCount(); }

	CStrObject* FormatTotalTime() {
		int total = CdAudio_GetTotalDurationSeconds();
		char buf[16];
		snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
		return new CStrObject(buf);
	}

	CStrObject* FormatTrackTime(int track) {
		int dur = CdAudio_GetTrackDurationSeconds(track + 1);  // script passes 0-based index
		char buf[16];
		snprintf(buf, sizeof(buf), "%d:%02d", dur / 60, dur % 60);
		return new CStrObject(buf);
	}

	void LaunchDisc() {
		// On real Xbox this reboots into the DVD player app.
		// On desktop, we intercept and just start the DVD inline player.
		fprintf(stderr, "[DiscDrive] LaunchDisc intercepted\n");
		CObject* pRoot = (CObject*)g_pObject;
		if (pRoot) {
			CallFunction(pRoot, "StartDVDPlayer");
		}
	}
	void OpenTray() {}
	void CloseTray() {}
	CStrObject* getArtist() { return new CStrObject; }
	CStrObject* getTitle()  { return new CStrObject; }
	CStrObject* getTrackName(int) { return new CStrObject; }
};

CDiscDrive* CDiscDrive::s_instance = nullptr;
IMPLEMENT_NODE("DiscDrive", CDiscDrive, CNode)

// Set the disc type from outside the XAP (used by menu_bar for simulation).
// Video: directly invokes StartDVDPlayer (bypasses XAP, matches prior desktop behaviour).
// Audio: fires the normal OnDiscInserted callback so the XAP music screen handles it.
// none : fires OnDiscRemoved (or navigates away from the DVD player if it was active).
void DiscDrive_SetDiscType(const char* type) {
	CDiscDrive* dd = CDiscDrive::s_instance;
	if (!dd) return;

	bool wasNone = !dd->m_discType || strcmp(dd->m_discType, "none") == 0;
	bool isNone  = strcmp(type, "none") == 0;

	if (dd->m_discType) free(dd->m_discType);
	dd->m_discType = strdup(type);

	if (isNone) {
		if (!wasNone) {
			CObject* pRoot = (CObject*)g_pObject;
			if (pRoot) CallFunction(pRoot, "GoToLauncher");
			fprintf(stderr, "[DiscDrive] discType='none', navigated to launcher\n");
		}
	} else if (strcmp(type, "Video") == 0) {
		CObject* pRoot = (CObject*)g_pObject;
		if (pRoot) {
			CallFunction(pRoot, "StartDVDPlayer");
			fprintf(stderr, "[DiscDrive] discType='Video', called StartDVDPlayer\n");
		}
	} else {
		// Audio CD (or anything else): let the XAP script decide via OnDiscInserted
		if (!dd->m_locked)
			CallFunction(dd, "OnDiscInserted");
		fprintf(stderr, "[DiscDrive] discType='%s', fired OnDiscInserted\n", type);
	}
}

START_NODE_PROPS(CDiscDrive, CNode)
	NODE_PROP(pt_string, CDiscDrive, discType)
	NODE_PROP(pt_boolean, CDiscDrive, locked)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CDiscDrive
START_NODE_FUN(CDiscDrive, CNode)
	NODE_FUN_IV(getTrackCount)
	NODE_FUN_SV(FormatTotalTime)
	NODE_FUN_SI(FormatTrackTime)
	NODE_FUN_VV(LaunchDisc)
	NODE_FUN_VV(OpenTray)
	NODE_FUN_VV(CloseTray)
	NODE_FUN_SV(getArtist)
	NODE_FUN_SV(getTitle)
	NODE_FUN_SI(getTrackName)
END_NODE_FUN()


// ============================================================================
// CLiveAccounts - from LiveAccounts.cpp
// ============================================================================
class CLiveAccounts : public CNode
{
public:
	CLiveAccounts() : m_nCurrentIndex(0), m_bLogon(false), m_fLogOnSuccess(false), m_fLogOnInProgress(false) {}

	DECLARE_NODE(CLiveAccounts, CNode)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()

	int m_nCurrentIndex;
	bool m_bLogon, m_fLogOnSuccess, m_fLogOnInProgress;

	int GetNumberOfAccounts() { return 0; }
	int GetNumAccountsOnHD() { return 0; }
	CStrObject* GetAccountName(int) { return new CStrObject; }
	void Refresh() {}
	CStrObject* GetMessageOfTheDayText() { return new CStrObject; }
	void ShowIcon(int) {}
	void ClearMOTDCache() {}
	void ClearLastLogonUser() {}
	void Logon(int, const TCHAR*) {}
	void Logoff() {}
	int IsPasswordVerified() { return 0; }
	int IsBackFromEntryPoint() { return 0; }
	void PersistUser(int) {}
	int IsVoiceAllowed() { return 0; }
	CStrObject* GetGameInvites() { return new CStrObject; }
	CStrObject* GetFriendInvites() { return new CStrObject; }
	CStrObject* GetNumberOfFriendsOnline() { return new CStrObject(_T("0")); }
	void LaunchEntryPoint(int, int, const TCHAR*) {}
};

IMPLEMENT_NODE("LiveAccounts", CLiveAccounts, CNode)

START_NODE_PROPS(CLiveAccounts, CNode)
	NODE_PROP(pt_boolean, CLiveAccounts, bLogon)
	NODE_PROP(pt_boolean, CLiveAccounts, fLogOnSuccess)
	NODE_PROP(pt_boolean, CLiveAccounts, fLogOnInProgress)
	NODE_PROP(pt_integer, CLiveAccounts, nCurrentIndex)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CLiveAccounts
START_NODE_FUN(CLiveAccounts, CNode)
	NODE_FUN_IV(GetNumberOfAccounts)
	NODE_FUN_IV(GetNumAccountsOnHD)
	NODE_FUN_SI(GetAccountName)
	NODE_FUN_VV(Refresh)
	NODE_FUN_SV(GetMessageOfTheDayText)
	NODE_FUN_VI(ShowIcon)
	NODE_FUN_VV(ClearMOTDCache)
	NODE_FUN_VV(ClearLastLogonUser)
	NODE_FUN_VIS(Logon)
	NODE_FUN_VV(Logoff)
	NODE_FUN_IV(IsPasswordVerified)
	NODE_FUN_IV(IsBackFromEntryPoint)
	NODE_FUN_VI(PersistUser)
	NODE_FUN_IV(IsVoiceAllowed)
	NODE_FUN_SV(GetGameInvites)
	NODE_FUN_SV(GetFriendInvites)
	NODE_FUN_SV(GetNumberOfFriendsOnline)
	NODE_FUN_VIIS(LaunchEntryPoint)
END_NODE_FUN()


// ============================================================================
// CDisplay. XAP-callable view over the desktop window's resolution + mode.
// Same g_windowResolution / g_windowMode the ImGui Settings -> Display tab
// drives, so changes from either surface stay in sync.
// ============================================================================

extern int  g_windowResolution;
extern int  g_windowMode;
extern bool g_displayChangeRequested;
extern void SaveDesktopSettings();

class CDisplay : public CNode
{
public:
	CDisplay() {}
	DECLARE_NODE(CDisplay, CNode)
	DECLARE_NODE_PROPS()
	DECLARE_NODE_FUNCTIONS()

	int GetResolution() { return g_windowResolution; }
	int GetMode()       { return g_windowMode; }

	void SetResolution(int n)
	{
		if (n != 0 && n != 720 && n != 1080 && n != 1440 && n != 2160) return;
		if (g_windowResolution == n) return;
		g_windowResolution = n;
		g_displayChangeRequested = true;
		SaveDesktopSettings();
	}

	void SetMode(int n)
	{
		if (n < 0 || n > 2) return;
		if (g_windowMode == n) return;
		g_windowMode = n;
		g_displayChangeRequested = true;
		SaveDesktopSettings();
	}

	// Convenience predicates the scene reads to render YES/NO labels.
	int IsResolution720()  { return g_windowResolution == 720; }
	int IsResolution1080() { return g_windowResolution == 1080; }
	int IsResolution1440() { return g_windowResolution == 1440; }
	int IsResolution2160() { return g_windowResolution == 2160; }
	int IsFullscreen()     { return g_windowMode != 0; }
};

IMPLEMENT_NODE("Display", CDisplay, CNode)

START_NODE_PROPS(CDisplay, CNode)
END_NODE_PROPS()

#undef _FND_CLASS
#define _FND_CLASS CDisplay
START_NODE_FUN(CDisplay, CNode)
	NODE_FUN_IV(GetResolution)
	NODE_FUN_IV(GetMode)
	NODE_FUN_VI(SetResolution)
	NODE_FUN_VI(SetMode)
	NODE_FUN_IV(IsResolution720)
	NODE_FUN_IV(IsResolution1080)
	NODE_FUN_IV(IsResolution1440)
	NODE_FUN_IV(IsResolution2160)
	NODE_FUN_IV(IsFullscreen)
END_NODE_FUN()
