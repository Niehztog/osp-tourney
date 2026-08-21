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
`botlib.h` and `anorms.h` are excluded from the *formatting* passes only. (The
RSA reference `md5c.c`/`md5.h` was in that list too; it is gone with the
NetGames USA logging that was its only caller.) They keep their upstream shape — including the
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

Not Q2PRO's
-----------

Three decisions that are this branch's own rather than replayed from anywhere:

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
  `if (++found == 2)` now, and both `sprintf`s in it are bounded — see **Bugs
  and security fixes** below, which is that end-to-end audit.

* **Team names from cvars are truncated to 15 characters.** `team_a_name` and
  `team_b_name` were `strcpy`'d into a 32-byte field; every other write to that
  field caps at 15, the `teamname` command squeezes its argument down to 15
  non-space characters, the scoreboard cell is `%15s`, and `team <name>` only
  ever compared the first 15. The cvar path is the odd one out, so it caps at
  15 too now.

NetGames USA
------------

tourney v2.75 shipped with three files' worth of NetGames USA logging:
`nglog.c` (the ngLog / ngWorldStats file writer), `ngmark.c` (the ngWorldStats
verification mark) and `q2log.c` (the event formatter), plus the RSA reference
MD5 implementation the mark was built on. It wrote two logs at once: a plain
ngLog 1.2 text log, and a second copy of the same lines XOR-scrambled a byte at
a time with an MD5 running checksum appended at close, so that ngWorldStats
could tell a doctored log from a real one.

None of that has anywhere to go any more. `Quake2.ngWorldStats.com` and the
ngStats post-processors have been offline for over twenty years, and the code
was not idle in their absence:

* `ngLog_logClose` ran `system("<cwd>/NetGamesUSA.com/ngWorldStats/bin/ngWorldStats -d ... &")`
  at the end of every map, with two unquoted 1024-byte paths built by
  `sprintf` into 1024-byte buffers.
* `ngLog_ngStatsCall` did the same for `ngStatsQ2T`, and the admin menu had a
  row that paused the whole server for a hundred thousand seconds while the
  listen-server host looked at the results.
* `ClientBeginDeathmatch` stuffed
  `cmd _ngws_client_id $ngWorldStats_password $ngworldstats_password` to every
  connecting client, i.e. it asked each player to send back the contents of
  their own ngWorldStats account password, in cleartext, to whichever server
  they happened to join.
* `q2log_clientid_cmd` took the answer with `strncpy(dst, gi.argv(1), 16)` into
  a field it never terminated.

**The mod's own remaining logging does not cover what it recorded.** The
Standard Log (`stdlog.c`, `sl_log_method`) writes one line per kill, connect,
disconnect and rename plus a map header, and nothing else: no item pickups, no
accuracy, no votes, no team events, no match settings, no observer/chasecam
transitions. And it was mutually exclusive with ngLog — `OSP_gameInit` cleared
`sl_log_method` outright whenever `nglog_logstyle` was set. The admin log
(`server_adminlog`) is a referee audit trail, not statistics.

So the events are kept, and written locally instead. `osp_stats.c` takes every
event the ngLog layer formatted — match settings, connect/enter/leave/rename,
observer and chasecam transitions, team joins and renames, votes proposed and
decided, item pickups, uses, drops and expiries, kills, suicides and team
kills, and the full per-weapon accuracy table — and appends one JSON object per
line to a file in the game directory:

    statsfile           0 = off, 1 = on (default 1)
    statsname           file name (default osptourney.jsonl)
    stats_logchat       log chat lines (default 0)
    stats_logallpickups log every pickup, not just the powerful ones (default 0)

One object per line, appended, never rewritten: safe against a server dying
mid-match, greppable, and readable by anything that can parse JSON. Player,
team and map names come off the network or out of a config file, so everything
outside printable ASCII is `\u`-escaped — tourney's "green" names are ordinary
text with 0x80 added to every byte, and a stray quote in a player name cannot
corrupt the file. No new libraries are linked in; the game still depends on
nothing but libc and libm, and with `nglog.c` gone nothing in the tree calls
`system()`, `gethostbyname()` or `ftime()` any more.

The Standard Log is otherwise untouched and still does what it did. It is not
NetGames USA code — StdLog 1.2 is a different and unrelated format that merely
borrowed ngLog's file writer; that writer now lives in `sl_write.c` itself, so
the two logs are independent and both can be on at once.

