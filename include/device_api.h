#ifndef GRUDA_DEVICE_API_H
#define GRUDA_DEVICE_API_H

#include <Arduino.h>
#include "wallet.h"

/**
 * Register this device with id.grudge-studio.com/api/devices/register.
 * Sends publicKey, firmware version, hardware type.
 * Stores returned device token in NVS.
 * Returns true on success.
 */
bool device_register(const GrudaWallet& wallet, const String& grudgeId);

/**
 * Send heartbeat to id.grudge-studio.com/api/devices/heartbeat.
 * Includes block height, uptime, peer count.
 * Returns true on success.
 */
bool device_heartbeat(const GrudaWallet& wallet, uint32_t blockHeight,
                      uint32_t uptime, uint8_t peers);

/**
 * Load saved device token from NVS (if any).
 */
String device_get_saved_token();

#endif /* GRUDA_DEVICE_API_H */
