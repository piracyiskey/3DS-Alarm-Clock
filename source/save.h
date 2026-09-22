#pragma once
#include <stdbool.h>
#include "clock.h"

bool save_exists(void);
bool save_read(ClockSaveData* out);
bool save_write(const ClockSaveData* data);
bool save_delete(void);
void save_ensure_dir(void);