Gone with the subsystem: 17 cvars (`nglog_logstyle`, `nglog_logstyle_working`,
`nglog_logname`, `nglog_flush`, `nglog_buffer`, `nglog_logchat`,
`nglog_logallpickups`, `nglog_logmiscpickup`, `nglog_worldstats`,
`nglog_ngstats_exec`, `nglog_ngstats_browser`, `nglog_ngstats_cfg`,
`nglog_ngstats_logdir`, `nglog_ngstats_vidrestart`, `ngWorldStats_Status`,
`__dummy_nglog_name` and the `version` lookup the log header used), one client
command (`_ngws_client_id`), the `ngWorldStats_password` field on
`client_respawn_t`, the "View ngStats (browser)" row in both menus, the
`who_paused == -3` pause state it used, and the `Stats at:
http://Quake2.ngWorldStats.com` scoreboard banner with its status bar slot.
`docs/server-settings.txt` and `docs/commands.txt` are updated to match, as are
the fifteen shipped configs.

Bugs and security fixes
-----------------------

`docs/SECURITY_REVIEW.md` on `main` is a full-source review of the
reconstruction. It exists because the reconstruction reproduces tourney's
security holes on purpose — byte-exactness means reproducing what the original
got wrong. This branch is not the reconstruction, and it does not keep them.

Everything that review lists as remotely reachable memory corruption is fixed
here, and so is everything the audit it asked for turned up afterwards.

**Remotely reachable memory corruption.**

* `OSP_kickplayer_cmd` copied the raw command tail into a 32-byte stack buffer
  with `strcpy`. Any team captain — and the first human on a team becomes one
  automatically — could smash the stack with `kickplayer <400 characters>`.
* `OSP_vote_cmd` copied the vote's value into `vote_value[64]` with `strcpy`.
  The `hook`, `runes`, `bfg` and `quad` arms validate nothing, so
  `vote hook <1000 characters>` overran it by nearly a kilobyte. Any player.
* The same function wrote the target's client id back over `gi.argv(2)` with
  `sprintf`, i.e. into the engine's own tokenizer buffer, past the end of a
  short token.
* `OSP_teamskin_cmd` did `strcpy(teams[t].skin, gi.argv(1))` into a 128-byte
  field and then `sprintf`'d the same token twice into a 256-byte one. A token
  is up to `MAX_TOKEN_CHARS`.
* `ClientConnect` and `OSP_getPlayerAddr` both `strcpy`'d the client's address
  into `edict_t::osp_e37c`, which is 32 bytes with the **referee flag** as the
  next field. An address longer than 31 characters — an IPv6 literal, which
  Q2PRO supports — granted referee status on connect.
* `OSP_sayteam_cmd` expanded `%l`, `%a`, `%h`, `%w`, `%n`, `%r` and `%t` into a
  1024-byte buffer with `strcpy`, checking only that the *start* of each write
  was still inside it. `say_team %n%n%n%n…` with enough visible players ran off
  the end.
* `OSP_startDemos` built a demo name from six `%s` fields — two team names, an
  unbounded `demo_tag` cvar and a `MAX_QPATH` map name — into `char[100]`, then
  re-formatted it in place with `"record "` and `"\n"` wrapped round a
  100-byte sanitised copy.
* `OSP_addTeamMember`, `OSP_defaultTeam` and `OSP_readdTeamMember` `sprintf`'d
  `"skin %s\n"` from a 128-byte team skin into 64- and 164-byte buffers, and
  copied a 128-byte skin into an 80-byte edict field. `OSP_defaultTeam` also
  read 48 bytes past that field with `strncpy(dst, src, 128)`.
* `read_player_entry` filled `players.txt`'s exactly-sized destinations with
  exactly-`n` `strncpy`s and no terminator, so an over-long field ran on into
  the next row of the 200-entry arrays. The same unterminated pattern was in
  `OSP_teamjoin_cmd`, `OSP_joincode_cmd`, `OSP_defaultjoincode_cmd`,
  `OSP_playerAllow` and all four `r_ban`/`r_unban` commands, and in
  `default_timelimit`/`default_fraglimit`/`default_hook`, which are 8 bytes
  each and were filled with `strncpy(dst, src, 8)`.
* `OSP_readLine` copied a 1023-byte high-score line into 64-byte locals with
  `strcpy`, which were then copied into 16-byte table fields.
