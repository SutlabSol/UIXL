// xui_games.cpp: see xui_games.h.

#include "xui_games.h"

#include "app_paths.h"
#include "json_util.h"
#include "xboxfs_drive.h"   // Plat_MkdirP

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

namespace {

std::vector<XuiGame> s_games;
unsigned             s_gen = 1;

std::string GamesFile() { return AppPath("Configs/games.json"); }

bool ReadAll(const std::string& path, std::string& out)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	char buf[8192];
	size_t n;
	out.clear();
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
	fclose(f);
	return true;
}

bool FileExists(const std::string& p)
{
	struct stat st;
	return !p.empty() && stat(p.c_str(), &st) == 0;
}

std::string Lower(std::string s)
{
	for (auto& c : s) c = (char)tolower((unsigned char)c);
	return s;
}

std::string ExtOf(const std::string& p)
{
	size_t dot = p.find_last_of('.');
	size_t sep = p.find_last_of("/\\");
	if (dot == std::string::npos || (sep != std::string::npos && dot < sep)) return "";
	return Lower(p.substr(dot));
}

// "Halo: Custom Edition!" -> "halo-custom-edition"
std::string Slug(const std::string& s)
{
	std::string out;
	for (char c : s) {
		if (isalnum((unsigned char)c)) out.push_back((char)tolower((unsigned char)c));
		else if (!out.empty() && out.back() != '-') out.push_back('-');
	}
	while (!out.empty() && out.back() == '-') out.pop_back();
	if (out.empty()) out = "game";
	return out;
}

bool CopyBinary(const std::string& src, const std::string& dst)
{
	FILE* in = fopen(src.c_str(), "rb");
	if (!in) return false;
	FILE* out = fopen(dst.c_str(), "wb");
	if (!out) { fclose(in); return false; }
	char buf[65536];
	size_t n;
	bool ok = true;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
	fclose(out);
	fclose(in);
	return ok;
}

// Copies an image into Library/Images and returns the user-dir-relative path.
std::string ImportImage(const std::string& src, const std::string& name)
{
	if (src.empty() || !FileExists(src)) return "";
	std::string ext = ExtOf(src);
	if (ext.empty()) ext = ".png";
	Plat_MkdirP(AppPath("Library/Images"));
	// Unique per import so changing an image never shows a stale cached one.
	char rel[512];
	snprintf(rel, sizeof(rel), "Library/Images/%s-%lld%s",
	         Slug(name).c_str(), (long long)time(nullptr), ext.c_str());
	if (!CopyBinary(src, AppPath(rel))) return "";
	return rel;
}

void DeleteImportedImage(const std::string& rel)
{
	if (rel.rfind("Library/Images/", 0) == 0) remove(AppPath(rel.c_str()));
}

} // namespace

void XuiGames_Load()
{
	s_games.clear();
	std::string js;
	if (ReadAll(GamesFile(), js)) {
		size_t arr = Json_FindArray(js, "games");
		if (arr != std::string::npos) {
			for (const std::string& o : Json_SplitArray(js, arr)) {
				XuiGame g;
				g.name         = Json_GetString(o, "name");
				g.exe          = Json_GetString(o, "exe");
				g.args         = Json_GetString(o, "args");
				g.workdir      = Json_GetString(o, "workdir");
				g.image        = Json_GetString(o, "image");
				g.watchProcess = Json_GetString(o, "watchProcess");
				if (!g.name.empty() || !g.exe.empty()) s_games.push_back(g);
			}
		}
	}
	s_gen++;
	fprintf(stdout, "[games] %d game(s) loaded from %s\n", (int)s_games.size(), GamesFile().c_str());
}

bool XuiGames_Save()
{
	std::string js = "{\n  \"games\": [\n";
	for (size_t i = 0; i < s_games.size(); i++) {
		const XuiGame& g = s_games[i];
		js += "    {\n";
		js += "      \"name\": "         + JsonQuote(g.name)         + ",\n";
		js += "      \"exe\": "          + JsonQuote(g.exe)          + ",\n";
		js += "      \"args\": "         + JsonQuote(g.args)         + ",\n";
		js += "      \"workdir\": "      + JsonQuote(g.workdir)      + ",\n";
		js += "      \"image\": "        + JsonQuote(g.image)        + ",\n";
		js += "      \"watchProcess\": " + JsonQuote(g.watchProcess) + "\n";
		js += (i + 1 < s_games.size()) ? "    },\n" : "    }\n";
	}
	js += "  ]\n}\n";

	std::string path = GamesFile();
	std::string tmp = path + ".tmp";
	FILE* f = fopen(tmp.c_str(), "wb");
	if (!f) return false;
	fwrite(js.data(), 1, js.size(), f);
	fclose(f);
	remove(path.c_str());
	bool ok = rename(tmp.c_str(), path.c_str()) == 0;
	s_gen++;
	return ok;
}

int XuiGames_Count() { return (int)s_games.size(); }

const XuiGame* XuiGames_Get(int idx)
{
	if (idx < 0 || idx >= (int)s_games.size()) return nullptr;
	return &s_games[idx];
}

int XuiGames_Add(const XuiGame& in, const std::string& imageSrc)
{
	XuiGame g = in;
	if (g.name.empty()) g.name = XuiGames_NameFromExe(g.exe);
	if (g.name.empty()) return -1;
	g.image = ImportImage(imageSrc, g.name);
	s_games.push_back(g);
	XuiGames_Save();
	return (int)s_games.size() - 1;
}

bool XuiGames_Update(int idx, const XuiGame& g)
{
	if (idx < 0 || idx >= (int)s_games.size()) return false;
	std::string keepImage = s_games[idx].image;
	s_games[idx] = g;
	s_games[idx].image = keepImage;   // images change only via SetImage
	return XuiGames_Save();
}

bool XuiGames_SetImage(int idx, const std::string& imageSrc)
{
	if (idx < 0 || idx >= (int)s_games.size()) return false;
	std::string rel = ImportImage(imageSrc, s_games[idx].name);
	if (!imageSrc.empty() && rel.empty()) return false;
	DeleteImportedImage(s_games[idx].image);
	s_games[idx].image = rel;
	return XuiGames_Save();
}

bool XuiGames_Remove(int idx)
{
	if (idx < 0 || idx >= (int)s_games.size()) return false;
	DeleteImportedImage(s_games[idx].image);
	s_games.erase(s_games.begin() + idx);
	return XuiGames_Save();
}

bool XuiGames_Move(int idx, int delta)
{
	int to = idx + delta;
	if (idx < 0 || idx >= (int)s_games.size() || to < 0 || to >= (int)s_games.size()) return false;
	std::swap(s_games[idx], s_games[to]);
	return XuiGames_Save();
}

std::string XuiGames_ImagePath(int idx)
{
	const XuiGame* g = XuiGames_Get(idx);
	if (!g || g->image.empty()) return "";
	std::string p = (g->image.size() > 1 && (g->image[1] == ':' || g->image[0] == '/'))
		? g->image : std::string(AppPath(g->image.c_str()));
	return FileExists(p) ? p : "";
}

std::string XuiGames_DirOf(const std::string& path)
{
	size_t sep = path.find_last_of("/\\");
	return sep == std::string::npos ? std::string() : path.substr(0, sep);
}

std::string XuiGames_NameFromExe(const std::string& exe)
{
	size_t sep = exe.find_last_of("/\\");
	std::string base = (sep == std::string::npos) ? exe : exe.substr(sep + 1);
	size_t dot = base.find_last_of('.');
	if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
	return base;
}

unsigned XuiGames_Generation() { return s_gen; }
