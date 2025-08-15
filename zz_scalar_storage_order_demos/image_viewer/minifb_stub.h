#pragma once
// Minimal stub wrapper for MiniFB integration. If MiniFB is available (user supplies -DWITH_MINIFB and includes path),
// we include it; otherwise we provide a no-op interface so the demo still builds.
// This keeps upstream patch series clean because the demo directory will be removed before submission.
#ifdef WITH_MINIFB
#include <MiniFB.h>
#else
static inline int mfb_open_ex(const char *title, unsigned width, unsigned height, int flags) { (void)title;(void)width;(void)height;(void)flags; return 1; }
static inline int mfb_update(int window, const void *buffer) { (void)window;(void)buffer; return 0; }
static inline void mfb_close(int window){ (void)window; }
#endif
