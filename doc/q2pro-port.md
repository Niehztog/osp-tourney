OSP Tourney DM on the Q2PRO game API
====================================

This branch (`q2pro-enhancements`) is the OSP Tourney DM reconstruction with
the whole of Q2PRO's `baseq2` commit history replayed on top of it.

It is **not** the reconstruction. That lives on `main`, where the tree is
measured against the shipped `gamei386.so` and `gamex86.dll` and the
`asm_matching/` oracles still mean something. Here they do not: the astyle
pass, the retyping and the API changes mean nothing in this tree assembles to
the original images. Do not use this branch for address matching, and do not
compare it against the binaries.

Why
---

tourney is id's 3.20 game source with a large mod grafted onto it, plus the Q2
CTF SDK and the Gladiator Bot SDK as donors. It carries every rough edge that
1998 source shipped with — unbounded `sprintf` into fixed buffers, float timers
that drift, savegame code that writes raw pointers to disk, `char` arithmetic on
network input. Q2PRO has spent twenty-odd years fixing exactly that code.
Replaying its history gets all of it at once, rather than re-deriving each fix.

Method
------

Both trees descend from id's Quake II 3.20 game source, so this was a rebase
rather than a merge or a rewrite.

1. **Establish the common root.** Q2PRO's first game commit is a normalised
   copy of id's 3.20/3.21 source — tabs expanded, CRLF stripped, `qboolean`
   spellings unified, `Com_sprintf` renamed, monster frame tables re-braced.
   That normalisation was reproduced exactly (the reproduction of Q2PRO's
   import commit is byte-identical to the real one) and then applied to
   tourney's own 3.20 base, which commit `8d481e4` on `main` records unmodified
   and which is byte-identical to the copy used for the same port of Rocket
   Arena 2. Both trees now sit on the same root.

2. **Record tourney as a delta.** The reconstruction becomes a single commit on
   that root: 66 files, of which 40 are tourney's own, and 44 baseq2 files it
   does not ship at all — every monster except the shared `m_move.c`.

3. **Replay.** Q2PRO's 188 game-source commits are rebased onto that delta, one
   at a time, conflicts resolved per commit.

Step 3 is the interesting part, because roughly a fifth of Q2PRO's commits are
*mechanical*. Applying those as diffs would only reach the files baseq2 and
tourney have in common — `osp_main.c`, `osp_menus.c`, `arena`-side code, the
logging layer and the menu engine would have been left behind in the old style,
and the result would not compile once the shared headers changed underneath it.

So mechanical commits were replayed by **re-running the transformation** over
the whole tree. A file byte-identical to Q2PRO's is taken verbatim; everything
else is transformed locally with the same tool and settings Q2PRO used. The
passes replayed this way:

| pass | what it does |
|---|---|
| astyle | the "Massive coding style change" reformat |
| stdbool | `qboolean`/`qtrue`/`qfalse` → `bool`/`true`/`false` |
| qrand | `rand()` → `Q_rand()` |
| qatoi | `atoi()` → `Q_atoi()` |
| nextthink | `nextthink` seconds → frame numbers |
| timers | debounce, client, monster and misc timers → frame numbers |
| desig | positional item initialisers → designated |
| precarray | precache strings → NULL-terminated arrays |
| floatsuffix | `1.0` → `1.0f` |
| maxclients | `maxclients->value` → `game.maxclients` |
| striptrail, wsfix | the two whitespace passes |

The exception is the donor layer: the Gladiator Bot SDK's six `bl_*` files,
`botlib.h`, the RSA reference `md5c.c`/`md5.h` and `anorms.h` are excluded from
the *formatting* passes only. They keep their upstream shape — including the
SDK's `} //end of the function NAME` idiom — but they took every API and type
change, and they compile with the same flags as everything else.

Deviations
----------

Q2PRO changes with no site in tourney, or a different one, were resolved by
hand:

