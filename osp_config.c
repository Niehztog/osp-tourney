// osp_config.c -- <INVENTED FILENAME>. The serverconfigs.txt loader and its
// three lookup helpers.

#include "g_local.h"
#include "bl_main.h"

// The alternate server configs, loaded from configs.txt: conf_name[i] is the
// .cfg filename and conf_info[i] an optional friendly description.  Either one
// may be used to name a config in a vote, which is what OSP_configExists is
// resolving.
int conf_size = 0;
char    conf_info[50][64];
char    conf_name[50][64];

// Read the server-config list (serverconfigs.txt unless vote_config_list says
// otherwise) and fill conf_name/conf_info from it.  A line is
// "<file>\t<description>"; the description is optional.  A blank line, a
// comment and a line naming a file that is not there are all skipped WITHOUT
// consuming a table slot -- that is what the i-- before each continue is.
// gamex86.dll: 100235C0..10023B11
// gamei386.so: 00047B3C..00048056
void OSP_configLoad(void)
{
    char    cfgpath[64];
    char    line[1024];
    int     i;
    FILE    *cf = NULL;
    char    *e;     // function scope: real's PE gives it a shallow slot
    // DECLARATION INITIALISERS, not statements: gcc creates a temp while
    // expanding an initialiser and it lands between the variable it
    // initialises and the next declaration, which is what puts the pooled
    // "serverconfigs.txt" address between `cfglist` and `cfgdefault` in real's
    // frame.  Written as plain assignments the temp comes after every
    // declared local instead, and the three slots rotate.
    cvar_t  *gamedir = gi.cvar("gamedir", "tourney", 0);
    cvar_t  *base = gi.cvar("basedir", ".", 0);
    cvar_t  *cfglist = gi.cvar("vote_config_list", "serverconfigs.txt", 0);
    cvar_t  *cfgdefault = gi.cvar("vote_config_default", "0", 0);
    cvar_t  *cdefn = gi.cvar("vote_config_defaultname", "default", 0);
    conf_size = 0;

    if (gamedir && base) {
        sprintf(cfgpath, "%s/%s/", base->string, gamedir->string);
        if (cfglist)
            strcat(cfgpath, cfglist->string);
        else
            strcat(cfgpath, "serverconfigs.txt");

        cf = fopen(cfgpath, "r");
        if (cf) {
            for (i = 0; i < 32; i++) {
                if (!fgets(line, 1024, cf))
                    break;

                line[1023] = 0;
                if ((e = strchr(line, '\r')))
                    * e = 0;
                if ((e = strchr(line, '\n')))
                    * e = 0;
                if ((e = strchr(line, '#')))
                    * e = 0;

                // A positive `if` around the whole remainder with `i--` as its
                // `else`.
                if (strlen(line) > 1) {
                    conf_info[i][0] = 0;
                    if ((e = strchr(line, '\t'))) {
                        *e = 0;
                        e++;
                        strncpy(conf_info[i], e, 63);
                        conf_info[i][63] = 0;
                    }

                    sprintf(cfgpath, "%s/%s/%s", base->string, gamedir->string, line);
                    if (OSP_configFileExists(cfgpath))
                        strncpy(conf_name[i], line, 63);
                    else
                        i--;
                } else
                    i--;
            }

            fclose(cf);
            conf_size = i;

            if (!conf_size) {
                gi.dprintf("No server configs found.\n\n");
                gi.cvar_set("vote_enable_config", "0");
            } else {
                gi.dprintf("%d server configs found:\n", conf_size);

                for (i = 0; i < conf_size; i++) {
                    if (conf_info[i][0])
                        gi.dprintf("- %s [%s]\n", conf_info[i], conf_name[i]);
                    else
                        gi.dprintf("- [%s]\n", conf_name[i]);
                }

                if ((int)cfgdefault->value && cdefn->string &&
                    strcmp(cdefn->string, "default")) {
                    sprintf(cfgpath, "%s/%s/%s", base->string, gamedir->string,
                            cdefn->string);

                    if (OSP_configFileExists(cfgpath))
                        gi.dprintf("** Default config is: %s\n",
                                   cdefn->string);
                    else {
                        gi.dprintf("** Default config \"%s\" not found!\n",
                                   cdefn->string);
                        gi.dprintf("** No default config will be used.\n");
                        gi.cvar_set("vote_config_default", "0");
                        gi.cvar_set("vote_config_defaultname", "default");
                    }
                } else
                    gi.dprintf("** No default config will be used.\n");

                gi.dprintf("\n");
            }
        } else {
            gi.dprintf("\n\"%s\" server config list not found. No configs loaded.\n\n",
                       cfgpath);
            gi.cvar_set("vote_enable_config", "0");
        }
    }
}

// gamex86.dll: 10023B11..10023BE2
// gamei386.so: 00048058..00048140
void OSP_configList(edict_t *ent)
{
    int     i;

    if (!conf_size) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, no alternate server configs available.\n");
        return;
    }

    gi.cprintf(ent, PRINT_HIGH, "\nAvailable alternate server configs:\n");
    for (i = 0; i < conf_size; i++) {
        if (conf_info[i][0])
            gi.cprintf(ent, PRINT_HIGH, "  %s [%s]\n", conf_info[i],
                       conf_name[i]);
        else
            gi.cprintf(ent, PRINT_HIGH, "  %s\n", conf_name[i]);
    }
    gi.cprintf(ent, PRINT_HIGH, "\n");
}

// `ent` NULL means "this is a vote, not a client command": the description is
// then rewritten in place to the real filename, and the complaint goes to the
// console instead of to a player.
// gamex86.dll: 10023BE2..10023CBA
// gamei386.so: 00048140..00048215
bool OSP_configExists(edict_t *ent, char *name)
{
    int     i;

    for (i = 0; i < conf_size; i++) {
        if (!Q_stricmp(name, conf_name[i]))
            return true;

        if (conf_info[i][0] && !Q_stricmp(name, conf_info[i])) {
            if (!ent)
                strcpy(name, conf_name[i]);
            return true;
        }
    }

    if (ent)
        gi.cprintf(ent, PRINT_HIGH, "\"%s\" is not a valid server config.\n",
                   name);
    else
        gi.dprintf("(vote) Invalid \"%s\" server config specified.\n", name);
    return false;
}

// gamex86.dll: 10023CBA..10023D00
// gamei386.so: 00048218..00048253
bool OSP_configFileExists(char *path)
{
    FILE        *f;

    f = fopen(path, "r");
    if (!f)
        return false;
    fclose(f);
    return true;
}
