#ifndef GRUDA_SCREENSAVER_H
#define GRUDA_SCREENSAVER_H

#include "config.h"

#define SCREENSAVER_IDLE_MS  60000  /* 60 seconds idle → screensaver */

void screensaver_show();
void screensaver_hide();
bool screensaver_is_active();
void screensaver_reset_timer();
void screensaver_check(uint32_t idleTimeoutMs = SCREENSAVER_IDLE_MS);

#endif
