// xcloud_vgames.h: inject a signed-in xCloud account's games and consoles
// into the Virtual Games DB. See xcloud_vgames.cpp.

#pragma once

// Kick a background sign-in + fetch, if a token is saved. Call on boot and
// after a fresh login. No-op if not signed in or already running.
void XcloudVGames_Sync();

// Drive the sync state machine. Call once per frame from the main loop; the
// actual g_vgames injection happens here on the main thread.
void XcloudVGames_Tick();

// Drop the injected streaming entries (call on sign-out).
void XcloudVGames_Clear();
