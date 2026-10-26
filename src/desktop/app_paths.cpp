#include "app_paths.h"
#include "platform_env.h"
#include "path_ci.h"
#include "xboxfs_drive.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// XUI Launcher: the whole Data tree (scenes, skins, fonts, audio, shaders, boot
// animation) stays with the binary so updates always take effect. Only
// Configs/ and Library/ (settings, games list, game images) live in the
// per-user directory.
static const char* kShippedOnly = "Data";

const char* Plat_ExeDir() {
    static char s_dir[1024] = {0};
    static bool s_init = false;
    if (s_init) return s_dir;
    s_init = true;

#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, s_dir, sizeof(s_dir));
    if (len > 0) {
        char* last = strrchr(s_dir, '\\');
        if (last) *last = '\0';
    }
#elif defined(__APPLE__)
    uint32_t sz = sizeof(s_dir);
    if (_NSGetExecutablePath(s_dir, &sz) == 0) {
        char* last = strrchr(s_dir, '/');
        if (last) *last = '\0';
    }
#else
    ssize_t len = readlink("/proc/self/exe", s_dir, sizeof(s_dir) - 1);
    if (len > 0) {
        s_dir[len] = '\0';
        char* last = strrchr(s_dir, '/');
        if (last) *last = '\0';
    }
#endif
    if (!s_dir[0]) strcpy(s_dir, ".");
    return s_dir;
}

const char* Plat_UserDataDir() {
    static char s_dir[1024] = {0};
    static bool s_init = false;
    if (s_init) return s_dir;
    s_init = true;

#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (appdata && *appdata)
        snprintf(s_dir, sizeof(s_dir), "%s\\XUI Launcher", appdata);
    else
        snprintf(s_dir, sizeof(s_dir), "%s\\AppData\\Roaming\\XUI Launcher", PlatformHomeDir());
#elif defined(__APPLE__)
    snprintf(s_dir, sizeof(s_dir), "%s/Library/Application Support/XUI Launcher", PlatformHomeDir());
#else
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg)
        snprintf(s_dir, sizeof(s_dir), "%s/xui-launcher", xdg);
    else
        snprintf(s_dir, sizeof(s_dir), "%s/.local/share/xui-launcher", PlatformHomeDir());
#endif
    return s_dir;
}

const char* Plat_ShippedDir() {
#ifdef __APPLE__
    // Inside a .app the binary sits in Contents/MacOS and the payload in
    // Contents/Resources. Loose builds keep everything beside the binary.
    static char s_dir[1024] = {0};
    static bool s_init = false;
    if (!s_init) {
        s_init = true;
        const char* exe = Plat_ExeDir();
        size_t n = strlen(exe);
        const char kTail[] = "/Contents/MacOS";
        size_t t = sizeof(kTail) - 1;
        if (n > t && strcmp(exe + n - t, kTail) == 0)
            snprintf(s_dir, sizeof(s_dir), "%.*s/Contents/Resources", (int)(n - t), exe);
        else
            snprintf(s_dir, sizeof(s_dir), "%s", exe);
    }
    return s_dir;
#else
    return Plat_ExeDir();
#endif
}

static bool IsUserOwned(const char* logical) {
    if (!logical || !*logical) return false;
    size_t n = strlen(kShippedOnly);
    if (strncasecmp(logical, kShippedOnly, n) == 0 &&
        (logical[n] == '\0' || logical[n] == '/' || logical[n] == '\\'))
        return false;
    return true;
}

static void CopyFile(const char* src, const char* dst) {
    // Never overwrite: the user's copy wins, we only fill gaps.
    struct stat exists;
    if (stat(dst, &exists) == 0) return;
    FILE* in = fopen(src, "rb");
    if (!in) return;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(out);
    fclose(in);
}

