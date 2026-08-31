
enum {
    PMENU_ALIGN_LEFT,
    PMENU_ALIGN_CENTER,
    PMENU_ALIGN_RIGHT
};

typedef struct pmenuhnd_s {
    struct pmenu_s *entries;
    int cur;
    int num;
} pmenuhnd_t;

typedef struct pmenu_s {
    char *text;
    int align;
    void *arg;
    void (*SelectFunc)(edict_t *ent, struct pmenu_s *entry);
} pmenu_t;

// NOTHING INCLUDES THIS FILE.  p_menu.c includes only g_local.h, which carries
// its own copy of both typedefs and all of these prototypes -- so g_local.h is
// what the build reads and this header is a spare.  It is kept in step anyway,
// because a spare that has drifted is worse than no spare at all.
void PMenu_Open(edict_t *ent, const pmenu_t *entries, int cur, int num);
void PMenu_Close(edict_t *ent);
void PMenu_UpdateEntry(pmenu_t *entry, const char *text, int align,
                       void (*SelectFunc)(edict_t *ent, struct pmenu_s *entry));
void PMenu_Sync(edict_t *ent, const pmenu_t *entries);
void PMenu_Do_Update(edict_t *ent);
void PMenu_Update(edict_t *ent);
void PMenu_Next(edict_t *ent);
void PMenu_Prev(edict_t *ent);
void PMenu_Select(edict_t *ent);
