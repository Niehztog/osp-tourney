#include "g_local.h"

// THREE DEPARTURES FROM WHAT THE BINARY DID, each of which changes behaviour
// the reconstruction was faithful to.  They are on this branch and not on
// `q2pro-enhancements` for that reason: `asm_matching/` compares these sources
// against the shipped 2.75 objects, and every one of these makes that
// comparison fail on purpose.
//
//  1. THE BUILDER IS BOUNDED.  The original writes into a `char string[1400]`
//     with `sprintf(string + strlen(string), ...)` and no check at all -- the
//     entries are the mod's own and today they fit, so it never bit, but it is
//     a stack buffer whose contents go straight to gi.WriteString.  Switching
//     to snprintf is not enough on its own: an snprintf that runs out of room
//     stops wherever it happens to be, which is the middle of a token -- a
//     half-written `xv 6`, or a `"` that never closes.  Each entry is composed
//     into its own scratch buffer and appended only if it fits WHOLE.
//
//  2. THE ENTRIES ARE COPIED PER CLIENT, and this is the one that was doing
//     damage.  The handle pointed straight at the caller's array, and
//     osp_menus.c's menu tables are FILE-SCOPE GLOBALS whose text points at
//     file-scope statics, restaged from one client's `resp` state by an
//     update*Menu() immediately before the render.  That works only while the
//     two run back to back, and PMenu_Next/Prev skipped the restage entirely:
//     a client moving the cursor redrew whatever the LAST client to touch the
//     table had staged.  Two players in the voting menu showed each other's
//     pending timelimit while proposing their own, and the admin row's
//     SelectFunc -- the only gate between a player and the kick/ban menu,
//     since returnMainAdmin_menu, adminMenu, playerAdminChoose and rban_cmd
//     re-check `osp_e39c` nowhere -- is written from the same globals.
//
//  3. THE REDRAW IS RATE-LIMITED.  The original rebuilds and unicasts ~1300
//     RELIABLE bytes on every cursor keypress.  PMenu_Update now marks the
//     menu dirty and ClientEndServerFrame flushes at most five times a second,
//     forcing one through after a second of silence.
//
// Allocation moves from libc malloc/free to gi.TagMalloc/gi.TagFree with
// TAG_LEVEL, which is what departure 2 needs to be safe: the copy is per
// client and per menu, so it has to be released on a level change whether or
// not the client ever closes the menu.

// What svc_layout carries.  The original spells it as a bare 1400 at the one
// place it is used.
#define PMENU_MAX   1400

// One whole item at a time; an item that does not fit is dropped and the caller
// is told once, rather than the buffer being run past its end.
static bool menu_append(char *string, size_t size, size_t *len, const char *item)
{
    size_t n = strlen(item);

    if (*len + n >= size)
        return false;
    memcpy(string + *len, item, n + 1);
    *len += n;
    return true;
}

// Note that the pmenu entries are duplicated: the static tables in osp_menus.c
// can then be used for several clients and edited per client without
// interference.  Departure 2 above is what this loop is.
void PMenu_Open(edict_t *ent, const pmenu_t *entries, int cur, int num)
{
    pmenuhnd_t *hnd;
    const pmenu_t *p;
    int i;

    if (!ent->client)
        return;

    if (ent->client->menu) {
        gi.dprintf("warning, ent already has a menu\n");
        PMenu_Close(ent);
    }

    hnd = gi.TagMalloc(sizeof(*hnd), TAG_LEVEL);

    hnd->entries = gi.TagMalloc(sizeof(pmenu_t) * num, TAG_LEVEL);
    memcpy(hnd->entries, entries, sizeof(pmenu_t) * num);
    // duplicate the strings since they may be from static memory
    for (i = 0; i < num; i++)
        if (entries[i].text)
            hnd->entries[i].text = G_CopyString(entries[i].text);

    hnd->num = num;

    if (cur < 0 || !entries[cur].SelectFunc) {
        for (i = 0, p = entries; i < num; i++, p++)
            if (p->SelectFunc)
                break;
    } else
        i = cur;

    if (i >= num)
        hnd->cur = -1;
    else
        hnd->cur = i;

    ent->client->showscores = true;
    ent->client->inmenu = true;
    ent->client->menu = hnd;

    PMenu_Do_Update(ent);
    gi.unicast(ent, true);
}

void PMenu_Close(edict_t *ent)
{
    int i;
    pmenuhnd_t *hnd;

    if (!ent->client->menu)
        return;

    hnd = ent->client->menu;
    for (i = 0; i < hnd->num; i++)
        if (hnd->entries[i].text)
            gi.TagFree(hnd->entries[i].text);
    gi.TagFree(hnd->entries);
    gi.TagFree(hnd);
    ent->client->menu = NULL;
    ent->client->showscores = false;
    ent->client->inmenu = false;
    ent->client->menudirty = false;

    gi.WriteByte(svc_layout);
    gi.WriteString("xv 0 yv 0 string \" \"");
    gi.unicast(ent, true);
}

// only use on menus that have been opened with PMenu_Open.  A NULL text is
// allowed: the mod's tables use blank rows as spacing and half of every table
// is one.
void PMenu_UpdateEntry(pmenu_t *entry, const char *text, int align,
                       void (*SelectFunc)(edict_t *ent, struct pmenu_s *entry))
{
    if (entry->text)
        gi.TagFree(entry->text);
    entry->text = text ? G_CopyString((char *)text) : NULL;
    entry->align = align;
    entry->SelectFunc = SelectFunc;
}

