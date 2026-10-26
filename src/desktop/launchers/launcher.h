// launcher.h: pluggable launcher contract.
//
// One module per provider that needs to rewrite a spec into a runnable
// command (RetroArch is the live example). Each module fills a Launcher
// struct and registers it at app startup. The dispatcher in launch.cpp
// asks the registry "who owns this spec, and how do I build a runnable
// command?" before spawning. A spec no module claims is spawned as-is.

#pragma once

#include <stddef.h>

struct Launcher {
	// Stable identifier.
	const char* id;

	// Human-readable label.
	const char* displayName;

	// Returns true if this launcher claims the given spec. The
	// dispatcher walks registered launchers in priority order and the
	// first that claims wins.
	bool (*Claims)(const char* spec);

	// Priority for Claims ordering; lower runs first, so a scheme-
	// specific module claims ahead of any broader one.
	int priority;

	// Translate the claimed spec into a spawn-ready command. Returns
	// true if outCmd was filled; on false the dispatcher falls back to
	// the spec verbatim.
	bool (*Build)(const char* spec, char* outCmd, size_t outSize);
};

// Register a launcher. Modules call this from their RegisterFoo()
// which Launchers_RegisterAll() invokes once at startup.
void Launcher_Register(const Launcher* l);

// Find the launcher that claims this spec, by priority. NULL if none.
const Launcher* Launcher_FindForSpec(const char* spec);

// Build a spawn-ready command for `spec` via the matching launcher's
// Build(). If no launcher claims, copies spec verbatim. Always succeeds.
void Launcher_Build(const char* spec, char* outCmd, size_t outSize);

// Register every built-in launcher module. Called once from
// sdl_main.cpp at startup, before the dispatcher is exercised.
void Launchers_RegisterAll();
