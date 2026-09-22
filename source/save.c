#include "save.h"
#include <stdio.h>
#include <sys/stat.h>

#define SAVE_DIR  "sdmc:/3ds/3ds-clock"
#define SAVE_PATH SAVE_DIR "/save.dat"

void save_ensure_dir(void)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir(SAVE_DIR, 0777);
}

bool save_exists(void)
{
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool save_read(ClockSaveData* out)
{
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return false;

    size_t n = fread(out, 1, sizeof(*out), f);
    fclose(f);
    return (n == sizeof(*out)) && (out->magic == CLOCK_SAVE_MAGIC);
}

bool save_write(const ClockSaveData* data)
{
    save_ensure_dir();
    FILE* f = fopen(SAVE_PATH, "wb");
    if (!f) return false;

    size_t n = fwrite(data, 1, sizeof(*data), f);
    fclose(f);
    return n == sizeof(*data);
}

bool save_delete(void)
{
    return remove(SAVE_PATH) == 0;
}
