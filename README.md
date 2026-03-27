# GRUDA Node
GRUDA Node is an ESP32-based Grudge Account device with a local DevApp for flashing, monitoring, and managing firmware. The device now uses a Grudge Account login flow instead of relying only on a local wallet identity.

## What it is
- ESP32 firmware built with PlatformIO and Arduino
- LVGL touchscreen interface
- Local Node.js DevApp for build, upload, flash, monitor, and account actions
- Grudge backend integration for:
  - account login
  - treaty / alerts sockets
  - GrudaChain node status

## Login model
- The device keeps a local wallet key in NVS for device signing
- The user identity is the Grudge Account session
- On first boot, or after logout, the device shows a 6-character pairing code
- The user visits `id.grudge-studio.com/device` and enters the code
- The device polls `api.grudge-studio.com/device/auth/poll`
- Once approved, the session is saved in NVS and the main UI loads

## Project layout
- `src/` — firmware source
- `src/ui/` — LVGL screens and tabs
- `include/` — firmware headers and generated image headers
- `devapp/` — local Node.js dashboard
- `assets/` — source images and gifs
- `scripts/` — local helper scripts
- `tools/` — image conversion helpers

## New account-related files
- `include/account.h`
- `src/account.cpp`
- `src/ui/screen_login.cpp`

## Environment
Copy `.env.example` to `.env` and fill in safe values only.

Important:
- Do not put admin private keys on the device
- Do not put database URLs on the device
- Do not commit real secrets
- Rotate the current secrets from the existing `.env` before production use

## DevApp
Start the dashboard:

```bash
npm install --prefix devapp
npm --prefix devapp run dev
```

Open:
- `http://localhost:3000`

The DevApp can:
- build firmware
- upload firmware
- flash and open serial monitor
- parse device serial output to detect login state
- send a `LOGOUT` command to the device over the selected COM port

## Firmware flow
1. Boot splash
2. Wi-Fi connect
3. Account resume from NVS
4. If no session, show login screen
5. After login, initialize wallet, node, treaty, alerts, and shell UI

## Notes
- The login screen currently depends on the backend implementing the pairing endpoints
- DevApp account state is cached from serial monitor output
- Device logout currently restarts the device after session wipe