* Six filesystem paths were built by unbounded `sprintf` into `char[64]`
  (`osp_config.c`, `osp_maps.c`, `osp_plist.c`, `osp_display.c`,
  `osp_hiscore.c` twice). They are `MAX_OSPATH` and `Q_snprintf` now.

**Crashes and out-of-bounds accesses.**

* `OSP_votePercent` divided by a head count that reaches zero as soon as the
  last human leaves a server with bots on it — SIGFPE, and it is called every
  frame while a vote is running.
* `OSP_updateHighScores` divided by `fraglimit` without testing it, which a
  referee can set to zero with `r_fraglimit 0`, and read `rowi` uninitialised
  when no client was in first place.
* `OSP_specbot_vote` accepted a bot index equal to the list length, walked one
  past the end and dereferenced NULL.
* `OSP_1v1Remove` wrote `p_order[-1]` when the queue was empty.
* `OSP_DoRankSort` scanned `i <= game.maxclients` and read
  `game.clients[game.maxclients]`.
* `OSP_showScores` and the three team scoreboards in `osp_players.c` capped
  their page with `>` where the terminating NUL needs `>=`.
* `OSP_showScores` dereferenced `chase_target` in three arms without testing
  it; the fourth arm tests it, which is how the omission shows.
* `OSP_1v1QueueCheck` read `->client->resp` before testing `->client`.
* `teams[]` has two entries and `resp.team` is 2 for a client on no team;
  `p_camera.c`, `g_chase.c`, `g_combat.c`, `osp_hook.c`, `osp_menus.c` and
  `ClientUserinfoChanged` all indexed it without checking. There is one
  `OSP_teamNameFor()` for that now.
* `p_acc[]` is indexed by a client id that is `-1` until
  `ClientBeginDeathmatch` hands one out; five sites did not check.
* `OSP_setMOTD` read `fgetc` into a `char` and compared it against `-1`, which
  is never true where `char` is unsigned. On arm and aarch64 — both of which
  Q2PRO builds for — the skip-to-end-of-line loop never terminated.
* `OSP_menuVotePercent` cast a 32-byte menu text buffer to `edict_t *`, passed
  it as an entity, and `sprintf`'d through it. It takes a `char *` and a size
  now.
* `g_spawn.c`'s `SpawnEntities` declared a local `extern int botglobals;` and
  assigned through it, which zeroed the first `int` of the bot SDK's
  `bot_globals_t`. That is `numbots`, so it says so now.
* `ClientUserinfoChanged` stored a frame number in the Gladiator SDK's
  `char *charname` and cast it back through `int`, which does not round-trip on
  a 64-bit build. It has its own `int` field now.
* `NextMap`'s random map picker called `srand()` and then drew from `Q_rand()`,
  which `srand()` does not feed. It calls `Q_srand()`.
* `OSP_playerAllow` scanned `i < game.maxclients` for a duplicate name, so the
  last client slot was never checked, and matched ban addresses by plain
  prefix, so banning `1.2.3.4` also banned `1.2.3.40` through `1.2.3.49`.
* `OSP_joincode_cmd` announced a new joincode to `t < game.maxclients`.
* `OSP_recoverClient` passed a client index to `OSP_teamCount()`, which takes a
  team number.
* `sl_WriteStdLogDeath` passed NULL to `%s` when the world killed a player with
  a cause it had no name for, tested `FL_OSP_BOT` where its two siblings test
  `FL_BOT`, and did not check that the victim was a client at all.
* `OSP_talkto_cmd` carried its own copy of vanilla's flood check, including the
  out-of-range `flood_when[]` index Q2PRO fixed in `FloodProtect()`. It calls
  `FloodProtect()`.
* `OSP_teamskin_cmd`'s bot arm rewrote the *caller's* userinfo rather than the
  bot's, with a skin indexed by the client loop counter rather than by the
  team — walking off the end of `teams[]` for every client past the second.

**Hidden and hostile behaviour.**

