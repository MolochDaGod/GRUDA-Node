#include "discord.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static const char* WEBHOOK_URL =
    "https://" DISCORD_WEBHOOK_HOST DISCORD_WEBHOOK_PATH;

bool discord_post(const String& content) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.begin(WEBHOOK_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    JsonDocument doc;
    doc["content"]  = content;
    doc["username"] = "GRUDA Node";

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    http.end();

    bool ok = (code == 200 || code == 204);
    if (!ok) {
        Serial.printf("[DISCORD] Post failed: HTTP %d\n", code);
    }
    return ok;
}

bool discord_post_embed(const String& title, const String& description,
                        uint32_t color) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.begin(WEBHOOK_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    JsonDocument doc;
    doc["username"] = "GRUDA Node";

    JsonArray embeds = doc["embeds"].to<JsonArray>();
    JsonObject embed = embeds.add<JsonObject>();
    embed["title"]       = title;
    embed["description"] = description;
    embed["color"]       = (int)color;

    /* Timestamp */
    embed["footer"]["text"] = "GRD-17 Hardware Node";

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    http.end();

    bool ok = (code == 200 || code == 204);
    if (!ok) {
        Serial.printf("[DISCORD] Embed post failed: HTTP %d\n", code);
    }
    return ok;
}
