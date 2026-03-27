# GRUDA Node Handoff Notes (2026-03-27)

## Current Device State

- Firmware target: ESP32 dev module on COM6.
- PlatformIO env: `esp32dev` in `platformio.ini`.
- Latest successful upload was completed after UI and touch changes.

## What Was Updated

### UI / UX

- Header bar redesigned to be cleaner and less noisy.
- Account icon is now clickable and opens a settings panel.
- Tab icon bar made larger and higher contrast for touch.
- Boot splash switched to animated intro frames with loading bar.
- Wallet tab no longer has constantly spinning coin.

### Runtime updates

- Wallet and node tabs now refresh from loop state.
- Treaty / Vote / Alerts refresh hooks connected in shell updater.
- Multi-network Wi-Fi selection added (`WIFI_SSID`, `_2`, `_3`).

### Dev Control App

- Added token + swap policy config in `devapp/config/tokens.js`.
- Added runtime config endpoints in `devapp/server.js`:
  - `/api/runtime-config`
  - `/api/tokens`
  - `/api/swap-policy`
  - `/api/auth/providers`
- Added dashboard panels for supported tokens and swap model.
- Added env loading via `dotenv` and package dependency.

## Touchscreen Fixes Applied

- Touch handling moved to configurable macros in `include/config.h`.
- Added touch threshold config (`TOUCH_Z_THRESHOLD`).
- Added coordinate transforms:
  - `TOUCH_SWAP_XY`
  - `TOUCH_INVERT_X`
  - `TOUCH_INVERT_Y`
- Added calibration constants:
  - `TOUCH_CAL_X_MIN`, `TOUCH_CAL_X_MAX`
  - `TOUCH_CAL_Y_MIN`, `TOUCH_CAL_Y_MAX`
  - `TOUCH_CAL_ROTATION`
- Added clamping + smoothing for more stable touch points.

## If Touch Is Still Off

- Tune these values in `include/config.h` and reflash:
  - `TOUCH_CAL_*`
  - `TOUCH_SWAP_XY`
  - `TOUCH_INVERT_X`
  - `TOUCH_INVERT_Y`
  - `TOUCH_Z_THRESHOLD`
- Most common hardware issue:
  - Touch axes are swapped or one axis inverted.

## Security Notes (Important)

- `.env` currently contains high-sensitivity secrets and API keys.
- Rotate and replace exposed credentials before production use.
- Do not put swap routing secrets or admin private keys on ESP32.
- Recommended architecture:
  - Server-routed swaps, device-approved actions only.
  - Device signs constrained payloads, not broad privileged operations.

## Requested Token Set Captured

- SOL
- POLY
- ETH
- GBUX (Solana): `55TpSoMNxbfsNJ9U1dQoo9H3dRtDmjBZVMcKqvU2nray`
- GRUDA (Polygon): `0xa6fd32edc7c037b77537b673da9971df0f34a721`

## Next Suggested Work Items

1. Add real Web3Auth login/session flow in devapp frontend + backend verify path.
2. Add quote aggregation and slippage protections server-side.
3. Add explicit device-side approval screen for swap details.
4. Add serial touch diagnostic mode to print raw touch points for calibration.
