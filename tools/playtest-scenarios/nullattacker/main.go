// nullattacker -- does a blocked door's target_explosion take the server down?
//
// WHERE THIS FILE LIVES AND WHY.  Every other play-test scenario lives in the
// harness ($HARNESS, see tools/playtest.sh), because what it knows is true of
// any Quake II server.  This one is not: it depends on a map fixture that is
// this project's -- tools/nullattacker-map.py -- so it is versioned with the
// fixture instead.  Run it by dropping it into the harness:
//
//	cp -r tools/playtest-scenarios/nullattacker $HARNESS/scenarios/
//	tools/nullattacker-map.py -o /tmp/q2dm8.ent
//	cd $HARNESS && go run ./scenarios/nullattacker \
//	    -bin <q2proded> -ref <install> -lib <project>/release/game<arch>.so
//
// THE PRODUCER IS ID'S, UNCHANGED.  Nothing in g_func.c ever assigns
// `activator` on a func_door, so a door that reverses because something
// blocked it hands on the zero it was spawned with:
//
//	door_blocked() -> door_go_up(ent, ent->activator) -> G_UseTargets()
//	-> a targeted target_explosion -> T_RadiusDamage(self, self->activator, ...)
//
// id's own T_Damage survives that by accident -- its one `attacker->client`
// read sits behind `!(dflags & DAMAGE_RADIUS)`, which short-circuits on exactly
// this path -- but OSP's accuracy block in T_RadiusDamage reads
// `attacker->client` outright, before any of its own guards, so the NULL is
// dereferenced and the server process dies.
//
// WHAT THE FIXTURE ARRANGES is described in tools/nullattacker-map.py.  The
// short version: the client spawns into an open doorway, stands still, and the
// door closes on it.  The client does nothing at all.
//
// BOTH SIGNS.  A build without the boundary dies: the server process exits on
// SIGSEGV at the moment of the reversal, and `-gdb` prints the frame.  A build
// with it survives AND the blast lands -- the client is damaged and says so --
// which is what proves the crash site was reached rather than merely avoided.
//
// Q2PRO ONLY: the fixture is an entity-string override (`map_override_path`),
// which Yamagi does not have.  The defect is on both engines and so is the fix;
// only this way of provoking it is q2pro's.
package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"q2playtest/playtest"
)

var (
	engine  = flag.String("engine", "q2pro", "q2pro (only: yq2 has no entity override)")
	binary  = flag.String("bin", "", "dedicated server binary")
	ref     = flag.String("ref", "../../../yquake2/release_", "reference install")
	lib     = flag.String("lib", "", "game library under test")
	entfile = flag.String("ent", "/tmp/q2dm8.ent", "entity string override")
	mapname = flag.String("map", "q2dm8", "map to run")
	dir     = flag.String("dir", "/tmp/nullattacker", "scratch dir")
	port    = flag.Int("port", 27990, "UDP port")
	label   = flag.String("label", "run", "label")
	secs    = flag.Int("secs", 50, "seconds to watch after the client is in")
	gdb     = flag.Bool("gdb", false, "run the server under gdb and print a backtrace on the fault")
	verbose = flag.Bool("v", true, "log every position change")
)

const statHealth = 1

