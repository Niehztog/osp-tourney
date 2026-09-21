# OSP Tourney DM on the Q2PRO game API -- native Makefile.
#
# Same shape as the reconstruction's Makefile: same debug/release split, same
# $(BUILDDIR)/game$(ARCH).$(SHLIBEXT) target, same one-rule-per-TU tail, and
# GAME_OBJS still in the link order recovered from the shipped binary. Link
# order no longer decides anything here -- this tree does not byte-match the
# original -- but it is kept because it is recovered evidence, not taste.
#
# THIS IS NOT THE ORACLE, and unlike on the reconstruction branch there is no
# oracle to be: the astyle pass, the retyping and the API changes mean nothing
# in this tree assembles to the shipped image. asm_matching/ still works, but
# only against `main`.

BUILD_DEBUG_DIR=debug$(BUILDSUFFIX)
BUILD_RELEASE_DIR=release$(BUILDSUFFIX)

# API=old builds against the classic game ABI -- GAME_API_VERSION 3,
# gclient_old_t, pmove_old_t -- which is what Yamagi Quake II, r1q2 and id's own
# 3.20 dedicated server load.  The default (API=new) is Q2PRO's 3302.
#
# It is a SUPPORTED CONFIGURATION and not a curiosity: this mod's whole audience
# is 1v1 and team tournaments on other people's servers, and half the servers
# still running Quake II are not Q2PRO.  Every source file already carries the
# `#if USE_NEW_GAME_API` branch for it -- g_save.c's client field table and save
# version, p_client.c's PM_trace signature, g_local.h's PM_TIME_SHIFT -- so the
# only thing missing was a way to ask for it.  See doc/q2pro-port.md.
#
# The two builds land in separate directories so both can exist at once, which
# is what makes an A/B across engines a single `make`.
API=new
ifeq ($(API),old)
BUILDSUFFIX=-oldapi
API_CFLAGS=-DUSE_NEW_GAME_API=0
else ifeq ($(API),new)
BUILDSUFFIX=
API_CFLAGS=
else
$(error API must be `new` (Q2PRO, the default) or `old` (Yamagi Quake II, r1q2, id 3.20))
endif

# Q2PRO looks for game<CPUSTRING><suffix> next to the mod directory, and its
# CPUSTRING is meson's cpu_family: x86_64, x86, arm, aarch64.
ARCH:=$(shell uname -m | sed -e 's/i.86/x86/' -e 's/^armv.*/arm/')

CC=gcc

# config.h is Q2PRO's build configuration for a standalone game library, and
# shared/ holds the engine headers the game links against (shared.h, game.h,
# list.h, platform.h) plus shared.c.
INCLUDES=-I. -Ishared
BASE_CFLAGS=-DHAVE_CONFIG_H $(INCLUDES) -Dstricmp=strcasecmp $(API_CFLAGS)

RELEASE_CFLAGS=$(BASE_CFLAGS) $(MODERN_CFLAGS) -O2
DEBUG_CFLAGS=$(BASE_CFLAGS) $(MODERN_CFLAGS) -g -O0

# Required to build 1999 C with a current gcc.
#  -fno-strict-aliasing -fwrapv  the workspace standard for this era of code
#  -Wall                         the tree is clean under it.  It used to be -w,
#                                which is how a set of real defects went
#                                unmeasured -- see doc/q2pro-port.md.  -Wextra
#                                is still too loud to gate on: it reports the
#                                335 old-style `foo()` prototypes tourney's own
#                                code is written in.
#
# The reconstruction's -fno-stack-protector / -D_FORTIFY_SOURCE=0 relaxation is
# gone: tourney's own string handling has been audited end to end and every
# unbounded copy into a fixed buffer is now a bounded one, so the hardening the
# toolchain offers is switched on rather than switched off.
MODERN_CFLAGS=-fno-strict-aliasing -fwrapv -Wall \
	-D_FORTIFY_SOURCE=2 -fstack-protector-strong

LDFLAGS=-ldl -lm

SHLIBEXT=so

SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared -Wl,--no-undefined

DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
DO_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<

TARGETS=$(BUILDDIR)/game$(ARCH).$(SHLIBEXT)

build_debug:
	@-mkdir $(BUILD_DEBUG_DIR)
	$(MAKE) targets API=$(API) BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

build_release:
	@-mkdir $(BUILD_RELEASE_DIR)
	$(MAKE) targets API=$(API) BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)"

all: build_debug build_release

targets: $(TARGETS)

