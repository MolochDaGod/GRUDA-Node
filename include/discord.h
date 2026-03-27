#ifndef GRUDA_DISCORD_H
#define GRUDA_DISCORD_H

#include <Arduino.h>

/**
 * Post a message to the device Discord webhook.
 * Uses HTTPS POST to discord.com.
 * Returns true on success (HTTP 204).
 */
bool discord_post(const String& content);

/**
 * Post a rich embed to the device Discord webhook.
 * color: hex int (e.g. 0xFF6600 for orange)
 */
bool discord_post_embed(const String& title, const String& description,
                        uint32_t color = 0xFF6600);

#endif /* GRUDA_DISCORD_H */
