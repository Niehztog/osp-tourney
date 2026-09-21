#!/bin/sh
#
# check-null-attacker.sh -- fail the build if the damage path reads `attacker`
# before it has answered the case where there is not one.
#
# An `edict_t *attacker` that is legitimately NULL reaches T_Damage and
# T_RadiusDamage.  Nothing in g_func.c ever assigns `activator` on a func_door,
# so a door that reverses because something blocked it hands on the zero it was
# spawned with:
#
#   door_blocked() -> door_go_up(ent, ent->activator) -> G_UseTargets()
#   -> a targeted target_explosion -> T_RadiusDamage(self, self->activator, ...)
#
# id's own T_Damage survives that by accident: its one `attacker->client` read
# sits behind `!(dflags & DAMAGE_RADIUS)`, which short-circuits on exactly the
# path that produces the NULL.  That is a coincidence between one flag test and
# one call site, not a guard, and nothing added since inherits it -- the
# match-mode friendly-fire arm, the runes, and the ten reads in
# T_RadiusDamage's accuracy block all dereference before testing.  So both
# functions normalise once, at the top:
#
#     if (!attacker)
#         attacker = world;
#
# `world` is g_edicts[0] and its `client` is NULL, so every downstream
# `attacker->client` test still answers what the missing attacker meant.
#
# THIS CHECK EXISTS BECAUSE THAT IS A TWO-LINE INVARIANT WITH TWENTY SILENT
# BENEFICIARIES.  Delete it and the tree still builds, still boots, still passes
# every other check, and dies on the first map in the rotation with a
# target_explosion on a door -- measured, on this tree, as a SIGSEGV at
# g_combat.c's accuracy block with `attacker=0x0` in the frame.
#
# WHAT IS A FINDING.  Three things, per function:
#
#   * the normalisation missing altogether;
#   * any use of `attacker` ABOVE it -- which is the same crash with the fix
#     still in the file;
#   * a substituted value that is not `world`, since `world->client` is what
#     makes every downstream test still answer correctly.
#
# WHAT IS NOT.  Anything below the normalisation, guarded or not: that is the
# point of having it.  Comments and string literals are blanked first, so this
# file's own prose and g_combat.c's are not findings.
#
# Usage:
#   check-null-attacker.sh FILE...     check those files, exit 1 on a violation
#   check-null-attacker.sh --self-test run the control (see run_self_test below)

set -e

self="$0"
dir=$(dirname "$self")

