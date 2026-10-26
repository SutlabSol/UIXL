// ES-DE library injection into the Virtual Games DB. See esde_vgames.cpp.

#pragma once

// Rescan the ES-DE install at esdeRoot and replace whatever we injected last
// time. Safe to call repeatedly; an empty or missing root just clears.
void EsdeVGames_Sync(const char* esdeRoot);

// Drop every injected entry, for when the user turns ES-DE off.
void EsdeVGames_Clear();