func main() {
	flag.Parse()
	if *lib == "" {
		fmt.Fprintln(os.Stderr, "need -lib")
		os.Exit(2)
	}
	if *binary == "" {
		*binary = "../../../q2pro/builddir-test/q2proded"
	}

	d := filepath.Join(*dir, *engine+"-"+*label)
	os.RemoveAll(d)
	for _, sub := range []string{"baseq2", "tourney", "tourney/maps"} {
		os.MkdirAll(filepath.Join(d, sub), 0o755)
	}
	for _, p := range []string{"pak0.pak", "pak1.pak", "pak2.pak"} {
		os.Symlink(filepath.Join(*ref, "baseq2", p), filepath.Join(d, "baseq2", p))
	}
	ents, _ := os.ReadDir(filepath.Join(*ref, "tourney"))
	for _, e := range ents {
		n := e.Name()
		if strings.HasPrefix(n, "game") || strings.Contains(n, "oracle") ||
			strings.Contains(n, "v275-real") {
			continue
		}
		os.Symlink(filepath.Join(*ref, "tourney", n), filepath.Join(d, "tourney", n))
	}
	body, err := os.ReadFile(*lib)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	// q2pro loads game<CPUSTRING>.so and its CPUSTRING is not always the
	// name the Makefile built, so install under both spellings.
	for _, n := range []string{filepath.Base(*lib), "gamearm64.so", "gameaarch64.so"} {
		os.WriteFile(filepath.Join(d, "tourney", n), body, 0o755)
	}
	os.WriteFile(filepath.Join(d, "tourney", "maps.txt"), []byte(*mapname+"\n"), 0o644)
	ent, err := os.ReadFile(*entfile)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	os.WriteFile(filepath.Join(d, "tourney", "maps", *mapname+".ent"), ent, 0o644)

	args := []string{"+set", "basedir", d, "+set", "homedir", d,
		"+set", "game", "tourney", "+set", "dedicated", "1",
		"+set", "deathmatch", "1", "+set", "maxclients", "8",
		"+set", "timelimit", "0", "+set", "fraglimit", "0",
		// this is what makes the override load at all
		"+set", "map_override_path", "maps",
		"+set", "net_port", strconv.Itoa(*port), "+set", "sv_iplimit", "0",
		"+map", *mapname}

	logPath := filepath.Join(d, "server.log")
	logf, _ := os.Create(logPath)
	bin, binArgs := *binary, args
	if *gdb {
		// gdb catches the SIGSEGV and prints where it happened, which is the
		// difference between "the server died" and "it died at this read".
		binArgs = append([]string{"--batch", "-ex", "run", "-ex", "bt 8",
			"-ex", "info registers x0 x1", "--args", *binary}, args...)
		bin = "gdb"
	}
	cmd := exec.Command(bin, binArgs...)
	cmd.Dir = d
	cmd.Stdout, cmd.Stderr = logf, logf
	if err := cmd.Start(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	died := make(chan error, 1)
	go func() { died <- cmd.Wait() }()
	defer func() { cmd.Process.Kill() }()

	fmt.Printf("== nullattacker %s/%s ==\n", *engine, *label)
	fmt.Printf("   lib %s\n", *lib)
	time.Sleep(4 * time.Second)

	// The override is a precondition, not a result: without it the map is
	// the stock one and nothing below means anything.
	if !grep(logPath, "Loaded entity string") {
		fmt.Println("   the server did not load the entity override -- check map_override_path")
		fmt.Println("  FAIL  precondition")
		os.Exit(2)
	}
	fmt.Println("   entity override loaded")

	b := playtest.NewBot("victim", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "client:", err)
		os.Exit(2)
	}
	// CONNECTING IS NOT ENTERING under OSP: a connected client is an
	// observer -- MOVETYPE_NOCLIP, unlinked, invisible to the push physics --
	// until it joins, and the pmove type is not the witness for that here.
	// The server's own broadcast is.
	entered := false
	for i := 0; i < 8 && !entered; i++ {
		b.Cmd("join")
		time.Sleep(time.Second)
		entered = grep(logPath, "entered the game")
	}
	if !entered {
		fmt.Println("  FAIL  the client never entered the game")
		os.Exit(2)
	}
	time.Sleep(time.Second)
	p := b.Origin()
	fmt.Printf("   client standing at %.0f %.0f %.0f, health %d\n", p[0], p[1], p[2], b.Stat(statHealth))

	// Now do nothing.  The door is what moves.
	start := time.Now()
	minHealth := 1000
	blasted := false
	deadline := time.After(time.Duration(*secs) * time.Second)
	tick := time.NewTicker(200 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case err := <-died:
			fmt.Printf("   t=%.1fs  SERVER GONE: %v\n", time.Since(start).Seconds(), err)
			time.Sleep(2 * time.Second)
			for _, l := range tail(logPath, 24) {
				fmt.Printf("   log| %s\n", l)
			}
			fmt.Println("  FAIL  the blocked door's target_explosion killed the server")
			os.Exit(1)
		case <-deadline:
			q := b.Origin()
			fmt.Printf("   client at %.0f %.0f %.0f, health %d, lowest seen %d, blast witnessed %v\n",
				q[0], q[1], q[2], b.Stat(statHealth), minHealth, blasted)
			if !blasted && minHealth > 90 {
				for _, l := range b.Prints() {
					fmt.Printf("   say| %s\n", l)
				}
				fmt.Println("  FAIL  no blast damage: the crash site was never reached")
				os.Exit(1)
			}
			for _, l := range b.Prints() {
				fmt.Printf("   say| %s\n", l)
			}
			fmt.Println("  PASS  the door reversed, the explosion ran, the server lived")
			return
		case <-tick.C:
			if h := b.Stat(statHealth); h > 0 && h < minHealth {
				minHealth = h
			}
			for _, l := range b.Prints() {
				if strings.Contains(l, "blew up") || strings.Contains(l, "died") {
					blasted = true
				}
			}
			if q := b.Origin(); *verbose && (abs(q[2]-p[2]) > 4 || abs(q[0]-p[0]) > 4 || abs(q[1]-p[1]) > 4) {
				fmt.Printf("   t=%.1fs  moved to %.0f %.0f %.0f health %d\n",
					time.Since(start).Seconds(), q[0], q[1], q[2], b.Stat(statHealth))
				p = q
			}
		}
	}
}

func abs(f float64) float64 {
	if f < 0 {
		return -f
	}
	return f
}

func grep(path, want string) bool {
	body, err := os.ReadFile(path)
	return err == nil && strings.Contains(string(body), want)
}

func tail(path string, n int) []string {
	body, err := os.ReadFile(path)
	if err != nil {
		return nil
	}
	lines := strings.Split(strings.TrimRight(string(body), "\n"), "\n")
	if len(lines) > n {
		lines = lines[len(lines)-n:]
	}
	return lines
}