* `ClientConnect` built the string `\name\` byte by byte, so that it never
  appeared in the shipped binary's `.rodata`, and tested whether a client's
  userinfo happened to *begin* with the name key. The result was sticky for the
  whole connection, and its one consumer kicked the player with
  `"%s was kicked for using a BOT!"` the first time they fired. Userinfo key
  order is a consequence of client cvar registration order, so this is a test
  of which engine build somebody runs, not of anything they did — any engine
  that differs from whatever OSP tested against has every one of its users
  auto-banned and publicly accused on their first shot. It is gone, along with
  the `client_respawn_t` field it set.
* `OnBotDetection` and `OSP_speedDetect` both ended by unicasting a *random*
  `svc_*` command byte followed by zero to two random operand bytes, and only
  then a `svc_disconnect` — a deliberate attempt to desync the receiving
  client's protocol parser. Both send the disconnect and nothing else now.
* `OSP_isreferee_cmd` compared the referee password with `strncmp` over
  `strlen(password)`, so anything starting with the password authenticated, and
  *disconnected* a client whose password did not match — so a stale
  `ref_passwd` in somebody's config locked them out of the server entirely. It
  is a whole-string `strcmp` and a silent refusal now. It also reached
  `greenname` by indexing 16 bytes past `netname`.
* `OSP_speedDetect` passed an `int` to a `%f` conversion in the admin log.

**Timer units.** The `nextthink` and `timers` passes in the table above retype
every timer from a float count of seconds to an `int` count of frames. That
makes a field's type stop saying what its unit is: `int nextthink` and
`int timestamp` hold frames, `float level.time` holds seconds, and mixing them
compiles silently and runs wrong by a factor of ten. A mechanical pass can
convert a write without its matching read, or miss a file its diff never
touched, and nothing complains -- not the compiler, not a boot test, not an
entity census, because none of those waits for a think.

`g_phys.c`'s `SV_RunThink` was missed outright. It kept `float thinktime` and
compared `ent->nextthink` -- an `int` frame count since `g_local.h` -- against
`level.time`, so **every think in the mod fired ten times late**: doors, plats,
item respawns, trigger delays, the whole match state machine. Measured with a
breakpoint: an item scheduled for `level.framenum + 2` fired on frame 20.
Q2PRO's own Ground Zero pack had the identical defect, in the identical
function, from the identical replay; `doc/mission-packs.md` there records it.

Six more sites were converted on one side only. Each is restored to the form
baseq2 has for the same expression:

* `Cmd_Kill_f` tested `(level.time - respawn_framenum) < 5`. The difference is
  always negative once the frame counter has run for a second, so **`kill` was
  refused for good** outside a live match.
* `target_lightramp_think` computed its lightstyle character *and* its loop
  condition from `(level.time - timestamp) / FRAMETIME`, so the character was
  garbage and the think never terminated.
* `misc_viper_bomb_prethink` scaled the bomb's velocity by
  `timestamp - level.time`.
* `Weapon_HandGrenade` derived both the fuse and the throw speed from
  `grenade_framenum - level.time`.
* `ClientThink`'s post-respawn reset tested
  `respawn_framenum + 0.2f < level.time`, which is never true.

These are masked by `SV_RunThink` in the *opposite* direction, so fixing either
half alone moves the breakage rather than removing it. They land together.

Two checks find the class. Cross-unit mixing: for every line mentioning
`level.time`, look for an `int`-declared timer field on the same line. Lost
scale: grep for `level.framenum` next to a floating-point literal with no
`* BASE_FRAMERATE`. Neither catches a `SV_RunThink` written wholly in the wrong
unit, and only one thing does -- **run a level and wait**. Break on a think and
print `level.framenum`; the frame it fires on must be the frame it was
scheduled for.

**Three more, found the same way.** `bl_main.c` called `abs()` on a `vec3_t`
element to build the bots' view-relative `upmove`; `abs()` takes an `int`, so
any magnitude below 1 truncated to zero and **bots could never swim up or
down**. `p_menu.c`'s menu handle is a libc allocation hanging off `gclient_t`
outside `pers`, and nothing closed it on `ClientDisconnect` while
`PutClientInServer` memsets the part of the struct it lives in -- so it leaked
on every disconnect and every spawn, including the spawn every client gets on a
level change. And `g_local.h` declared `OSP_showMOTD`, `OSP_showParams` and
`OSP_showHighScores` as `()` rather than with prototypes, which switched off
argument checking: the first two are called with no argument where the
definition takes an `edict_t *`, and the third is called *with* one where the
definition takes none. Harmless only because no body uses the parameter, and a
hard error under C23. All three are `(void)` now, at the declaration, the
definition and the call.

**Hardening.** The Makefile's `-fno-stack-protector -D_FORTIFY_SOURCE=0` is
gone; the release build is `-D_FORTIFY_SOURCE=2 -fstack-protector-strong`. That
was only possible once every unbounded copy into a fixed buffer had become a
bounded one, which is what most of the list above is.

**What is not fixed.** Two things in that review are design decisions rather
than defects, and changing them would remove documented behaviour rather than
fix it:

* The server still stuffs `cmd _is_referee $ref_status $ref_passwd` to every
  client, so referee auto-login still crosses the network in cleartext and any
  server can collect the password its visitors use elsewhere. The two concrete
  defects in that path are fixed; the mechanism itself is `referee_enable`'s
  documented behaviour. Set `referee_enable 0` if that matters to you.
* `client_botdetect`'s speed check asks the client to report its own
  `timescale` and believes the answer, so it detects nothing and only accrues
  strikes against honest players who left `timescale` above 1 in a config. It
  no longer sends a corrupt protocol payload when it fires, but it is still
  security theatre.

What is checked
---------------

* All 40 baseq2 item-list entries match Q2PRO's field for field; tourney's five
  runes and two dummy flag items are the only additions, and no baseq2 item is
  missing.
* All 108 spawn classnames survive, and all of tourney's client-command strings
  except `_ngws_client_id` — 137 of the original 138. Of the cvars, 17 are gone
  with the NetGames USA logging and four are new (`statsfile`, `statsname`,
  `stats_logchat`, `stats_logallpickups`); everything else survives.
* `g_ptrs.c` regenerates byte-identical from Q2PRO's own `genptr.py` over this
  tree.
* The JSON stats log round-trips: every record `osp_stats.c` can write parses
  as JSON, including player, team and map names carrying quotes, backslashes
  and high-bit "green" bytes.
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
`foo()` prototypes and a scattering of dead locals.

The bug and security work on this branch moved that again. Measured with plain
`-Wall -Wextra` over every `.c` file, the branch produced 380 warnings before it
and 330 after. What went: every `sprintf` overflow and `strncpy` truncation
report in tourney's own code, both pointer/int casts, seventeen implicit
function declarations (the `bl_*` headers are included where their functions
are called now), a dozen dead locals, three dead labels and eight `abs()` calls
on a float. What is left is 202 unused parameters, 102 signed/unsigned
comparisons and 17 missing field initialisers, none of them in code this branch
changes.

**The Makefile builds with `-Wall` now, and the tree is clean under it on both
gcc and clang.** It used to build with `-w`, and that is how the timer-unit
defects below went unmeasured for a whole audit: `-w` silences the compiler
about code nobody is reading either. The residual `-Wall` reports that stood in
the way are gone with it -- the four set-but-unused locals in vanilla
`g_items.c`/`g_weapon.c` (deleted, with a note where each one was, since this
branch is not address-matched), the two `-Wpointer-sign` and the `#endif` label
in the Gladiator SDK, `SelectCoopSpawnPoint` and two invented-name statics
(tagged `q_unused` like the rest of the dead-but-kept code) -- and enabling it
immediately found a real one: `SpawnEntities` saved and restored
`edict_t::osp_e3b0` with `strncpy(dst, src, 127)` into an 80-byte field, 48
bytes past its end, on every level load. That is the same field and the same
mistake the `OSP_defaultTeam` entry above records, at a second site the audit
missed. All three save/restore pairs take their length from the field now.

`-Wextra` is still not gated on: it reports the 335 old-style `foo()`
prototypes tourney's own code is written in, which is a rewrite rather than a
fix.

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
security holes. This branch does not — see **Bugs and security fixes** above for
what was found and what was done about it. Q2PRO's fixes to the shared code came
across with everything else, tourney's own string handling has now been audited
end to end, and the Makefile builds with `_FORTIFY_SOURCE` and the stack
protector on rather than off.

`docs/SECURITY_REVIEW.md` on `main` still describes the reconstruction
accurately, and is worth reading for context; every finding in it that is a
defect rather than a design decision is fixed here, and the two design decisions
it lists that are not are called out above.

That audit was source review, not testing: this tree has been built, its warning
set measured, its stats log driven through a harness and its shared library
loaded, but it has **not** been run against a live server with real clients.
Treat it as materially safer than the reconstruction rather than as proven.
Known-unchanged: the vanilla files keep their original behaviour except where
Q2PRO changed them, `shared_shared.c` is vendored verbatim, and the Gladiator
Bot SDK's own string handling (`bl_spawn.c`'s 32-byte bot-name copies, reachable
only when `serveronlybotcmds` is 0, which tourney does not default to) has not
been touched.
