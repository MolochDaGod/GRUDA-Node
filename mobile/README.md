# GRUDA Node — Android Client

Android app for Grudge accounts, wallet, node dashboard, and game launcher.
Built with Expo / React Native. Connects to the same Grudge backend as the ESP32 master node.

## Quick Start

```bash
# Install dependencies
npm install --prefix mobile

# Start Expo dev server (scan QR with Expo Go on your phone)
npm --prefix mobile start
```

## Build APK (for testing)

```bash
# One-time: install EAS CLI
npm install -g eas-cli

# One-time: login to Expo
eas login

# One-time: link project (creates EAS project ID)
cd mobile && eas init

# Build a shareable APK (preview profile)
eas build --platform android --profile preview
```

The APK will be downloadable from the EAS dashboard when the build completes.

## Publish to Google Play

### 1. Create a Google Cloud Service Account

This lets EAS upload builds directly to your Play Console (https://g.dev/grudge).

1. Go to [Google Cloud Console](https://console.cloud.google.com/)
2. Create a new project (or use existing) → Enable **Google Play Android Developer API**
3. Go to **IAM & Admin → Service Accounts** → Create service account
4. Grant role: **Service Account User**
5. Create a JSON key → download it
6. Save the key as `mobile/google-play-key.json` (gitignored)

### 2. Link the Service Account to Play Console

1. Open [Play Console](https://play.google.com/console/) → **Setup → API access**
2. Click **Link** next to your Google Cloud project
3. Under **Service accounts**, grant the service account **Release manager** permissions

### 3. Create the app listing

1. In Play Console → **Create app**
2. Package name: `com.grudgestudio.grudanode` (matches `app.json`)
3. Complete the store listing, content rating, and target audience sections
4. Upload a first AAB manually or via EAS:

```bash
# Build production AAB
eas build --platform android --profile production

# Submit to Play Store (internal testing track)
eas submit --platform android --profile production
```

### 4. Update app.json

After running `eas init`, replace `YOUR_EAS_PROJECT_ID` in `app.json` with the real ID.

## Project Structure

```
mobile/
├── App.tsx                    # Root — tab navigation
├── app.json                   # Expo config (package, splash, deep links)
├── eas.json                   # Build profiles (dev/preview/production)
├── src/
│   ├── config.ts              # Backend URLs (mirrors include/config.h)
│   ├── theme.ts               # Colors/spacing (mirrors web/index.html CSS)
│   ├── types.ts               # TS types (mirrors ESP32 C++ structs)
│   ├── services/
│   │   ├── api.ts             # REST client (mirrors src/account.cpp HTTP calls)
│   │   ├── chain-ws.ts        # WebSocket client (mirrors _chain_ws_event in main.cpp)
│   │   └── session.ts         # Secure session storage (mirrors NVS on ESP32)
│   └── screens/
│       ├── AccountScreen.tsx   # Login via id.grudge-studio.com
│       ├── WalletScreen.tsx    # Web3Auth Solana wallet
│       ├── NodeScreen.tsx      # Live chain dashboard
│       └── GamesScreen.tsx     # Game launcher
```

## Web3Auth Wallet Setup (TODO)

To enable real wallet functionality:

```bash
npm install @web3auth/react-native-sdk @solana/web3.js
```

Then update `WalletScreen.tsx` to initialize Web3Auth with your `WEB3_Client_ID` from `.env`.

## Adding More Play Store Titles

Edit the `GAMES` array in `src/screens/GamesScreen.tsx`.
Add the `playStoreUrl` field when a title is published — the launcher will prefer opening the native app.

## Environment

- **Master node (ESP32)** stays on the desktop — validates blocks, runs GrudaChain
- **This app (Android)** is a client node — connects to chain, holds wallet, manages account
- Both speak the same WebSocket protocol to `wss://ws.grudge-studio.com/grudachain`

## Security Notes

- Auth tokens stored in Android Keystore via `expo-secure-store`
- No secrets in the APK — all auth flows go through `id.grudge-studio.com`
- `google-play-key.json` is gitignored — never commit it
