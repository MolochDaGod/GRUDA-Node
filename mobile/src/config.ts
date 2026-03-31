import Constants from 'expo-constants';

const extra = Constants.expoConfig?.extra ?? {};

/* ── Backend endpoints (mirrors include/config.h) ───── */
export const API_HOST = extra.apiHost ?? 'api.grudge-studio.com';
export const WS_HOST = extra.wsHost ?? 'ws.grudge-studio.com';
export const ID_HOST = extra.idHost ?? 'id.grudge-studio.com';

export const API_BASE = `https://${API_HOST}`;
export const WS_BASE = `wss://${WS_HOST}`;
export const ID_BASE = `https://${ID_HOST}`;

/* ── WebSocket namespaces (matches WS_NS_* defines) ─── */
export const WS_NS_TREATY = '/treaty';
export const WS_NS_CHAIN = '/grudachain';
export const WS_NS_ALERTS = '/alerts';

/* ── Device auth paths (matches DEVICE_* defines) ────── */
export const DEVICE_AUTH_POLL_PATH = '/device/auth/poll';
export const DEVICE_REGISTER_PATH = '/api/devices/register';
export const DEVICE_HEARTBEAT_PATH = '/api/devices/heartbeat';

/* ── Token addresses ─────────────────────────────────── */
export const GBUX_MINT = extra.gbuxMint ?? '55TpSoMNxbfsNJ9U1dQoo9H3dRtDmjBZVMcKqvU2nray';
export const GRUDA_POLYGON = extra.grudaPolygon ?? '0xa6fd32edc7c037b77537b673da9971df0f34a721';

/* ── Timing ──────────────────────────────────────────── */
export const CHAIN_RECONNECT_MS = 10_000;
export const STATUS_POLL_MS = 15_000;
export const HEARTBEAT_INTERVAL_MS = 30_000;

/* ── Game URLs ───────────────────────────────────────── */
export const GRUDGE_WARLORDS_URL = 'https://grudgewarlords.com';
export const GRUDGE_STUDIO_URL = 'https://grudge-studio.com';

/* ── Platform identifier sent in WS auth ─────────────── */
export const PLATFORM = 'android';
export const APP_VERSION = '1.0.0';