# pm_time has no fixed unit -- 8 ms tics on a plain server, milliseconds on an
# extended one -- so every hold has to be written `<ms> >> PM_TIME_SHIFT`.  The
# pre-extension spellings (`14`, `160>>3`) still compile and still look right,
# so nothing in the toolchain catches them; this does.  Run it by hand with
# `make check-pm-time`, and `make check-pm-time-control` restores two of the
# defects to show the check actually fires.  See tools/check-pm-time.sh.
#
# It runs as a stamp the objects depend on rather than as a prerequisite of
# `targets`, so it is ordered strictly before every compile even under -j, and
# re-runs only when a source it scans has changed.
CHECK_SRCS=$(wildcard *.c *.h shared/*.c shared/*.h)
CHECK_STAMP=$(BUILDDIR)/.pm-time-ok

$(CHECK_STAMP): $(CHECK_SRCS) tools/check-pm-time.sh
	tools/check-pm-time.sh $(CHECK_SRCS)
	@touch $@

check-pm-time:
	tools/check-pm-time.sh $(CHECK_SRCS)

check-pm-time-control:
	tools/check-pm-time.sh --self-test

# The same shape, for the same reason: T_Damage and T_RadiusDamage answer a
# NULL `attacker` once, at the top, and every read below them depends on that
# having happened.  Delete those two lines and the tree still builds, still
# boots, and dies on the first map in the rotation with a target_explosion on
# a door.  `make check-null-attacker-control` restores each of the three defect
# shapes to show the check actually fires.  See tools/check-null-attacker.sh.
NULLATK_STAMP=$(BUILDDIR)/.null-attacker-ok

$(NULLATK_STAMP): g_combat.c tools/check-null-attacker.sh
	tools/check-null-attacker.sh g_combat.c
	@touch $@

check-null-attacker:
	tools/check-null-attacker.sh g_combat.c

check-null-attacker-control:
	tools/check-null-attacker.sh --self-test

.PHONY: all build_debug build_release targets check-pm-time check-pm-time-control \
	check-null-attacker check-null-attacker-control \
	clean clean-debug clean-release clean2

# In the link order recovered from the shipped gamei386.so, with q_shared.o
# replaced by the engine's shared.c and Q2PRO's generated g_ptrs.c appended.
GAME_OBJS = \
	$(BUILDDIR)/g_ai.o \
	$(BUILDDIR)/g_cmds.o \
	$(BUILDDIR)/g_combat.o \
	$(BUILDDIR)/g_func.o \
	$(BUILDDIR)/g_items.o \
	$(BUILDDIR)/g_main.o \
	$(BUILDDIR)/g_misc.o \
	$(BUILDDIR)/g_monster.o \
	$(BUILDDIR)/g_phys.o \
	$(BUILDDIR)/g_save.o \
	$(BUILDDIR)/g_spawn.o \
	$(BUILDDIR)/g_svcmds.o \
	$(BUILDDIR)/g_target.o \
	$(BUILDDIR)/g_trigger.o \
	$(BUILDDIR)/g_turret.o \
	$(BUILDDIR)/g_utils.o \
	$(BUILDDIR)/g_weapon.o \
	$(BUILDDIR)/m_move.o \
	$(BUILDDIR)/p_camera.o \
	$(BUILDDIR)/p_client.o \
	$(BUILDDIR)/p_hud.o \
	$(BUILDDIR)/p_trail.o \
	$(BUILDDIR)/p_view.o \
	$(BUILDDIR)/p_weapon.o \
	$(BUILDDIR)/shared_shared.o \
	$(BUILDDIR)/osp_config.o \
	$(BUILDDIR)/osp_main.o \
	$(BUILDDIR)/osp_display.o \
	$(BUILDDIR)/g_chase.o \
	$(BUILDDIR)/osp_cmds.o \
	$(BUILDDIR)/osp_hook.o \
	$(BUILDDIR)/osp_hiscore.o \
	$(BUILDDIR)/osp_menus.o \
	$(BUILDDIR)/osp_runes.o \
	$(BUILDDIR)/osp_teams.o \
	$(BUILDDIR)/osp_plist.o \
	$(BUILDDIR)/osp_maps.o \
	$(BUILDDIR)/osp_detect.o \
	$(BUILDDIR)/osp_stats.o \
	$(BUILDDIR)/stdlog.o \
	$(BUILDDIR)/sl_write.o \
	$(BUILDDIR)/p_menu.o \
	$(BUILDDIR)/bl_botcfg.o \
	$(BUILDDIR)/bl_cmd.o \
	$(BUILDDIR)/bl_debug.o \
	$(BUILDDIR)/bl_main.o \
	$(BUILDDIR)/bl_redirgi.o \
	$(BUILDDIR)/bl_spawn.o \
	$(BUILDDIR)/g_ptrs.o

$(BUILDDIR)/game$(ARCH).$(SHLIBEXT) : $(GAME_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(GAME_OBJS) $(LDFLAGS)

$(BUILDDIR)/%.o : %.c $(CHECK_STAMP) $(NULLATK_STAMP)
	$(DO_SHLIB_CC)

#####

clean: clean-debug clean-release

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR)

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR)

clean2:
	-rm -f $(GAME_OBJS) $(CHECK_STAMP) $(NULLATK_STAMP)
