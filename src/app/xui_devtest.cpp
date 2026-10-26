// xui_devtest.cpp: see xui_devtest.h.

#include "xui_devtest.h"

#include <bgfx/bgfx.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "stb_image_write.h"

namespace {

struct Cmd { std::string op, arg; int ms = 120; };

std::vector<Cmd> s_cmds;
size_t  s_pc        = 0;
bool    s_active    = false;
bool    s_quit      = false;
Uint32  s_waitUntil = 0;
Uint8   s_virtual[SDL_NUM_SCANCODES];
Uint8   s_merged[SDL_NUM_SCANCODES];
SDL_Scancode s_heldKey   = SDL_SCANCODE_UNKNOWN;
Uint32       s_releaseAt = 0;

std::string Trim(const std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return {};
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

struct ShotCallback : public bgfx::CallbackI
{
	~ShotCallback() override {}
	void fatal(const char* file, uint16_t line, bgfx::Fatal::Enum, const char* str) override
	{
		fprintf(stderr, "[bgfx fatal] %s(%u): %s\n", file, line, str);
		abort();
	}
	void traceVargs(const char*, uint16_t, const char*, va_list) override {}
	void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
	void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
	void profilerEnd() override {}
	uint32_t cacheReadSize(uint64_t) override { return 0; }
	bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
	void cacheWrite(uint64_t, const void*, uint32_t) override {}
	void screenShot(const char* path, uint32_t w, uint32_t h, uint32_t pitch,
	                bgfx::TextureFormat::Enum, const void* data, uint32_t, bool yflip) override
	{
		std::vector<uint8_t> rgba((size_t)w * h * 4);
		const uint8_t* src = (const uint8_t*)data;
		for (uint32_t y = 0; y < h; y++) {
			const uint8_t* row = src + (size_t)(yflip ? (h - 1 - y) : y) * pitch;
			uint8_t* dst = &rgba[(size_t)y * w * 4];
			for (uint32_t x = 0; x < w; x++) {
				dst[x * 4 + 0] = row[x * 4 + 2];
				dst[x * 4 + 1] = row[x * 4 + 1];
				dst[x * 4 + 2] = row[x * 4 + 0];
				dst[x * 4 + 3] = 255;
			}
		}
		if (stbi_write_png(path, (int)w, (int)h, 4, rgba.data(), (int)w * 4))
			fprintf(stdout, "[devtest] screenshot saved: %s (%ux%u)\n", path, w, h);
		else
			fprintf(stderr, "[devtest] screenshot FAILED: %s\n", path);
	}
	void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
	void captureEnd() override {}
	void captureFrame(const void*, uint32_t) override {}
};

ShotCallback s_callback;

} // namespace

void XuiDev_ParseArgs(int argc, char** argv)
{
	memset(s_virtual, 0, sizeof(s_virtual));
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--script") != 0 || i + 1 >= argc) continue;
		std::string script = argv[++i];
		size_t pos = 0;
		while (pos <= script.size()) {
			size_t semi = script.find(';', pos);
			if (semi == std::string::npos) semi = script.size();
			std::string part = Trim(script.substr(pos, semi - pos));
			pos = semi + 1;
			if (part.empty()) continue;
			Cmd c;
			size_t sp = part.find(' ');
			c.op = part.substr(0, sp);
			if (sp != std::string::npos) c.arg = Trim(part.substr(sp + 1));
			if (c.op == "key") {
				size_t sp2 = c.arg.find(' ');
				if (sp2 != std::string::npos) {
					c.ms = atoi(c.arg.c_str() + sp2 + 1);
					c.arg = c.arg.substr(0, sp2);
				}
			}
			s_cmds.push_back(c);
		}
		s_active = !s_cmds.empty();
	}
}

bool XuiDev_Active() { return s_active; }
bgfx::CallbackI* XuiDev_BgfxCallback() { return &s_callback; }
bool XuiDev_QuitRequested() { return s_quit; }

const Uint8* XuiKeyboardState()
{
	int n = 0;
	const Uint8* real = SDL_GetKeyboardState(&n);
	if (!s_active) return real;
	if (n > SDL_NUM_SCANCODES) n = SDL_NUM_SCANCODES;
	for (int i = 0; i < n; i++) s_merged[i] = real[i] | s_virtual[i];
	return s_merged;
}

static void PushKeyEvent(SDL_Scancode sc, bool down)
{
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	e.key.keysym.scancode = sc;
	e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
	SDL_PushEvent(&e);
}

void XuiDev_Tick()
{
	if (!s_active) return;
	Uint32 now = SDL_GetTicks();

	if (s_heldKey != SDL_SCANCODE_UNKNOWN) {
		if (now < s_releaseAt) return;
		s_virtual[s_heldKey] = 0;
		PushKeyEvent(s_heldKey, false);
		s_heldKey = SDL_SCANCODE_UNKNOWN;
		s_waitUntil = now + 150;   // let the dashboard see the release
	}
	if (now < s_waitUntil) return;

	while (s_pc < s_cmds.size()) {
		const Cmd& c = s_cmds[s_pc++];
		fprintf(stdout, "[devtest] %s %s\n", c.op.c_str(), c.arg.c_str());
		if (c.op == "wait") {
			s_waitUntil = now + (Uint32)(atof(c.arg.c_str()) * 1000.0);
			return;
		} else if (c.op == "key") {
			SDL_Scancode sc = SDL_GetScancodeFromName(c.arg.c_str());
			if (sc == SDL_SCANCODE_UNKNOWN) { fprintf(stderr, "[devtest] unknown key %s\n", c.arg.c_str()); continue; }
			s_virtual[sc] = 1;
			PushKeyEvent(sc, true);
			s_heldKey = sc;
			s_releaseAt = now + (Uint32)c.ms;
			return;
		} else if (c.op == "shot") {
			bgfx::requestScreenShot(BGFX_INVALID_HANDLE, c.arg.c_str());
			s_waitUntil = now + 300;
			return;
		} else if (c.op == "quit") {
			s_quit = true;
			return;
		}
	}
}
