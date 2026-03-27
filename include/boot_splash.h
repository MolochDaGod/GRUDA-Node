#ifndef GRUDA_BOOT_SPLASH_H
#define GRUDA_BOOT_SPLASH_H

#include "config.h"

void boot_splash_show();
void boot_splash_set_progress(uint8_t pct, const char* msg);
void boot_splash_hide();
bool boot_splash_active();

#endif
