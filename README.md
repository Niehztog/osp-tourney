# OSP Tourney DM — Q2PRO enhancements

This branch (`q2pro-enhancements`) takes the OSP Tourney DM reconstruction and
brings it up to date: **every commit Q2PRO has made to id's `baseq2` game
source is replayed on top of it** — 188 of them, from the 3.20 import to
current master.

That brings in the modern game API, frame-number timers, the rewritten
savegame system, protocol extensions, and twenty years of accumulated crash,
overflow and out-of-bounds fixes. tourney's own code comes through intact: the
cvars, the client commands, all 108 spawn classnames, the arena and team
system, the runes, the bot glue and the menu engine.

Two things are the port's own rather than Q2PRO's.

**The NetGames USA logging is gone.** ngWorldStats and ngStats have been
offline for over twenty years, and what the subsystem still did on every map
change was scramble a log nothing can read, launch `ngWorldStats` and
`ngStatsQ2T` through `system()`, and ask every connecting client to hand over
its ngWorldStats account password in cleartext. The events it recorded were
worth keeping, so they are written locally instead — one JSON object per line,
appended to `osptourney.jsonl` in the game directory, controlled by `statsfile`
and `statsname`. The Standard Log (`sl_log_method`) is unaffected and now has a
file writer of its own, so the two can run at once.

**tourney's own bugs are fixed.** The reconstruction reproduces them on purpose;
a port should not ship them. That covers every client-reachable buffer overflow
in the mod — including one that granted referee status to anyone connecting
from a long enough address — a dozen crashes and out-of-bounds accesses, a
hidden auto-ban that fired on the client's userinfo key order rather than on
anything the player did, and two kick paths that deliberately fed the departing
client a corrupt protocol message. The Makefile builds with `_FORTIFY_SOURCE`
and the stack protector on as a result, rather than off.

* [doc/q2pro-port.md](doc/q2pro-port.md) — how the replay was done, which
  passes were re-run rather than diffed, every deviation, what was removed and
  why, every bug fixed, and what was checked.

Build it the same way as the reconstruction: `make`. Output is `game<cpu>.so`,
which is what Q2PRO looks for.

**This is not the reconstruction.** That lives on `main`, where 1044 of 1051
Linux functions and 1050 of 1082 Windows functions are byte-identical to the
shipped v2.75 binaries. This tree has been reformatted, retyped and
restructured, and nothing in it assembles to those images any more — don't use
it for address matching, and don't compare it against the binaries. The
`asm_matching/` oracles are only meaningful on `main`.

---

# OSP Tourney DM

OSP Tourney DM is a tournament/deathmatch mod for Quake II, released by
Orange Smoothie Productions between 1999 and 2000. Its source code was
never published by the original authors; this repository is an independent
reconstruction of it, built to compile and link into a working Quake II
game library that a Quake II server can load in place of the original
binary. It targets the behaviour of the v2.75 release.

## Authenticity

Every reconstructed function is built with the same compilers the original
release used, and checked instruction-for-instruction against the shipped
v2.75 binaries:

- **Linux** (gcc 2.7.2.3): all 1051 exported functions present, 1044 of
  them (99%) byte-identical machine code; the rest differ only in register
  allocation. Nothing missing, nothing invented.
- **Windows** (Visual C++ 6.0 SP3): all 1082 functions located, 1050 of
  them (97%) byte-identical; the remainder differ only in register
  allocation or relocated addresses.

Global variables, data tables, string literals and struct layouts match
both binaries byte-for-byte as well.

## Building

```
make
```

Produces a debug and a release build, under `debug/` and `release/`
respectively, named `game<arch>.so` for the host architecture. Requires
GCC or Clang on a GNU/Linux system; no dependencies beyond a standard C
toolchain. Drop the resulting shared library into a Quake II `baseq2`-style
game directory as `game.so` to run it with a Quake II 3.20-compatible
engine, such as Yamagi Quake II.

## Contents

- The mod's source: a flat set of `.c`/`.h` files plus the `Makefile` that
  builds them.
- `docs/` — the mod's own shipped documentation: README, changelog,
  server-settings and console-command reference.
- `configs/` — the mod's own shipped example server configurations:
  deathmatch/team/1v1/instagib rule sets, MOTD text, bot loadouts, map
  rotation lists, and a player allow/ban list template.

## Provenance

Reconstructed on top of id Software's Quake II 3.20 game SDK, id's Quake II
CTF 1.02 SDK (the grapple hook and menu system), Mr. Elusive's Gladiator
Bot Q2 game SDK (the bot integration layer), and the RSA Data Security MD5
reference implementation (`md5c.c`, used for log verification; present on
`main`, removed on `q2pro-enhancements` with the logging that called it).
Each of those components carries its own original license terms.

## Disclaimer

Not affiliated with id Software or Orange Smoothie Productions. Provided
for compatibility and preservation purposes.