static void CopyTree(const char* src, const char* dst) {
    struct stat st;
    if (stat(src, &st) != 0) return;
    Plat_MkdirP(dst);
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s/*", src);
    struct _finddata_t fd;
    intptr_t h = _findfirst(pattern, &fd);
    if (h == -1) return;
    do {
        if (!strcmp(fd.name, ".") || !strcmp(fd.name, "..")) continue;
        char s[1024], d[1024];
        snprintf(s, sizeof(s), "%s/%s", src, fd.name);
        snprintf(d, sizeof(d), "%s/%s", dst, fd.name);
        if (fd.attrib & _A_SUBDIR) CopyTree(s, d);
        else                       CopyFile(s, d);
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR* dir = opendir(src);
    if (!dir) return;
    struct dirent* e;
    while ((e = readdir(dir)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char s[1024], d[1024];
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
        snprintf(d, sizeof(d), "%s/%s", dst, e->d_name);
        struct stat es;
        if (stat(s, &es) == 0 && S_ISDIR(es.st_mode)) CopyTree(s, d);
        else                                          CopyFile(s, d);
    }
    closedir(dir);
#endif
}

// Copy one tree into the user dir if it isn't there yet.
static void SeedTree(const char* rel) {
    char dst[1024], src[1024];
    snprintf(dst, sizeof(dst), "%s/%s", Plat_UserDataDir(), rel);
    snprintf(src, sizeof(src), "%s/%s", Plat_ShippedDir(), rel);
    struct stat st;
    if (stat(src, &st) == 0) CopyTree(src, dst);   // fills gaps, never clobbers
    else                     Plat_MkdirP(dst);
}

void AppPaths_Init() {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;

    const char* user = Plat_UserDataDir();
    Plat_MkdirP(user);

    // Configs and Library wholesale, then Data subdir by subdir so shaders
    // can be skipped. Seeding from what shipped beside the binary doubles as
    // the one time migration; the original is left alone as a fallback.
    SeedTree("Configs");
    SeedTree("Library");

    // Data/ is shipped-only (see kShippedOnly); nothing to seed.
}

const char* AppPathf(const char* fmt, ...) {
    char logical[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(logical, sizeof(logical), fmt, ap);
    va_end(ap);
    return AppPath(logical);
}

const char* AppPath_Tree(const char* tree) {
    // One slot per tree; only the drive letter roots land here.
    static struct { char name[16]; char path[1024]; } s_cache[8];
    static int s_count = 0;
    if (!tree || !*tree) return "";

    for (int i = 0; i < s_count; i++)
        if (strcasecmp(s_cache[i].name, tree) == 0) return s_cache[i].path;
    if (s_count >= 8) return tree;

    const char* root = IsUserOwned(tree) ? Plat_UserDataDir() : Plat_ShippedDir();
    snprintf(s_cache[s_count].name, sizeof(s_cache[0].name), "%s", tree);
    snprintf(s_cache[s_count].path, sizeof(s_cache[0].path), "%s/%s", root, tree);
    return s_cache[s_count++].path;
}

void Plat_OpenInFileManager(const char* dir) {
    if (!dir || !*dir) return;
#ifdef _WIN32
    ShellExecuteA(NULL, "open", dir, NULL, NULL, SW_SHOWNORMAL);
#else
    // Fork so a slow file manager can't stall us, and no shell is involved.
    pid_t pid = fork();
    if (pid == 0) {
#ifdef __APPLE__
        execlp("open", "open", dir, (char*)NULL);
#else
        // Under flatpak we want the host's file manager.
        if (getenv("FLATPAK_ID"))
            execlp("flatpak-spawn", "flatpak-spawn", "--host", "xdg-open", dir, (char*)NULL);
        execlp("xdg-open", "xdg-open", dir, (char*)NULL);
#endif
        _exit(127);
    }
#endif
}

const char* AppPath(const char* logical) {
    static char s_bufs[8][1024];
    static int s_next = 0;
    char* out = s_bufs[s_next];
    s_next = (s_next + 1) & 7;

    if (!logical || !*logical) { out[0] = '\0'; return out; }

    if (logical[0] == '/' || (logical[0] && logical[1] == ':')) {
        snprintf(out, 1024, "%s", logical);
        Plat_ResolveCaseInsensitive(out, 1024);
        return out;
    }

    const char* root = IsUserOwned(logical) ? Plat_UserDataDir() : Plat_ShippedDir();
    snprintf(out, 1024, "%s/%s", root, logical);
    Plat_ResolveCaseInsensitive(out, 1024);
    return out;
}
