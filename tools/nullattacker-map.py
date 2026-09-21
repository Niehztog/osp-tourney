"""Build the map fixture that reproduces the NULL-attacker crash.

WHAT IT IS FOR.  `T_Damage` and `T_RadiusDamage` normalise a NULL `attacker`
to `world` (see tools/check-null-attacker.sh for the contract and g_combat.c
for the reasoning).  This script builds the map that proves the crash was real
and that the two lines answer it, so the fix is not a reading of the source.

THE PRODUCER IS ID'S, UNCHANGED.  Nothing in g_func.c ever assigns `activator`
on a func_door, so a door that reverses because something blocked it hands on
the zero it was spawned with:

    door_blocked() -> door_go_up(ent, ent->activator) -> G_UseTargets()
    -> a targeted target_explosion -> T_RadiusDamage(self, self->activator, ...)

WHAT THE FIXTURE ADDS, all of it in a q2pro entity-string override, with no
change to any map geometry -- every brush it uses is already in the BSP:

  * every info_player_deathmatch is replaced by ONE, so the client spawns
    where the fixture wants it and nowhere else;
  * a func_door built from an existing brush model, translated by its `origin`
    key so its CLOSED position is that spawn point and its open position is
    clear above it;
  * a trigger_always that opens the door 0.2 s into the level, before any
    client has connected -- which is what lets the client spawn into the
    doorway rather than inside a closed door;
  * the door's `wait`, which closes it again on top of the standing client;
  * four target_explosions ringing the spawn point, far enough out that
    CanDamage's traces do not start inside the player's bounding box and close
    enough to be inside their own blast radius.

The client then does nothing at all.  It stands where it spawned.

RUNNING IT.

    tools/nullattacker-map.py -o /tmp/q2dm8.ent
    cp /tmp/q2dm8.ent <server>/tourney/maps/q2dm8.ent
    q2proded +set game tourney +set deathmatch 1 +set map_override_path maps \
             +map q2dm8

Then connect a client, `join`, and stand still.  `map_override_path` is q2pro's;
Yamagi has no entity-string override, so the fixture is q2pro-only -- the DEFECT
is not, and neither is the fix.  tools/playtest-scenarios/nullattacker drives
all of this with a headless client and asserts on both signs.

WHAT IT LOOKED LIKE.  Measured on q2dm8, spawn 2, model *3, against the tree at
839f0f1 with the debug build under gdb:

    Program received signal SIGSEGV, Segmentation fault.
    T_RadiusDamage (inflictor=0x..., attacker=0x0, damage=60, ignore=0x0,
                    radius=100, mod=25) at g_combat.c:482
    482   if (ent->client && attacker->client && (ent != attacker) && ...
    #1 target_explosion_explode (self=0x...)            at g_target.c:234
    #2 use_target_explosion (..., activator=0x0)        at g_target.c:249
    #3 G_UseTargets (ent=0x..., activator=0x0)          at g_utils.c:218
    #4 door_go_up (self=0x..., activator=0x0)           at g_func.c:948
    #5 door_blocked (self=0x..., other=0x...)           at g_func.c:1105
    #6 SV_Physics_Pusher (ent=0x...)                    at g_phys.c:555

and with the fix, the same run: the door reverses, the four explosions damage
the client for 37 each, it dies ("blew up" -- MOD_EXPLOSIVE with no attacker,
which is the obituary a NULL produced too), and the server stays up.
"""
import argparse, struct, sys

