// Minimal q2pro build configuration for this standalone game library.
// The engine normally generates this with meson; a game library only needs
// the switches that shape the shared headers.
#define USE_CLIENT              0
#define USE_SERVER              0
#define USE_PROTOCOL_EXTENSIONS 1

// The game ABI, and it is the one switch here a build is expected to move.
// `make API=old` passes -DUSE_NEW_GAME_API=0 and the guard lets it win: that
// build exports GAME_API_VERSION 3 with gclient_old_t/pmove_old_t and loads in
// an engine that predates Q2PRO's extended API -- Yamagi Quake II, r1q2, or
// id's own 3.20 server.  Protocol extensions stay ON either way; they are a
// separate axis (see g_local.h's PM_TIME_SHIFT), and with the old ABI the
// engine simply never negotiates them.
#ifndef USE_NEW_GAME_API
#define USE_NEW_GAME_API        1
#endif
#define USE_DEBUG               0
#define USE_FPS                 0
#define USE_MD5                 0
#define USE_LITTLE_ENDIAN       1
#define VERSION                 "osp-q2pro"
#define CPUSTRING               "x86"
#define BUILDSTRING             "portable"
