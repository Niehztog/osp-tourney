#!/bin/bash
# Play-test this game library against real dedicated servers of BOTH engine
# families, with headless clients on the wire asserting what they observe.
#
# The harness itself is user-wide (~/.claude/skills/q2-playtest/harness, built
# on libq2); this file is the versioned record of how to invoke it for this
# project, which is the half that belongs in the tree.
#
#   tools/playtest.sh                     both engines, the fixed build
#   tools/playtest.sh q2pro               Q2PRO only
#   tools/playtest.sh yq2                 Yamagi only
#   LIB_NEW=... LIB_OLD=... tools/playtest.sh      A/B against another build
#
# WHY BOTH ENGINES.  This mod's audience is tournaments on other people's
# servers and most of those are not Q2PRO, so `API=old` (GAME_API_VERSION 3,
# gclient_old_t, pmove_old_t) is a supported configuration and not a curiosity.
# Several of the defects the battery covers behave DIFFERENTLY on the two
# families rather than simply being present on both:
#
#   * `map` issued mid-game is IGNORED by Q2PRO on a dedicated server
#     (sv_allow_map defaults to 0), which wedges an arm that returns without
#     clearing the intermission flags -- and is honoured by Yamagi as a FULL
#     SERVER RESTART, which changes the level and drops every connected player.
#     One bug, two symptoms; the battery asserts on both.
#   * the rune-cache staleness is MASKED on the old engines, because the full
#     restart re-runs InitGame and recomputes the cache as a side effect.
#
# A row that only ran on one family would have called each of those fixed.
set -u

HERE=$(cd "$(dirname "$0")/.." && pwd)
HARNESS=${HARNESS:-$HOME/.claude/skills/q2-playtest/harness}
LIB_NEW=${LIB_NEW:-$HERE/release/gameaarch64.so}
LIB_OLD=${LIB_OLD:-$HERE/release-oldapi/gameaarch64.so}
WHICH=${1:-both}

fail=0
run() {
    local eng=$1 lib=$2 port=$3
    if [ ! -f "$lib" ]; then
        echo "missing $lib -- run 'make build_release'$([ "$eng" = yq2 ] && echo " API=old")"
        fail=1
        return
    fi
    ( cd "$HARNESS" && go run ./scenarios/ospthink \
        -engine "$eng" -lib "$lib" -label check -port "$((port + 40))" ) || fail=1
    ( cd "$HARNESS" && go run ./scenarios/ospfixes \
        -engine "$eng" -lib "$lib" -label check -port "$port" ) || fail=1
}

# The old-API library is what Yamagi loads; the default one is Q2PRO's.
case "$WHICH" in
    both|q2pro) run q2pro "$LIB_NEW" 27930 ;;
esac
case "$WHICH" in
    both|yq2)   run yq2   "$LIB_OLD" 27960 ;;
esac

exit $fail