/*
================
PMenu_Sync

Re-copy a template into this client's private rows.

Departure 2 gave each client its own copy, which is what stops one client's
menu being redrawn from another's staged text -- but it also cuts the
update*Menu() builders off from the rows they are meant to be updating.  They
still compose into the file statics in osp_menus.c, because a dozen tables'
worth of `Menu[7].text = tm_admin;` is the shape that file is, and turning all
of it into PMenu_UpdateEntry() calls would be a rewrite of osp_menus.c rather
than a fix to the engine.  So the template keeps its job as the STAGING AREA
and this function takes the copy: after it returns, nothing another client does
to the globals can be seen by this one.

SILENT WHEN NO MENU IS OPEN, and that is the interesting half.  The builders
are called from two places -- the openers, which stage and then call PMenu_Open
(which takes its own copy), and the leaves, which restage while the menu is up.
Only the leaves need this, and they are exactly the sites that used to end
`PMenu_Update(ent); gi.unicast(ent, true);`.  The guard is what lets a caller
not have to know which of the two it is.

The template must be the one this menu was opened with; `hnd->num` is the
length, so a shorter table would be read past its end.  Each builder writes
exactly one table and each leaf syncs the table its builder wrote, which is
what makes that hold.
================
*/
void PMenu_Sync(edict_t *ent, const pmenu_t *entries)
{
    pmenuhnd_t *hnd;
    int i;

    if (!ent->client || !ent->client->menu)
        return;

    hnd = ent->client->menu;

    for (i = 0; i < hnd->num; i++) {
        PMenu_UpdateEntry(hnd->entries + i, entries[i].text,
                          entries[i].align, entries[i].SelectFunc);
        hnd->entries[i].arg = entries[i].arg;
    }
}

void PMenu_Do_Update(edict_t *ent)
{
    char string[PMENU_MAX];
    char item[128];
    size_t len;
    int i;
    pmenu_t *p;
    int x;
    pmenuhnd_t *hnd;
    char *t;
    bool alt = false;
    bool dropped = false;

    if (!ent->client->menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->menu;

    len = 0;
    string[0] = 0;
    menu_append(string, sizeof(string), &len, "xv 32 yv 8 picn inventory ");

    for (i = 0, p = hnd->entries; i < hnd->num; i++, p++) {
        if (!p->text || !*(p->text))
            continue; // blank line
        t = p->text;
        if (*t == '*') {
            alt = true;
            t++;
        }

        if (p->align == PMENU_ALIGN_CENTER)
            x = 196 / 2 - strlen(t) * 4 + 60;
        else if (p->align == PMENU_ALIGN_RIGHT)
            x = 60 + (212 - strlen(t) * 8);
        else
            x = 60;

        // The whole entry -- position and text -- is one item: emitting the
        // `yv`/`xv` pair and then dropping the string would leave the cursor
        // moved with nothing drawn.
        //
        // The original's four-way if/else chain says exactly this: the arrow is
        // on the cursor row, and the font is `string2` when the cursor row and
        // the leading `*` DISAGREE.  It is an XOR, and the case that makes it
        // one is a starred row that is also the cursor row -- that one draws in
        // `string`, not `string2`.
        Q_snprintf(item, sizeof(item), "yv %d xv %d %s \"%s%s\" ",
                   32 + i * 8, x - ((hnd->cur == i) ? 8 : 0),
                   ((hnd->cur == i) != alt) ? "string2" : "string",
                   (hnd->cur == i) ? "\x0d" : "", t);

        if (!menu_append(string, sizeof(string), &len, item))
            dropped = true;
        alt = false;
    }

    if (dropped)
        gi.dprintf("PMenu: layout exceeded %d bytes; entries were dropped "
                   "whole\n", PMENU_MAX);

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

// Departure 3's door.  The original composed and unicast ~1300 reliable bytes
// here on every keypress; this defers to ClientEndServerFrame, which flushes at
// most five times a second and forces one through after a second of silence.
void PMenu_Update(edict_t *ent)
{
    if (!ent->client->menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    if (level.time - ent->client->menutime >= 1.0f) {
        // been a second or more since last update, update now
        PMenu_Do_Update(ent);
        gi.unicast(ent, true);
        ent->client->menutime = level.time;
        ent->client->menudirty = false;
    }
    ent->client->menutime = level.time + 0.2f;
    ent->client->menudirty = true;
}

void PMenu_Next(edict_t *ent)
{
    pmenuhnd_t *hnd;
    int i;
    pmenu_t *p;

    if (!ent->client->menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->menu;

    if (hnd->cur < 0)
        return; // no selectable entries

    i = hnd->cur;
    p = hnd->entries + hnd->cur;
    do {
        i++, p++;
        if (i == hnd->num)
            i = 0, p = hnd->entries;
        if (p->SelectFunc)
            break;
    } while (i != hnd->cur);

    hnd->cur = i;

    PMenu_Update(ent);
}

void PMenu_Prev(edict_t *ent)
{
    pmenuhnd_t *hnd;
    int i;
    pmenu_t *p;

    if (!ent->client->menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->menu;

    if (hnd->cur < 0)
        return; // no selectable entries

    i = hnd->cur;
    p = hnd->entries + hnd->cur;
    do {
        if (i == 0) {
            i = hnd->num - 1;
            p = hnd->entries + i;
        } else
            i--, p--;
        if (p->SelectFunc)
            break;
    } while (i != hnd->cur);

    hnd->cur = i;

    PMenu_Update(ent);
}

void PMenu_Select(edict_t *ent)
{
    pmenuhnd_t *hnd;
    pmenu_t *p;

    if (!ent->client->menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->menu;

    if (hnd->cur < 0)
        return; // no selectable entries

    p = hnd->entries + hnd->cur;

    if (p->SelectFunc)
        p->SelectFunc(ent, p);
}