* **Flood protection.** Q2PRO extracted the flood check into `FloodProtect()`
  and fixed an out-of-range read in it. tourney had already replaced that spot
  with its own inline check gated on `!team && !match_paused`. Q2PRO's function
  is adopted and tourney's gating kept: `if (!team && !match_paused &&
  FloodProtect(ent))`.

* **Jump sounds.** Q2PRO's fix tests the *previous* frame's `pm_flags`, so the
  `client->ps.pmove = pm.s` save had to move after it. tourney's `ClientThink`
  is heavily restructured, so both the test and the move were placed by hand.

* **`RF_BEAM` old_origin guard.** Q2PRO added it to baseq2's unconditional
  `VectorCopy`; tourney's equivalent is already guarded by `FL_OLDORGNOTSET`,
  so the new condition joined that one.

* **Spawn floor-clip and the 200 ms spawn hold.** Both landed in tourney's
  `PutClientInServer`/`ClientBeginDeathmatch`, which are structured differently
  from baseq2's.

* **Second powerup timer.** Q2PRO put `STAT_TIMER2_ICON`/`STAT_TIMER2` in stat
  slots 18 and 19. tourney uses every slot up to 28 — 18..21 for frags and team
  scores, 22..26 for the runes, 27..28 for the popup menus — so they moved to
  29/30, and the `if 29 … endif` block was added to all four of tourney's
  status bars as well as to baseq2's `single_statusbar`.

* **`sv_maplist`, `maxspectators`, `spectator_password`, `needpass`.** Q2PRO's
  `InitGame` registers these; nothing in tourney reads them, because it has its
  own map rotation and observer system. The registrations are kept — they still
  appear in serverinfo — and `g_local.h` says so.

* **Internal linkage.** Q2PRO made a number of baseq2 functions `static` once
  nothing outside their file called them. tourney still does, from its own
  files, so `EndDMLevel`, `DeathmatchScoreboard`, `Cmd_InvUse_f`, `Cmd_Kill_f`,
  `InitClientPersistant`, `Use_Quad` and `Use_Invulnerability` keep external
  linkage. The reverse also happens: `HelpComputer`, `PlayerSort`,
  `ai_run_melee`, `ai_run_missile`, `ai_run_slide` and the `enemy_vis`/
  `enemy_range` pair are dead here because tourney replaced their callers with
  the bot library, so they are tagged `q_unused` rather than deleted.

* **Statusbar strings.** Q2PRO reduced duplication by concatenating
  `dm_statusbar` onto `single_statusbar`. tourney's four bars are each complete,
  so none of them is concatenated — and because `osp_cmds.c` sends
  `dm_statusbar` itself, they keep external linkage instead of becoming
  file-local.

* **`monster_makron`.** Q2PRO added it to the spawn table. tourney ships no
  monster code, so the row and its prototype are dropped.

* **`Swap_Init`.** Lived in `q_shared.c`; Q2PRO's `shared.c` is
  little-endian-only and has no equivalent, so the call in `GetGameAPI` is gone.

* **The Gladiator SDK's `PMF_*` redefinitions.** `bl_main.c` redefined seven of
  them inside `BotSetPMoveState` as its own documentation of what the engine
  passes in. Q2PRO's `shared.h` defines the same seven with identical values, so
  the shadowing copies are dropped and the note kept. `botlib.h`'s
  `PointContents` slot is const-ified to match `gi.pointcontents`, which is
  assigned straight into it — a change of pointer type, not of ABI.

Two fixes are not Q2PRO's
-------------------------

* **`pers.spectator` goes back to `int`.** tourney also uses it as a strike
  counter: `OSP_speedCheat_cmd` increments it and `OSP_speedDetect` tests it
  against 3. In the original it was vanilla's `qboolean`, an int-sized enum, so
  counting worked. Q2PRO's conversion to C99 `bool` would saturate it at 1 and
  make the `>= 3` test unreachable, so this one field is not converted.

* **The two deliberate overruns are fixed.** The reconstruction reproduces them
  on purpose; a port should not ship them. `OSP_defaultteam_cmd` copied 128
  bytes into an 80-byte field with a `strncpy` that also never
  NUL-terminated — both copies are `Q_strlcpy` now. `OSP_startDemos` used
  `if (found++ == 2) break;`, which lets a third player write `name[2]` and
  `cids[2]`, one element past both arrays, before the loop exits; it is
  `if (++found == 2)` now. The Makefile still relaxes `_FORTIFY_SOURCE` and the
  stack protector, because tourney's string handling has not been audited end
  to end — that is the obvious next piece of work on this branch.

What is checked
---------------

* All 40 baseq2 item-list entries match Q2PRO's field for field; tourney's five
  runes and two dummy flag items are the only additions, and no baseq2 item is
  missing.
* All 108 spawn classnames, all 267 cvars and all 138 client-command strings
  survive.
* `g_ptrs.c` regenerates byte-identical from Q2PRO's own `genptr.py` over this
  tree.
* tourney adds five entity spawn keys over vanilla (`botlib`, `name`, `skin`,
  `charfile`, `charname`); all five are in `g_spawn.c`'s `temp_fields[]`, where
  Q2PRO moved that table. Its 26-entry relocatable-pointer set is covered by the
  descriptor tables, `client` included — Q2PRO rebuilds `ent->client` from the
  edict index in `ReadLevel` rather than saving it, exactly as baseq2 does.
* Every function the reconstruction defined still exists, except the
  `q_shared.c` math and byte-order helpers (now the engine's), the old savegame
  reader/writer (rewritten), and `G_TouchSolids`/`PrintPmove`/`CheckBlock`,
  which were dead in the original and which Q2PRO removed.
* Each auto-resolved deletion of a region tourney had gutted was logged and
  reviewed; the 39 of them are the monster helpers, `Cmd_PlayerList_f`, the
  baseq2 scoreboard and the coop key-stripping loop.

Warnings
--------

Compiled with `-Wall -Wextra` plus Q2PRO's own warning set, the original tree
produces 663 warnings and this one produces 515. No category is above its
original count.

What is gone: 111 pointer-to-int casts, 16 non-exhaustive switches, 7 ignored
return values, and the uninitialised-use reports. What was found and fixed
along the way, all of it exposed by Q2PRO's `q_printf` annotations on the game
import table or by its const-ification:

* six places passing a runtime string as a format — `gi.centerprintf(ent,
  message)` and friends — each a format-string hole
* `"\"%s\" has already been invited.\n"` in `osp_teams.c` with no argument at
  all for the `%s`
* four `%d` conversions fed a pointer difference or an `unsigned long`
* `PrecacheItem` still parsing a space-separated string after the item table had
  moved to precache arrays, which would have read the array as text

What remains is pre-existing tourney code the port does not touch: 335 old-style
`foo()` prototypes, 66 `sprintf` overflow warnings, 20 `strncpy` truncations and
a scattering of dead locals and labels.

Building
--------

Unchanged from the reconstruction:

    make                # native debug + release
    make build_release  # release only

Output is `game<cpu>.so`, which is what Q2PRO looks for. `config.h` is Q2PRO's
build configuration for a standalone game library and `shared/` holds the engine
headers the game links against; both are vendored verbatim so the ABI matches
whatever engine loads the library.

The Windows cross builds and the two `asm_matching/` oracles are not wired up on
this branch. They belong to `main`, where they still mean something.

Security
--------

The reconstruction deliberately keeps tourney's original bugs, including its
security holes. This branch does not: Q2PRO's fixes to the shared code came
across with everything else, the format-string problems above are fixed, and so
are the two overruns. tourney's *own* string handling has not been audited, so
this is safer than the reconstruction but not audited-safe. Do not put it on a
public server without reading `docs/SECURITY_REVIEW.md` on `main` first — most
of what it lists is tourney's own code and still applies here.
