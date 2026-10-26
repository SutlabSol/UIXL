// boot_anim.cpp: cold-boot animation playback.
//
// XUI Launcher replaces Theseus's libmpv-based player with a dependency-free
// one: the original boot video is pre-split at build time into a numbered JPEG
// frame sequence plus an OGG soundtrack (see build/make-boot-anim.sh):
//
//   Data/Boot/frames/0001.jpg ... NNNN.jpg
//   Data/Boot/boot.ogg
//   Data/Boot/fps.txt           (optional, defaults to 30)
//
// Frames are decoded with stb_image, uploaded to a bgfx texture and drawn as a
// fullscreen quad; audio plays through SDL_mixer. Esc / Enter / Space / click /
// controller A skips.
//
// Original boot animation (c) Microsoft / Pipeworks Software 2001.

#include "display.h"
#include "boot_anim.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <bgfx/bgfx.h>
#include "d3d8_sdl.h"   // for g_bgfxProgBlit, Bgfx_BlitTexturedQuad

#include "stb_image.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

bool g_bootAnimMuted = false;   // set by the launcher when sounds are off

static bool FileExists(const std::string& p)
{
	struct stat st;
	return stat(p.c_str(), &st) == 0;
}

static std::string FramePath(const std::string& dir, int idx)
{
	char name[32];
	snprintf(name, sizeof(name), "/frames/%04d.jpg", idx);
	return dir + name;
}

bool BootAnim_PlayAndWait(SDL_Window* win, const char* path)
{
	if (!win || !path || !*path) return false;
	const std::string dir(path);

	if (!FileExists(FramePath(dir, 1))) {
		fprintf(stderr, "[boot_anim] no frames in %s\n", path);
		return false;
	}
	if (!bgfx::isValid(g_bgfxProgBlit)) {
		fprintf(stderr, "[boot_anim] blit program not initialized; skipping\n");
		return false;
	}

	// Frame count + rate.
	int frameCount = 0;
	while (FileExists(FramePath(dir, frameCount + 1))) frameCount++;
	double fps = 30.0;
	if (FILE* f = fopen((dir + "/fps.txt").c_str(), "rb")) {
		double v = 0;
		if (fscanf(f, "%lf", &v) == 1 && v > 1.0 && v < 240.0) fps = v;
		fclose(f);
	}

	// Probe the first frame for dimensions.
	int fw = 0, fh = 0, fc = 0;
	if (!stbi_info(FramePath(dir, 1).c_str(), &fw, &fh, &fc) || fw <= 0 || fh <= 0)
		return false;

	bgfx::TextureHandle tex = bgfx::createTexture2D(
		(uint16_t)fw, (uint16_t)fh, false, 1,
		bgfx::TextureFormat::RGBA8,
		BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

	// Audio (optional). SDL_mixer ref-counts Mix_OpenAudio, so opening here
	// and closing afterwards leaves the dashboard's own audio init untouched.
	bool audioOpened = false;
	Mix_Music* music = nullptr;
	if (!g_bootAnimMuted) {
		std::string ogg = dir + "/boot.ogg";
		if (FileExists(ogg)) {
			if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
				audioOpened = true;
				extern float g_masterVolume;
				int mv = (int)(g_masterVolume * MIX_MAX_VOLUME + 0.5f);
				if (mv < 0) mv = 0;
				if (mv > MIX_MAX_VOLUME) mv = MIX_MAX_VOLUME;
				Mix_VolumeMusic(mv);
				Mix_Volume(-1, mv);
				music = Mix_LoadMUS(ogg.c_str());
				if (music) Mix_PlayMusic(music, 0);
			}
		}
	}

	const Uint64 freq  = SDL_GetPerformanceFrequency();
	const Uint64 start = SDL_GetPerformanceCounter();
	int  shown      = 0;
	bool running    = true;
	bool sawAnyDraw = false;

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) running = false;
			else if (e.type == SDL_KEYDOWN &&
			         (e.key.keysym.sym == SDLK_ESCAPE ||
			          e.key.keysym.sym == SDLK_RETURN ||
			          e.key.keysym.sym == SDLK_SPACE))
				running = false;
			else if (e.type == SDL_MOUSEBUTTONDOWN) running = false;
			else if (e.type == SDL_CONTROLLERBUTTONDOWN &&
			         (e.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
			          e.cbutton.button == SDL_CONTROLLER_BUTTON_START))
				running = false;
		}
		if (!running) break;

		double t = (double)(SDL_GetPerformanceCounter() - start) / (double)freq;
		int want = 1 + (int)(t * fps);
		if (want > frameCount) break;

		if (want != shown) {
			int w = 0, h = 0, c = 0;
			stbi_uc* px = stbi_load(FramePath(dir, want).c_str(), &w, &h, &c, 4);
			if (px) {
				if (w == fw && h == fh) {
					bgfx::updateTexture2D(tex, 0, 0, 0, 0, (uint16_t)fw, (uint16_t)fh,
						bgfx::copy(px, (uint32_t)(fw * fh * 4)));
					sawAnyDraw = true;
				}
				stbi_image_free(px);
			}
			shown = want;
		}

		// Runs before the main loop exists, so it has to do its own
		// reconcile or the whole animation plays at the wrong scale.
		extern void Gfx_ReconcileDrawable();
		Gfx_ReconcileDrawable();

		int winW = 0, winH = 0;
		Plat_GetDrawableSize(&winW, &winH);
		if (winW <= 0) winW = 1;
		if (winH <= 0) winH = 1;
		bgfx::setViewRect(0, 0, 0, (uint16_t)winW, (uint16_t)winH);
		bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);

		// Letterbox to preserve the video's aspect ratio.
		float va = (float)fw / (float)fh, wa = (float)winW / (float)winH;
		float sx = 1.f, sy = 1.f;
		if (wa > va) sx = va / wa; else sy = wa / va;
		if (sawAnyDraw) Bgfx_BlitTexturedQuad(tex, sx, sy, 0);
		else            bgfx::touch(0);

		bgfx::frame();
	}

	if (music) { Mix_HaltMusic(); Mix_FreeMusic(music); }
	if (audioOpened) Mix_CloseAudio();
	bgfx::destroy(tex);
	return sawAnyDraw;
}
