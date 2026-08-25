#!/bin/sh
#
# check-pm-time.sh -- fail the build on a pm_time assignment that builds a
# duration without going through PM_TIME_SHIFT.
#
# pmove_state_t::pm_time does not have a fixed unit.  On a plain server it is a
# byte counting 8 ms tics; once protocol extensions are negotiated it is a
# uint16_t counting milliseconds.  g_local.h hides that behind
#
#     #define PM_TIME_SHIFT   (game.csr.extended ? 0 : 3)
#
# so every duration is written in milliseconds and shifted on the way in:
# `160 >> PM_TIME_SHIFT`.  The pre-extension spellings -- a bare `14`, or a
# hardcoded `160 >> 3` -- still compile and still look right, but they put an
# 8 ms-tic count into a millisecond field, so the hold lasts an eighth as long
# as intended on an extended server.  That is a silent gameplay bug on exactly
# the servers this port exists to run on, and no compiler warning covers it.
#
# Two spellings are legitimate and are not durations, so they are exempt:
#
#   * `pm_time = 0` -- cancels the hold.  Zero is zero in either unit.
#   * `a.pm_time = b.pm_time` -- copies a value that was already shifted by
#     whoever built it.  Copying does not construct a duration.
#
# Anything else assigned to a pm_time must mention PM_TIME_SHIFT.  Note the
# rule keys on the *name* pm_time, not on the type, so it also covers
# bl_main.c's `buc.pm_time`, which feeds botlib's fixed 8 ms-tic ABI and needs
# the inverse shift.
#
# Usage:
#   check-pm-time.sh FILE...        check those files, exit 1 on a violation
#   check-pm-time.sh --self-test    run the control (see run_self_test below)
#
# Only the sources on the compiler's include path (`.` and `shared/`) are
# passed in by the Makefile.  The donor trees vendored at the top level --
# vanilla-q2-3.20-src/, q2ctf-1.02-src/, gladq2-0.96-src/ -- are full of the
# unshifted 1997 spelling and are deliberately out of scope: they are read-only
# reference, not part of the build.

set -e

self="$0"
dir=$(dirname "$self")

