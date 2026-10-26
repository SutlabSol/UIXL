// Window geometry in the two units that matter. Same on a 1x display, and
// off by the scale factor on Retina or Windows at 150%. Mixing them renders
// the dashboard at half resolution or puts chrome off screen.

#pragma once

// Pixels. For anything handed to bgfx.
void Plat_GetDrawableSize(int* w, int* h);

// Points. For input and ImGui.
void Plat_GetWindowSize(int* w, int* h);

// pixels / points. 2.0 on Retina.
float Plat_GetDisplayScale(void);