def pakfile(pak, want):
    with open(pak, 'rb') as f:
        magic, dirofs, dirlen = struct.unpack('<4sii', f.read(12))
        if magic != b'PACK':
            raise SystemExit(f'{pak}: not a pak')
        f.seek(dirofs)
        for _ in range(dirlen // 64):
            rec = f.read(64)
            name = rec[:56].split(b'\0')[0].decode()
            pos, ln = struct.unpack('<ii', rec[56:])
            if name == want:
                f.seek(pos)
                return f.read(ln)
    raise SystemExit(f'{want} not in {pak}')

def lumps(bsp):
    return [struct.unpack('<ii', bsp[8 + 8 * i:16 + 8 * i]) for i in range(19)]

def entstring(bsp):
    o, l = lumps(bsp)[0]
    return bsp[o:o + l].split(b'\0')[0].decode('latin-1')

def model(bsp, n):
    o, l = lumps(bsp)[13]
    rec = bsp[o + 48 * n:o + 48 * n + 48]
    mins = struct.unpack('<3f', rec[0:12])
    maxs = struct.unpack('<3f', rec[12:24])
    return mins, maxs

def blocks(ents):
    out, cur = [], None
    for line in ents.splitlines():
        line = line.strip()
        if line == '{':
            cur = []
        elif line == '}':
            out.append(cur)
            cur = None
        elif cur is not None and line:
            cur.append(line)
    return out

def keyval(block, key):
    for line in block:
        if line.startswith(f'"{key}"'):
            return line.split('"')[3]
    return None

def emit(block):
    return '{\n' + '\n'.join(block) + '\n}\n'

def main():
    ap = argparse.ArgumentParser()
    # pak1 is where id put the deathmatch maps; any retail copy will do.
    ap.add_argument('--pak', default='/usr/share/games/quake2/baseq2/pak1.pak',
                    help='retail pak holding the map')
    ap.add_argument('--map', default='q2dm8')
    ap.add_argument('--model', type=int, default=3, help='brush model to borrow as the door')
    ap.add_argument('--spawn', type=int, default=2, help='which info_player_deathmatch to keep')
    ap.add_argument('--ring', type=float, default=44.0, help='target_explosion ring radius')
    ap.add_argument('--dmg', type=int, default=60)
    ap.add_argument('--wait', type=int, default=12, help='seconds the door stays open')
    ap.add_argument('--open-at', type=float, default=0.0,
                    help='delay on the trigger_always; 0 means "before anyone connects"')
    ap.add_argument('-o', default='-')
    a = ap.parse_args()

    bsp = pakfile(a.pak, f'maps/{a.map}.bsp')
    ents = entstring(bsp)
    mins, maxs = model(bsp, a.model)
    size = [maxs[i] - mins[i] for i in range(3)]

    bs = blocks(ents)
    spawns = [b for b in bs if keyval(b, 'classname') == 'info_player_deathmatch']
    if a.spawn >= len(spawns):
        raise SystemExit(f'{a.map} has {len(spawns)} deathmatch spawns')
    px, py, pz = (float(v) for v in keyval(spawns[a.spawn], 'origin').split())

    # The door's CLOSED box: centred on the spawn in x/y, its floor 96 below
    # the spawn origin, so the descent crosses the whole standing player.
    dx = (px - size[0] / 2) - mins[0]
    dy = (py - size[1] / 2) - mins[1]
    dz = (pz - 96) - mins[2]
    # `angle -1` is straight up; `lip` negative so the open position clears a
    # standing player by a wide margin (travel = height - lip).
    lip = -128

    out = []
    for b in bs:
        if keyval(b, 'classname') == 'info_player_deathmatch':
            continue
        out.append(emit(b))

    out.append(emit([
        '"classname" "info_player_deathmatch"',
        f'"origin" "{px:.0f} {py:.0f} {pz:.0f}"',
        f'"angle" "{keyval(spawns[a.spawn], "angle") or 0}"',
    ]))
    out.append(emit([
        '"classname" "func_door"',
        f'"model" "*{a.model}"',
        f'"origin" "{dx:.0f} {dy:.0f} {dz:.0f}"',
        '"targetname" "crusher"',
        '"target" "boom"',
        '"angle" "-1"',
        '"speed" "100"',
        f'"lip" "{lip}"',
        f'"wait" "{a.wait}"',
        '"dmg" "1"',
    ]))
    always = [
        '"classname" "trigger_always"',
        f'"origin" "{px:.0f} {py:.0f} {pz:.0f}"',
        '"target" "crusher"',
    ]
    if a.open_at:
        always.append(f'"delay" "{a.open_at}"')
    out.append(emit(always))
    for ex, ey in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        out.append(emit([
            '"classname" "target_explosion"',
            '"targetname" "boom"',
            f'"origin" "{px + ex * a.ring:.0f} {py + ey * a.ring:.0f} {pz:.0f}"',
            f'"dmg" "{a.dmg}"',
        ]))

    text = ''.join(out)
    sys.stderr.write(
        f'{a.map}: spawn {a.spawn} at {px:.0f} {py:.0f} {pz:.0f}; '
        f'door model *{a.model} size {size[0]:.0f}x{size[1]:.0f}x{size[2]:.0f} '
        f'translated by {dx:.0f} {dy:.0f} {dz:.0f}, travel {size[2] - lip:.0f}\n')
    if a.o == '-':
        sys.stdout.write(text)
    else:
        open(a.o, 'w').write(text)

main()