# ---------------------------------------------------------------------------
# The scanner.
#
# Comments and string/char literals are blanked by a character state machine
# before matching -- without that, the explanation above T_Damage's own
# normalisation, which names `attacker->client` twice, would be a finding.
# Function bodies are then followed by brace depth, so only the two functions
# that take an `attacker` are inspected and only between their braces.
# ---------------------------------------------------------------------------
scan() {
    awk '
    function blank(line,   out, i, n, c, d) {
        out = ""
        n = length(line)
        i = 1
        while (i <= n) {
            c = substr(line, i, 1)
            d = substr(line, i, 2)
            if (state == "code") {
                if (d == "/*") { state = "block"; i += 2; continue }
                if (d == "//") { break }
                if (c == "\"") { state = "str"; i++; continue }
                if (c == "'"'"'") { state = "chr"; i++; continue }
                out = out c
                i++
                continue
            }
            if (state == "block") {
                if (d == "*/") { state = "code"; i += 2; continue }
                i++
                continue
            }
            if (state == "str") {
                if (c == "\\") { i += 2; continue }
                if (c == "\"") { state = "code" }
                i++
                continue
            }
            if (state == "chr") {
                if (c == "\\") { i += 2; continue }
                if (c == "'"'"'") { state = "code" }
                i++
                continue
            }
        }
        return out
    }
    function report(msg) { printf "%s:%d: %s\n", FILENAME, FNR, msg; bad++ }
    BEGIN { state = "code"; depth = 0; fn = ""; bad = 0; seen = 0; header = 0 }
    FNR == 1 { state = "code"; depth = 0; fn = ""; header = 0 }
    {
        line = blank($0)
        gsub(/[ \t]+/, " ", line)
        gsub(/^ | $/, "", line)

        # a definition, not a declaration and not a call
        if (fn == "" && line ~ /^(static )?void (T_Damage|T_RadiusDamage) ?\(/ && line !~ /;[ ]*$/) {
            fn = line
            sub(/^(static )?void /, "", fn)
            sub(/ ?\(.*$/, "", fn)
            seen = 1
            ok = 0
            depth = 0
            header = 1   # the header names the parameter; that is not a read
        }
        if (fn == "") next

        # follow the body by brace depth; the header line opens none
        opened = gsub(/{/, "{", line)
        closed = gsub(/}/, "}", line)

        if (!header && !ok && line ~ /(^| |[^A-Za-z0-9_])attacker([^A-Za-z0-9_]|$)/) {
            if (line ~ /attacker = [A-Za-z_][A-Za-z0-9_]*/) {
                rhs = line
                sub(/^.*attacker = /, "", rhs)
                sub(/[^A-Za-z0-9_].*$/, "", rhs)
                if (rhs == "world")
                    ok = 1
                else
                    report(fn "() normalises a NULL attacker to `" rhs "`, not `world`")
            } else if (line !~ /^if \( ?! ?attacker ?\)( ?{)?$/) {
                report(fn "() reads `attacker` above the NULL boundary: " line)
                ok = 1   # one finding per function is enough to say it
            }
        }

        header = 0
        depth += opened - closed
        if (depth <= 0 && opened + closed > 0) {
            if (!ok)
                report(fn "() has no `if (!attacker) attacker = world;` boundary")
            fn = ""
        }
    }
    END {
        if (!seen) {
            # Not pedantry: if the two functions are renamed or moved out of
            # the files the Makefile passes, a check that silently found
            # nothing would report the tree as clean for ever after.
            printf "check-null-attacker: neither T_Damage nor T_RadiusDamage was found in the files scanned -- did they move?\n"
            bad++
        }
        exit (bad > 0)
    }
    ' "$@"
}

# ---------------------------------------------------------------------------
# The control.
#
# A check that is never seen to fail is not evidence of anything.  So the
# control drives it in both directions: it restores each of the three defect
# shapes in a real copy of g_combat.c and requires a hit on each, it requires
# prose about `attacker->` to stay silent, and it requires the real tree to
# pass -- that last one is what stops a checker that always fails from
# satisfying the first three.
# ---------------------------------------------------------------------------
run_self_test() {
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT

    rc=0
    pass() { printf '  ok      %s\n' "$1"; }
    fail() { printf '  FAILED  %s\n' "$1"; rc=1; }

    src="$dir/../g_combat.c"

    # Positive control 1: the boundary deleted from both functions, which is
    # the donor's own state and the one this check exists for.
    sed '/^        attacker = world;$/d' "$src" > "$tmp/gone.c"
    if ! cmp -s "$src" "$tmp/gone.c"; then
        out=$(scan "$tmp/gone.c" 2>&1) && fail "boundary deleted -- check did not fire" || {
            # With the boundary gone the first unguarded read is what gets
            # named -- which is the donor's own shape and the right diagnosis.
            n=$(printf '%s\n' "$out" | grep -c ':' || true)
            case $out in
                *"T_Damage()"*) d=1 ;; *) d=0 ;;
            esac
            case $out in
                *"T_RadiusDamage()"*) r=1 ;; *) r=0 ;;
            esac
            if [ "$n" -eq 2 ] && [ "$d" -eq 1 ] && [ "$r" -eq 1 ]; then
                pass "boundary deleted -- check fires on both functions"
            else
                fail "expected one finding per function, got: $out"
            fi
        }
    else
        fail "could not delete the boundary -- has g_combat.c been reformatted?"
    fi

    # Positive control 2: the boundary still there, but a read placed above it
    # -- the shape that made guarding one site at a time useless.
    awk '{ if ($0 ~ /^    if \(!attacker\)$/ && !done) { print "    if (attacker->client) damage = 0;"; done = 1 } print }' \
        "$src" > "$tmp/above.c"
    if grep -q 'if (attacker->client) damage = 0;' "$tmp/above.c"; then
        if scan "$tmp/above.c" >/dev/null 2>&1; then
            fail "read above the boundary -- check did not fire"
        else
            pass "read placed above the boundary -- check fires"
        fi
    else
        fail "could not place a read above the boundary"
    fi

    # Positive control 3: normalised to something whose ->client is not NULL.
    sed 's/^        attacker = world;$/        attacker = targ;/' "$src" > "$tmp/wrong.c"
    if grep -q 'attacker = targ;' "$tmp/wrong.c"; then
        if scan "$tmp/wrong.c" >/dev/null 2>&1; then
            fail "normalised to targ -- check did not fire"
        else
            pass "normalised to something other than world -- check fires"
        fi
    else
        fail "could not change the substituted value"
    fi

    # Negative control: prose and strings naming attacker must stay silent,
    # including above the boundary, which is where this tree's own is.
    cat > "$tmp/noise.c" <<'EOF'
void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, int mod)
{
    /* attacker->client is read below; see also attacker->classname */
    // if (attacker->client) ...
    const char *s = "attacker->client";
    if (!attacker)
        attacker = world;
    if (attacker->client)
        mod = 0;
}
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage)
{
    if (!attacker)
        attacker = world;
    if (attacker->client)
        damage = 0;
}
EOF
    if scan "$tmp/noise.c" >/dev/null 2>&1; then
        pass "comments and strings naming attacker are not findings"
    else
        fail "a comment or string literal tripped the check"
    fi

    # Negative control: the real tree, unmodified, must pass.
    if scan "$src" >/dev/null 2>&1; then
        pass "the tree as committed passes"
    else
        fail "the tree as committed does not pass"
    fi

    if [ "$rc" -eq 0 ]; then
        echo "check-null-attacker: control passed"
    else
        echo "check-null-attacker: CONTROL FAILED -- the check is not trustworthy" >&2
    fi
    return $rc
}

if [ "$1" = "--self-test" ]; then
    run_self_test
    exit $?
fi

scan "$@"