# ---------------------------------------------------------------------------
# The scanner.
#
# C comments and string literals are stripped by a character state machine
# before matching, which is not paranoia: `byte pm_time; // each unit = 8 ms`
# in shared.h and `// byte pm_time;` in bl_main.c both parse as assignments if
# you match raw lines.  Statements are then accumulated to the `;` so an
# assignment split over two lines is still seen whole.
# ---------------------------------------------------------------------------
scan() {
    awk '
    function flush_stmt(   rhs, op) {
        if (stmt ~ /(^|[^A-Za-z0-9_])pm_time[ \t]*([-+*/%&|^]|<<|>>)?=[^=]/) {
            rhs = stmt
            sub(/^.*[^A-Za-z0-9_]pm_time[ \t]*([-+*/%&|^]|<<|>>)?=/, "", rhs)
            gsub(/^[ \t]+|[ \t]+$/, "", rhs)
            gsub(/[ \t]+/, " ", rhs)

            op = "ok"
            if (rhs == "0")                                            op = "ok"
            else if (rhs ~ /PM_TIME_SHIFT/)                            op = "ok"
            else if (rhs ~ /^[A-Za-z_][A-Za-z0-9_]*((\.|->)[A-Za-z_][A-Za-z0-9_]*)*(\.|->)pm_time$/) op = "ok"
            else                                                       op = "bad"

            if (op == "bad") {
                printf "%s:%d: pm_time duration built without PM_TIME_SHIFT: %s\n", FILENAME, stmt_line, rhs
                bad++
            }
        }
        stmt = ""
        stmt_line = 0
    }
    BEGIN { state = "code"; stmt = ""; stmt_line = 0; bad = 0 }
    FNR == 1 { flush_stmt(); state = "code" }
    {
        n = length($0)
        i = 1
        while (i <= n) {
            c = substr($0, i, 1)
            d = substr($0, i, 2)
            if (state == "code") {
                if (d == "/*")      { state = "block"; i += 2; continue }
                if (d == "//")      { break }
                if (c == "\"")      { state = "str";  i++; stmt = stmt "\"\""; continue }
                if (c == "'"'"'")   { state = "chr";  i++; stmt = stmt "'"'"''"'"'"; continue }
                if (c == ";" || c == "{" || c == "}") { flush_stmt(); i++; continue }
                if (stmt == "" && c ~ /[ \t]/) { i++; continue }
                if (stmt == "") stmt_line = FNR
                stmt = stmt c
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
                if (c == "\"") { state = "code"; i++; continue }
                i++
                continue
            }
            if (state == "chr") {
                if (c == "\\") { i += 2; continue }
                if (c == "'"'"'") { state = "code"; i++; continue }
                i++
                continue
            }
        }
        if (state == "code" && stmt != "") stmt = stmt " "
    }
    END { flush_stmt(); exit (bad > 0) }
    ' "$@"
}

# ---------------------------------------------------------------------------
# The control.
#
# A check that is never seen to fail is not evidence of anything -- it could be
# matching nothing at all.  So the control drives it in both directions: it
# restores the two historical defect spellings and requires a hit on each, it
# requires the two exempt spellings to stay silent, and it requires the real
# tree to pass.  That last one is what stops a checker that simply always fails
# from satisfying the first three.
# ---------------------------------------------------------------------------
run_self_test() {
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT

    rc=0
    pass() { printf '  ok      %s\n' "$1"; }
    fail() { printf '  FAILED  %s\n' "$1"; rc=1; }

    # Positive control 1: the bare-literal spelling, restored into a real copy
    # of p_client.c at the site commit 0f0d1c7 converted.  `14` is 14 tics =
    # 112 ms on a plain server, and 14 ms on an extended one.
    sed 's/pm_time = 112 >> PM_TIME_SHIFT/pm_time = 14/' \
        "$dir/../p_client.c" > "$tmp/p_client.c"
    if grep -q 'pm_time = 14;' "$tmp/p_client.c"; then
        if out=$(scan "$tmp/p_client.c" 2>&1); then
            fail "restored bare literal 14 -- check did not fire"
        else
            case $out in
                *"pm_time duration built without PM_TIME_SHIFT: 14"*)
                    pass "restored bare literal 14 -- check fires" ;;
                *)  fail "check fired but not on the restored line: $out" ;;
            esac
        fi
    else
        fail "could not restore the defect -- p_client.c site moved?"
    fi

    # Positive control 2: the hardcoded-shift spelling from Q2 3.20's g_misc.c.
    sed 's/pm_time = 160 >> PM_TIME_SHIFT/pm_time = 160>>3/' \
        "$dir/../g_misc.c" > "$tmp/g_misc.c"
    if grep -q 'pm_time = 160>>3;' "$tmp/g_misc.c"; then
        if scan "$tmp/g_misc.c" >/dev/null 2>&1; then
            fail "restored hardcoded 160>>3 -- check did not fire"
        else
            pass "restored hardcoded 160>>3 -- check fires"
        fi
    else
        fail "could not restore the defect -- g_misc.c site moved?"
    fi

    # Negative control 1: the two exempt spellings must stay silent.
    cat > "$tmp/exempt.c" <<'EOF'
void f(void)
{
    ent->client->ps.pmove.pm_time = 0;          /* cancels the hold */
    buc.pm_time = bot->client->ps.pmove.pm_time; /* copy, not a duration */
}
EOF
    if scan "$tmp/exempt.c" >/dev/null 2>&1; then
        pass "exempt spellings (literal 0, pm_time copy) stay silent"
    else
        fail "exempt spellings tripped the check"
    fi

    # Negative control 2: comments and string literals must not parse as code.
    cat > "$tmp/noise.c" <<'EOF'
/* byte pm_time; = 14 */
// pm_time = 14;
const char *s = "pm_time = 14;";
typedef struct { unsigned short pm_time; /* each unit = 8 ms */ } t;
EOF
    if scan "$tmp/noise.c" >/dev/null 2>&1; then
        pass "comments and strings are not mistaken for assignments"
    else
        fail "a comment or string literal tripped the check"
    fi

    # Negative control 3: the real tree, unmodified, must pass.  Without this
    # a checker hardwired to `exit 1` would satisfy every test above.
    if (cd "$dir/.." && scan $(for f in *.c *.h shared/*.c shared/*.h; do
            [ -f "$f" ] && echo "$f"
        done)) >/dev/null 2>&1; then
        pass "the tree as committed passes"
    else
        fail "the tree as committed does not pass"
    fi

    if [ "$rc" -eq 0 ]; then
        echo "check-pm-time: control passed"
    else
        echo "check-pm-time: CONTROL FAILED -- the check is not trustworthy" >&2
    fi
    return $rc
}

if [ "$1" = "--self-test" ]; then
    run_self_test
    exit $?
fi

if [ $# -eq 0 ]; then
    echo "usage: $0 FILE... | $0 --self-test" >&2
    exit 2
fi

if scan "$@"; then
    exit 0
fi

echo "check-pm-time: a pm_time duration is missing PM_TIME_SHIFT (see tools/check-pm-time.sh)" >&2
exit 1
