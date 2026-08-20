// sl_write.c -- the Standard Log's low-level writers: one function per record
// field, plus the log file's own open/close.
//
// StdLog 1.2 is not a NetGames USA format and has nothing to do with ngLog
// beyond having borrowed its file writer in v2.75 -- which is why the two were
// mutually exclusive there, and why `sl_log_method` was force-cleared whenever
// ngLog logging was on.  The writer is local to this file now, so the Standard
// Log and the JSON stats log (osp_stats.c) run independently of each other.

#include "g_local.h"

#include <errno.h>

static FILE     *sl_file;
static int      sl_buffered;        // lines written since the last flush

// sl_log_flush: 0 = let the system buffer decide, 1 = flush every 40 lines,
// 2 (the default) = flush every line.
#define SL_BUFFER_LINES 40

// gamex86.dll: 10063130..10063164
// gamei386.so: 00073B68..00073BA5
void sl_LogMapName(game_import_t *import, char *mapname)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tMap\t%s", mapname);
    sl_logWrite(output);
}

// gamex86.dll: 10063164..1006319D
// gamei386.so: 00073BA8..00073C0D
void sl_LogGameStart(game_import_t *import, float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tGameStart\t\t\t%d", (int)time);
    sl_logWrite(output);
}

// gamex86.dll: 1006319D..100631CD
// gamei386.so: 00073C10..00073C4A
void sl_LogVers(game_import_t *import)
{
    sl_logWrite("\t\tStdLog\t1.2");
}

// gamex86.dll: 100631CD..10063201
// gamei386.so: 00073C4C..00073C89
void sl_LogPatch(game_import_t *import, char *patch)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPatchName\t%s", patch);
    sl_logWrite(output);
}

static const char *const sl_months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// gamex86.dll: 10063201..1006331F
// gamei386.so: 00073C8C..00073DA2
void sl_LogDate(game_import_t *import)
{
    char        output[1024];
    time_t      tval;
    struct tm   *lt;
    int         monthnum;

    time(&tval);
    lt = localtime(&tval);
    if (!lt)
        return;

    monthnum = lt->tm_mon;
    if (monthnum < 0 || monthnum > 11)
        monthnum = 11;

    Q_snprintf(output, sizeof(output), "\t\tLogDate\t%.2d %s %d", lt->tm_mday,
               sl_months[monthnum], lt->tm_year + 1900);
    sl_logWrite(output);
}

// gamex86.dll: 1006331F..1006337E
// gamei386.so: 00073DA4..00073DF8
void sl_LogTime(game_import_t *import)
{
    char        output[1024];
    time_t      t;
    struct tm   *tmp;

    time(&t);
    tmp = localtime(&t);
    if (!tmp)
        return;

    Q_snprintf(output, sizeof(output), "\t\tLogTime\t%.2d:%.2d:%.2d",
               tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    sl_logWrite(output);
}

// gamex86.dll: 1006337E..100633B2
// gamei386.so: 00073E38..00073E9D
void sl_LogDeathFlags(game_import_t *import, unsigned long flags)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tLogDeathFlags\t%lu", flags);
    sl_logWrite(output);
}

// gamex86.dll: 100633B2..100633EB
// gamei386.so: 00073E38..00073E9D
void sl_LogGameEnd(game_import_t *import, float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tGameEnd\t\t\t%d", (int)time);
    sl_logWrite(output);
}

// gamex86.dll: 100633EB..10063406
// gamei386.so: 00073EA0..00073ECA
void sl_CloseLogFile(void)
{
    if (sl_file) {
        fflush(sl_file);
        fclose(sl_file);
        sl_file = NULL;
    }
    sl_buffered = 0;
    sl_status = 0;
}

// gamex86.dll: 10063406..10063443
// gamei386.so: 00073ECC..00073F35
void sl_LogPlayerConnect(game_import_t *import, char *name, int unused,
                         float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPlayerConnect\t%s\t\t%d", name,
               (int)time);
    sl_logWrite(output);
}

// gamex86.dll: 10063443..10063480
// gamei386.so: 00073F38..00073FA1
void sl_LogPlayerLeft(game_import_t *import, char *name, float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPlayerLeft\t%s\t\t%d", name,
               (int)time);
    sl_logWrite(output);
}

// gamex86.dll: 10063480..100634C1
// gamei386.so: 00073FA4..00074010
void sl_LogPlayerRename(game_import_t *import, char *oldname, char *newname,
                        float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPlayerRename\t%s\t%s\t%d", oldname,
               newname, (int)time);
    sl_logWrite(output);
}

// The record is "<scorer> <other> <event> <weapon> <score> <time> <ping>",
// with the second field left empty for a suicide.
// gamex86.dll: 100634C1..1006354B
// gamei386.so: 00074010..000740E3
void sl_LogScore(game_import_t *import, char *player, char *other, char *event,
                 char *weapon, int score, float time, int ping)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "%s\t%s\t%s\t%s\t%d\t%d\t%d",
               player ? player : "", other ? other : "", event ? event : "",
               weapon ? weapon : "", score, (int)time, ping);

    sl_logWrite(output);
}

// gamex86.dll: 1006354B..100636C2
// gamei386.so: 000740E4..00074303
int sl_OpenLogFile(game_import_t *import)
{
    if (sl_status) {
        sl_status = 2;
        return 2;
    }

    sl_log_method = gi.cvar("sl_log_method", "0", 0);
    sl_filename = gi.cvar("sl_filename", "stdlog.log", 0);
    sl_log_style = gi.cvar("sl_log_style", "0", 0);
    sl_log_flush = gi.cvar("sl_log_flush", "2", 0);

    // Bit 0 is "record to a local file".  The UDP and TCP collector bits the
    // Standard Log defines were never implemented here, and the shipped
    // documentation says so.
    if (!((int)sl_log_method->value & 1) || !sl_filename->string[0]) {
        sl_status = 0;
        return 0;
    }

    // sl_filename keeps v2.75's meaning: a path relative to the Quake II
    // base directory, not to the game directory.
    sl_file = fopen(sl_filename->string, "a");
    if (!sl_file) {
        gi.dprintf("Couldn't create Standard Log \"%s\": %d\n",
                   sl_filename->string, errno);
        sl_status = 0;
        return 0;
    }

    sl_buffered = 0;
    sl_status = 1;
    gi.dprintf("Standard Log logging enabled (%s).\n", sl_filename->string);

    return 1;
}

// gamex86.dll: 100636C2..100636E5
// gamei386.so: 00074304..00074333
void sl_logWrite(char *line)
{
    if (!sl_status || !sl_file)
        return;

    if (fprintf(sl_file, "%s\n", line) < 0) {
        gi.dprintf("Error writing to Standard Log: %d\n", errno);
        sl_CloseLogFile();
        return;
    }

    if (!(int)sl_log_flush->value)
        return;

    if ((int)sl_log_flush->value == 1) {
        if (++sl_buffered < SL_BUFFER_LINES)
            return;
        sl_buffered = 0;
    }

    fflush(sl_file);
}
